# Manyfold

**One room, many variations.** Manyfold is a reverb whose rooms come from a quantum
circuit. Run the circuit once and you get a *manyfold*: a collection of room variations
and the probabilities connecting them. Then play a sound through it and leap from room to
room. Each room has its own size, speed, brightness and direction; the old room keeps
ringing while the new one takes over.

Built for the Qollab + IonQ hackathon, October 9–11, 2026. Theme: **Quantum Playground**.
This effort is supported by Qollab & IonQ.

- Project on Qollab: **[link to the published project]**
- Demo video (2–3 minutes, no sign-in): **[public video link]**
- Source: <https://github.com/danvogel/manyfold-reverb> (MIT)

## What it does

The web page is a lab notebook with five pages:

| Page | What you do there |
|---|---|
| Cover | Read how it works and open the ready-made manyfold |
| A · Generate | Choose 16 or 64 rooms, the walk depth and phase, a seed and shot count; run the circuit on the backend Qollab selected; see which outcomes came up and the leap odds |
| B · Shape | Set how much the rooms vary, how they are ordered along the curve, edit any room, compare the quantum leap odds with plain chance, save the manyfold |
| C · Play | Pick a sound or upload your own, choose when rooms change (by hand, when a note is loud, or at the end of every loop), set immersion and linger, switch the effect on, record a take |
| Appendix | The loaded manyfold's parameters, how it is made, the circuits as OpenQASM, credits |

A ready-made 64-room manyfold loads on the cover, so you can play before generating.

## What problem it explores

Can superposition and interference be *the instrument*, not just a dice roll?

- **The rooms are measurement outcomes.** A six-qubit property circuit (RY, RZ, a CNOT
  chain, RX) is measured once per room; its six bits set reflection spacing, playback
  speed and damping, and the parity picks forward or reversed tails.
- **The leap odds are interference.** The rooms are the corners of a hypercube (16 = 2⁴,
  64 = 2⁶). A coherent walk, RX mixing and RZ twists with a CZ chain, repeated *depth*
  times, runs from every corner at once. Where it lands sets the odds of leaping there.
  The phase control changes which paths cancel and which pile up.
- **Plain chance for comparison.** The same walk computed with the phases removed after
  every step gives a classical random walk. For the bundled 64-room manyfold the two
  disagree by a total-variation distance of 0.38 on average and at least 0.29 from every
  room, so the quantum odds are audibly not the classical ones. Shape shows both.
- **One job.** Source, destination and property registers are entangled and measured
  together: 18 qubits for 64 rooms, 14 for 16, so an IonQ simulator run fits one job.

The audio itself is classical: a small portable C++ reverb core, compiled to WebAssembly,
runs inside the Qollab page. No quantum speedup, no literal many-worlds claim. The
circuit decides *what the rooms are* and *where you can go*.

## How to run it on Qollab

1. Open the project page and press **Run**. Choose the built-in simulator, or any free
   simulator, when Qollab asks. Judges can use the free local simulator; results do not
   depend on IonQ hardware.
2. The notebook appears below. Press **Play** on the cover to hear the ready-made
   manyfold. Switch the effect on in step 5 of Play.
3. To make a new manyfold, open **A · Generate**, choose a size and press the button.
   The run sends exactly one circuit to the selected backend. On an IonQ simulator, start
   with a small shot count; noise-model simulation time grows with shots.
4. Downloads (a manyfold as JSON, a recorded take as WAV, the experiment log) arrive
   through Qollab's download console.

Browsers: recent Chrome, Edge, Brave or Opera, or Firefox 153+. Safari and iOS cannot run
Qollab projects. The audio never leaves your browser; only the circuit goes to the backend.

## How to run it locally

Requirements: Python 3, `qiskit==2.5.2`, `numpy==2.4.6` (see `requirements.txt`), Clang
with C++17, Make. Rebuilding the browser core needs Emscripten 6.x; the compiled core is
a single file under 100 KB.

```sh
make test                      # Python, C++ and JS test suites
make test-wasm                 # compiles the core with em++ and tests it in Node
make local-harness qollab-bundle
python3 tools/notebook_server.py
```

Open <http://127.0.0.1:8765/>. The local server runs Generate with Qiskit's
BasicSimulator; Shape and Play work from any static server. `make studio` builds a
native macOS version of the same Generate/Shape/Play flow in `build/ManyspaceStudio.app`.

## How to replicate or extend it

- `manyspace/galaxy.py`: the circuits (`property_circuit`, `walk_circuit`,
  `single_job_circuit`), exact and sampled generation, validation, OpenQASM export.
- `manyspace/workflow.py`: the one-job backend run and the bridge Qollab's page calls.
- `web/model.js`: the manyfold file format (schema version 4), outcome-to-room mapping,
  range limits, ordering, edits and migration of older files.
- `cpp/core.hpp`, `cpp/session.hpp`: the reverb core and the session that stages room
  changes, triggers and tails. `cpp/wasm_api.cpp`: the browser shim.
- `web/wasm-host.js`: runs the core through Web Audio. `web/notebook*.js`, `web/notebook*.css`:
  the notebook presenter.
- `docs/QUANTUM.md`, `docs/DSP.md`, `docs/EVIDENCE.md`: the method, the sound, and the
  measurements behind every claim above. `docs/QOLLAB-RUNTIME.md`: what the Qollab sandbox
  allows, verified from its served source and a live probe.

Ideas we did not finish: 256 rooms fits a 22-qubit simulation and runs locally, but its
file exceeds Qollab's 2 MB per-file limit; a leaner encoding would fix it. The tails are a
damped feedback delay with resampled grains, not a dense production reverb.

## Disclosure

- **Advance work.** The ideas (a set of rooms, a curve through them, quantum transitions,
  independent departing tails) were explored before the window with AI assistance, and an
  earlier prototype informed the build brief. Some viability sketches happened on the
  morning of October 9, hours before the start. All implementation in this repository was
  started fresh after the hackathon began; no earlier code was inspected or reused.
  Details: `docs/PROVENANCE.md`.
- **AI assistance.** Concept, ideation and mutation by Dan Vogel. Code by Dan Vogel
  supported by AI: OpenAI Codex and Anthropic Claude assisted with design, code, tests,
  copy and documentation. Translation and adaptation for WebAssembly and Qollab by AI and
  Emscripten.
- **Layout and design.** Drawings, hand-lettering, layout and copy by Dan Vogel and his
  daughter, supported by AI.
- **Sounds.** The built-in phrases are synthesized by this repository's own code.
- **Third-party components, none vendored as source:** Qiskit (Apache-2.0), NumPy
  (BSD-3-Clause), Emscripten runtime glue embedded in the compiled core (MIT / NCSA),
  fonts Architects Daughter (SIL OFL 1.1) and Permanent Marker (Apache-2.0), with their
  license texts in `web/fonts/`. No API keys, accounts or secrets are in the project.

## License

MIT. See `LICENSE`.
