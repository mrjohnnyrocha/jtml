# JTML Competitive Features Roadmap

Status: active implementation roadmap.

JTML is currently a reactive templating language with a compact syntax and a native CLI. To compete with React, Vue, Angular, Next.js, Nuxt, and SvelteKit, the project needs production features in four tiers.

## Tier 1: Must-Have

| Feature | Status |
| --- | --- |
| `fetch` async data | Hardened: Friendly `let name = fetch "url"` creates the stable `{ loading, data, error, stale, attempts, hasData }` core plus browser-local metadata (`status`, `ok`, `url`, `key`, `method`, `updatedAt`), supports `method`, JSON `body`, `cache`, `credentials`, `timeout`, `retry`, `stale keep|clear`, `group`, `key`, `dedupe`, `every`/`revalidate`, `background`, `lazy`, `refresh actionName`, action-scoped `invalidate name`, `invalidate group name`, `invalidate all`, and browser-side re-rendering for simple `show`/`if`/`for` bindings |
| client-side `route` | Hardened: `route "/path" as Component`, `:param` bindings, wildcard fallback, `layout Layout`, `load lazyFetch`, `link ... to "/path"` hash nav, `link ... active-class "cls"` reactive active-link highlighting, `guard "/path" require var [else "/fallback"]` client-side route guards, `activeRoute` built-in reactive variable |
| scoped `style` blocks | Implemented first slice: Friendly `style` lowers to CSS scoped under `[data-jtml-app]` |
| true component isolation | Architectural bridge shipped: repeated Friendly component calls still get per-instance lowered names for compatibility, but each wrapper now executes inside a real per-instance runtime environment; semantic component definitions expose runtime plans with local state/actions/derived/effects, event bindings, slot/body shape, and action ownership; element bindings, event handlers, dirty recalculation, `/api/state`, `/api/components`, `/api/component-definitions`, and `/api/component-action` all resolve through runtime metadata. Full non-expanded `ComponentInstance` template execution remains planned |

## Tier 2: Differentiators

| Feature | Status |
| --- | --- |
| built-in `store` | Implemented first slice: Friendly `store name` lowers to shared dictionary state with `name.field` reads, store-field assignments, and namespaced store actions such as `auth.logout` |
| `effect` reactive side effects | Implemented first slice: Friendly `effect variable` lowers to a generated subscription function |
| zero-config dev server | Implemented first slice: `jtml dev <file.jtml|app/>` hot-reloads and serves static assets |
| AI-native authoring | Implemented first slice: `jtml generate`, `jtml explain`, `jtml suggest`, `jtml fix`, source-preserving Friendly formatting, [`../reference/ai-authoring-contract.md`](../reference/ai-authoring-contract.md), structured repair diagnostics for CLI/Studio/LSP, Friendly parse source-line remapping, and targeted LSP repair actions such as event handler creation |
| media and graphics | First slices implemented: current aliases cover `image`, `video`, `audio`, `embed`, `file`, `dropzone`, `graphic`, `bar`, `dot`, `group`, `chart bar`, `chart line`, grouped/stacked/multi-series charts, chart annotations, `export svg png csv` chart controls, and `scene3d`; file selections produce previewable file objects; `video/audio ... into name` exposes browser-side playback state plus `play`/`pause`/`seek` actions; `scene3d ... into sceneState` exposes renderer/fallback state and host updates; raw canvas/SVG/custom elements and `extern` bridges work today; the linter warns on missing media accessibility attributes and first 3D production issues. Next slices should add richer chart mark styling, 3D renderer packages, and Studio media inspectors |

## Tier 3: Ecosystem

Package management now has a local-first first slice (`jtml add` installs files/directories into `jtml_modules`, records `jtml.packages.json`, writes deterministic `jtml.lock.json` fingerprints, `jtml install` restores/verifies package state for CI, and bare imports resolve through nearest `jtml_modules`). The CLI implementation now has explicit module/package owners in `cli/module_loader.*` and `cli/package_manager.*`; semantic explain now reports structured import records and module files. Remote registries, semantic versions, richer object/collection type diagnostics, deeper package-aware editor intelligence, and runtime component objects remain planned after the P0/P1 language/runtime pieces stabilize.

