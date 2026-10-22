# Native ternary dependency repair

Baseline is clean merged main a7ade6f (PR25 included), on branch
codex/native-ternary-dependencies. Original and resumed build logs remain in
/tmp/jtml-ternary-*.log; baseline executables are frozen separately under
/tmp/jtml-ternary-before/bin. No existing worktree was recreated.

## First incorrect layer

The original Friendly ternary-minimal fixture defines x=1 and result=true ? x : 0,
then increments x. Its authored expected/native DOM trace is 1 -> 2. Baseline
native state/DOM stays 1 -> 1, while production state/DOM gives 1 -> 2. The
ungrouped/grouped controls and the focused x+0 control pass on the baseline.
[evidence.json](evidence.json) preserves exact before/after traces and binary hashes.

Parser AST Conditional already has condition/whenTrue/whenFalse children.
Expression IR builds all three, and its generic dependency walk visits all
children. Semantic IR collectExpressionRefs also visits all three. The native
Interpreter gatherDeps switch omitted Conditional entirely and fell through its
empty default, losing the reads before Environment::deriveVariable stored its
CompositeKeys and added reverse invalidation edges. Assignment marks the existing
graph dirty and updateVariable reevaluates the saved expression; those layers
work when the missing edges are supplied.

The production edit adds one case to that existing native AST traversal. It
recurses into condition, true branch and false branch using the same Environment.
It does not parse source, evaluate branches, inspect user function bodies, change
ownership, replace the evaluator, add locks, or create new semantic machinery.
Variable lookup in the existing collector reads current values; property/index
collection visits expression nodes without evaluating indices. Conditional
execution still evaluates only the selected branch in evaluateExpression.

## Bounded guarantees

| Contract | Evidence |
| --- | --- |
| Condition and both branches remain dependencies | Native dependency-key assertions plus inactive-branch writes and repeated toggles |
| Nested ternaries and grouping | Native and production traces; independent compiler read/provenance checks |
| Member roots, owned aliases and computed indices | Native C++ and browser/native fixtures, including a conditional used as an index |
| Derived invalidation chains | Saved AST graph recomputes choice then result |
| No branch execution during collection | Both inactive function-call arms retain calls=0; an inactive index containing division by zero is not evaluated |
| Component ownership and sibling/string-parameter isolation | Two-instance event traces; unchanged rejection/provenance gates |
| Original source and fail-closed diagnostics | Unchanged CLI source mapping and strict N04/source diagnostic checks |

Eight new fixtures live as directly runnable original Friendly sources in
 tests/conformance/fixtures/ternary/. Their independently authored state tables
are in ternary_cases.cjs; equality checks prevent source drift. All eight fail
against the frozen baseline with native-only discrepancies, and pass after the
repair. Eight C++ cases cover dependency edges and evaluation invariants; the first
seven were run before the repair and all failed. The existing nonconditional
control inside the minimal test passed before and after.

The new event tests use explicit true/false assignments to isolate subscription
correctness from the separately observed native !/&&/|| value defect. They do not
replace or weaken the original logical fixtures. A first draft named a displayed
value selected, colliding with boolean HTML attribute syntax; it was corrected to
choice while retaining the same independent numeric outcomes. A numeric parameter
trial exposed a supplemental native/browser scope-representation difference; its
shared type contract remains UNVERIFIED, so the final isolation fixture uses
quoted string parameters. Those trial failure logs/traces remain under /tmp.

## Exact catalog maintenance

The unchanged initial matrix: 13 equivalent / nine known defects. After repair:
**14 equivalent / eight known defects**, out of the same 22 original cases.
Exactly **one complete case resolves: ternary-minimal**. Its original source and
normative expected outcomes are unchanged. The full old catalog is retained in
 tests/conformance/history/pr25-known-discrepancies.json.

