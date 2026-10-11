#!/usr/bin/env python3
"""Loopback notebook preview with an actual local Qiskit generation backend."""
import argparse
import asyncio
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
import math
import re
from pathlib import Path
import sys
import threading

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))


def generate_model(config):
    if not isinstance(config, dict) or set(config) != {'count', 'seed', 'depth', 'phase', 'shots'}:
        raise ValueError('Expected count, seed, depth, phase and shots')
    for key, lo, hi in [('count', 16, 64), ('seed', 0, 0xffffffff), ('depth', 2, 8), ('shots', 1024, 65536)]:
        value = config[key]
        if type(value) is not int or not lo <= value <= hi:
            raise ValueError('Invalid ' + key)
    phase = config['phase']
    if type(phase) not in (int, float) or not math.isfinite(phase) or not -math.pi <= phase <= math.pi:
        raise ValueError('Invalid phase')
    if config['count'] not in (16, 64) or config['shots'] % 1024:
        raise ValueError('Use 16/64 rooms and shots in multiples of 1024')
    from qiskit.providers.basic_provider import BasicSimulator
    from manyspace.workflow import generate_on_backend
    backend = BasicSimulator(seed_simulator=config['seed'])
    return asyncio.run(generate_on_backend(backend, **config))


class NotebookHandler(SimpleHTTPRequestHandler):
    generation_lock = threading.Lock()

    def json_response(self, status, data):
        body = json.dumps(data, allow_nan=False, separators=(',', ':')).encode()
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'no-store')
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == '/api/backend':
            return self.json_response(200, {'name': 'local Qiskit BasicSimulator'})
        return super().do_GET()

    def do_POST(self):
        if self.path != '/api/generate':
            return self.json_response(404, {'error': 'Unknown endpoint'})
        # Browser requests must originate on this loopback server. No CORS support.
        expected = 'http://' + self.headers.get('Host', '')
        if not re.fullmatch(r'(127\.0\.0\.1|localhost):[0-9]{1,5}', self.headers.get('Host', '')) or self.headers.get('Origin') != expected:
            return self.json_response(403, {'error': 'Generate must run from this notebook'})
        try:
            length = int(self.headers.get('Content-Length', '0'))
            if not 0 < length <= 1024:
                raise ValueError('Invalid request length')
            if self.headers.get('Content-Type') != 'application/json':
                raise ValueError('Expected JSON')
            config = json.loads(self.rfile.read(length))
        except (ValueError, UnicodeError) as error:
            return self.json_response(400, {'error': str(error)})
        if not self.generation_lock.acquire(blocking=False):
            return self.json_response(409, {'error': 'A local generation is already running'})
        try:
            result = generate_model(config)
            self.json_response(200, result)
        except ValueError as error:
            self.json_response(400, {'error': str(error)})
        except Exception as error:
            self.json_response(500, {'error': 'Local simulation failed: ' + str(error)})
        finally:
            self.generation_lock.release()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, default=8765)
    args = parser.parse_args()
    directory = ROOT / 'build/local'
    if not (directory / 'index.html').is_file():
        parser.error('Run make local-harness first')
    server = ThreadingHTTPServer(('127.0.0.1', args.port), partial(NotebookHandler, directory=str(directory)))
    print(f'Multi-Verb: http://127.0.0.1:{args.port}/ · local Qiskit simulator', flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == '__main__':
    main()