## Tier 4: Moonshots

Full-stack one-file JTML remains a future track. It should build on stable routing, fetch, stores, and server/runtime separation rather than shortcut around them.

## Priority Matrix

| Feature | Effort | Impact | Priority |
| --- | --- | --- | --- |
| `fetch` async data | Medium | Critical | P0 |
| Client-side routing | Medium | Critical | P0 |
| Scoped styles | Small | Critical | P0 |
| Component isolation | Large | Critical | P0 |
| Built-in `store` | Medium | High | P1 |
| `effect` reactive | Small | High | P1 |
| Zero-config dev/app scaffold | Small | High | P1 |
| AI-native tooling | Medium | High | P1 |
| Media and graphics | Medium | High | P1 |
| Studio = language home | Medium | High | P1 |
| JTML code modularization | Medium | High | P1 |
| Language interop (FFI) | Large | High | P2 |
| Package manager | Local package install + bare `jtml_modules` import resolution + deterministic `jtml.lock.json` + `jtml install` shipped; remote registry and version solver remain | Medium | P2 |
| Type annotations | Primitive mismatch + action arity checking shipped; richer object field checks remain | Medium | P2 |
| IDE / LSP | VS Code highlighting/snippets, CLI fallback diagnostics/format/fixes, and native `jtml lsp` diagnostics, formatting, completions, hover, document/workspace symbols, cross-file definition, references, rename, signature help, document highlights, selection range, source fix-all, and targeted event-handler quick-fixes shipped | Medium | P2 |
| Full-stack mode | Very large | Moonshot | P3 |

## New tracks (this sprint)

### Studio as the language home

`jtml studio` should be both the IDE and the public web home for the language.
The shell stays the same split-view editor, but its *content* is authored in
JTML (under `tutorial/`) so JTML dogfoods itself as a content platform. The
home lesson welcomes new users, links to the AI authoring contract, the
language reference, and the example gallery, and frames the progressive lesson
path. Every lesson is canonical Friendly JTML so what users read is what
`jtml fmt` produces.

### JTML code modularization

JTML is currently single-file. To compete with module-aware ecosystems, the
language needs:

- Friendly `use Component from "./file.jtml"` and `use { foo, bar } from "./mod.jtml"`.
- Bare package imports such as `use Button from "ui-kit"` resolve through the
  nearest `jtml_modules/ui-kit/index.jtml` or `package.jtml`.
- A real module graph resolved relative to the importing file with cycle
  detection. The current first slice covers Friendly `use`, nested relative
  imports from each importer, and shared `check` / `build` / `serve --watch` /
  `dev` / `explain` graph loading.
- Per-file scoped state, components, and stores; only `use`-declared names
  cross the boundary. Explicit-export filtering now powers named imports in
  the compatibility lowering path: requested names must be exported, missing
  exports list available public names, duplicate exports are errors, and
  private declarations do not cross named-import boundaries. Re-export barrels
  such as `export use Card from "./card.jtml"` forward public declarations.
  Remaining P1 work: full semantic module ownership and imported store
  identity polish.
- An app shape under `pages/`, `components/`, `stores/`, `assets/` consumed by
  `jtml dev app/` and `jtml build app/ --out dist`.

### Language interoperability

JTML's runtime is C++; the production target is HTML+JS. Useful interop hooks
across the stack:

- **Frontend interop**: `extern function jsCall(name, args)` for calling
  user-supplied JavaScript in the browser. First slices shipped as
  `extern action from "window.path"` browser-hosted actions, explicit
  `html raw` / `css raw` escape hatches for trusted host surfaces, plus a
  stable runtime API so other frameworks (React, Vue, Svelte islands) can
  mount a JTML page as a custom element.
