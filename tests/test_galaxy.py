import copy
import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
from qiskit import QuantumCircuit
from qiskit.quantum_info import Statevector, DensityMatrix
from manyspace.galaxy import generate, validate, load, single_job_circuit, walk_circuit, walk_step


class GalaxyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.g = generate(16)

    def test_saved_reproducibility(self):
        self.assertEqual(self.g, generate(16))
        self.assertNotEqual(self.g["rooms"], generate(16, seed=8)["rooms"])
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "preset.json"
            p.write_text(json.dumps(self.g))
            self.assertEqual(load(p), self.g)

    def test_hilbert_and_acoustic_order(self):
        r = self.g["rooms"]
        self.assertEqual(len({tuple(x["xy"]) for x in r}), 16)
        for a, b in zip(r, r[1:]):
            self.assertEqual(sum(abs(x - y) for x, y in zip(a["xy"], b["xy"])), 1)
        e = self.g["evidence"]
        self.assertLess(e["adjacency_after"]["mean"], e["adjacency_before"]["mean"])

    def test_interference_and_phase_dependence(self):
        self.assertGreater(self.g["evidence"]["interference_tv_min"], 0.1)
        other = generate(16, phase=0.31)
        # Compare in physical source order: property ordering can change with phase.
        def restore(g, key):
            ix = np.argsort([r["source"] for r in g["rooms"]])
            return np.array(g[key])[np.ix_(ix, ix)]
        self.assertTrue(np.allclose(restore(self.g, "dephased"), restore(other, "dephased")))
        self.assertFalse(np.allclose(restore(self.g, "transitions"), restore(other, "transitions")))

    def test_single_job_conditional_mathematics(self):
        qc = single_job_circuit(16).remove_final_measurements(inplace=False)
        joint = Statevector(qc).probabilities(range(8)).reshape(16, 16)  # dest, source
        self.assertTrue(np.allclose(joint.sum(axis=0), 1 / 16))
        for source in range(16):
            state = QuantumCircuit(4)
            for q in range(4):
                if source & (1 << q):
                    state.x(q)
            state.compose(walk_circuit(4, 3, 0.73), inplace=True)
            self.assertTrue(np.allclose(joint[:, source] * 16, Statevector(state).probabilities()))

    def test_dephased_baseline_against_density_matrix(self):
        # Independent channel calculation: evolve, then discard all off-diagonals.
        source = 5
        rho = DensityMatrix.from_label(format(source, "04b"))
        for _ in range(3):
            rho = rho.evolve(walk_step(4, .73))
            rho = DensityMatrix(np.diag(np.diag(rho.data)))
        ix = np.argsort([r["source"] for r in self.g["rooms"]])
        p = np.asarray(self.g["dephased"])[np.ix_(ix, ix)]
        self.assertTrue(np.allclose(rho.probabilities(), p[source]))

    def test_reject_corruption(self):
        mutations = [lambda g: g["transitions"][0].__setitem__(0, float("nan")),
                     lambda g: g["transitions"][0].__setitem__(0, -0.1),
                     lambda g: g["rooms"][0].__setitem__("speed", 8),
                     lambda g: g["rooms"][1].__setitem__("id", g["rooms"][0]["id"]),
                     lambda g: g["graph"]["edges"].pop(),
                     lambda g: g["runtime"].__setitem__("playback_seed", 1.5),
                     lambda g: g["runtime"].__setitem__("playback_seed", 2**64),
                     lambda g: g.pop("generation"),
                     lambda g: g.__setitem__("rooms", None),
                     lambda g: g["rooms"][0].__setitem__("speed", True),
                     lambda g: g["rooms"][0].__setitem__("property_outcome", 999),
                     lambda g: g["rooms"][0].__setitem__("distance_ms", 40),
                     lambda g: g["rooms"][0].__setitem__("mode", "decay" if g["rooms"][0]["mode"] == "swell" else "swell")]
        for mutate in mutations:
            g = copy.deepcopy(self.g)
            mutate(g)
            with self.assertRaises(ValueError):
                validate(g)

    def test_64_after_16(self):
        g = generate(64)
        validate(g)
        self.assertEqual(g["evidence"]["single_job_qubits"], 18)
        self.assertGreater(g["evidence"]["interference_tv_min"], 0.1)
        self.assertEqual(g["evidence"]["shots_for_expected_1024_per_source"], 65536)


if __name__ == "__main__":
    unittest.main()
