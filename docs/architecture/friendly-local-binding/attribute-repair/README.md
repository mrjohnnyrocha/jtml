# Attribute/content classification repair

The saved PR head 342da37 incorrectly lowered the original Friendly source in
[evidence.json](evidence.json) to `@h2 title=data-label` / `show "marker"`.
Unchanged main and the repair lower it to `@h2 data-label="marker"` /
`show "Revenue"`. The original failed lowering and native diagnostics remain
recorded; a successful native body-plan render alone did not establish correctness.

## Classification contract

Element parsing owns token roles: syntax names, expression values, inline content,
and complete attributes. Binding consumes the roles produced by that parser.
It no longer maintains an independent attribute-value state machine. Production
creation and updates consume the same content classification with the compiler's
component binding names. The actual chart and metric parsers report their roles
too; chart field/type/format names remain literals.

A leading bound name can denote inline content before an attribute sequence.
When the following token is also bound, the parser's consumed-value roles decide
whether it starts a valued attribute. Thus `style style "color: red"` retains the
first style as content, while `title title` retains title as an attribute key and
binds its value. Input/textarea attribute and placeholder rules are preserved.
Attribute names, events, boolean attributes and utility modifiers retain their
existing parser grammar. A complete attribute must have a valid HTML name and an
assignment separator; equality/comparison/grouped expressions are not attributes.
Quoted trailing content is retained where the existing parser accepts it.
Ambiguous unsupported authoring forms are not a new language feature.

Only expression sites reach the existing lexer-based substitution. Literal string
bytes, member/property names, object keys, external identifiers and declarations
retain the binding repair's contract. The original source/body records and source
mapping remain untouched. Classification errors carry the authored component line;
strict N04 checks exact file/line/column and ownership rejection separately.
No interpreter, async worker, server or browser runtime asset code changes.

## Independent evidence and limitations

`tests/conformance/attribute_cases.cjs` adds 40 independently authored cases:
35 equivalent cases plus five observations of one existing native body-plan
complete-attribute defect. All 40 require correct lowering, lint, native diagnostics
and production DOM/attribute behavior. Button event dispatch is checked by DOM
updates. The five complete-attribute cases retain their normative Revenue
expectation and exact failing native-visible traces; they are **FAIL / KNOWN
DEFECT**, never semantic passes. A changed failure, unexpected resolution or an
additional failure aborts validation. The separate literal-source main control in
evidence.json proves this native rendering defect predates PR25; do not reclassify
supported Friendly syntax as unsupported to conceal it. Repairing that legacy
renderer is outside this compiler binding change.

The unchanged original differential matrix remains 13 equivalent / nine known
defects: seven original discrepancies resolved, nine still present. Its independent
expectations and catalog are unchanged by this repair. Strict mode must remain
nonzero. The additional native body-plan boundary above is separate evidence,
not an extra resolution or an edit to that frozen matrix.

Focused C++ regressions include data-/ARIA/standard attributes, repeated names,
boolean/event/complete forms, comparisons, property keys, primitive modifiers,
chart/metric roles and authored error lines. Existing sibling, nested/imported
component, declaration, string/interpolation and provenance tests remain gates.

Commands:

```sh
ctest --test-dir build -R 'Friendly|SharedScopeDeclaration' --output-on-failure
scripts/test_semantic_conformance.sh
scripts/test_semantic_conformance.sh --strict # expected exit 2, unresolved defects
python3 tests/test_taskboard_rejections.py build/jtml /tmp/jtml-n04 --require-provenance
JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1 scripts/verify_all.sh
```

Use the pinned Playwright/Chromium environment described by the CI workflow.
Reports retain actual backend traces under dist/conformance. The supplemental
browser runner has a hard subprocess timeout and closes every page/browser.

Development failures are preserved in /tmp/jtml-pr25-attribute-final-gate.log
and /tmp/jtml-pr25-shared-*.log; the final corrected gate is recorded separately
in /tmp/jtml-pr25-attribute-final-gate-v2.log. Do not count the failed development
gate as passing. The historical Classic HTTP reset remains UNVERIFIED with its
original evidence in ../causal-investigation; no concurrency change or causal
attribution is made here.

A separate exploration, loop-title-investigation.jtml, is **not an equivalent
passing case**: unchanged main already rejects it in lint with undefined `data`
and `label` in expanded lowering. Its loop-variable collision support contract
remains unverified. The source and main diagnostic are retained; this PR does not
claim to repair that Friendly loop-name lowering boundary or add it to the
supported matrix. Static element preparation nevertheless uses the existing
Runtime Plan lexical environment, including loop names, consistently for creation
and updates. Compiler-only prepared parts are stripped before asset serialization;
the unchanged compatibility-asset budgets remain mandatory.

The first gate after that plan-sharing refinement failed the existing
large_keyed_reorder compatibility-asset budget (180779 > 180000 bytes). This is
preserved in /tmp/jtml-pr25-attribute-final-gate-v3.log. The compiler cache is now
excluded from serialized assets, with a dedicated C++ regression; the final gate
must be a separate run after that correction.

## Final local result

Final source gate: /tmp/jtml-pr25-attribute-final-gate-v4.log, PASS. It ran all
399 CTests, production reference, Taskboard/provenance/security/async/lifecycle
regressions, nine mandatory browser benchmarks with clean exits, and remaining
predeploy checks. See validation.json for the preserved log hash and exact matrix.
Runtime remains 271564 / 272000 bytes. The compatibility large-keyed-reorder plan
is 170617 / 180000 bytes; no budget was raised. Final example lint passes with
the four unchanged CI exclusions. Strict mode exits 2 as required by the nine
remaining catalogued defects. The catalog and normative oracles are unchanged.

The preserved production assets independently reproduce the original bug:
browser-before-after.json records saved-head text marker / title NaN, versus
repaired text Revenue / data-label marker. No browser exceptions were necessary
to expose the bad output. Final review found no additional PR-specific P0/P1
regression or supported causal path to the Classic async reset. That historical
failure is tracked separately as https://github.com/mrjohnnyrocha/jtml/issues/26.
Final-head hosted Ubuntu/macOS CI must still be confirmed before merge readiness.
