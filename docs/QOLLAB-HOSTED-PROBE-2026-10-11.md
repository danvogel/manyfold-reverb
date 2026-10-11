# Qollab hosted probe results — October 11, 2026 UTC

Tested 2026-10-11 UTC in Chrome **154.0.0.0** on Dan's Mac (browser-reported user agent; full patch version not available). Profile dmv. Exactly one new test project created:
https://qollab.xyz/u/dmv/manyspace-runtime-probe-unpubl

UI verified **Draft / Editing**, **No versions yet** at creation, and Draft / Editing again after completion. Official creation guide confirms projects start as private drafts and drafts with no public version remain private. No Publish or submission action. The linked Terms and Contributor Policy were accepted through Create after Dan's explicit 01:46 UTC approval (Sentinel_cdd68c2e3b5881919cd04e37858afa9d).

## Actual matrix

| Check | Hosted result | Evidence |
|---|---|---|
| Gesture-triggered audio graph | PASS instrumented; individual audition UNKNOWN | AudioContext resumed to running at 48 kHz; baseline scheduled 440 Hz for 1 s. Decoded WAV analyser peak 0.0499893 and context clock 1.056 s. Agent did not audition individual checks; see user confirmation below |
| Native WebAudio | PASS | Oscillator/gain/destination and decoded buffer/analyser/destination worked |
| Synthetic local file selection | PASS | Native picker selected synthetic.json; sandbox read 47 chars |
| Local WAV decode | PASS | Synthetic 96044-byte, 440 Hz WAV decoded to 48000 frames, mono, 48 kHz, 1 s |
| C++ WASM + 4096-frame ScriptProcessor adapter | PASS | Current existing core and web/wasm-host.js processed finite nonzero stereo output in actual sandbox; four/eight-tail metrics below |
| Minimal blob AudioWorklet | FAIL | AbortError: Failed to load worklet module script: blob:null/c8ea11aa-0aa9-40b7-8061-9913b8c1d3ed (a dependency or cross-origin script failed to load) |
| CDN AudioWorklet | UNKNOWN | Existing probe had no MANYSPACE_WORKLET_URL; no unrecognized third-party module installed |
| WASM compile/run | PASS | Empty module instantiated; actual C++ core init 5.6 / 2.7 ms and successful prepare/command/process/telemetry |
| Python bridge | PASS after diagnostic import correction | Static import transferred Uint8Array and base64, each 65536 bytes |
| Free local Qiskit | PASS | basic_simulator, 18 qubits, depth 7, 1024 shots, 960 distinct outcomes, all 64 source labels, 0.157 s |
| Supported sendFile export | PASS during active run | Qollab console displayed probe-report-final.json; clicked Download; actual file recovered and parsed into scratch evidence |
| Four/eight tails and rapid leaps | PASS | 36 leap commands at ~200 ms intervals, reference scene, decay 8 s; four/eight active voice budgets reached with finite output and no overruns/late callbacks |

## Timing and output evidence

48 kHz, 4096 frames = **85.333 ms** per buffer. Late threshold = 128 ms. Adapter maximum includes diagnostic output scanning and JS buffer copies.

| Reference-scene run | Wall duration | Callbacks | Max processing | Max callback gap | Late / overruns | Peak output |
|---|---:|---:|---:|---:|---:|---:|
| Four tails | 10082 ms | 118 | 15.7 ms | 85.5 ms | 0 / 0 | 0.159352 |
| Eight tails | 10150 ms | 119 | 21.7 ms | 85.6 ms | 0 / 0 | 0.159353 |

Both observed 34 room changes during 36 leaps, maximum active voices equal to budget, finite output throughout. Snapshot playing=true was captured before each test's explicit play=0 and host.close; contexts were then closed. Quantum generation ran only after both playback tests closed. Baseline JS sine: 117 callbacks over 10 seconds, zero late intervals. These are short reference-scene smoke checks, not a measured end-to-end latency, long-duration reliability, or perceptual listening result.

## Failures and diagnostic corrections

Original unchanged probe failed Python and Qiskit before job dispatch: **TypeError: Failed to resolve module specifier 'probe.bridge'**. Replaced its dynamic imports with one supported static import in scratch main.js. This resolved both; it is a probe loading error, not a blocked Python runtime.

