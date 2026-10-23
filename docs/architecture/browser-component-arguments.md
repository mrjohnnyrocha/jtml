# Browser nested argument word boundaries

Baseline: merged PR33, `5998830`. The unchanged
`tests/conformance/fixtures/nested/known-grouped-argument.jtml` expects **202**:
parent count 100, Child receives `(count + 1)`, then multiplies by 2.
Frozen-main native rendered HTML is 202; clean-production browser DOM is NaN.

## First incorrect layer and repair

Friendly lowering retains the complete expanded arithmetic expression. Semantic
IR and Runtime Plan retain `Child (count + 1)`, its caller `count` read and the
correct binary Expression IR. The first loss is **Runtime Plan JSON word-plan
serialization**: its separate whitespace splitter emits four words/plans:
`Child`, `(count`, `+`, `1)`. The browser's existing balanced splitter reads two
words, but parameter evaluation consumes the fragment plan at index 1. The
legacy source evaluator returns that fragment as text, so multiplication is NaN.

`compileRuntimeWordPlans` now reuses the existing Friendly quote/group-aware
scanner. It emits two plans, with a typed binary expression at index 1, reading
`count` from the supplied caller scope. No parser, expression evaluator, scope
implementation, runtime JS or native interpreter is added or changed. Original
node source coordinates and provenance remain intact.

Production validation also requires nested arguments to satisfy the existing
Expression IR/direct-JS contract. `Child unknown(count)` previously emitted
runnable assets; it now rejects before all four assets, identifying Probe,
Child/value and original file/line/column, with source-first remediation.

## Independent evidence and scope

- Original source and 202 oracle unchanged: native/browser **202/202** after fix.
  The unchanged catalog first rejected this unexpected resolution. Its old
  signature is retained in `history/pr33-nested-known-discrepancies.json`.
- One new authored fixture combines grouping, arithmetic, computed indices,
  ternaries, shadowed child count, repeated parent actions, separate siblings and
  retained nested wrappers/text. Numeric addition exposes string concatenation;
  expectations are authored numbers, not compiler-generated expectations.
  Native expanded child state, native HTML and real CSP-restricted browser DOM
  pass independently. Frozen main fails its browser oracle while native passes.
- Compiler regression: complete grouped/indexed/object/escaped-string argument
  plans, caller type environment and dependency reads. Separate CLI regression:
  unknown-call rejection at original `index.jtml:6:3`, no runnable assets.
- Injecting a new failure into the resolved case is rejected by the catalog.
  No independent expectations, original sources or remaining signatures changed.

**One of two supplemental cases resolves; one remains.**
`known-global-parameter-alias.jtml` still gives native 20/browser NaN against 200.
This is a different top-level binding boundary: serialized instance parameters
contain the expression text `count`; browser `scanComponentInstances` and
`componentScopeFor` copy it as a value instead of evaluating caller bindings.
The native public-alias failure remains separately reproducible. This PR does
not alter either path or claim general top-level argument correctness. Strict
nested conformance still exits **2**. Five separate native complete-attribute
renderer observations remain FAIL / KNOWN DEFECT.

## Verification and review

Focused tests: **221/221 PASS**. Full `scripts/verify_all.sh`: **PASS**, including
**439/439 CTests**, all nine `browserExit=clean` benchmarks, production reference,
Taskboard C01–C14, strict N04, original 22 cases, ternary/logical/unary, lifecycle,
security/async, packaging/LSP/editor and eight live smoke checks; no test/tool
skips. Runtime unchanged at **271633/272000 bytes**, SHA256
`da88340a15ab9fb1557d67f8c6109f7c376fe7e2ac523b05dcef03020d86b011`.
Playwright 1.63.0 / Chromium 153.0.8010.12. Exact-head Ubuntu/macOS CI results
are recorded in the PR; hosted CI is distinct from the complete local gate.

Reproduce using the existing tooling environment:

```sh
ctest --test-dir build -R 'Friendly|RuntimePlan|ExpressionIr|UnsupportedNestedArgument' --output-on-failure
node tests/browser/nested_environments.cjs --strict  # exit 2: remaining global defect
JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1 scripts/verify_all.sh
```

Critical review: sharing the scanner changes all word-plan token boundaries,
so strings, delimiters, attributes/events and multiple arguments require existing
frontend/production regressions, not only the 202 fixture. Validation intentionally
rejects unsupported nested expressions instead of trusting source fallback.
Linked/module child IDs determine the correct callee arity. Existing lexical
ownership, keyed identity, CSP, diagnostics and budgets remain mandatory. The
remaining top-level global binding defect requires a separate focused repair.
