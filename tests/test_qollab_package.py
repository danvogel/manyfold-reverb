"""Packaging tests: stale/extraneous files and archive path preservation."""
from pathlib import Path
import tempfile
import unittest
import zipfile

from tools.package_qollab import CONCAT, DIRECT, FILE_LIMIT, freeze_bundle, validate_bundle


class QollabPackageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.bundle = self.root / "bundle"
        for source in set(DIRECT.values()) | {s for group in CONCAT.values() for s in group}:
            path = self.root / source
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes((source + "\n").encode())
        for name in set(DIRECT) | set(CONCAT):
            sources = CONCAT.get(name, (DIRECT.get(name),))
            path = self.bundle / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"".join((self.root / s).read_bytes() for s in sources))

    def test_archive_root_paths_and_snapshot_match(self):
        records = validate_bundle(self.bundle, self.root)
        output = self.root / "release"
        manifest = freeze_bundle(self.bundle, output, records)
        self.assertEqual(manifest["file_count"], len(DIRECT) + len(CONCAT))
        with zipfile.ZipFile(output / "manyspace-qollab.zip") as z:
            self.assertIn("index.html", z.namelist())
            self.assertIn("manyspace/workflow.py", z.namelist())
            self.assertNotIn("manifest.json", z.namelist())
            for record in records:
                self.assertEqual(z.read(record["path"]),
                                 (output / "project" / record["path"]).read_bytes())
        with self.assertRaises(FileExistsError):
            freeze_bundle(self.bundle, output, records)

    def test_missing_and_unexpected_files_rejected(self):
        (self.bundle / "extra.txt").write_text("leftover")
        (self.bundle / "index.html").unlink()
        with self.assertRaisesRegex(ValueError, "inventory mismatch"):
            validate_bundle(self.bundle, self.root)

    def test_stale_concatenation_rejected(self):
        (self.root / "web/notebook.js").write_text("new source")
        with self.assertRaisesRegex(ValueError, "Stale bundled file: main.js"):
            validate_bundle(self.bundle, self.root)

    def test_oversized_file_rejected(self):
        (self.bundle / "main.css").write_bytes(b"x" * (FILE_LIMIT + 1))
        with self.assertRaisesRegex(ValueError, "oversized"):
            validate_bundle(self.bundle, self.root)

    def test_changes_between_validation_and_packaging_rejected(self):
        records = validate_bundle(self.bundle, self.root)
        (self.bundle / "index.html").write_text("changed")
        with self.assertRaisesRegex(ValueError, "changed during packaging"):
            freeze_bundle(self.bundle, self.root / "release", records)
        self.assertFalse((self.root / "release").exists())

    def test_symlink_rejected(self):
        (self.bundle / "extra").symlink_to(self.root / "web/notebook.js")
        with self.assertRaisesRegex(ValueError, "symlink"):
            validate_bundle(self.bundle, self.root)


if __name__ == "__main__":
    unittest.main()
