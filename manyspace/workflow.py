"""Quantum-only workflow artifact; shaping and playback are separate consumers."""
import argparse
import inspect
import json
from pathlib import Path

import numpy as np

from .galaxy import generate, single_job_circuit, validate


def quantum_from_legacy(galaxy):
    validate(galaxy)
    order = sorted(range(len(galaxy["rooms"])), key=lambda i: galaxy["rooms"][i]["source"])
    rooms = [galaxy["rooms"][i] for i in order]
    return {
        "format": "manyspace.quantum-galaxy", "version": 1,
        "rooms": [{"id": r["id"], "source": r["source"], "outcome": r["property_outcome"]} for r in rooms],
        "transitions": [[galaxy["transitions"][i][j] for j in order] for i in order],
        "dephased": [[galaxy["dephased"][i][j] for j in order] for i in order],
        "graph": galaxy["graph"], "generation": galaxy["generation"],
        "circuits": galaxy["circuits"], "provenance": galaxy["provenance"],
        "baseline_kind": "computed ideal dephased reference",
    }


def generate_quantum(count=64, seed=7, depth=3, phase=.73):
    return quantum_from_legacy(generate(count, seed, depth, phase))


def observed_quantum(reference, counts, backend_name, shots):
    """Decode the single-job source/destination/property layout; refuse uncovered rows."""
    q = json.loads(json.dumps(reference))
    count = len(q["rooms"])
    bits = int(np.log2(count))
    transition = np.zeros((count, count), dtype=float)
    properties = np.zeros(64, dtype=float)
    coverage = np.zeros(count, dtype=int)
    total = 0
    clean_counts = {}
    for key, amount in counts.items():
        key = str(key).replace(" ", "")
        if not key or any(c not in "01" for c in key) or len(key) > 2*bits+6:
            raise ValueError("Unexpected backend measurement layout")
        if type(amount) is not int or amount <= 0:
            raise ValueError("Invalid backend shot count")
        number = int(key, 2)
        source = number & (count-1)
        destination = (number >> bits) & (count-1)
        outcome = (number >> (2*bits)) & 63
        transition[source, destination] += amount
        properties[outcome] += amount
        coverage[source] += amount
        total += amount
        normalized_key = key.zfill(2*bits+6)
        clean_counts[normalized_key] = clean_counts.get(normalized_key, 0) + amount
    if total != shots:
        raise ValueError("Backend shot total differs from requested shots")
    if np.any(coverage == 0):
        raise ValueError("Some room origins have no samples. Start a new run with more shots.")
    transition /= coverage[:, None]
    outcomes = np.random.default_rng(q["generation"]["seed"]).choice(64, size=count, p=properties/total)
    for room, outcome in zip(q["rooms"], outcomes):
        room["outcome"] = int(outcome)
    q["transitions"] = transition.tolist()
    q["generation"].update(backend=backend_name, shots=shots, property_samples=count,
                           source_coverage=coverage.tolist(),
                           property_selection="Seeded sampling from the measured property histogram")
    q["measurements"] = clean_counts
    return q


async def generate_on_backend(backend, count=64, seed=7, depth=3, phase=.73, shots=65536):
    """One backend submission, including property measurements. Hosted path needs verification."""
    if not 1024 <= shots <= 65536:
        raise ValueError("Use 1,024–65,536 shots")
    reference = generate_quantum(count, seed, depth, phase)
    circuit = single_job_circuit(count, depth, phase)
    from qiskit import transpile
    circuit = transpile(circuit, basis_gates=["rx", "ry", "rz", "cx"], optimization_level=1)
    job = backend.run(circuit, shots=shots)
    if inspect.isawaitable(job):
        job = await job
    result = job.result()
    if inspect.isawaitable(result):
        result = await result
    name = getattr(backend, "name", type(backend).__name__)
    if callable(name):
        name = name()
    return observed_quantum(reference, dict(result.get_counts()), str(name), shots)


def bundled_reference():
    # Literal path is required by Qollab's project data-file loader.
    return json.loads(Path("presets/reference64.json").read_text())


def bundled_core():
    """Text of the single-file Emscripten C++ core; JavaScript evaluates it in-page."""
    # Literal path is required by Qollab's project data-file loader.
    return Path("assets/manyspace-core.js").read_text()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--count", type=int, default=64)
    parser.add_argument("--seed", type=int, default=7)
    parser.add_argument("--depth", type=int, default=3)
    parser.add_argument("--phase", type=float, default=.73)
    args = parser.parse_args()
    print(json.dumps(generate_quantum(args.count, args.seed, args.depth, args.phase),
                     allow_nan=False, separators=(",", ":")))


if __name__ == "__main__":
    main()
