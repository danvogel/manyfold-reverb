# Local slice evidence

Commands run on the local Mac, Python 3.13.0, Qiskit 2.5.2, NumPy 2.4.6, Clang C++17.
These results are from fresh code; no old prototype tests were inherited.

```sh
make test
make demo
python3 -m manyspace.verify_job
clang++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Icpp cpp/test_core.cpp -o build/test_core_sanitized
./build/test_core_sanitized
```

`make test` passes 20 Python tests when both Mac bundles exist (including three
native decoder/metadata tests and four workflow tests), two JavaScript suites,
and five C++ suites: core with both
16- and 64-room snapshots, Session, reverse balance, and room tone filters. Python tests check seeded reference and JSON roundtrip,
malformed schemas/probabilities/mappings, Hilbert cells, acoustic ordering,
phase dependence, a separate dephased density-matrix calculation, source-label
conditional mathematics, deterministic WAV bytes and output speed feasibility.

Native tests cover exact unity dry at Mix=0, nonzero-tail silent reentry versus
an equivalent different room, independent visit epochs/gains, absolute age targets,
unequal audible levels and quietest weighted eviction, pending retarget, no callback
allocation during rapid leaps, fixed buffers and finite output. Runs at 44.1/48/96
kHz are byte-identical across 1/127/511-frame processing when events land at the
same samples. Physically matched tone-burst late energy stays within 5% across
rates. Extreme-range 96 kHz/full-scale/reverse-grain fixtures use 31-frame blocks,
4/8 engines and nondefault ramps; adjacent wet steps stay below 0.10 in that fixture.
These are concrete fixtures, not guarantees of inaudibility for arbitrary inputs.

Dry-only trigger tests cover sustained notes, tail-only silence, rearm, short
cooldown intervals, and automatic Next. Late feedback decay is tested at all three
rates. Half/0.8333/normal/1.5-speed sine fixtures have dominant frequency within
13 Hz of 440×speed, allowing grain sidebands, and meaningful decaying tails.
Reverse-grain tests establish finite nonzero different output; they do not establish
a complete reverse swell. ASan/UBSan passes the current 64-room core, Session and
reverse-balance suites with no reported error after the ring-wrap fix below.

## Musical listening slice

Additional commands run:

```sh
make native
make listening
make test-native
clang++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Icpp -DUSE_GALAXY64 cpp/test_core.cpp -o build/test_core64_sanitized
clang++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Icpp cpp/test_session.cpp -o build/test_session_sanitized
clang++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Icpp cpp/test_balance.cpp -o build/test_balance_sanitized
./build/test_core64_sanitized
./build/test_session_sanitized
./build/test_balance_sanitized
```

The first Session sanitizer run found a heap read one past the delay-ring stamps:
adding the ring length to a tiny negative float position could round to exactly
the length. Wrapping and interpolation position now use double. The longer
12-second phrase-traversal fixture and a focused near-integer-delay fixture pass
ASan/UBSan after this correction. Normal tests passed before the correction, so
their success alone did not establish memory safety.

Reverse overlap energy normalization is clamped to 1–3×, with a silence floor and
smoothed gain. Thirty matched forward/reverse pairs (eight tones from 55–2000 Hz,
noise and transient noise, at 44.1/48/96 kHz) have maximum absolute wet-energy
difference **0.473902 dB** after the tone changes below, below the 2 dB acceptance bound. Original chord exports
show reverse **+0.0180 dB** and original plucks **−0.0815 dB** against their forward
treatments over the active musical interval. These are fixture results, not a
guarantee of equal perceived loudness for arbitrary material.

Nine scenarios render raw dry/mix/wet and matched dry/mix files with JSON manifests.
Identical-source comparisons have byte-identical dry files. Matched pairs are
within 0.5 dB RMS over the same active interval and never increase gain; all raw
samples are finite and below unity in these exports. Repeated rendering produces
identical manifest and mix-WAV hashes. Longer pluck decay produces over 10× late
energy after correcting for the differing Mix settings. A separate Session test
holds source, Decay and traversal fixed: changing only Sensitivity from .35 to .90
produces **16 → 48** drum-triggered jumps. The two listening drum presets also
change Decay and traversal, so their whole-preset comparison is not an isolated
Sensitivity experiment.

