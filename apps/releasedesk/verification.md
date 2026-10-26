# ReleaseDesk verification

## Root-conditional component lifecycle — 2026-10-26

Baseline: merged PR #46 main `26eb51c638507b54c6632ccadb079635f92b668c`.
Local host: macOS 14.4.1 arm64, Chromium 153.0.8010.12, Playwright 1.63.0.

| Check | Result |
| --- | --- |
| Public-registry review negative | On `c3e10cf`, the strengthened test expects `["Child_1"]` after mount but receives `[]`; reproduced before the registry correction |
| Independent baseline negative | Unchanged probe fails at first `Child: 0` on main; definition/instance ownership remains present in actual manifest |
| Focused compiler regressions | PASS, 130/130; added client-plan test proves local state/action ownership remains separate from global state/actions |
| Unchanged reproduction in actual strict-CSP browser output | PASS, initially absent, four show/hide cycles, initial 0 → two clicks → 2, fresh state each remount, increasing generations, empty registry after hide |
| Independent siblings/nested/keyed fixture | PASS, initially true conditional mounts, three additional cycles, sibling state/DOM isolation, keyed reorder retains draft/DOM, keyed removal/remount resets removed child, callback executes once |
| Public introspection | Exact nonempty source-derived counts and DOM identities; all three public lists and lookup share objects/runtime metadata; retained keyed/sibling identities, fresh remount objects, complete removal; four injected keyed failures restore ten public objects, ordering and metadata |
| Ownership cleanup | Exact live DOM/registry correspondence; hidden declarations absent from active registry; detached controls cannot resurrect instances; no registry/scope/handler/dispatch leaks or patch/create fallbacks |
| Root arguments / generated handlers | Parameter update retains local state; conditional controls and growing plain root loop dispatch once; no inline scripts/handlers, eval, new Function, compatibility fallback or CSP/console errors |
| Unsupported direct root-loop component | Rejected before runnable assets at authored `root-loop-rejected.jtml:10:7`, naming component Child and remediation to component-owned keyed loops |
| Existing Counter/CSP/ReleaseDesk | PASS unchanged suites, including API/SQLite create/persist/reload/failure/recovery and strict script CSP |
| Full local gate | PASS once, exit 0; 476/476 CTests, nine required benchmarks with clean exit, existing production/Taskboard/provenance/conformance/nested/keyed/rollback/security/async/literal checks, packages/install/LSP/editor/site and live smoke 8/8; no verification checks skipped |
| Budgets | Runtime 271,961 / 272,000 bytes (+106 versus main; 39 bytes headroom); app.js unchanged 2,043 / 20,000; no threshold changes |
| Remaining boundaries | Direct root-loop component ownership rejects; implicit component `into` without explicit `value` exposed an unrelated patch fallback; root IDs/select, five native renderer observations and reference HTML 99,939 / 70,000 remain limitations |

See [the traced cause and exact commands](../../docs/architecture/root-conditional-mount.md).
The original probe, previous acceptance fixtures, ReleaseDesk product source and
v0.3 release workflows are unchanged. Hosted exact-head CI is separate PR evidence.
No release was tagged/published or PR merged by this work.

## Historical PR #46 strict script CSP correction — 2026-10-26

Starting point: merged PR #45 main
`32bb777a307484f497c550a8158c9b0390dce2ac`. macOS 14.4.1 arm64, Chromium
153.0.8010.12, Playwright 1.63.0. Hosted CI remains separate PR evidence.

| Check | Result |
| --- | --- |
| Original root-controls probe on main | Reproduced: strict script CSP blocks Increment; Clicks stays 0 |
| Focused compiler regressions | PASS, 129/129 Friendly/attribute tests; native/live dispatch assertions retained |
| Strict HTTP CSP / real Chromium | PASS: original probe, unchanged Counter Add/Reset and independent root/dynamic/component controls |
| Root transport semantics | PASS: quoted arguments, input/change/checkbox, keyboard modifiers/keyup values, focus/blur, double click, hover/scroll, bubbling order, native validation and submit without navigation |
| Dynamic ownership | PASS: root conditional/loop controls, sibling component isolation, component-owned conditional child remount twice |
| Independent negative controls | Inline script and inline handler blocked; both expected CSP violations observed |
| Generated assets | Four clean assets; no inline executable scripts/handlers, eval, new Function or compatibility fallback; inert JSON data block retained |
| ReleaseDesk HTTP/SQLite | PASS: all five API tests and existing create/persist/full reload, real failure/retry/restart assertions under strict script CSP; no unexpected console/CSP errors |
| Shared runtime / app bootstrap | 271,855 / 272,000 bytes unchanged; app.js 2,043 / 20,000 bytes, previously 619 |
| Full local gate | PASS, exit 0; 475/475 CTests, nine mandatory browser benchmarks with clean exit, production reference/Taskboard/provenance/conformance/lifecycle/security/async/literals, packaging/install/LSP/editor/site and live smoke 8/8; no verification checks skipped |
| Remaining defects | Five native renderer observations and reference HTML 99,939 / 70,000 bytes remain known failures; root IDs, programmatic select property and root-conditional component initialization remain limitations |

