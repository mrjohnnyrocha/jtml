# Independent reference-model contract

This is a design for a **future** interpreter, not its implementation. It is
normative under the approval conditions in [the specification](../reactive-core-v1.md).
A6a/A6b/A6c remain unapproved: this contract specifies common transitions and
conditional failure alternatives, not an implicit rollback/termination default.
The current snapshot probe and existing conformance adapters are not this model.
They deliberately execute the implementation under test and cannot establish
an independent semantics merely by agreeing with each other.

## Independence boundary

The model MUST NOT call or copy Runtime Plan evaluation, production Expression IR
evaluation/code generation, browser runtime functions, generated component modules,
legacy `Environment` recomputation, or the compiler's scheduling algorithms.
Neither production JSON nor a serialized Runtime Plan is a trusted semantic input.
Do not derive expected snapshots by reading `window.jtml.state` or compiler output.
The checked-in vectors are independently authored expectations.

Use a small independent parser/resolver for the Core source subset, or manually
reviewed model ASTs tied to the exact source files. For initial implementation,
reviewed model ASTs are acceptable, with a separate source-to-AST equivalence audit;
this MUST NOT be reported as testing the production parser. If a production parser
is used by the implementation adapter, a production parsing mistake must still
be detectable against the independent model AST. Source spelling substitution
and guessed storage aliases are not a resolver.

## Input document, version 1

The future model accepts one closed document with these fields. Unknown fields
or node kinds reject the model input, rather than becoming generic source text.
This schema is conceptual; this PR intentionally adds no parser or evaluator.

| Field | Contract |
| --- | --- |
| `schemaVersion` | Integer `1`; independent of Runtime Plan schema/version |
| `entry` | Module identity of the entry module |
| `policies` | Explicitly approved scalar (A6a), interpolation (A6b) and failure (A6c P1/P2/P3) choices; no defaults. Missing approval yields `PENDING_POLICY` for dependent evaluation, not a model-chosen policy |
| `sources` | Exact original UTF-8 file contents and digests; source spans and literal spellings must be checked against these independently of the production frontend |
| `modules` | Ordered module records: ID, file identity, textual import order, named exports, declarations, actions, component definitions, root effect declarations and render descriptions |
| `steps` | Ordered external entries: resolved action identity, caller identity, scalar argument literals, optional expected generation; no arbitrary host code |
| `observers` | Passive text/state/derived/DOM-projection subscriptions, scoped to application or named component generations; observations only, no writes |
| `limits` | Reference-resource limits; exceeding one yields `MODEL_LIMIT`, never a JTML semantic reject or a PASS |

Each declaration has `id`, `name`, `scope`, `source` (file/line/column), `kind`
(`let` or `get`) and `expression`. Component definitions have parameter IDs,
local declarations/actions and render descriptions. Imports name an exported
declaration/action and the defining module; reexports preserve its identity.
No storage key or linked global name table is an input semantic primitive.

An expression is exactly one of:

- `{ "literal": { "kind": "number|string|boolean", "value": ... } }`;
- `{ "read": "binding-identity" }`;
- `{ "unary": "- | !", "operand": expression }`;
- `{ "binary": operator, "left": expression, "right": expression }`;
- `{ "select": expression, "then": expression, "else": expression }`.

The notation `number|string|boolean` means one of those three strings, not a
union-valued runtime object. For the unapproved S1 encoding candidate, kind `number` uses a finite decimal
lexical string as `value`; for kind `string` it is a decoded Unicode JSON string; for kind
`boolean` it is a JSON Boolean. The input encoding MUST preserve `-0` distinctly
(e.g. number lexical text `"-0"`); JSON numeric round trips alone cannot certify
that distinction. References are resolved identities after the independent
resolution pass, never expression strings. Interpolation under the approved B1/B2 lexical policy must become explicit
resolved AST expressions before execution; B1 literal segments stay data.
An uncertifiable shape rejects, rather than invoking a second runtime parser.
If S2 is selected, non-finite values need an explicitly approved input encoding
before this schema can be finalized; S1 is not an implicit numeric default.

