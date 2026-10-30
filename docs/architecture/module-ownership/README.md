# Deferred Friendly source ownership

Baseline: clean main `a3dec30fd4e682805a06164114ecb235a4922a6d`.
Fresh C++17 compiler, Python bindings off. Baseline executable SHA-256:
`9c8681c40629ea6b2a1f54589f2ed6a74fc9a0e86cfaad3975a6cfa00f19f677`.
No application sources, language syntax, runtime, release artifacts or budgets change.

## Independently reproduced baseline

Read `docs/application-inventory.md` and its CLI ledger first. All nine original
sources fail on unchanged main with exit 1. [before.json](before.json) retains
exact argv/stdout/stderr; [minimal-before.json](minimal-before.json) retains four
independent reductions. For example:

```sh
./build/jtml build examples/friendly_effect.jtml --target browser --out dist/ownership-before/examples/friendly_effect
```

The diagnostic is `Cannot build clean browser production module plan for <absolute
source>: Cannot determine module ownership in <absolute source>`. This happens
before executable module plans or browser assets, despite earlier check/lint success.

## Trace and classification

1. The source loader retains original file/range identity in `MappedSource`,
   including selected exports and transitive imported components.
2. `friendlyToClassicWithSourceMap` expands Friendly constructs. An effect emits
   a function immediately and a `subscribe` when its block closes. Its `OpenBlock`
   omitted `sourceLine`, so that subscription was attributed to line zero.
   Redirect, invalidate and guard declarations defer their `@meta` emission until
   after the main loop. They wrote directly to `out`, bypassing `emitLine` and
   therefore leaving no entries in `classicLineToFriendlyLine`.
3. The parser's `spanFromTokenRange` can neither recover an exact generated range
   nor an original attribution for those nodes. AST clones correctly retain this
   absence; the fault precedes cloning and semantic resolution.
4. Linked Semantic Program analysis can still report actions/effects/routes.
   However, `parseStandaloneModuleForBrowserBuild` must partition the linked AST
   into real module owners before `summarizeSemanticAst`, `cloneSemanticAst` and
   per-module analysis. It rejects the unattributed subscription/metadata node.
5. Consequently `buildSemanticProject` never receives a complete module source
   list. No import/export graph resolution or Runtime Project Plan is reached.
   When attribution is present, the graph assigns real module IDs, resolves
   exports/reexports, and `buildRuntimePlan` uses each retained module AST and
   annotates component definition/instance ownership. Clean emission follows
   the existing production validator and closed asset schema.

Thus all nine **ownership diagnostics have one root cause: lost attribution for
source-derived deferred nodes**, with two implementation omissions. These are not
legitimate unsupported-shape rejections, fake owners, missing exports, or mistakes
in these nine applications' import declarations. No filename exception or entry
module fallback is appropriate. This does not establish that every feature in
these applications executes correctly in the browser.

## Before / after matrix

All rows first fail at **Friendly AST module partition, before Semantic Project**.
`E` = effect subscription line zero; `M` = deferred metadata has no source-map entry.
`1 → 0` denotes clean-build process exit codes only.

| Original source | Diagnosis | Build before → after | Expected behavior / measured browser limit |
| --- | --- | --- | --- |
| `demos/index.jtml` | E first; also redirect/invalidate/guard M | 1 → 0 | Real module attribution restored. Layout renders, but route state changes, but page content/sign-in audit still fails; not an accepted application. |
| `examples/friendly_effect.jtml` | E | 1 → 0 | Increment should trigger effect text. Counter changes to 1; Last remains unchanged: FAIL. |
| `examples/friendly_fetch_invalidate.jtml` | invalidate M | 1 → 0 | Initial GET and another GET after Save: PASS with explicit local HTTP test endpoint. |
| `examples/friendly_redirect.jtml` | redirect M | 1 → 0 | Component button should navigate to dashboard: FAIL. |
| `examples/friendly_route_guards.jtml` | guard M | 1 → 0 | Empty token blocks dashboard and redirects to login: PASS for tested path. |
| `studio/samples/effects.jtml` | E | 1 → 0 | Increment should update Last/Direction. Count changes; effect output remains idle: FAIL. |
| `studio/samples/fetch.jtml` | invalidate group/all M | 1 → 0 | Initial GET and group invalidation GET: PASS with explicit local HTTP test endpoint. |
| `studio/samples/redirect.jtml` | redirect M | 1 → 0 | Component button should navigate to dashboard: FAIL. |
| `studio/samples/routes.jtml` | guard M | 1 → 0 | Prefilled token permits dashboard: PASS for tested path. |

