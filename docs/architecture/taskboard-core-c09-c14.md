# Taskboard Core functional completion: C09–C14

Base: main `1a5a9df`, merged PR #14. The approved 32-scenario contract and original
C01–C08/lifecycle oracles are unchanged. All mutations use actual JTML, native
browser controls, immutable IDs and TaskBoard-owned object records. PR #15's
server security, listeners, origin validation, adapter synchronization and
interpreter function ownership are untouched.

| Scenario | Result | Independent Chromium evidence                                                                                                                                                                                                                                                                     |
| -------- | ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| C09      | PASS   | Exact Before→After→Before→Final sequence, inactive Fallback B update, logical/ternary labels, both nested levels and attributes. Active branch counter/DOM/generation survive same-branch writes; Board B's complete DOM and canonical data stay unchanged.                                       |
| C10      | PASS   | Help counter survives an unrelated empty-note save. Two off/on cycles destroy exactly one old Help generation each and remount counter 0; task state and retained task generations survive.                                                                                                       |
| C11      | PASS   | Delete 103 by ID: 102,104,101,105; counts 4/3/1. Three owned generations and handlers disappear; captured detached controls cannot mutate state. Remaining drafts/counters, DOM/generations survive; focus goes to 104's primary edit control.                                                    |
| C12      | PASS   | Native creation allocates only 106: 102,104,101,105,106; counts 5/4/1. First-up and last-down keyboard moves are strict canonical/registry/DOM no-ops, preserving focused inputs, backward selections and unsaved drafts.                                                                         |
| C13      | PASS   | Complete 102,104,105,106: counts 5/0/5 and All complete. Open unmounts all 15 task/nested registrations and shows No tasks match this filter, not No tasks. Complete remounts fresh generations in the required order with committed notes and counter 0.                                         |
| C14      | PASS   | Delete last first, then the remaining tasks: previous/next/create-input focus destinations work. Zero records/tasks/handlers; detached controls remain inert. Create 107 without identity reuse. Reload restores both boards' original C01 values, order/counts and two physical board instances. |

The [continuation oracle](../../tests/browser/taskboard_completion.cjs) sets up the
C08 checkpoint through controls and derives expected records/order independently
of emitted metadata. Checkpoints compare canonical records, rendered values,
counts, DOM/registry bijection, sibling isolation and absence of fallback/errors/
network/navigation. Original C01–C03 and C04–C08 remain separate mandatory runs.
The [runner](../../scripts/test_taskboard_core.sh) makes every new case mandatory
in CI and the full local gate. Each original runner's UNVERIFIED exclusions are
scoped to that runner, not an aggregate claim that later scenarios failed.

## Generic repairs and negative controls

- **Removal focus:** the prior click handler could only focus a surviving
  activation control. It now optionally reads a reactive, container-scoped
  `data-jtml-focus-after-removal` selector when both focus and the activation
  control were removed. A surviving activation control keeps priority (C08);
  connected focus/selection are untouched. The
  [minimal fixture](../../tests/fixtures/production/removal-focus/index.jtml) and
  [oracle](../../tests/browser/removal_focus.cjs) cover all three cases and sibling
  isolation. This is an opt-in DOM policy, with no Taskboard selectors in runtime.
- **Action-local no-op:** runtime render dependency selection included template
  control-flow nodes inside actions. A boundary loop changed only local iterator
  state but triggered full-render fallback. Excluding action descendants from
  the rendered read set fixes the earliest incorrect decision. Rendered uses of
  those variables still contribute dependencies. The
  [minimal loop fixture](../../tests/fixtures/production/action-local-noop/index.jtml)
  and [oracle](../../tests/browser/action_local_noop.cjs) retain an unsaved input,
  exact selection, nested counter, DOM/generations and working handlers over
  three no-op loops. No unconditional success or fallback was added.
- **Instance projection:** linked instances without module identity were merged
  alongside their equivalent module-qualified records, exposing four board
  entries for two DOM instances. Compiler client serialization now copies module
  ownership only from an exact, unique mirror; it does not merge ambiguous module
  instances or normalize user data. A focused CTest preserves ambiguous-module
  separation; the browser oracle requires two actual instances after reload.
- **Redundant output:** the root already owns the linked client payload. The
  public project's linkedPlan retains its closed schema with empty collections,
  avoiding a second serialization/merge of the same payload. Executable module
  plans and compiler/explain RuntimeProjectPlan data remain intact.