Original sendFile returned without throwing but produced no console artifact once the top-level run ended. Diagnostic main.js keeps its run active until Finish; console artifacts then appeared. Download event API timed out after 10 seconds despite successful browser download; expected Downloads file existed, contained this run's exact results, and was copied/parsed into scratch evidence.

Extension setFiles initially failed: file-URL access not enabled. Supported native macOS picker succeeded without changing permissions. No security protection or CSP bypass, and no chrome://version retry.

## Scope and recommendation

Dan subsequently reported: **“my laptop definitely played sounds through most of the tests”** (user message Sentinel_d288aea163788191a9eff7875d403796). This confirms audible playback during most tests collectively. It does not attribute listening to every individual check or establish glitch-free output, sound quality, or long-duration reliability. Raw exported evidence still says heard=unknown because it predates this user confirmation; it is preserved unchanged.

The existing ScriptProcessor C++ WASM route is viable in actual Qollab on this Mac for the tested four/eight-tail reference workloads. Keep quantum generation outside playback, use supported static Python imports, and keep the run active while calling sendFile. File selection and native decode are viable; **the current production wasm-host.js still explicitly rejects loadAudio/custom WAV**. This test confirms the browser capability, not implemented custom-input integration. Blob AudioWorklet is blocked; CDN delivery remains unverified.

Only isolated probe diagnostic files and the new unpublished test draft changed. Source probe manifest rechecked unchanged, including README; no source-repo edits, CLI writer interactions, existing-project edits, new permissions, hardware/paid/remote jobs, publishing, or submission. No third-party code installed. All added code consists of the existing local C++ core/adapter and small diagnostics. Finished runtime is Idle / Done.

## Preserved evidence and provenance

Small text/JSON evidence is copied under [qollab-hosted-probe-2026-10-11/](qollab-hosted-probe-2026-10-11/). Files keep their original names and byte content. No compiled core, binary fixture, account credential, or secrets were copied into docs. The original and diagnostic probe sources remain separate in the scratch task.

### Evidence files

- probe-report-final.json — actual sendFile download (13 lines; export PASS is logged after the serialized report, so not inside this file).
- timing-results.json — exact adapter snapshots extracted from download.
- baseline-hosted.txt — original probe outcomes and errors.
- creation.txt and agreement-verification.md — creation/consent and linked agreement evidence.
- diagnostic-changes.md and diagnostic-manifest.json — scratch diagnostic provenance.
- source-manifest.json — original probe SHA256 manifest.

## Earlier public-source findings: constraints, not new empirical tests

Read October 10, 2026; see [QOLLAB-RUNTIME.md](QOLLAB-RUNTIME.md) for the full served CSP and runtime source details, and [RUNTIME.md](RUNTIME.md) for the older superseded check. The hosted probe confirms only the matrix above. These documented/source-inspected rules were not independently retested wholesale:

- Project shape: top-level `index.html`, `main.js`, optional `main.css`; HTML wrapper/script tags removed, other HTML/CSS files ignored. Supported static imports include relative project JS, dotted Python names, installed Python packages, full CDN URLs. Default Python module imports give undefined. Use precreated selected `backend`, `.callPromising()` / `.callPromisingKwargs()`, and proxy `.toJs()`; never construct an alternate backend. Requirements select compatible pure-Python/Pyodide packages; probe requested `qiskit==2.5.2`, without separately printing installed version.
- Limits: 100 files/project, 2 MB/file, 200 MB/account; no documented run-time limit, Python Stop grace five seconds; no persistent storage/cookies. Binary asset loading requires plain literal paths in Python; JS project-relative fetch is unsupported. Named assets held in memory. Networking GET/OPTIONS only, no credentials; opaque-origin CORS requires wildcard response access.
- Frame: source showed `sandbox="allow-scripts"`, no same-origin/download/form/popup/modal/top-navigation allowance. Use `sendFile` rather than an iframe download anchor. Workers/SharedWorker/postMessage/network socket APIs are neutered; navigator reduced to languages/userAgent, no microphone/mediaDevices or MIDI surface; IndexedDB/XHR disabled; SharedArrayBuffer unavailable (also observed in probe).
- CSP: `worker-src 'none'`; `script-src 'self' 'unsafe-eval' 'unsafe-inline' www.googletagmanager.com e.qollab.xyz cdn.jsdelivr.net connect.facebook.net`, with no blob/data allowance. Media allows blob/data. AudioContext, WebAssembly, AudioWorklet and MessagePort are not neutered. Pyodide and quantum work run on the main thread. The reported `location.origin` was https://qollab.xyz while the failed worklet URL was blob:null/...; do not infer a non-opaque security origin from the former.
- AudioWorklet uses script policy, not ordinary worker policy. Prior CSP analysis makes an allowed jsDelivr module a candidate, not proof of live success. No CDN module or WASM-in-worklet was tested. No restriction was bypassed.
- Backend facts from prior docs: built-in local 24q, Braket local 25q, IBM local noise simulators 120–156q, all free; IonQ remote Aria noise 25q and Forte noise 29q, one job/run, free; Forte hardware 36q costs credits, one job/run. Remote noise simulation time grows per shot. Qubit ceilings and the observed local source coverage do not guarantee success or coverage elsewhere.
- Browser support needs JSPI; prior docs exclude Safari/iOS. This test covers one Mac/Chrome configuration only. No microphone, MIDI, alternate browser, remote IonQ, paid/hardware, public access, submission, full Studio UI, long-run playback, end-to-end latency or maximal-character stress validation.

