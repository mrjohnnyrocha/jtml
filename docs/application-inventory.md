# Existing JTML application portfolio

Initial audit base: public RC.3 source `2cec9263d4a2e5fa859c3703615aeb6f2ed5b668`.
Cleanup rebased onto main `d7cf360cb89c18eeea100ffefff6aefb3c453797` after PR #56;
its RC.3 website/guide changes are retained intact. Commands below use a fresh local `0.3.0-dev` build from that
source, not a relabelled release executable. No new application, compiler change,
release, deployment or deletion is included. Check/lint/build success is separate
from browser acceptance.

## Portfolio and maturity

| Source / actual entrypoint | Classification | Data and persistence | Measured boundary |
| --- | --- | --- | --- |
| `apps/releasedesk/index.jtml` (directory build `apps/releasedesk`) | Real local application; bounded append-only issue workflow | Actual Python HTTP API + SQLite, empty until user creates records; survives browser/server restart. Unsaved draft is transient. No accounts or public-service contract. | Check/lint/clean build pass. Existing API and Chromium persistence/failure/recovery suite is the functional acceptance oracle. |
| `demos/opspulse/index.jtml` (directory build `demos/opspulse`) | **Canonical OpsPulse showcase**, single file | Literal metrics, sample user/role, transient stores; no fetch, database or persistence. Refresh/Save settings buttons have no actions. | Check passes; lint 0 errors/4 advisories; clean build passes. Actual route/navigation/theme/filter acceptance **fails**. Not a functional app. |
| `demos/opspulse/pages`, `components`, `stores` | Preserved unfinished modular exploration | Different literal metrics in `ops-data`; no backend. Not imported by `index.jtml`; no independent route entrypoint. | 10/17 files pass check/lint/build after bounded path/export repairs. Seven pages fail missing exports. A directory build never exercised these modules. |
| `demos/index.jtml` (directory build `demos`) | Enterprise Operations language showcase | Literal incidents/automation; GET users/posts/runtime, POST login, external `host.notify`. Live server supplies synthetic users/posts/login and runtime introspection, not a real business database. | Check passes; lint 0 errors/13 advisories. Explicit clean browser build rejects module ownership. Default gate build is a different mode. |
| `demos/example.jtml` | Preserved denser OpsPulse showcase/reference | Root-level transient state; GET `/api/usage`, `/api/users`, `/api/incidents`, lazy releases/media; illustrative records and simulated actions. No matching production API or persistence. | Check passes; lint 0 errors/10 advisories; clean build passes. Actual route/navigation/theme/filter acceptance **fails**; static serving also exposes missing API/media URLs. |
| `tests/fixtures/production/taskboard-core/index.jtml` | Adversarial compiler/runtime regression fixture | Component-owned literal task records and in-memory edits; reload resets. No SQLite, API or persistent product workflow. | Check/clean build pass; lint 0 errors/1 accessibility advisory. Existing C01–C14, lifecycle, focus/identity and negative suites are the acceptance oracles. Keep all fixtures/oracles. |
| `examples/` (each source file, except helper modules below) | Tutorial/sample and compatibility regression corpus | Usually literal/local state. Fetch/host samples need an external or demo API; illustrative saves/logins are not persisted. | 44 source files: check 44/44, lint 43/44, clean build 40/44. No blanket browser-functional claim. Counter has separate real-browser coverage. |
| `studio/samples/` (13 files in `manifest.json`) | Studio tutorial/sample corpus | Local illustrative state; fetch uses users/teams, POST login, routes lazy-load users. No database. Studio resource/editor persistence is distinct from sample application data. | Check/lint 13/13, clean build 9/13. Parser/linter/resource-distribution tests exist; not 13 browser-accepted applications. |
| `site/index.html`, `site/deploy.html`, `site/security.html`; Counter source `examples/friendly_counter.jtml` | Public static documentation + compiled interactive showcase | Counter is transient local state, no backend. `site-build.json` identifies actual compiler and assets. | Recommend public RC.3 downloads. Keep `.github/workflows/site-pages.yml`'s exact RC.2 compiler/hash pin: the published Counter is still RC.2-built. Local development site builds have their own provenance. |

All 78 inventoried source files and their actual command outcomes are listed in
[the per-file ledger](application-inventory/cli-results.md). Files that only
provide imports are measured for diagnostics, not counted as standalone products:
`examples/modules/card.jtml`, `modules/friendly_message.jtml`,
`imported_keywords.jtml`, and `studio_logic.jtml`. `jtml_studio.jtml` is a UI
showcase importing that logic, not the shipped Studio implementation.
`keywords.jtml` is Classic compatibility coverage; `jtl_core.jtl` is experimental
JTL. The remaining example sources use Friendly JTML 2, including older HTML-style
examples; age alone is not a reason to delete them.

Fetch samples explicitly need `/api/users` (`friendly_fetch`,
`friendly_fetch_invalidate`, `friendly_route_load`), `/api/posts`
(`friendly_fetch_refresh`) or POST `/api/login` (`friendly_fetch_post`).
`friendly_extern` and `friendly_interop` need `host.notify`; media samples use
illustrative data and browser-selected files. `examples/users.json` is sample
input, not a production datastore. Studio's `fetch`, `fetch-post` and `routes`
need API contracts; all other Studio samples are local teaching examples.

## OpsPulse decision and bounded repairs

Retain `demos/opspulse/index.jtml` as the one canonical OpsPulse source because it
is the existing documented/gate directory entrypoint and has no external API
requirement. **Canonical means where to work, not that it passes acceptance.**
The alternative modules and `demos/example.jtml` remain clearly labelled
explorations. They contain different media, releases and API demonstrations, so
browser failure and superficial overlap do not prove redundancy.

