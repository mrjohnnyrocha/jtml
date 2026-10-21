#!/usr/bin/env python3
"""Bounded startup experiments; every outcome retained, no retry-to-pass policy."""
import json
import os
import pathlib
import socket
import subprocess
import sys
import tempfile
import time
import urllib.request

binary = pathlib.Path(sys.argv[1]).resolve()
fixture = pathlib.Path(sys.argv[2]).resolve()
count = int(sys.argv[3]) if len(sys.argv) > 3 else 12
mode = sys.argv[4] if len(sys.argv) > 4 else 'serve'
assert mode in ('serve', 'watch', 'studio')
out = pathlib.Path(tempfile.mkdtemp(prefix='jtml-native-store-startups-'))
results = []
for index in range(count):
    while True:
        with socket.socket() as sock:
            sock.bind(('127.0.0.1', 0))
            port = sock.getsockname()[1]
        if port < 65455:
            break
    log = out / ('startup-%02d.log' % index)
    env = dict(os.environ, ASAN_OPTIONS='halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1', TSAN_OPTIONS='halt_on_error=1')
    with log.open('w') as stream:
        command = [str(binary), 'studio', '--port', str(port)] if mode == 'studio' else [str(binary), 'serve', str(fixture), '--port', str(port)] + (['--watch'] if mode == 'watch' else [])
        process = subprocess.Popen(command,
                                   cwd=fixture.parents[1], stdout=stream, stderr=subprocess.STDOUT, env=env)
        deadline = time.monotonic() + 8
        ready = False
        # Health polling is observation, not the trigger of a claimed deterministic
        # reproduction. The fixed deadline bounds all startup experiments.
        while process.poll() is None and time.monotonic() < deadline:
            try:
                with urllib.request.urlopen(('http://127.0.0.1:%d' % port) + ('/' if mode == 'studio' else '/api/health'), timeout=.1) as response:
                    ready = response.status == 200
                if ready: break
            except (OSError, TimeoutError):
                pass
        startup_exit = process.poll()
        if process.poll() is None: process.terminate()
        try: process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=3)
        results.append(dict(mode=mode, attempt=index+1, ready=ready, startupExit=startup_exit,
                            finalExit=process.returncode, log=str(log)))
    print(json.dumps(results[-1]), flush=True)
(out/'results.json').write_text(json.dumps(dict(binary=str(binary), fixture=str(fixture), attempts=results), indent=2)+'\n')
print('Evidence:', out, flush=True)
