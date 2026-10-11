# Qollab runtime: verified capabilities and limits

Sources read October 10, 2026 (UTC): `https://qollab.xyz/learn/docs/runtime-environment`,
`/learn/docs/js-qiskit-projects`, `/learn/docs/compute-backends`, `/learn/docs/faq`, plus
Qollab's served runtime itself: `https://qollab.xyz/jsworker.html` (HTTP headers and
body), the project-page chunk `/_nuxt/BWPMdZ7B.js` and the frame runtime
`/_nuxt/jsworker-C6a5_czr.js`. Bundle hashes change on deploy; re-check the served
files if observed behavior differs. Hosted probe results, when available, belong in the
last section. See [HACKATHON-RULES.md](HACKATHON-RULES.md) for the rules and
[RUNTIME.md](RUNTIME.md) for the earlier dated check.

## Project shape

- Top-level `index.html` + `main.js` make a JS project. `main.css` is optional and the
  only stylesheet loaded. `index.html` is inserted into the page body: `<html>`,
  `<head>`, `<body>` and `<script>` tags are dropped. Other `.html`/`.css` files are
  ignored; build extra markup from JavaScript.
- `main.js` imports project JS with `./` or `../` paths (`.js`/`.mjs`, index files
  resolved), Python modules by dotted name (`import * as m from 'quantum.bell'`),
  installed Python packages by name (`import { QuantumCircuit } from 'qiskit'`), and
  anything else by full URL, e.g. `https://cdn.jsdelivr.net/npm/<pkg>/+esm`. `three`,
  `three/addons/`, `postprocessing`, `@tweenjs/tween.js` and `meshline` are bundled.
- Python functions are called from JS with `.callPromising(...)` or
  `.callPromisingKwargs(...)`; plain calls fail on IonQ backends with a ConnectionError
  about XHR. Return values are proxies; `.toJs()` unpacks them. Default imports of
  Python modules give `undefined`.
- `backend` is pre-created from the Select QPU dialog and available in `main.js` and
  its imports. Never construct a backend. `await backend.run(circuit, { shots })`.
- Python packages go in `requirements.txt`; Qiskit is always present (`>=2.5.2,<3.0.0`
  unless pinned). Pure-Python wheels and Pyodide-built packages install; NumPy, SciPy and
  Matplotlib come with Pyodide.

## Limits

- 100 files per project, **2 MB per file**, 200 MB per account.
- No run time limit; Stop gives Python five seconds.
- No storage between runs. `localStorage`, `sessionStorage`, IndexedDB and cookies are
  unavailable. Ship saved states as project files.
- Data files load only when their path appears as a **plain string literal** in Python
  (not an f-string, `+` or `os.path.join`). Binary files work. Every named file is held
  in memory. JS cannot `fetch()` project files; Python must read them and return them.
- Networking: GET only (and OPTIONS). POST/PUT/DELETE fail with "network access is
  restricted". The frame has origin `null`, so servers must send
  `Access-Control-Allow-Origin: *`. No cookies or credentials are sent.

## Backends

| Backend | Group | Qubits | Jobs per run | Cost |
|---|---|---:|---|---|
| Built-in simulator | Local, in browser | 24 | unlimited | free |
| AWS Braket local simulator | Local, in browser | 25 | unlimited | free |
| IBM noise-model simulators (8 devices) | Local, in browser | 120–156 | unlimited | free |
| IonQ Aria 1 / Aria 2 (noise models) | Remote, IonQ cloud | 25 | **one** | free |
| IonQ Forte 1 / Forte Enterprise 1 (noise models) | Remote, IonQ cloud | 29 | **one** | free |
| IonQ Forte 1 / Forte Enterprise 1 | Hardware | 36 | **one** | credits |

Remote noise-model simulation runs once per shot, so time grows with shots; start
small. Qubit figures are ceilings, not guarantees. IonQ publishes no wait times or job
timeout. A second IonQ job in a run stops the run with an error.

## The JS runtime frame (verified from source)

- The project page creates `<iframe sandbox="allow-scripts" class="js-runtime"
  src="/jsworker.html?1#<runtime module URL>">`. Only `allow-scripts` is set: the frame
  has an **opaque origin** and lacks `allow-same-origin`, `allow-downloads`,
  `allow-forms`, `allow-popups`, `allow-modals` and `allow-top-navigation`. Inside the
  UI, `<a download>`, `window.open` and `target="_blank"` links are blocked. Downloads
  go through the `sendFile(name, contentType, data)` global, which the console turns
  into a download with a preview or copy button.