Corrected importer-relative paths in Dashboard, Shell and all four UI modules;
added the omitted FilterState import in AppHeader; made existing stores/components
explicit exports for the existing named import consumers. No component body or
state behavior was rewritten. The corrected modules are still not promoted into
the canonical entrypoint. `tests/test_application_paths.py` imports the real
files and checks their transitive graph and named exports. It also separates the
current download recommendation from historical Counter provenance. All three
tests pass; unchanged post-#56 main fails 9 import assertions/subtests. The
initial pre-#56 negative control failed 10, including the then-stale download
recommendation. The green regression script is included in CI and the full gate.

The remaining module defects are application-source problems, not missing
compiler import support:

- All seven non-dashboard pages import nonexistent `PageHero`; Usage/Releases
  additionally import nonexistent `ProgressBar`.
- Settings reads/writes undeclared `appState.workspace` and `settingsSaved`;
  Incidents uses undeclared `selectedIncident` and `draftNote`.
- Those seven pages pass one argument to the two-argument Shell.
- Store-member `into` bindings exceed the documented local writable-let contract.
- Canonical routes are `/`, `/usage`, `/people`, `/incidents`, `/settings`, `*`.
  Releases/Media modules are not registered. The historical reference separately
  declares `/releases` and `/media`; neither is silently added to the canonical app.

No file deletion is justified: `verify_all.sh` consumes both top-level demos and
the OpsPulse directory/asset; CLI demo defaults use the existing demo; modular
files retain unique material. Existing tests, benchmarks, fixtures, release
records and RC.2 evidence are preserved.

## Reproduction and acceptance

```sh
python3 scripts/inventory_applications.py
python3 tests/test_application_paths.py
# Same existing Playwright/browser configuration as the repository browser tests:
node tests/browser/application_inventory.cjs
scripts/test_releasedesk.sh
scripts/test_taskboard_core.sh
scripts/verify_all.sh
```

The inventory runner saves each command and output to
`dist/application-inventory/results.json`; its own exit zero only means collection
completed. The browser probe builds the actual OpsPulse sources in clean mode,
serves over HTTP, waits for the public route state, follows UI links and tries
Theme and Portugal. It does not mock APIs or inject JTML state. It intentionally
returns nonzero while acceptance fails; it is an audit probe, not a weakened
required suite. Output is `dist/application-browser-inventory/results.json` plus
screenshots. Existing ReleaseDesk/Taskboard acceptance stays mandatory in the gate.

For the separate clean-build blocker:

```sh
./build/jtml build demos/index.jtml --target browser --out dist/demo-repro
```

This reports `Cannot determine module ownership`. The same signature affects
`examples/friendly_effect`, `friendly_fetch_invalidate`, `friendly_redirect`,
`friendly_route_guards` and Studio `effects`, `fetch`, `redirect`, `routes`.
These precise existing sources are the reproductions; compiler redesign is
outside this cleanup. Classic `examples/keywords.jtml` separately has one lint
error. See the ledger for diagnostics rather than removing these examples.

## Preserved evidence

The [browser report](application-inventory/browser-results.json) and screenshots
([OpsPulse](application-inventory/opspulse.png),
[historical reference](application-inventory/reference.png)) retain the exact
pre-rebase negative run: Chromium headless-shell 153.0.8010.12, 32 checks, 27
failures, exit 1. The compiler and both probed application sources are unchanged
by PR #56. Failed resources are real HTTP 404s, not mocked responses. The report
records the public route list with wildcard first and NotFoundPage selected at `/`.

The initial full gate using Google Chrome 156.0.8078.48 stopped at the strict-CSP
suite on an unexpected resource 404; the unchanged suite passed with the existing
headless-shell. The earlier headless-shell full log ends with all 480 CTests,
nine clean-exit benchmarks, ReleaseDesk API 5/5 and browser persistence/recovery,
Taskboard C01–C14, site/package/resource checks and live smoke 8/8 passing. Its
process handle expired, so final candidate acceptance is recorded separately in
the cleanup PR with the exact commit and captured exit code. The known five
native-renderer discrepancies and reference HTML size overage remain reported.

## Remaining issues, ranked by user impact

1. **High — OpsPulse is not usable as a clean-browser application.** Its declared
   pages render the Not found surface and navigation/state acceptance fails.
   The public runtime route list places `*` before `/`, and `getCurrentRoute()`
   selects `NotFoundPage` at `/`, despite the source declaring `*` last.
   Investigate route precedence separately with these
   unchanged sources and expected pages; do not hide failure behind a successful build.
2. **High — broad showcases/samples have clean-build blockers.** The nine
   module-ownership failures above need bounded reproductions and compiler-owned
   fixes. `check`/`lint` cannot stand in for clean production execution.
3. **Medium — modular OpsPulse remains incomplete.** Missing definitions, state,
   arguments and unsupported bindings prevent promotion. Finish it only with an
   explicit decision to replace the canonical single file and browser parity proof.
4. **Medium — ReleaseDesk uncertain POST retries can duplicate a committed issue.**
   Existing recovery says reload before retry; no idempotency key exists. It is a
   trusted local append-only tool, not a multi-user production service.
5. **Medium — broader known boundaries remain.** Root IDs/select synchronization,
   general slots, native renderer differences and the unchanged reference HTML
   size overage are not solved here. No blanket browser/live parity is claimed.

Develop **ReleaseDesk** next. The smallest valuable functional increment is a
text/severity filter over its already loaded, real SQLite issue records, with
explicit empty-results and Clear filters controls. Keep saving/reloading intact;
verify matching, no matches, clearing, and persistence/reload in the existing real
API browser suite. This adds useful behavior without a new app, endpoint,
dependency, compiler feature or synthetic data.
