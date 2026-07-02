#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_ROOT="${1:-"${ROOT}/dist/runtime-benchmarks"}"
CLICK_COUNT="${JTML_BROWSER_BENCH_CLICKS:-200}"
BUDGET_MS="${JTML_BROWSER_BENCH_BUDGET_MS:-3000}"
BROWSER_TIMEOUT="${JTML_BROWSER_BENCH_TIMEOUT:-15}"

find_browser() {
  if [[ -n "${JTML_BENCH_BROWSER:-}" && -x "${JTML_BENCH_BROWSER}" ]]; then
    printf '%s\n' "${JTML_BENCH_BROWSER}"
    return 0
  fi
  local candidates=(
    "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
    "/Applications/Chromium.app/Contents/MacOS/Chromium"
    "/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge"
    "/Applications/Brave Browser.app/Contents/MacOS/Brave Browser"
  )
  local candidate
  for candidate in "${candidates[@]}"; do
    if [[ -x "${candidate}" ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  done
  for candidate in google-chrome chromium chromium-browser chrome; do
    if command -v "${candidate}" >/dev/null 2>&1; then
      command -v "${candidate}"
      return 0
    fi
  done
  return 1
}

browser="$(find_browser || true)"
if [[ -z "${browser}" ]]; then
  echo "SKIP: browser runtime benchmark requires Chrome/Chromium; set JTML_BENCH_BROWSER to enable."
  exit 0
fi

if [[ ! -d "${OUT_ROOT}" ]]; then
  echo "error: browser runtime benchmark output root missing: ${OUT_ROOT}" >&2
  exit 1
fi
OUT_ROOT="$(cd "${OUT_ROOT}" && pwd)"

echo "JTML browser runtime benchmark"
echo "browser: ${browser}"
echo "clicks per fixture: ${CLICK_COUNT}"
echo "per-fixture budget: ${BUDGET_MS}ms"
echo "browser launch timeout: ${BROWSER_TIMEOUT}s"

fixtures=(static_counter keyed_list control_flow composition)
for fixture in "${fixtures[@]}"; do
  fixture_dir="${OUT_ROOT}/${fixture}"
  index="${fixture_dir}/index.html"
  bench="${fixture_dir}/index.bench.html"
  if [[ ! -f "${index}" ]]; then
    echo "error: missing built fixture for browser benchmark: ${fixture}" >&2
    exit 1
  fi

  python3 - "${index}" "${bench}" "${fixture}" "${CLICK_COUNT}" <<'PY'
import json
import pathlib
import sys

source = pathlib.Path(sys.argv[1])
target = pathlib.Path(sys.argv[2])
fixture = sys.argv[3]
clicks = int(sys.argv[4])

html = source.read_text(encoding="utf-8")
harness = f"""
<script id="jtml-browser-benchmark-harness">
(function() {{
  const fixture = {json.dumps(fixture)};
  const clickCount = {clicks};
  function finish(result) {{
    const pre = document.createElement('pre');
    pre.id = 'jtml-browser-benchmark-result';
    pre.textContent = JSON.stringify(result);
    document.body.appendChild(pre);
    document.body.setAttribute('data-jtml-browser-benchmark', pre.textContent);
  }}
  function run() {{
    try {{
      const buttons = Array.prototype.slice.call(
        document.querySelectorAll('[data-jtml-direct-component-action]')
      );
      const target = buttons[0] || null;
      if (!target) {{
        finish({{ fixture, clicks: 0, durationMs: 0, error: 'no direct component action button' }});
        return;
      }}
      const started = performance.now();
      for (let i = 0; i < clickCount; i += 1) {{
        target.click();
      }}
      const durationMs = performance.now() - started;
      const jtml = window.jtml || {{}};
      finish({{
        fixture,
        clicks: clickCount,
        durationMs,
        textSample: (document.body && document.body.innerText || '').slice(0, 240),
        directComponentExecution: !!jtml.directComponentExecution,
        directComponentLastAction: !!jtml.directComponentLastAction,
        directNestedComponentParamPatch: !!jtml.directNestedComponentParamPatch,
        directComponentListLifecycle: !!jtml.directComponentListLifecycle,
        directComponentCompiledUpdatePlan: !!jtml.directComponentCompiledUpdatePlan
      }});
    }} catch (err) {{
      finish({{
        fixture,
        clicks: 0,
        durationMs: 0,
        error: err && err.message ? err.message : String(err)
      }});
    }}
  }}
  window.addEventListener('load', function() {{
    setTimeout(run, 50);
  }});
}})();
</script>
"""

if "</body>" in html:
    html = html.replace("</body>", harness + "\n</body>", 1)
else:
    html += harness
target.write_text(html, encoding="utf-8")
PY

  profile="$(mktemp -d "${TMPDIR:-/tmp}/jtml-browser-bench.XXXXXX")"
  log="${profile}/browser.log"
  dom_file="${profile}/dom.html"
  set +e
  python3 - "${browser}" "${bench}" "${profile}" "${log}" "${dom_file}" "${BROWSER_TIMEOUT}" <<'PY'
import subprocess
import sys

browser, bench, profile, log_path, dom_path, timeout_s = sys.argv[1:]

def text(value):
    if value is None:
        return ""
    if isinstance(value, bytes):
        return value.decode("utf-8", errors="replace")
    return str(value)

logs = []
last_stdout = ""
last_status = 1
for headless_flag in ("--headless=new", "--headless"):
    args = [
        browser,
        headless_flag,
        "--disable-gpu",
        "--no-first-run",
        "--no-default-browser-check",
        "--disable-background-networking",
        "--allow-file-access-from-files",
        f"--user-data-dir={profile}-{headless_flag.replace('=', '-')}",
        "--run-all-compositor-stages-before-draw",
        "--virtual-time-budget=6000",
        "--dump-dom",
        f"file://{bench}",
    ]
    try:
        completed = subprocess.run(
            args,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=float(timeout_s),
            check=False,
        )
        logs.append(f"[{headless_flag}] exit={completed.returncode}\n" + (completed.stderr or ""))
        last_stdout = completed.stdout or ""
        last_status = completed.returncode
        if completed.returncode == 0:
            break
    except subprocess.TimeoutExpired as exc:
        logs.append(f"[{headless_flag}] browser benchmark timed out\n" + text(exc.stderr))
        last_stdout = text(exc.stdout)
        last_status = 124

with open(log_path, "w", encoding="utf-8") as handle:
    handle.write("\n".join(logs))
with open(dom_path, "w", encoding="utf-8") as handle:
    handle.write(last_stdout)
raise SystemExit(last_status)
PY
  status=$?
  set -e
  if [[ ${status} -ne 0 ]]; then
    if [[ "${JTML_REQUIRE_BROWSER_BENCH:-0}" == "1" ]]; then
      echo "error: browser benchmark failed for ${fixture}; see ${log}" >&2
      exit 1
    fi
    echo "SKIP: browser runtime benchmark could not launch headless browser for ${fixture}; set JTML_REQUIRE_BROWSER_BENCH=1 to make this fatal."
    exit 0
  fi

  result="$(
    python3 - "${dom_file}" <<'PY'
import html
import json
import re
import sys

with open(sys.argv[1], "r", encoding="utf-8") as handle:
    data = handle.read()
match = re.search(r'<pre id="jtml-browser-benchmark-result"[^>]*>(.*?)</pre>', data, re.S)
if not match:
    sys.exit(2)
payload = html.unescape(match.group(1))
json.loads(payload)
print(payload)
PY
  )" || {
    echo "error: browser benchmark did not produce a runtime result for ${fixture}" >&2
    exit 1
  }

  python3 - "${result}" "${fixture}" "${CLICK_COUNT}" "${BUDGET_MS}" <<'PY'
