# Enterprise route-layout composition

Baseline: clean merged main `c98fb9d87e1530443f62b53522c6566b8e348fb0`
(after PR #62). Dedicated worktree `enterprise-browser`, branch
`codex/enterprise-browser`. The Enterprise source and existing inventory oracle
are unchanged. This PR corrects caller-owned content in **nested default layout slots**;
it does not implement authentication or declare the Enterprise demo functional.

## Independent observations

The unchanged [inventory before](before.json) reproduces both failures in real
Chromium 153.0.8010.12: the Command route selects `/command`, but the heading is
missing; the sign-in check times out locating Email, before action dispatch.
All nine builds exit 0. The inventory audit exits 1.

The [detailed audit](detailed-before.json), [diagnostic controls](diagnostic-controls.json)
and [minimal regression](nested-regression-before.json) distinguish the boundaries:

1. **Compiler composition defect.** The source contains Command and Login bodies
   and declares both with `layout AppLayout`; no page composition is missing from
   the application source. Friendly route expansion inserts the route instance
   directly where the layout's `slot` occurs. Unlike ordinary component expansion,
   it omits `data-jtml-supplied-slot` and a semantic slot invocation. Semantic IR
   records the real route/layout instances, but the layout has empty slot metadata;
   Runtime Plan carries that emptiness into the manifest. Initial generated HTML
   contains the routed content. At bootstrap, static layout `create` replaces the
   layout DOM. `directStaticSlotCreateExpression` copies caller DOM only from the
   owning instance's supplied-slot marker, so it produces an empty slot. The
   router still selects the correct route. The minimal no-layout control renders
   Command; the otherwise equivalent nested-layout control fails on unchanged main.
2. **The reported sign-in failure is initially blocked by the same composition
   defect.** There is no accessible input and no sign-in click, action resolution
   or navigation to classify at that point. After restoring the slot, the original
   inventory's input-location and navigation assertions pass unchanged.
3. **Independent module-input ownership limitation remains.** A login-only
   control retaining the original Login/signIn source removes layout and unrelated
   media/data components. Typing in its raw email input does not change module
   `loginEmail`; the direct component binding validator rejects a target with no
   component-local `let`. In the restored original caller DOM, typing likewise
   leaves module `loginEmail` unchanged. The source exceeds the existing component
   `into` contract; this PR does not silently expand that contract or invent owners.
4. **Independent redirect/state execution defect remains.** The real generated
   action plan contains the authored token/audit assignments. Coverage observes
   redirect dispatch, but no `executeClientAction` for signIn/signOut: the existing
   redirect interceptor navigates before ordinary root action execution. After
   sign-in, token remains `demo-token` and audit remains `Ready for review.`;
   after sign-out, token still remains populated. The settled Command navigation
   is consequently allowed by the guard. A passing redirect with a prefilled token
   is not evidence of sign-in semantics or authentication. Correcting execution
   ordering is a separate action/redirect change and is deliberately deferred.
5. **External requirements remain external.** Static serving supplies no
   `/api/login`, `/api/posts`, `/api/runtime`, host.notify or illustrative media
   files. POST login is the separate `sessionProbe` fetch, not the signIn action.
   This audit returns real 404s and supplies no fake authentication. Missing
   services do not explain the initial missing DOM or the local action's skipped
   assignments. The original demo explicitly seeds an illustrative local token;
   neither that source nor route guards are changed.

The broader full-file no-layout diagnostic encountered separate initialization
errors (`Unescaped string delimiter`, `Unsupported component argument update`).
It is retained, but is not used as an isolated login proof.

## Bounded correction

Friendly lowering wraps routed content in the same caller-owned supplied-slot
marker used by ordinary component calls. It retains the already generated route
instance ID, parameters, local bindings and source line. The layout's semantic
slot is a compact invocation of the declared route component and route parameters;
Semantic IR and Runtime Plan preserve this relation and normal module annotation.
There is no generated fake owner, filename special case, runtime interpreter,
router change or fallback. Existing static component functions consume the marker.
No compiler/runtime code outside `src/friendly.cpp` changes.

An intermediate expanded slot record copied the entire lowered route subtree,
adding 1,105,118 HTML bytes. That candidate was stopped (gate exit 143) and its
[asset measurements](assets-intermediate.json) retained. The final record stores
only the component invocation, avoiding that demonstrated duplication. Shared
runtime and component JS bytes do not change; budgets are unchanged. The final
[asset ledger](assets.json) records Enterprise HTML **794,017 → 803,485 raw**
and **65,755 → 66,513 gzip** (+9,468 / +758); runtime stays **272,877 raw /
50,828 gzip**, component JS stays **754,535 raw**, and bootstrap stays **2,043 raw**.

## Browser acceptance and remaining negatives

[Inventory after](after.json) passes every original check, including Command
content and sign-in navigation, with audit exit 0. The stronger
[detailed after](detailed-after.json) still exits **1** for input/state/guard
behavior. Its expectations are not inverted or converted to expected passes.

| Observation on unchanged Enterprise | Before | After |
| --- | --- | --- |
| Clean build | 0 | 0 |
| Command route selected | PASS | PASS |
| Command heading visible | FAIL | PASS |
| Email input visible/accessible | FAIL | PASS |
| Sign-in navigation in original inventory | FAIL before click | PASS |
| Input mutates module loginEmail | Blocked by missing DOM | FAIL |
| Sign-in token and audit assignments | Blocked by missing DOM | FAIL |
| Sign-out clears token; subsequent guard denies Command | Blocked by missing DOM | FAIL |
| External application API/media available | FAIL | FAIL |

The source's component-to-module reads, missing layout appName/release values,
and unresolved isolated-counter text are additional limitations exposed by restored
content, not repaired application functionality. No authentication claim is made.

The first minimal probe also exposed a **separate root-level slot omission**:
`buildComponentBodyPlan` marks only `kind == template` as render roots, omitting
an unwrapped top-level `slot`. The original failing fixture is retained at
`tests/fixtures/production/route-layout/index.jtml`; the same oracle still exits
1 for it. Enterprise uses a slot nested under `page`, so this is outside the
bounded correction. The nested fixture is independently proven red on unchanged
main, rather than treating the root-slot negative as green.

```sh
# Set the repository's real Chromium / Playwright environment variables.
node tests/browser/module_ownership_inventory.cjs          # before 1, after 0
node tests/browser/enterprise_audit.cjs                    # remains 1
node tests/browser/route_layout.cjs                        # nested: before 1, after 0
JTML_ROUTE_LAYOUT_SOURCE=tests/fixtures/production/route-layout/index.jtml \
  node tests/browser/route_layout.cjs                      # separate root-slot negative: 1
ctest --test-dir build -R 'RouteLayoutsRetain|RouteAndLayout|Slot|Route|route' --output-on-failure
JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1 scripts/verify_all.sh
```

The new required browser regression asserts route selection, no-layout control,
Command/Login content with nested layout, shell/footer visibility and no page
exceptions under strict script CSP. The compiler regression checks the compact
slot invocation, parameter identity, source attribution, local binding identity
and absence of source-span leakage. Existing inventory/security/route/component
lifecycle assertions are unchanged. The detailed audit is deliberately not a green
acceptance suite while its independent defects remain.

Final full-gate, nine clean-exit benchmark and exact-commit Ubuntu/macOS evidence
are recorded separately in the PR. **GO only for the validated nested route-slot
content correction; NO-GO for functional Enterprise sign-in or complete demo
support.** No release assets, deployment or automatic merge are included.

## Local gate evidence

The [recorded gate](gate.json) exits **0** with **493/493 CTests**, all nine
required Chromium benchmark clean exits, strict CSP/security and route/component
lifecycle regressions, package/relocation/LSP/editor checks and eight live runtime
smoke checks. [Verification](verification.json) records exact commands, exits and
measurements. This does not turn the separate Enterprise or root-slot negative
audits green. Exact pushed-commit acceptance and Ubuntu/macOS CI are on the PR.
