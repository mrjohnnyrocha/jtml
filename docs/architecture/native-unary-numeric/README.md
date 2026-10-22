# Native unary numeric repair

Baseline: clean merged PR28/main `44ef526`. The unchanged conformance matrix
reproduced **16 equivalent / 6 FAIL / KNOWN DEFECT**. Original expectations and
Friendly sources are unchanged. [evidence.json](evidence.json) preserves the four
resolved sources and complete before/after native/browser traces, new adversarial
traces, original minimization and frozen binary hashes. Logs/binaries remain under
`/tmp/jtml-unary-*` as supplementary, temporary evidence.

## First incorrect runtime layer and bounded repair

The parser correctly builds unary-minus AST nodes with numeric children. Native
`evaluateExpression` then converts even numeric operands through `toString()` /
`stod`, losing precision through display formatting, and constructs the result
from `std::to_string`, making it a string. Subsequent + concatenates, while */-
reject numeric arithmetic on that string. Parsing is not the owning defect.

Numeric operands now use `getNumber()` directly and the negated double constructs
a numeric VarValue. This preserves the actual double, including signed zero,
without formatting either input or result. VarValue, parser, generic evaluator,
logical evaluation, gatherDeps, environments, locks and async workers are unchanged.

For nonnumeric operands the same warning, `stod(toString())` conversion and
`Invalid numeric format in unary '-'` error remain. Existing numeric strings
still convert with a warning; arbitrary mixed-type coercion is not newly defined.
Native-only invalid boolean/empty string/nonnumeric string/array/object probes
verify exact diagnostics. No broad constructor or coercion rule changes.

Seven typed C++ cases cover direct/repeated/grouped negation, negative literals,
nested arithmetic, fractions, signed zero via getNumber/signbit, full precision
host-injected doubles, derived/ternary updates, AST/IR limits and invalid operands.
Corrected tests linked against frozen source-identical PR28 core: **1 PASS / 6
FAIL**; repaired **7 PASS**. The first precision probe incorrectly assigned an
undeclared host variable; that setup failure was corrected and is not counted as
runtime evidence. The saved pre-fix core comes from `7ed1853`, whose src/include
contents match merged baseline `44ef526`; tests use identical compiled objects.
Focused unary/logical/ternary/function-call checks pass **28/28** after repair.

## Exact original catalog accounting

**Four of six** original catalogued cases now fully pass both independently
specified native values/DOM and production browser state/DOM: unary-negative,
generated-24-0, generated-24-1 and generated-24-7. Original matrix:
**20 equivalent / 2 FAIL / KNOWN DEFECT**, still 22 cases. The two nested-environment
diagnostic signatures are byte-for-byte unchanged; strict mode still exits 2.
The prior catalog is archived in `tests/conformance/history/pr28-known-discrepancies.json`.
The unchanged catalog first rejected the four resolutions; no expectations or
actual traces were automatically blessed.

The reducer previously asserted this unary failure must exist forever. It now
checks the same seed and executes the original minimized negative-number source
as a passing control after resolution. If the seed still fails, the original
failure-preserving reduction runs and rejects an unexpected failure class. It
never reports an invented successful minimization. Historical reduction remains
in evidence; catalog guards still reject unexpected resolutions and new failures.

## New compiler/browser evidence: not repaired or counted as passes

Ten new authored cases run against both backends. Frozen main fails all ten on
native semantics. The repaired native passes **all ten values and native DOM**.
Only **two** are fully equivalent: negative initialization/reactivity and integer
arithmetic. The other **eight remain FAIL / KNOWN DEFECT**, independently exposing
an existing compiler/browser unary-minus gap (eight observations, not eight new
root causes). Repeated/grouped/derived unary expressions can become source-text
strings or NaN; dynamic unary roots also lose Runtime Plan reads. The pre-fix
browser/plan failures are exactly unchanged by this native repair.

Expression IR currently returns Source/Unknown for unary minus, while a numeric
literal operand and binary subtraction control type as Number. Native tests make
that distinction explicit instead of claiming the IR typed unary result is String
or Number. Clean compilation nevertheless accepts derived `-x` and repeated
negation and produces incorrect browser values. Compilation success is not proof
of supported semantic execution. Missing IR support and source-expression emission
need a separate compiler/static-backend repair with the same oracles. Do not
normalize these outcomes or claim complete unary browser/native conformance.

The exact reviewed failures are pinned in
`tests/conformance/unary-browser-known-discrepancies.json`. Catalog mode verifies
their source/oracle hashes and full signatures, printing FAIL / KNOWN DEFECT.
Unknown failures, native regressions, changed signatures or unexpected resolutions
abort. `node tests/browser/unary_numeric.cjs --strict` exits 2 for these failures.
Actual-report fault injection proves a new native failure cannot hide alongside
a known browser failure; original-catalog fault injection likewise detects a new
resolved-case failure and changed expectations.

Unary action assignments (`x = -x`, even negative literal assignments) already
fail closed in production. The initial new probe confirmed that boundary; the
permanent unsupported-unary-assignment fixture now requires exact original
file/line/column, remediation and no runnable output. Shared reactive fixtures
use supported binary writes such as `0 - x`; native C++ separately exercises
actual unary assignments. No language support is broadened to make the suite pass.

Signed-zero type/sign is verified directly in native C++. Native ostream prints
-0 whereas JS String prints 0; raw signed-zero display equivalence is unverified.
The public zero fixture explicitly adds numeric zero for its displayed result,
and tests restored positive zero independently. Arbitrary-precision display parity
and nonnumeric browser/native coercion remain unverified.

The pre-existing supplemental native complete-attribute renderer defect is also
unchanged: **35 equivalent / 5 FAIL / KNOWN DEFECT observations** in its 40-case
suite. Nested scope diagnostics are not conflated with incorrect parameter values.
Historical Classic HTTP response reset issue #26 remains UNVERIFIED.

## Reproduction and critical review

With pinned Playwright/Chromium configured:

```sh
ctest --test-dir build -R '^(NativeUnaryNumeric|NativeLogical|NativeTernaryDependencies|InterpreterCalls)\.' --output-on-failure
scripts/test_semantic_conformance.sh
scripts/test_semantic_conformance.sh --strict
node tests/browser/unary_numeric.cjs --strict
node tests/conformance/self_test.cjs
JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1 scripts/verify_all.sh
```

Critical review: the production diff is confined to unary-minus input/result
construction. No parsing, dependency, ownership, scope, source attribution,
synchronization or browser behavior changes. Number-preserving negation also
prevents repeated operations from entering compatibility string conversion.
Nonnumeric conversion and its diagnostics remain explicit. Conservative reads
and short-circuit execution retain PR27/28 coverage. The new browser failures are
proven on frozen main and stay visible; they are follow-up work, not repaired by
changing this PR's scope. Local and exact-head hosted validation are separate.

## Final local validation

[validation.json](validation.json): full `verify_all.sh` PASS with **421/421 CTests**,
all **nine benchmarks clean-exit**, production references, strict N04/source
provenance, Taskboard, existing logical/ternary/lifecycle/security/async/literal
regressions, package/site/LSP checks and **8/8 live smoke**, no verification skips.
Original strict and supplemental strict each exit **2**, for their separately
catalogued real defects. Shared runtime is byte-identical **271564/272000 bytes**;
no browser assets or budgets change. Exact-head Ubuntu/macOS CI is reported on
the PR separately. A green catalog gate is not zero semantic discrepancies.
