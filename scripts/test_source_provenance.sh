#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
binary="${JTML_BIN:-$root/build/jtml}"
out="$(mktemp -d "${TMPDIR:-/tmp}/jtml-strict-provenance.XXXXXX")"
python3 "$root/tests/test_taskboard_rejections.py" "$binary" "$out" --require-provenance
python3 "$root/tests/test_source_diagnostics.py" "$binary"
