# Browser Core v1: proposed application contract and current evidence

Baseline: merged main `bc5350db56220c3c5f68993ed22711ba1c1a883c`, checked
against remote main before work. PR #89 is unmerged and excluded. This is a
bounded clean-browser profile proposal, not ratification of Reactive Core A6,
general framework readiness, or browser/native/compatibility equivalence.

## Evidence and classifications

The selected implementation's [verification record](verification.md) includes
failed attempts, full local gate results and the remaining readiness boundary.

[baseline.json](baseline.json) records fresh builds of all 24 unchanged
`docs/spec/reactive-core-v1/vectors.json` programs and real Chromium
151.0.7922.34 execution of every accepted program. HTTP enforces
`script-src 'self'`. Actual source/derived storage, settled `.out` text and
imperative show output are compared with the original authored expectations.
V09 addresses are resolved through its explicit read actions, never by deriving
expected values from generated plans. V16 active instance scope supplies local
values. Rejected cases emit none of the four runnable assets. Browser close
completed. The catalog and compiler binary digests are recorded.

The six unchanged suites `action_get_reads`, `derived_publication`,
`mutable_initialization`, `reactive_core_observation`, `component_module_capture`
and `root_effects` also passed on this baseline. They supplement snapshot checks
with effect-entry order/watch values, selected synchronous DOM observations,
generated lifecycle/cleanup, source-located negatives and malformed artifacts.
They do not establish arbitrary observer atomicity or logical commit counts.
PASS below means the observed bounded vector behavior; it never upgrades that
larger unverified contract. Rejections are successful safety acceptance, not
executed application passes. Diagnostic names in the draft are symbolic: V18,
V23 and V24 currently explain rejection without their proposed E_* spelling.

| Vector | Latest-main classification | Concrete evidence |
| --- | --- | --- |
| V01 | PASS | Value 1 -> 2 -> 3 |
| V02 | PASS | 0\|0 -> 2\|1; one E1 watching 2; show 2 |
| V03 | PASS | saved=2, twice=2 in the same root action |
| V04 | PASS | 4\|2 -> 6\|3 -> 8\|4 |
| V05 | PASS | 2\|2\|4 -> 3\|4\|7 -> 4\|6\|10 |
| V06 | PASS | module x=10/hits=0 after both calls; each show 4 |
| V07 | PASS | same-value assignments give hits 1 then 2 |
| V08 | PASS | chosen 2 -> 2 -> 11 -> 11 -> 5 |
| V09 | PASS | distinct owner storage; show private! then entry! |
| V10 | PASS | imported component private! -> private!! |
| V11 | PASS | source-ordered initialization gives 3\|5 |
| V12 | REJECTED BY DESIGN | E_INIT_FORWARD at index.jtml:2:1 |
| V13 | REJECTED BY DESIGN | E_DERIVED_CYCLE at index.jtml:2:1 |
| V14 | REJECTED BY DESIGN | E_UNRESOLVED at index.jtml:4:3 |
| V15 | REJECTED BY DESIGN | E_UNSUPPORTED_EXPR at index.jtml:2:1 |
| V16 | PASS | 0\|0 -> 0\|1 -> absent -> absent -> 2\|0; existing lifecycle hooks verify generations/cleanup |
| V17 | PASS | 1\|2\|3; E1 watching 1 then E2 watching 2 |
| V18 | REJECTED BY DESIGN | cyclic root effect writes rejected before assets |
| V19 | PENDING POLICY | observed Infinity; A6a/A6c unapproved, no error/rollback oracle selected |
| V20 | PASS | shows 2,4 before DOM update; settled 2\|4\|4 |
| V21 | PASS | imported seed=7, importer copy=8 |
| V22 | PENDING POLICY | catalog requires rejection; actual Step changes seed 7 -> 9; mandatory action_get_reads also requires imported writes |
| V23 | REJECTED BY DESIGN | decoded interpolation cannot certify lexical ownership; no assets |
| V24 | REJECTED BY DESIGN | component-local/nested subscriptions rejected |

This supersedes the classifications in the pinned historical gap ledger for
this baseline only. No original source, expectation, reference-model fixture,
policy decision or earlier observation is changed.
V22 is a confirmed counterexample to the proposed A5 read-authority direction.
Subsequent full-gate inspection found that action_get_reads.cjs explicitly
requires named/reexported imported replacement to work. This conflict requires
contract review; it is classified PENDING POLICY, not an unqualified regression.
The original snapshot mismatch and first audit classification remain recorded.
Final baseline tally: 15 PASS, seven REJECTED BY DESIGN, two PENDING POLICY.

## Proposed supported application profile

