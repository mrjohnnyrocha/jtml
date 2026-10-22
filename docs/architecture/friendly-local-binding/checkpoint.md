# Friendly local binding repair checkpoint

Base main 100c618 (merged PR #24). Worktree friendly-local-binding/jtml-v1,
branch codex/friendly-local-binding. No PR published yet; no runtime repair.

Unchanged baseline: /tmp/jtml-pr25-baseline.log, six equivalent cases and sixteen
known defects. First incorrect layer confirmed: substituteIdentifiers in
src/friendly.cpp misses lexer identifiers inside grouping/index delimiters.

Prototype replaces reference identifiers using the existing lexer byte positions,
retains property names/dictionary keys/string bytes, and rewrites only balanced
interpolation expressions. Friendly syntactic positions use existing attribute,
event and boolean vocabularies; declaration/binding names that overlap Classic
keywords retain existing renaming. Original body/provenance metadata is retained.

Failures discovered during development:
- New test's escaped-brace spelling failed existing literal decoding before binding.
  Revised to a supported escaped backslash identifier literal; no decoder change.
- Real regression: treating data-task-id as subtraction rewrote attribute names.
  Fixed by classifying Friendly template attribute positions, including values,
  rather than applying a raw lexer pass indiscriminately.
- Friendly local names can overlap Classic keywords (object); preserve these
  bindings without rewriting string tokens or property names.

Focused tests before final attribute-position revision: 130/130 PASS;
additional adversarial tests 2/2 PASS. Latest revision is being tested in
/tmp/jtml-pr25-focused5.log; do not count earlier results as final.

Unchanged oracle /tmp/jtml-pr25-after.log independently shows seven resolved:
indexed-dependencies, grouped-local-minimal, generated-24-2 through generated-24-6.
Nine remain. Boolean-comparison now reaches the separate stale-ternary defect;
generated-24-0/1/7 still expose unary numeric strings. No corresponding native
repairs made. Original full catalog retained under tests/conformance/history/.
Reviewed current catalog check passed /tmp/jtml-pr25-catalog.log (13 equivalent,
nine known defects). The reducer now isolates the remaining unary type mechanism;
a separate grouped zero-event control must pass. Re-run after latest code edit.

Required remaining work: review lexical scope/parameter and imported/nested
adversarial coverage; re-run current catalog and strict nonzero; full CTests,
nine clean-exit benchmarks and full verify_all; review asset/provenance deltas;
commit/push/open focused PR and verify final Ubuntu/macOS CI. Do not merge.
