# Implementation gap ledger at the pinned baseline

Baseline: `ae8f984dc3c35163cc702f7e494a5f2b015124fb`. This is evidence and a proposed
work ordering, **not an implementation plan authorized by this PR**. No runtime
correction is included. “Currently incorrect” means a demonstrated discrepancy
against the explicitly proposed Core rule; it does not retroactively declare all
legacy behavior a regression against an already-ratified language contract.
A correctly rejected unsupported shape is never counted as a correctness bug.

## Evidence boundary

[baseline-observations.json](baseline-observations.json) records all 24 builds,
21 accepted programs executed in real Chromium headless shell 151.0.7922.34 under
`script-src 'self'`, and three compile-time rejections. The compiler was built from
`1bf19a1be1665f186aa3b4b6aa441b74a4581681`; `git diff` confirms its source tree is
identical to the requested merge baseline. The binary digest is recorded. Both
browser.close and server.close completed. Source is preserved in
[vectors.json](vectors.json), independently of the compiler output.

Observations are post-load/post-click DOM text, raw runtime storage and immediate
`show` output. **No subscription/commit phase hook, paint sampling, or exhaustive
MutationObserver trace was installed.** Notification count/order for the existing
bounded root subset has separate regression evidence; complete Core atomicity
remains unverified. These are clean-browser observations, not native/live parity
claims. This documentation PR does not rerun or claim the full runtime gate.

Use these four classifications literally:

- **T — Already implemented and tested:** the stated bounded aspect has named
  existing tests and/or a matching executed vector, not a proof of all Core rules.
- **V — Implemented but insufficiently verified:** code/evidence supports part of
  the rule, but complete required phase/identity/lifetime observations are missing.
- **I — Currently incorrect:** an executed counterexample contradicts the draft.
- **P — Not yet supported:** a target capability or proposed restriction is not a
  current supported contract. An explicit fail-closed rejection can be correct.

## Every normative rule, with current evidence