- **Backend interop**: `jtml serve` exposes `GET /api/health`,
  `/api/bindings`, `/api/state`, `/api/runtime`, and `POST /api/event` as a
  stable HTTP contract so backends and tools in any language (Node, Python, Go,
  Rust, IDEs, test runners) can talk to a JTML page.
- **Embedding**: keep `jtml_engine` (pybind11) as the reference embedding and
  ship a small C ABI (`jtml_render`, `jtml_load`, `jtml_dispatch`,
  `jtml_component_action`) so any language with FFI can host the runtime.
- **Out**: a `jtml export --target react|vue` lowering that emits a
  framework-native component wrapping the transpiled HTML + bindings, so
  existing apps can adopt JTML page-by-page.

### Media and graphics

JTML should compete in media-heavy and visual interfaces, not only form-and-list
apps. The current language can embed `image`, `video`, `audio`, `embed`, raw
`canvas`, SVG/custom elements, and host-provided browser APIs through `extern`.
The next track should make this AI-friendly and production-shaped:

- ✅ `file` and `dropzone` aliases with `into` bindings for selected files.
- ✅ Reactive media controllers: `video src movie controls into player` exposes
  `player.currentTime`, `player.duration`, `player.paused`, and actions such
  as `player.play`, `player.pause`, and `player.seek(0)`.
- Browser-safe image metadata, previews, thumbnails, resize/crop/filter helpers
  for small assets, with heavier processing delegated to explicit CLI/server
  pipelines.
- Audio/video metadata and preview helpers: waveform, duration, poster frame,
  thumbnail, upload/progress state.
- ✅ SVG-first authoring aliases: `graphic` lowers to accessible SVG, and
  `bar`, `dot`, `line`, `path`, `polyline`, `polygon`, `svgtext`, and `group`
  lower to standard SVG shape tags.
- ✅ Accessible SVG charts: `chart bar data rows by label value total`,
  `chart line ...`, axis labels, legends, grid lines, grouped multi-series
  bars, stacked bars, and multi-series line charts.
- ✅ 3D mount contract: `scene3d "Product" scene productScene camera orbit into sceneState`
  lowers to an accessible canvas, calls `window.jtml3d.render(canvas, spec)`
  when available, draws a fallback otherwise, and publishes renderer status.
- Studio media/graphics workbench: asset previews, drag-and-drop examples,
  chart/graphic gallery, and a live media-state inspector.

Detailed plan: [`media-graphics-roadmap.md`](media-graphics-roadmap.md).

## Implementation Order