Session tests exercise FIFO capacity/full rejection, 100,000 threaded SPSC commands,
127/511-frame render invariance, exact phrase scheduling, paused-state freezing,
larger-than-scratch callbacks, loop/nonloop EOF, stopped-state replay preparation,
nonfinite/over-unity source sanitization, four/eight budgets, rapid live knob/bus/
jump changes, gain/output bounds and zero counted callback heap allocations.

The actual app's ExtAudioFile helper decodes 16-bit mono, 24-bit stereo and float
stereo WAV fixtures and resamples 44.1/48/96 kHz sources. Tests verify duration,
frame counts, channel RMS and six-second padding, plus rejection of empty, corrupt,
over-60-second, multichannel files and invalid target rates. Bundle validation
rejects incorrect types, coordinates, identities, source ranges, acoustic settings
and probabilities. All three native file/metadata tests pass without an audio device.

Desktop smoke testing observed real 48 kHz callbacks, advancing source cursor and
nonzero meters, Pause, live controls, Next/Leap, scenario selection, wet-bus/loop
changes, keyboard map focus/navigation and Restart retaining adjusted controls.
The observed callback maximum ranged up to 1.50 ms with zero reported overruns in
these observations. This is app callback timing on this Mac, not total device
latency or a hardware/platform guarantee. The restricted-shell `--audio-smoke`
failed because the default audio component was unavailable there; desktop launch
worked. The final numerical ring-wrap fix was checked through the sanitizer suites
and native rebuild/file tests; the running desktop copy was left alone when its
controls were being adjusted. It must be quit/reopened to use the final executable.
The native WAV chooser and hot device changes were not separately desktop-tested.

No by-ear sound-quality judgement was substituted for Dan's audition. Reverse
grains and the sparse delay topology remain as described in [DSP.md](DSP.md).

| Reference | Adjacent distance mean before → after | Maximum after | Coherent/dephased TV mean / min |
|---|---:|---:|---:|
| 16 rooms | 0.541890 → 0.207545 | 0.434613 | 0.343024 / 0.159879 |
| 64 rooms | 0.497527 → 0.104101 | 0.380058 | 0.382341 / 0.291149 |

Distances use normalized distance/speed/damping weights 0.5/0.3/0.2, exclude mode
and last-to-first wrap, and benefit from duplicate outcomes (35 unique among 64).
No perceptual smoothness or sound-quality claim follows from the metric alone.

The actual local BasicSimulator single job used 18 qubits, 65,536 shots,
seed_simulator=23. Per-source coverage was 958–1,129 shots, none missing.
Transition TV from exact: mean 0.084181, max 0.108249. Property TV from exact:
mean 0.086970, max 0.111438. The run took 0.428 seconds in the measured invocation.
Recorded generic transpilation has 18 qubits, depth 36, 26 CX; BasicSimulator's own
compiled circuit uses its broader basis and depth 15. Neither proves IonQ native
transpilation. Exact distributions and interference proof remain separate from
sampling noise and future hardware noise.

Initial 12-second offline renders used approximately 0.051 seconds for four engines
and 0.097 seconds for eight. Four/eight delay-buffer memory was 614,656/1,229,312
bytes at 48 kHz (plus snapshot/voice/container overhead). No 64 permanently live
delay networks exist. This is an offline total-time benchmark, not worst callback
latency, embedded CPU viability or hosted browser performance.

Failed/untested paths: conventional .git creation denied (alternate .local-git
succeeded); public docs web fetch failed (browser read succeeded); HTTP listener
denied; file-URL probe blocked by browser policy; advisor/external review wrapper
absent; WASM/worklet, Qollab hosted execution, IonQ execution, full reverse
swell and hardware/plugin integration remain untested or unfinished. No code left
the laptop for a Qollab project or IonQ job.

## Generate / Shape / Play Studio

