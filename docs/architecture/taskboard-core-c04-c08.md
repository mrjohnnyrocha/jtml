# Taskboard interactive workflow: C04–C08

Base: main `08fd449` (merged PR #12). The approved acceptance contract is
unchanged. This extends the existing parent-owned ID→record map; rows emit
requests and retain only drafts, edit state and an interaction counter.

| Scenario | Result | Independent browser evidence |
| --- | --- | --- |
| C04 | PASS | Enter saves only t-101 as Audit first; whitespace save does not mutate records/IDs; cancel restores t-102 and clears its associated error. Title save count is one. |
| C05 | PASS | Native Enter saves exact markup-looking note once; input, backward [7,16] selection, focus, counter 2 and nested instances survive. |
| C06 | PASS | Three Alt+F8 moves produce 102,103,104,101,105; all captured row/nested/input references and generations survive, including t-102's draft/counter. Subsequent submit reaches only 102. Alt+F7 moves up. |
| C07 | PASS | Complete/open/complete produces 5/3/2, 5/4/1, 5/3/2; nested status/title/data-completed update without replacing retained controls. |
| C08 | PASS | Open genuinely removes 101/103 and six registrations exactly once. All remounts fresh state/generations from saved records; discarded drafts stay discarded; visible siblings retain identity/drafts/counters. Removal focuses the filter, remount does not steal focus. |

The [workflow oracle](../../tests/browser/taskboard_workflow.cjs) constructs its
expected records/order independently of emitted metadata. It checks canonical
state, counts, DOM/registry bijection, identities, submit instrumentation, native
keyboard interaction, absence of fallback/errors/network activity, and sibling B.
The original C01–C03 and both lifecycle/commit-recovery oracles are unchanged.
All 32 acceptance requirements and the support matrix remain unchanged.

## Generic defects exposed and repaired

- Ownership analysis treated scalar computations as borrowed aliases, rejecting
  writes into a parent-owned map. Unary/arithmetic/comparison results are scalar
  copies; logical AND/OR retain provenance because they can return objects.
  The focused CLI regression fails before repair; borrowed-reference rejection
  remains tested. Parent handlers explicitly store strings, preserving payloads.
- Emit-only actions have no local writes. Treating that empty set as requiring
  a render replaced child controls after the parent had already patched them.
  Empty writes now skip the unnecessary child render.
- Static component keydown shorthand lacked a direct production binding. Bare
  action names now receive a closed key/modifier payload without runtime eval.
  Native default behavior is retained; Alt+F8/F7 avoid native text-editing actions.
  Unsupported invocation arguments fail closed rather than silently disappearing.
- Computed assignment indices used a different evaluator from computed reads,
  corrupting swaps into duplicate IDs. Bracket evaluation now uses the existing
  compiled component-expression evaluator.
- A synchronous browser-local activation that removes the focused element now
  focuses its surviving activation control. Retained inputs and asynchronous live
  behavior are unaffected.

The [small event fixture](../../tests/fixtures/production/component-events/index.jtml)
and [oracle](../../tests/browser/component_events.cjs) isolate emit-only retention,
native Enter/button submit, keyboard payloads, computed-index swaps and removal
focus. Four temporary copies of the final assets each revert one runtime fix;
all four fail the unchanged final oracle at the relevant assertion. These are
negative controls, not production asset edits. An early fixture used invalid
nested paragraphs; that test markup was corrected before accepting its results.
Both new oracles run from the existing mandatory Taskboard CI/full-gate runner.

## Verification and limits

Local macOS arm64 Release, C++17, Python bindings off, Node 22.23.3, Playwright
1.63.0 and Chrome Headless Shell 153.0.8010.12, using cached dependencies:

- Focused CTests **49/49**; full CTests **348/348**.
- C01–C08, retained controls, generic events, both original P1 regressions and all
  four commit-recovery cases PASS.
- Complete production reference and all nine browser/asset benchmarks PASS with
  clean browser exits and unchanged budgets.
- `scripts/verify_all.sh` PASS on the first final run, live smoke **8/8**, no skips.
- Taskboard `check` PASS. **Lint FAIL: 182 errors / five warnings**, including
  lowered component/action-local name reports; no linter fix or pass is claimed.
- Node/shell syntax and diff whitespace checked; scenario contract/original
  browser oracles remain byte-for-byte unchanged.

The known intermittent native `friendly_store` crash remains unresolved. The UI
full gate passed its store smoke. A separate clean `native-store-investigation`
worktree at main ran five pre-change compiler startups: 5/5 healthy. This small
sample does not disprove the earlier malloc/SIGSEGV evidence or establish a fix.
No native crash investigation changes are mixed into this implementation.

| Uncompressed asset | PR #12 application | Interactive application | Delta |
| --- | ---: | ---: | ---: |
| HTML | 316624 | 712195 | +395571 |
| Shared runtime | 270406 | 271114 | +708 |
| Component module | 178082 | 244488 | +66406 |
| Bootstrap | 619 | 619 | 0 |

Application sources differ across this comparison; added forms/actions account
for application growth. Runtime remains below the unchanged **272000-byte**
budget; all nine fixture budgets pass. No application budget is newly asserted.
The reference HTML comparison remains visible, not reclassified as passing.

Logs are ephemeral `/tmp/jtml-pr13-verify.log`, `jtml-pr13-focused-final.log`,
`jtml-pr13-workflow-strengthened.log`, `jtml-pr13-negative-{emit,keyboard,index,focus}.log`
and `jtml-pr13-lint.log`. Hosted Ubuntu/macOS CI is separate evidence, recorded
on the PR's final head. C09–C14, N01–N08, T1/T2, full T0, precise source provenance,
general slots and complete semantic conformance remain unverified. No merge is
automated.
