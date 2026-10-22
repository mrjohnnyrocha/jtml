# Differential semantic conformance — first bounded contract

This harness compares original Friendly input, compiler Semantic IR / Expression
IR / Runtime Plan metadata, clean-production Chromium, and the actual native
Interpreter via public snapshot/component-action APIs. It does not substitute a
new interpreter. Native HTML is parsed inertly by DOMParser; its scripts never run.

## Observable matrix

| Feature | Classification and observation |
| --- | --- |
| Finite integer initialization, +, *, ordered assignments | Equivalent in six bounded cases; exact independent numeric traces |
| String literals/escaping and owned member aliases | Equivalent in the authored cases; exact DOM and state traces |
| Conditional action/rendering | Equivalent in the bounded numeric cases |
| Component-local sibling isolation | Equivalent in the ordered-action fixture |
| Derived ternaries | Supported contract, KNOWN DEFECT: native derived result stays stale |
| Grouped expressions and computed local indices | Supported contract, KNOWN DEFECT: Friendly expansion loses local identifiers |
| Unary numeric negation | Supported numeric contract, KNOWN DEFECT: native string changes later addition |
| Nested parameter values | Values agree; KNOWN DEFECT: native reports erroneous scope errors |
| Event delivery | Intentionally different interfaces: real DOM click vs public native action dispatch; compare completed named-action effects, not transport IDs |
| Keyed state, unsaved input, cleanup, remount | Browser PASS; native retained DOM/input identity UNVERIFIED, not fabricated or counted equivalent |
| Borrowed writes / unknown target calls | UNSUPPORTED clean-production shapes; require exact original diagnostic and no runnable assets. Native acceptance is not compared |
| Boolean representation/coercion, short circuit side effects | UNVERIFIED; boolean expressions are exercised as conditions, not universal coercion/type parity |
| Async, HTTP/WS transport parity, emitted payload ordering, routing, fetch, stores, arbitrary errors | UNVERIFIED in this harness; existing gates remain mandatory |

Language authority: docs/reference/language-reference.md documents expressions,
reactive `get`, components and local ownership; ARCHITECTURE.md requires retained
nested/keyed identity and compiler-owned dependencies. The oracle uses explicit
hand-calculated traces, plus a finite integer grammar `(x +/- a) * b` whose
expected results are host integer arithmetic. Compiler plans never supply expected
values. No claim is made about unspecified mixed-type coercions.

## Running and interpretation

Build with JTML_BUILD_TESTS=ON, Python bindings off. Node, Python3 and the same
Playwright 1.63.0 / Chromium tooling required by existing CI are needed.

```
scripts/test_semantic_conformance.sh
scripts/test_semantic_conformance.sh --strict
node tests/conformance/self_test.cjs
```

The default verifies the exact reviewed defect catalog. Every discrepancy is
printed FAIL / KNOWN DEFECT CANDIDATE and remains a semantic failure. Strict mode
exits 2 while discrepancies remain. Catalog mode exits zero only when all
expected failures reproduce exactly and all other observations pass. New failures,
changed signatures, changed source/oracle hashes, and resolutions fail review.
There is no automatic blessing command. An expectation fault injection, new
browser failure and unexpected resolution are independently rejected in CTest.
A real source mutation also makes both backends agree on a wrong answer: the
independent oracle rejects both. This is not counted as a semantic case.

Twenty-two differential cases (14 authored, eight seed-24 grammar cases), one
browser-only lifecycle case and two fail-closed cases form the 25-case matrix.
Six differential cases pass; sixteen reproduce reviewed defects. Compiler read /
write supersets, original file/line/column and exact range assertions are checked
separately. Over-approximate dependencies are accepted; missing dependencies fail.

Browser DOM text/events are the public semantic oracle. `window.jtml` scope
snapshots supplement it with type/ownership diagnostics; generated names and IDs
are not compared. Registry checks supplement independent DOM identity, detached
node and remount assertions. Native getStateJSON is a public diagnostic interface;
its rendering can materialize nested components, so unrestricted snapshot purity
is not claimed. Numeric values, numeric strings and missing fields remain distinct.

Each event is followed by two animation frames, never polling for the expected
answer. This slice supports synchronous actions only. Each native/compile child
has a 20-second timeout; the whole runner has a 240-second process-group timeout
which kills child browsers on expiry. Normal close is awaited. CSP forbids dynamic
code and network access; external asset requests, exceptions, CSP violations,
compatibility assets/fallback flags and the 272000-byte budget are checked.

Evidence under dist/conformance includes original inputs, requests, full native
logs, normalized traces, lowered source, compiler plans, rejection diagnostics,
minimization attempts and failure.json on abort. CI uploads these after execution;
upload failures cannot replace the original test result. Native instance numbers
are normalized only in diagnostic signatures; raw diagnostics are retained.

## Confirmed counterexamples and independent follow-ups

1. `fixtures/grouped-local-minimal.jtml`: `let x = 0; get result = (x)` must
   produce 0. Native loses result and reports undefined x; browser gives 0.
   `ungrouped-local-control.jtml` gives 0 in both. Earliest layer:
   src/friendly.cpp substituteIdentifiers only replaces complete/prefix tokens,
   missing identifiers inside delimiters. Indexed local names share this mechanism.
   Follow-up: AST-aware Friendly component binding substitution.
2. `fixtures/ternary-minimal.jtml`: result must change 1 -> 2 after step.
   Browser does; native stays 1. Lowered names and plan reads are correct;
   src/interpreter.cpp gatherDeps omits Conditional expressions. Follow-up:
   conservative native conditional dependency traversal.
3. `fixtures/unary-negative.jtml`: numeric -1 plus 1 must be numeric 0.
   Native stores a string and concatenates; browser yields 0. Earliest layer:
   Interpreter::evaluateExpression Unary '-' returns std::to_string in VarValue.
   Follow-up: preserve numeric unary result type.
4. `fixtures/nested-parameter-minimal.jtml`: child result is 4 in both, but native
   reports undefined parent state during expanded execution. Nested environments
   attach to globalEnv in Interpreter::attachComponentEnvironment, losing the
   expanded parent's lookup chain. Follow-up: review native nested environment
   ownership without conflating these diagnostics with incorrect rendered values.

Run the strict command above to reproduce all four classes. Each authored source
is permanently checked in under tests/conformance/fixtures. The catalog pins exact
actual traces and signatures; normative expected traces live separately in cases.cjs.
The seed-24 reducer removes events, constants and expression grammar forms while
requiring missing native x, the same diagnostic and no browser/plan failure. It
converges to `(x)` in ten attempts, nine accepted; `x` is the rejecting control.
Minimality is only with respect to these finite reduction moves, not all JTML.

No compiler/interpreter/runtime repairs are included. Existing T0, provenance,
async lifetime, native synchronization, CSP and performance gates are preserved.
