# Friendly component argument capture

Baseline: merged PR #31, `188f6b9`. Scope: Friendly lowering only; native
Environment, interpreter, browser runtime, independent original fixtures and
expectations are unchanged.

## Cause and smallest repair

Expansion recursively lowered the callee body, then applied parameter
substitution and local renaming consecutively. `value -> count` inserted the
caller's identifier; the second pass treated that new `count` as an authored
callee local and rewrote it to `__Child_1_count`.

Combine the disjoint parameter/local maps and apply the existing lexer-aware,
syntax-role-aware substitution once. Its replacements are not recursively
rescanned. Local declarations still get instance-specific names; inserted
arguments remain in the caller's lexical frame and are resolved by the enclosing
expansion. The existing parser still owns strings, interpolation, property names,
object keys, attributes and events; no alternative parser or name exceptions.

A related role boundary matters: substituting an argument named `title` into an
inline-content site must not turn it into an attribute key during subsequent
translation. Preserve an existing InlineContent role with grouping when the
replacement spells an existing attribute boundary. This reuses the shared parser
classification and changes no attribute grammar. A probe that previously emitted
`@h2 title=data-label` now emits the authored data attribute and `show (title)`.

## Independently checked resolution

The original `known-parameter-capture.jtml` is byte-identical; its oracle remains
`{parentCount:100,childCount:10,result:200}`. The frozen baseline produces expanded
result **20**, while the parent dynamically renders **200**. After repair:

- Lowering: `__Child_1_result = __Parent_2_count * 2`, not child count * 2.
- Native expanded result **200**, child count **10**, parent count **100**.
- Native rendered view **200** and clean-production real-browser DOM **200**.
- Exact source provenance, parameter dependency reads, CSP and no external
  requests checked independently.

The unchanged active catalog initially rejected the unexpected resolution. Only
then was its entry removed manually. Its old signature remains in
`tests/conformance/history/pr31-nested-known-discrepancies.json`. Four new tests
fail on frozen baseline source; focused coverage also checks global attribute
names, member roots, literal keys, interpolation, actions, sibling/grandchild
state and repeated updates. The sibling fixture compares expanded states with
explicit arithmetic expectations separately from native/browser rendered views.

## Separate unresolved boundaries

These checked-in counterexamples reproduce on frozen main and this candidate;
none is a claimed pass or repaired by modifying another backend:

1. `nested/known-grouped-argument.jtml`: Child receives `(count + 1)`, count=100.
   Independent child expectation **202**; native view **202**, browser **NaN**.
   Clean production accepts the program. Browser argument handling is a separate
   follow-up; this PR does not silently redefine grouping semantics.
2. `nested/known-global-parameter-alias.jtml`: global count=100 passed directly
   to Probe, whose own count=10. Independent result **200**; native result **20**,
   browser DOM **NaN**. Repaired lowering correctly references global `count`;
   native initially derives **200**, then registering public local aliases and
   recalculating changes resolution to the local count and yields **20**. The
   browser global-argument failure is separately observable. These require
   backend ownership/argument work; native environment repair is outside scope.

Both use exact source/oracle/signature catalogs and remain FAIL / KNOWN DEFECT.
Supplemental strict mode exits 2; unknown failures, changed expectations and
unexpected resolutions are rejected. The five existing native complete-attribute
renderer observations remain independently catalogued and unchanged. Therefore
this repair does not claim universal component-argument conformance.

## Reproduction and critical review

Use the established pinned Playwright/Chromium environment:

```
ctest --test-dir build -R '^(Friendly|NativeComponentEnvironment)' --output-on-failure
node tests/conformance/runner.cjs --strict --minimize
node tests/browser/nested_environments.cjs
node tests/browser/nested_environments.cjs --strict # 2 for separate backend defects
scripts/verify_all.sh
```

Setting JTML_BIN/JTML_CONFORMANCE_PROBE to independently built baseline binaries
reproduces the prior failure. Saved local evidence lives under
`/tmp/jtml-capture-before/`, `/tmp/jtml-capture-negative.log`,
`/tmp/jtml-capture-stable-review/`, and `/tmp/jtml-capture-global-{before,after}/`;
checked-in sources/oracles and historical signatures are the durable evidence.

Review: one substitution preserves caller ownership; local maps exclude declared
parameters and instance prefixes isolate siblings. Metadata still records authored
parameters/actions, source lines are unchanged, and strings/member names are
handled by the existing lexer. No native synchronization, lifetime, coercion,
compatibility fallback or browser execution code changes. The separately exposed
backend failures limit the supported evidence rather than being hidden by catalog
maintenance.

Stable-candidate verification: **204/204 focused tests**, **437/437 CTests**,
original **22/22** matrix (all source hashes/oracles unchanged), new sibling
expanded/native-rendered/browser case, original 200/200/200 case, catalog fault
injection, strict N04/provenance, production reference, Taskboard, security,
async/lifecycle and complete `verify_all.sh` PASS. All **nine** browser benchmarks
have `browserExit=clean`; zero checks skipped. Runtime remains byte-identical at
**271633 / 272000 bytes**, SHA256
`da88340a15ab9fb1557d67f8c6109f7c376fe7e2ac523b05dcef03020d86b011`.
Existing reference HTML size warning remains visible and budgets unchanged.
Supplemental strict mode remains 2 for the separate backend failures; none is
counted as a semantic pass. Local full-gate log: `/tmp/jtml-capture-full-gate.log`.
Exact-head Ubuntu/macOS CI is separate evidence recorded in the PR.