Statements are `assign(targetId, expression)`, `if(condition, then, else)` or
`show(expression)`. Parameters have their own IDs; no save/restore by replacing
module storage. A root effect record has a subscription ID, one owned `let` watch,
a callback body, and a stable textual ordinal. Potential writes are independently
collected from all callback branches.

Render input is a small virtual description: ordered scalar text/attribute
observers, conditional children, and component instantiations with explicit
parameter expressions and defining-module identity. It is **not** generated HTML,
a body-plan opcode stream, a static-update selector list, or browser DOM code.
This projection is sufficient for the scalar vectors and component-generation
lifetime. It does not model general HTML layout, CSS, collection reconciliation,
fetch, stores, or WebSocket timing.

Example input fragment for V03:

```json
{
  "declarations": [
    {"id":"main::x","kind":"let","expression":{"literal":{"kind":"number","value":"0"}}},
    {"id":"main::saved","kind":"let","expression":{"literal":{"kind":"number","value":"0"}}},
    {"id":"main::twice","kind":"get","expression":{"binary":"*","left":{"read":"main::x"},"right":{"literal":{"kind":"number","value":"2"}}}}
  ],
  "actionBody": [
    {"assign":"main::x","expression":{"literal":{"kind":"number","value":"1"}}},
    {"assign":"main::saved","expression":{"read":"main::twice"}}
  ]
}
```

This is a fragment illustrating AST shape, not a complete valid input: a complete
record also includes source/scope, action identity, modules, imports and observers.

## Abstract state and transitions

State is `(C, W?, frames, initialized, generations, rootEffects, written,
phase, sequence, trace, dispatchStatus, failedPartial?)`. C contains only the last successful persistent
state, derived projection and active virtual render tree. Frames contain
invocation-local cells. `written` contains persistent declaration identities, not
source names. Transitions are:

1. **Resolve/validate**: resolve lexical/module identities independently; reject
   unsupported forms, illegal authority, invalid initializers and dependency
   cycles. No observable initialization is performed on reject.
2. **Initialize**: use R4 module/source order; create fresh generations and local
   source state; compute formula values by mathematical substitution; register
   non-eager effects. Emit commit 0.
3. **Enter**: reject a terminated/missing/destroyed action target; check arity and
   scalar arguments under the approved A6a policy; initialize episode state W
   from C (rollback-capable under P1/P2); allocate a fresh
   invocation frame. No commit is emitted yet.
4. **Execute**: interpret one statement at a time. Every read of a `get` recursively
   evaluates its formula from current W and the correct immutable capture/frame
   identities. Each persistent assignment joins `written`; temporary assignments
   never do. `show` appends an immediate imperative-output event.
5. **Deliver**: discard the action frame. Consider root effects in the R7 order;
   append `notify` before `effect-enter`; evaluate the callback against W; append
   `effect-exit`. Append writes and allow downstream eligibility, once each.
6. **Publish**: recursively calculate all formula observations and virtual render
   outputs from final W. Remove dead generations and edges, create new generations
   with fresh local state, and publish one complete C at sequence + 1. Emit one
   commit event; only now may commit/DOM observer snapshots be emitted.
7. **Error (conditional A6c)**: no successful commit; discard the invocation
   frame, cancel undelivered work, and retain already delivered output in the trace.
   P1 restores all episode source/cache/generation state to C and terminates the
   application; P2 performs that restoration and allows a new explicit action
   from C with an empty write set; P3 retains successful preceding source writes
   only as labeled failedPartial, leaves C/DOM last-successful, invalidates partial
   derived observations and terminates dispatch until new initialization. No
   automatic retry/replay is permitted. If policy approval is missing, report
   `PENDING_POLICY`; do not execute a candidate failure oracle as if it were selected.