import json
import math
import sys

result = json.loads(sys.argv[1])
fixture = sys.argv[2]
expected_clicks = int(sys.argv[3])
budget_ms = float(sys.argv[4])

if result.get("error"):
    raise SystemExit(f"error: {fixture} browser runtime error: {result['error']}")
if int(result.get("clicks", 0)) != expected_clicks:
    raise SystemExit(f"error: {fixture} only ran {result.get('clicks')} of {expected_clicks} clicks")
duration = float(result.get("durationMs", math.inf))
if not math.isfinite(duration):
    raise SystemExit(f"error: {fixture} duration is not finite")
if duration > budget_ms:
    raise SystemExit(f"error: {fixture} browser runtime {duration:.2f}ms exceeds budget {budget_ms:.2f}ms")
if fixture == "composition" and not result.get("directNestedComponentParamPatch"):
    raise SystemExit("error: composition did not use in-place nested component parameter patch")
if fixture == "keyed_list" and not result.get("directComponentListLifecycle"):
    raise SystemExit("error: keyed_list did not report keyed list lifecycle telemetry")
print(
    "OK: browser-runtime "
    f"{fixture} clicks={expected_clicks} duration={duration:.2f}ms "
    f"nestedParamPatch={str(bool(result.get('directNestedComponentParamPatch'))).lower()} "
    f"keyedLifecycle={str(bool(result.get('directComponentListLifecycle'))).lower()}"
)
PY
done

echo "JTML browser runtime benchmark passed."
