# Compiler-owned unary minus

Baseline: clean merged PR29/main `ca8df4f`. Original sources and independent
expectations remain in [unary_cases.cjs](../../tests/conformance/unary_cases.cjs)
and its checked Friendly fixtures. The prior complete traces/failure signatures
remain in [PR29 evidence](native-unary-numeric/evidence.json); they are not copied.

## Causal path and repair

For `get result = -x`, baseline Expression IR is Source/Unknown; Runtime Plan
reads are `["-x"]`, and browser result is the string `"-x"`. Nested minus can
produce strings such as `"--1.000000000000000"`, NaN, and missing invalidation.
AST parsing and original-source lowering are correct. The first incorrect layer
is `buildExpressionIr`, which discarded the unary operand by returning Source.

Unary `-` now has a Number-valued Unary node and its existing operand child.
The ordinary dependency walker supplies roots through grouping, members, computed
indices and nested/conditional expressions. The same example now has `kind:unary`,
`operator:-`, `valueType:number`, reads `["x"]` and direct JS `-(scope["x"])`
(with the existing null-safe path wrapper). No new source scanner/parser is used.

Direct Expression IR emission now selects the validated operator instead of
hardcoding `!`. Existing static plan helpers and the browser's compiled-plan
consumer recognize the same two operators. Neither a compatibility fallback nor
dynamic evaluation is added. Native unary semantics and interpreter code are
unchanged. This establishes numeric negation, not general mixed-type coercion
parity or a new typechecker.

## Independent accounting and new boundaries

| Existing supplemental case | Baseline defect | After |
| --- | --- | --- |
| unary-direct | repeated negation becomes source text | PASS |
| unary-fractions | source strings and missing x read | PASS |
| unary-grouped-arithmetic | NaN and missing roots | PASS |
| unary-derived-chain | source string / NaN, missing root | PASS |
| unary-ternary-logical | source strings and wrong condition | PASS |
| unary-owned-index | source string and missing roots | PASS |
| unary-zero | repeated negation becomes source text | PASS |
| unary-sibling-isolation | source strings and missing root | PASS |

**8/8 catalogued supplemental failures resolve; 0 remain.** The unchanged old
catalog first rejected all eight unexpected resolutions. Individual source hashes,
authored numeric expectations, native/browser DOM/state and compiler reads were
reviewed before removing entries. IR read checks are strengthened; original ten
sources and numeric expectations are unchanged. The two original passing cases
remain passes, plus one new action case: **11 equivalent unary cases**.

Supporting unary IR naturally repairs the former rejection of numeric `x = -x`.
The old rejection source is preserved unchanged and now must compile. A new real
native/browser action fixture checks repeated mutations/derived updates. A separate
`x = -unknown(x)` fixture must still reject at original line 5, column 5, with
remediation and no runnable assets. Unknown operator and unsupported-child C++
controls preserve operator allowlisting. Two new C++ tests fail on baseline;
focused compiler/native logical/ternary/unary tests pass **24/24**. An initial
switch-placement mistake was caught by the new subscript test and corrected before
full validation; member/subscript direct support remains intact.

Existing original matrix stays **20 equivalent / 2 FAIL / KNOWN DEFECT**. Its
nested-environment signatures are unchanged and global strict mode must exit 2.
The separate native renderer remains **35 equivalent / 5 known failure observations**.
No known failure is counted as a semantic pass. Actual-report fault injection
rejects new browser, native and compiler-plan failures after catalog maintenance.

## Reproduction, evidence and critical review

Run `scripts/test_semantic_conformance.sh`; global strict reproduction is
`scripts/test_semantic_conformance.sh --strict`. Focused compiler controls:
`ctest --test-dir build -R '^RuntimePlan.Unary' --output-on-failure`.
Baseline/fixed generated plans, JS and traces are under
`/tmp/jtml-compiler-unary-{baseline,after-unreviewed,final-focused}`; full logs use
`/tmp/jtml-compiler-unary-*.log`. CI retains the standard dist/conformance artifacts.

Review: source spans still come from the same AST; no parser/lowering, environment,
ownership, native runtime, synchronization or worker changes. Unary results are
scalar copies; borrowed write/call validation remains required. Both plan consumers
must agree on operator selection. Unknown operators/children remain non-direct;
logical ! behavior and member/index recursion are retained. Mixed-type parity,
raw signed-zero display parity and historical HTTP-reset issue #26 remain unverified.
No separate new semantic defect was found during this scoped repair.

## Final local verification

One stable-candidate `verify_all.sh` run: **PASS**, **423/423 CTests**, all **nine
clean-exit benchmarks** (plus reference-app), production reference, strict N04 /
source diagnostics, Taskboard, conformance, lifecycle, security, async, literal,
package/site/LSP checks and live smoke **8/8**. No verification skips or retries.
Shared runtime **271633/272000 bytes** (+69, budget unchanged). Global strict
conformance exits **2** for the two unchanged native diagnostic cases; the five
existing supplemental native renderer failure observations remain visible.
Exact final-head Ubuntu/macOS results are recorded on the PR separately.

Full local gate log SHA256: `941906870ca842c0a9907cc854a39e9d6ad804fad5523b070a3be92deec40fec`.
