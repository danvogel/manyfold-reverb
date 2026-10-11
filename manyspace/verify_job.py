"""One actual FREE LOCAL backend.run; no IonQ/network/account interaction."""
import json
import time
from pathlib import Path
import numpy as np
from qiskit import transpile
from qiskit.providers.basic_provider import BasicSimulator
from qiskit.quantum_info import Operator, Statevector
from .galaxy import single_job_circuit, property_circuit, walk_circuit


def verify(count=64, shots=65536):
    n = count.bit_length() - 1
    qc = single_job_circuit(count)
    backend = BasicSimulator()
    compiled = transpile(qc, backend, optimization_level=1)
    start = time.perf_counter()
    counts = backend.run(compiled, shots=shots, seed_simulator=23).result().get_counts()
    rows = np.zeros((count, count), dtype=int)
    properties = np.zeros((count, 64), dtype=int)
    for bits, frequency in counts.items():
        value = int(bits.replace(" ", ""), 2)
        source = value & (count - 1)
        destination = (value >> n) & (count - 1)
        prop = value >> (2 * n)
        rows[source, destination] += frequency
        properties[source, prop] += frequency
    total = rows.sum(axis=1)
    if np.any(total == 0):
        raise ValueError("unobserved source: cannot construct its row; increase shots")
    p = rows / total[:, None]
    expected = np.abs(Operator(walk_circuit(n, 3, .73)).data.T) ** 2
    tv = np.abs(p - expected).sum(axis=1) / 2
    property_expected = Statevector(property_circuit(.73)).probabilities()
    property_tv = np.abs(properties / total[:, None] - property_expected).sum(axis=1) / 2
    report = dict(backend="BasicSimulator local", remote_run=False, job_calls=1, count=count,
                  shots=shots, seed_simulator=23, qubits=compiled.num_qubits,
                  depth=compiled.depth(), gates=dict(compiled.count_ops()),
                  seconds=time.perf_counter() - start,
                  source_shots_min=int(total.min()), source_shots_max=int(total.max()),
                  transition_tv_mean=float(tv.mean()), transition_tv_max=float(tv.max()),
                  property_tv_mean=float(property_tv.mean()), property_tv_max=float(property_tv.max()),
                  missing_sources=0, ionq_verified=False,
                  caveat="Sampling proof, not yet the saved-preset production path or IonQ compatibility proof")
    assert report["transition_tv_max"] < .2 and report["property_tv_max"] < .2
    return report


if __name__ == "__main__":
    report = verify()
    Path("build").mkdir(exist_ok=True)
    Path("build/single-job-evidence.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
