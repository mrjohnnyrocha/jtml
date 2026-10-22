# Native logical expression semantics

Baseline: merged PR27/main `8af2c95`, clean isolated worktree. The unchanged
22-case conformance matrix reproduced **14 equivalent / 8 FAIL / KNOWN DEFECT**.
The original source, authored expectations and exact before/after native/browser
traces are preserved in [evidence.json](evidence.json). Frozen baseline binaries
and full logs are under `/tmp/jtml-logical-*`; their hashes are recorded.

## Contract and earliest incorrect layer

Expression IR's `binaryResultType` classifies logical operators as Boolean;
`VarValue` documents and implements a distinct bool alternative, as do numeric
comparisons. Independent Boolean truth tables specify the expected results.
Native `evaluateExpression` instead passed non-null C-string pointers spelling
"true" or "false" to `VarValue`. Overload resolution prefers pointer-to-bool
conversion over the user-defined std::string conversion. The constructor control
proves that `VarValue("false")` is bool true while `VarValue(false)` is bool false.
AST/Expression IR and compiler reads are already correct; evaluation is the first
incorrect layer. Adding a global C-string overload would affect unrelated callers
and is unnecessary.

`!` now constructs the actual negated bool. `&&` evaluates the left operand once,
returns false immediately when false, otherwise evaluates and converts the right
operand once. `||` returns true immediately when the left is true, otherwise
converts the right. Inactive RHS expressions never execute. Nonlogical operators
retain their existing left-to-right evaluation. No parser, lowering, source map,
component environment, function-call ownership, execution lock or worker changes.

Native compatibility truthiness is deliberately preserved: exact "true"/"false",
then existing `stod` conversion, then empty/nonempty strings. Boolean literals'
existing native string representation is also unchanged. Arbitrary mixed-type
native/JavaScript coercion and operand-returning JavaScript &&/|| behavior are
**UNVERIFIED cross-backend contracts**, not silently normalized by this repair.
The equivalent browser/native slice uses boolean-valued operands, comparisons
and logical expressions. Native-only compatibility tests pin existing coercions
without asserting universal backend parity.

Dependency collection remains independent: `gatherDeps` visits **both** binary
operands and all ternary children without executing functions or index arithmetic.
A newly lazy evaluation must not remove inactive dependencies. Explicit native
read-set checks, compiler metadata/provenance checks and repeated event traces
verify this boundary. General function-body dependency inference remains outside
this bounded contract.

## Reproductions and independent controls

- Minimal Friendly originals: `tests/conformance/fixtures/logical/logical-not.jtml`,
  `logical-and.jtml`, `logical-or.jtml`. Old native !true, false && true and
  false || false all give true; repaired native and production browser give false.
- Seven C++ cases: truth tables/types, actual overload selection, compatibility
  truthiness, skipped versus active errors, ordered/exactly-once function effects,
  conservative reactive dependencies and an inactive computed index dividing by
  zero. Baseline **1 PASS / 6 FAIL**; repair **7 PASS**. Direct active division-by-zero
  controls must throw. Quiescent borrowed environment reads introduce no workers.
- Eight new original Friendly fixtures compare native values, inert native HTML,
  production DOM and actual clicks, with authored truth tables/state transitions.
  Baseline **8 FAIL**; repair **8 PASS**. Cases cover reactive logical operations,
  guarded zero division, nested ternaries, owned member aliases, computed indices
  and sibling isolation. CSP, source provenance, no networking/fallbacks and asset
  budget assertions come from the unchanged runner. Browsers close in finally.
- Side-effecting function expression controls use the native supported API; they
  do not add unsupported production function-expression calls or pretend browser
  side-effect parity was tested. Browser guard cases verify public DOM outcomes;
  native active-error controls separately prove arithmetic errors are observable.

Commands (with the existing pinned Playwright/Chromium environment):

```sh
ctest --test-dir build -R '^(NativeLogical|NativeTernaryDependencies|InterpreterCalls)\.' --output-on-failure
scripts/test_semantic_conformance.sh
scripts/test_semantic_conformance.sh --strict
node tests/conformance/self_test.cjs
scripts/test_source_provenance.sh
JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1 scripts/verify_all.sh
```

The full gate and both CI platforms invoke the new browser/native fixtures through
`test_semantic_conformance.sh`; existing stages remain intact. All existing
function-call, ternary, lifecycle, security, async and production gates remain.

## Catalog accounting and critical review

Exactly **two** previously catalogued cases resolve completely:
`branch-dependencies` now follows 2,2,5,11,11,13,8; `boolean-comparison` follows
no,no,yes,no,no. Their original sources and independently authored expectations
are unchanged. The unchanged old catalog correctly rejected those unexpected
resolutions before manual review. Its full content is archived as
`tests/conformance/history/pr27-known-discrepancies.json`.

The original 22-case matrix is now **16 equivalent / 6 FAIL / KNOWN DEFECT**.
All six remaining signatures are exactly unchanged: unary-negative, three seeded
unary-negative cases, and two nested-native-scope diagnostic cases. Strict mode
must exit 2. The separate complete-attribute native-renderer defect retains all
five failure observations in the unchanged 40-case attribute suite (35 equivalent).
No catalogued defect becomes a pass merely because catalog verification succeeds.
Eight new logical cases and eight existing ternary cases are additional passes,
not additions to the original 22-case denominator.

Oracle self-tests reject backend agreement on wrong outcomes, type drift, missing
values and missing dependencies. Deliberate injection into actual repaired reports
proves the maintained catalog rejects a newly introduced failure in a resolved
case and a changed expectation for a still-failing case.

Review found no new ownership, scope, provenance or synchronization mechanism:
only logical evaluation/result construction changes. Existing locks span recursive
expression evaluation exactly as before; no wait/join is introduced. Short-circuit
execution is intentionally different from conservative dependency traversal.
Known unary numeric representation, nested environment diagnostics and native
attribute rendering remain unresolved. Historical Classic HTTP reset issue #26
remains UNVERIFIED and is not claimed fixed.

## Final local validation

[validation.json](validation.json) records the frozen-source full gate: **414/414
CTests**, all **nine benchmarks with clean exits**, strict N04 plus diagnostic
provenance checks, production reference, Taskboard, lifecycle, security, async,
literal, package/site/LSP and **8/8 live smoke** PASS, with no verification skips.
The reference-app browser check also exits cleanly. Shared browser runtime is
byte-identical before/after: **271564/272000 bytes**, SHA256
`e8ff5f35ac6ce187abfc9ce5686da8b47af5b4bea4949336ef5985ae0fb84b17`.
No budgets or concurrency expectations changed. Strict conformance exits **2**
for the six preserved genuine defects. Exact final-head Ubuntu/macOS hosted
results are recorded on the PR separately; local gate success does not imply CI.
