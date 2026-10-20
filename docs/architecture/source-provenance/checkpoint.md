# Source provenance mission checkpoint

2026-10-20. Branch `codex/original-source-provenance`, worktree
`/Users/joaorocha/.codex/worktrees/source-provenance/jtml-v1`.
Base main `094e5304704a6fd9b626dd63de59c57f5ca5fcaa` includes merged PR #19.
Its final head `3172279` passed Ubuntu/macOS CI run `39326198680`.
No repeat of identical-tree async verification is needed without a new failure.
Previous temporary mission checkpoint is absent; existing sanitizer worktrees
and commits are preserved. Native corruption remains unresolved.

## Design invariants (before implementation)

1. Source identity is an immutable original file plus original byte range.
   Equal spelling at different offsets or in different files is distinct.
2. Module selection, concatenation and export stripping copy ranges and their
   provenance together. Removed bytes acquire no generated positions.
3. Synthesized bytes have no exact original range. Attribution to a triggering
   construct must be distinguished from exact copied text.
4. Composition uses recorded transformation ranges, never searches for matching
   source strings, guessed line offsets, or component-name joins.
5. Original coordinates are computed from original bytes: offsets are bytes,
   lines are one-based and columns count Unicode scalar values (tabs count one).
   CRLF is one line break. Ranges are half-open.
6. Provenance stays compiler-owned; clean browser assets do not receive maps.
7. A consumer may claim exact provenance only for a contiguous original range.
   Mixed-file/generated ranges must be explicit, not plausibly wrong locations.

Stage 1: retain mappings through module loading, independently test copied and
synthetic ranges. Stage 2: compose Friendly lowering, tokens/AST, Expression IR,
semantic and runtime plans. Stage 3: migrate diagnostic consumers and prove
unchanged strict N04. N04/T0 remain BLOCKED until that final proof.

## Stage 1 result

Loader mapping is implemented. Release focused tests **6/6 PASS**; complete
CTest **381/381 PASS**. Full `scripts/verify_all.sh` **PASS**, including nine
required browser benchmarks with clean exits, production references, Taskboard,
lifecycle, literal fidelity, security, async adapters and live smoke 8/8.
No validation skipped. Exact legacy loader output/rejection comparison against
pre-change libraries passed for 66 examples/production fixtures.

ASan/UBSan and separate TSan each passed six focused tests with halt-on-error.
Only the changed loader, mapping and test translation units were instrumented;
unchanged linked dependencies were Release. This is not full-program sanitizer
coverage. Reproduction script: `evidence/run-focused-sanitizers.py` (macOS).
The strict N04 acceptance command still exits **2**, as expected at this stage;
N04/T0 remain **BLOCKED**. Native corruption remains unresolved.

Stage 1 adds no map data to browser assets and changes no diagnostic consumer.
The next stage must consume these original ranges during Friendly lowering,
retain generated/synthetic distinctions, and propagate through tokens/AST,
Expression IR, Semantic IR and Runtime Plan before migrating diagnostics.

Logs: `/tmp/jtml-provenance-stage1-gate.log`,
`/tmp/jtml-source-sanitizers.log`, `/tmp/jtml-stage1-strict-n04.log`.
Next exact action: commit/review/push Stage 1, verify final-head hosted CI,
then start Stage 2 on a separate branch based on Stage 1.
