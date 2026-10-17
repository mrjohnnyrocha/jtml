# Conservative conditional dependencies

This slice fixes conditional dependency correctness in the existing static browser
pipeline. It does not freeze the complete v0.3 language surface or declare a
release ready.

## Contract and supported writes

For the expression shapes covered here, required observable dependencies must be
a subset of compiled dependencies. Over-approximation may cause unnecessary
updates; missing dependencies must not leave the UI stale.

The bounded supported matrix uses compiler-owned Expression IR for literals,
observable paths, object properties, computed subscripts, arithmetic, comparisons,
logical operators, negation, conditional expressions and derived values. `if`,
`show` and `text` own complete expressions. Both conditional expression arms and
both template branches contribute reads, including the inactive branch. Existing
attribute and nested component argument syntax retains its production contracts.
Calls and other conditions without direct static expression code generation fail
production validation with component/source context and remediation.

Supported observable writes in this slice target component-local `let` values:
root replacement, object members and computed array/object subscripts, including
arithmetic compound assignment. A write invalidates its path and observable root;
resolving a computed target also records its index reads. In-place member/index
writes conservatively invalidate every object-valued root in the same component
scope, so owned aliases cannot hide a mutation. Nested object-valued parameters
are conservatively dirty when their invocation updates. Scalar parameter equality
uses `Object.is`, including sign-sensitive zero changes.

Component parameters and action arguments are borrowed values. Production rejects
direct parameter writes, path mutations through their transitive local aliases,
and writes without a compiler-known local `let` root. Potential borrowed aliases
are propagated from right-hand expression dependencies, not from lvalue index
reads. Initializers/assignments without direct static expressions are treated as
potentially borrowed for path mutation. This intentionally conservative ownership
check can reject safe copies containing parameter-derived values; it is not a
full typed ownership analysis. Use owned local state or a parent action. Cross
component shared-object mutation is not silently accepted as a supported write.

## First incorrect layer and repairs

Expression IR already read `title` in `title == "After"`. Runtime Plan processed
that condition with the element attribute grammar: it consumed `title` as an
attribute and `==` as its value, producing `reads: []`. Complete-expression heads
now use Expression IR dependency extraction directly.

The static emitter now unions both template branches into the owning `if` region.
The synthetic `else` parent maps to that region during descendant suppression,
so a region replacement cannot be followed by a patch to an absent old branch.
Compiler/runtime dependency closure uses finite worklists instead of truncating
valid derived chains after a fixed number of passes. Descendant collection uses
a visited set. Loop collection dependencies, including transitive derived collection roots,
survive iterator shadowing; row-local
key dependencies are excluded in Runtime Plan rather than promoted to outer
observables.

The first full gate caught a compatibility update-plan budget regression after
restoring a shadowed collection root: row-local key reads had also been restored,
duplicating a large region entry. Correcting the scope of key reads resolved the
failure. No budget was changed. The final large-keyed-reorder compatibility asset
is 177847 bytes against its existing 180000-byte limit.

The repeated-action investigation initially suspected duplicate event dispatch.
Assignment stack traces showed separate intended iterations instead; no event
handling change was necessary. The additional real defect was object-reference
equality suppressing an in-place nested parameter mutation.

## Evidence at the initially reviewed head

`conditional-reactivity.jtml` is mandatory in
`scripts/test_production_reference.sh`. Its real browser scenario executes 28
actions and 86 assertions, independently changing title, count, boolean, fallback,
offset, limit, property, index and indexed property values. It also checks local
object aliases, root replacements, repeated branch transitions, derived values,
sibling isolation, stable component/unaffected DOM identity, registry ownership,
no patch fallback, and no WebSocket/fetch/XHR or browser errors.

The exact final fixture fails on the original PR #8 compiler at the first Advance:
the conditional text remains Before and the else-only fallback remains Fallback 0.
The repaired compiler passes it. CTests independently list required read sets and
check Expression IR, Runtime Plan and generated static selectors; additional tests
cover branch ownership, loop shadowing, observable write metadata, borrowed aliases,
unsupported conditions and accepted owned writes with parameter-supplied indices.

