# Provenance and boundaries

Manyspace Reverb is a provisional name. This fresh implementation began on
2026-10-10 UTC (within the supplied October 9–11 build window). Dan supplied
BUILD-BRIEF-MANYSPACE.md, which predates this implementation and remains intact.

Prior to the window, Dan explored the quantum room galaxy, Hilbert traversal,
quantum transitions, independent departing tails, speed/reverse behavior and
portable effect-module architecture with AI assistance. An earlier
QuantumReverbAtlas prototype and Subsong framework informed the supplied brief.
No earlier implementation code/history was inspected or copied here. During local
UI planning, a permitted historical Subsong plan was read for conceptual host
context; Subsong source and prior prototype code were not inspected or reused.
This repository is new
implementation, not a claim that the underlying ideas originated in the window.

OpenAI Codex assisted with design, code, tests and documentation. The local
codex-advisor wrapper specified by the skill was absent; no independent advisor
result was obtained. All generated audio uses original synthesized input.
The newer listening set contains original eight-bar plucked, drum and chord
material, generated deterministically by this repository's C++ code.

Fresh source is prepared for MIT release, but no public release, remote, account
operation, license acceptance or grant use is authorized. No third-party DSP
code, soundtrack, JUCE, DaisySP or Subsong source is included. Qiskit 2.5.2
installed package declares Apache-2.0; NumPy 2.4.6 is BSD-3-Clause with bundled
component notices (installed expression: BSD-3-Clause AND 0BSD AND MIT AND Zlib
AND CC0-1.0). They are runtime dependencies, not vendored/relicensed code.
Before distribution, preserve their full notices and review bundled binaries.
The browser core is compiled with Emscripten 6.0.12 (MIT / University of Illinois NCSA
dual license, installed by Dan on 2026-10-10). Its JavaScript runtime glue is embedded
in `build/wasm/manyspace-core.js` and therefore ships inside the Qollab bundle; credit
Emscripten and keep its license text with any published copy. No Emscripten source is
vendored into this repository.

Required eventual project-page attribution: “This effort is supported by Qollab & IonQ”.
Submission, public MIT licensing, video, hosted verification, twelve-month
availability and award obligations remain future user decisions/work.

The adversarial-review skill's external CLI wrapper was also absent. A separate
read-only Codex review agent and the lead's direct review were used. They found
and fixed source/property mapping validation gaps and strengthened audible-tail
and eviction tests. This is not a claim of independent model/vendor review.
The listening-host review also identified EOF replay and display/compiled-map
risks, which were addressed. Direct ASan verification caught a float ring-wrap
overflow and confirmed its fix, independently of the review agents.

The subsequent Generate/Shape/Play interface, SVG diagrams, CSS, artifact model
and native bridge were authored freshly here, using system WebKit rather than
copied design assets or a third-party frontend kit. The updated host-applicability
instruction exempts Codex leads from Claude's external recruitment workflow.
This task used direct review and tests; it did not recruit another agent/model.

## October 11 distribution preparation

Dan created `danvogel/manyfold-reverb` publicly with an MIT license and requested a
minimal repository plus full Qollab testing. That request supersedes the original
no-remote/no-upload boundary for this project preparation and private hosted tests.
A curated source export preserves the original README for Dan/dot review and does
not include authoring Git history, native Studio, full-resolution artwork originals,
or unrelated earlier work. Public hackathon submission and paid hardware remain
separate actions. Qollab's draft creation terms still need action-time confirmation.
