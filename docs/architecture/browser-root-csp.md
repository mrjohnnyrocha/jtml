# Browser root events under strict script CSP

Scope: ReleaseDesk issue 3, starting from merged PR #45 main
`32bb777a307484f497c550a8158c9b0390dce2ac`. This is a browser event-transport
correction, not a language or renderer redesign.

## Cause and change

The unchanged `apps/releasedesk/probes/root-controls.jtml` reproduced the original
failure: under the HTTP header `Content-Security-Policy: script-src 'self'`,
Increment left Clicks at 0 and Chromium reported a blocked inline handler.
The compiler emitted `onclick`, `oninput` and `onsubmit`, although the production
runtime and component modules were already external scripts without dynamic
compilation.

Browser-local transpilation now emits `data-jtml-event-<type>` with HTML-escaped
JSON `[binding identifier, action invocation]`. This is data consumed by the
existing action parser/dispatcher, not JavaScript compiled or evaluated from the
attribute. The same compiler-owned emitter installs delegated listeners in the
external `app.js` bootstrap and in the explicitly inline browser-local embedding
API. No extra asset or application-authored frontend JavaScript is needed.

The dispatcher, action argument evaluation, reactive state, fetches and component
runtime remain unchanged. Event recognition now includes Friendly's existing
keydown, focus, blur and double-click names, which were missing from the shared
attribute classifier. Root keydown receives the same key/modifier record shape
as supported component keydown; input/change/keyup retain their value payloads,
and scroll retains its existing window-scroll string payload.

Delegation snapshots the original event path and dispatches target-to-ancestor,
so a reactive action can remove its own DOM without losing ancestor handling.
Focus, blur and scroll use capture because they do not bubble. Submit calls
`preventDefault()` synchronously before action dispatch, while browser constraint
validation still prevents an invalid form from emitting submit. Forms without
a JTML submit handler retain native navigation. Conditional/loop root controls
need no listener registration, per-node cache, or remount cleanup.

The legacy live/native HTML binding protocol remains explicit and unchanged.
Two native store tests now read binding IDs from declarative browser metadata;
their action, argument and state assertions remain intact. The root-submit
regression checks the new transport and cancellation-before-dispatch contract;
real Enter submission with a delayed HTTP POST proves the navigation guard.

## CSP audit and limits

The tested clean builds contain only external executable script references and
the inert `type="application/json"` client manifest. They contain no inline event
attributes, runtime `eval`, `new Function`, or compatibility assets. Generated
component listeners remain separate; static siblings and component-owned dynamic
child remounts retain their event ownership. Shared runtime stays **271,855 /
272,000 bytes**; app bootstrap grows from **619 to 2,043 / 20,000 bytes**.
Existing asset/performance thresholds are unchanged.

ReleaseDesk now serves `script-src 'self'` and passes its full real HTTP/SQLite
workflow under that policy. Its separate style policy still permits inline
styles. This does not establish a strict style policy, arbitrary author-provided
raw HTML or JavaScript-URL safety, extern-host compliance, or strict CSP for
legacy inline live/preview documents.

The root ID/label and programmatic SELECT property limitations remain. A separate
minimal `apps/releasedesk/probes/root-conditional-component.jtml` reproduces a
component introduced by a root conditional without component initialization:
Toggle → Child increment reports `__Child_1_increment` missing and retains a
lowered placeholder, with no CSP violation. The rendering runtime is unchanged;
this mounting shape is not claimed as supported by the event correction.
Component-owned conditional child creation/removal/remount and plain root
conditional/loop controls pass separately. Five catalogued native renderer
observations and the reference HTML size overage remain known defects.

## Reproduce

Use the normal build and the existing Playwright/browser configuration:

```sh
cmake --build build --parallel
ctest --test-dir build -R 'FriendlySyntax|AttributeClassifier' --output-on-failure
node tests/browser/strict_csp.cjs
scripts/test_releasedesk.sh
JTML_REQUIRE_BROWSER_BENCH=1 JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1 scripts/verify_all.sh
```

The browser scripts require `JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT` (directory with
Playwright in `node_modules`) and `JTML_BENCH_BROWSER` (real Chromium executable).
They fail rather than skip when unavailable. CI and the full local gate run the
same new regression; no release workflow or accepted fixture is changed.

`strict_csp.cjs` builds the original root-controls probe, unchanged Friendly Counter
and independent `tests/fixtures/csp/root-events.jtml`. A real loopback HTTP server
sets the restrictive script header. Chromium operates the generated controls:
click/arguments including quoting and backslash, input/change/checkbox values,
key/modifier and keyup value payloads, focus/blur, double-click, hover/scroll,
required form validation, Enter submit, native unhandled form navigation,
bubbling order, conditional and loop controls, component sibling isolation and
component-owned child remounts. Separate intentionally inline script/handler
negatives must be blocked and emit both policy violations. Application errors or
policy violations fail the positive cases. No action adapter, state injection,
mock API response or synthetic DOM is used as interaction evidence.

ReleaseDesk separately verifies actual loading/empty states, delayed HTTP save,
SQLite commit, full reload, validation, real database failure, stopped server,
retry and process restart, with strict header and zero unexpected CSP/console
errors. The temporary test database never touches a user's running app database.

## Verification

Local evidence: macOS 14.4.1 arm64, Chromium 153.0.8010.12, Playwright 1.63.0.
The compact final results are in
[`apps/releasedesk/verification.md`](../../apps/releasedesk/verification.md).
Hosted Ubuntu/macOS checks are recorded separately on the PR, not inferred from
local success.
