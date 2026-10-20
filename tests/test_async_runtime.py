#!/usr/bin/env python3
"""Real HTTP/WS adapter coverage for Classic async functions (no rendering changes)."""
import tempfile
import subprocess
from pathlib import Path
import test_local_server_security as live

CODE = r'''define count = 0\\
async function increment()\\
    count = count + 1\\
\\
element button onClick=increment()\\
    show "Increment"\\
#
show count\\
'''

def main():
    probe = Path(live.CLI).parent / 'tests' / 'jtml_async_probe'
    for mode in ('locked-destruction', 'self-destruction'):
        result = subprocess.run([str(probe), mode], capture_output=True, text=True, timeout=5)
        assert result.returncode == 86, (mode, result.returncode, result.stdout, result.stderr)
        assert 'Interpreter destruction while holding an execution lock is unsupported' in result.stderr, result.stderr
        print(f'PASS boundary {mode}: unsupported operation rejects with diagnostic exit 86; no hang', flush=True)
    with tempfile.TemporaryDirectory(prefix='jtml-owned-async-') as temporary:
        directory = Path(temporary)
        source = directory / 'async.jtml'
        source.write_text(CODE)
        for mode in ('serve', 'studio', 'watch'):
            with live.running(mode, source, directory) as port:
                html = (live.post(port, '/api/run', {'code': CODE})['html']
                        if mode == 'studio' else live.request(port)[2])
                event = live.event_from_html(html)
                live.check_security(port, mode, event)
                live.check_concurrency(port, event, mode)
                if mode == 'studio':
                    # The inherited reload/tooling oracle uses its Friendly fixture.
                    assert live.snapshot(port) == 0
                else:
                    assert live.snapshot(port) == 40
            print(f'PASS async {mode}: HTTP/WS events, concurrent state and lifecycle; security negatives retained', flush=True)

if __name__ == '__main__':
    main()
