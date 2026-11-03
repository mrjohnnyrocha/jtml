# A6 decision options and compatibility review

This review splits the unapproved A6 into **A6a**, **A6b** and **A6c**. None is
ratified. The user's acceptance in principle of A1–A5 and A7 is limited to a bounded
Core with the compatibility/unsupported boundaries below; it does not select an
A6 failure policy. Recommendations here are recommendations for approval, not
normative defaults or statements of implemented behavior.

## A6a — scalar domain, operands, equality and coercion

Two concrete directions are available:

| Option | Proposed contract | Compatibility consequence |
| --- | --- | --- |
| S1 — explicit strict Core (preferred) | Finite binary64 numbers, strings and Booleans; Boolean-only conditions/`!`/`&&`/`||`; numeric arithmetic except documented string `+`; kind-sensitive equality, numeric `==` equating signed zeros, change reports distinguishing them | An explicitly selected future Core profile must reject or guard incompatible uses; existing Friendly/native/live/compat behavior is not silently changed |
| S2 — preserve backend coercions | Admit non-Boolean truthiness and existing non-finite arithmetic behavior | Requires a new common truthiness, equality, coercion and non-finite representation decision before an independent oracle can agree across backends; “whatever JS/native does” is not a portable specification |

S1 is the preferred direction for a small independent model. There is **no new
Core compiler flag or mode implemented by this PR**. Existing `--prod`,
`--compat` and `--debug` select their existing browser asset contracts; none
is an implicit selection of strict Core scalar semantics. Before any future profile is
exposed, its admission boundary, diagnostics and migration guidance must be
explicit. Do not globally reinterpret an existing program or silently replace an
existing error/compatibility contract because the spec is merged. S1 approval
would still need the exact string comparison/number formatting and coercion
choices currently proposed in R1; those are not implied by Boolean-only typing.

### Existing supported behavior affected

| Existing source/test | Evidence at the pinned baseline | Effect of the proposed strict profile |
| --- | --- | --- |
| `NativeLogical.ExistingNativeTruthinessIsPreserved` in `tests/test_interpreter_logical.cpp` | Intentionally tests `0 && 2`, `2 || 0`, numeric negation and strings including empty, `"false"`, `"0"`, `"true"`, `"hello"` | These are supported native compatibility expressions, not defects. Strict Boolean admission excludes them. The test and its existing backend contract must remain intact |
| `Interpreter::isTruthy` / `evaluateExpression` in `src/interpreter.cpp` | Native truthiness uses string conversion, true/false spellings and `std::stod`, then nonempty-string truthiness | JS truthiness is not an equivalent migration, especially for `"false"` and numeric strings. No universal rewrite such as `s != ""` preserves all native cases |
| `examples/friendly_store.jtml`: `if auth.user`; `examples/friendly_fetch.jtml`: `if users.error` | Checked-in uses of string/payload truthiness | Full programs also use stores/fetch outside the scalar Core. Document both boundaries instead of attributing their exclusion solely to Boolean typing or changing their current behavior |
| `examples/friendly_media.jtml`: `if selectedImage` | Existing object/media conditional use | Media/member/object behavior is already outside this scalar profile; preserve its existing broader contract |
| `tests/conformance/logical_cases.cjs`, `NativeLogical.BooleanTruthTablesAndType`, Reference's `done`/`help` predicates | Boolean declarations and numeric comparisons producing Booleans | These predicate shapes fit S1; that is not proof their full containing programs conform to Core |
| `NativeLogical.InactiveErrorsAreSkippedAndActiveErrorsStillThrow`, logical-and/or guard vectors | False/true Boolean guards prevent evaluation of a division by zero; active native division throws | Finite-result checks must occur only for evaluated arithmetic. Conservative dependency collection must not evaluate an inactive arm or reject a valid guard merely because its divisor can be zero |
| V19 in the preserved browser observations | Executing `1 / 0` stores Infinity and renders it | Finite-result checking changes observed browser behavior. V19 is not an existing rollback test, since no browser error was raised |