Built `make studio`, ran `make test-studio`, and generated the local
`make qollab-bundle` output. Node 24.15.0 runs the portable artifact-model and
asynchronous UI-host tests. Python's four workflow tests cover seeded quantum-only
generation, phase dependence, decoding/layout/coverage rejection, a real single
BasicSimulator submission of 4,096 shots over 16 origins, and native scene import.
The counted backend adapter made exactly one submission; every source had samples.
It was not a hosted Qollab/IonQ test.

Model checks cover source identity, retained quantum provenance, overrides and
restoration, preserving selected identity through ordering, permutation of both
matrix axes, all Hilbert cells and adjacent steps for 16/64 rooms, round trips and
rejection of inconsistent derived data/invalid files. UI tests exercise idle entry,
rapid Play/Pause before asynchronous host replies, stale telemetry, full-queue
rejection, restart bus retention, Shape's available bus, pausing for editing,
targeting the field's original room identity and superseded preparation.

Native `--scene-smoke` renders valid 16/64-room materialized snapshots through the
same C++ Session. Changing only the selected room's speed changes output while
source, controls and destination stay fixed. Incomplete rows or
missing required booleans are rejected. The 16-room, 64-room and edited snapshots
also pass ASan/UBSan with no reported issue, as does malformed-scene rejection:

```sh
clang++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Icpp -Inative -fobjc-arc -fblocks native/studio.mm -framework AppKit -framework AudioToolbox -framework CoreAudio -framework WebKit -o build/studio_sanitized
./build/studio_sanitized --scene-smoke build/workflow/scene.json
./build/studio_sanitized --scene-smoke build/workflow/scene16.json
./build/studio_sanitized --scene-smoke build/workflow/edited.json
./build/studio_sanitized --scene-smoke build/workflow/invalid.json
```

The invalid fixture intentionally exits 1 with `loop`; success means rejection,
not a successful render. These probes don't initialize an audio device.

Desktop visual checks showed the three-way entry, primary demo action, Play map,
transport/knobs and real 48 kHz telemetry. Leap changes the destination. Shape
marked a 72.4 ms hand edit while preserving origin 16/outcome 63. Its interference
chart and Path audition were visible; Path advanced the actual destination and
inspector. Fresh local Qiskit generation completed with 64 origins and 35 property
outcomes. The native save dialog wrote `build/workflow/ui-quantum-galaxy.json`
(250,377 bytes), which passed portable model validation afterward.

The desktop-control service stalled during initial launch, then worked for those
checks, then stopped finding the preview window during the final import check.
Native dialog import, narrow layouts and the final UI polish weren't separately
desktop-retested. Import/serialization and the final transport/editor behavior
are covered by the automated tests above. No by-ear quality or screen-reader
acceptance is claimed. The old listening application source was untouched.

All generated bundle files are below 2 MiB. The 64-room reference scene serializes
below that cap, and large exports can use compact JSON. The Qollab adapter/bundle
is prepared locally; actual hosted dependency loading, uploads/downloads, backend
execution and C++ audio remain unverified. Nothing was uploaded or published.
Direct review was performed by the Codex lead under the updated host applicability
instructions; no new review agent or external wrapper was recruited.

## Loop, Pause and tone corrections

`make test` passes all 20 Python tests without skips, both JavaScript suites and
all five C++ suites after rebuilding both Mac bundles. `make studio native
qollab-bundle` succeeds. README's SHA-256 remains the value in
[README-REVIEW.md](README-REVIEW.md); the original native host source and saved
reference artifacts are unchanged.

The initial loop regression failed because playback included six seconds of
appended tail padding. All three built-in sources now repeat at the exact rounded
32-beat boundary at 44.1/48/96 kHz, retaining wet histories. Loop off still plays
the padding. Path makes exactly four advances per repeated eight-bar phrase;
a second failing regression caught accumulated rounding producing an extra
advance at the wrap. Each two-bar boundary is now rounded independently. Custom
source tests cover enabling Loop after entering the padding and rejecting an
out-of-bounds loop endpoint. Pause tests retain the 5 ms fade, then freeze cursor,
voice gain/level/age and visits with exactly zero output.

