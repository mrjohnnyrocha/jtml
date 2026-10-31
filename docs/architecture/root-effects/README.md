# Root scalar effect execution

This implements the first bounded clean-browser effect subset proposed by
[the earlier investigation](../friendly-effects/README.md). That investigation
remains historical evidence, not the status of this implementation.

Baseline: main `138d8f729b0363f8f06f28d9a177dadce7ddf509`, after PR #61.
The original examples, inventory audit and ownership fixture are unchanged.
Acceptance is recorded below only after the complete gate and final-commit CI.

## Supported contract

- Explicit module/root mutable scalar `let` dependencies (number, string, boolean).
  Registration is non-eager and occurs once per application bootstrap, never on
  rendering. Same-value assignments count as writes.
- Existing root action dispatch is the notification boundary. Multiple writes
  coalesce; callbacks see the final action state after parameter restoration.
  Unrelated writes and parameter-shadow writes do not trigger a subscription.
- Existing Expression IR and statement execution handle owned scalar assignments,
  scalar `show` and nested `if`/`else`. No parallel interpreter is added.
- Callback writes can trigger downstream effects. A conservative static write /
  dependency DAG defines one finite pass. Stable topological ordering preserves
  source order between ready callbacks; downstream effects observe upstream
  writes. Each subscription runs at most once per dispatch. The compiler rejects
  all potential cycles, including conditionally unreachable cycles.
- At most 64 subscriptions and 1024 statements per callback. Dynamic unsubscribe,
  nested registration, component-local effects, collection/member dependencies,
  derived/borrowed dependencies, callback arguments, loops, calls and async bodies
  fail closed with source diagnostics. Root imperative writes/calls in an effect
  program must move into actions. Known non-scalar dependency writers fail at
  compile time; unknown argument values have a runtime scalar check.
- Imported module subscriptions retain real module ownership and original source
  spans. Ambiguous colliding module state/action names fail closed instead of
  binding a callback to another module's state. Aliased/borrowed watches are not
  part of this subset. Components gain no effect lifecycle support.

The typed relation flows from `SubscribeStatementNode` into
`SemanticProgram.subscriptions`, then owned `RuntimePlanSubscription` records,
then a closed client table containing only module ID, callback and dependency.
The canonical project table replaces the bootstrap table once. Compiler source
spans and write sets stay out of the public clean manifest. Existing live/native
subscriptions remain on their backend; the new browser flush is browser-local.

## Before and after

Run with a real Chromium executable and Playwright module root:

```sh
node tests/browser/module_ownership_inventory.cjs
node tests/browser/root_effects.cjs
ctest --test-dir build -R 'RootScalarEffects|DeferredFriendlyMinimalSourcesBuildClean|BannerSynchronization' --output-on-failure
JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1 scripts/verify_all.sh
```

[Before](before.json) preserves exact unchanged-main build commands and browser
observations (audit exit 1). [After](after.json) preserves the same audit and its
remaining failures (exit 1). Every row builds with exit 0 before and after.

| Source | Before Chromium | After Chromium |
| --- | --- | --- |
| examples/friendly_effect.jtml | Count updates; Last stale | Last updates on dependency write |
| studio/samples/effects.jtml | Count updates; Last/Direction stale | Last and Direction update |
| examples/friendly_redirect.jtml | Redirect passes | Passes |
| studio/samples/redirect.jtml | Redirect passes | Passes |
| examples/friendly_fetch_invalidate.jtml | Fetch/invalidation passes | Passes |
| studio/samples/fetch.jtml | Fetch/invalidation passes | Passes |
| examples/friendly_route_guards.jtml | Guard passes | Passes |
| studio/samples/routes.jtml | Route passes | Passes |
| demos/index.jtml | Command content / sign-in fail | Same two failures retained |

The new browser regression additionally exercises initial state, same-value and
repeated writes, batching/final visibility, unrelated writes, parameter shadowing,
conditional writes, chained callbacks, module isolation, source ordering, scalar
show and 200 further UI dispatches under strict script CSP. It never injects
JTML state or invokes an effect callback directly. Compiler negatives cover
unsupported bodies/owners/cycles, duplicate callbacks, bounded tables, dynamic
unsubscribe and corrupt closed-schema records.

The earlier CTest failures were investigated separately. The ownership fixture's
`effect count` / `show count` exposed a real validator regression: scalar show is
now retained in the Runtime Plan and executed using the existing expression
executor. The fixture and original assertion were preserved. Startup failed at
Python's initial `http.bind(('127.0.0.1', 0))` with `PermissionError: [Errno 1]
Operation not permitted`, before starting JTML. An independent sandboxed run
reproduced it; an independently permitted run passed serve/watch/studio checks.
No startup test, server code or assertion was changed.

## Runtime budget proposal

[Measured assets](assets.json) use deterministic Python `gzip.compress(mtime=0)`.
The shared asset grows from **271,990 raw / 50,526 gzip** to **272,877 raw /
50,828 gzip**: **+887 raw (+0.326%) / +302 gzip (+0.598%)**. This includes scalar
show support. The intermediate pre-show asset was 272,718 raw / 50,791 gzip.

A demonstrated duplicate component emit-contract normalization was extracted into
one helper, retaining fresh dictionary copies at both call sites. No whitespace
minification, second reactive engine or semantic compromise was used to fit.
The proposal raises only the shared runtime ceiling from **272,000 to 273,000**
(+1,000 bytes, 0.368%; 123 bytes headroom). Mirrored assertions use the same cap;
all other asset and browser timing budgets remain unchanged. This buys actual
root subscription execution, bounded chains and same-value write semantics in
both original examples. The increase is a reviewable performance tradeoff, not
permission to hide an unexplained failing size check.

Browser startup/interaction measurements and complete acceptance results are
recorded in the accompanying verification evidence. Passing measurements are
budget evidence, not a statistical claim of zero performance regression.

## Remaining limitations

The inventory still reports missing Enterprise command-page composition and
sign-in navigation failure. OpsPulse interactions, broader effect semantics,
component lifecycle subscriptions, fetch behavior and the pre-existing production
reference HTML size overage are unchanged. No release artifacts or website are
updated. GO applies only to the supported root scalar contract after acceptance;
NO-GO for claiming full production functionality of all nine inventory sources.

## Local acceptance

The [complete gate log](gate.json) records exit **0**, **492/492 CTests**, all
nine required Chromium benchmarks with `browserExit=clean`, strict CSP/security,
route/redirect/component regressions, package/LSP/editor checks and all eight
live runtime smoke checks. [Verification](verification.json) records the exact
command and exits; [performance](performance.json) compares same-browser baseline
and implementation timings. The existing reference HTML overage is still printed,
and conformance UNKNOWN/excluded cases remain explicit. CI on the final pushed
commit is a separate acceptance requirement, recorded on the PR.