For a known numeric predicate the *intended* rewrite may be `n != 0`; for a string
presence test it may be `s != ""`. Authors must choose the intended predicate,
not assume those transformations preserve every backend's coercion. Unknown
parameter/data types need a proof or an evaluated-site guard under S1; permissive
fallback to truthiness is not strict typing. A statically known ill-typed expression
can be an admission error even in an inactive arm. A well-typed but potentially
zero-divisor expression remains lazy at execution. Unresolved names and unsupported
calls still undergo whole-program certification under R1/R3/R11.

Finite inputs/literals do not guarantee finite results: division/remainder by
zero, overflow of `a * b` or `a + b`, and repeated accumulation need specified
checks at evaluated assignments/formula reads. Underflow to finite signed zero
is distinct from a non-finite result. Native division and integer-cast modulo in
`evaluateExpression` are not a cross-backend IEEE remainder specification. Keeping
signed-zero numeric equality separate from write-triggered notifications is
necessary: neither value equality nor a same-value assignment suppresses R7's
accepted write notification rule.

**Additional oracle coverage before S1 implementation:** non-Boolean conditions
and operators, numeric/string coercions and comparisons, modulo of nonintegers,
overflow, guarded zero division, signed-zero equality/change reports and number
formatting. Existing Boolean/finite small-integer successes cannot settle those
choices. No existing test/gate is removed or reclassified by this document.

## A6b — interpolation, escaping and lexical certification

The required security/correctness invariant is independent of the choice below:
**every executable read introduced by interpolation must resolve to a declaration
before execution; decoding or reparsing a string must never capture a generated
storage key or fall back to a legacy namespace.** String values are not executable.
PR #76's rejection of uncertified string ASTs remains correct for its bounded
backend; it is not a language-wide prohibition on literal brace text.

| Option | Delimiter and escaping policy | Consequence |
| --- | --- | --- |
| B1 — source-delimited interpolation (preferred) | Only unescaped source `{e}` delimits interpolation. Decode literal-segment escapes once; encoded/escaped braces in those segments remain literal data and are never rescanned as expressions | Supports literal brace text without adding hidden references. A backend unable to prove literal preservation must explicitly reject that shape as unsupported; it must not guess |
| B2 — decoded-delimiter interpolation | Decode before identifying interpolation delimiters, retain source provenance, then parse and resolve every embedded expression before certification | Encoded braces may introduce reads; unresolved/generated-key guesses reject. Literal brace escaping requires a separate explicit spelling rule. Cannot reuse a later runtime string parser |

B1 is recommended because it makes escaping an ordinary literal-preservation
operation. It is **not approved or implemented here**. The original blanket
encoded-brace exclusion is withdrawn as a settled normative target. R1's original
concatenation/scalar-to-text proposal, single interpolation result type, brace
escaping, quote/backslash/Unicode behavior and source location mapping remain
A6b decisions. Neither B1 nor B2 permits a source-name alias into generated storage.

V23 remains the **current bounded compiler certification rejection** and records
that fact explicitly. It does not ratify a global rejection rule for encoded
braces: B1 could admit the string as literal data when preservation is proven;
B2 must resolve any decoded expression and reject an unresolved generated-name
read. Future semantic vectors must cover both normal interpolation and literal
escaping after B1/B2 approval. Preserve the existing PR #76 regression until a
separate proved implementation justifies changing its supported boundary.

## A6c — failure publication, rollback, faulting and recovery

The options below cover an error raised during an action, formula evaluation,
effect callback or terminal projection preparation **before successful publication**.
Static rejection and initialization failure still publish no initial commit.
Host crashes, DOM patch exceptions and arbitrary external/custom-element callbacks
remain outside the bounded scalar episode. No option promises distributed
transactions, undo of IO, or rollback of already emitted `show` output.

`C` denotes the immutable last successful logical commit and its DOM; `W` denotes
an episode's evolving source state. A failed RHS must be checked before storing
its result, if A6a's domain checking is selected. A failed **application instance**
means the whole reactive domain participating in the episode, including its
module roots and descendant component generations, not just a temporary action
frame whose destruction would leave shared root subscriptions running.