Once the policies are approved, an independent model can copy immutable maps
for every statement (retaining or discarding the failed W according to policy)
and recompute every formula on every read, with DFS recursion-stack cycle checks.
It needs **no production dirty cache, queue, pass budget or incremental scheduler**.
For the small effect fixtures, enumerate possible effect permutations and select
the lexicographically earliest one satisfying the potential-write partial order.
An empty set of valid orders is a cycle. This deliberately avoids reproducing the
compiler's Kahn-style scheduler. Resource bounds can keep this exhaustive model
small; `MODEL_LIMIT` is not evidence of a production defect or conformance.

For rendering, first compute the virtual active-instance tree from the terminal
working state, then its expressions, including fresh initialization of new local
state. Nested mounts recursively repeat this finite process; recursive component
instantiation is outside the Core profile. Pure projections cannot write back
into W. Destroyed generation identities are never reused. The first implementation
may limit itself to the two-level, nonrecursive component descriptions in V10/V16.

## Output and observable trace

A result contains `validation` (`accept`, classified reject, or `MODEL_LIMIT`),
`initial`, ordered `steps`, terminal snapshots, source-attributed diagnostics,
and a trace. Permitted trace events are:

| Event | Required payload and ordering |
| --- | --- |
| `imperative-output` | Episode, phase (`action`/effect ID), statement ordinal, scalar value; immediate statement order |
| `notify` | Episode, subscription identity, watched declaration identity, coherent watched value at delivery; before callback entry |
| `effect-enter` / `effect-exit` | Episode and subscription identity; exit status `ok` or `error`; never emitted for registration or skipped effects; error exit precedes episode failure disposition |
| `destroy` / `create` | Logical generation and owner; terminal render-tree transition, old generation destroyed before replacement delivery |
| `commit` | Sequence, complete persistent state and derived values, active generation tree, virtual DOM projection; one for initialization and one per successful external entry |
| `observer` | Owner, commit sequence and requested snapshot; no dead owner; no cross-commit mixture |
| `reject` / `episode-error` | Diagnostic, original location and affected identities; error includes selected policy, rolled-back/partial-state disposition and next-entry authority; neither is a successful commit |
| `PENDING_POLICY` | Unapproved/missing A6 choice and affected vector; no candidate outcome is reported as PASS |

Internal evaluation counts and physical patch counts are absent. A structured
number encoding must follow the approved A6a choice and distinguish non-finite
values from a domain error; Infinity cannot become
JSON null and accidentally compare equal to “uninitialized”.

## Comparing an implementation without borrowing its semantics

The implementation adapter compiles the unchanged source and performs real UI
entries (or a separately identified native public action entry). It records public
scalar outputs, real DOM, callback order where test observation exists, owner
lifetimes and diagnostics. It MAY use emitted metadata solely to map a physical
instance to an already independently specified identity. It MUST NOT obtain
expected values or binding resolution from that metadata.

Compare values by scalar kind and R6 equality, ordered imperative/effect traces,
commit sequence, and active-generation projections. Do not compare generated IDs,
JavaScript text, recomputation counts, or wall-clock times. A missing phase hook
is `UNVERIFIED`, not an assumed empty notification list. Passive DOM instrumentation
must not install app handlers, mutate JTML state or dispatch callbacks on its own.

Baseline evidence here records post-load/post-click snapshots and show output;
it does not observe every mutation observer invocation or commit boundary. Full
atomicity and notification/lifetime traces therefore remain **pending adapter
instrumentation**. Do not attach expected failures to required CI or replace
strong existing assertions with this smaller model. Any future model defect
correction must preserve the original failing production counterexample and
change its independently reviewed oracle explicitly.

## Failure policy approval boundary

The [A6c comparison](a6-decisions.md#a6c--failure-publication-rollback-faulting-and-recovery)
is part of this contract. V19 has explicit candidate outcomes, not a default fault
transition. Test the distinguishing prior-write/show/next-entry witness and an
error in a later effect after approval; V19's first failing assignment cannot
prove rollback. Successful-path traces remain as specified by A1/A2/A3/A7.
The model must not implement P2 merely because this review recommends it.
