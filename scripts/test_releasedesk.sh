#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
JTML="${JTML_BIN:-"${ROOT}/build/jtml"}"
OUT="${ROOT}/dist/releasedesk"
"$JTML" check "${ROOT}/apps/releasedesk/index.jtml"
"$JTML" lint "${ROOT}/apps/releasedesk/index.jtml"
"$JTML" build "${ROOT}/apps/releasedesk" --target browser --out "$OUT"
PYTHONDONTWRITEBYTECODE=1 python3 "${ROOT}/apps/releasedesk/test_api.py"
node "${ROOT}/apps/releasedesk/test_browser.cjs" "$OUT"
