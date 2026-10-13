#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_ROOT="${1:-"${ROOT}/dist/runtime-benchmarks"}"
CLICK_COUNT="${JTML_BROWSER_BENCH_CLICKS:-200}"
BUDGET_MS="${JTML_BROWSER_BENCH_BUDGET_MS:-3000}"
BROWSER_TIMEOUT="${JTML_BROWSER_BENCH_TIMEOUT:-15}"
REQUIRE_CLEAN_EXIT="${JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT:-0}"

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
  if [[ "${JTML_REQUIRE_BROWSER_BENCH:-0}" == "1" ]]; then
    echo "error: required browser runtime benchmark needs Chrome/Chromium; set JTML_BENCH_BROWSER." >&2
    exit 1
  fi
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
echo "require clean browser exit: ${REQUIRE_CLEAN_EXIT}"

SERVER_PORT="$(
  python3 - <<'PY'
import socket
with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
    sock.bind(("127.0.0.1", 0))
    print(sock.getsockname()[1])
PY
)"
SERVER_LOG="${OUT_ROOT}/browser-benchmark-http.log"
RESULT_ROOT="${OUT_ROOT}/.browser-benchmark-results"
rm -rf "${RESULT_ROOT}"
mkdir -p "${RESULT_ROOT}"
python3 - "${SERVER_PORT}" "${OUT_ROOT}" "${RESULT_ROOT}" >"${SERVER_LOG}" 2>&1 <<'PY' &
import functools
import http.server
import json
import pathlib
import re
import sys
import urllib.parse

port = int(sys.argv[1])
out_root = sys.argv[2]
result_root = pathlib.Path(sys.argv[3])

class BenchmarkHandler(http.server.SimpleHTTPRequestHandler):
  def do_POST(self):
    parsed = urllib.parse.urlparse(self.path)
    fixture = urllib.parse.parse_qs(parsed.query).get("fixture", [""])[0]
    if parsed.path != "/__jtml_browser_benchmark_result" or not re.fullmatch(r"[A-Za-z0-9_-]+", fixture):
      self.send_error(404)
      return
    length = int(self.headers.get("Content-Length", "0"))
    if length <= 0 or length > 1024 * 1024:
      self.send_error(400)
      return
    payload = self.rfile.read(length)
    try:
      json.loads(payload)
    except (UnicodeDecodeError, json.JSONDecodeError):
      self.send_error(400)
      return
    (result_root / f"{fixture}.json").write_bytes(payload)
    self.send_response(204)
    self.end_headers()

handler = functools.partial(BenchmarkHandler, directory=out_root)
http.server.ThreadingHTTPServer(("127.0.0.1", port), handler).serve_forever()
PY
SERVER_PID=$!
cleanup_server() {
  if kill -0 "${SERVER_PID}" >/dev/null 2>&1; then
    kill "${SERVER_PID}" >/dev/null 2>&1 || true
  fi
}
trap cleanup_server EXIT
sleep 0.2
echo "fixture server: http://127.0.0.1:${SERVER_PORT}/"