UI tests cover one pointer press sending one explicit stop before the subsequent
click, unchanged telemetry preserving the button text node, keyboard/accessibility
activation, secondary/disabled presses, and Play retaining normal field-focus
behavior. The native view also accepts the initial activation click. Desktop
checks observed single-click Play/Pause in Shape Room and an advancing Path; their
cursors remained at 00:04 and 00:19 after stopping. This is actual desktop behavior,
not inferred from the asynchronous host mock.

The room tone suite compares wet energy with alternate filter settings while
holding source, spacing, speed, damping and decay fixed. Across 18 combinations
of three rates, three speeds and two directions, changing High cut 16,000→1,200 Hz
gives a worst 8 kHz input energy ratio of **2.68119e-6**; changing Low cut 20→500 Hz
gives a worst 30 Hz input energy ratio of **5.53683e-5**. Separate midrange fixtures
retain body. Extreme cutoff/jump tests at 8/44.1/96 kHz stay finite and bounded,
and settled Mix=0 preserves exact dry, including zeros. The decay test now uses
a fixed-duration 1 ms pulse at each rate, since a single sample has different
physical bandwidth and the filters intentionally remove high-frequency energy.
Raised-cosine grain windows, input bandwidth limiting and wet filters are described
in [DSP.md](DSP.md). Sparse echoes and grain sidebands remain; no by-ear quality
acceptance is claimed.

Version 2 Scene tests cover filter derivation, room overrides, strict range/crossed
cutoff rejection, round trips and migration of version 1 snapshots. Migration
preserves quantum data, order, existing edits and performance choices; stale
original materialized room data still fails validation. A scene exported by the
previous committed model was also successfully loaded with its edits intact.
Native 16/64-room scene tests show that changing only a selected room's filter
changes C++ output, and reject invalid cutoff snapshots.

ASan/UBSan passes updated 64-room Core, Session, reverse balance and tone suites,
plus native 16/64-room and filtered Scene renders. Additional commands:

```sh
clang++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Icpp cpp/test_tone.cpp -o build/test_tone_sanitized
./build/test_tone_sanitized
./build/studio_sanitized --scene-smoke build/workflow/room-filtered.json
```

The refreshed listening exports retain byte-identical dry inputs within each
comparison and pass RMS balance/reproducibility checks. The local Qollab bundle's
largest file is 266,972 bytes; it remains a local preparation, with hosted audio
and backend verification still outstanding.

## Browser core (WASM) and hosted-run research

Commands run on October 10, 2026:

```sh
make build/test_wasm_api && ./build/test_wasm_api
clang++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Icpp cpp/test_wasm_api.cpp -o build/test_wasm_api_sanitized && ./build/test_wasm_api_sanitized
PATH=/opt/homebrew/bin:$PATH make wasm        # Emscripten 6.0.12 → build/wasm/manyspace-core.js, 73,995 bytes
node tests/test_wasm_core.js
make qollab-bundle                              # 10 files, all below 2 MB
make local-harness
make test && make test-studio
```

`cpp/wasm_api.cpp` renders a 64-room, 48 kHz, two-second mixed-block sequence
(128/4096/511/1024-frame blocks) **bit-identical** to `Session` driven directly, rejects
bad counts, budgets, controls, probabilities and oversize blocks, reports two active
voices after a leap, and prepares 16-room reverse/half treatments. ASan/UBSan report no
error. `tests/test_wasm_core.js` evaluates the compiled module the way the browser host
does, drives `web/wasm-host.js` with a fake `AudioContext`, and checks paused silence,
finite audible bounded output (peak below unity), leap/room changes in telemetry,
knob/bus commands, rejection of custom WAV and malformed scenes, and re-preparation with
a 16-room Shape path scene. `tests/test_ui.js` now also covers the hosted path (lazy
host creation, prepare/bus/leap routing, no Load WAV button).

Dan auditioned `build/local/` in Chrome on October 10, 2026 and confirmed the hosted
UI plays through the WASM core. Not verified: any run inside Qollab (sandbox, IonQ
backend, `sendFile`, file picker). The
docs/runtime research is recorded in [QOLLAB-RUNTIME.md](QOLLAB-RUNTIME.md) and
[HACKATHON-RULES.md](HACKATHON-RULES.md); the hosted probe is `probe/qollab/`.

