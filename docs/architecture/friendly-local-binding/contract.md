# Friendly component-local binding

Component expansion assigns each owned local a distinct instance name. Every
reference to that local must resolve to the same binding irrespective of grouping,
indexing or nesting. Member names and dictionary literal keys are not references;
string bytes remain literal except balanced, unescaped interpolation expressions.
Action parameters/declarations retain their existing scoped lowered names. The
existing production restriction against unsafe nested redeclarations is unchanged.
External identifiers absent from the binding map remain external.

The repair uses existing lexer identifier boundaries and original byte positions,
not substring/regex substitution. Friendly template attribute/event positions use
existing syntax vocabularies, while their expression values are bound. Friendly
text and trailing inline-content positions remain expressions. Friendly names
that are Classic keywords retain existing declaration/reference renaming.
Compiler original body records and provenance mapping remain unchanged; generated
expanded text remains attributed lowering, not falsely exact authored bytes.

Before: `let x = 0; get result = (x)` expands the declaration to __Probe_1_x but
leaves `(x)` unresolved. After: `(__Probe_1_x)`. Ungrouped `x` remains the control.
`items[index]` now binds both the root and the computed index, while `record.x`
binds only record and `{ x: x }` retains the key x. Independent C++ tests cover
nested grouping, function arguments, external callees, action parameters, literal
strings, escaped backslashes, interpolation, property keys and sibling names.

Unchanged PR24 baseline: six equivalent / sixteen known defects. After repair:
thirteen equivalent / nine known defects. Seven resolved entries are explicitly
removed: grouped-local-minimal, indexed-dependencies, generated-24-2 through -6.
No normative expectation or source fixture was changed. Original signatures remain
in tests/conformance/history/pr24-known-discrepancies.json. Boolean-comparison now
exposes its stale ternary dependency; generated-24-0/1/7 expose remaining unary
number-type errors. Their changed signatures were inspected, not blessed wholesale.

The reducer now isolates the still-unfixed unary number-type defect. A separate
zero-event `(x)` control must pass; the ungrouped control remains in the matrix.
Catalog injection self-tests still reject changed expectations, new failures and
unexpected resolutions. Native ternary, unary and nested environment mechanisms
are unchanged.

Reproduction: scripts/test_semantic_conformance.sh; --strict must still fail.
Before/after logs: /tmp/jtml-pr25-baseline.log, /tmp/jtml-pr25-after.log and
/tmp/jtml-pr25-catalog.log. Full validation results are recorded on the PR.
Development failures and pending evidence are retained in checkpoint.md.
