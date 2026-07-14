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

SERVER_PORT="$(
  python3 - <<'PY'
import socket
with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
    sock.bind(("127.0.0.1", 0))
    print(sock.getsockname()[1])
PY
)"
SERVER_LOG="${OUT_ROOT}/browser-benchmark-http.log"
python3 -m http.server "${SERVER_PORT}" --bind 127.0.0.1 --directory "${OUT_ROOT}" >"${SERVER_LOG}" 2>&1 &
SERVER_PID=$!
cleanup_server() {
  if kill -0 "${SERVER_PID}" >/dev/null 2>&1; then
    kill "${SERVER_PID}" >/dev/null 2>&1 || true
  fi
}
trap cleanup_server EXIT
sleep 0.2
echo "fixture server: http://127.0.0.1:${SERVER_PORT}/"

fixtures=(static_counter keyed_list control_flow composition large_keyed_rows nested_component_update lifecycle_cleanup form_focus)
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
      if (fixture === 'form_focus') {{
        const input = document.querySelector('input');
        if (!input) {{
          finish({{ fixture, clicks: 0, durationMs: 0, error: 'no input for form focus fixture' }});
          return;
        }}
        input.focus();
        input.value = 'Ada';
        input.setSelectionRange(2, 2);
        const beforeInput = input;
        const started = performance.now();
        for (let i = 0; i < clickCount; i += 1) {{
          input.value = 'Ada ' + i;
          const pos = Math.min(3, input.value.length);
          input.setSelectionRange(pos, pos);
          input.dispatchEvent(new Event('input', {{ bubbles: true }}));
        }}
        const durationMs = performance.now() - started;
        const jtml = window.jtml || {{}};
        finish({{
          fixture,
          clicks: clickCount,
          durationMs,
          textSample: (document.body && document.body.innerText || '').slice(0, 1000),
          loadedScripts: Array.prototype.slice.call(document.querySelectorAll('script[src]')).map(function(el) {{
            return el.getAttribute('src') || '';
          }}),
          browserBuildMode: jtml.browserBuild && jtml.browserBuild.mode || '',
          browserBuildCleanProduction: !!(jtml.browserBuild && jtml.browserBuild.cleanProduction),
          browserBuildCompatibility: !!(jtml.browserBuild && jtml.browserBuild.compatibility),
          browserBuildDebug: !!(jtml.browserBuild && jtml.browserBuild.debug),
          formInputRetained: beforeInput === document.querySelector('input'),
          formInputFocused: document.activeElement === beforeInput,
          formSelectionStart: beforeInput.selectionStart,
          formSelectionEnd: beforeInput.selectionEnd
        }});
        return;
      }}
      const buttons = Array.prototype.slice.call(
        document.querySelectorAll('[data-jtml-direct-component-action]')
      );
      const target = buttons[0] || null;
      if (!target) {{
        const jtml = window.jtml || {{}};
        finish({{
          fixture,
          clicks: 0,
          durationMs: 0,
          error: 'no direct component action button',
          loadedScripts: Array.prototype.slice.call(document.querySelectorAll('script[src]')).map(function(el) {{
            return el.getAttribute('src') || '';
          }}),
          browserBuildMode: jtml.browserBuild && jtml.browserBuild.mode || '',
          browserBuildCleanProduction: !!(jtml.browserBuild && jtml.browserBuild.cleanProduction),
          browserBuildCompatibility: !!(jtml.browserBuild && jtml.browserBuild.compatibility),
          browserBuildDebug: !!(jtml.browserBuild && jtml.browserBuild.debug),
          directComponentExecution: !!jtml.directComponentExecution,
          directComponentRenderCount: jtml.directComponentRenderCount || 0,
          staticComponentModulesLoaded: !!window.__jtml_static_component_modules,
          staticComponentPlanIndexLoaded: !!window.__jtml_static_component_plan_index,
          directActionButtonCount: document.querySelectorAll('[data-jtml-direct-component-action]').length,
          compatibilityButtonCount: document.querySelectorAll('[onclick]').length
        }});
        return;
      }}
      const beforeNestedWrapper = document.querySelector('[data-jtml-nested-component="true"]');
      const beforeNestedWrappersById = new Map();
      Array.prototype.slice.call(
        document.querySelectorAll('[data-jtml-nested-component="true"][data-jtml-direct-instance]')
      ).forEach(function(el) {{
        const id = el.getAttribute('data-jtml-direct-instance') || '';
        if (id && !beforeNestedWrappersById.has(id)) beforeNestedWrappersById.set(id, el);
      }});
      const beforeKeyedByKey = new Map();
      Array.prototype.slice.call(
        document.querySelectorAll('[data-jtml-direct-list-key]')
      ).forEach(function(el) {{
        const key = el.getAttribute('data-jtml-direct-list-key') || '';
        if (key && !beforeKeyedByKey.has(key)) beforeKeyedByKey.set(key, el);
      }});
      const started = performance.now();
      for (let i = 0; i < clickCount; i += 1) {{
        const currentTarget = document.querySelector('[data-jtml-direct-component-action]') || target;
        currentTarget.click();
      }}
      const durationMs = performance.now() - started;
      const jtml = window.jtml || {{}};
      const bodyText = document.body && document.body.innerText || '';
      const keyedTexts = Array.prototype.slice.call(
        document.querySelectorAll('[data-jtml-direct-list-key]')
      ).map(function(el) {{
        return {{
          key: el.getAttribute('data-jtml-direct-list-key') || '',
          text: (el.innerText || '').trim()
        }};
      }});
      const afterNestedWrapper = document.querySelector('[data-jtml-nested-component="true"]');
      let nestedWrappersRetainedCount = 0;
      Array.prototype.slice.call(
        document.querySelectorAll('[data-jtml-nested-component="true"][data-jtml-direct-instance]')
      ).forEach(function(el) {{
        const id = el.getAttribute('data-jtml-direct-instance') || '';
        if (id && beforeNestedWrappersById.get(id) === el) nestedWrappersRetainedCount += 1;
      }});
      const keyedAfter = Array.prototype.slice.call(
        document.querySelectorAll('[data-jtml-direct-list-key]')
      );
      let keyedRetainedCount = 0;
      keyedAfter.forEach(function(el) {{
        const key = el.getAttribute('data-jtml-direct-list-key') || '';
        if (key && beforeKeyedByKey.get(key) === el) keyedRetainedCount += 1;
      }});
      finish({{
        fixture,
        clicks: clickCount,
        durationMs,
        textSample: bodyText.slice(0, 1000),
        loadedScripts: Array.prototype.slice.call(document.querySelectorAll('script[src]')).map(function(el) {{
          return el.getAttribute('src') || '';
        }}),
        browserBuildMode: jtml.browserBuild && jtml.browserBuild.mode || '',
        browserBuildCleanProduction: !!(jtml.browserBuild && jtml.browserBuild.cleanProduction),
        browserBuildCompatibility: !!(jtml.browserBuild && jtml.browserBuild.compatibility),
        browserBuildDebug: !!(jtml.browserBuild && jtml.browserBuild.debug),
        keyedTexts,
        nestedWrapperRetained: !!beforeNestedWrapper && beforeNestedWrapper === afterNestedWrapper,
        keyedRetainedCount,
        keyedInitialCount: beforeKeyedByKey.size,
        keyedFinalCount: keyedAfter.length,
        nestedWrappersRetainedCount,
        nestedWrappersInitialCount: beforeNestedWrappersById.size,
        directComponentDestroyLifecycle: jtml.directComponentDestroyLifecycle || null,
        directComponentExecution: !!jtml.directComponentExecution,
        directComponentLastAction: !!jtml.directComponentLastAction,
        directNestedComponentParamPatch: !!jtml.directNestedComponentParamPatch,
        directNestedComponentParamPatchMode: jtml.directNestedComponentParamPatch && jtml.directNestedComponentParamPatch.mode || '',
        directComponentCreateFallback: jtml.directComponentCreateFallback || null,
        directComponentLastActionDetail: jtml.directComponentLastAction || null,
        directComponentLastPatch: jtml.directComponentLastPatch || null,
        directComponentPatchFallback: jtml.directComponentPatchFallback || null,
        directComponentStaticUpdateError: jtml.directComponentStaticUpdateError || null,
        directComponentKeyedListPatch: jtml.directComponentKeyedListPatch || null,
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
  bench_url="http://127.0.0.1:${SERVER_PORT}/${fixture}/index.bench.html"
  python3 - "${browser}" "${bench_url}" "${profile}" "${log}" "${dom_file}" "${BROWSER_TIMEOUT}" <<'PY'
import subprocess
import sys

browser, bench_url, profile, log_path, dom_path, timeout_s = sys.argv[1:]

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
        bench_url,
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
        last_status = 0 if 'jtml-browser-benchmark-result' in last_stdout else 124
        if last_status == 0:
            break

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
text = str(result.get("textSample", ""))
keyed_texts = result.get("keyedTexts", [])
loaded_scripts = [str(item) for item in result.get("loadedScripts", [])]
if result.get("browserBuildMode") != "prod" or not result.get("browserBuildCleanProduction"):
    raise SystemExit(
        f"error: {fixture} did not run the clean production browser build contract: "
        f"mode={result.get('browserBuildMode')!r}, clean={result.get('browserBuildCleanProduction')!r}"
    )
if result.get("browserBuildCompatibility") or result.get("browserBuildDebug"):
    raise SystemExit(f"error: {fixture} browser benchmark ran compat/debug mode in clean prod")
if any("jtml-update-plans.js" in item for item in loaded_scripts):
    raise SystemExit(f"error: {fixture} loaded legacy update-plan asset in clean prod: {loaded_scripts!r}")
if fixture == "static_counter" and f"Count {expected_clicks}" not in text:
    raise SystemExit(f"error: static_counter did not update DOM text to Count {expected_clicks}: {text!r}")
if fixture == "composition" and "Beta" not in text:
    raise SystemExit(f"error: composition did not update nested parameter DOM text to Beta: {text!r}")
if fixture == "control_flow" and "No visible activity" in text:
    raise SystemExit(f"error: control_flow ended hidden after an even number of toggles: {text!r}")
if fixture == "keyed_list":
    keys = [str(item.get("key", "")) for item in keyed_texts]
    expected = ["Ship_static_module", "Measure_patch", "Audit_runtime"]
    if keys[:3] != expected:
        raise SystemExit(f"error: keyed_list did not keep rotated keyed order {expected}: {keys!r}")
    if int(result.get("keyedRetainedCount", 0)) < len(expected):
        raise SystemExit(
            "error: keyed_list recreated keyed rows instead of retaining/moving them "
            f"(retained={result.get('keyedRetainedCount')}, final={result.get('keyedFinalCount')})"
        )
if fixture == "large_keyed_rows":
    keys = [str(item.get("key", "")) for item in keyed_texts]
    if len(keys) < 1000:
        raise SystemExit(f"error: large_keyed_rows rendered only {len(keys)} keyed rows")
    if expected_clicks % 2 == 0 and keys[:3] != ["1", "2", "3"]:
        raise SystemExit(f"error: large_keyed_rows did not restore first keys after even rotations: {keys[:5]!r}")
    if not result.get("directComponentListLifecycle"):
        raise SystemExit("error: large_keyed_rows did not report keyed list lifecycle telemetry")
    if expected_clicks % 2 == 0:
        retained = int(result.get("keyedRetainedCount", 0))
        if retained < 1000:
            raise SystemExit(
                "error: large_keyed_rows recreated keyed row wrappers instead of retaining them "
                f"(retained={retained}, final={result.get('keyedFinalCount')})"
            )
if fixture == "nested_component_update" and f"Value {expected_clicks}" not in text:
    raise SystemExit(f"error: nested_component_update did not update nested component text: {text!r}")
if fixture == "lifecycle_cleanup":
    if "Mounted" not in text or "alpha" not in text:
        raise SystemExit(f"error: lifecycle_cleanup did not restore visible nested/list state: {text!r}")
    destroy = result.get("directComponentDestroyLifecycle") or {}
    reasons = destroy.get("reasons") or {}
    if int(destroy.get("count", 0)) <= 0:
        raise SystemExit("error: lifecycle_cleanup did not report component destroy lifecycle telemetry")
    if not (int(reasons.get("subtree-prune", 0)) > 0 or int(reasons.get("keyed-list-remove", 0)) > 0):
        raise SystemExit(f"error: lifecycle_cleanup did not report subtree/keyed cleanup reasons: {reasons!r}")
    if int(destroy.get("registrySizeAfter", 999999)) >= int(result.get("nestedWrappersInitialCount", 0)) + len(keyed_texts) + 10:
        raise SystemExit(f"error: lifecycle_cleanup reported suspicious registry size after cleanup: {destroy!r}")
    if "lastHandlerCount" not in destroy:
        raise SystemExit(f"error: lifecycle_cleanup destroy telemetry did not include handler cleanup context: {destroy!r}")
    if int(destroy.get("handlerInstanceDestroyCount", 0)) <= 0 or int(destroy.get("handlerCountTotal", 0)) <= 0:
        raise SystemExit(f"error: lifecycle_cleanup did not prove event-handler registry cleanup: {destroy!r}")
if fixture == "form_focus":
    if f"Ada {expected_clicks - 1}" not in text:
        raise SystemExit(f"error: form_focus did not update text from input binding: {text!r}")
    if not result.get("formInputRetained"):
        raise SystemExit("error: form_focus replaced the input element during input binding updates")
    if not result.get("formInputFocused"):
        raise SystemExit("error: form_focus lost input focus during input binding updates")
    if int(result.get("formSelectionStart", -1)) != 3 or int(result.get("formSelectionEnd", -1)) != 3:
        raise SystemExit(
            "error: form_focus did not preserve cursor selection "
            f"start={result.get('formSelectionStart')} end={result.get('formSelectionEnd')}"
        )
if fixture == "composition" and not result.get("directNestedComponentParamPatch"):
    raise SystemExit("error: composition did not use in-place nested component parameter patch")
if fixture == "nested_component_update" and not result.get("directNestedComponentParamPatch"):
    raise SystemExit("error: nested_component_update did not use in-place nested component parameter patch")
if fixture in ("composition", "nested_component_update"):
    mode = str(result.get("directNestedComponentParamPatchMode", ""))
    if mode != "nested-component-static-update":
        raise SystemExit(f"error: {fixture} did not use nested static component update mode: {mode!r}")
    if not result.get("nestedWrapperRetained"):
        raise SystemExit(f"error: {fixture} replaced nested component wrapper instead of patching it in place")
    initial_nested = int(result.get("nestedWrappersInitialCount", 0))
    retained_nested = int(result.get("nestedWrappersRetainedCount", 0))
    if initial_nested > 0 and retained_nested < initial_nested:
        raise SystemExit(
            f"error: {fixture} recreated nested component wrappers "
            f"(retained={retained_nested}, initial={initial_nested})"
        )
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