Public references: [runtime environment](https://qollab.xyz/learn/docs/runtime-environment), [JS/Qiskit projects](https://qollab.xyz/learn/docs/js-qiskit-projects), [compute backends](https://qollab.xyz/learn/docs/compute-backends), [FAQ](https://qollab.xyz/learn/docs/faq), [creation/private drafts](https://qollab.xyz/learn/building-your-first-qollab-project/create-project). The private-draft guide was read during this run. Served runtime references from prior inspection: `/jsworker.html`, `/_nuxt/BWPMdZ7B.js`, `/_nuxt/jsworker-C6a5_czr.js`; deployment hashes can change.

## Exact implementation touchpoints for another agent (not changes made here)

1. Scratch [static import / bridge calls](qollab-hosted-probe-2026-10-11/diagnostic-excerpts.md) uses `import * as bridge from 'probe.bridge';`, removing the two dynamic `await import('probe.bridge')` declarations. Python helpers and `.callPromising()` remained unchanged. Production analogous dynamic imports are in [`web/ui.js`](../web/ui.js) at lines 17 (`loadCoreText`), 377 (`generate`), 496 (initial reference load), as read when documenting. Use a static `import * as ManyspaceWorkflow from 'manyspace.workflow';` in the **Qollab-generated entry** and reference that namespace there; native/local scripts must retain their supported loading path. Do not insert a bare Python ES import into all native/local script contexts.
2. Scratch top level ends with `await new Promise(resolve => document.getElementById('finish').onclick = resolve);` (diagnostic main.js:242). Export button calls `sendFile` before Finish resolves. Production export is [`web/ui.js`](../web/ui.js) lines 355–359; Qollab bundling is [`Makefile`](../Makefile) line 109 onward. The hosted entry must retain an active run while UI exports occur. No production lifetime fix was implemented or verified.
3. Current [`web/wasm-host.js`](../web/wasm-host.js):31 rejects `performance.source === "custom"`; :107 rejects `method === "loadAudio"`; prepare passes source frame count 0 to `_ms_prepare`. [`web/ui.js`](../web/ui.js):416–417 dispatches `loadAudio`. Error: `Custom WAV input is not available in the hosted version. Choose a built-in phrase.` Browser selection/decode success is separate from feeding decoded samples into this adapter. Removing a throw alone is not custom-input support.
4. Timing instrumentation is scratch-only: callback-start gap at diagnostic main.js:147, WASM/buffer-copy/finite-scan timing :146–154, snapshot :164, reset :176. Test routine :210–230 disables the quantum button, then issues play=0 and closes the host before enabling it. Original host timing is [`web/wasm-host.js`](../web/wasm-host.js):67–68. References may drift with ongoing implementation; locate by named functions/strings as well as line numbers.

## Reproduction artifacts and hashes

Exact executed diagnostic archive (README excluded from upload) is retained at `/Users/danvogel/Documents/Codex/2026-10-10/task/qollab-test/probe-diagnostic.zip`; source directory `probe-diagnostic/`, baseline `probe-copy/`, fixtures `fixtures/`. Do not upload the evidence reports, the whole repo, or secrets. These paths supplement the self-contained documentation/evidence; conclusions do not depend on scratch availability. Earlier blanket statements in PROVENANCE.md / RUNTIME.md about no license acceptance, upload or hosted verification describe their earlier scope; this dated report records the later explicit test-only authorization and completed private-draft checks, without authorizing public release or submission.

The C++ core was 78,636 bytes, embedded as the exact source string from `build/wasm/manyspace-core.js`. Its build rule compiles `cpp/wasm_api.cpp` with dependencies `cpp/core.hpp`, `cpp/listening.hpp`, `cpp/session.hpp` using `-O2 -sMODULARIZE=1 -sEXPORT_NAME=createManyspace -sSINGLE_FILE=1`, 64 MiB fixed initial memory; prior build documentation records Emscripten 6.0.12. It was **not recompiled during these hosted checks**. Exact core SHA256 is in the table below and diagnostic-changes.md.

The adapter text came from `web/wasm-host.js`, with only diagnostic callback/timing/finite/peak counters added. Scene fields `format`, `version`, `rooms`, `transitions`, `performance` were copied from `build/workflow/scene.json`, with redundant room derived/edited/xy/id/source/outcome removed. Each test changed only budget=4/8, automatic=false, decay=8, mix=.65. Source=plucks, seed=11, loop=true, starting room=16. Native callback/destination were real browser AudioContext/ScriptProcessor, not fake test objects.

For exact replay, verify retained archive/hash, upload its unpacked files into an authorized private draft, run the free built-in simulator, execute each tail test serially, then quantum after playback closes, Python bridge with the synthetic JSON, WAV decode, Export while active, and Finish. For reconstructing when the archive is unavailable, [diagnostic-excerpts.md](qollab-hosted-probe-2026-10-11/diagnostic-excerpts.md) preserves the baseline corrections, adapter instrumentation and complete extra test routines without the compiled/binary payload. Supply the existing local single-file core matching its hash, scene matching its hash, and fixtures matching theirs. Changed build/scene hashes mean a new experiment, not exact reproduction. JSON fixture is `{"fixture":"synthetic-nonprivate","value":440}` followed by newline; WAV is one second mono PCM16 at 48 kHz, sample `round(3276*sin(2*pi*440*i/48000))` encoded little-endian with Python's standard wave header.

| Artifact | SHA-256 |
|---|---|
| `probe-diagnostic.zip` | `d072c7288c05a41273e737a0d9a961615f38dcb986fc6556cbe339aaee78590b` |
| `probe-diagnostic/main.js` | `a599226ad9c697cce6dab50d50279f64bb6ffa9e611d79a2eeae37d5a947c6d3` |
| `probe-diagnostic/index.html` | `178064905a48616b34c0e7387d47b4c9bfbd69d1b66bcd63f45018daa3d49640` |
| `probe-diagnostic/probe/bridge.py` | `473fbe1e539b2e7a67f9a2fc2b0d9c9f8015128f5e5f7a5549cc18024344f5c0` |
| `fixtures/synthetic-440hz.wav` | `ce371776883400d8355c4c36f7de746f154acf3a7b8ee2f5433291c4adcfc323` |
| `fixtures/synthetic.json` | `f0b3f4a3c3d71ae7b851e8cadd8ef4d403e12b60b110c7143fe580daf9d62ac5` |
| `evidence/probe-report-final.json` | `7172d8981ce11ebf381fa9ed7c2330efdcc9926fcf0f24d3de9c72411d3aefb1` |
| `evidence/timing-results.json` | `d092387bab4283f18cbe1e63040374796b6b721e41344e1e35a569c8d9f2fe8f` |
| Embedded `build/wasm/manyspace-core.js` | `74762e769a1a1a96464cce02a2ac177151e2e2e23345f6773449e332b0ce22b8` |
| Embedded trimmed scene (exact compact JSON literal, no trailing newline) | `05e723c32e66cfb0c2c3eecdd42f4937a03c5cc7545bd7911542c4f67c73ca2b` |