| Rule/aspect | Class | Current evidence and limit |
| --- | --- | --- |
| R1 scalar literal/arithmetic/conditional direct expressions | T | `tests/test_interpreter_unary.cpp`, `tests/conformance/cases.cjs`, `tests/browser/ternary_dependencies.cjs`, `tests/browser/logical_expressions.cjs`; narrow tested types |
| R1 full string-interpolation typing, finite-results/strict operand and scalar fault policy | P | V19 stores Infinity after `1 / 0`; finite literals in scalar certification are not finite-result enforcement. A6 is a proposed tightening |
| R1/R11 unknown pure calls rejected everywhere | I | V15 clean build succeeds and renders `mystery()` as text. Component direct-expression rejection does not certify root expressions |
| R2 distinct module scalar identities in the PR #76 subset | T | `BrowserScalarStorage.OwnedInitializersReadsWritesAndParameterShadowing`, `tests/browser/module_scalar_storage.cjs`; V09 shows owner isolation |
| R2 parameter identity/restoration in the bounded subset | T | V06 emits 4 twice while module x=10/hits=0; root effect and scalar browser regressions cover restoration |
| R2 general module/derived/action identity and capture authority | P | PR #75 guard plus #76 certification; colliding derived/actions/captured UI still reject. Read-only import write authority is proposed (V22 currently builds) |
| R3 conservative expression dependencies and lazy arms | T | `NativeTernaryDependencies.BothBranchesAndConditionRemainSubscribed`, `GatheringNeverExecutesInactiveBranches`; compiler/body-plan tests; V08 snapshots |
| R3 purity/cycle preflight uniformly at module root | I | V13 cyclic root getters compile and publish an empty runtime value projection instead of rejecting; V15 is also accepted |
| R4 source-order `let` initialization without collisions | I | V11 initializes z=2 but omits a/result; DOM retains an unresolved placeholder. The emitted name-keyed state object runs a before z |
| R4 import-before-importer initialization | I | V21 initializes exported seed=7 but omits importer copy=8; visible `{{copy}}` remains |
| R4 forward initializer rejection uniformly | I | V12 builds and initializes later=2, silently omitting first. PR #76 rejects forwards only when certifying its collision subset |
| R4 non-eager root subscriptions | T | V02/V07/V17 initial hits/chain stay zero; existing `root_effects.cjs` initial-state checks |
| R4 no `get` initializer reads, import-DAG policy | P | Deliberate minimal-profile decisions in A4; not claims about all accepted legacy sources |
| R5 sequential assignments and repeat entries | T | V01 gives 1→2→3; `root_effects.cjs` batches/repeats; existing arithmetic/conditional conformance |
| R5 full invocation/arity/reentry and serial commit trace | V | Parameter restoration exists; exact cross-action commit events and all arity/fault edges are not covered here |
| R6 fresh `get` reads within a root action | I | V03 final saved=0/twice=2, required saved=2. V20 outputs 0,0, required 2,4 |
| R6 write set distinct from value change | T | V07 same-value writes increment hits once per dispatch; `assignClientPath` and `flushRootSubscriptions` |
| R6 signed-zero equality and all invalidation paths | V | Component parameter equality uses Object.is; this is not complete root change/commit observer coverage |
| R7 bounded root notification/effect cascade | T | V02 final 2\|1/show 2, V17 final 1\|2\|3; `root_effects.cjs` covers batching, chaining, order, parameter writes and repeated dispatch |
| R7 source-order DAG/cycle rejection in current root subset | T | V18 rejects; `CliBuild.RootScalarEffectsRejectUnsupportedGraphsWithSourceDiagnostics`; `orderRootSubscriptions` |
| R7 all owner-qualified/global effect combinations | P | Watches on derived/imported/component values and scalar-collision programs with effects are intentionally unsupported; not a bug |
| R8 single semantic commit / arbitrary passive DOM observers | V | Current root body calls `applyClientState` after restoration and flushes effects before DOM. V02/V17 terminal snapshots agree; full observer atomicity not measured |
| R8/R9 fresh coherent root chain/diamond snapshots | I | V04 after first Step z=3/a=4 (required a=6); V05 zleft=3/zright=4/atotal=4 (required 7). A later action advances the stale total |
| R9 component dependency closure | T / V | `conditional-reactivity` and native ternary tests cover transitive invalidation; these do not prove full root/module/DOM glitch-freedom |
| R10 parameter-driven component retention/destruction | T / V | V16 visible 0\|0→0\|1→absent→absent→2\|0; root conditional/keyed suites check registry/scope cleanup. Exact dead-subscription deliveries remain uninstrumented |
| R10 defining-module capture / component-local effect lifetime | P | V10 capture is unsupported and renders empty; V24 local effect correctly rejects. V16 tests render dependency cleanup without introducing local effects |
| R11 unresolved references uniformly rejected before execution | I | V14 clean build accepts `missing` in an action. It is not in PR #76's colliding certification path |
| R11 transactional scalar certification and decoded interpolation | T | V23 correctly rejects; `DecodedStringExpressionsCannotBypassBindingProof` covers generated-key capture, initializer/forward/imported reads and parameter shadows; previous assets preserved |
| R11 terminal fault / committed snapshot retention | P | A6 is proposed; V19 currently publishes Infinity. No external-effect rollback is claimed |
| R12 backend-specific evidence and explicit unsupported claims | V | Existing gates separate native/browser and catalog known discrepancies. No existing suite certifies this whole proposed profile |

## Smallest discrepancies by correctness impact

1. **Identity and authority before broader capture (P, prerequisite).** Retain
   defining declaration identities through all root value consumers, formula
   reads, parameter frames and subscriptions. General root/module capture is
   not provided by adding a component's spelling lookup into the global dictionary.
   PR #76's static-control subset must remain narrow until those consumers are
   proven. Relevant: `SemanticImportedSymbol`/`SemanticProject` in
   [module_graph.h](../../../include/jtml/semantic/module_graph.h),
   `buildRuntimePlan(SemanticProject, ...)`, `lowerBrowserScalarStorage` and
   `validateBrowserRuntimeBindingOwnership`. This is a missing target capability,
   **not** a request to remove fail-closed guards.
2. **Validate root expression/reference/cycle admission (I, V12–V15).** A clean
   root plan can currently accept unresolved or cyclic formulas and fallback text.
   Relevant: `buildRuntimePlan`, `runtimePlanBindingPlansObjectToJson`,
   `compileRuntimeExpressionPlanForOptions`, `bootstrapClientManifest`,
   `applyClientDerived`, `evaluateClientBodyExpression`. A smallest future slice
   would prove/reject those root shapes before asset writes; no new evaluator is
   needed to state the requirement. Cycle/unresolved tests must remain negative,
   not become “eventually renders something” acceptance.
