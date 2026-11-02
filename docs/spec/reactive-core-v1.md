# JTML Reactive Core v1 — normative draft

Baseline investigated: `ae8f984dc3c35163cc702f7e494a5f2b015124fb` (main after
PR #76). **Proposed contract, not a statement of current conformance.** MUST,
MUST NOT and MAY are requirements for an implementation claiming this profile
*after ratification*. Approval items A1–A7 below are open. Until they are resolved,
this document is a consistent proposal, not permission to implement it as final.
The [gap ledger](reactive-core-v1/gaps.md) classifies every rule against the pinned
implementation. Existing JTML programs outside this profile retain their existing
backend contracts; this PR changes no compiler, runtime, CI or performance limit.

The [24 vectors](reactive-core-v1/vectors.json), their
[reading and execution contract](reactive-core-v1/conformance.md), and the
[independent reference-model contract](reactive-core-v1/reference-model.md) form
part of this proposal. Vector expectations are normative examples. Baseline
observations are evidence only and cannot redefine those expectations.

## R1. Domain and admissible programs

A scalar is a Boolean, a Unicode string (no normalization), or a finite IEEE-754
binary64 number, including signed zero. Literal escapes are decoded exactly once
before expression analysis. The admitted pure expression forms are scalar
literals, resolved binding reads, grouping, unary `-`/`!`, arithmetic
`+ - * / %`, comparisons `== != < <= > >=`, Boolean `&& ||`, and `?:`.
Friendly quoted interpolation with source delimiters `{e}` is sugar for ordered
concatenation of literal segments and the scalar text of e (equivalently include
an empty string on the left). Each embedded e uses this same pure expression
profile and is resolved before execution. Core v1 does not admit encoded brace
delimiters such as `\u007b`/`\u007d` inside a literal segment: reject them as
`E_UNCERTIFIED_EXPR`, rather than introducing a new read while reparsing a decoded
string later. Literal brace escaping is outside this first profile. Ordinary
non-delimiter string escapes are decoded once; no string value is executable.
Evaluation is left to right; Boolean operators and conditional arms are lazy.
Operands of `!`, `&&`, `||` and conditions MUST be Boolean; arithmetic other than
`+` MUST have numeric operands. `+` adds numbers, or concatenates if either operand
is a string, converting the other scalar to text. Boolean text is `true`/`false`;
number text is the shortest round-tripping decimal, with `-0` rendered `0`.
String comparisons are lexicographic by Unicode scalar value. Numeric comparisons
are numeric; mixed-type comparisons other than equality are errors. Equality is
false for different scalar kinds; numeric `==` equates `0` and `-0`.

A non-finite arithmetic result, division/remainder by zero, invalid operand kind,
or non-scalar action argument is `E_SCALAR_DOMAIN`. This is a **proposed** stricter
profile, not an existing language-wide typechecker. Invalid Unicode scalar strings
are outside this profile. Vectors use exact small integers and ASCII except escape
controls, so they do not silently settle every host-number formatting corner.

Calls inside pure expressions, member/index access, object/array values, fetch,
async actions, loops, extern/raw execution, and declarations inside action bodies
are outside this profile. An implementation MUST reject an unsupported shape,
including one in an inactive expression arm, rather than execute source text,
coerce an unresolved name to text, guess a dependency, or use a legacy global.
No runtime `eval` or `new Function` is permitted. This does not remove broader
existing component/collection contracts from JTML; they are not certified by Core v1.

## R2. Identity, ownership and authority

A declaration identity is `(defining module, lexical scope, declaration ordinal)`.
A component-local storage identity adds a component **generation**, not merely
its component type or current DOM position. An invocation parameter adds an
invocation/frame identity. Module identity comes from resolved module records,
not a basename or source spelling. These are abstract identities; this document
does not prescribe storage keys, public JSON layout, or alpha-renaming machinery.

Resolution is lexical: invocation parameters shadow enclosing bindings; component
parameters/locals shadow defining-module declarations; a component's free name
resolves in its **defining module**, never its caller's module. Duplicate value
or action declarations in one lexical namespace are errors. Value and action
namespaces are separate. Two modules may independently declare `tone` or `step`.
Alpha-renaming an otherwise well-formed declaration and all its resolved uses
MUST leave every trace unchanged (apart from diagnostic spelling).

Imports and reexports reference the original declaration identity and do not copy
storage. An imported binding and a component's defining-module capture confer
read authority only. A root action may write its own module's `let`; a component
action may write its own instance's `let`; either may write its own parameters.
To mutate another owner's state, dispatch that owner's exported action as an
external entry point. Importing a value alone MUST NOT grant write authority.
The fully general rule is a target: PR #76 certifies only a narrow scalar subset.
Generated identifiers MUST never capture an unresolved source reference, including
a reference introduced by string decoding/interpolation (V23).

## R3. Dependency graph and purity

Every read/write operand MUST carry its resolved identity through execution.
For a pure expression `e`, `deps(e)` is the union of referenced state/derived
identities in its entire syntax, including both conditional arms and all Boolean
operands. Collecting dependencies MUST NOT evaluate expressions. An implementation
MAY over-approximate this set, but MUST NOT omit a required identity or make an
inactive arm execute. Unused reevaluation counts are not observable semantics.
A `get` is a pure, read-only formula, not mutable cached source text. Assigning to
one is `E_DERIVED_WRITE`. Host time, randomness, IO and action calls are not formulas.

A directed edge points from a dependency to its derived consumer. The graph MUST
be acyclic, including self-edges and potential inactive-arm edges. Reject a cycle
before initialization with `E_DERIVED_CYCLE`, identifying source declarations in
the cycle. Do not resolve a cycle with a pass limit, default value, last-value
cache, or “until unchanged” evaluation. See V04, V05, V08 and V13.

## R4. Initialization

A Core project has a finite acyclic import graph. Import cycles are rejected as
`E_MODULE_CYCLE` in this profile; this makes no claim about all existing JTML
module-cycle behavior. Visit dependencies before importers, preserving textual
import order between independent dependencies; visit each resolved module once.
Within a module, initialize mutable `let` declarations exactly once in source
order. A `let` initializer may read earlier initialized local `let` declarations
and initialized imported scalar `let` bindings (read-only import authority). It
may not read a later local `let`,
itself, or any `get` in this initial profile (`E_INIT_FORWARD` or
`E_INIT_DERIVED`, respectively). There are no action-body declarations here.

`get` names are resolved over the complete enclosing declaration scope, so forward
formula references can be analyzed for cycles. After all mutable initialization,
evaluate the acyclic formulas to their unique values. Source/name/JSON object order
MUST NOT affect those values. Register root subscriptions non-eagerly; registration
and derived initialization MUST NOT invoke an effect callback. Publish state,
derived values and the initial DOM as commit 0. An initialization error publishes
no initial commit and no effect. Build rejection MUST preserve previously emitted
successful assets. The broad initializer rule is not yet implemented uniformly.

```jtml
jtml 2
let z = 2
let a = z + 1
get result = a + z
page
  text result
```

The initial source state is `{z: 2, a: 3}`, derived state `{result: 5}`, and visible
text `5`, irrespective of whether a serializer emits `a` before `z` (V11).

## R5. Synchronous action execution

Each accepted external action entry starts one **episode** against the last
committed snapshot `C`. Evaluate its arguments in the caller's committed lexical
context, left to right, before installing a new invocation frame. Arity is exact.
The action executes sequential assignments, `if`/`else`, and scalar `show` in
source order against a private working state `W` initially equal to `C`.
Nested action calls, synchronous reentry, `await` and implicit host callbacks
inside the episode are outside this profile. Later external entries run serially,
after the preceding episode completes; an implementation MUST NOT merge two
external actions merely because they occur in one host event-loop turn.

An assignment evaluates its RHS before changing its resolved destination. Updating
a parameter changes only its frame cell and never dirties an enclosing declaration.
Destroy the invocation frame before delivering any root subscription. Parameter
“restoration” is an observational requirement, not permission to temporarily
replace another declaration's identity. Successful return, including no persistent
writes, proceeds to exactly one commit; temporary frame state is never published.

```jtml
jtml 2
let x = 0
let saved = 0
get twice = x * 2
when step
  x = 1
  saved = twice
page
  text saved
  button "Step" click step
```

After Step, `x=1`, `saved=2`, `twice=2`. Reading a pre-action cache and saving `0`
is forbidden even if the DOM later recomputes `twice` (V03).

## R6. Mutation, invalidation and fresh reads

Maintain a set `written` of persistent `let` identities assigned during the
episode, separate from the set whose final values differ from `C`. Every executed
assignment is a write, including `x = x` and `x = 1; x = 0` starting from zero.
A false conditional branch produces no writes. Repeated writes coalesce by
identity for notification eligibility; they still execute sequentially.

A `get` retains the lexical identities resolved at its declaration: invoking an
action with a same-spelled parameter MUST NOT rebind the formula to that frame.
Component parameter cells belong to their own component scope, not the caller.

A persistent write invalidates all transitive formula consumers. Reading any
`get` at any permitted observation point MUST recursively mean its formula over
**the current W**, with all its dependencies from that same state. Cached and
uncached implementations must be indistinguishable. In particular, an action
can read its just-written state and current derived consequences. Recomputing a
formula is neither a source mutation nor a subscription notification.

For value-change reports use kind-sensitive equality, with signed zeros distinct
(the `Object.is` relation on this finite scalar domain). This relation never
suppresses the write notifications specified next. V07 separates these concepts.

## R7. Subscription notification and effect execution

This profile specifies the boundary of the **existing bounded root scalar effect
form**, not a new general subscribe API. Each `effect x` is a distinct subscription
identity whose watch is one defining-module mutable scalar `let`. Watches on
`get`, components, imports, collections, or dynamic registrations are outside
this effect adapter and MUST be rejected, even though render observers can read
`get` and imported captures. Callbacks have no parameters. They use R5's statement
forms and may assign existing owned scalar state. Initialization is non-eager.

Before delivery, discard the action frame and make derived reads coherent. Let
`writes(E)` include all potential persistent writes in callback E, including both
branches. Order effects by the edges `E -> F` when `writes(E)` contains F's watch.
Reject every potential cycle, including self-feedback, with `E_EFFECT_CYCLE`.
Between ready effects use defining-module initialization order then source order;
this order MUST NOT depend on emitted action names or hash iteration.

Consider every effect once in that order. If its watch is in the accumulated
`written` set, deliver **one notification**, recording the watch value immediately
before the callback, then execute the callback. Otherwise skip it. A notification
is a delivery event, not reevaluation, execution of the callback's first statement,
or DOM publication. Callback writes join `written`; their derived consequences
must be coherent before subsequent callbacks read. An effect can therefore enable
a downstream effect later in the same episode. Every effect runs at most once;
there are no recursive dispatches, unbounded fixed-point rounds or feedback.

An effect may read other state as it stands at its turn; no promise makes an early
effect see a later effect's future writes. V17 specifies this ordering. This is
observable staged execution inside one episode, not simultaneous callbacks.
Implementation resource limits MAY cause an explicit pre-execution unsupported
rejection; they MUST NOT silently truncate subscriptions or formulas. The existing
64-subscription/1024-callback-statement limits remain unchanged by this PR.

## R8. Commit boundary and observable atomicity

After the action and eligible acyclic effects finish, stabilize formulas and
compute the final render projection from W. One successful episode publishes one
logical commit `(sequence, persistent state, derived values, active generations,
render projection)`. State and formulas in that commit MUST describe the same W.
DOM observers see the projection of that commit before the next external action
begins. Rendering MUST NOT itself register duplicate root effects or cause writes.

There may be multiple physical DOM patches, but there MUST be no admitted JTML
observer callback, host-event action entry or paint between patches of a logical
commit. Passive `MutationObserver` callbacks may receive multiple mutation records;
reading the managed DOM in that callback MUST yield the coherent final projection,
not an intermediate diamond value. Mutation-record counts, recomputation counts,
patch counts, and debug dictionary contents are not a stable observation API.
This profile excludes synchronous arbitrary host/custom-element callbacks during
patching; it does not promise a transaction over arbitrary third-party JavaScript.

The observation permissions are exact:

| Observer | Permitted values during an episode |
| --- | --- |
| Action RHS, condition or `show` | Current sequential W, with fresh derived reads; may see intermediate action values |
| Root effect notification | Coherent W after the action and all preceding callbacks, before its own callback |
| Effect RHS, condition or `show` | Current sequential W at that statement, with fresh derived reads |
| Commit observer, passive DOM read/paint | Previous C until publication; then the final commit only |
| Destroyed generation's observer | No later delivery at all |

Thus `x=1; show twice; x=2; show twice` starting at zero emits `2,4`; an `effect x`
is delivered once with `x=2`; committed DOM never shows the interim `x=1` (V20).
`show` is an explicit imperative output, **not** a DOM observer or an atomic commit.
This contract promises no rollback of console output, IO already performed by an
external effect, navigation, network requests, or distributed transactions.

## R9. Glitch-freedom

For a snapshot `S` and acyclic formula graph, define `D(S)` recursively by pure
substitution of formulas. Every admitted derived observation is a projection of
`S + D(S)` for exactly one of the W/C snapshots allowed by R8. Mixing new left and
old right diamond arms is forbidden, even temporarily inside a delivered callback.
Terminal correctness after a second action is not sufficient. These constraints
apply equally to root values, retained component arguments and imported captures;
backend architecture does not weaken the requirement. The implementation need
not use topological queues: only the observations, not scheduling machinery, are
specified. V04/V05/V20 are intentionally independent of recomputation counts.

## R10. Render-observer ownership and lifecycle

A render observer belongs to a specific application or component generation. A
component's initial parameters are evaluated in its caller; its free captures
retain R2's defining-module identity. A retained instance receives new parameter
values without reinitializing its local `let`. A removed subtree loses its render
dependency edges, subscriptions, queued deliveries, invocation authority and
owned state before any later commit is delivered. Destruction is idempotent.
A remount allocates a new generation and initializes fresh local state. The old
generation MUST NOT receive updates or affect the new one, even if a DOM ID or
key spelling is reused. V16 uses parameter propagation to exercise cleanup without
claiming implicit module capture already works. Component-local `effect` syntax
remains outside this adapter (V24); render subscriptions are not that feature.

## R11. Rejection and faults

Resolve all references and prove graph/authority/profile validity before
initialization. An unsupported, unresolved, cyclic or unauthorized program is
rejected with original file/line/column and component context where relevant;
there is no “best effort” value, runnable partial output or compatibility fallback.
Symbolic diagnostic codes in the vectors classify the failure; existing JTML
messages are not claimed to implement these new codes yet.

For an unforeseen domain failure during an episode, emit a fault, publish **no
successful commit for that episode**, retain the last committed snapshot as the
last valid observation, discard pending deliveries, and mark that instance
faulted. Further dispatches reject until a new instance is initialized. Previously
emitted imperative outputs remain emitted; already-performed external effects
are not undone. This is a proposed logical publication/fault policy, not a claim
that today's runtime rolls back mutations or physical DOM. Host process crashes
and DOM patch exceptions are outside the scalar model; they cannot be reported
as successful conforming commits. V19 is pending approval of this policy.

## R12. Conformance claims and exclusions

A backend must name the supported profile and report explicit unsupported
rejections. It may not claim full Core v1 conformance by passing only scalar
collision controls, doctor, a research milestone, or the current full gate.
Passing snapshot vectors alone does not prove R8's observer atomicity. Native,
clean browser, compat/debug and live backends need separately identified evidence.
Existing required gates remain unchanged; pending target vectors are kept outside
them. Stores, effects beyond R7, fetch and distributed/network behavior are not
specified by this scalar core. OpsPulse's store/member and implicit module-read
problems motivate the [gap analysis](reactive-core-v1/gaps.md#opspulse-theme), not
an unsupported promise that this PR fixes that application.

## Approval record required before implementing the reference interpreter

All entries are **proposed / awaiting human approval**. They are resolved within
this draft so its vectors have definite answers; approving or changing them is a
product/semantics decision, not an implementation detail.

| ID | Exact proposed decision | Consequence requiring approval |
| --- | --- | --- |
| A1 | Fresh read-your-writes for `get` inside actions and effects; `show` remains immediate | Changes current stale root action observations (V03/V20); does not buffer external output |
| A2 | One commit per external action including its acyclic root-effect cascade; no commit interleaving | Defines batching, notification order and DOM atomicity; no cross-action batching |
| A3 | Write-triggered, non-eager root subscriptions, same-value writes included; static DAG with source-order ties | Preserves bounded existing root semantics rather than value-change or active-dependency subscriptions |
| A4 | Source-ordered earlier-`let` initializers; no initializer `get` reads; complete-scope acyclic `get` formulas; import DAG | Sets explicit restrictions and initialization/cycle error behavior; later extensions require a revision |
| A5 | Defining-module identities, read-only imports/captures and generation-owned render edges | Generalizes beyond PR #76's supported subset; no imported-value write authority |
| A6 | Finite scalar domain, operand/string-interpolation rules, signed-zero change equality, terminal fault with no successful publication | Tightens current coercion/non-finite/error behavior; does not promise rollback of external effects |
| A7 | Conservative dependency union with lazy evaluation; commit/DOM observer projection as the conformance oracle | Rejects inactive-arm cycles; requires phase/lifetime instrumentation beyond today's snapshot suites |

An approved revision MUST record decisions and adjust affected vectors together.
Neither this PR nor a green CI run constitutes that approval.