| Policy | Persistent state and publication | Effects and immediate output | Recovery and implementation cost | Compatibility |
| --- | --- | --- | --- | --- |
| P1 — rollback + failed instance termination | Roll back **all episode writes**, including writes by preceding effect callbacks, to C; discard tentative generations/formulas; no successful commit or new DOM projection | Cancel undelivered notifications/callbacks; delivered notifications and already emitted show/external output remain historical attempted-episode events and are never retracted | Reject further entries; only fresh application initialization recovers. Requires a staged state or complete undo journal, cache/write-set/frame/lifetime cleanup, and instance teardown | Neither browser nor native code establishes this whole guarantee today. More disruptive availability than existing exception handling |
| P2 — rollback + recoverable action error | Same persistent/derived/generation/DOM rollback to C and no successful commit for the failed episode | Same cancellation and preservation of already delivered output as P1. No automatic retry or replay of callbacks | Clear episode writes/queues, restore all scopes, leave application active at C. Next explicit external entry starts a new episode. Requires P1's rollback work plus proof that no dirty work/subscription/frame/lifetime authority leaks into the next action | Preferred for the deliberately bounded scalar Core, but a real new guarantee. More usable than terminating the app; unsafe to claim for arbitrary IO, host callbacks or today's backends |
| P3 — explicitly bounded nontransactional failure | Retain only successful source writes before the first failure in a **failed partial state**; do not store the invalid RHS. Preserve C's DOM and publish no successful commit. Do not expose stale derived caches as coherent partial derived values | Cancel the remaining episode; preceding callback writes remain in partial state and delivered notifications/show/external outputs remain. No continuation of downstream effects | Mark entire application failed; diagnostic inspection may expose the labeled partial source state, never a successful commit snapshot. New initialization is required; no implicit resume from partial state. Cheaper than rollback but still needs explicit failure containment and queue/frame cleanup | Closest to current direct-write structure, but current code does not prove these stopping, diagnostic, or isolation guarantees. Weaker failed-episode source atomicity must be an explicit exception to R8, not hidden behind A2 |

For P1/P3, the last-rendered DOM remains inert: normal root/component deliveries
and action authority are disabled for the failed application. C is historical
last-successful state, not a claim that its old generations remain dispatchable.
Only fresh initialization allocates active generations again.

All policies preserve successful-episode A1/A2/R7/R8 behavior. P1/P2 provide
transactional **persistent-state** failure atomicity; they do not turn already
executed callbacks, notifications or `show` into reversible effects. P3 only
provides the successful-commit/DOM boundary and a labeled failed partial source
state. It must not advertise failed-source rollback or allow later normal actions
to read that partial state as if the episode had committed.

### A concrete distinguishing witness (analysis, not a new gated vector)

```jtml
jtml 2
let x = 0
let y = 1
when fail
  x = 2
  show x
  y = 1 / 0
when inspect
  show x
page
  text "{x}|{y}"
  button "Fail" click fail
  button "Inspect" click inspect
```

Assuming A6a makes the evaluated division an error, all three emit immediate
`show 2` before the error and issue no successful commit for Fail. P1 restores
x=0/y=1 and rejects Inspect because the instance terminated. P2 restores x=0/y=1
and permits a *new explicit* Inspect episode to show 0. P3 preserves failed partial
x=2/y=1 for diagnostics, keeps DOM at the last successful `0|1`, and rejects Inspect
until reinitialization. No policy emits a normal commit with the partial state.

If the failure occurs in a later effect instead, P1/P2 must also roll back earlier
callbacks' source writes; P3 retains those writes as failed partial state. The
already delivered `notify`, `effect-enter` and `show` events remain associated
with the failed episode under every policy. The failing callback gets an error
exit, not a normal `effect-exit`; later callbacks do not run. Recovery never
silently replays an earlier effect. This makes notification delivery, execution,
and successful publication separate facts.

### R5/R7/R8/R11 consistency under each option

