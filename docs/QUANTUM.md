# Quantum mapping and measured comparison

Generation uses Qiskit circuits for both properties and transitions. One six-qubit
property circuit uses RY/RZ rotations, a CNOT chain and final RX rotations. Seeded
sampling of its exact measurement distribution yields six property bits per room:
bits 0–1 map linearly to 15–95 ms reflection spacing, bits 2–3 to 0.5–1.5 speed,
bits 4–5 to 0.2–0.85 damping. Outcome popcount parity selects decay/swell intent.
These are four-bin property mappings, so duplicates are possible. The six-bit
property outcome and all derived fields are checked on load.

Properties are generated first. Then best-start greedy nearest-neighbor ordering
minimizes mean successive normalized weighted Euclidean distance among the tried
paths. Weights are distance 0.5, speed 0.3, damping 0.2. Mode is not included in
that metric; mode flips can still be audible. Stable room/source IDs survive the
ordering, and transition rows/columns are permuted consistently. Hilbert coordinates
place this sequence on 4×4/8×8 maps. Hilbert geometry itself offers no acoustic
smoothness guarantee. The last-to-first Next wrap is not part of the adjacent
path metric and may make a larger jump.

The walk graph is the n-bit hypercube (n=4 or 6). Each step applies RX(1.1) mixing
on every bit, RZ(phase×(q+1)) and CZ between neighboring bit positions. Mixing can
flip multiple bits during a step; this is a circuit walk on the hypercube, not a
coin-controlled nearest-neighbor lattice walk. The default burst has depth 3,
phase 0.73. Starting at source s, its final distribution is |⟨d|U³|s⟩|².

The matched baseline applies the same step then complete computational-basis
dephasing after every step. Its stochastic kernel is |U|², composed three times.
A separate Qiskit density-matrix evolution test independently checks this channel.
Changing phase changes coherent distributions while leaving this baseline unchanged.
Total variation is half the L1 distance per source; exported evidence reports mean
and minimum over sources. This is ideal interference evidence, not measured
hardware benefit, algorithmic speedup or a musical preference result.

Local reference generation uses exact Operator/Statevector simulations. `shots=0`
means exact transition probabilities; `property_samples=count` records seeded
classical sampling of the quantum property distribution. Runtime is only a
classical xorshift64* draw from a saved probability row, reset at each audible leap.

## Single-job candidate

Prepare source labels uniformly, copy each label into destination via CNOT, evolve
only destination with the burst, and append an independent six-qubit property
circuit. Measure everything once. The joint distribution is
P(source,destination,property)=|⟨destination|U³|source⟩|²×P(property)/N.
Conditioning on each source gives the intended transition row. Measurement bits
are low n source, next n destination, highest six property bits.

For 64 rooms this requires 18 qubits. `manyspace.verify_job` actually submits
ONE job to the installed local BasicSimulator and validates source/destination
and property distributions, using 65,536 shots (1,024 expected per source).
This experiment does not yet build a production preset from the observed counts:
that adapter must choose/document a property aggregation mapping and refuse
missing-source rows. The current references use the separate exact local path.
No unobserved row should silently become an invented uniform row.

Generic RX/RY/RZ/CX transpilation is recorded in the presets. Actual IonQ gate
compilation, provider result decoding, allowed shots and noise behavior still
need verification. Source-label overhead buys one-job consolidation, not quantum
speedup; high shot counts can be costly in time on a noisy remote simulator.

Qiskit state and measurement APIs were checked against the
[official Statevector documentation](https://quantum.cloud.ibm.com/docs/en/api/qiskit/qiskit.quantum_info.Statevector).
