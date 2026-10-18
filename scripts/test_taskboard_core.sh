#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${JTML_TASKBOARD_OUT:-"${ROOT}/dist/taskboard-core"}"
"${JTML_BIN:-"${ROOT}/build/jtml"}" build \
  "${ROOT}/tests/fixtures/production/taskboard-core/index.jtml" --target browser --out "${OUT}"
python3 - "${OUT}" <<'PY'
import pathlib, re, sys
root = pathlib.Path(sys.argv[1])
for name in ('index.html', 'jtml-runtime.js', 'components/index.js', 'app.js'):
    source = (root / name).read_text()
    assert not re.search(r'\bnew\s+Function\b|\beval\s*\(', source), name
    assert 'jtml-update-plans.js' not in source, name
    if name == 'components/index.js':
        for forbidden in ('__jtml_static_update_plans', '"bodyPlan"', '"bodySource"', '"bodyHex"'):
            assert forbidden not in source, (name, forbidden)
    print(f'asset: {name} {(root / name).stat().st_size} bytes')
assert not (root / 'jtml-update-plans.js').exists()
assert (root / 'jtml-runtime.js').stat().st_size <= 272000
PY
"${JTML_BIN:-"${ROOT}/build/jtml"}" build \
  "${ROOT}/tests/fixtures/production/retained-control/index.jtml" --target browser --out "${OUT}/retained-control"
node "${ROOT}/tests/browser/taskboard_core.cjs" "${OUT}"
"${JTML_BIN:-"${ROOT}/build/jtml"}" build \
  "${ROOT}/tests/fixtures/production/component-events/index.jtml" --target browser --out "${OUT}/component-events"
node "${ROOT}/tests/browser/component_events.cjs" "${OUT}/component-events"
node "${ROOT}/tests/browser/taskboard_workflow.cjs" "${OUT}"