1. ✅ Ship scoped styles, `jtml dev`, static asset builds, and AI-native helper commands so the authoring loop feels complete.
2. ✅ Harden `fetch` first slice: `method`, JSON `body`, `cache`, `credentials`, `timeout`, `retry`, `stale keep|clear`, `lazy`, `refresh actionName`, and action-scoped `invalidate fetchName` options wire production-oriented browser requests; runtime intercepts refresh actions, refreshes invalidated fetches after mutation actions dispatch, starts lazy fetches from route `load`, and avoids server round-trips where safe. `redirect "/path"` Friendly keyword added for programmatic hash navigation from action bodies.
3. ✅ Studio revamped: Friendly-2.0 samples (counter, form, dashboard, fetch, POST fetch, store, effects, routes, redirect, media, charts, components), categorized sidebar, full reference panel covering every language feature, generated Compatibility/HTML artifact inspector, updated keyword highlighting.
4. ✅ Harden routing: `link active-class` reactive active-link highlighting, `guard "/path" require var else "/fallback"` client-side route guards checked on every hash change, `activeRoute` built-in reactive variable always reflects current path.
5. ✅ Component instance markers and runtime registry: each component call emits `<div data-jtml-instance="Name_N">` with public component name, role, params, local binding map, and source line; each `make Component` also emits a hidden definition marker with params, source line, encoded Friendly body, body/root-template counts, slot count, local state/actions/derived/effects, event bindings, and a semantic `runtimePlan`. The browser runtime scans instances into `window.__jtml_components` / `window.jtml.getComponentInstances()` and dispatches `jtml:components-ready`. The interpreter treats component wrappers as semantic boundaries, executes their local `let`/`when` declarations inside per-instance runtime environments, resolves element bindings/events through the owning instance, validates local action dispatch against the semantic definition, exposes component-local state/actions in `/api/state` and `/api/components`, exposes original component contracts through `/api/component-definitions`, and lets tools call local actions through `/api/component-action`.
6. ✅ AI authoring contract and implementation roadmap added under `docs/`.
7. ✅ Friendly formatter path now preserves high-level Friendly constructs in CLI and Studio instead of lowering them to Classic artifacts.
8. ✅ Repairable diagnostics first slice: shared diagnostic contract with severity/code/message/line/column/hint/example, JSON output for `check`/`lint`, and Studio API diagnostic arrays on parse/lint failures.
9. ✅ `jtml fix` first slice: safe mechanical repairs for missing Friendly header, tab indentation, trailing whitespace, final newline, plus JSON diagnostics for remaining parse errors.
10. ✅ `jtml explain --json` richer summary: state, constants, derived values, actions, components, routes, structured semantic route and fetch records, stores, effects, style blocks, diagnostics, first-slice semantic node counts, dependency edges, and attribute classification sourced from the parsed AST.
11. ✅ Friendly source-map diagnostics first slice: parse diagnostics produced after Friendly-to-Classic lowering are remapped back to original Friendly source lines in `jtml check --json` and `jtml lsp`.
12. ✅ First direct component body-plan execution slice: browser-local component instances can render common templates from `RuntimePlan.componentDefinitions[].bodyPlan`, initialize per-instance local state, render simple `if`/`else`/`for` control flow, carry authored slot plans, render slots and nested component calls, preserve common attributes, pass simple action arguments, execute simple local assignment actions, and re-render the owning instance. Nested browser-local component calls now register addressable runtime identities for local action ownership. The live interpreter exposes matching slot/body-plan metadata, executes simple body-plan assignment actions with action arguments while falling back safely for unsupported bodies, and exposes first-slice `renderedHtml` from the same body plan for tooling/parity checks. Browser-local and live body-plan renderers now preserve semantic UI primitives and modifiers as `jtml-*` classes plus `data-jtml-ui*` attributes. `jtml serve` also exposes `/api/rendered-components`, carries `renderedComponents` in event responses, injects supported live component wrappers from body-plan HTML into the initially served document, keeps rendered component buttons interactive through `/api/component-action`, and preserves compatibility DOM fallback for unsupported shapes. Nested component calls also have a first emitted-event bridge and explicit contract: `make Child emits picked`, `make Child emits picked(item)`, and `make Child emits picked(item: string)` are exposed through semantic/runtime JSON with `emitArity`, `emitPayloads`, and `emitPayloadTypes`, and `Child on picked choose("preset")` routes child direct actions through validated parent handlers in browser-local and live runtimes, forwarding emitted args after any preset handler args. Declared payload arity is checked against known parent action signatures, and browser-local/live direct component dispatch enforce simple emitted payload types. Action body lines such as `incBy(2)` and `picked("Ada Lovelace")` now lower to body-plan `call` nodes for direct local-action dispatch or declared event emission. Next: broaden the supported body-plan subset until expanded compatibility output becomes a deliberate fallback rather than the normal supported-component path.
13. ✅ Route layout first slice: `route "/path" as Page layout AppLayout` injects route content into the layout component's `slot`, so shared chrome (nav, footer) wraps multiple routes.
14. Next: Studio as the language home — new `00-welcome` JTML-authored lesson, every lesson in Friendly syntax, Studio default tab is the home lesson.
15. ✅ JTML code modularization first slice: Friendly `use … from` is resolved before lowering so imported Friendly components, state, and actions can participate in `check`, `build`, `serve --watch`, `dev`, and `explain` module graphs with cycle detection. Nested relative imports resolve from each importing file. `jtml explain` also has a recoverable module-graph path: if an imported module fails to parse, the entry can still explain, the failed module remains in the semantic project, and `JTML_MODULE_PARSE` reports the module path plus best-known source coordinates.
16. ✅ Per-file export scopes first slice: Friendly `export let`, `export when`,
    `export make`, `export store`, and related top-level declarations are
    accepted; named imports require matching public declarations, missing
    exports list available names, duplicate exports are diagnostics, private
    declarations stay private under named imports, and side-effect imports
    still load whole files for compatibility.
