#!/usr/bin/env python3
"""Real HTTP/WebSocket security and concurrency checks; standard library only."""
import base64
import concurrent.futures
import contextlib
import http.client
import json
import os
from pathlib import Path
import re
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
CLI = str(Path(sys.argv[1]).resolve())
CODE = 'jtml 2\nlet count = 0\nwhen increment\n  count += 1\npage\n  button "Increment" click increment\n  show count\n'


def request(port, path='/', method='GET', data=None, headers=None):
    connection = http.client.HTTPConnection('127.0.0.1', port, timeout=10)
    try:
        connection.request(method, path, body=json.dumps(data) if data is not None else None,
                           headers=headers or {})
        response = connection.getresponse()
        return response.status, dict(response.getheaders()), response.read().decode()
    finally:
        connection.close()


def post(port, path, data, headers=None):
    status, response_headers, body = request(port, path, 'POST', data,
        {'Content-Type': 'application/json', **(headers or {})})
    assert status == 200, (path, status, body)
    assert 'Access-Control-Allow-Origin' not in response_headers, response_headers
    result = json.loads(body)
    assert result.get('ok') is True, (path, result)
    return result


def available_port():
    for _ in range(100):
        with socket.socket() as first, socket.socket() as second:
            first.bind(('127.0.0.1', 0))
            port = first.getsockname()[1]
            if port > 65455:
                continue
            try:
                second.bind(('127.0.0.1', port + 80))
                return port
            except OSError:
                pass
    raise AssertionError('could not reserve an HTTP/WebSocket port pair')


def external_address():
    # UDP connect chooses a local interface without sending a packet.
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.connect(('192.0.2.1', 9))
        address = probe.getsockname()[0]
    assert not address.startswith('127.'), 'external-interface coverage unavailable'
    return address


def handshake(port, headers=()):
    client = socket.create_connection(('127.0.0.1', port), timeout=5)
    key = base64.b64encode(os.urandom(16)).decode()
    supplied = {name.lower() for name, _ in headers}
    fields = ([] if 'host' in supplied else [('Host', f'127.0.0.1:{port}')]) + [
        ('Upgrade', 'websocket'), ('Connection', 'Upgrade'),
        ('Sec-WebSocket-Key', key), ('Sec-WebSocket-Version', '13'), *headers]
    client.sendall(('GET / HTTP/1.1\r\n' + ''.join(f'{k}: {v}\r\n' for k, v in fields) + '\r\n').encode())
    # Read only the HTTP headers: preserve any immediately queued WS frame.
    response = bytearray()
    while not response.endswith(b'\r\n\r\n'):
        chunk = client.recv(1)
        assert chunk, response
        response.extend(chunk)
        assert len(response) < 16384
    return client, int(response.split(b' ', 2)[1])


def receive_json(client):
    def read(count):
        data = bytearray()
        while len(data) < count:
            chunk = client.recv(count - len(data))
            assert chunk, 'WebSocket closed before frame completion'
            data.extend(chunk)
        return bytes(data)
    first, second = read(2)
    assert first & 0x0f == 1, ('expected text frame', first)
    length = second & 127
    if length == 126:
        length = struct.unpack('!H', read(2))[0]
    elif length == 127:
        length = struct.unpack('!Q', read(8))[0]
    return json.loads(read(length))


def send_event(client, event):
    data = json.dumps({'type': 'event', **event}).encode()
    mask = os.urandom(4)
    length = len(data)
    header = bytes([0x81, 0x80 | length]) if length < 126 else bytes([0x81, 0xfe]) + struct.pack('!H', length)
    client.sendall(header + mask + bytes(value ^ mask[index % 4] for index, value in enumerate(data)))


def snapshot(port):
    client, status = handshake(port + 80)
    with client:
        assert status == 101, status
        message = receive_json(client)
        assert message['type'] == 'populateBindings', message
        return message['bindings']['state']['count']


def event_from_html(html):
    match = re.search(r"sendEvent\('([^']+)', 'onClick', \['([^']+)'\]\)", html)
    assert match, 'counter event binding missing'
    return {'elementId': match[1], 'eventType': 'onClick', 'args': [match[2]]}


