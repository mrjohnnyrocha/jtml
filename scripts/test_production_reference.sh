#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
JTML="${JTML_BIN:-"${ROOT}/build/jtml"}"
OUT="${ROOT}/dist/production-reference"
FIXTURE="${ROOT}/tests/fixtures/production/reference-app"
INDEX_BUDGET_BYTES=70000
# A browser is required: a successful compile alone is not interaction evidence.
export JTML_REQUIRE_BROWSER_BENCH=1
mkdir -p "${OUT}"
source="${FIXTURE}/index.jtml"
case "${JTML_REFERENCE_REGRESSION:-}" in
  "") ;;
  form-submit|keyed-local-actions|routing|nested-heading)
    source="${FIXTURE}/regressions/${JTML_REFERENCE_REGRESSION}.jtml" ;;
  *) echo "error: unknown reference regression" >&2; exit 1 ;;
esac
"${JTML}" build "${source}" --target browser --out "${OUT}/reference-app"
python3 - "${OUT}/reference-app" <<'PY'
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1])
assets = ("index.html", "jtml-runtime.js", "components/index.js", "app.js")
for name in assets:
    path = root / name
    assert path.is_file(), f"missing production asset: {name}"
    # The existing microbenchmark budgets remain in benchmark_runtime.sh.
    # This composed app reports its sizes; only the shared runtime uses that budget.
    if name == "jtml-runtime.js":
        assert path.stat().st_size <= 272000, "shared runtime exceeds existing budget"
    print(f"asset: {name} {path.stat().st_size} bytes")
    source = path.read_text()
    assert not re.search(r"\bnew\s+Function\b|\beval\s*\(", source), f"dynamic compilation in {name}"
    if name in ("index.html", "app.js"):
        assert "jtml-update-plans.js" not in source, f"legacy asset reference in {name}"
        assert "compatibilityUpdatePlans" not in source, f"compatibility mode advertised in {name}"
assert not (root / "jtml-update-plans.js").exists(), "legacy update-plan asset emitted"
assert "__jtml_static_update_plans" not in (root / "components/index.js").read_text(), "legacy global published"
print("OK: clean production assets, shared runtime budget, no dynamic compilation")
PY
"${ROOT}/scripts/benchmark_browser_runtime.sh" "${OUT}" reference-app

if [[ -z "${JTML_REFERENCE_REGRESSION:-}" ]]; then
  echo "PASS: supported production reference application"
  index_bytes="$(wc -c < "${OUT}/reference-app/index.html" | tr -d ' ')"
  if (( index_bytes > INDEX_BUDGET_BYTES )); then
    echo "KNOWN SIZE REGRESSION: index.html ${index_bytes} bytes exceeds the existing ${INDEX_BUDGET_BYTES}-byte microfixture budget"
    echo "  __jtml_client_manifest is source-rich and duplicated across project/component definition payloads"
  fi

  for regression in form-submit keyed-local-actions routing nested-heading; do
    case "${regression}" in
      form-submit) expected_failure="form-submit saves input in visible DOM" ;;
      keyed-local-actions) expected_failure="keyed-local-actions saves input in visible DOM" ;;
      routing) expected_failure="routing starts without initialization errors" ;;
      nested-heading) expected_failure="nested heading updates when parameter is named title" ;;
    esac
    log="$(mktemp "${TMPDIR:-/tmp}/jtml-reference-${regression}.XXXXXX")"
    if JTML_REFERENCE_REGRESSION="${regression}" "$0" >"${log}" 2>&1; then
      rm -f "${log}"
      echo "error: ${regression} unexpectedly passed; update its regression status" >&2
      exit 1
    fi
    if ! grep -Fq "${expected_failure}" "${log}"; then
      cat "${log}" >&2
      rm -f "${log}"
      echo "error: ${regression} failed for an unexpected reason" >&2
      exit 1
    fi
    rm -f "${log}"
    echo "KNOWN FAILURE: ${regression} — ${expected_failure}"
  done
fi
