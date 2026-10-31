# Friendly effects: execution gap and budget decision

**NO-GO for a production effect correction in this PR.** This is an investigation
and concrete proposal, not a fix. The requested budget stop condition applies:
no responsibly verified implementation fits the available ten bytes. This does
not prove that every possible compiler/runtime design is too large. No compiler,
runtime, schema, example, budget, assertion or gate has been changed.

Baseline: clean main `da688a5bf9f07cab1aee239975e04b21e325b945`, after PR #60.
Branch: `codex/friendly-effect-investigation`. Existing worktree reused after the
redirect PR merged. Required restart/architecture documents were read before work.

## Reproduction, unchanged sources

```sh
cmake --build build --parallel 6
# Existing Playwright and real Chromium environment variables are required:
node tests/browser/module_ownership_inventory.cjs
ctest --test-dir build -R 'FriendlyEffect|EffectLowersToSubscription' --output-on-failure
./build/jtml explain examples/friendly_effect.jtml --json
./build/jtml interpret docs/architecture/friendly-effects/live-probe.jtml
```

[Browser evidence](browser-before.json) records exact build commands, process
results and UI observations in Chromium headless-shell 153.0.8010.12 under strict
script CSP. The inventory process exits **1**; both effects compile with exit 0.
The two pre-existing interpreter/lowering CTests pass (2/2). No mocked JTML state,
effect callbacks or successful assertions have been substituted into the audit.

| Source | Build | After Increment | Expected | Decision |
| --- | --- | --- | --- | --- |
| `examples/friendly_effect.jtml` | 0 | Count 1; Last remains `No changes yet.` | Last `Count changed to 1` | FAIL retained |
| `studio/samples/effects.jtml` | 0 | Count 1; Direction `idle`; Last unchanged | Last `Count changed to 1`; Direction `positive` | FAIL retained |

There is no fixed "after" cohort: production sources are unchanged, so these
observed browser failures remain. The Enterprise demo's page-content/sign-in
checks also fail. Redirect checks now pass after PR #60. OpsPulse interactions,
five known native renderer discrepancies and the reference HTML size overage
remain outside this investigation.

## Exact compiler/execution trace

1. `friendlyToClassicWithSourceMap` in `src/friendly.cpp` emits a zero-argument
   function for the effect body and a deferred `subscribe <function> to <variable>`.
   PR #59 correctly preserved the authored attribution of that deferred node.
2. `Parser::parseSubscribeStatement` produces `SubscribeStatementNode` with
   `functionName` and `variableName`. The original AST contains the subscription;
   clean HTML even retains a nonexecuting diagnostic comment for this node.
3. `src/semantic.cpp` recognizes generated function names via
   `effectVariableFromFunction`, collecting `effects` and a `subscribes` usage
   edge. This is summary/usage information, not an executable subscription record
   retaining callback identity, binding owner, source span and lifecycle policy.
4. `buildRuntimePlan(program, semantic)` handles top-level Define, Derive and
   Function nodes. It **does not handle SubscribeStatement**. The generated effect
   function is retained as an ordinary RuntimePlanAction, but its executable
   dependency-to-callback relation is lost. The per-module plan takes the same path.
5. `runtimePlanToClientJson` / `emitClientManifestScript` serialize that action,
   state and derived plans. There is no subscription table. The closed public
   manifest schema likewise allows no subscription field. [Compiler observations](compiler-observations.json)
   preserve Semantic IR and Runtime Plan from the supported `explain --json` API.
6. Browser bootstrap registers the function in `clientActions`; root mutations
   execute `runClientStatements`, then `applyClientState`. Neither consumes the
   subscription. `applyClientDerived` evaluates pure derived values; it is not an
   effect scheduler. Merely exporting the function or calling it on every render
   cannot implement dependency-triggered subscription semantics.
