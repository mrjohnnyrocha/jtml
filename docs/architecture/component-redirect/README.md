# Clean component-button redirects

Baseline: unchanged main `82d6c75a9d10b70d85e88e25975f87e087f6b2c6`, after PR #59.
Dedicated worktree/branch: `component-redirect` / `codex/component-button-redirect`.
Neither original sample is edited.

## Independent reproduction and boundary

`tests/browser/component_redirect.cjs` compiles the two original redirect samples
and a minimal comparison fixture, serves the real production assets with
`script-src 'self'`, and clicks native buttons in Chromium. Its process exits 1
on any failed assertion. The [baseline ledger](before.json) retains build argv,
exit codes, button HTML, failed navigation assertions and Chromium precise-coverage
call counts. Both original samples build successfully but fail navigation.
The root-button control passes; component-global and component-local controls fail.

The trace establishes the boundary rather than inferring it from the earlier PR:

1. Generated button metadata contains a valid component ID, original action name
   and JSON arguments. Chromium calls `runDirectComponentAction` once. Thus the
   browser event listener and instance lookup are reached.
2. In both original samples, the target `when` is global. It is absent from the
   component's local action catalog. Dispatch falls through to
   `runDirectComponentFallbackAction` and then `executeClientAction` (all observed
   in coverage). Registration is sufficient to reach this boundary.
3. Lowered DOM includes the matching redirect metadata and target. Root
   `window.sendEvent` consults its redirect binding before ordinary action execution;
   the root control reaches router navigation. Component fallback instead calls
   ordinary action execution directly. That executes the action's non-redirect
   Runtime Plan body, which has no redirect operation. It never invokes navigation.
4. Local actions have an additional boundary: their retained body plan contains
   the authored `redirect` with its compiler-produced literal word plan, but the
   component statement executor has no redirect case. It records an unsupported
   statement and falls back instead of completing within the owning scope.

## Bounded correction

Separate redirect paths from fetch refresh callbacks and use the same redirect-only
interceptor for root transport dispatch and the already-resolved component-global
fallback. Local actions remain first in resolution order. Execute a local literal
redirect from its compiler-owned body node, retaining normal statement order,
argument restoration and writes in the owning component. Forward existing event
arguments when a component falls back to an ordinary global action.

Redirects continue through the existing `window.__jtml_redirect` and router;
hash history and route guards are unchanged. The redirect lookup has no object
prototype. The component fallback is called only behind existing browser-local
call-site guards. No fetch action interception is added to component fallback.
No source scanner, generated owner, router redesign, runtime eval or compatibility
asset is introduced. The existing Friendly quoted-path syntax remains the contract.

## Browser acceptance

[After evidence](after.json) records the same original sources and the expanded
independent oracle. The required regression runs in both the full local gate and
Ubuntu/macOS CI. It checks:

- Both unchanged sample buttons navigate to `/dashboard`.
- Root and component callers of global redirect actions navigate.
- Two sibling local actions with the same name navigate and retain independent
  state; an identically named global action targeting `/wrong` does not capture them.
- A local action receives argument 2 and changes its own counter exactly once;
  back navigation restores the home route and its counter while the sibling stays 0.
- Local redirects to a protected path go through the existing guard to `/login`.
- Ordinary component-to-global fallback forwards its argument.
- Strict script CSP, clean production mode, no compatibility asset, no page errors,
  and no unsupported local statement fallback for supported local redirects.

```sh
# Existing repository Playwright/Chromium environment variables are required.
node tests/browser/component_redirect.cjs
JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1 scripts/verify_all.sh
```

Final full-gate exit, nine clean-exit benchmarks, unchanged asset budgets and
Ubuntu/macOS results on the pushed commit are recorded in the PR acceptance body.
Compilation alone is not acceptance. Asset headroom remains very small and is
reported by the unchanged benchmark gate.

## Remaining failures

The unchanged `tests/browser/module_ownership_inventory.cjs` remains an audit
that exits 1 while independent runtime failures remain. Root effect subscriptions
still do not execute in either effect sample. Enterprise demo composition and
sign-in remain broken; its global component bindings and external service
requirements remain limitations. OpsPulse interaction failures, five known native
renderer discrepancies and the reference HTML size overage are not repaired.
Only the two redirect-sample failures are retired by this correction; the PR does
not claim general production readiness or complete application acceptance.
