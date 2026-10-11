import io
import json
import unittest
from unittest.mock import patch
from tools.notebook_server import NotebookHandler, generate_model


class NotebookServerTests(unittest.TestCase):
    config = dict(count=16, seed=7, depth=3, phase=.73, shots=4096)

    def request(self, config=None, origin='http://127.0.0.1:8765', path='/api/generate'):
        handler = object.__new__(NotebookHandler)
        data = json.dumps(self.config if config is None else config).encode()
        handler.headers = {'Host': '127.0.0.1:8765', 'Origin': origin,
                           'Content-Type': 'application/json', 'Content-Length': str(len(data))}
        handler.path = path
        handler.rfile = io.BytesIO(data)
        handler.wfile = io.BytesIO()
        handler.send_response = lambda status: setattr(handler, 'status', status)
        handler.send_header = lambda *args: None
        handler.end_headers = lambda: None
        handler.do_POST()
        return handler.status, json.loads(handler.wfile.getvalue())

    def test_actual_measured_local_generation(self):
        for count in (16, 64):
            config = dict(self.config, count=count)
            status, model = self.request(config)
            self.assertEqual(status, 200)
            self.assertEqual(model['format'], 'manyspace.quantum-galaxy')
            self.assertEqual(len(model['rooms']), count)
            self.assertEqual(model['generation']['backend'], 'basic_simulator')
            self.assertEqual(sum(model['measurements'].values()), 4096)
            self.assertTrue(all(n > 0 for n in model['generation']['source_coverage']))
            self.assertTrue(all(abs(sum(row)-1) < 1e-9 for row in model['transitions']))
            self.assertEqual(model, generate_model(config), 'seed reproducibly samples the circuit')

    def test_invalid_configuration_does_not_start_simulation(self):
        for key, value in [('count', 256), ('seed', -1), ('seed', True), ('depth', 1),
                           ('phase', float('nan')), ('shots', 1025), ('shots', 0)]:
            with self.subTest(key=key, value=value):
                status, result = self.request(dict(self.config, **{key: value}))
                self.assertEqual(status, 400)
                self.assertIn('error', result)

    def test_cross_origin_and_concurrent_requests_rejected(self):
        with patch('tools.notebook_server.generate_model') as generate:
            self.assertEqual(self.request(origin='http://example.com')[0], 403)
            self.assertEqual(self.request(path='/other')[0], 404)
            NotebookHandler.generation_lock.acquire()
            try:
                self.assertEqual(self.request()[0], 409)
            finally:
                NotebookHandler.generation_lock.release()
            generate.assert_not_called()
