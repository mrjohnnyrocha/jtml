# Verification — 2026-10-25

Environment: macOS arm64, C++17 / AppleClang 15. Baseline was freshly configured
and built at PR #39 merge `95e7bf0fceb39fa1f5d934e65170acdd88b00695` before edits.
The combined candidate received a clean rebuild; resumed verification rebuilt
incrementally without changing implementation. No implementation/test changes
followed the successful full gate below.

| Check | Result |
| --- | --- |
| Focused parser/linter/import/native setter tests | 57/57 PASS |
| Complete CTest before gate | 468/468 PASS |
| Complete `scripts/verify_all.sh` on stable candidate | PASS, exit 0, one run; no tool/test skips |
| CTests inside complete gate | 468/468 PASS |
| Mandatory performance browser fixtures | 9/9 PASS, every `browserExit=clean` |
| Separate production reference browser | PASS, `browserExit=clean` |
| Taskboard C01–C14, identity/lifecycle/focus and N01–N08 | PASS; original fixtures/oracles unchanged |
| Strict N04 and source-provenance suite | PASS; exact original file/line/column/component rejection contract preserved |
| Original semantic conformance | 22 equivalent; independent negative controls retained |
| Nested ownership supplement | 6 mandatory cases PASS; original 200 native state/native HTML/browser result retained |
| Native attribute supplement | 5 FAIL / KNOWN DEFECT observations retained; catalog verification is not a semantic pass |
| Local security and owned async / serve, Studio, watch | PASS; original denials/lifecycle checks retained |
| Literal/CSP, keyed lifecycle, package/refactor/LSP/editor/site checks | PASS within gate |
| Final live runtime smoke | 8/8 PASS |
| Taskboard check / lint | Both exit 0; lint 0 errors / 1 legitimate advisory |
| `git diff --check` | PASS |

Run the gate with installed Node, Python, curl and Chromium/Playwright:

```sh
export NODE_PATH=/tmp/jtml-global-tools/node_modules
export JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT=/tmp/jtml-global-tools
export PLAYWRIGHT_BROWSERS_PATH=/tmp/jtml-global-browsers
export JTML_BENCH_BROWSER=/tmp/jtml-global-browsers/chromium_headless_shell-1243/chrome-headless-shell-mac-arm64/chrome-headless-shell
export JTML_REQUIRE_BROWSER_BENCH=1
export JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1
export PYTHONDONTWRITEBYTECODE=1
scripts/verify_all.sh
```

Substitute equivalent installed tool paths. Browser used: Chromium
153.0.8010.12. Gate scripts and all existing assertions/budgets are unchanged.
The browser benchmark timings are local fixture observations, not performance
superiority claims.

## Asset comparison and preserved limitations

Independently building unchanged Taskboard with the saved baseline executable
and the candidate yields byte-identical `jtml-runtime.js` (**271855 / 272000
bytes**, 145-byte margin), `components/index.js` (346249 bytes) and `app.js`
(619 bytes). HTML decreases from 1022172 to 1015237 bytes: global generated setter
action records disappear, declaration comments move into the owning component,
and standalone argument references in comments are correctly bound. Browser
behavior is verified by the unchanged contracts rather than HTML byte equality.
Hashes and sizes are retained in `evidence.json`.

The gate still reports the established reference microfixture HTML budget
observation: **99878 bytes / 70000 bytes**. This is a recorded known size failure,
not a newly passing budget. The five native complete-attribute observations also
remain genuine failures. No budgets, discrepancy catalogs or expected outcomes
were weakened. Native/browser parity, general type checking, complete token-exact
spans, release identity and production readiness are not established by this PR.

## Earlier failed checks and repairs

The first complete CTest attempt had two failures: new malformed-postfix controls
exposed a recovery AST returned despite parser errors, and moving module setters
inside ordinary DOM elements broke the existing native file-input event contract.
Both were repaired in implementation, preserving the tests. Component setters
now live at component declaration boundaries; module helpers retain their original
scope. The final native component-dispatch regression initially read the binding
update response as full state; it was corrected to use `getStateJSON()` and passed,
then passed again in focused/full/gate runs. These failed attempts are not passes
or separate successful full-gate runs.

Local raw transcripts remain ephemeral under `/tmp/jtml-remaining-*`; committed
evidence is compact source controls, diagnostic inventories, hashes and this
report. Hosted CI is separate evidence, checked once for the published commit and
reported on the PR; local verification does not imply hosted CI success.
