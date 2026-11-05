#!/usr/bin/env python3
"""Real HTTP/SQLite tests; no compiler or browser needed."""
from functools import partial
from contextlib import closing
from concurrent.futures import ThreadPoolExecutor
import importlib.util
import json
from pathlib import Path
import sqlite3
import socket
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
        self.server = app.Server(('127.0.0.1', 0), handler)
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

    def keyed(self, key='a' * 32):
        return dict(title='Title', description='Description', severity='high', request_key=key)

    def key_count(self):
        with closing(sqlite3.connect(self.database)) as connection:
            return connection.execute('SELECT count(*) FROM issue_requests').fetchone()[0]

    def test_key_replay_normalization_conflict_and_current_snapshot(self):
        payload = self.keyed()
        status, first = self.request(payload)
        self.assertEqual(status, 201)
        self.assertFalse(first['replayed'])
        status, replay = self.request({**payload, 'title': ' Title ', 'description': '\nDescription  '})
        self.assertEqual(status, 200)
        self.assertTrue(replay['replayed'])
        self.assertEqual(replay['created'], first['created'])
        for field, value in [('title', 'Other'), ('description', 'Other'), ('severity', 'low')]:
            self.assertEqual(self.request({**payload, field: value})[0], 409)
        self.assertEqual(self.count(), 1)
        self.assertEqual(self.request()[1]['issues'], [first['created']])
        status, second = self.request({**payload, 'request_key': 'b' * 32})
        self.assertEqual(status, 201)
        self.assertNotEqual(second['created']['id'], first['created']['id'])
        app.initialize(self.database)  # Existing database/schema reopens without losing keys.
        status, replay = self.request(payload)
        self.assertEqual(status, 200)
        self.assertEqual(replay['created'], first['created'])
        self.assertEqual(replay['revision'], 2)
        self.assertEqual([issue['id'] for issue in replay['issues']], [2, 1])
        self.assertEqual((self.count(), self.key_count()), (2, 2))

    def test_concurrent_same_key_and_conflicting_payloads(self):
        barrier = threading.Barrier(8)
        def create(_):
            barrier.wait()
            return self.request(self.keyed())
        with ThreadPoolExecutor(max_workers=8) as executor:
            results = list(executor.map(create, range(8)))
        self.assertEqual(sorted(status for status, _ in results), [200] * 7 + [201])
        self.assertEqual({body['created']['id'] for _, body in results}, {1})
        self.assertEqual((self.count(), self.key_count()), (1, 1))
        barrier = threading.Barrier(2)
        def conflict(title):
            barrier.wait()
            return self.request({**self.keyed('c' * 32), 'title': title})
        with ThreadPoolExecutor(max_workers=2) as executor:
            results = list(executor.map(conflict, ['Left', 'Right']))
        self.assertEqual(sorted(status for status, _ in results), [201, 409])
        self.assertEqual((self.count(), self.key_count()), (2, 2))

    def test_committed_response_body_lost_then_replayed(self):
        payload = self.keyed()
        body = json.dumps(payload).encode()
        with socket.create_connection(self.server.server_address, timeout=5) as client:
            client.sendall(b'POST /api/issues HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Type: application/json\r\nContent-Length: ' +
                           str(len(body)).encode() + b'\r\nConnection: close\r\n\r\n' + body)
            headers = b''
            while not headers.endswith(b'\r\n\r\n'):
                chunk = client.recv(1)
                self.assertTrue(chunk)
                headers += chunk
            self.assertIn(b'201', headers.split(b'\r\n')[0])
            # Discard the socket without receiving/parsing the actual JSON confirmation.
        self.assertEqual((self.count(), self.key_count()), (1, 1))
        status, replay = self.request(payload)
        self.assertEqual(status, 200)
        self.assertEqual(replay['created']['id'], 1)
        self.assertEqual(self.count(), 1)

    def test_invalid_key_and_payload_do_not_consume_key(self):
        for key in ['', None, True, 42, 'A' * 32, 'a' * 31, 'a' * 33, 'g' * 32, ' a' * 16]:
            self.assertEqual(self.request(self.keyed(key))[0], 400)
        self.assertEqual(self.request({**self.keyed(), 'title': '   '})[0], 400)
        self.assertEqual((self.count(), self.key_count()), (0, 0))
        self.assertEqual(self.request(self.keyed())[0], 201)

    def test_mapping_failure_rolls_back_the_issue_insert(self):
        with closing(sqlite3.connect(self.database)) as connection:
            connection.execute("CREATE TRIGGER reject_request BEFORE INSERT ON issue_requests BEGIN SELECT RAISE(ABORT, 'real SQLite mapping failure'); END")
            connection.commit()
        self.assertEqual(self.request(self.keyed())[0], 503)
        self.assertEqual((self.count(), self.key_count()), (0, 0))
        with closing(sqlite3.connect(self.database)) as connection:
            connection.execute('DROP TRIGGER reject_request')
            connection.commit()
        self.assertEqual(self.request(self.keyed())[0], 201)
        self.assertEqual((self.count(), self.key_count()), (1, 1))

    def test_existing_database_migration_preserves_legacy_rows(self):
        with closing(sqlite3.connect(self.database)) as connection:
            connection.execute('DROP TABLE issue_requests')
            connection.execute("INSERT INTO issues (title, description, severity) VALUES ('Legacy', 'Existing data', 'low')")
            connection.commit()
        app.initialize(self.database)
        app.initialize(self.database)
        self.assertEqual(self.request()[1]['issues'][0]['title'], 'Legacy')
        status, created = self.request(self.keyed())
        self.assertEqual(status, 201)
        self.assertEqual(created['created']['id'], 2)
        self.assertEqual(self.request(self.keyed())[0], 200)
        self.assertEqual((self.count(), self.key_count()), (2, 1))

    def test_keyless_legacy_requests_remain_separate_creations(self):
        payload = dict(title='Title', description='Description', severity='low')
        self.assertEqual(self.request(payload)[0], 201)
        self.assertEqual(self.request(payload)[0], 201)
        self.assertEqual((self.count(), self.key_count()), (2, 0))

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