7. Components retain `localEffects`, `kind: effect` nodes and read sets in their
   body plan. Browser code copies effect names into inspection metadata, but has
   no activation/notification dispatcher for them. `componentRenderedReadSet`,
   `entriesByRead` and `changes.writes` drive rendering/patch selection, not
   side-effect execution. Destroy/generation machinery exists but currently has
   no subscription queue to cancel.

## Live versus browser semantics actually established

`Interpreter::interpretSubscribe` resolves the callback in its lexical environment
and resolves the dependency VarID. It registers with
`Environment::subscribeFunctionToVariable`. That method deduplicates by
VarID/function name and **does not invoke the callback initially**. Explicit
unsubscribe removes its subscription ID. Scalar callbacks support zero or one
argument; Friendly generated functions use zero. Classic arrays/dicts have separate
argument expansion behavior; it is not a proposed browser contract.

`markDirty`, dirty-owner transfer, topological recalculation and
`notifySubscribers` implement notifications. Dirty sets are cleared before
callbacks so callback writes survive for a subsequent recalculation. Observations
in [live-probe.log](live-probe.log), from the checked-in probe:

- Initial subscription: hits 0 (non-eager).
- Assigning count 0 again: hits 1. Value equality alone is not a write detector.
- Subsequent top-level count 1 and count 2 assignments: hits 2 and 3.
- The `twice()` action writes count twice. The subsequent in-program `show`
  still prints hits 3; the final dirty-owner flush runs one callback, taking hits
  to 4 with count 4. Inlining after each assignment would instead run twice and
  change when callbacks become observable. This is not acceptable parity proof.

The existing FriendlyEffect CTest separately proves a live UI action updates the
outer `last` binding after dispatch. These results are not proof of universal
live parity, lifecycle cleanup or cycle safety. Function invocation has a depth
limit of 128, but Environment follow-up dirty recalculation is recursive and does
not establish a safe bounded browser effect pump. Automatic subscription cleanup
on every live component lifecycle is not established here. Browser production has
**none of the subscription execution behaviors above**; component effect metadata
is not evidence of implemented behavior.

## Proposed minimum supported contract (not shipped)

- One explicit scalar dependency: a module-owned or component-owned mutable
  `let`. No implicit tracking of every expression read. Start by rejecting watches
  of borrowed parameters, collections/member paths, fetch objects and derived
  values until their notification rules are independently specified and tested.
- Non-eager registration, including fresh component mounts. Seed output remains
  unchanged until a dependency write; same-value writes still count. Coalesce a
  dependency's writes at the existing action/dirty-scope boundary, not at every
  assignment or every render. Independent dispatches produce independent callbacks.
- Bodies use the existing validated assignment/conditional Expression IR and
  writable-owned-let rules. Initially reject unbounded loops, async/extern work,
  unknown calls and writes to borrowed data with original source/component
  remediation. This subset is not permission to silently drop unsupported bodies.
- A compiler-owned effect record carries module/lexical owner, dependency binding,
  callback/body identity, source span, conservative reads/writes and ordering.
  Build from SubscribeStatement AST or component effect nodes; never decode
  generated names or rescan Friendly text in the CLI. Keep spans compiler-side;
  compact clean client records must omit original paths, text and debug payloads.
- Conservative dependency/write cycles are rejected before clean emission.
  Callback writes settle through a bounded shared dirty-scope frame. Unknown
  ordering or unsupported shapes fail closed. No recursive action/render loop,
  arbitrary iteration-to-convergence, timers, eval or runtime source compilation.
- Root subscriptions live once per app/module. Local subscriptions belong to the
  existing instance ID and generation, not a component name or source line alone.
  Repeated renders/parameter refreshes do not register again. Hidden retained
  routes keep their existing lifetime; conditional removal/keyed destruction
  invalidates queued work before removal. Remount creates a fresh non-eager owner.
  Sibling instances never share mutable callback/dirty state.