| Rule | P1 | P2 | P3 |
| --- | --- | --- | --- |
| R5 action state | Isolated rollback-capable W; discard frame on success/error | Same, then restore dispatchable C on error | Evolving state can be written eagerly; failed partial W survives only in labeled diagnostics, with dispatch disabled |
| R7 effects | DAG/at-most-once order on successful episodes; on error cancel remaining deliveries and undo episode state writes, not delivered output | Same; next external entry has an empty episode write set and no carried callbacks | Same successful order; on error stop immediately, retain completed writes as partial and clear future delivery authority |
| R8 commit/atomicity | C and DOM remain last successful snapshot on failed episode | Same; next entry starts from C | No successful partial commit; explicitly weaker failed-source atomicity, with partial diagnostics separate from C/DOM |
| R11 error result | Failure with `rolled-back`, terminal instance | Recoverable action error with `rolled-back`, active instance | Failure with `partial-state`, terminal instance; no rollback claim |

R5/R7/R8/R11 specify the common successful path plus these conditional cases;
none may independently fix the old P1 behavior as the default. The independent
model must require an explicitly approved failure policy. If it is absent, a
failure-path vector yields `PENDING_POLICY`, not P1, P2, P3 or a conformance PASS.
V19 therefore stays pending on both A6a's error trigger and A6c's publication policy.
V19 alone cannot distinguish rollback from retained preceding writes: its first
assignment fails. The witness above and an effect-failure counterpart are required
additional tests after approval, not replaced by V19's current Infinity observation.

### Preferred recommendation, not selection

Recommend **P2 for the bounded scalar Core**, with an explicit recoverable action
error, no successful publication, complete episode-state rollback, and no automatic
retry. Its usefulness and coherent next-action baseline justify the extra work
when callbacks are limited to scalar writes and immediate show. Before approving
P2, require a design proving restoration of source cells, formula caches, written
sets, parameter frames, tentative mount generations and pending notification
queues for action-, derived-, effect- and projection-preparation errors. External
IO and arbitrary callbacks remain unsupported; emitted output is not undone.

P1 is a defensible simpler recovery contract if reliable resumption cannot be
proved, but it does not avoid implementing rollback. P3 is the honest alternative
if rollback is deferred; approve it only with its explicit failed-source atomicity
exception, terminated dispatch and labeled partial diagnostics. If none of these
costs/contracts is approved, leave failure semantics and V19 pending rather than
label the runtime's current behavior a policy.

## Observed implementation is separate from all three proposals

`assignClientPath` writes directly to clientState. `executeClientAction` restores
parameters in `finally`; it has no episode-state rollback journal and reaches
`applyClientState` only after the body returns normally. `flushRootSubscriptions`
clears its write set in `finally`, but does not undo earlier callback writes.
These source facts do not prove any full P1/P2/P3 contract. Native
`evaluateExpression` throws on active division/modulo by zero; the preserved
native truthiness test deliberately retains coercion. No new failure/recovery
browser experiments are claimed in this review.

The original **21 Chromium executions remain observations, not 21 conformance
passes**. Their byte-for-byte preserved evidence includes browser Infinity in
V19, not an error, rollback, termination or recovery test. Full commit/notification/
MutationObserver/lifetime traces remain unverified. Existing CI can validate
repository health; it cannot select A6a/b/c or ratify these semantics.

## Review validation

The documentation checks preserve all 24 source-file maps and the original
baseline-observations.json bytes, check V19's unselected P1/P2/P3 alternatives,
validate the split approval record, and validate local Markdown links/anchors,
fences and JSON. `git diff --check` passes. Three existing tests were rerun on the
pinned runtime build and passed: NativeLogical.BooleanTruthTablesAndType,
NativeLogical.ExistingNativeTruthinessIsPreserved and
NativeLogical.InactiveErrorsAreSkippedAndActiveErrorsStillThrow. These are
bounded compatibility checks, not Core conformance certification. No runtime,
compiler, example, CI, required gate or budget is modified; the full runtime gate
was not rerun for this documentation-only refinement. Hosted CI is reported
separately for the actual pushed commit.