## Per-room stereo spread

`make test` passes 20 Python tests without skips, both JS suites, seven C++ suites
and the compiled WASM host test. `make studio native` builds both local apps;
`make qollab-bundle local-harness` refreshes the browser artifacts. New stereo
tests cover 24 rate/speed/direction cases, centered wet output, dry unity,
127/1024-block identity, retained tail width, rapid visits with 4/8 engines and
invalid values. Worst mono wet RMS level difference: **0.0141972 dB**; worst
half-spread side RMS ratio error from 0.5: **0.000235313**. ASan/UBSan on
`cpp/test_stereo.cpp` reports no error.

Native scene smoke tests confirm a selected-room spread edit changes rendered
output, reject out-of-range/missing spread, and render migrated version 2 scenes.
The compiled browser core also renders centered wet audio at 0% and stereo wet
audio at 100%. JS tests cover percent-to-0–1 conversion, range editing,
roundtrips, and validation-before-migration of version 1/2 scenes.

An isolated comparison against the previous committed `cpp/core.hpp`, using the
same Session, pluck source, 127-frame blocks and one room change, produces the
same float-output hash at full spread: **1724364751656994706** for both versions.
Previous preset/header files and the original native app source remain unchanged.

The initial Emscripten rebuild failed because the managed sandbox could not write
the Homebrew cache. Copying its existing 35 MB cache under `build/` and running
`EM_CACHE=/Users/danvogel/code/quantum-reverb/build/stereo-emscripten-cache make wasm`
succeeded without installing anything or changing the system cache. This is a
local build workaround, not a hosted-runtime test. Stereo behavior and known UI
export limitations are documented in [STEREO.md](STEREO.md).

ASan/UBSan on the updated seven-float WASM shim also passes. Browser tests reject
both an older six-float module and one missing the room-stride API. Live Studio
UI verification confirms a 0% Spread edit updates the map's accessible room label,
prepares native playback, advances the cursor and pauses on a single click.
Restore derived values returns the selected room to 100% and removes the edit.
Studio is left paused in Shape; the restored listening setup also passes native
scene rendering. These observations do not substitute for a by-ear stereo review.

## Room character — October 10, 2026

Implemented ten additions (eleven numeric fields, with separate modulation rate)
in the C++ core, scene v4 model, native snapshots, 18-float WASM interface and Shape
UI. See [ROOM-CHARACTER.md](ROOM-CHARACTER.md) for exact controls and interpretation.
No circuit/qubit changes, external backend jobs or hosted upload were performed.

Verified `make test`: 23 Python tests, model/UI suites, eight C++ suites and compiled
WASM host checks pass. Native scene smoke tests change every new field independently
and verify a changed audio-energy result; missing/null fields are rejected. Model
tests cover round-trip, finite/range/integer validation, v1–3 migration and stable
character after rearrangement. UI tests cover all map displays, percent conversion,
per-room overrides, global ranges and uninterrupted comparison changes.

`cpp/test_character.cpp` covers 8/44.1/48/96 kHz and exact 1/127/511-frame block
invariance, separate audible-energy changes for all ten additions, pre-delay onset
shift (within two frames), attack-preserving bloom, duck recovery, held late energy,
shimmer decay at 0.5/1.5× and Decay 8, retained departing character, 4/8-engine rapid
visits, age targets and zero process/jump allocations. Diffusion late energy relative
to its neutral impulse fixture changed +0.394/+2.311/+2.641/+0.553 dB at those four
rates; this is a fixture measurement, not a universal decay/loudness bound.

The full-feature 48 kHz history/allpass buffer payload is 1,061,728 bytes for four
voices and 2,123,456 for eight. In the final optimized native test run, eight voices
at 4096 frames took at most 1.881 ms on this machine (85.33 ms block deadline).
Compiled WASM with eight active visits processed and checked 48×4096 frames in
237.9 ms aggregate in Node's fake AudioContext. This includes JavaScript sample
validation and is not a measured Qollab/browser callback maximum. Timing varies
with host load. The single-file compiled module remains below 2 MB.

