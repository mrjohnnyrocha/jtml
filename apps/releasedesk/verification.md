# ReleaseDesk verification

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