- `jsworker.html` is served with this CSP:
  `default-src 'self' blob: data: api.cms.qollab.xyz api.qollab.xyz pub.qollab.xyz
  www.google.com *.google-analytics.com quantum-garden.s3.amazonaws.com github.com
  raw.githubusercontent.com e.qollab.xyz www.facebook.com cdn.jsdelivr.net pypi.org
  files.pythonhosted.org; style-src 'unsafe-inline' *; font-src *; script-src 'self'
  'unsafe-eval' 'unsafe-inline' www.googletagmanager.com e.qollab.xyz cdn.jsdelivr.net
  connect.facebook.net; object-src 'none'; frame-src 'self' pub.qollab.xyz
  www.google.com www.youtube.com qollab.simplybook.me qollab.simplybook.it;
  frame-ancestors 'self'; connect-src *; worker-src 'none'; img-src blob: data: *;
  media-src blob: data: *`. Also `Cross-Origin-Opener-Policy: same-origin` with no
  COEP, so `SharedArrayBuffer` is unavailable.
- The runtime script neuters on `window`: `postMessage`, `Worker`, `SharedWorker`,
  `WebSocket`, `WebSocketStream`, `BroadcastChannel` and `alert` become no-ops;
  `Notification` is null; `indexedDB.open` throws "This API is disabled"; `navigator`
  is replaced by `{languages, userAgent}` (no `mediaDevices`, so no microphone);
  `XMLHttpRequest` throws "XHR is not supported, please use fetch() API". `fetch` is
  wrapped to enforce GET and to route IonQ calls through api.qollab.xyz.
- **Not touched:** `AudioContext`, `AudioWorklet`, `WebAssembly`, `MessagePort`,
  `setTimeout` (returns a promise when called without a function),
  `URL.createObjectURL` (wrapped only to convert Matplotlib figures to SVG blobs).
  Pyodide (`cdn.jsdelivr.net/pyodide/v314.0.7`) runs in this same frame on the main
  thread using WebAssembly JSPI, so WebAssembly compilation demonstrably works and
  `'unsafe-eval'` covers it.
- Project JavaScript is resolved by the runtime and evaluated in-page (Babel
  `transform-modules-commonjs`, then eval). Bare specifiers are tried as Python modules
  first, then as dynamic `import()`. URL specifiers use dynamic `import()` and are
  subject to `script-src`, which allows `cdn.jsdelivr.net`.

## Implications for Manyspace audio

- Web Audio works in this exact frame: the Quantum Hackathon Soundtrack JS project
  (synthesized voices, convolver reverb) runs there. A click inside the frame satisfies
  the autoplay gesture requirement.
- `audioWorklet.addModule(blobURL)` will be refused: `script-src` has neither `blob:`
  nor `data:`. The local probe in `probe/index.html` tests exactly that pattern and is
  therefore not representative of Qollab.
- `worker-src 'none'` does **not** govern worklets. CSP Level 3 §6.8.1 routes the
  `audioworklet` and `paintworklet` request destinations to `script-src-elem` (falling
  back to `script-src`) and only `worker`, `sharedworker` and `serviceworker` to
  `worker-src`. A worklet module served from `cdn.jsdelivr.net` (for example
  `https://cdn.jsdelivr.net/gh/<user>/<public repo>@<commit>/worklet.js`) satisfies
  `script-src`. Worklets inherit the document CSP, so WebAssembly compilation inside
  the worklet is allowed by `'unsafe-eval'`; a compiled `WebAssembly.Module` can also be
  passed in through `processorOptions`. This path is unverified in a live run.
- `ScriptProcessorNode` needs no script fetch at all and can run the WASM core on the
  main thread. It is deprecated but universally supported. Qiskit generation runs on the
  same thread, so do not generate while audio plays.
- WASM bytes can arrive without external hosting: as an Emscripten `-sSINGLE_FILE`
  module stored as a project JS file (base64 inside, decoded with `atob`), or as a binary
  data file read by Python (`open("assets/manyspace.wasm", "rb")` with a literal path)
  and returned to JS. Keep each file under 2 MB.
- Not available: microphone, downloads other than `sendFile`, outbound links from the
  UI, any Safari or iOS browser (prior documented support limits). `<input type="file">`
  selection/text read and synthetic WAV decode passed the October 11 hosted probe;
  the current C++ adapter still rejects custom audio. See the hosted report below.

## Hosted probe results

Hosted on October 11, 2026 in a private unpublished draft: see
[QOLLAB-HOSTED-PROBE-2026-10-11.md](QOLLAB-HOSTED-PROBE-2026-10-11.md) for the
actual matrix, timing, errors, corrected probe imports/export lifetime and preserved
evidence. Dan confirmed audible sounds during most tests collectively. Four/eight-tail
C++ WASM ScriptProcessor checks, file selection/decode, Python bridge, local Qiskit
and active-run sendFile passed; blob AudioWorklet failed, CDN worklet remains unknown.

Local equivalent, October 10, 2026: the compiled core (`make wasm`, Emscripten 6.0.12)
runs through `web/wasm-host.js` in Node with a fake `AudioContext`
(`tests/test_wasm_core.js`). Dan auditioned `make local-harness` served over
`http://127.0.0.1:8765` in Chrome on October 10, 2026: the hosted UI played the demo scene
through the WASM core. Neither run is the Qollab sandbox.
