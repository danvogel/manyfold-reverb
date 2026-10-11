# Qollab publication rehearsal

Prepared October 11, 2026. **Local package ready; full notebook hosted rehearsal
pending.** The earlier hosted probe establishes runtime capabilities, not a pass
for this complete notebook. This pass creates no hosted project or public release.
Dan performs draft upload/runs under the existing project boundary.

## Repeatable packaging

From the repository root:

```sh
make test test-wasm
python3 tools/package_qollab.py
```

The packaging command runs `make qollab-bundle`, validates exactly the intended
12 files against their current source bytes, and writes a new timestamped directory
under `build/qollab-rehearsals/`. Existing rehearsal directories are never overwritten.
It rejects missing, unexpected, empty, stale, oversized and symlinked files.
It preserves directory paths and verifies the resulting ZIP's contents.

Each rehearsal contains:

- `project/`: the frozen upload tree.
- `manyspace-qollab.zip`: the same tree, with `index.html` at the archive root.
- `manifest.json`: byte sizes and SHA-256 hashes, including the ZIP hash.

**Only the contents of `project/` belong in Qollab.** Do not upload its containing
folder, manifest, tests, screenshots or the repository. ZIP import is not assumed;
the archive is a convenient local transport copy. If the editor supports ZIP import,
verify that it preserves the root and nested paths shown below. Otherwise transfer
the individual files retaining those paths.

## First frozen candidate

Directory: `build/qollab-rehearsals/20261011T161603364179Z/`

| Fact | Verified local result |
|---|---|
| Files | 12 |
| Unpacked bytes | 559,439 |
| ZIP bytes | 174,164 |
| Largest file | `presets/reference64.json`, 266,972 bytes |
| Stylesheet | 78,072 bytes, including embedded fonts |
| ZIP SHA-256 | `a88c1bfb544c27179fdc8aabea942d204bfa84b052f9bdaeddd70b9c36ebc973` |

Upload inventory:

```text
index.html
main.css
main.js
requirements.txt
manyspace/__init__.py
manyspace/galaxy.py
manyspace/verify_job.py
manyspace/workflow.py
presets/reference64.json
assets/manyspace-core.js
assets/architects-daughter-LICENSE.txt
assets/permanent-marker-LICENSE.txt
```

The validator uses a conservative 2,000,000-byte per-file ceiling and the recorded
100-file project ceiling from [QOLLAB-RUNTIME.md](QOLLAB-RUNTIME.md). It does not
check account storage remaining. An attempted current documentation fetch was
unavailable, so these limits are recorded facts, not a new live quota verification.

The entry uses a static Python import, `.callPromising`, the selected `backend`,
`sendFile`, and an active top-level await. Reference and core resource paths are
literal Python strings. Fonts and WASM are embedded/project-local; no art, notebook
screenshots, Studio study images, local HTTP server or native app ships here.
Selected decorative sketches are still awaiting production integration.

## Private-draft rehearsal — Dan's hosted pass

Use a new private rehearsal draft, separate from the earlier runtime probe. Keep
its description plainly marked as a rehearsal. Import the frozen files and confirm
the 12-file inventory. Choose the **free built-in simulator** first; do not select
hardware or spend credits as part of this checklist. Stop/Run starts a fresh UI;
download artifacts before stopping when they must be retained.

| Check | Expected evidence |
|---|---|
| Startup | Notebook opens to Cover; fonts render; no import/package/runtime error; no computation or sound starts by itself. |
| Direct Play | Bundled 64-room scene is usable without Generate/Shape. Choose input, trigger, blend and movement, enable effect, then audition. |
| Audio/transport | Play, Loop, Stop and restart behave; no long padding gap on phrase wraps. Pause/Stop responds to a single activation. Audio is actually audible. |
| Trigger/movement | Manual override, Auto threshold and Each Loop work; Next follows the curve and Leap follows saved probabilities. Now and Still ringing remain distinct. |
| Blend | Immersion/Linger respond live; wet level and high-blend musicality are acceptable by ear. Local numerical tests are not an audible hosted pass. |
| Shape | Load default; map parameter selector shows actual values, including Stereo Spread; edit room, restore, adjust ranges and reorder. Audition Room/Path and Stop. |
| Scene round-trip | Save scene through Qollab's `sendFile`, download from the host console, upload it in a fresh run; edits, order and performance settings return. |
| Audio upload | Upload a nonprivate short WAV, choose once/loop, verify processed sound. Custom audio is separate from scene JSON and must be reselected after a fresh run. |
| Take/log | Record briefly, stop recording, export WAV and log through the host console while the run is active. Downloaded WAV opens, is stereo and contains the audible take. |
| Generate | Stop audio. In a fresh run choose 16 rooms and 1,024 shots, keep default depth/phase/seed, generate once. Backend result, outcome tally and odds must be real; save and send to Shape. |
| 64-room Generate | In another fresh run choose 64 rooms and 4,096 shots. Verify source coverage, populated results, saved provenance and downstream Shape/Play. |
| Failure/readability | Invalid uploaded JSON produces an understandable error without a false success state. Read the app at the real Qollab frame width, including a narrow window. |

Do not confuse the local screenshot fixture with a new sampled job. If generation
reports uncovered source rows, retain the error and retry in a fresh run with more
shots; do not silently fill missing rows. If startup, playback or export fails,
retain the console error and identify the frozen candidate hash before changing it.

After the built-in pass, an optional **free IonQ simulator** pass can verify one
remote job in a fresh run (start with 16 rooms and 1,024 shots). Confirm the selected
backend is a free simulator in the Run dialog. This remote pass is not performed or
asserted by local packaging; the full 64-room remote case remains a separate test.

## Record results and prepare the eventual public candidate

Record draft URL, candidate ZIP hash, date, browser/version, selected backend,
room count/shots, audible observations, downloaded artifact names and errors under
`docs/`. Mark each hosted check pass/fail/not tested. A successful earlier probe or
local test must not be substituted for a notebook hosted result.

Once issues are fixed, rerun relevant checks and package a **new** frozen candidate.
Keep the last tested private draft as evidence. Public description, project naming,
README narrative, credits and submission wording remain Dan/dot's review work.
The Appendix still has a `[GitHub link]` placeholder and artwork blanks; resolve or
explicitly accept those before the final public presentation. Publication and
hackathon submission are later actions, not effects of this packaging command.

## Local validation completed

- `make test test-wasm`: passed October 11; 26 Python tests plus JS, native C++ and
  compiled browser-core/audio checks. Log: `build/qollab-rehearsal-tests.log`.
- Packaging tests: six passed, covering archive root/paths and frozen bytes,
  existing-output protection, missing/extraneous files, stale concatenation,
  oversized files, symlinks and intervening edits.
- Candidate inventory/source hashes, byte limits, archive CRC and archive-byte
  equality passed. README SHA-256 remains
  `c02fb65cafede634990c0b3c55614da987256d2ad31f5550fe09be0e10bcac64`.

Related: [WEB-INTERFACE.md](WEB-INTERFACE.md),
[QOLLAB-HOSTED-PROBE-2026-10-11.md](QOLLAB-HOSTED-PROBE-2026-10-11.md),
and [README-REVIEW.md](README-REVIEW.md).