The profile is clean `--target browser` output, with no implicit compatibility
fallback. All promises are about accepted, compiler-certified shapes; unsupported
combinations must reject with original source ownership and remediation.

| Area | Verified implementation boundary | Not certified by v1 |
| --- | --- | --- |
| State | Scalar module lets, source/dependency-module ordered initialization, local component state; structured values have separate existing application contracts | General types, coercion/finite-result enforcement; arbitrary duplicate/repeated initializer slots |
| Derived | Root structured pure expressions, acyclic conservative dependencies, topological publication, fresh reads inside root actions/effects | General calls; fresh component action reads; component forward getter dependencies now reject; arbitrary passive observers |
| Actions | Synchronous root assignments/show/if, parameter shadowing/restoration, owned component actions/callbacks in tested shapes | Async JTML actions, reentry/arity/fault guarantees beyond named tests; nested calls as pure formulas |
| Effects | Non-eager module-owned scalar mutable watches; same-value write notifications; one delivery per dispatch, ordered acyclic cascades before tested DOM publication | Component-local, derived/imported/collection watches, dynamic unsubscribe; more than 64 subscriptions |
| Modules | Resolved named imports/reexports, dependency-before-importer initialization; narrow collision alpha-lowering; selected imported-component scalar captures | General collision combinations, structured/attribute-only/action-only capture, arbitrary closures; read-only imported replacement policy (V22 conflict) |
| Components | Parameters/emits, local writable inputs, retained nested/keyed updates, root conditional mount/remount/cleanup; explicit caller-authorized text input in its documented subset | Direct root-loop components; general writable borrowed/member bindings; component effects |
| Fetch bridge | Real same-origin HTTP GET and lazy POST, loading/error/stale state, explicit refresh, SQLite persistence/failure/restart acceptance through ReleaseDesk | Automatic exactly-once mutation, overlapping request arbitration, general async transactionality, success callbacks/await; no such claim from compile success |
| Assets/CSP | Four compiler assets, closed public schema, external scripts/listeners, no eval/new Function or clean compatibility asset, runtime <=273000 B | Raw host code security, strict style CSP, secrecy of client state, equivalence of debug/live modes |

Fetch and owned collections/components are bounded application bridges alongside
the scalar core, not additions to the existing Reactive Core expression domain.
The serialized-request ReleaseDesk workflow is the fetch acceptance reference;
this proposal does not broaden it to concurrent mutation semantics. Existing
supported behavior outside this profile remains intact.

## Prioritized correctness blockers

1. **P1: component action derived reads are stale.** Independent source below
   compiles; Step yields `0|2` and scope `{x:1,saved:0,twice:2}` rather than
   `2|2`. Root V03's fix does not apply to `runComponentPlanStatements`.
2. **P1, closed by the selected fail-closed prerequisite: component derived
   order is not graph-certified.** `get a = z * 2` before `get z = x + 1`
   compiles on main. Initial output is `z * 2.000000000000000|2`; after x
   increments it is `4|3`, not `6|3`. `applyComponentDerived` traverses body
   order. Clean production now rejects a top-level component getter reading a
   same-scope nonpreceding getter, including self/cycles and inactive arms,
   with `E_COMPONENT_DERIVED_ORDER`, original source and dependency-order
   remediation. Existing root topological scheduling is unchanged. Reordering
   these getters is the supported source remediation; no component scheduler
   or runtime bytes are added.
3. **Policy blocker: named import write authority.** V22 expects a read-only
   binding, but current mandatory root-action acceptance explicitly permits
   named/reexported mutable import writes. A proposed E_IMPORT_WRITE guard was
   withdrawn when that unchanged oracle failed. No existing test or import
   contract is weakened. Decide how to reconcile A5 and legacy acceptance
   before introducing a profile-specific authority rule.
4. **Policy blocker: failure containment and scalar domain.** V19 and A6 remain
   pending; successful final snapshots cannot prove failure containment.

Only item 2 is implemented. It has executable wrong-output evidence and a
compiler-only source remediation. Item 1 remains an admitted-program defect.
The matrix is a proposed readiness contract; no broad component freshness,
module authority or fault semantics are silently selected.

```jtml
jtml 2
make Card
  let x = 0
  let saved = 0
  get twice = x * 2
  when step
    x = 1
    saved = twice
  box class "out"
    text "{saved}|{twice}"
  button "Step" click step
page
  Card
```

For the ordering counterexample, use x=1, `get a = z * 2`, then
`get z = x + 1`, a Step action `x += 1`, and output `{a}|{z}` in Card.
The independent supplemental probes use actual generated browser output and
controls; they do not inject JTML state. Their recorded scope is browser
file-loaded execution; the original 24-vector run uses strict HTTP CSP.

