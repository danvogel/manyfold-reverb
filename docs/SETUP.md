# Developer setup for the minimal Manyfold repository

README.md is preserved for Dan/dot's public submission copy review. Its early slice description contains stale facts; current setup lives here and corrections live in README-REVIEW.md.

This repository contains the notebook presenter, Python/Qiskit generation and validation, portable C++ audio core, relevant regression tests, fonts/art derivatives and a source-checked precompiled browser core. Native Studio, proposals, screenshots, high-resolution art originals, prototype history and build outputs remain in the authoring workspace.

## Local run

Requires Python 3, Node, Make; the supplied browser core avoids a compiler for ordinary previews. Python dependencies are pinned in requirements.txt. To install them into your own virtual environment:

```sh
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install -r requirements.txt
make local-harness
python3 tools/notebook_server.py
```

Open http://127.0.0.1:8765/. Use `--port 8766` if 8765 is already occupied. This server is loopback only and runs real local Qiskit generation. A plain static server can preview the bundled scene but cannot generate.

## Tests and explicit WASM rebuild

```sh
make test
make test-wasm
```

Tests require a C++17 compiler (the default is clang++). Native Studio-specific tests are skipped because that host is outside this export. Remaining schema, quantum generation, pairing/gating, map, DSP and browser-adapter checks run locally.

To rebuild the browser core after C++ changes, install Emscripten and run `make wasm`. Emscripten 6.0.12 produced the supplied core. `EMXX` selects its compiler, and `EM_CACHE` may point to a writable cache. Ordinary builds reject prebuilt/source hash mismatches instead of silently playing stale DSP. Updating the checked-in prebuilt core and hash manifest is a deliberate release operation.

## Qollab upload

```sh
make qollab-bundle
python3 tools/package_qollab.py
```

Upload only the frozen `manyspace-qollab.zip` (or the contents of its sibling `project/`), not this entire source repository. Qollab's upload form accepts ZIP files and preserves the nested paths; the resulting app has 13 files, including font and Emscripten notices. Choose the free built-in simulator for the first pass. No paid or hardware backend is needed. Persistence uses scene/take/log downloads; the Qollab sandbox does not preserve browser storage between runs.

## Source layout

- `web/`: model, notebook pages, controls, embedded font/art styles and WASM host adapter.
- `manyspace/`: circuits, conditioning, schema validation and Qollab Python bridge.
- `cpp/`: portable effect, tail/session handling, source synthesis and WASM ABI.
- `presets/`: reusable 16/64-room references.
- `prebuilt/`: single-file WASM JavaScript plus source hashes.
- `tests/`: relevant core, workflow, presenter, audio and packaging checks.

Quantum circuits generate saved room outcomes and transition probabilities. During audio playback, traversal and processing are classical. Old room visits retain independently decaying tails. Artwork derivatives are embedded once in CSS; original scans/photos stay in the authoring workspace.

MIT applies to original project code/art; bundled font notices and Emscripten's license are retained separately. Qiskit and NumPy are installed dependencies, not vendored source. No credentials or local Git history belong in the exported tree.
