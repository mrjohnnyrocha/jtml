# Root action commits before redirect

Baseline: clean main after PR #63, `3aefe6aded95938d87a39bae8ddbd08877e9a1f5`.
Dedicated worktree `root-action-redirect`, branch `codex/root-action-redirect`.
The Enterprise source, original detailed audit and budget are unchanged.

## Reproduction and ownership trace

The [unchanged Enterprise audit](enterprise-before.json) exits 1 in real Chromium
153.0.8010.12: Sign in navigates but leaves `demo-token` / `Ready for review.`;
Sign out navigates but keeps the token, and the Command guard permits re-entry.
The independent [minimal negative](before.json) fails six checks across root and
component global action dispatch; both redirect-only controls pass. Its
[source](../../../tests/fixtures/production/root-action-redirect/index.jtml) has
root state, multiple writes, an effect, parameterized actions, route guards,
root controls and component controls. The [supported-value Enterprise negative](enterprise-supported-before.json)
independently fails clearing the original illustrative token.

The failure crosses these existing owners:

1. Friendly lowering (`src/friendly.cpp`) collects `redirectActions` and emits
   `data-jtml-redirect-action` / `data-jtml-redirect-to` metadata. Root redirect
   statements are action-level metadata, not statements in the lowered AST.
   Source attribution of the metadata is retained. No source scanning is added.
2. AST / Semantic IR / Runtime Plan preserve the authored assignments and action
   parameters. `collectRuntimeStatements` supplies the normal root action body;
   `runtimePlanActionsObjectToJson` emits its expression plans in the manifest.
   A redirect-only action has a valid empty body.
3. `startRedirectBindings` indexes the metadata. `sendEvent` used to call
   `redirect(fnName)` before `executeClientAction`. Component-global fallback
   likewise used `redirect(name) || executeClientAction(...)`. Both paths stopped
   at navigation, skipping every assignment and parameter binding in the action.
   The original audit's one `executeClientAction` coverage call is unrelated
   input dispatch; it does not establish execution of signIn/signOut.
4. `executeClientAction` already owns parameter save/bind, Runtime Plan execution,
   restoration in `finally`, derived updates, root subscription flush and DOM
   updates. `assignClientPath` records writes; subscriptions coalesce writes per
   dispatch and observe final state after parameter restoration. Same-value
   assignments are writes. The first new test mistakenly expected equality
   deduplication on repeated token writes; its oracle was corrected to the
   existing documented semantics before the final successful run. This does not
   change the unchanged-main failure (execution counters stay zero).
5. `window.__jtml_redirect` writes the hash. Existing hash-change routing runs
   `applyRoutes` / `checkGuards` against committed `clientState`, then uses the
   existing route component mounting and lifecycle paths. History remains hash
   history; there is no replacement router, synthetic navigation or direct guard
   invocation.

## Execution order and correction

Redirect requests navigation; it is not a `return` or terminal action statement.
The language reference describes navigation and does not specify early return;
existing component `runComponentPlanStatements` explicitly continues after a
local redirect. Root metadata represents an action-completion redirect. The
fixture deliberately writes `tail` after the redirect to preserve this
nonterminal behavior. This change does not turn metadata into ordered/conditional
redirect statements or introduce a second action interpreter.

The runtime change has three small edits:

- Execute a resolved component-global Runtime Plan action before considering a
  metadata-only fallback redirect. A handled action returns true, so it cannot
  navigate or execute again through fallback.
- Inside `executeClientAction`, apply its redirect metadata only after restoring
  parameters and `applyClientState` (including effects). Exceptions retain the
  existing `finally` restoration and cannot fall through to navigation.
- Keep the early redirect interceptor only for the live backend. Browser-local
  root dispatch now reaches the same executor as component-global fallback.

Order: bind parameters → execute authored writes once → restore parameters →
commit derived/effect/render updates → request hash navigation → existing route
and guard processing. Root invalidations retain their existing dispatch path.
Component-local execution and live transport implementation are unchanged.

## Browser acceptance

The [minimal after](after.json) passes all nine checks: root/component globals,
multiple writes, nonterminal tail, once-per-click counters, same-value effects,
shadowed parameter restoration, removal of previously absent parameters,
effect state at the navigation boundary, updated guards, redirect-only controls,
back/forward without action replay, strict script CSP and clean assets.

The [supported-value Enterprise after](enterprise-supported-after.json) passes:
Sign out clears the token and writes `Signed out.` before requesting navigation;
Command re-entry is denied; Sign in with the unchanged supported module value
`ada@example.com` writes both token and audit then reaches Command. No input
state is injected, no source is edited, and no API is mocked.

The [original detailed audit after](enterprise-after.json) deliberately still
exits 1: 8 pass / 3 fail. Typing in component Email still leaves module
`loginEmail` at `ada@example.com`; the two assertions expecting
`probe@example.test` consequently fail. Those same action assignments now run
with the actual supported module value. Sign-out and guard assertions turn green.
This is an independent component-to-module input ownership limitation; it is
not hidden, inverted into an expected pass, or fixed by expanding `into`.
External API/media requests still return real 404s. The local illustrative token
is not external authentication.

The existing component redirect suite stays green, including original samples,
local state writes, sibling isolation, history, protected local redirects and
fallback arguments. Full gate also covers routing, effects and component lifecycle.

## Assets and verification

Shared runtime: **272,877 → 272,925 raw bytes (+48)**; gzip **50,828 → 50,841
(+13)**. Budget stays **273,000**, leaving **75 bytes**. No assertions or budgets
were loosened. The [asset hashes](assets.json) confirm unchanged fixture HTML, component modules and bootstrap.

Commands (with the real Chromium executable and Playwright root configured):

```sh
node tests/browser/enterprise_audit.cjs              # before 1, after 1: input limitation remains
node tests/browser/root_action_redirect.cjs          # before 1, after 0
node tests/browser/enterprise_action_redirect.cjs    # before 1, after 0
node tests/browser/component_redirect.cjs            # after 0
ctest --test-dir build -R 'Route|route|Effect|effect|Slot|slot|Action|action' --output-on-failure
JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1 JTML_REQUIRE_BROWSER_BENCH=1 scripts/verify_all.sh
```

Focused CTests: 67/67 pass. The [full gate](verification.json) exits 0 with
493/493 CTests, all nine mandatory clean-exit browser benchmarks, eight live
runtime smoke checks and no missing-tool skips. Five existing native
complete-attribute renderer discrepancies, reference HTML microfixture warning
and explicit catalogued UNKNOWN/excluded coverage remain outside this correction. Exact pushed-commit acceptance
and Ubuntu/macOS CI are recorded on the PR separately from local evidence.

**GO only for root action execution before redirect with the tested existing
metadata semantics. NO-GO for working component-to-module login input, real
Enterprise authentication or complete demo support.** No merge, release or
deployment is authorized or performed.
