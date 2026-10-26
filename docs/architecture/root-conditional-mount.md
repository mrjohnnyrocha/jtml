# Root-conditional component mounting

Baseline: merged PR #46 main `26eb51c638507b54c6632ccadb079635f92b668c`.
The ReleaseDesk reproduction is unchanged. This correction owns browser root
insertion/registration, not application code or JTML language semantics.

## First loss of ownership

Friendly expansion emits Child's instance marker, lowered body and local names.
Semantic IR records the definition, local state/action and physical call-site ID.
The client Runtime Plan preserves the instance's module ownership, parameters,
local map and typed body plans. An independent CTest verifies local `count` and
`increment` remain component-owned rather than global state/actions.

At startup, `scanComponentInstances()` previously copied every declaration into
the instance list, including records with no connected element. Root conditions
are initially empty template containers. `applyClientState()` later inserted
their lowered HTML with `innerHTML`, but did not register/render components.
Consequently placeholders remained and root event transport dispatched a lowered
name absent from the browser's global actions. This is the first missing runtime
step, not a missing semantic record or CSP failure.

## Owning correction

`replaceClientTemplate()` is called at the existing root condition/loop insertion
points. It synchronously uses the existing descendant destructor before replacing
HTML, resolves connected component roots against the manifest catalog, registers
them through `registerDynamicComponentInstance()`, and invokes
`renderDirectComponent()` and its generated static create path. Their wrappers
carry the existing direct-instance identity used by teardown and event ownership.
Parent creation replaces expanded nested markup; disconnected expanded children
are skipped, while the generated parent owns actual nested/keyed children.
There is no polling, observer, timer, new listener layer or second lifecycle
registry. The same delegated component events dispatch once to the live instance.

Startup registers only connected roots without another expanded component owner;
hidden declarations stay in the immutable manifest catalog, not the active
registry. Subsequent mounts use existing generation counters and fresh local
scopes. Existing static/dynamic instance lookup covers these roots, and observable
argument updates include dynamically mounted roots without reinitializing locals.
The touched initialization code reuses compiler-serialized manifest records and
compacts equivalent metadata to retain the **272,000-byte** shared-runtime limit.

Live/native event and rendering implementations are unchanged. Browser-local
compilation explicitly rejects components directly repeated by root loops: that
backend repeats one physical call-site ID without per-item ownership. The AST
placement check rejects before assets, naming the component and its original
file/line/column, with remediation to a component-owned keyed loop. It does not
reject ordinary root loop controls or existing component-owned keyed lists.

## Independent browser acceptance

`tests/browser/root_conditional_mount.cjs` builds the unchanged ReleaseDesk probe
and a new independent sibling/nested/keyed fixture. It serves the real four
production assets over loopback HTTP with `script-src 'self'` and drives Chromium.
All positive state changes use actual UI controls; observation hooks never inject
application state or provide frontend replacements.

- Initially hidden probe has no active instance; four show/hide cycles initialize
  at 0, increment exactly twice to 2, and return to an empty dynamic registry.
  Generations increase and retained detached buttons cannot resurrect instances.
- Two conditional regions and multiple instances keep independent local state.
  An initially true region mounts during initial application update; hiding and
  restoring it resets state.
- Three cycles exercise nested keyed counters and callback-owning children,
  reorder with retained DOM/draft, removal/recreation, exact callback counts,
  whole-subtree teardown and unchanged sibling DOM/state.
- Mounted root parameters update while retaining local state; root conditional
  controls and a growing plain root loop dispatch exactly once.
- Every connected direct-instance wrapper matches one live registry entry.
  Existing destruction diagnostics report no registry/scope/handler/dispatch
  leaks. No unexpected console errors, CSP violations or create/patch fallbacks.
- The unsupported direct root-loop component rejects at the independently fixed
  authored location (`root-loop-rejected.jtml:10:7`) before runnable assets.

Main failed the independently written browser test at first `Child: 0`, before
the correction. No prior fixture, oracle or threshold was edited. Existing CSP,
Counter, ReleaseDesk and keyed lifecycle/rollback tests run unchanged in the gate.

```sh
cmake --build build --parallel
ctest --test-dir build -R 'FriendlySyntax|AttributeClassifier' --output-on-failure
node tests/browser/root_conditional_mount.cjs
JTML_REQUIRE_BROWSER_BENCH=1 JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1 scripts/verify_all.sh
```

Set `JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT` and `JTML_BENCH_BROWSER` to real installed
Playwright/Chromium paths. The browser test is mandatory in both hosted CI jobs and
the local gate. Local and hosted evidence are distinct.

## Remaining limits

Direct root-loop components need a future per-item ownership plan; they now fail
closed rather than sharing one scope/identity. Component-owned keyed loops remain
supported. No new async subscription/effect lifetime semantics are introduced.
The existing explicit `value draft into draft` component-input pattern is tested;
implicit component `input into draft` without `value` independently exposed a
compiled-patch fallback and remains outside this lifecycle fix. Root ID/label,
programmatic root SELECT property, five native renderer observations and reference
HTML size overage remain known limitations. Raw HTML/extern/live-preview CSP and
arbitrary external resources are outside this bounded clean-browser proof.

Local verification: **130/130 focused CTests**, **476/476 full CTests**, new
strict-CSP lifecycle acceptance and the complete gate **PASS, exit 0**, run once
on the stable candidate. All nine mandatory benchmarks exited cleanly; existing
CSP/Counter/ReleaseDesk, keyed lifecycle/rollback, provenance, security, package/
install and live smoke (8/8) pass, with no verification checks skipped. Shared
runtime is **271,926 / 272,000 bytes** (+71, 74 bytes headroom); app.js stays
**2,043 / 20,000**. Known reference HTML remains **99,939 / 70,000** and five
native renderer observations remain known failures. No acceptance limit changed.

Compact results are recorded in [ReleaseDesk verification](../../apps/releasedesk/verification.md).
