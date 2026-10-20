# Provenance propagation — work in progress

Stage 2 branch `codex/source-provenance-propagation`, worktree
`/Users/joaorocha/.codex/worktrees/source-provenance-lowering/jtml-v1`, based on
Stage 1 `34dc8c0` (PR #20). No diagnostic completion claim yet.

Exact copied ranges and generated attribution are distinct. Tokens retain the
immutable lowering context; parser spans retain original range, exactness and
context in addition to existing generated coordinates. AST clones must preserve
all span fields recursively; Expression IR already copies parser spans. Semantic
component records retain the context directly from their template AST node,
not by matching component names. Runtime body/slot nodes resolve their recorded
input line/column through that context. A transformed line receives attribution,
not a false exact copy claim. Import stubs remain synthetic.

Current Release configure/build is running; `/tmp/jtml-propagation-configure.log`
and `/tmp/jtml-propagation-build.log`. No propagation test result yet.
Next: add original-coordinate/clone tests, resolve build errors, run focused tests,
review for synthetic misattribution and production source-data leakage. Then
validate/commit this stage before diagnostic consumer migration.

PR #20 `34dc8c0` now has PASS Ubuntu/macOS CI run `39434704486`.
Four independent propagation tests added (literal lexeme bytes, cloned spans,
Friendly attribution, original borrowed-write Runtime Plan, synthetic/mixed
ranges). First build began before tests were added: rebuild required before
counting any results. No code in this stage is frozen or validated yet.

Further audit: parser previously attached spans only at outer expression calls;
intermediate precedence/member nodes could be spanless. Existing grammar token
boundaries now attach spans at each node creation. String tokens retain the raw
lexeme end offset (decoded string length is not a source extent). Interpolation
IR operations inherit explicit attribution and cannot claim exact authored
expression spans. Added repeated nested-expression and interpolation oracles.
Rebuild required after these edits. Earlier 10/10 focused PASS predates this
expanded nested-expression fix and is not final validation.

Full gate on `8d75dcf` failed compatibility `large_keyed_reorder` asset budget:
190857 > 180000 bytes. Populating nested spans exposed an emission leak of
compiler-only spans. The static-asset emission boundary now removes `sourceSpan`
for both module and compatibility records; a dedicated test pins exclusion.
Budgets are unchanged. Focused tests now 13/13 PASS. Final gate is rerunning in
`/tmp/jtml-propagation-assets-final-gate.log`; the earlier failed log is retained
at `/tmp/jtml-propagation-final-gate.log`. Sanitizers require a refreshed final
probe including the new emitter translation unit and asset-exclusion test.
Old AST negative control exits 1: `FAIL original AST clone loses 9:5 source span`.