@contextlib.contextmanager
def running(mode, source, directory, port=None, stop_signal=signal.SIGTERM):
    port = port or available_port()
    command = [CLI, 'studio' if mode == 'studio' else 'serve']
    if mode != 'studio':
        command += [str(source)]
    if mode == 'watch':
        command += ['--watch']
    command += ['--port', str(port)]
    with (directory / f'{mode}.log').open('w') as log:
        process = subprocess.Popen(command, cwd=ROOT, stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + 20
            while True:
                assert process.poll() is None, f'{mode} exited during startup; see {log.name}'
                try:
                    if request(port)[0] == 200:
                        with socket.create_connection(('127.0.0.1', port + 80), timeout=1):
                            pass
                        break
                except OSError:
                    pass
                assert time.monotonic() < deadline, f'{mode} did not become ready'
                time.sleep(.05)
            yield port
        except BaseException:
            log.flush()
            print(Path(log.name).read_text()[-16000:], file=sys.stderr)
            raise
        finally:
            if process.poll() is None:
                process.send_signal(stop_signal)
            try:
                result = process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
                raise AssertionError(f'{mode} did not terminate on signal {stop_signal}')
            assert result in (0, -stop_signal, 128 + stop_signal), (mode, result, log.name)
            for listener in (port, port + 80):
                with socket.socket() as client:
                    client.settimeout(.5)
                    assert client.connect_ex(('127.0.0.1', listener)) != 0, (mode, 'listener survived shutdown', listener)


def check_security(port, mode, event):
    origin = f'http://127.0.0.1:{port}'
    for host in (f'127.0.0.1:{port}', f'localhost:{port}'):
        status, headers, _ = request(port, headers={'Host': host, 'Origin': 'http://' + host})
        assert status == 200, (host, status)
        assert headers.get('Access-Control-Allow-Origin') != '*', headers
    paths = ['/api/event', '/api/login'] + (['/api/run', '/api/export', '/api/format', '/api/lint', '/api/fix']
        if mode == 'studio' else ['/api/component-action'])
    bad_headers = [
        {'Origin': 'https://attacker.example'}, {'Origin': 'null'}, {'Origin': ''},
        {'Origin': f'http://127.0.0.1:{port + 1}'},
        {'Host': f'evil.example:{port}'}, {'Host': f'localhost.evil:{port}', 'Origin': origin},
        {'Host': f'127.0.0.1:{port + 1}'}, {'Sec-Fetch-Site': 'cross-site'},
        {'Sec-Fetch-Site': 'same-site'}, {'Origin': origin + '/'},
    ]
    before = snapshot(port)
    for path in paths:
        for headers in bad_headers:
            status, response_headers, _ = request(port, path, 'POST', {'code': CODE, **event},
                {'Content-Type': 'text/plain', **headers})
            assert status == 403, (mode, path, headers, status)
            assert response_headers.get('Access-Control-Allow-Origin') != '*'
    assert snapshot(port) == before, 'rejected requests changed interpreter state'
    for path in ('/', '/api/bindings'):
        assert request(port, path, headers={'Host': f'evil.example:{port}'})[0] == 403
        assert request(port, path, headers={'Origin': 'https://attacker.example'})[0] == 403
    assert request(port, '/api/run', 'OPTIONS', headers={'Origin': 'https://attacker.example'})[0] == 403
    assert request(port, '/api/run', 'OPTIONS', headers={'Origin': origin})[0] == 204
    # Strict duplicate-authority handling, including case-insensitive duplicates.
    for name, value in [('Host', f'evil.example:{port}'), ('Origin', 'https://attacker.example')]:
        client = socket.create_connection(('127.0.0.1', port), timeout=3)
        with client:
            duplicate = f'Host: 127.0.0.1:{port}\r\n' if name == 'Host' else f'Host: 127.0.0.1:{port}\r\nOrigin: {origin}\r\n'
            client.sendall((f'GET / HTTP/1.1\r\n{duplicate}{name.lower()}: {value}\r\nConnection: close\r\n\r\n').encode())
            status = int(client.recv(1024).split(b' ', 2)[1])
            assert status in (400, 403), (name, status)
    ws_port = port + 80
    for headers in [(), (('Origin', origin), ('Sec-Fetch-Site', 'same-site')),
                    (('Host', f'localhost:{ws_port}'), ('Origin', origin), ('Sec-Fetch-Site', 'cross-site'))]:
        client, status = handshake(ws_port, headers)
        with client:
            assert status == 101, (headers, status)
            assert receive_json(client)['type'] == 'populateBindings'
    for headers in [
        (('Origin', 'null'),), (('Origin', ''),), (('Origin', 'https://attacker.example'),),
        (('Origin', f'http://localhost:{port + 1}'),),
        (('Host', f'evil.example:{ws_port}'),),
        (('Sec-Fetch-Site', 'cross-site'),),
        (('Origin', origin), ('Origin', 'https://attacker.example')),
        (('Host', f'localhost:{ws_port}'), ('host', f'evil.example:{ws_port}')),
    ]:
        client, status = handshake(ws_port, headers)
        with client:
            assert status != 101 and status in (400, 403), (headers, status)
    address = external_address()
    for listener in (port, ws_port):
        with socket.socket() as client:
            client.settimeout(.5)
            assert client.connect_ex((address, listener)) != 0, ('externally reachable listener', address, listener)


def check_concurrency(port, event, mode):
    before = snapshot(port)
    client, status = handshake(port + 80)
    with client:
        assert status == 101
        receive_json(client)
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
            requests = [pool.submit(post, port, '/api/event', event) for _ in range(20)]
            for _ in range(20):
                send_event(client, event)
            for future in requests:
                future.result(timeout=20)
        expected = before + 40
        deadline = time.monotonic() + 10
        while snapshot(port) != expected:
            assert time.monotonic() < deadline, (mode, 'lost concurrent HTTP/WS updates', snapshot(port), expected)
            time.sleep(.05)
    if mode == 'studio':
        with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
            results = [pool.submit(post, port, path, {'code': CODE}) for path in
                       ['/api/run', '/api/export', '/api/format', '/api/fix', '/api/lint'] * 3]
            for future in results:
                future.result(timeout=20)
        assert snapshot(port) == 0, 'Studio reload did not leave a coherent fresh interpreter'


def main():
    with tempfile.TemporaryDirectory(prefix='jtml-local-security-') as temporary:
        directory = Path(temporary)
        source = directory / 'counter.jtml'
        source.write_text(CODE)
        for mode in ('serve', 'studio', 'watch'):
            with running(mode, source, directory, stop_signal=signal.SIGINT if mode == 'studio' else signal.SIGTERM) as port:
                if mode == 'studio':
                    html = post(port, '/api/run', {'code': CODE})['html']
                else:
                    html = request(port)[2]
                event = event_from_html(html)
                check_security(port, mode, event)
                check_concurrency(port, event, mode)
                if mode == 'watch':
                    source.write_text(CODE.replace('Increment', 'Reloaded'))
                    deadline = time.monotonic() + 10
                    while '"Reloaded"' not in request(port)[2]:
                        assert time.monotonic() < deadline, 'watch did not reload'
                        time.sleep(.1)
                    assert snapshot(port) == 0, 'watch reload did not reset the interpreter coherently'
                else:
                    before_browser = snapshot(port)
                    subprocess.run(['node', str(ROOT / 'tests/browser/local_server_security.cjs'), str(port), mode, json.dumps(event)],
                                   check=True, cwd=ROOT, timeout=45)
                    assert snapshot(port) == (2 if mode == 'studio' else before_browser + 4), 'foreign browser POST changed interpreter state'
            print(f'PASS: {mode}: loopback HTTP/WS, Host/Origin denials, 40 concurrent mutations, shutdown releases both ports', flush=True)
        with running('serve', source, directory, port):
            assert request(port)[0] == 200 and snapshot(port) == 0
        print('PASS: immediate restart reuses both released listener ports', flush=True)
        for port in ('0', '65500'):
            result = subprocess.run([CLI, 'serve', str(source), '--port', port], cwd=ROOT,
                                    capture_output=True, text=True, timeout=5)
            assert result.returncode != 0 and '65455' in result.stderr, result.stderr
    print('PASS: local server security integration; no checks skipped')


if __name__ == '__main__':
    main()
