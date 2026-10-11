# Minimal Manyfold repository preparation

Destination requested by Dan: https://github.com/danvogel/manyfold-reverb.
The browser shows a public repository with the MIT LICENSE and initial commit
`e0ba675533e12e67eeaf67e5063adc562211103c`.

A curated source tree is at `build/public-repo/manyfold-reverb`. Its separate local
repository uses `.local-git`, has origin set to the requested GitHub URL, and contains
no original workspace history. `tools/export_public_repo.py` creates a new tree only;
it refuses existing destinations. Included files are explicitly selected, not copied
from the entire working directory. The generated manifest records each source byte
hash; README is preserved verbatim.

Included: notebook UI, Python generation/validation, portable C++ core, relevant tests,
16/64 presets, font/compiled-runtime notices, compressed art derivatives, a source-hash
checked prebuilt WASM module and developer setup. Omitted: native hosts, high-resolution
art/photos, studies, previews, old probes, credentials, caches and build products.
The export carries font licenses and Emscripten's complete permissive license.

Verified in the exported tree: `make test local-harness qollab-bundle` passes,
including C++ DSP/session/stereo/character checks, Python quantum/workflow tests,
JavaScript workflow/presenter controls and the compiled browser audio adapter.
The few native-Studio-specific Python tests explicitly skip; that host is outside
this export. Log: `build/minimal-repo-tests.log`.

The shell cannot resolve github.com and GitHub is signed out in Chrome. Consequently,
local repository preparation must not be described as a completed GitHub push.
An ordinary fetch/merge/push retains the existing GitHub initial LICENSE commit;
never force-push or replace that history. Verified setup commands live in the
export's `docs/SETUP.md`; README corrections are supplied separately for Dan/dot.

The Qollab frozen candidate is independent of GitHub upload. ZIP import is now visibly
supported: the staged creation form recognizes 13 files, about 1 MB, “Runs as JS”.
Candidate: `build/qollab-rehearsals/20261011T192743900899Z/`, with original art and the
Next/Leap marker correction. The new thirteenth file is Emscripten's license notice.
Full hosted startup/audio/generation must still be tested after draft creation.