17. ✅ App folder shape first slice: `jtml new app <dir>` creates `index.jtml`, `components/`, `stores/`, and `assets/`, and the generated app builds through the same zero-config module graph.
18. ✅ Framework export first slice: `jtml export <file> --target html|react|vue -o out` emits static HTML or iframe-based React/Vue wrapper components for incremental adoption.
19. ✅ Custom element export first slice: `jtml export <file> --target custom-element -o jtml-app.js` emits a standards-based `<jtml-app>` Web Component that can be embedded by plain HTML, React, Vue, Svelte, Angular, or any CMS shell.
20. ✅ Runtime HTTP contract first slice: `jtml serve`/`jtml dev` expose `/api/health`, `/api/bindings`, `/api/state`, `/api/runtime`, and richer `/api/event` responses with bindings + state; documented in [`../tooling/runtime-http-contract.md`](../tooling/runtime-http-contract.md).
21. ✅ Browser extern action first slice: `extern notify from "host.notify"` declares a client-side action, and event calls such as `click notify("Saved")` invoke `window.host.notify("Saved")` before server dispatch.
22. ✅ C ABI first slice: release builds now produce `libjtml` plus `include/jtml/c_api.h`; hosts can render Friendly/Classic source, load a runtime context, inspect bindings/state/components JSON, dispatch DOM events, and call component-local actions without shelling out to the CLI.
23. ✅ Type-aware linter first slices: `let`/`const`/`get` annotations preserve through Classic lowering and formatter output, the linter reports primitive mismatches as `JTML_TYPE_MISMATCH`, and ordinary action/function arity mismatches as `JTML_ARITY`.
24. ✅ Editor support first slices: VS Code grammar highlights Friendly JTML 2 declarations/keywords, snippets now produce canonical app, component, fetch, route, store, effect, scoped style, raw HTML/CSS escape hatch, input binding, and extern examples, the extension launches `jtml lsp` when available and falls back to CLI diagnostics, formatting, and safe fixes.
25. ✅ LSP intelligence: completions include same-file and imported user-defined symbols, hover works across the module graph, go-to-definition jumps through `use` / `import`, workspace symbols scan the project, references and rename share a string/comment-aware scanner, signature help understands `make`/`when`/`function` parameters, document highlights and indentation-based selection ranges are implemented, and code actions include both `source.fixAll.jtml` and targeted `JTML_EVENT_ACTION` handler creation.
26. ✅ Local package manager first slice: `jtml add <path|name> [--from path]` installs local `.jtml` files or package directories into `jtml_modules/`, records `jtml.packages.json`, writes deterministic `jtml.lock.json` package/file fingerprints, `jtml install [--json]` restores missing local packages and verifies lockfile fingerprints for CI, and the CLI/linter/LSP module graph resolves bare package specifiers through nearest `jtml_modules`.
27. ✅ Media/graphics first slice: docs/examples, `canvas`/SVG-friendly element parity, `file`/`dropzone` bindings, runtime file previews, drag/drop dispatch, reactive audio/video controllers, media client actions, SVG-first `graphic`/`bar`/`dot`/`line`/`path`/`polyline`/`polygon`/`group` aliases, accessible SVG bar charts, `scene3d` 3D mounts/state bindings, host `spec.update(...)`, and media/3D lint warnings.
28. ✅ Richer chart primitives: line charts, axes, legends, grid lines, grouped
    bars, stacked bars, multi-series lines, and series colours.
