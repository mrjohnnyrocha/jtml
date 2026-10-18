#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
python3 "${ROOT}/tests/test_local_server_security.py" "${JTML_BIN:-"${ROOT}/build/jtml"}"