Exact supplemental sources are retained in [probes](probes/). Reproduce from
an isolated checkout, then open each generated index.html and click Step:

```sh
./build/jtml build docs/architecture/browser-core-v1/probes/component-action-fresh.jtml --target browser --out dist/core-action-fresh
./build/jtml build docs/architecture/browser-core-v1/probes/component-forward-chain.jtml --target browser --out dist/core-forward-chain
```

The first must eventually produce `2|2` and remains a confirmed defect. The
second now rejects before assets; moving z before a must initialize `4|2`
and produce `6|3`. The targeted independent browser suite verifies that
remediation, repeated updates, sibling/DOM retention and source-located rejection.

## Failure containment proposal — review required, not implemented

Propose a deliberately nontransactional **fail-stop** contract for synchronous
Core evaluation exceptions: stop remaining action/effect work, restore temporary
parameter frames, clear pending notifications, mark the application failed,
reject subsequent entries and invalidate late fetch completions for that failed
generation. A full reload/new initialization is the recovery boundary. Expose
the original source/action and label any retained state as failed partial state.
Do not advertise source-state rollback, undo of show/host/HTTP effects, or
recoverable action retry. Preserve the last DOM only when evaluation failed
before publication; a failure during DOM/lifecycle publication may leave partial
DOM, which must become inert and visibly failed. This explicit publication
exception needs review; it is not the existing A6 P1/P2 rollback model or proof
of P3's stronger last-successful-DOM promise.

This proposal does not decide whether division by zero, mixed operands or
non-finite results raise exceptions: A6a approval is separately required. Normal
HTTP failures remain fetch error data, not automatic whole-application faults.
Extern/async action execution is outside the bounded fault contract.

Acceptance before adopting it: an exception after an earlier source write and
show; failure in a later effect after an earlier callback; failure during
component update; attempted detached/normal entries after failure; late fetch
completion after failure; and reload recovery. Assert no downstream work,
no success signal, restored frames, explicit partial-state labeling, and no
automatic replay. These are future requirements, not passing current tests.

## Mandatory browser acceptance matrix

All required runs need a real browser, actual output, source-derived addresses
only for observations, independently authored expected values, explicit clean
browser/server shutdown, strict script CSP, no unexpected console/network errors
and unchanged asset budgets. A skipped browser is failure, not acceptance.

| Obligation | Required suite / oracle |
| --- | --- |
| Complete proposed profile inventory | browser_core_v1.cjs: unchanged 24 vectors; V19/V22 remain PENDING POLICY and keep this readiness audit nonzero |
| Owner identity and authorization | module_binding_collisions.cjs, module_scalar_storage.cjs, component_module_capture.cjs, explicit_input.cjs; V22 contract reconciliation; existing imported-write positives preserved |
| State/import source order | mutable_initialization.cjs: V11/V12/V21, import diamonds and malformed order certificates |
| Fresh root derived reads/publication | action_get_reads.cjs, derived_publication.cjs; original V03/V04/V05/V20 and negative transient-write control |
| Effects/phase observations | root_effects.cjs, reactive_core_observation.cjs: batching, same-value writes, source ties, cascades, callback freshness |
| Component lifetime/identity | root_conditional_mount.cjs, scripts/test_taskboard_core.sh, scripts/test_taskboard_negative.sh, scripts/test_keyed_lifecycle.sh; V16 generations, focus/caret, cleanup, detached controls |
| Real HTTP persistence/recovery | scripts/test_releasedesk.sh: no mocked API or injected state; retained serialized-request boundary; PR #89 excluded |
| CSP/output contract | strict_csp.cjs, scripts/benchmark_runtime.sh; no eval/new Function/implicit compat and unchanged 273000-byte cap |
| Component derived coherence | component_derived_order.cjs is required in CI/full gate; action-read freshness still needs its corrective slice |
| Failure containment | Review and implement the bounded policy, then the explicit fault matrix above |

The component-order suite is additive and mandatory in CI/verify_all.sh. The
full-vector readiness audit intentionally exits nonzero for pending policy or
confirmed defects; it is not installed as a waived green repository gate.
Existing suites/oracles remain mandatory and unchanged. A green repository gate
is not v1 GO while component freshness and policy remain unresolved. Hosted
Ubuntu/macOS checks and the complete local verify_all.sh are separate evidence.

## Decision

**NO-GO for a Browser Core v1 readiness claim. GO for the narrow component
getter-order admission prerequisite, subject to the full existing gate and
exact-head CI.** Fail-closed ordering does not fix component action freshness,
settle V22's conflicting contracts or ratify failure policy. Any PR must contain
this substantive compiler correction and its independent before/after browser
acceptance, rather than documentation alone.
