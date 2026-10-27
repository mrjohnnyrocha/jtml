# Component input synchronization — 2026-10-27

The PR #51 references are correct. `input into name` on a simple component-local
writable `let` is intended to bind state to the input's displayed value as well as
input events back to state. The published RC.2 failure is a **compiler defect in
the generated static component path**, not an unsupported empty-string assignment
or an agent failure. It also affects nonempty programmatic assignments.

## Independent reproduction and artifact identity

The exact published macOS arm64 RC.2 binary was run without rebuilding or replacing
it. Its SHA-256 is
`b96cadca3a0ff010ddcebf931eba60822a89ee40da7537be2024ac59417f17b9`.
The previously anonymously downloaded published archive was independently checked
against the published SHA256SUMS, and its `release/bin/jtml` member hashes to that
same binary. Archive SHA-256:
`225b0f5e76251e1b70ef9d2fd450a028e2854edbfc3a5546891f5d4332dd94f9`.
Release source: `13c53ab475c9e82ac4be2b232f744d664e926dea`.
See [provenance](evidence/provenance.json).

The minimal fixture is byte-identical to PR #51's standalone fixture (SHA-256
`aea5093492c49566dd50d1fcd13f5713b7f280d4525f18cddd48612375351284`):

```jtml
jtml 2
make Editor
  let draft = ""
  get echo = "Draft: [{draft}]"
  when clear
    draft = ""
  input into draft aria-label "Draft"
  button "Clear" click clear
  text echo
page
  Editor
```

Type `Before reset`, then click Clear. RC.2 shows `Draft: []`, but the textbox
still displays `Before reset`. The unchanged PR #51 probe independently reproduces
this, as does the new browser test. Neither test changes application state through
injected JavaScript. Chromium 153.0.8010.12 / Playwright 1.63.0 on macOS arm64 was
used for both compiler cohorts. Raw results and minimal traces are preserved in
[evidence](evidence/); original benchmark evidence stays on draft PR #51.

## Intended semantics and the missing coverage

Before this fix, `src/friendly.cpp` already lowered `input into name` to
`value=name` plus `onInput=setName()`, with a generated setter assigning `name`.
`FriendlySyntax.PageElementsEventsAndInputLowerToClassic` explicitly asserts that
pair. The existing authoring contract recommends `into` for form inputs; the
support-candidate matrix covers simple local writable state and retained focus.
`FriendlySyntax.ComponentInputHelpersDispatchInTheirOwningEnvironment` checks
per-instance setters. These establish the intent without treating the broader
partial forms implementation as a guarantee for every HTML control.

Existing `form_focus` and reference form-submit browser regressions exercised
user input flowing to state and preservation of input identity/focus/selection.
They never assigned a different state value and checked the dirty textbox's
`.value` afterward. The independent benchmark exposed that missing direction.

## Precise root cause and bounded correction

There are two omissions in the component compiler:

1. `templateExpressionDependencies` in `src/runtime_plan.cpp` skipped `into` and
   its target. The input body node had a write to `draft` but no read, so the static
   update plan had no input patch scheduled for a `draft` write. The echo node
   did have that dependency and updated correctly.
2. `compileStaticElementParts` in
   `src/jtml/runtime/static_update_plan_emitter.cpp` emitted only
   `data-jtml-direct-component-bind="draft"`. That metadata lets the runtime's
   delegated input listener write component state. It emitted no `value`
   expression for creation or retained patches. Adding an invalidation alone
   would still leave no value to apply. The existing attribute-map code already
   preserves empty strings for `value`; this is not a truthiness-filter defect.

The fix makes input bindings read their target in the Runtime Plan and compiles
the implicit value through the existing typed Expression IR path. Generated
creation and update functions then use the existing attribute/property machinery.
Dirty inputs require setting the DOM `.value` property; changing the HTML value
attribute alone is insufficient. The existing equality guard assigns the property
only if it differs, so input-origin writes retain cursor selection. File inputs
are excluded from property assignment because browsers prohibit setting a nonempty
file value. The `file`/`dropzone` aliases receive no new value binding.

No runtime expression evaluator, source scanner, compatibility fallback or
benchmark-specific logic is added. The runtime JavaScript asset is unchanged.

## Compatibility and evaluation cohorts

- No source migration is required for these references. Their local `let` targets
  satisfy the existing binding contract; an explicit `value name` is not required.
- Clean, compat and debug builds use the corrected generated static path and are
  tested. Root lowering, native/live setters and action semantics are unchanged.
- Empty and nonempty assignments now synchronize retained text inputs, mirrors
  and conditional controls, without replacing the input or affecting siblings.
- This is not a claim about textarea/select synchronization, checkbox checked
  state, file selection, numeric coercion, IME or all fallback/manual body plans.
  Invalid member/derived/parameter targets remain fail-closed under the existing
  validation tests.
- Already deployed RC.2 assets retain their defect. Only rebuilding with the
  corrected development compiler changes the emitted application. The published
  release and its artifacts remain unchanged.
- The corrected binary identifies itself as `0.3.0-dev` and is recorded by its own
  SHA-256 in the development result. It is a separate cohort. PR #51's criteria,
  report, source and RC.2 failures are unchanged; no agent advantage or adoption
  evidence is inferred from the corrected references.

## Reproduce the regression

```sh
JTML_BIN=/absolute/path/to/compiler \
JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT=/absolute/path/to/playwright-project \
JTML_BENCH_BROWSER=/absolute/path/to/chromium \
node tests/browser/input_state_sync.cjs
```

Set `JTML_INPUT_SYNC_OUT` to preserve a separate evidence directory per compiler;
`JTML_INPUT_SYNC_MODES=clean` selects only production for the RC.2 comparison.
The default checks all three modes and fails rather than skipping absent tooling.
The test covers the original reset, nonempty replacement, focused programmatic
clear/replacement, user typing after reset, mirrored bindings, conditional
creation/remount, sibling isolation, fresh reload, identity and selection.
It runs in CI and in `scripts/verify_all.sh`. A Runtime Plan unit regression
requires both read and write dependencies while keeping file binding semantics
separate. See [verification](evidence/verification.json) for the complete measured
scope, including remaining known failures and the unchanged benchmark rerun.

## Verification results

The full local gate passes: **477/477 CTests**, all nine browser benchmarks with
clean exits, all six new synchronization scenarios, package relocation checks,
and live smoke **8/8**, with no verification skips. The five known native
attribute-renderer discrepancies remain; the reference HTML is now 99,953 bytes
against the existing 70,000-byte microfixture budget (99,939 at the prior baseline).
Those observations are not reclassified as successes and no budget was raised.

Running the unchanged PR #51 probe with the corrected compiler exits 0. Running
its unchanged full calibration matrix in a separate development cohort passes
both JTML references and both React references; both seeded stale-confirmation
defects still fail as intended. These are reference/harness results, with zero
measured agent trials. Hosted CI remains separate evidence.