All four final negative controls fail the unchanged generic oracles: temporary
copies restore old focus behavior, the intermediate wrong focus priority, or the
old action-read selection; an actual saved main compiler build with the final
runtime restores duplicate instance projections. Each fails at its intended
assertion, not a missing asset or browser failure. Only temporary assets were
changed. The projection CTest also fails before repair and passes after it.

During test development, C09 lacked controls, C11 lacked deletion then focus,
C12 exposed the no-op fallback, and C14 first lacked empty UI then exposed duplicate
instances. An initial minimal focus fixture had invalid ungrouped attribute
expression syntax; its build failure was not accepted as runtime evidence.
The first focus candidate broke the original C08 priority assertion; the final
policy repairs it and adds the corresponding minimal negative control.

## Verification and limits

Local macOS arm64 Release; Node 22.23.3, Playwright 1.63.0 and Chrome Headless
Shell 153.0.8010.12; Python bindings off, cached pinned build dependencies:

- Focused CTests **41/41**; full CTests **358/358**.
- `scripts/verify_all.sh` **PASS** on the first full run: complete production
  reference, C01–C14, both original lifecycle cases, all four bounded commit-recovery
  cases, clean/compat literal fidelity, malformed-source controls, native call
  timeout regressions, tooling/package/site/editor checks and live smoke **8/8**.
- All nine required browser/asset benchmarks **PASS**, clean browser exits, no
  skipped checks or raised budgets. Omitted legacy assets in clean mode are
  intentional output, not a skipped validation.
- Taskboard `check` **PASS**. Lint **FAIL: 232 errors / five warnings**, including
  lowered component/action-local names. No linter repair or lint pass is claimed.
- Node/shell syntax, Markdown/local links and diff whitespace checked. SHA-256
  comparison confirms the approved contract/support matrix and original oracles
  unchanged; all **32** scenario IDs remain unique and present.

**N01–N08 and A01–A10 remain UNVERIFIED** as complete scenarios. Full adversarial
T0 acceptance remains incomplete. Original source provenance/S1–S3, broader slots
and semantic conformance are not established. Ambiguous module identity is not
silently collapsed. The known native `friendly_store` malloc/SIGABRT/SIGSEGV
instability remains unresolved; this passing store smoke does not prove a fix.
No native/server/interpreter ownership changes or investigation were mixed in.
Hosted Ubuntu/macOS CI is separate final-head evidence, recorded on the PR.

## Measured output

Bytes; deterministic gzip level 9 and Brotli quality 11, using Node's existing
zlib. These are measurements, not new application budgets or extra production
assets. The runner emits final measurements on every run.

| Asset            | C01–C08 main raw / gzip / Brotli | C01–C14 final raw / gzip / Brotli |
| ---------------- | -------------------------------: | --------------------------------: |
| HTML             |           712771 / 55668 / 19246 |            735287 / 60078 / 26844 |
| Component module |            244488 / 17590 / 6632 |             327403 / 23522 / 8478 |
| Shared runtime   |           271629 / 49929 / 40418 |            271969 / 50037 / 40502 |
| Bootstrap        |                  619 / 312 / 248 |                   619 / 312 / 248 |

Runtime grows **340** raw bytes, leaving **31** under the unchanged **272000**
budget. HTML grows 22516 raw / 4410 gzip / 7598 Brotli bytes versus the smaller
application; component code grows 82915 raw / 5932 gzip / 1846 Brotli bytes.

The same final source with the saved main compiler produces HTML
**1043021 / 81410 / 26958** bytes. Removing the redundant linked copy and qualifying
instance mirrors reduces that to **735287 / 60078 / 26844**: **307734** fewer raw
bytes, **21332** fewer gzip bytes, but only **114** fewer Brotli bytes. Brotli
already compresses duplicated payloads well; no 30% Brotli/network win is claimed.
Component-module growth is application growth, not hidden by the HTML reduction.

Remaining duplication is explicit: linked/global and module-qualified component
plans/functions still coexist in component modules, and the HTML still carries
root plus module definitions. Their provenance/layout is not interchangeable in
general, so this milestone does not speculate about removing those ownership
paths. The production reference HTML falls from 135374 to **98172** bytes but
still exceeds the **70000-byte microfixture comparison**; its warning stays visible.

Ephemeral evidence: `/tmp/jtml-core-final-{verify,taskboard,projection-focused}.log`,
`/tmp/jtml-core-final-negative-{focus,focus-priority,noop,projection}.log`,
`/tmp/jtml-core-final-projection-negative.log`, `/tmp/jtml-core-final-lint.log` and
`/tmp/jtml-core-final-sizes.json`. No automatic merge.
