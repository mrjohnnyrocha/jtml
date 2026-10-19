#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHONDONTWRITEBYTECODE=1 python3 "$repo_root/tests/test_async_runtime.py" "${JTML_BIN:-"$repo_root/build/jtml"}"
