# Browser route declaration precedence

Baseline: clean main `e58fff7a2b4c72a2591f8cc16e83c286dc0772e3` (PR #57).
Both OpsPulse application sources and `tests/browser/application_inventory.cjs`
are unchanged. Fresh local compiler, Chromium headless-shell 153.0.8010.12.

## Cause and ownership

This is a compiler defect, not incorrect OpsPulse route declarations.

1. `expandComponentLinesImpl` in `src/friendly.cpp` visits source lines in order
   and emits routed sections in place. The parser retains those AST node positions.
2. `collectFromNodes` in `src/semantic.cpp` visits AST nodes in order, but
   `SemanticSets::routeRecords` was a `std::set<tuple<path, component, params, loads>>`.
   Insertion silently sorted executable records lexicographically by path.
   This is the first incorrect transformation.
3. `buildRuntimePlan` copies `semantic.routeRecords` into `plan.routes` unchanged.
4. `runtimePlanRoutesToJson` appends in vector order; `emitClientManifestScript`
   serializes that client JSON. Neither stage introduces the reorder.
5. Browser `collectRouteBindings` uses manifest order and finds the associated
   DOM section by path/name. `applyRoutes` intentionally stops matching after
   the first match; `matchRouteParams('*', path)` always matches. Thus every
   declared path selected the first, wildcard record.

The original standalone fixture emitted DOM sections in order
`/`, `/user/new`, `/user/:id`, `*`, but its client manifest contained
`*`, `/`, `/user/:id`, `/user/new`. The original browser selected `*` at `/`.
A fallback-only runtime patch would leave the exact `/user/new` route incorrectly
shadowed by `/user/:id` and repair symptoms downstream of the ownership defect.

The fix uses an ordered vector and appends route records during AST traversal.
The diagnostic string summary `semantic.routes` remains sorted; it is not the
execution table. No runtime matcher, application source, API, release asset,
module-ownership logic, or fallback policy changes. Declaration order defines
first-match precedence, including an intentionally early wildcard; this is not
an automatic specificity ranking. Exact duplicate records remain deduplicated at their first occurrence,
preserving the prior record-identity behavior without sorting the execution table.
The regression includes two identical `/user/new` declarations; bypassing
deduplication fails with five records instead of four (negative control verified).

## Reproduction and regression

```sh
cmake -S . -B build -DJTML_BUILD_TESTS=ON -DJTML_BUILD_PYTHON=OFF
cmake --build build --parallel
# Use the repository's Playwright and Chromium environment variables.
node tests/browser/route_precedence.cjs
node tests/browser/application_inventory.cjs
ctest --test-dir build -R 'Route|route' --output-on-failure
JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1 scripts/verify_all.sh
```

`tests/fixtures/production/route-precedence/index.jtml` is standalone and needs
no backend. The real-browser regression fails against original main with
`'*' !== '/'` ([negative log](regression-before.txt)); the compiler regression
also fails at Semantic IR, Runtime Plan and client JSON ([negative log](unit-before.txt)).
After the fix all 15 route-focused CTests pass and the browser regression passes.
The browser regression is required in the full local gate and Ubuntu/macOS CI.

Coverage: direct document loads at `/` and hash deep links, exact-before-parameter
precedence, decoded parameters, wildcard fallback, UI links, hash changes, public
`jtml.navigate` (including same-path calls), independent page counters, retained
route DOM identity, and fresh conditional child remount without resetting its
sibling. Routes currently retain hidden pages; the test explicitly preserves that
lifecycle contract. Direct navigation means loading the hash-router URL; adding
pathname/history routing or an HTTP SPA rewrite is outside this correction.

## Every unchanged inventory check

Both runs exit 1: before **5/32 pass, 27 fail**; after **22/32 pass, 10 fail**.
All 14 route checks and all five canonical navigation checks pass after the fix.
Raw observations, headings, route tables, resource failures and exception details
are preserved in [before](inventory-before.json) and [after](inventory-after.json).

| Check | Before | After |
| --- | --- | --- |
| opspulse route / | FAIL | PASS |
| opspulse route /usage | FAIL | PASS |
| opspulse route /people | FAIL | PASS |
| opspulse route /incidents | FAIL | PASS |
| opspulse route /settings | FAIL | PASS |
| opspulse route /missing | PASS | PASS |
| opspulse navigation Usage | FAIL | PASS |
| opspulse navigation People | FAIL | PASS |
| opspulse navigation Incidents | FAIL | PASS |
| opspulse navigation Settings | FAIL | PASS |
| opspulse navigation Dashboard | FAIL | PASS |
| opspulse state Theme | FAIL | FAIL |
| opspulse state Portugal | FAIL | FAIL |
| opspulse no page errors | PASS | PASS |
| opspulse no failed resources | PASS | PASS |
| reference route / | FAIL | PASS |
| reference route /usage | FAIL | PASS |
| reference route /people | FAIL | PASS |
| reference route /incidents | FAIL | PASS |
| reference route /releases | FAIL | PASS |
| reference route /media | FAIL | PASS |
| reference route /settings | FAIL | PASS |
| reference route /missing | PASS | PASS |
| reference navigation Usage | FAIL | FAIL |
| reference navigation People | FAIL | FAIL |
| reference navigation Incidents | FAIL | FAIL |
| reference navigation Settings | FAIL | FAIL |
| reference navigation Dashboard | FAIL | FAIL |
| reference state Theme | FAIL | FAIL |
| reference state Portugal | FAIL | FAIL |
| reference no page errors | PASS | PASS |
| reference no failed resources | FAIL | FAIL |

## Remaining limitations

- Canonical Theme and Portugal controls still produce no observed class/text
  change. Their store/action/binding behavior needs separate investigation; this
  evidence does not establish whether the remaining cause is source or runtime.
- The historical reference's five navigation links and Theme button are not
  visible to the unchanged probe on the Summary page. Direct route rendering
  works, but the surrounding shell remains absent. Its Portugal click produces
  no observed text change. No shell, slot or application features were added.
- The reference still requests missing `/api/usage`, `/api/users`,
  `/api/incidents`, `/api/releases`, `/api/media`, and unresolved `media.data.*`
  resources. Static serving supplies no application API. Failures remain failures.
- The seven unfinished modular pages, missing exports/state/arguments, nine
  independent module-ownership clean-build failures, five known native renderer
  discrepancies and reference HTML-size overage remain outside this fix.
- OpsPulse is still not a fully functional production application. Successful
  route selection does not establish backend, persistence, or broad browser/live
  parity. This change does not authorize deployment, release, or merge.

Full-gate and final-head hosted CI results are recorded in the focused PR.