ASan/UBSan character tests pass using:

```
clang++ -std=c++17 -O1 -g -fsanitize=address,undefined -Icpp cpp/test_character.cpp -o build/test_character_sanitize
./build/test_character_sanitize
```

A separate before/after C++ neutral-descriptor comparison rendered 127-frame blocks,
reverse 0.5×, Spread 0.6, Mix 0.79, Decay 6.9 and a room change. Both float hashes were
`8237379043731532635`; new defaults preserve that legacy output bit for bit.

`make listening` now exports ten original musical A/B pairs under
`build/listening/character/`. Identical-source, finite/headroom, RMS match (<0.1 dB),
measurable wet difference and repeated-file hash checks pass. Hold's baseline already
has diffusion; Shimmer's baseline already uses 1.5× grains. No subjective listening
preference or physical-room realism has been claimed.

Native Studio UI verification: retained Plucks / Mix 79% / Decay 6.9 / Sensitivity
76% setup; Space, Movement and Feedback controls show derived values; editing
room-57 Diffusion to 65% updates its tile and prepares audio; Play advances the
cursor, display selection preserves playback and single-click Pause works. Restore
derived removes the test edit. Studio is left paused with fresh character defaults;
`build/character-listening.scene.json` preserves that setup. Source reference preset
and README remain unchanged.

## 256-room experiment — October 10, 2026

See [SCALE-256.md](SCALE-256.md). `make test-scale256` passes exact generation,
reproducibility, one actual local BasicSimulator job at 22 qubits/65,536 shots,
coverage of all 256 origins (209–300 shots), 16×16 Hilbert/model/UI behavior,
native snapshot rendering and compiled WASM eight-visit playback. Native exact
127/511-frame output at 44.1/48/96 kHz and callback-allocation checks pass; ASan/UBSan
pass. The 23-test Python regression suite and existing JS/C++/WASM checks pass.

Exact compact Galaxy/Scene sizes are 2.86/5.90 MB; sampled sizes are 3.81/6.11 MB.
Portable serialization and full native-file import correctly reject them at the
existing 2 MB limit. A smaller audio-only native snapshot passes, but is not a
portable Galaxy/Scene. Default generation remains 64; 256 is a local experimental
path. After explicit launch authorization, desktop native GUI inspection confirmed
the 16×16 map, room 255 selection, Next wrap to 0 and playback telemetry. A remaining
64-room command-bridge limit was found and fixed, with prepared-count regression
tests passing for 16/64/256. The cursor later showed 12 seconds with playback paused.
No subjective sound-quality claim is made; see SCALE-256.md for the GUI check's scope.

Dan's decision (Oct 10, 2026): skip 256 for the hackathon on the file-size limit alone.
The Studio build shows the 16 × 16 curve and the leaps across it well (Dan's words:
"really cool"). Carried as future work in the interface's Appendix copy (leaner matrix
encoding or loading in parts). The notebook design sketches' map is hard-wired to 8 × 8,
but the hosted interface builds on web/ui.js, which already handles 16 × 16.

## Multi-Verb notebook (October 10–11, 2026)

The replacement Qollab presenter, Each loop boundaries, custom-input staging,
processed stereo take capture, trigger migration and word descriptions passed the
full local regression suite and compiled WASM checks. Chrome intervals of 75.7 s
(Each loop) and 78.4 s (Auto) reported zero callback-execution overruns; that metric
is not a listening or main-thread dispatch guarantee. Native 256-room invariance
and allocation checks still pass. The new notebook has not been hosted or submitted.

[WEB-INTERFACE-VERIFICATION.md](WEB-INTERFACE-VERIFICATION.md) records exact
measurements, downloaded take/log artifacts, 16-room bridge-fixture provenance,
responsive screenshots and the remaining headless/screen-reader/real-file-upload
checks. [WEB-INTERFACE.md](WEB-INTERFACE.md) records setup, limits and architecture.
