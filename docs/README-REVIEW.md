# README facts for Dan and dot to review

README.md remains byte-for-byte preserved. Its SHA-256 before and after this
implementation is `c02fb65cafede634990c0b3c55614da987256d2ad31f5550fe09be0e10bcac64`.
This file flags factual updates; it does not draft submission narrative.

- The statement that an interactive map/control UI is outside the completed local
  slice is now stale: a native Mac listening app exists, with 64 map positions,
  three knobs, transport, trigger/traversal controls, original musical scenarios,
  WAV loading and four/eight-engine budgets. It is a local app; hosted UI/WASM
  execution is still unverified.
- The initial demo WAVs still exist as fixtures. The newer musical comparisons
  are generated with `make listening` into `build/listening/` and include shared
  input plus RMS-matched dry/effect exports.
- Reverse grains now have bounded correlation-aware overlap compensation; the
  severe level loss is corrected in tested fixtures. A complete reverse swell
  remains unimplemented, and the underlying sparse feedback-delay topology is
  unchanged.
- Decay and Sensitivity now smooth parameter changes. The native host stages
  controls through a bounded audio-thread queue.
- Verified developer setup/run instructions and architecture now live in
  [DEVELOPMENT.md](DEVELOPMENT.md) and [NATIVE.md](NATIVE.md). Dan and dot can decide
  what public setup facts and links belong in README.
- Old implementation/history was not copied. A permitted historical Subsong plan
  was read for host concepts during planning; no Subsong source was inspected or
  reused. [PROVENANCE.md](PROVENANCE.md) records this distinction.

No public project, hosted run, IonQ run, release, submission or award commitment
has been made by this implementation.

Further verified setup facts for review: `make studio` builds the separate
Generate/Shape/Play app at `build/ManyspaceStudio.app`. Its default Play path uses
a built-in scene. Local Qiskit generation, editable rooms/ordering, interference
charts and JSON persistence are implemented. `make qollab-bundle` only packages
local files; hosted audio/execution is not verified. Details belong in
[STUDIO.md](STUDIO.md). No README narrative was edited.

New verified implementation facts for review: Studio now exposes per-room wet
High cut/Low cut and exports Scene version 2, while loading version 1 scenes with
default filters. Phrase looping skips appended tail padding. Setup commands are
unchanged; these are implementation facts, not proposed submission prose.

Further facts for review (October 10, 2026, browser core): the statement that a WASM
adapter is not part of the slice is now stale. `make wasm` compiles the same C++ core
with Emscripten into one 74 KB file, `web/wasm-host.js` plays it in the browser through
a ScriptProcessorNode, and the Qollab bundle ships it as a project data file read through
Python. This is tested natively, under sanitizers and in Node; it has not yet been run
inside Qollab or auditioned in a browser. Qollab's rules and sandbox facts are recorded
in HACKATHON-RULES.md and QOLLAB-RUNTIME.md. README narrative was not edited.

Stereo update facts for review: per-room wet Stereo spread is now available in
Shape and in both the native and compiled browser core. Downloads use Scene
version 3; version 1/2 scene imports preserve their old 100% stereo width. Fresh
scenes derive 35–100% spread from the existing measured spacing bin. Tests cover
mono playback, independent tail width and scene compatibility. Details and the
native Save-dialog limitation belong in [STEREO.md](STEREO.md), not submission
narrative. README content remains unchanged.

Room character facts for Dan/dot review (October 10): ten added per-room controls
are implemented in both audio builds; modulation exposes separate depth/rate.
Fresh scenes use classical interpretations of existing measured outcomes and stable
source identity, without additional quantum bits. Scene v4 preserves older files'
sound via neutral migration. Bloom is an attack-preserving wet arrival envelope;
reverse grains remain distinct from reversing a complete tail. Verified setup,
matched samples and limitations are in docs/ROOM-CHARACTER.md and docs/EVIDENCE.md.
README narrative has not been edited.

Notebook facts for Dan/dot review (October 10–11): the Qollab bundle now contains
the five-page Multi-Verb notebook presenter, strict procedure gates, Each loop
triggering, uploaded-audio staging, processed stereo take recording and embedded
fonts. The local browser and compiled core were tested; this new notebook has not
been uploaded or run inside Qollab. The earlier hosted probe remains evidence for
runtime capabilities, not verification of this replacement presenter. Setup and
limits are in WEB-INTERFACE.md. One factual copy correction: Permanent Marker is
Apache 2.0; Architects Daughter is SIL OFL 1.1. README narrative is unchanged.

Publication rehearsal facts (October 11): `python3 tools/package_qollab.py` creates
a validated frozen 12-file project tree, ZIP and SHA-256 manifest. First candidate
is 559,439 bytes unpacked / 174,164 bytes zipped. Local tests pass; the full notebook
hosted rehearsal is pending. The earlier hosted probe did verify runtime capabilities,
so older statements above that all hosted execution is unverified are historical.
Appendix `[GitHub link]` remains a public-presentation correction to review. Setup
and the hosted checklist are in QOLLAB-PUBLICATION-REHEARSAL.md; no README narrative,
publication or submission was changed.

Minimal repository preparation (October 11): Dan created the public MIT repository
`https://github.com/danvogel/manyfold-reverb` and authorized preparing it for this
project. A source-only notebook/core export is prepared separately under
`build/public-repo/manyfold-reverb`; its tests and builds were run independently.
README is still preserved verbatim and contains early-slice facts that need Dan/dot
review before submission: the interactive notebook and WASM adapter now exist,
local/hosted probe support is verified, rich room character and stereo are implemented,
and the public repository exists. Native Studio is outside the minimal export, so
its developer commands belong to the authoring repository's docs. The minimal
repository's verified commands are in `docs/SETUP.md`. Source upload/push and the
full hosted notebook result must be recorded separately once actually completed.
