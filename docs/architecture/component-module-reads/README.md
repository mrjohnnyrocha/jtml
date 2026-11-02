# Component module reads: blocked ownership prerequisite

Historical investigation retained as unsupported-behavior evidence. The separate
[collision guard](../module-binding-collisions/README.md) adds fail-closed validation;
it does not implement this module-read contract.

Baseline: main after PR #74, `50e1427ced568b00f56cb1e97d24f2084ab9aed4`.
No runtime/compiler correction or PR is proposed. This is a negative regression
and a prerequisite report, not implementation acceptance.

## Failing browser regression

`docs/architecture/component-module-reads/evidence/module_state_reads.cjs` and the 14 fixtures in
`docs/architecture/component-module-reads/evidence/fixtures/` preserve the established A/B/D
controls and specify the desired C/E class transitions. They are deliberately
not wired into CI or the full gate while the implementation is blocked.

```sh
JTML_BIN=/tmp/opspulse-theme-clean-build/jtml \
JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT=/tmp/jtml-input-browser-driver \
JTML_BENCH_BROWSER=/tmp/jtml-input-browsers/chromium_headless_shell-1234/chrome-headless-shell-mac-arm64/chrome-headless-shell \
JTML_MODULE_READ_RESULTS=/tmp/opspulse-module-read-regression \
node docs/architecture/component-module-reads/evidence/module_state_reads.cjs
```

Actual Chromium: 6 controls pass, all 8 C/E cases fail. Both expression and
interpolation forms are covered on page and box. The expected derived transitions
are light/dark/light; the store-field transitions are false/true/false. C/E
instead remain classless (box retains its built-in jtml-box class). D patches
correctly. All element and parent identity comparisons pass; no browser errors;
normal browser.close() and server.close() complete. Results are written even
when assertions fail; there is no waived assertion or simulated state mutation.
These are initial/two-toggle regressions, not the complete requested reset,
shadowing, lifecycle and destruction acceptance suite.

## Blocking counterexample

Two defining modules declare private scalar state with the same spelling:

```jtml
// private.jtml
jtml 2
let tone = "private"
when setPrivate
  tone = "private-updated"
export make PrivateCard
  page class tone
    button "Private" click setPrivate
```

```jtml
// index.jtml
jtml 2
use PrivateCard from "./private.jtml"
let tone = "entry"
when setEntry
  tone = "entry-updated"
make EntryCard
  page class tone
    button "Entry" click setEntry
page
  EntryCard
  PrivateCard
```

The clean build succeeds. Its actual client manifest retains module 0's
`state.tone = '"entry"'` and module 1's `state.tone = '"private"'`. The linked
root contains one `tone`. Both emitted actions write `lhs: "tone"`, with no
binding-owner operand. Chromium observations of actual button clicks:

| Event | Public runtime state |
| --- | --- |
| Initial | tone = entry |
| Private | tone = private-updated |
| Entry | tone = entry-updated |
| Private | tone = private-updated |

Both component classes remain absent, as expected on this uncorrected baseline.
Zero browser errors and clean browser/server exit. This is evidence that the
current backend cannot distinguish these runtime binding addresses, not evidence
that the requested module-read contract grants either cross-module access.

## First unsupported ownership boundary

- `SemanticProject` and `RuntimeModulePlan` retain module owners. Component
  definitions also have module identity.
- `RuntimePlanBinding` stores only name/expr; `RuntimePlanAction` and assignment
  statements carry spelling-based names/lhs. There is no root binding identity
  operand surviving into execution.
- `runtimePlanBindingsObjectToJson` / `runtimePlanToClientJson` serialize bindings
  keyed by name. Per-module envelopes retain the distinct declarations, but the
  linked projection already collapses their addresses.
- Browser `mergeRuntimePlanIntoManifest` calls `mergeRuntimeBindings` with
  overwrite=false into shared state/derived objects. Its module stamp applies to
  component definitions/instances, not executable binding addresses.
- `bootstrapClientManifest` initializes one `clientState`; `applyClientDerived`
  reads/writes it by name; `executeClientAction` and `assignClientPath` write that
  same dictionary. This is the first runtime loss of the retained per-module
  distinction in the actual client manifest.
- `componentScopeFor` intentionally has parameters and component locals. Adding
  captured paths from `clientState` would not recover the discarded binding
  distinction; subscriptions keyed by those paths would observe the wrong owner.

A sound general defining-module contract first needs either compiler-issued
hygienic storage identities carried through all root reads/writes and derived
plans, or module-addressed root execution contexts. Initialization, action
parameters/restoration, store mutations, derived evaluation and existing root
subscriptions must agree on that identity before component captures subscribe.
That expands beyond adding component subscriptions and constitutes the substantial
root execution change for which this task explicitly required stopping. An
entry-only/unique-name shortcut would not implement the full defining-module
contract or imported-private-binding isolation; none was shipped.

The retained 28-case differential evidence remains in `/tmp/opspulse-matrix/`.
The private counterexample, generated manifest, browser runner and observations
are in `/tmp/opspulse-private-owner/`. CLI explain's fallback entry module AST
is incomplete for this import case, so the ownership evidence above uses the
actual clean-build manifest, not that fallback explain projection.

## Status and limits

Only negative tests/fixtures and this report changed. No compiler/runtime source
change, commit, PR, merge or deployment. Runtime and component payload deltas:
zero. Canonical OpsPulse inventory was not rerun; its last measured result remains
22/32 passing, with Theme and Portugal failing. Store-derived member semantics
were not modified. No supported Theme correction is claimed.

No passing-after implementation exists. Reset, shadowing, nested/multiple-instance
subscriptions, remount/destruction and cross-module-write prevention are not
verified. Counter, Reference, ReleaseDesk, Taskboard, CTest and verify_all.sh were
not run for this blocked test-only candidate. No new CI run exists. These omissions
are not acceptance passes.
