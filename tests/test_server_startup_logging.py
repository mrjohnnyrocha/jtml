#!/usr/bin/env python3
"""Deterministic audit of real startup banners; no scheduling sleeps."""
import pathlib
import socket
import subprocess
import sys

binary = pathlib.Path(sys.argv[1]).resolve()
root = pathlib.Path(__file__).resolve().parents[1]
for mode in ('serve', 'watch', 'studio'):
    with socket.socket() as http:
        http.bind(('127.0.0.1', 0))
        port = http.getsockname()[1]
    assert port < 65455, 'ephemeral HTTP port cannot pair with WebSocket'
    result = subprocess.run([str(binary), mode, str(port), str(root/'examples/friendly_store.jtml')],
                            cwd=root, capture_output=True, text=True, timeout=15)
    assert result.returncode == 0 and 'PASS startup banner owns' in result.stderr, (mode, result.returncode, result.stderr)
    print('PASS', mode, 'banner execution-lock ownership through flush')