[after.json](after.json) retains exact original clean-build commands/results.
[browser-after.json](browser-after.json) retains real Chromium observations, route
state, failed resources and failures. Browser tests use `script-src 'self'` and
ordinary UI actions. The users/teams HTTP endpoint is a declared test fixture,
not evidence of a deployed API or persisted application. Other demo API/host
contracts are not supplied. Startup checks assert clean mode and no JS exceptions;
they do not assert successful resources or application functionality.

## Bounded correction and regression

Carry the effect's authored line in its block. Retain the authored redirect,
invalidation and guard lines until emission, and emit through the existing
source-map writer. Combined invalidation metadata is attributed to its first
invalidation statement; it remains an attributed generated construct, never a
claim of token-exact source. Lowered bytes and ordering are unchanged.

The ownership gate remains fail-closed, with an expanded diagnostic explaining
the missing attribution, lowered location, and how to supply explain/debug
information. No source ownership is inferred for synthetic input. No semantic
resolution, module-ID policy, import selection, export identity, lexical binding,
component context, production validation or runtime dispatch is relaxed.

The new source-propagation tests fail against unchanged main
([unit-before.txt](unit-before.txt)). They check original file/line/column,
non-exact generated spans, cloning, synthetic inputs, and distinct imported/entry
origins. CLI regressions cover four independent minimal sources, imported actions
alongside imported component identity, and all nine unchanged inventory sources.
Existing missing-export, reexport, lexical-ownership, CSP/security, route and
production-reference tests remain required.

```sh
cmake -S . -B build -DJTML_BUILD_TESTS=ON -DJTML_BUILD_PYTHON=OFF
cmake --build build --parallel
ctest --test-dir build -R 'DeferredFriendly|InventoryOwnership' --output-on-failure
# Existing repository Playwright/Chromium environment variables are required:
node tests/browser/module_ownership_inventory.cjs
JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1 scripts/verify_all.sh
```

The inventory browser audit intentionally exits **1** while functional failures
remain. It is not inserted as an always-green gate or converted to expected-pass
assertions. Its output is `dist/module-ownership-audit/results.json`. Final full
gate and Ubuntu/macOS CI evidence, with the exact pushed commit, belongs in this
PR's acceptance record; a successful build or green CI cannot replace this audit.

## Unresolved, separate corrections

- **Root effects:** Runtime Plan emits the generated function as an action, but
  does not retain executable subscription wiring. The browser's root action
  execution updates state without dispatching those effect subscriptions. Both
  original effects files independently reproduce this; the minimal effect fixture
  isolates the missing-attribution issue before runtime. Do not implement a new
  effect scheduler in this ownership correction.
- **Component-to-global redirect dispatch:** redirect metadata registers a callback
  in `__jtml_refresh_fns`, consumed by `window.sendEvent`. Component button fallback
  uses `runDirectComponentFallbackAction` → `executeClientAction`, bypassing that
  interception. Both redirect files reproduce the failure. Fixing dispatch and its
  ordering/side-effect semantics is separate from source ownership.
- **Application binding boundaries:** the demo contains component `into` targets
  whose state is declared globally (including Command and Login). The route-guard
  example also declares `loginEmail` globally and binds it inside Login. Existing
  lint advisories identify these as lacking local writable `let` state. They exceed
  the documented component binding contract; the guard-denial test does not accept
  the login flow. The ownership diagnostic hid these independent source/backend
  limitations, and this PR does not repair the sources or broaden that contract.
- **Enterprise demo layout/navigation:** its initial surface lacks the intended
  page content and shows `Release undefined`; route state changes, but intended page content and sign-in do
  not complete in the audit. This broader composition/global-state boundary is
  unresolved. It also depends on `/api/posts`, `/api/runtime`, `/api/login` and
  `host.notify`; this PR does not implement those services or claim app acceptance.

**NO-GO for claiming these nine sources are clean-production functional.** The
ownership correction is reviewable independently; the PR stays draft with these
runtime failures explicit. Do not merge it as a complete production-support fix.
