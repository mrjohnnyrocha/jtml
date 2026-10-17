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

## Evidence

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