29. ✅ Chart export controls shipped: `export svg png csv` adds browser-local export buttons backed by the same SVG renderer and chart data rows.
30. Next: richer chart mark styling, 3D renderer packages, Studio media inspectors.
31. ✅ Browser manifest hardening: client manifests now use compact execution
    serializers, omit source paths/body payloads/tooling summaries, escape
    script-breaking JSON, mark import-stub modules as tooling-only, and use
    module-aware component identities before name-only compatibility fallback.
32. ✅ Live component action parity first slice: direct component action
    arguments and assignment operators now run in the live interpreter over the
    same body-plan contract, with browser/live metadata parity tests.
33. ✅ Live component template surface first slice: live component state now
    exposes body-plan `renderedHtml` for common templates, parameter
    expressions, slots, and nested component calls.
34. ✅ Module-aware live component identity: interpreter runtime definitions,
    instances, `/api/state`, and `/api/component-definitions` now carry module
    IDs and prefer module-scoped component lookup before name-only
    compatibility fallback. Duplicate exported component names are pinned in
    RuntimeProjectPlan tests.
35. ✅ Import-aware nested body-plan identity: body-plan template nodes now
    carry `definitionModule`, so nested component calls authored inside
    imported component bodies bind to the exported target module before
    compatibility fallback.
36. ✅ Live rendered component action bridge: component buttons produced by
    the live body-plan render surface now carry direct action metadata and the
    browser runtime dispatches them through `/api/component-action`, preserving
    simple action arguments and patched rendered HTML updates.
37. ✅ Body-plan render support truthfulness: component state and
    `/api/rendered-components` now expose `renderedHtmlSupported`, so
    state/action-only components do not claim live template parity and stay on
    compatibility DOM fallback.
38. ✅ Initial live body-plan transport ownership: supported component wrappers
    are body-plan-rendered in the initial served HTML and marked with
    `data-jtml-live-body-plan-transport="body-plan"` plus a stable rendered
    hash; the browser runtime verifies current markup without a startup DOM
    patch and uses `/api/rendered-components` only for refresh/action updates.
    Route links rendered through the live body-plan path preserve hash
    navigation metadata.
39. ✅ Rich platform attributes in direct body-plan rendering: browser-local
    and live body-plan renderers preserve a wider form/media/SVG attribute
    surface, `aria-*`, `data-*`, quoted values with spaces, and Friendly input
    aliases with sensible default `type` attributes.
40. ✅ Live nested body-plan instances: supported nested live components now
    retain dynamic runtime identity, route nested local actions through
    `/api/component-action`, evaluate params/state as values, and keep separate
    local state for repeated nested components inside `for` loops. Stale nested
    descendants are pruned after supported top-level renders, so removed
    loop/branch children cannot keep accepting component actions. Browser-local
    direct action arguments now use the same top-level parser for quoted strings
    with spaces.
41. ✅ First keyed/reordered collection lifecycle slice: component `for` loops
    can carry an explicit `key` tail in the semantic body plan, and nested
    component identity now follows keyed items through reorder. Next: richer
    component modifiers, broader keyed diff semantics, remote registry,
    semantic versions, richer incremental LSP text sync,
    package-aware auto-imports, stores, richer object field/collection types,
    and runtime `ComponentInstance` objects once P0 runtime semantics are
    stable.
42. Planned, not implemented: contract-first JTL backend APIs for governed
    internal operations. The intended wedge is operational UI over real backend
    actions: approval consoles, incident consoles, AI-governance controls,
    cost controls, deployment controls, and support operations. Phases:
    `type`/`error`/API operation signatures, OpenAPI generation,
    policy/validation hooks, runtime adapters, typed JTML `fetch`/future
    `call` integration, and enterprise hardening.