branch-dependencies improves: selected x writes now invalidate result. It still
fails because !enabled produces true instead of false. Its failure remains
**FAIL / KNOWN DEFECT**; only that reviewed partial-resolution trace changes,
with the same source/oracle hashes. boolean-comparison has the same failing trace
as before and is reclassified more accurately. Literal controls !true,
false && true, and false || false all independently produce native true instead
of false even without observable dependencies. Inspection identifies const char*
to bool constructor selection in evaluateExpression's logical result creation.
This is a separate evaluation defect, not another missing dependency edge.
It is not repaired in this focused PR, and no unary-negative behavior changes.

The other six signatures are unchanged: four unary-number/type cases and two
nested-environment diagnostics cases. The five existing supplemental observations
of one native complete-attribute renderer defect also remain explicit failures.
None are semantic passes. Strict mode must remain nonzero. Catalog fault-injection
controls must continue rejecting changed expectations, new failures and unexpected
resolutions; no expectation or source file from the original matrix was regenerated.

## Limits and critical review

This restores conditional traversal for the existing statically traversable
expression subset. It does not establish arbitrary native function-body/free-read
dependency inference or unrestricted interpreter concurrency safety. Impure native
function calls are used only to assert that collection itself never executes
branches; this is not a new production function-call feature. Logical &&/|| eager
execution/value semantics, numeric parameter scope representation and the existing
renderer/environment failures are not silently claimed fixed. Unsupported clean
production shapes keep existing fail-closed checks.

The reviewer checked the entire production diff: one ten-line AST case, no
changes to evaluation, Environment storage/lookup, compiler plans, source mapping,
server admission/security, function frames, worker lifetimes, locks or waits.
The same-instance CompositeKey policy and existing lexical ownership restrictions
remain intact. Unioning inactive-branch reads may cause conservative extra updates;
it intentionally avoids branch-dependent stale subscriptions. Existing arrays'
extra path keys are preserved, not redesigned. Browser assets and budgets are
unchanged source-wise. Security/async gates and real browser tests remain required.

Reproduce with the pinned Playwright/Chromium environment used by CI:

```sh
ctest --test-dir build -R NativeTernaryDependencies --output-on-failure
scripts/test_semantic_conformance.sh
scripts/test_semantic_conformance.sh --strict # must exit 2 while defects remain
scripts/test_source_provenance.sh
JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1 scripts/verify_all.sh
```

The supplemental runner is part of the existing 240-second owned subprocess gate;
it writes partial reports, asserts independent results and always closes the browser.
This PR does not resolve or attribute the historical Classic reset in issue #26.
Final local and exact-head hosted results are reported on the PR.

## Final local validation

Full /tmp/jtml-ternary-full-gate.log: PASS, including 407/407 CTests, production
reference, Taskboard, CSP/lifecycle, real HTTP/WebSocket security and async tests,
nine mandatory benchmarks with clean browser exits, packaging and all remaining
predeploy stages. Runtime stays 271564 / 272000 bytes; no budget changes.
Strict N04 and seven adversarial source-diagnostic cases pass. CI example lint
passes with its four unchanged explicit exclusions. Strict conformance exits 2
as required for the eight unresolved original cases.

The real updated catalog was fault-injected after maintenance: changing a newly
resolved-case expectation, changing an unresolved-case expectation, and adding a
browser failure are all rejected. Evidence: /tmp/jtml-ternary-catalog-injection.log;
the permanent generic catalog fault-injection self-tests also pass. The unmodified
post-fix catalog initially failed, as designed, and is preserved separately in
/tmp/jtml-ternary-after-unreviewed.log. The final reviewed catalog passes without
changing any original normative expectations.

See validation.json for counts and the full-gate log hash. Hosted Ubuntu/macOS
results must refer to the exact published head and are reported on the PR.
Recommended independent follow-up: repair native logical-result construction
(and separately establish &&/|| short-circuit execution), with its own negative
controls. That work must not repair or mask the unrelated unary-number,
nested-environment or renderer failures by changing expectations.
