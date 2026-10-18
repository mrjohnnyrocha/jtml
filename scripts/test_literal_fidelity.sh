#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${JTML_LITERAL_OUT:-"${ROOT}/dist/literal-fidelity"}"
for mode in clean compat; do
  if [[ "$mode" == compat ]]; then
    "${JTML_BIN:-"${ROOT}/build/jtml"}" build "$ROOT/tests/fixtures/production/literal-fidelity/index.jtml" --target browser --out "$OUT/$mode" --compat
  else
    "${JTML_BIN:-"${ROOT}/build/jtml"}" build "$ROOT/tests/fixtures/production/literal-fidelity/index.jtml" --target browser --out "$OUT/$mode"
  fi
done
python3 - "$OUT" <<'PY'
from pathlib import Path
import re,sys
root=Path(sys.argv[1])
assert not (root/'clean/jtml-update-plans.js').exists()
for mode in ('clean','compat'):
 for file in (root/mode).rglob('*.js'):
  assert not re.search(r'\beval\s*\(|\bnew\s+Function\b',file.read_text()),file
 assert (root/mode/'jtml-runtime.js').stat().st_size<=272000
PY
node "$ROOT/tests/browser/literal_fidelity.cjs" "$OUT"

python3 - "${JTML_BIN:-"${ROOT}/build/jtml"}" <<'PYTHON'
import pathlib, subprocess, sys, tempfile
with tempfile.TemporaryDirectory(prefix='jtml-literal-invalid-') as directory:
 root=pathlib.Path(directory)
 for index, literal in enumerate((r'"\q"', r'"\u12xz"', r'"\uD800"', '"unterminated')):
  source=root/'invalid.jtml'
  source.write_text('jtml 2\nlet text = '+literal+'\npage\n  show text\n')
  result=subprocess.run([sys.argv[1], 'build', str(source), '--target', 'browser', '--out', str(root/str(index))], capture_output=True, text=True, timeout=10)
  assert result.returncode != 0, (literal, result.stdout, result.stderr)
  assert result.stderr.strip(), (literal, 'missing diagnostic')
print('malformed browser source: 4/4 rejected with diagnostics')
PYTHON