Commands, transport review and bounded CSP audit are recorded in
[the compiler report](../../docs/architecture/browser-root-csp.md). The server
now removes `unsafe-inline` from **script** CSP; inline styles remain permitted
separately. No ReleaseDesk product source, accepted fixture or budget was changed.
The final review tightened the unit assertion to inspect the root wiring emitter
specifically; the focused compiler suite was rerun. Executable code is identical
to the full-gate candidate. The minimal newly observed component-mount probe reaches dispatch with no CSP
violation and is documented as a limitation, not counted as passing functionality.

## Historical PR #45 application evidence

Starting point: main `fa6d10e13e676eed42f9ac1b0d586c3fdbe7ea93`, fresh isolated
worktree. Local host: macOS 14.4.1 arm64. Hosted PR CI is separate evidence.

| Check | Result |
| --- | --- |
| JTML check/lint | PASS, zero diagnostics |
| Actual `jtml build apps/releasedesk --target browser` | PASS; exactly four production assets; no compatibility assets, eval or new Function |
| Five Python HTTP/SQLite tests | PASS; real sockets/database, field/type/size/origin rejection and genuine database failure |
| Playwright app acceptance | PASS; actual HTTP, compiled JTML and SQLite, no mocked API responses or injected state |
| Loading/empty UI and required fields | PASS; GET/POST gates observe real loading, in-flight form disabled, empty title/description never POST |
| Create/persist/full browser reload | PASS; keyboard submission creates exactly one stored row and does not navigate; full reload reads it back |
| Failure/recovery | PASS; whitespace rejected, SQLite path failure returns 503 with no new row/success, draft retained, database repair + retry works |
| API stopped/restarted | PASS; visible error, same-origin Reload issues recovers; full reload after process restart retains both records |
| Accessible controls | PASS; visible implicit labels, native required select, keyboard submit, desktop/mobile viewport without horizontal overflow |
| Browser console/network | No unexpected console/page errors, WebSockets or JTML live-event calls; intentional 400/503/refused connection errors confined to failure phases |
| Two compiler regression tests | PASS; lazy initial state and root form navigation guard; eager fetches/native required validation retained |
| Remaining-gap probes | Reproduced independently: ignored explicit ID, programmatic root select mismatch, strict CSP blocks generated root inline events |
| Full local gate | PASS, exit 0; 473/473 CTests, all nine clean-exit browser benchmarks, reference/Taskboard/provenance/conformance/lifecycle/security, async/literals, package/refactor/LSP/editor/site/install checks and live smoke 8/8 |

Commands (from the repository root, with real browser paths configured as in the
app README):

```sh
ctest --test-dir build -R 'LazyFetchStartsIdle|RootSubmitAction' --output-on-failure
scripts/test_releasedesk.sh
JTML_REQUIRE_BROWSER_BENCH=1 JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1 scripts/verify_all.sh
```

The Playwright screenshot is generated at `dist/releasedesk-e2e.png`; mobile at
`dist/releasedesk-mobile.png`. The test's sample issues exist only in a temporary
test database, which is deleted afterward. No sample issues are seeded into the
application database.

The gate retains five known native renderer discrepancies and the reference HTML
size failure (99,950 / 70,000 bytes; +72 bytes for emitted submit guards). These
are **not** claimed as fixed. Shared-runtime budgets and all prior oracles remain
unchanged. The app permits generated inline handlers and does not claim strict
script CSP, universal native parity, exactly-once network retries, or public
production readiness. See gaps.md for minimal reproductions and scope.

Hosted macOS initially exceeded the test harness's five-second Python readiness
wait before any browser interaction. The harness now waits for the same actual
listening-port announcement for at most 60 seconds and reports startup latency;
all interaction/persistence assertions and runtime budgets are unchanged. The
full local gate above predates this test-only readiness adjustment; the complete
ReleaseDesk test is rerun for the final harness.
The final harness also passed all browser acceptance with an independently delayed
Python launcher (6.6s initial and 6.1s restart), which exceeded the old readiness
window. No backend/API response, UI state or persistence assertion was mocked.
