#!/usr/bin/env python3
"""Real HTTP/SQLite tests; no compiler or browser needed."""
from functools import partial
from http.server import ThreadingHTTPServer
from contextlib import closing
import importlib.util
import json
from pathlib import Path
import sqlite3
import sys
import tempfile
import threading
import unittest
from urllib.error import HTTPError
from urllib.request import Request, urlopen

sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location('releasedesk', Path(__file__).with_name('server.py'))
app = importlib.util.module_from_spec(spec)
spec.loader.exec_module(app)


class ReleaseDeskAPI(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.database = self.root / 'issues.sqlite3'
        app.initialize(self.database)
        handler = partial(app.Handler, database=self.database, directory=str(self.root))
        self.server = ThreadingHTTPServer(('127.0.0.1', 0), handler)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.addCleanup(self.close)
        self.url = f'http://127.0.0.1:{self.server.server_port}/api/issues'

    def close(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()

    def request(self, payload=None, *, raw=None, headers=None):
        data = raw if raw is not None else (json.dumps(payload).encode() if payload is not None else None)
        request = Request(self.url, data=data, headers=headers or {'Content-Type': 'application/json'})
        try:
            response = urlopen(request, timeout=5)
        except HTTPError as error:
            response = error
        with response:
            return response.status, json.load(response)

    def count(self):
        with closing(sqlite3.connect(self.database)) as connection:
            return connection.execute('SELECT count(*) FROM issues').fetchone()[0]

    def test_empty_create_and_persisted_snapshot(self):
        self.assertEqual(self.request(), (200, {'issues': [], 'revision': 0}))
        status, body = self.request(dict(title=' Fix release ', description=' Repro steps\nDetails ', severity='high'))
        self.assertEqual(status, 201)
        self.assertEqual(body['created']['title'], 'Fix release')
        self.assertEqual(body['created']['description'], 'Repro steps\nDetails')
        self.assertEqual(body['issues'], [body['created']])
        self.assertEqual(body['revision'], 1)
        self.assertEqual(self.count(), 1)
        self.assertEqual(self.request()[1]['issues'], body['issues'])

    def test_invalid_fields_never_create_rows(self):
        valid = dict(title='Title', description='Description', severity='medium')
        for payload in ([], {}, {**valid, 'extra': True}, {**valid, 'title': '   '},
                        {**valid, 'description': ''}, {**valid, 'severity': 'urgent'},
                        {**valid, 'title': 1}, {**valid, 'description': 'x' * 4001},
                        {**valid, 'title': 'x' * 121}, {**valid, 'severity': None}):
            with self.subTest(payload=repr(payload)[:100]):
                self.assertEqual(self.request(payload)[0], 400)
                self.assertEqual(self.count(), 0)

    def test_malformed_json_and_non_json_rejected(self):
        self.assertEqual(self.request(raw=b'{')[0], 400)
        self.assertEqual(self.request(raw=b'\xff')[0], 400)
        self.assertEqual(self.request(raw=b'x', headers={'Content-Type': 'text/plain'})[0], 415)
        self.assertEqual(self.request(raw=b'x' * 20001)[0], 413)
        self.assertEqual(self.count(), 0)

    def test_cross_origin_write_rejected(self):
        status, _ = self.request(dict(title='Title', description='Description', severity='low'),
                                 headers={'Content-Type': 'application/json', 'Origin': 'https://example.com'})
        self.assertEqual(status, 403)
        self.assertEqual(self.count(), 0)

    def test_actual_sqlite_failure_is_not_success(self):
        backup = self.root / 'backup.sqlite3'
        self.database.rename(backup)
        self.database.mkdir()
        try:
            self.assertEqual(self.request()[0], 503)
            self.assertEqual(self.request(dict(title='Title', description='Description', severity='low'))[0], 503)
        finally:
            self.database.rmdir()
            backup.rename(self.database)
        self.assertEqual(self.count(), 0)
        self.assertEqual(self.request()[0], 200)


if __name__ == '__main__':
    unittest.main()
