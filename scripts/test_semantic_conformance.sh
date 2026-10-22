#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# Known discrepancies are reproduced defects, never semantic PASS results.
python3 - "$root" "${1:---verify-catalog}" <<'PY'
import os, pathlib, signal, subprocess, sys
root = pathlib.Path(sys.argv[1])
process = subprocess.Popen(['node', str(root/'tests/conformance/runner.cjs'), sys.argv[2], '--minimize'],
                           cwd=root, start_new_session=True)
try:
    result = process.wait(timeout=240)
except subprocess.TimeoutExpired:
    # Own the entire child group, including browsers; no orphan on a deadlock.
    os.killpg(process.pid, signal.SIGKILL)
    process.wait()
    print('FAIL: semantic conformance exceeded the 240-second hard timeout', file=sys.stderr)
    sys.exit(124)
sys.exit(result if result >= 0 else 128 - result)
PY
