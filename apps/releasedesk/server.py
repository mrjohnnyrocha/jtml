#!/usr/bin/env python3
"""Loopback-only ReleaseDesk static files + SQLite JSON API. Python stdlib only."""
import argparse
from contextlib import contextmanager
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import sqlite3
from urllib.parse import urlsplit

SEVERITIES = {'low', 'medium', 'high', 'critical'}
MAX_BODY = 20000
ROOT = Path(__file__).resolve().parents[2]


@contextmanager
def connect(database):
    connection = sqlite3.connect(database, timeout=3)
    connection.row_factory = sqlite3.Row
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


def snapshot(connection):
    rows = [dict(row) for row in connection.execute('SELECT * FROM issues ORDER BY id DESC')]
    return {'issues': rows, 'revision': max((row['id'] for row in rows), default=0)}


def validate(payload):
    if not isinstance(payload, dict) or set(payload) != {'title', 'description', 'severity'}:
        raise ValueError('Provide title, description and severity')
    if not all(isinstance(payload[key], str) for key in payload):
        raise ValueError('Issue fields must be strings')
    title, description = payload['title'].strip(), payload['description'].strip()
    if not 1 <= len(title) <= 120 or not 1 <= len(description) <= 4000:
        raise ValueError('Title must be 1–120 characters and description 1–4000; whitespace is not content')
    if payload['severity'] not in SEVERITIES:
        raise ValueError('Choose low, medium, high or critical severity')
    return title, description, payload['severity']


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, database, **kwargs):
        self.database = database
        super().__init__(*args, **kwargs)

    def end_headers(self):
        self.send_header('Cache-Control', 'no-store')
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.send_header('Content-Security-Policy', "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; connect-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'")
        super().end_headers()

    def send_json(self, status, payload):
        encoded = json.dumps(payload, ensure_ascii=False).encode('utf-8')
        self.send_response(status)
        self.send_header('Content-Type', 'application/json; charset=utf-8')
        self.send_header('Content-Length', str(len(encoded)))
        self.end_headers()
        self.wfile.write(encoded)

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
            title, description, severity = validate(payload)
        except (ValueError, UnicodeError):
            self.send_json(400, {'error': 'Invalid issue: provide title (1–120), description (1–4000) and a supported severity'})
            return
        try:
            with connect(self.database) as connection:
                # Serialize mutation + returned snapshot, and commit before success is sent.
                connection.execute('BEGIN IMMEDIATE')
                cursor = connection.execute('INSERT INTO issues (title, description, severity) VALUES (?, ?, ?)',
                                            (title, description, severity))
                result = snapshot(connection)
                result['created'] = next(row for row in result['issues'] if row['id'] == cursor.lastrowid)
            self.send_json(201, result)
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
    with ThreadingHTTPServer(('127.0.0.1', args.port), handler) as server:
        print(f'ReleaseDesk: http://127.0.0.1:{server.server_port} · database: {database}', flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == '__main__':
    main()
