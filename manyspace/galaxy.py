"""Qiskit room properties and multi-step walk; classical saved-preset playback."""
import argparse
import json
import math
from pathlib import Path

import numpy as np
from qiskit import QuantumCircuit, qasm2, transpile
from qiskit.quantum_info import Operator, Statevector

RANGES = {"distance_ms": (15.0, 95.0), "speed": (0.5, 1.5), "damping": (0.2, 0.85)}
WEIGHTS = (0.5, 0.3, 0.2)  # normalized distance, speed, damping


def hilbert_xy(side, index):
    x = y = 0
    scale = 1
    while scale < side:
        rx, ry = (index // 2) & 1, (index ^ (index // 2)) & 1
        if not ry:
            if rx:
                x, y = scale - 1 - x, scale - 1 - y
            x, y = y, x
        x += scale * rx
        y += scale * ry
        index //= 4
        scale *= 2
    return [x, y]


def property_circuit(phase):
    qc = QuantumCircuit(6, name="room_properties")
    for q in range(6):
        qc.ry(0.7 + 0.19 * q, q)
        qc.rz(phase * (q + 1), q)
    for q in range(5):
        qc.cx(q, q + 1)
    for q in range(6):
        qc.rx(0.8, q)
    return qc


def walk_step(nqubits, phase):
    qc = QuantumCircuit(nqubits, name="hypercube_step")
    for q in range(nqubits):
        qc.rx(1.1, q)
        qc.rz(phase * (q + 1), q)
    for q in range(nqubits - 1):
        qc.cz(q, q + 1)
    return qc


def walk_circuit(nqubits, depth, phase):
    qc = QuantumCircuit(nqubits, name="coherent_burst")
    for _ in range(depth):
        qc.compose(walk_step(nqubits, phase), inplace=True)
    return qc


def single_job_circuit(count, depth=3, phase=0.73):
    """Candidate: source+destination+properties, only final measurement.

    source is low n bits, destination next n, property outcome highest 6.
    Conditional destination distribution = |<destination|U|source>|².
    Independent property samples are collected grouped by source label.
    """
    n = int(math.log2(count))
    if count not in (16, 64, 256):
        raise ValueError("count must be 16, 64 or 256")
    qc = QuantumCircuit(2 * n + 6)
    for q in range(n):
        qc.h(q)
        qc.cx(q, n + q)
    qc.compose(walk_circuit(n, depth, phase), range(n, 2 * n), inplace=True)
    qc.compose(property_circuit(phase), range(2 * n, 2 * n + 6), inplace=True)
    qc.measure_all()
    return qc


def distance(a, b):
    return math.sqrt(sum(w * ((a[k] - b[k]) / (hi - lo)) ** 2
                         for w, (k, (lo, hi)) in zip(WEIGHTS, RANGES.items())))


def adjacency(rooms):
    ds = [distance(a, b) for a, b in zip(rooms, rooms[1:])]
    return {"mean": float(np.mean(ds)), "max": max(ds), "differences": ds}


def generate(count=16, seed=7, depth=3, phase=0.73):
    if count not in (16, 64, 256) or not 2 <= depth <= 8 or not math.isfinite(phase):
        raise ValueError("use 16/64/256 rooms, depth 2..8 and finite phase")
    n = int(math.log2(count))
    rng = np.random.default_rng(seed)
    properties = property_circuit(phase)
    outcomes = rng.choice(64, size=count, p=Statevector(properties).probabilities())
    rooms = []
    for source, outcome in enumerate(outcomes):
        bins = [(int(outcome) >> (2 * q)) & 3 for q in range(3)]
        fields = {k: lo + (hi - lo) * b / 3
                  for (k, (lo, hi)), b in zip(RANGES.items(), bins)}
        rooms.append(dict(id=f"room-{source:02d}", source=source,
                          mode="swell" if int(outcome).bit_count() % 2 else "decay",
                          property_outcome=int(outcome), **fields))
    before = adjacency(rooms)
    # Properties are fixed first. Deterministic nearest-neighbor ordering, try every
    # start and minimize mean distance; this is a heuristic, not an optimum claim.
    distances = [[distance(a, b) for b in rooms] for a in rooms]
    candidates = []
    for start in range(count):
        order, remaining = [start], set(range(count)) - {start}
        while remaining:
            nxt = min(remaining, key=lambda j: (distances[order[-1]][j], j))
            order.append(nxt)
            remaining.remove(nxt)
        candidates.append(order)
    order = min(candidates, key=lambda o: adjacency([rooms[i] for i in o])["mean"])
    ordered = [rooms[i] for i in order]
    for i, room in enumerate(ordered):
        room.update(hilbert_index=i, xy=hilbert_xy(int(math.sqrt(count)), i))
    walk = walk_circuit(n, depth, phase)
    coherent = np.abs(Operator(walk).data.T) ** 2  # rows = source
    step = np.abs(Operator(walk_step(n, phase)).data.T) ** 2
    # Complete computational-basis dephasing after EACH step removes cross terms.
    dephased = np.linalg.matrix_power(step, depth)
    coherent = coherent[np.ix_(order, order)]
    dephased = dephased[np.ix_(order, order)]
    coherent /= coherent.sum(axis=1, keepdims=True)
    tv = np.abs(coherent - dephased).sum(axis=1) / 2
    candidate = single_job_circuit(count, depth, phase)
    compiled = transpile(candidate, basis_gates=["rx", "ry", "rz", "cx"], optimization_level=1)
    galaxy = dict(schema_version=1, rooms=ordered,
                  graph={"kind": "hypercube", "edges": [[rooms[a]["id"], rooms[a ^ (1 << q)]["id"]]
                         for a in range(count) for q in range(n) if a < (a ^ (1 << q))]},
                  transitions=coherent.tolist(), dephased=dephased.tolist(),
                  generation=dict(seed=seed, backend="qiskit.quantum_info exact local",
                                  shots=0, property_samples=count, depth=depth, phase=phase,
                                  qiskit_version=__import__("qiskit").__version__),
                  runtime=dict(playback_seed=11),
                  mapping=dict(ranges={k: list(v) for k, v in RANGES.items()}, distance_weights=list(WEIGHTS),
                               ordering="best-start greedy normalized weighted Euclidean",
                               mode="property outcome popcount parity"),
                  circuits=dict(properties=qasm2.dumps(properties), walk=qasm2.dumps(walk),
                                single_job_candidate=qasm2.dumps(compiled)),
                  evidence=dict(adjacency_before=before, adjacency_after=adjacency(ordered),
                                interference_tv_mean=float(tv.mean()), interference_tv_min=float(tv.min()),
                                single_job_qubits=compiled.num_qubits,
                                single_job_depth=compiled.depth(), single_job_gates=dict(compiled.count_ops()),
                                shots_for_expected_1024_per_source=count * 1024,
                                ionq_verified=False),
                  provenance=dict(fresh_code=True, prior_concepts="See docs/PROVENANCE.md",
                                  ai_assistance="OpenAI Codex", copied_prior_code=False))
    validate(galaxy)
    return galaxy


def _validate(g):
    def require(ok, message):
        if not ok:
            raise ValueError(message)
    require(g.get("schema_version") == 1, "unsupported schema")
    rooms = g.get("rooms", [])
    count = len(rooms)
    require(count in (16, 64, 256), "room count")
    require(len({r["id"] for r in rooms}) == count, "duplicate IDs")
    require(sorted(r["source"] for r in rooms) == list(range(count)), "source identity")
    for i, r in enumerate(rooms):
        require(type(r["source"]) is int and type(r["hilbert_index"]) is int, "integer identity/order")
        require(r["id"] == f'room-{r["source"]:02d}', "stable room identity")
        require(r["hilbert_index"] == i and r["xy"] == hilbert_xy(int(math.sqrt(count)), i), "Hilbert order")
        require(r["mode"] in ("decay", "swell"), "mode")
        outcome = r["property_outcome"]
        require(type(outcome) is int and 0 <= outcome < 64, "six-bit property outcome")
        require(r["mode"] == ("swell" if outcome.bit_count() % 2 else "decay"), "mode mapping")
        for k, (lo, hi) in RANGES.items():
            require(type(r[k]) in (int, float) and math.isfinite(r[k]) and lo <= r[k] <= hi, k)
        for q, (k, (lo, hi)) in enumerate(RANGES.items()):
            expected = lo + (hi - lo) * ((outcome >> (2 * q)) & 3) / 3
            require(abs(r[k] - expected) < 1e-9, "property mapping")
    for key in ("transitions", "dephased"):
        p = np.asarray(g[key], dtype=float)
        require(p.shape == (count, count) and np.isfinite(p).all() and (p >= 0).all() and (p <= 1).all(), key)
        require(np.allclose(p.sum(axis=1), 1, atol=1e-10, rtol=0), "probability normalization")
    ids = {r["id"] for r in rooms}
    expected = {tuple(sorted((f"room-{a:02d}", f"room-{a ^ (1 << q):02d}")))
                for a in range(count) for q in range(int(math.log2(count))) if a < a ^ (1 << q)}
    edges = g["graph"]["edges"]
    require(g["graph"]["kind"] == "hypercube" and len(edges) == len(expected)
            and {tuple(sorted(e)) for e in edges} == expected and all(set(e) <= ids for e in edges), "graph")
    gen = g["generation"]
    require(type(gen["seed"]) is int and gen["seed"] >= 0 and type(gen["shots"]) is int and gen["shots"] >= 0
            and bool(gen["backend"]) and 2 <= gen["depth"] <= 8 and math.isfinite(gen["phase"]), "generation")
    require(type(g["runtime"]["playback_seed"]) is int and 0 <= g["runtime"]["playback_seed"] < 2**64, "runtime seed")
    require(g["mapping"]["ranges"] == {k: list(v) for k, v in RANGES.items()}
            or g["mapping"]["ranges"] == RANGES, "mapping ranges")
    require(g["mapping"]["distance_weights"] == list(WEIGHTS), "mapping weights")
    require(all(isinstance(g["circuits"][k], str) and g["circuits"][k].startswith("OPENQASM 2.0;")
                for k in ("properties", "walk", "single_job_candidate")), "circuit definitions")
    require(g["provenance"]["ai_assistance"] and g["provenance"]["prior_concepts"], "provenance")
    # Reject non-finite values anywhere, including ancillary generation evidence.
    json.dumps(g, allow_nan=False)
    return g


def validate(g):
    try:
        return _validate(g)
    except (KeyError, TypeError, IndexError, AttributeError, OverflowError) as exc:
        raise ValueError(f"malformed galaxy: {exc}") from exc


def load(path):
    return validate(json.loads(Path(path).read_text()))


def export_header(g, path):
    """Validated off-thread preset adapter; C++ audio knows no JSON/Python."""
    validate(g)
    lines = ["// Generated from a validated saved galaxy. Do not hand edit.", "#pragma once",
             '#include "core.hpp"', "namespace manyspace {", "inline Galaxy referenceGalaxy() {",
             "Galaxy g{};", f"g.count = {len(g['rooms'])};"]
    for i, r in enumerate(g["rooms"]):
        lines.append(f'g.rooms[{i}] = Room{{{r["distance_ms"] / 1000:.9f}f, {r["speed"]:.9f}f, '
                     f'{r["damping"]:.9f}f, {str(r["mode"] == "swell").lower()}}};')
        for j, p in enumerate(g["transitions"][i]):
            lines.append(f"g.probabilities[{i}][{j}] = {p:.12f}f;")
    lines += ["return g;", "}", f"inline std::uint64_t referencePlaybackSeed() {{ return {g['runtime']['playback_seed']}ULL; }}", "}"]
    Path(path).write_text("\n".join(lines) + "\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--count", type=int, default=16)
    parser.add_argument("--seed", type=int, default=7)
    parser.add_argument("--out", default="presets/reference16.json")
    parser.add_argument("--header")
    parser.add_argument("--load", help="validate/export a saved preset without regeneration")
    args = parser.parse_args()
    galaxy = load(args.load) if args.load else generate(args.count, args.seed)
    if not args.load:
        Path(args.out).parent.mkdir(parents=True, exist_ok=True)
        Path(args.out).write_text(json.dumps(galaxy, indent=2, allow_nan=False) + "\n")
    if args.header:
        export_header(galaxy, args.header)
    print(json.dumps(galaxy["evidence"], indent=2))


if __name__ == "__main__":
    main()