3. **Source/import initialization order (I, V11/V21).** `RuntimePlanBinding`
   vectors retain declaration order, but
   `runtimePlanBindingsObjectToJson` in
   [runtime_plan_json.cpp](../../../src/runtime_plan_json.cpp) emits name-keyed
   objects; `bootstrapClientManifest` iterates them. General compilation does not
   use PR #76's fixed-width ordered keys when there is no collision. The smallest
   target is correct ordered initialization of the existing bounded scalar forms,
   without changing legacy output contracts speculatively in this PR.
4. **Root derived freshness and coherent closure (I, V03–V05/V20).**
   `runClientStatements` reads clientState's cached getters before the end-of-action
   `applyClientState`; `applyClientDerived` then evaluates getters once in object
   order. The exact source/functions are in
   [browser_runtime_assets.cpp](../../../src/browser_runtime_assets.cpp).
   A chain ordered consumer-first and an action that saves a freshly read getter
   are separate defects; repairing terminal order alone does not fix the latter.
5. **Phase/commit/lifetime observation (V, V02/V16/V17).** Existing tests establish
   final effects and cleanup for bounded cases, but do not count every semantic
   commit or inspect every passive DOM callback. Specify the observer oracle and
   instrument it independently before claiming arbitrary glitch-free publication.
   Relevant: `executeClientAction`, `flushRootSubscriptions`, `applyClientState`,
   `componentScopeFor`, `applyComponentDerived`, `destroyRuntimeComponentInstance`
   and generated static component update paths. This is an evidence gap, not a
   demonstrated new cleanup failure.
6. **Numeric/type/fault policy (P, V19) and imported-write policy (P, V22).** These
   require A5/A6 approval before corrective work: accepted legacy behavior is not
   by itself proof that the proposed stricter contract is already established.

Items 2–4 have directly executed scalar counterexamples. Items 1, 5 and 6 must not
be presented as fixed, or as already-ratified product defects. This ranking is a
correctness ordering, not authorization for a runtime/refactor follow-up in this PR.

## OpsPulse Theme

The canonical source is
[demos/opspulse/index.jtml](../../../demos/opspulse/index.jtml), **not** its sibling
modular experiment. It declares `appState.darkMode`, `toggleTheme`, and a store
getter `themeClass`; the Theme control calls `appState.toggleTheme`, and components
consume `appState.themeClass`. The existing
[application inventory](../../application-inventory.md) records failing Theme
acceptance, and the
[module-read investigation](../../architecture/component-module-reads/README.md)
separates store/member semantics from implicit component-to-module capture.

The intended conceptual transition is false/light → true/dark → false/light,
with consumers reading the defining owner's derived value. That motivation requires
R2 identity, R6 fresh formulas and R8/R10 coherent retained observers. It is **not**
a Core scalar fixture: stores and member lowering are outside R1, and PR #76 does
not provide implicit captured reads. V10 isolates the capture prerequisite using
only scalar declarations; it does not claim that solving V10 fixes store-derived
member naming. This PR neither modifies OpsPulse nor reruns its full application
inventory. Historical 22/32 inventory evidence remains separate from the new
scalar observations. Do not attribute the entire Theme failure to the newly
measured root ordering defect without a store/member execution trace.

## Required gates and further regressions

No required test, expected result, assertion, budget or CI workflow changes here.
The new catalog is pending independent-model implementation and explicit adapter
coverage; known mismatches stay out of required green gates. Existing
`tests/conformance/oracle.cjs` uses independent expected tables but inspects
production plans, and `tests/conformance_probe.cpp` executes the production native
interpreter. Both are valuable adapters; neither is the independent reference
interpreter specified by this PR.

After approval, prioritize permanently executable negative root admission vectors,
permuted long chains/diamonds, complete action/effect/DOM trace observation, and
capture/remount identities across reexports. Add interpolation-result typing, signed-zero, operand-domain and
fault-after-imperative-output vectors before implementing A6. Until then,
recomputation counts, full external effect safety, general stores, distributed
transactions and production readiness remain outside this claim.
