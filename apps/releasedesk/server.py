#!/usr/bin/env python3
"""Loopback-only ReleaseDesk static files + SQLite JSON API. Python stdlib only."""
import argparse
from contextlib import contextmanager
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import re
import sqlite3
from urllib.parse import urlsplit

SEVERITIES = {'low', 'medium', 'high', 'critical'}
MAX_BODY = 20000
ROOT = Path(__file__).resolve().parents[2]


class Server(ThreadingHTTPServer):
    request_queue_size = 32


@contextmanager
def connect(database):
    connection = sqlite3.connect(database, timeout=3)
    connection.row_factory = sqlite3.Row
    connection.execute('PRAGMA foreign_keys = ON')
    try:
        with connection:
            yield connection
    finally:
        connection.close()


def initialize(database):
    database.parent.mkdir(parents=True, exist_ok=True)
    with connect(database) as connection:
        connection.execute('''CREATE TABLE IF NOT EXISTS issues (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            title TEXT NOT NULL CHECK(length(trim(title)) BETWEEN 1 AND 120),
            description TEXT NOT NULL CHECK(length(trim(description)) BETWEEN 1 AND 4000),
            severity TEXT NOT NULL CHECK(severity IN ('low', 'medium', 'high', 'critical')),
            created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now'))
        )''')
        connection.execute('''CREATE TABLE IF NOT EXISTS issue_requests (
            request_key TEXT PRIMARY KEY NOT NULL CHECK(length(request_key) = 32),
            issue_id INTEGER NOT NULL UNIQUE REFERENCES issues(id)
        )''')


def snapshot(connection):
    rows = [dict(row) for row in connection.execute('SELECT * FROM issues ORDER BY id DESC')]
    return {'issues': rows, 'revision': max((row['id'] for row in rows), default=0)}


def validate(payload):
    required = {'title', 'description', 'severity'}
    if not isinstance(payload, dict) or not required <= set(payload) or set(payload) - required - {'request_key'}:
        raise ValueError('Provide title, description and severity')
    if not all(isinstance(payload[key], str) for key in payload):
        raise ValueError('Issue fields must be strings')
    title, description = payload['title'].strip(), payload['description'].strip()
    if not 1 <= len(title) <= 120 or not 1 <= len(description) <= 4000:
        raise ValueError('Title must be 1–120 characters and description 1–4000; whitespace is not content')
    if payload['severity'] not in SEVERITIES:
        raise ValueError('Choose low, medium, high or critical severity')
    key = payload.get('request_key')
    if 'request_key' in payload and (not isinstance(key, str) or not re.fullmatch('[0-9a-f]{32}', key)):
        raise ValueError('Request key must contain 32 lowercase hexadecimal characters')
    return title, description, payload['severity'], key


def create_issue(connection, title, description, severity, key):
    """Called inside BEGIN IMMEDIATE; the unique mapping and issue commit together."""
    existing = None
    if key is not None:
        existing = connection.execute('''SELECT issues.* FROM issue_requests
            JOIN issues ON issues.id = issue_requests.issue_id
            WHERE issue_requests.request_key = ?''', (key,)).fetchone()
    if existing is not None:
        if (existing['title'], existing['description'], existing['severity']) != (title, description, severity):
            return 409, {'error': 'This request key already created an issue with different fields'}
        created = dict(existing)
        status = 200
    else:
        cursor = connection.execute('INSERT INTO issues (title, description, severity) VALUES (?, ?, ?)',
                                    (title, description, severity))
        issue_id = cursor.lastrowid
        if key is not None:
            connection.execute('INSERT INTO issue_requests (request_key, issue_id) VALUES (?, ?)', (key, issue_id))
        created = dict(connection.execute('SELECT * FROM issues WHERE id = ?', (issue_id,)).fetchone())
        status = 201
    result = snapshot(connection)
    result['created'] = created
    if key is not None:
        result.update(request_key=key, replayed=status == 200)
    return status, result


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, database, **kwargs):
        self.database = database
        super().__init__(*args, **kwargs)

    def end_headers(self):
        self.send_header('Cache-Control', 'no-store')
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.send_header('Content-Security-Policy', "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; connect-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'")
        super().end_headers()

    def send_json(self, status, payload):
        encoded = json.dumps(payload, ensure_ascii=False).encode('utf-8')
        self.send_response(status)
        self.send_header('Content-Type', 'application/json; charset=utf-8')
        self.send_header('Content-Length', str(len(encoded)))
        self.end_headers()
        try:
            self.wfile.write(encoded)
        except (BrokenPipeError, ConnectionResetError):
            # The transaction has already committed. A keyed retry recovers it.
            pass

    def do_GET(self):
        path = urlsplit(self.path).path
        if path == '/api/issues':
            try:
                with connect(self.database) as connection:
                    self.send_json(200, snapshot(connection))
            except sqlite3.Error:
                self.send_json(503, {'error': 'Database unavailable; check the server and SQLite file'})
        elif path.startswith('/api/'):
            self.send_json(404, {'error': 'Unknown API endpoint'})
        else:
            super().do_GET()

    def do_POST(self):
        if urlsplit(self.path).path != '/api/issues':
            self.send_json(404, {'error': 'Unknown API endpoint'})
            return
        # Local tool, not an authenticated service. Reject cross-origin browser writes.
        origin = self.headers.get('Origin')
        if origin and origin != f'http://127.0.0.1:{self.server.server_port}':
            self.send_json(403, {'error': 'Use the local ReleaseDesk origin'})
            return
        if self.headers.get_content_type() != 'application/json':
            self.send_json(415, {'error': 'Expected application/json'})
            return
        try:
            length = int(self.headers.get('Content-Length', '0'))
            if not 0 < length <= MAX_BODY:
                self.send_json(413, {'error': 'Request body must be 1–20000 bytes'})
                return
            payload = json.loads(self.rfile.read(length))
            title, description, severity, key = validate(payload)
        except (ValueError, UnicodeError):
            self.send_json(400, {'error': 'Invalid issue: provide title (1–120), description (1–4000), a supported severity and, when supplied, a 32-character lowercase hexadecimal request_key'})
            return
        try:
            with connect(self.database) as connection:
                # Serialize mutation + returned snapshot, and commit before success is sent.
                connection.execute('BEGIN IMMEDIATE')
                status, result = create_issue(connection, title, description, severity, key)
            self.send_json(status, result)
        except sqlite3.Error:
            self.send_json(503, {'error': 'Database unavailable; the save was not confirmed'})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dist', type=Path, default=ROOT / 'dist/releasedesk')
    parser.add_argument('--db', type=Path, default=ROOT / 'dist/releasedesk-data/issues.sqlite3')
    parser.add_argument('--port', type=int, default=8765)
    args = parser.parse_args()
    dist, database = args.dist.resolve(), args.db.resolve()
    for asset in ('index.html', 'jtml-runtime.js', 'components/index.js', 'app.js'):
        if not (dist / asset).is_file():
            parser.error(f'Missing {dist / asset}; build apps/releasedesk --target browser first')
    if database == dist or dist in database.parents:
        parser.error('Keep the SQLite database outside the served static directory')
    initialize(database)
    handler = partial(Handler, directory=str(dist), database=database)
    with Server(('127.0.0.1', args.port), handler) as server:
        print(f'ReleaseDesk: http://127.0.0.1:{server.server_port} · database: {database}', flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == '__main__':
    main()
