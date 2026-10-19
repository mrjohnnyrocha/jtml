#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${JTML_KEYED_OUT:-"${ROOT}/dist/keyed-lifecycle"}"
for fixture in partial nested; do
  "${JTML_BIN:-"${ROOT}/build/jtml"}" build \
    "${ROOT}/tests/fixtures/production/keyed-lifecycle/${fixture}.jtml" --target browser --out "${OUT}/${fixture}"
done
python3 - "${OUT}" <<'PYTHON'
from pathlib import Path
import re, sys
for fixture in ('partial', 'nested'):
    root = Path(sys.argv[1]) / fixture
    assert not (root / 'jtml-update-plans.js').exists()
    for name in ('index.html', 'jtml-runtime.js', 'components/index.js', 'app.js'):
        source = (root / name).read_text()
        assert not re.search(r'\bnew\s+Function\b|\beval\s*\(', source), (fixture, name)
        assert 'jtml-update-plans.js' not in source
        if name == 'components/index.js':
            assert '__jtml_static_update_plans' not in source
        print(f'asset: {fixture}/{name} {(root / name).stat().st_size} bytes')
    assert (root / 'jtml-runtime.js').stat().st_size <= 272000
PYTHON
node "${ROOT}/tests/browser/keyed_lifecycle.cjs" "${OUT}"
node "${ROOT}/tests/browser/keyed_commit.cjs" "${OUT}"

"${JTML_BIN:-"${ROOT}/build/jtml"}" build \
  "${ROOT}/tests/fixtures/production/static-keyed-path/index.jtml" --target browser --out "${OUT}/static-keyed-path"
node "${ROOT}/tests/browser/component_render_path.cjs" "${OUT}/static-keyed-path/jtml-runtime.js"
node "${ROOT}/tests/browser/static_keyed_path.cjs" "${OUT}/static-keyed-path"
