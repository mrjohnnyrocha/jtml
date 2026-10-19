#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
JTML="${JTML_BIN:-"${ROOT}/build/jtml"}"
OUT="${JTML_TASKBOARD_NEGATIVE_OUT:-"${ROOT}/dist/taskboard-negative-suite"}"
mkdir -p "$OUT"
"$JTML" build "$ROOT/tests/fixtures/production/taskboard-core/index.jtml" --target browser --out "$OUT/taskboard"
"$JTML" build "$ROOT/tests/fixtures/production/taskboard-dependencies/index.jtml" --target browser --out "$OUT/dependencies"
for mode in clean compat; do
  if [[ "$mode" == compat ]]; then
    "$JTML" build "$ROOT/tests/fixtures/production/action-arguments/index.jtml" --target browser --compat --out "$OUT/arguments-$mode"
  else
    "$JTML" build "$ROOT/tests/fixtures/production/action-arguments/index.jtml" --target browser --out "$OUT/arguments-$mode"
  fi
  node "$ROOT/tests/browser/action_arguments.cjs" "$OUT/arguments-$mode"
done
node "$ROOT/tests/browser/component_tokenizer.cjs" "$OUT/arguments-clean/jtml-runtime.js"
node "$ROOT/tests/browser/taskboard_dependencies.cjs" "$OUT/dependencies"
node "$ROOT/tests/browser/taskboard_negative.cjs" "$OUT/taskboard"
# This executes every rejection/provenance audit. BLOCKED is reported, not PASS.
# The strict command below returns 2 while original diagnostic coordinates conflict:
# python3 tests/test_taskboard_rejections.py build/jtml <fresh-out> --require-provenance
rejections="$(mktemp -d "${TMPDIR:-/tmp}/jtml-taskboard-rejections.XXXXXX")"
python3 "$ROOT/tests/test_taskboard_rejections.py" "$JTML" "$rejections"
cp "$rejections/n04-results.json" "$OUT/n04-results.json"
rm -rf "$rejections"