Validation on macOS arm64 with Release/C++17, Node 22, Python 3.12, Playwright
1.63.0 and pinned Chrome Headless Shell 153.0.8010.12:

- 341/341 CTests pass.
- Complete production reference suite passes, including nested-heading, routing,
  native forms, keyed actions and the mandatory conditional matrix.
- All nine real browser benchmarks pass with clean browser exit and unchanged
  interaction/asset budgets.
- `scripts/verify_all.sh` passes with mandatory browser execution and no skipped
  validation; live runtime smoke passes all eight checks.
- Shared runtime is 271796 bytes against its existing 272000-byte limit.

The composed reference app still reports its existing HTML size limitation
(135374 bytes against the 70000-byte microfixture comparison); this slice does
not fix manifest duplication. Source-span propagation, full type/ownership
analysis, the full v0.3 support matrix, fresh hosted CI and release readiness are
separate evidence. Local logs are ephemeral under `/tmp/jtml-conditional-*`.

## PR #10 review repairs (reviewed head `6384d34`)

Three independent review findings were reproduced before editing with the reviewed
compiler and clean production builds in real Chrome:

1. Increment -> Mutate -> Increment left the Counter at 1; an unrelated object
   write removed the still-visible child's dynamic generation. Static conditional
   patches always replaced their subtree. Worse, creation registered replacement
   children before destruction unregistered those same identities.
2. A child action iterated a borrowed parameter collection, copied a row into a
   local alias and mutated it. The child showed 1 while the parent remained at 0.
   The borrowing graph omitted iterator bindings and conflated lexical names.
3. `result = input.push(1)` and `items[mystery()].score = 1` both built successfully.
   The former rendered literal source text; the latter silently left state
   unchanged. Ownership checks had not validated the complete assignment shape.

Conditional creation now records its selected branch in both initial/runtime
rendering and generated static creation. On an unchanged branch, the compiler
emits calls to its generated descendant patch cases, preserving mounted child DOM,
state and generations while updating consumed values and nested parameters.
Containers patch their attributes and descendants; nested components and regions
retain ownership of their own subtrees. On a genuine branch transition, old
children are destroyed before new children are created and registered. Conservative
object invalidation is unchanged; extra scheduling no longer destroys an active
interactive branch.

Borrowing now follows lexical binding identities through component/action
parameters, local declarations, loop collections/iterators and transitive aliases,
including nested containers. An owned collection iterator may mutate its members;
an iterator over a borrowed collection may not. Loop iterators restore shadowed names, but nested declarations share persistent
runtime storage. The later scope-leak repair below rejects colliding nested
declarations rather than claiming lexical declaration shadowing is supported.
Component-scope parameter names remain borrowed even if redeclared because the
runtime refreshes those names from parameters before actions. The analysis remains flow-insensitive
and conservative: potentially borrowed reference copies can still be rejected.
No cross-component mutation fallback is introduced.

Every production assignment must have a parsed static RHS, a writable path whose
entire computed indices support static expression compilation, and one of `=`,
`+=`, `-=`, `*=`, `/=`, `%=`. Root ownership is checked against the resolved binding.
Unknown/side-effectful calls anywhere in the RHS or lvalue fail the clean build
with component/source context and remediation. Supported dynamic indices such as
`items[index].score = 1` remain accepted and execute correctly.

### Regression and negative-control evidence

- Mandatory `conditional-lifecycle`: 22 actions/32 assertions cover a constant
  child and a parameterized child, repeated unrelated and relevant writes,
  retained DOM/state/actions, true->false->true twice, fresh state on recreation,
  destruction flags and exact registry consistency. The same fixture built with
  `6384d34` fails on the first unrelated write at the identity assertion.
- Mandatory `owned-loop-writes`: 3 actions/5 assertions exercise direct owned
  iterator mutation, alias mutation and a dynamic-index update in real Chrome.
- New CTests cover borrowed component/action collections, nested loops and
  containers, transitive aliases, lexical shadowing, legitimate owned writes,
  supported operators, and rejection of complete unsupported assignments. CLI
  tests require clean-build rejection with the source path and component name.
