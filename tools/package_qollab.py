#!/usr/bin/env python3
"""Build and freeze a local Qollab rehearsal; never upload or publish."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess
import zipfile


ROOT = Path(__file__).resolve().parents[1]
# Conservative decimal interpretation of the documented 2 MB per-file limit.
FILE_LIMIT = 2_000_000
FILE_COUNT_LIMIT = 100
DIRECT = {
    "index.html": "web/notebook.html",
    "requirements.txt": "web/qollab-requirements.txt",
    "presets/reference64.json": "presets/reference64.json",
    "assets/manyspace-core.js": "build/wasm/manyspace-core.js",
    "assets/architects-daughter-LICENSE.txt": "web/fonts/architects-daughter-LICENSE.txt",
    "assets/permanent-marker-LICENSE.txt": "web/fonts/permanent-marker-LICENSE.txt",
    "assets/emscripten-LICENSE.txt": "web/licenses/emscripten-LICENSE.txt",
    **{f"manyspace/{name}.py": f"manyspace/{name}.py"
       for name in ("__init__", "galaxy", "workflow", "verify_job")},
}
CONCAT = {
    "main.css": ("web/notebook-fonts.css", "web/notebook.css", "web/notebook-art.css"),
    "main.js": ("web/model.js", "web/wasm-host.js", "web/notebook-templates.js",
                "web/notebook-parameters.js", "web/notebook-art.js", "web/notebook.js", "web/notebook-entry.js"),
}
EXPECTED = frozenset(DIRECT) | frozenset(CONCAT)


def validate_bundle(bundle, root=ROOT):
    """Reject unexpected/stale files rather than silently shipping build leftovers."""
    paths = list(bundle.rglob("*"))
    if any(p.is_symlink() for p in paths):
        raise ValueError("Bundle contains a symlink")
    actual = {p.relative_to(bundle).as_posix() for p in paths if p.is_file()}
    if actual != EXPECTED:
        raise ValueError(f"Bundle inventory mismatch: missing={sorted(EXPECTED-actual)}, "
                         f"unexpected={sorted(actual-EXPECTED)}")
    if len(actual) > FILE_COUNT_LIMIT:
        raise ValueError("Bundle exceeds the file-count limit")
    records = []
    for name in sorted(actual):
        data = (bundle / name).read_bytes()
        if not data or len(data) > FILE_LIMIT:
            raise ValueError(f"Empty or oversized bundled file: {name} ({len(data)} bytes)")
        sources = CONCAT.get(name, (DIRECT.get(name),))
        expected = b"".join((root / source).read_bytes() for source in sources)
        if data != expected:
            raise ValueError(f"Stale bundled file: {name}")
        records.append({"path": name, "bytes": len(data),
                        "sha256": hashlib.sha256(data).hexdigest()})
    return records


def freeze_bundle(bundle, output, records):
    """Snapshot validated bytes and archive only project files at the ZIP root."""
    # Read and verify everything before creating output, guarding intervening edits.
    payload = {r["path"]: (bundle / r["path"]).read_bytes() for r in records}
    for r in records:
        if hashlib.sha256(payload[r["path"]]).hexdigest() != r["sha256"]:
            raise ValueError(f"Bundle changed during packaging: {r['path']}")
    output.mkdir(parents=True, exist_ok=False)
    snapshot = output / "project"
    archive = output / "manyspace-qollab.zip"
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as z:
        for name, data in sorted(payload.items()):
            path = snapshot / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            z.writestr(name, data)
    with zipfile.ZipFile(archive) as z:
        if z.testzip() is not None or set(z.namelist()) != set(payload):
            raise ValueError("Archive integrity/inventory check failed")
        for name, data in payload.items():
            if z.read(name) != data:
                raise ValueError(f"Archive contents differ: {name}")
    manifest = {
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "status": "local rehearsal only; hosted notebook verification pending",
        "limits": {"per_file_bytes": FILE_LIMIT, "file_count": FILE_COUNT_LIMIT,
                   "source": "docs/QOLLAB-RUNTIME.md; recorded limits, not a live quota check"},
        "file_count": len(records), "total_bytes": sum(r["bytes"] for r in records),
        "largest_file": max(records, key=lambda r: r["bytes"]),
        "archive": {"path": archive.name, "bytes": archive.stat().st_size,
                    "sha256": hashlib.sha256(archive.read_bytes()).hexdigest()},
        "files": records,
    }
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path,
                        help="New output directory; existing directories are never overwritten")
    args = parser.parse_args()
    subprocess.run(["make", "qollab-bundle"], cwd=ROOT, check=True)
    output = args.output or ROOT / "build/qollab-rehearsals" / datetime.now(
        timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    bundle = ROOT / "build/qollab"
    manifest = freeze_bundle(bundle, output, validate_bundle(bundle))
    print(f"Rehearsal: {output.resolve()}")
    print(f"{manifest['file_count']} files, {manifest['total_bytes']:,} bytes unpacked; "
          f"ZIP {manifest['archive']['bytes']:,} bytes")
    print("Upload contents of project/ retaining paths. The ZIP is a local transport archive; "
          "Qollab ZIP import is not assumed. See docs/QOLLAB-PUBLICATION-REHEARSAL.md.")


if __name__ == "__main__":
    main()