fixtures=(static_counter keyed_list control_flow composition large_keyed_updates large_keyed_reorder nested_component_update lifecycle_cleanup form_focus)
if (( $# > 1 )); then
  shift
  fixtures=("$@")
fi
for fixture in "${fixtures[@]}"; do
  fixture_dir="${OUT_ROOT}/${fixture}"
  index="${fixture_dir}/index.html"
  bench="${fixture_dir}/index.bench.html"
  if [[ ! -f "${index}" ]]; then
    echo "error: missing built fixture for browser benchmark: ${fixture}" >&2
    exit 1
  fi

  python3 - "${index}" "${bench}" "${fixture}" "${CLICK_COUNT}" "${ROOT}" <<'PY'
import json
import pathlib
import os
import sys

source = pathlib.Path(sys.argv[1])
target = pathlib.Path(sys.argv[2])
fixture = sys.argv[3]
clicks = int(sys.argv[4])

html = source.read_text(encoding="utf-8")
if fixture == "reference-app":
    scenario = pathlib.Path(sys.argv[5]) / "tests/fixtures/production/reference-app/browser.js"
    # Install network guards before any application script executes.
    html = html.replace("<head>", "<head><script>window.jtmlReferenceRegression = " + json.dumps(os.environ.get("JTML_REFERENCE_REGRESSION", "")) + ";" + scenario.read_text(encoding="utf-8") + "</script>", 1)
harness = f"""
<script id="jtml-browser-benchmark-harness">
(function() {{
  const fixture = {json.dumps(fixture)};
  const clickCount = {clicks};
  let running = false;
  const fixtureClickCount =
    fixture === 'reference-app' ? 4 :
    (fixture === 'large_keyed_updates' || fixture === 'large_keyed_reorder') ? clickCount + 1 : clickCount;
  function finish(result) {{
    const pre = document.createElement('pre');
    pre.id = 'jtml-browser-benchmark-result';
    pre.textContent = JSON.stringify(result);
    document.body.appendChild(pre);
    document.body.setAttribute('data-jtml-browser-benchmark', pre.textContent);
    const endpoint = '/__jtml_browser_benchmark_result?fixture=' + encodeURIComponent(fixture);
    const payload = new Blob([pre.textContent], {{ type: 'application/json' }});
    if (!navigator.sendBeacon(endpoint, payload)) {{
      fetch(endpoint, {{ method: 'POST', body: pre.textContent, keepalive: true }}).catch(function() {{}});
    }}
  }}
  async function run() {{
    if (running) return;
    running = true;
    try {{
      if (fixture === 'reference-app') {{
        const started = performance.now();
        const scenario = await window.runReferenceAppChecks();
        const build = window.jtml && window.jtml.browserBuild || {{}};
        finish({{
          fixture, clicks: scenario.clicks, durationMs: performance.now() - started,
          referenceChecks: scenario.checks,
          regression: window.jtmlReferenceRegression,
          browserBuildMode: build.mode,
          browserBuildCleanProduction: build.cleanProduction,
          browserBuildCompatibility: build.compatibility,
          browserBuildDebug: build.debug,
          loadedScripts: Array.from(document.scripts).map(el => el.src)
        }});
        return;
      }}
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
        for (let i = 0; i < fixtureClickCount; i += 1) {{
          input.value = 'Ada ' + i;
          const pos = Math.min(3, input.value.length);
          input.setSelectionRange(pos, pos);
          input.dispatchEvent(new Event('input', {{ bubbles: true }}));
        }}
        const durationMs = performance.now() - started;
        const jtml = window.jtml || {{}};
        finish({{
          fixture,
          clicks: fixtureClickCount,
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
      for (let i = 0; i < fixtureClickCount; i += 1) {{
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
        clicks: fixtureClickCount,
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
        runtimeRegistrySnapshot: jtml.getRuntimeRegistrySnapshot ? jtml.getRuntimeRegistrySnapshot() : null,
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
  function scheduleRun() {{
    setTimeout(run, 50);
  }}
  window.runJtmlBrowserBenchmark = run;
  if (document.readyState === 'loading') {{
    window.addEventListener('DOMContentLoaded', scheduleRun, {{ once: true }});
  }} else {{
    scheduleRun();
  }}
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
  exit_mode_file="${profile}/exit-mode.txt"
  result_file="${RESULT_ROOT}/${fixture}.json"
  rm -f "${result_file}"
  set +e
  bench_url="http://127.0.0.1:${SERVER_PORT}/${fixture}/index.bench.html"
  if [[ -n "${JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT:-}" ]]; then
    node - "${JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT}" "${browser}" "${bench}" "${bench_url}" "${log}" "${dom_file}" "${exit_mode_file}" "${BROWSER_TIMEOUT}" <<'JS'
const fs = require('node:fs');
const path = require('node:path');

  const [, , playwrightRoot, executablePath, benchPath, benchUrl, logPath, domPath, exitModePath, timeoutSeconds] = process.argv;
const { chromium } = require(path.join(playwrightRoot, 'node_modules', 'playwright'));

(async () => {
  const logs = [];
  let browser;
  try {
    browser = await chromium.launch({
      executablePath,
      headless: true,
      args: ['--disable-gpu', '--no-sandbox', '--disable-breakpad', '--disable-background-networking']
    });
    const page = await browser.newPage();
    page.on('console', message => logs.push('[console] ' + message.type() + ': ' + message.text()));
    page.on('pageerror', error => logs.push('[pageerror] ' + error.message));
    const timeout = Number(timeoutSeconds) * 1000;
    const html = fs.readFileSync(benchPath, 'utf8');
    const parsed = await page.evaluate(({ source, baseUrl }) => {
      const document = new DOMParser().parseFromString(source, 'text/html');
      const base = document.createElement('base');
      base.href = baseUrl;
      document.head.prepend(base);
      const scripts = [];
      for (const script of document.querySelectorAll('script')) {
        if (script.type === 'application/json') continue;
        scripts.push({
          src: script.getAttribute('src') || '',
          source: script.textContent || '',
          defer: script.hasAttribute('defer')
        });
        if (script.src) {
          const marker = document.createElement('script');
          marker.type = 'application/x-jtml-loaded';
          marker.setAttribute('src', script.getAttribute('src'));
          script.replaceWith(marker);
        } else {
          script.remove();
        }
      }
      return { html: '<!doctype html>\n' + document.documentElement.outerHTML, scripts };
    }, { source: html, baseUrl: benchUrl });
    const client = await page.context().newCDPSession(page);
    const frameTree = await client.send('Page.getFrameTree');
    await client.send('Page.setDocumentContent', {
      frameId: frameTree.frameTree.frame.id,
      html: parsed.html
    });
    const evaluateScript = async script => {
      const source = script.src
        ? fs.readFileSync(path.resolve(path.dirname(benchPath), script.src), 'utf8')
        : script.source;
      const result = await client.send('Runtime.evaluate', {
        expression: source,
        awaitPromise: true
      });
      if (result.exceptionDetails) {
        const exception = result.exceptionDetails.exception;
        logs.push('[pageerror] ' + (exception && exception.description
          ? exception.description
          : result.exceptionDetails.text || 'browser script evaluation failed'));
        await page.evaluate(message => {
          window.dispatchEvent(new ErrorEvent('error', { message }));
        }, exception && exception.description
          ? exception.description
          : result.exceptionDetails.text || 'browser script evaluation failed');
      }
    };
    for (const script of parsed.scripts.filter(script => !script.defer)) {
      await evaluateScript(script);
    }
    for (const script of parsed.scripts.filter(script => script.defer)) {
      await evaluateScript(script);
    }
    await page.waitForFunction(
      () => typeof window.runJtmlBrowserBenchmark === 'function' && !!window.jtml,
      undefined,
      { timeout }
    );
    await page.evaluate(() => window.runJtmlBrowserBenchmark());
    await page.waitForSelector('#jtml-browser-benchmark-result', { timeout });
    fs.writeFileSync(domPath, await page.content(), 'utf8');
    await browser.close();
    browser = undefined;
    fs.writeFileSync(exitModePath, 'clean', 'utf8');
    fs.writeFileSync(logPath, logs.join('\n'), 'utf8');
  } catch (error) {
    logs.push(error && error.stack ? error.stack : String(error));
    fs.writeFileSync(logPath, logs.join('\n'), 'utf8');
    fs.writeFileSync(exitModePath, 'failed', 'utf8');
    process.exitCode = 1;
  } finally {
    if (browser) await browser.close().catch(() => {});
  }
})();
JS
  else
    python3 - "${browser}" "${bench_url}" "${profile}" "${log}" "${dom_file}" "${exit_mode_file}" "${result_file}" "${BROWSER_TIMEOUT}" "${REQUIRE_CLEAN_EXIT}" <<'PY'
import subprocess
import sys
from pathlib import Path

browser, bench_url, profile, log_path, dom_path, exit_mode_path, result_path, timeout_s, require_clean_exit = sys.argv[1:]

def text(value):
    if value is None:
        return ""
    if isinstance(value, bytes):
        return value.decode("utf-8", errors="replace")
    return str(value)

logs = []
last_stdout = ""
last_status = 1
exit_mode = "failed"
for headless_flag in ("--headless=new", "--headless"):
    args = [
        browser,
        headless_flag,
        "--disable-gpu",
        "--no-sandbox",
        "--disable-breakpad",
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
        with open(dom_path, "w", encoding="utf-8") as dom_handle:
            completed = subprocess.run(
                args,
                stdout=dom_handle,
                stderr=subprocess.PIPE,
                text=True,
                timeout=float(timeout_s),
                check=False,
            )
        logs.append(f"[{headless_flag}] exit={completed.returncode}\n" + (completed.stderr or ""))
        last_stdout = Path(dom_path).read_text(encoding="utf-8")
        last_status = completed.returncode
        if completed.returncode == 0:
            exit_mode = "clean"
            break
    except subprocess.TimeoutExpired as exc:
        logs.append(f"[{headless_flag}] browser benchmark timed out\n" + text(exc.stderr))
        last_stdout = Path(dom_path).read_text(encoding="utf-8")
        if Path(result_path).is_file() or 'jtml-browser-benchmark-result' in last_stdout:
            logs.append(f"[{headless_flag}] benchmark result was produced before timeout; browser did not exit cleanly")
            last_status = 125 if require_clean_exit == "1" else 0
            exit_mode = "terminated-after-result"
            break
        last_status = 124
        exit_mode = "timeout-before-result"

with open(log_path, "w", encoding="utf-8") as handle:
    handle.write("\n".join(logs))
with open(exit_mode_path, "w", encoding="utf-8") as handle:
    handle.write(exit_mode)
raise SystemExit(last_status)
PY
  fi
  status=$?
  set -e
  if [[ ${status} -ne 0 ]]; then
    if [[ ${status} -eq 125 ]]; then
      echo "error: browser benchmark produced a result for ${fixture} but Chrome/Chromium did not exit cleanly; see ${log}" >&2
      exit 1
    fi
    if [[ "${JTML_REQUIRE_BROWSER_BENCH:-0}" == "1" ]]; then
      cat "${log}" >&2
      echo "error: browser benchmark failed for ${fixture}; see ${log}" >&2
      exit 1
    fi
    echo "SKIP: browser runtime benchmark could not launch headless browser for ${fixture}; set JTML_REQUIRE_BROWSER_BENCH=1 to make this fatal."
    exit 0
  fi

  if [[ -f "${result_file}" ]]; then
    result="$(
      python3 - "${result_file}" <<'PY'
import json
import sys

with open(sys.argv[1], "r", encoding="utf-8") as handle:
    payload = handle.read()
json.loads(payload)
print(payload)
PY
    )"
  else
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
    )"
  fi || {
    echo "error: browser benchmark did not produce a runtime result for ${fixture}" >&2
    exit 1
  }

  exit_mode="$(cat "${exit_mode_file}" 2>/dev/null || printf 'unknown')"
  python3 - "${result}" "${fixture}" "${CLICK_COUNT}" "${BUDGET_MS}" "${exit_mode}" <<'PY'
import json
import math
import sys

result = json.loads(sys.argv[1])
fixture = sys.argv[2]
expected_clicks = int(sys.argv[3])
if fixture == "reference-app":
    expected_clicks = {"": 4, "form-submit": 0, "keyed-local-actions": 1, "routing": 2, "nested-heading": 1}[result.get("regression", "")]
if fixture in ("large_keyed_updates", "large_keyed_reorder"):
    expected_clicks += 1
budget_ms = float(sys.argv[4])
exit_mode = sys.argv[5]

if result.get("error"):
    raise SystemExit(f"error: {fixture} browser runtime error: {result['error']}")
if int(result.get("clicks", 0)) != expected_clicks:
    raise SystemExit(f"error: {fixture} only ran {result.get('clicks')} of {expected_clicks} clicks")
duration = float(result.get("durationMs", math.inf))
if not math.isfinite(duration):
    raise SystemExit(f"error: {fixture} duration is not finite")
if duration > budget_ms:
    raise SystemExit(f"error: {fixture} browser runtime {duration:.2f}ms exceeds budget {budget_ms:.2f}ms")
if fixture == "reference-app":
  checks = result.get("referenceChecks", [])
  regression = result.get("regression", "")
  expected_checks = {"": 14, "form-submit": 2, "keyed-local-actions": 2, "routing": 5, "nested-heading": 2}[regression]
  if len(checks) != expected_checks:
    raise SystemExit(f"error: reference-app did not complete its interaction checks: {checks!r}")
  print("Reference app checks: " + "; ".join(checks))
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
if fixture == "large_keyed_updates":
    keys = [str(item.get("key", "")) for item in keyed_texts]
    if len(keys) < 1000:
        raise SystemExit(f"error: large_keyed_updates rendered only {len(keys)} keyed rows")
    if keys[:3] != ["1", "2", "3"]:
        raise SystemExit(f"error: large_keyed_updates should keep stable key order during content updates: {keys[:5]!r}")
    if "Row 101" not in text:
        raise SystemExit(f"error: large_keyed_updates did not leave the updated content visible after odd toggle: {text!r}")
    if not result.get("directComponentListLifecycle"):
        raise SystemExit("error: large_keyed_updates did not report keyed list lifecycle telemetry")
    retained = int(result.get("keyedRetainedCount", 0))
    if retained < 1000:
        raise SystemExit(
            "error: large_keyed_updates recreated keyed row wrappers instead of retaining them "
            f"(retained={retained}, final={result.get('keyedFinalCount')})"
        )
if fixture == "large_keyed_reorder":
    keys = [str(item.get("key", "")) for item in keyed_texts]
    if len(keys) < 1000:
        raise SystemExit(f"error: large_keyed_reorder rendered only {len(keys)} keyed rows")
    if keys[:5] != ["1000", "1", "2", "3", "4"]:
        raise SystemExit(f"error: large_keyed_reorder did not leave the rotated keyed order visible: {keys[:8]!r}")
    if not result.get("directComponentListLifecycle"):
        raise SystemExit("error: large_keyed_reorder did not report keyed list lifecycle telemetry")
    retained = int(result.get("keyedRetainedCount", 0))
    if retained < 1000:
        raise SystemExit(
            "error: large_keyed_reorder recreated keyed row wrappers instead of moving them "
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
    destroyed_ids = destroy.get("destroyedIds") or []
    destroyed_handler_ids = destroy.get("destroyedHandlerIds") or []
    destroyed_entries = destroy.get("entries") or []
    if not destroyed_ids:
        raise SystemExit(f"error: lifecycle_cleanup did not report concrete destroyed instance ids: {destroy!r}")
    if not destroyed_entries:
        raise SystemExit(f"error: lifecycle_cleanup did not report generation-aware destroyed entries: {destroy!r}")
    if int(destroy.get("registryDeltaTotal", 0)) <= 0:
        raise SystemExit(f"error: lifecycle_cleanup did not prove registry size decreased during cleanup: {destroy!r}")
    if "lastHandlerCount" not in destroy:
        raise SystemExit(f"error: lifecycle_cleanup destroy telemetry did not include handler cleanup context: {destroy!r}")
    if int(destroy.get("handlerInstanceDestroyCount", 0)) <= 0 or int(destroy.get("handlerCountTotal", 0)) <= 0:
        raise SystemExit(f"error: lifecycle_cleanup did not prove event-handler registry cleanup: {destroy!r}")
    if not destroyed_handler_ids:
        raise SystemExit(f"error: lifecycle_cleanup did not report handler-owning destroyed instance ids: {destroy!r}")
    if destroy.get("registryLeaks"):
        raise SystemExit(f"error: lifecycle_cleanup left destroyed ids registered at destroy time: {destroy!r}")
    if destroy.get("scopeLeaks"):
        raise SystemExit(f"error: lifecycle_cleanup left destroyed scopes registered at destroy time: {destroy!r}")
    if destroy.get("handlerLeaks"):
        raise SystemExit(f"error: lifecycle_cleanup left destroyed handler owners registered at destroy time: {destroy!r}")
    if destroy.get("dispatchLeaks"):
        raise SystemExit(f"error: lifecycle_cleanup allowed action dispatch to destroyed ids: {destroy!r}")
    registry = result.get("runtimeRegistrySnapshot") or {}
    current_generations = registry.get("dynamicGenerations") or {}
    handler_owner_ids = set(str(item) for item in registry.get("handlerOwnerIds") or [])
    for entry in destroyed_entries:
        entry_id = str(entry.get("id", ""))
        if not entry_id:
            raise SystemExit(f"error: lifecycle_cleanup reported a destroy entry without id: {destroy!r}")
        if not entry.get("r"):
            raise SystemExit(f"error: lifecycle_cleanup destroy entry did not prove registry removal: {entry!r}")
        if not entry.get("s"):
            raise SystemExit(f"error: lifecycle_cleanup destroy entry did not prove scope removal: {entry!r}")
        if int(entry.get("h", 0)) > 0 and not entry.get("e"):
            raise SystemExit(f"error: lifecycle_cleanup destroy entry did not prove handler removal: {entry!r}")
        if not entry.get("d"):
            raise SystemExit(f"error: lifecycle_cleanup destroy entry did not prove dispatch rejection: {entry!r}")
        if entry_id in current_generations:
            current_generation = int(current_generations.get(entry_id, 0))
            destroyed_generation = int(entry.get("g", 0))
            if current_generation <= destroyed_generation:
                raise SystemExit(
                    "error: lifecycle_cleanup remounted a destroyed id without a newer generation: "
                    f"id={entry_id!r} destroyed={destroyed_generation} current={current_generation} registry={registry!r}"
                )
        if int(entry.get("h", 0)) > 0 and entry_id in handler_owner_ids:
            current_generation = int(current_generations.get(entry_id, 0))
            destroyed_generation = int(entry.get("g", 0))
            if current_generation <= destroyed_generation:
                raise SystemExit(
                    "error: lifecycle_cleanup destroyed handler owner is still registered without remount generation: "
                    f"id={entry_id!r} entry={entry!r} registry={registry!r}"
                )
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
    f"keyedLifecycle={str(bool(result.get('directComponentListLifecycle'))).lower()} "
    f"browserExit={exit_mode}"
)
PY
done

echo "JTML browser runtime benchmark passed."
