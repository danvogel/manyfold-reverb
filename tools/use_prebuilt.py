"""Copy the supplied WASM only when its source hashes still match."""
import hashlib,json,shutil
from pathlib import Path
root=Path(__file__).resolve().parents[1]
for name,digest in json.loads((root/'prebuilt/manifest.json').read_text()).items():
    if hashlib.sha256((root/name).read_bytes()).hexdigest()!=digest:
        raise SystemExit('Prebuilt/source mismatch: '+name+'. Run make wasm with Emscripten, then build again.')
target=root/'build/wasm/manyspace-core.js';target.parent.mkdir(parents=True,exist_ok=True)
shutil.copyfile(root/'prebuilt/manyspace-core.js',target)