- Reuse the existing root/component statement executors, compiled expressions,
  change sets, rendering commit and registry/destroy checks. Instrument every
  supported mutation entry, including input setters, rather than watching DOM
  text or monkey-patching the public JS state object. Arbitrary external mutation
  of `jtml.state` is not newly promised as observable.

Required regression matrix before calling a future implementation complete:
original examples with repeated increment/decrement and unchanged initial text;
same-value writes; multiwrite/coalesced actions; unrelated writes and effect
chains; own versus borrowed bindings; conditional root/component mount/unmount;
two siblings; remount generations; destruction with pending work; no duplicate
registration; cycle/unsupported-shape source diagnostics; strict CSP; all original
module, redirect, route, security and lifecycle regressions.

## Measured size and duplication investigation

Run the measurement against an actual clean runtime; it never edits `src/` or
emitted assets:

```sh
python3 docs/architecture/friendly-effects/measure.py \
  dist/module-ownership-audit/examples/friendly_effect/jtml-runtime.js
```

[Measurements](asset-measurement.json) use raw emitted bytes and deterministic gzip.
The `.js.txt` file is an explicitly **rejected cost probe**, not an implementation
or a claimed lower bound on all designs. `executeEffect` is unresolved. It lacks
write-capture hooks, metadata/schema integration, generation snapshots, stable
ordering and complete cleanup. Its guards do not certify correct effect semantics.
Only its uncommented code bytes are included in the experiment.

| Experiment | Raw runtime bytes | Delta | Against 272000 |
| --- | ---: | ---: | ---: |
| Actual unchanged runtime | 271990 | 0 | 10 spare |
| Append incomplete notification probe | 272604 | +614 | 604 over |
| Offline extract duplicate emit-contract normalization | 271733 | -257 | 267 spare |
| Extraction plus incomplete probe | 272347 | +357 | 347 over |

Actual runtime gzip is 50526 bytes; with the probe it is 50767 (+241). The budget
is raw bytes; compression does not excuse an overage.

The extraction experiment identifies **two identical three-field emitArity /
emitPayloads / emitPayloadTypes normalization blocks** in component scanning.
A helper returning fresh dictionaries could preserve independent copies and save
257 bytes. This is demonstrated source duplication, not cosmetic minification;
it has not been applied or behaviorally certified. It still cannot pay even for
the incomplete cost probe. The duplicated-looking root/component derived and
statement loops differ in representation, fallback evaluation, ownership,
parameter restoration and rendering commit. Deleting one is not a proven safe
saving. The legacy Environment queue field is unused there and is C++, not
removable bytes from the browser runtime.

## Concrete next implementation proposal and stop decision

1. Add typed Semantic/Runtime effect binding records and closed client-schema
   validation with module identity and source spans. Build failing positive and
   negative native/Chromium fixtures before runtime work.
2. Prove a shared scoped execution/dirty-frame adapter using the current root and
   component executors. First measure a semantics-preserving normalization or
   compatibility-chunk extraction that funds the **complete** integration and all
   guards, with real browser/live regressions. Do not bank hypothetical savings.
3. Connect the subscription records to that frame and existing generation-aware
   mount/destroy path. Verify the full matrix above and settle only supported
   acyclic graphs. Re-measure every asset and run all gates on the final source.

Compiler-only static inlining is an alternative worth a separate design proof:
it adds zero shared-runtime bytes in principle. However, a simple per-assignment
expansion contradicts the measured dirty-boundary behavior. Correct branch-sensitive
write tracking, effect-chain ordering, input and module write coverage, and
conditional instance lifetimes still require a compiler/runtime ownership design.
Moving a second scheduler to `app.js` or making effects into eager derived values
would evade the constraint or change semantics; neither is an accepted fix.

**Stop here rather than shipping an unsafe shortcut.** No production runtime was
added. Initial/repeated/lifecycle/cycle browser behavior is not newly supported;
all existing effect failures remain. A green baseline gate/CI validates the
investigation-only tree, not effect execution. Full gate exits and exact CI commit
are recorded in the PR acceptance body.
