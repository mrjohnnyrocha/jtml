#!/usr/bin/env bash
set -euo pipefail

# Adapter for existing browser suites. Only their clean split build gets the
# separately emitted test asset. All compilation still uses the normal CLI.
: "${JTML_REAL_BIN:?}"
: "${JTML_TEST_RUNTIME_ASSET:?}"
args=("$@")
if [[ "${1:-}" != build ]]; then
  exec "$JTML_REAL_BIN" "${args[@]}"
fi
shift
target=
output=
while (( $# )); do
  case "$1" in
    --compat|--debug) exec "$JTML_REAL_BIN" "${args[@]}" ;;
    --target) target="$2"; shift ;;
    --out|-o) output="$2"; shift ;;
  esac
  shift
done

"$JTML_REAL_BIN" "${args[@]}"
if [[ "$target" == browser && -n "$output" ]]; then
  test -f "$output/jtml-runtime.js"
  cp "$JTML_TEST_RUNTIME_ASSET" "$output/jtml-runtime.js"
fi
