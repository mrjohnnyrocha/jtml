#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${BUILD_DIR:-$root/build}"
export JTML_REAL_BIN="$(cd "$build_dir" && pwd)/jtml"
emitter="$(dirname "$JTML_REAL_BIN")/jtml_browser_local_runtime_asset"
work="$(mktemp -d "${TMPDIR:-/tmp}/jtml-local-runtime.XXXXXX")"
trap 'rm -rf "$work"' EXIT
export JTML_TEST_RUNTIME_ASSET="$work/local-runtime.js"
export JTML_BIN="$root/tests/browser/browser_local_build.sh"

"$emitter" > "$JTML_TEST_RUNTIME_ASSET"
node --check "$JTML_TEST_RUNTIME_ASSET"
fixture="$root/tests/fixtures/performance/composition.jtml"
"$JTML_REAL_BIN" build "$fixture" --target browser --out "$work/full"
"$JTML_BIN" build "$fixture" --target browser --out "$work/local"
for asset in index.html app.js components/index.js; do
  cmp "$work/full/$asset" "$work/local/$asset"
done
cmp "$JTML_TEST_RUNTIME_ASSET" "$work/local/jtml-runtime.js"
# Check callers as well as declarations, including the generated component JS.
if grep -En 'runLiveComponentAction|applyLiveBodyPlanRender|refreshLiveBodyPlanRender|hasLiveBodyPlanServerRender|jtmlStableHash|wsPort|new WebSocket|/api/(event|component-action|rendered-components)|@JTML_' \
    "$work/local/jtml-runtime.js" "$work/local/app.js" "$work/local/components/index.js"; then
  echo "Browser-local assets contain a live transport reference" >&2
  exit 1
fi
python3 - "$work" <<'PY'
import gzip
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
full, local = [(root / mode / "jtml-runtime.js").read_bytes() for mode in ("full", "local")]
assert len(local) < len(full) <= 273000
print(f"Browser-local runtime raw {len(full)} -> {len(local)} bytes, "
      f"gzip {len(gzip.compress(full, mtime=0))} -> {len(gzip.compress(local, mtime=0))} bytes")
PY

for suite in strict_csp root_effects component_action_freshness root_conditional_mount imported_components input_state_sync route_precedence private_fail_stop private_fetch_fencing private_fetch_publication private_finite_arithmetic; do
  node "$root/tests/browser/$suite.cjs"
done
JTML_PRIVATE_COMPILER="$(dirname "$JTML_REAL_BIN")/jtml_browser_private_compiler" \
  node "$root/tests/browser/private_import_authority.cjs"
"$root/scripts/test_releasedesk.sh"