- Negative control: rebuilding the three new CTests against the reviewed emitter
  and runtime produces 0/3 passing tests (CTest exit 8). Restoring the repairs
  makes all three pass. No assertion or budget was weakened.

Final local verification: 344/344 CTests; complete production reference suite
(including the existing 28-action/86-assertion dependency matrix); all nine real
browser benchmarks; production asset/CSP checks; and `scripts/verify_all.sh`,
including live smoke 8/8. No checks were skipped and all benchmark browsers exited
cleanly. Runtime is 271879/272000 bytes; large-keyed-reorder compatibility asset
remains 177847/180000 bytes. The existing reference HTML size limitation remains
explicitly reported. Logs and the saved reviewed binary are ephemeral under
`/tmp/jtml-pr10-review/`. Hosted CI is checked separately against the pushed head.


## Shared declaration scope repair (reviewed head `5a56947`)

The review's exact `scope-leak-rejected.jtml` compiles at `5a56947`; two real
browser clicks change the child's borrowed `input.score` to 1 and 2 while its
parent DOM stays at 0. The compiler resolves the write to the outer owned `item`,
but the runtime's nested `let item = input` has overwritten that same storage.

Scope tracing establishes these existing backend contracts:

- `componentScopeFor` creates persistent component storage and refreshes parameters
  on later entry. Expression lookup and assignment read/write this same object.
- `runComponentPlanStatements` shares that object across action, conditional and
  loop statements. Declarations assign directly into it. Action formal parameters
  and loop iterators save/restore their own names; local declarations do not.
- Ownership validation resolves lexical binding identities and propagates reference
  provenance from initializers, assignments and iterator collections. A declaration
  collision invalidates the assumed separation between those identities.
- The general language reference does not promise lexical block declarations.
  The live interpreter's `interpretIf` also executes in its current environment,
  while explicit block statements create an environment. The existing
  `FriendlyEffect.EffectBodyMutatesOuterVariableAndPropagates` test deliberately
  requires a nested `let` to update an outer value. A global lexical runtime
  conversion would therefore need a separate semantics decision.

The clean-production validator now rejects nested state/derived declarations
whose name is shared by any other binding in the component, including parameters,
iterators and declarations in sibling branches or separate actions. It reports the
component and source location, explains shared storage, and asks for a unique local
name before assets are emitted. Without a collision, a nested declaration cannot
overwrite the distinct owned/borrowed binding used by lexical ownership analysis.
The existing reference-provenance validation remains in force for unique locals.

This is a conservative supported-shape restriction, not a runtime semantics change.
Some harmless duplicate names are rejected; use distinct names. The formerly
accepted action-local `items` shadowing a component parameter is now explicitly
rejected; its owned-loop positive test uses `ownedItems` instead. Compiler/runtime
code generation, compatibility backends, asset budgets and fallback policy are
unchanged. This repair does not claim complete language-wide lexical scopes.

The exact exploit is a tracked negative CLI/production-reference regression:
compilation must fail with source/component remediation and no runnable assets.
It is intentionally not executed as a supposedly supported production app. The
real-browser `owned-loop-writes` scenario additionally executes the unique-name
remediation through nested conditionals and both branches, repeated owned writes,
unchanged borrowed child/parent values, retained component identity and registry,
and no fallback/network/errors (6 actions/14 assertions total). Compiler cases
cover deep nesting, else, borrowed aliases, loop collection leaks, parameters,
iterators, derived declarations and cross-branch/action collisions. The exact CLI
and compiler regressions both fail against `5a56947`; the production-reference
negative guard also fails against that saved compiler.

Final local verification of this repair: 346/346 CTests, the full production
reference suite, all nine required browser/asset/CSP benchmarks, and
`scripts/verify_all.sh` including live smoke 8/8. No validation was skipped and
all browsers exited cleanly. The prior lifecycle, borrowed-iterator and assignment
regressions remain passing. Runtime is unchanged at 271879/272000 bytes; the large
keyed compatibility plan remains 177847/180000 bytes. The reference HTML limitation
remains 135374 bytes versus its 70000-byte microfixture comparison. Local logs are
under `/tmp/jtml-pr10-review/scope-*`; hosted CI is separate evidence.
