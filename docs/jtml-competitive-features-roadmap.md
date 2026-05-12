# JTML Competitive Features Roadmap

Status: active implementation roadmap.

JTML is currently a reactive templating language with a compact syntax and a native CLI. To compete with React, Vue, Angular, Next.js, Nuxt, and SvelteKit, the project needs production features in four tiers.

## Tier 1: Must-Have

| Feature | Status |
| --- | --- |
| `fetch` async data | Hardened: Friendly `let name = fetch "url"` creates `{ loading, data, error, stale, attempts }`, supports `method`, JSON `body`, `cache`, `credentials`, `timeout`, `retry`, `stale keep|clear`, `lazy`, `refresh actionName`, action-scoped `invalidate name`, and browser-side re-rendering for simple `show`/`if`/`for` bindings |
| client-side `route` | Hardened: `route "/path" as Component`, `:param` bindings, wildcard fallback, `layout Layout`, `load lazyFetch`, `link ... to "/path"` hash nav, `link ... active-class "cls"` reactive active-link highlighting, `guard "/path" require var [else "/fallback"]` client-side route guards, `activeRoute` built-in reactive variable |
| scoped `style` blocks | Implemented first slice: Friendly `style` lowers to CSS scoped under `[data-jtml-app]` |
| true component isolation | Architectural bridge shipped: repeated Friendly component calls still get per-instance lowered names for compatibility, but each wrapper now executes inside a real per-instance runtime environment; element bindings, event handlers, local state/actions, dirty recalculation, `/api/state`, `/api/components`, `/api/component-definitions`, and `/api/component-action` all resolve through runtime metadata. Full non-expanded `ComponentInstance` AST execution remains planned |

## Tier 2: Differentiators

| Feature | Status |
| --- | --- |
| built-in `store` | Implemented first slice: Friendly `store name` lowers to shared dictionary state with `name.field` reads, store-field assignments, and namespaced store actions such as `auth.logout` |
| `effect` reactive side effects | Implemented first slice: Friendly `effect variable` lowers to a generated subscription function |
| zero-config dev server | Implemented first slice: `jtml dev <file.jtml|app/>` hot-reloads and serves static assets |
| AI-native authoring | Implemented first slice: `jtml generate`, `jtml explain`, `jtml suggest`, `jtml fix`, source-preserving Friendly formatting, `docs/ai-authoring-contract.md`, structured repair diagnostics for CLI/Studio/LSP, Friendly parse source-line remapping, and targeted LSP repair actions such as event handler creation |
| media and graphics | First slice in progress: current aliases cover `image`, `video`, `audio`, `embed`, `file`, `dropzone`, `graphic`, `bar`, `dot`, `group`, `chart bar data rows by label value total`, and `scene3d`; file selections produce previewable file objects; `video/audio ... into name` exposes browser-side playback state plus `play`/`pause`/`seek` actions; `scene3d ... into sceneState` exposes renderer/fallback state and host updates; raw canvas/SVG/custom elements and `extern` bridges work today; the linter warns on missing media accessibility attributes and first 3D production issues. Next slices should add richer chart primitives, 3D renderer packages, and Studio media inspectors |

## Tier 3: Ecosystem

Package management now has a local-first first slice (`jtml add` installs files/directories into `jtml_modules`, records `jtml.packages.json`, writes deterministic `jtml.lock.json` fingerprints, `jtml install` restores/verifies package state for CI, and bare imports resolve through nearest `jtml_modules`). Remote registries, semantic versions, richer object/collection type diagnostics, deeper package-aware editor intelligence, and runtime component objects remain planned after the P0/P1 language/runtime pieces stabilize.

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
  detection (first slice shipped for Friendly `use`, including imported
  components before lowering).
- Per-file scoped state, components, and stores; only `use`-declared names
  cross the boundary. First explicit-export slice shipped: Friendly `export`
  marks public top-level declarations, and named imports include only matching
  exports when a module opts into exported APIs.
- An app shape under `pages/`, `components/`, `stores/`, `assets/` consumed by
  `jtml dev app/` and `jtml build app/ --out dist`.

### Language interoperability

JTML's runtime is C++; the production target is HTML+JS. Useful interop hooks
across the stack:

- **Frontend interop**: `extern function jsCall(name, args)` for calling
  user-supplied JavaScript in the browser. First slice shipped as
  `extern action from "window.path"` browser-hosted actions, plus a stable
  runtime API so other frameworks (React, Vue, Svelte islands) can mount a JTML
  page as a custom element.
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
  `bar`, `dot`, `line`, `path`, `polyline`, `polygon`, and `group` lower to
  standard SVG shape tags.
- Higher-level SVG text/axis/legend primitives so diagrams and annotated
  visuals export without custom JavaScript.
- ✅ Accessible SVG bar charts: `chart bar data rows by label value total`.
- ✅ 3D mount contract: `scene3d "Product" scene productScene camera orbit into sceneState`
  lowers to an accessible canvas, calls `window.jtml3d.render(canvas, spec)`
  when available, draws a fallback otherwise, and publishes renderer status.
- Studio media/graphics workbench: asset previews, drag-and-drop examples,
  chart/graphic gallery, and a live media-state inspector.

Detailed plan: `docs/media-graphics-roadmap.md`.

## Implementation Order

1. ✅ Ship scoped styles, `jtml dev`, static asset builds, and AI-native helper commands so the authoring loop feels complete.
2. ✅ Harden `fetch` first slice: `method`, JSON `body`, `cache`, `credentials`, `timeout`, `retry`, `stale keep|clear`, `lazy`, `refresh actionName`, and action-scoped `invalidate fetchName` options wire production-oriented browser requests; runtime intercepts refresh actions, refreshes invalidated fetches after mutation actions dispatch, starts lazy fetches from route `load`, and avoids server round-trips where safe. `redirect "/path"` Friendly keyword added for programmatic hash navigation from action bodies.
3. ✅ Studio revamped: 10 Friendly-2.0 samples (counter, form, dashboard, fetch, POST fetch, store, effects, routes, redirect, components), categorized sidebar, full reference panel covering every language feature, generated Classic/HTML artifact inspector, updated keyword highlighting.
4. ✅ Harden routing: `link active-class` reactive active-link highlighting, `guard "/path" require var else "/fallback"` client-side route guards checked on every hash change, `activeRoute` built-in reactive variable always reflects current path.
5. ✅ Component instance markers and runtime registry: each component call emits `<div data-jtml-instance="Name_N">` with public component name, role, params, local binding map, and source line; each `make Component` also emits a hidden definition marker with params, source line, and encoded Friendly body. The browser runtime scans instances into `window.__jtml_components` / `window.jtml.getComponentInstances()` and dispatches `jtml:components-ready`. The interpreter treats component wrappers as semantic boundaries, executes their local `let`/`when` declarations inside per-instance runtime environments, resolves element bindings/events through the owning instance, exposes component-local state/actions in `/api/state` and `/api/components`, exposes original component contracts through `/api/component-definitions`, and lets tools call local actions through `/api/component-action`.
6. ✅ AI authoring contract and implementation roadmap added under `docs/`.
7. ✅ Friendly formatter path now preserves high-level Friendly constructs in CLI and Studio instead of lowering them to Classic artifacts.
8. ✅ Repairable diagnostics first slice: shared diagnostic contract with severity/code/message/line/column/hint/example, JSON output for `check`/`lint`, and Studio API diagnostic arrays on parse/lint failures.
9. ✅ `jtml fix` first slice: safe mechanical repairs for missing Friendly header, tab indentation, trailing whitespace, final newline, plus JSON diagnostics for remaining parse errors.
10. ✅ `jtml explain --json` richer summary: state, constants, derived values, actions, components, routes, fetches, stores, effects, style blocks, and diagnostics.
11. ✅ Friendly source-map diagnostics first slice: parse diagnostics produced after Friendly-to-Classic lowering are remapped back to original Friendly source lines in `jtml check --json` and `jtml lsp`.
12. Next: replace compatibility source expansion with non-expanded runtime `ComponentInstance` AST execution. The scoped environment + definition registry bridge above is the migration surface.
13. ✅ Route layout first slice: `route "/path" as Page layout AppLayout` injects route content into the layout component's `slot`, so shared chrome (nav, footer) wraps multiple routes.
14. Next: Studio as the language home — new `00-welcome` JTML-authored lesson, every lesson in Friendly syntax, Studio default tab is the home lesson.
15. ✅ JTML code modularization first slice: Friendly `use … from` is resolved before lowering so imported Friendly components, state, stores, and actions can participate in `check`, `build`, `serve --watch`, and `dev` module graphs with cycle detection.
16. ✅ Per-file export scopes first slice: Friendly `export let`, `export when`,
    `export make`, `export store`, and related top-level declarations are
    accepted; named imports from exporting modules include only matching public
    declarations while side-effect imports preserve compatibility.
17. ✅ App folder shape first slice: `jtml new app <dir>` creates `index.jtml`, `components/`, `stores/`, and `assets/`, and the generated app builds through the same zero-config module graph.
18. ✅ Framework export first slice: `jtml export <file> --target html|react|vue -o out` emits static HTML or iframe-based React/Vue wrapper components for incremental adoption.
19. ✅ Custom element export first slice: `jtml export <file> --target custom-element -o jtml-app.js` emits a standards-based `<jtml-app>` Web Component that can be embedded by plain HTML, React, Vue, Svelte, Angular, or any CMS shell.
20. ✅ Runtime HTTP contract first slice: `jtml serve`/`jtml dev` expose `/api/health`, `/api/bindings`, `/api/state`, `/api/runtime`, and richer `/api/event` responses with bindings + state; documented in `docs/runtime-http-contract.md`.
21. ✅ Browser extern action first slice: `extern notify from "host.notify"` declares a client-side action, and event calls such as `click notify("Saved")` invoke `window.host.notify("Saved")` before server dispatch.
22. ✅ C ABI first slice: release builds now produce `libjtml` plus `include/jtml/c_api.h`; hosts can render Friendly/Classic source, load a runtime context, inspect bindings/state/components JSON, dispatch DOM events, and call component-local actions without shelling out to the CLI.
23. ✅ Type-aware linter first slices: `let`/`const`/`get` annotations preserve through Classic lowering and formatter output, the linter reports primitive mismatches as `JTML_TYPE_MISMATCH`, and ordinary action/function arity mismatches as `JTML_ARITY`.
24. ✅ Editor support first slices: VS Code grammar highlights Friendly JTML 2 declarations/keywords, snippets now produce canonical app, component, fetch, route, store, effect, scoped style, input binding, and extern examples, the extension launches `jtml lsp` when available and falls back to CLI diagnostics, formatting, and safe fixes.
25. ✅ LSP intelligence: completions include same-file and imported user-defined symbols, hover works across the module graph, go-to-definition jumps through `use` / `import`, workspace symbols scan the project, references and rename share a string/comment-aware scanner, signature help understands `make`/`when`/`function` parameters, document highlights and indentation-based selection ranges are implemented, and code actions include both `source.fixAll.jtml` and targeted `JTML_EVENT_ACTION` handler creation.
26. ✅ Local package manager first slice: `jtml add <path|name> [--from path]` installs local `.jtml` files or package directories into `jtml_modules/`, records `jtml.packages.json`, writes deterministic `jtml.lock.json` package/file fingerprints, `jtml install [--json]` restores missing local packages and verifies lockfile fingerprints for CI, and the CLI/linter/LSP module graph resolves bare package specifiers through nearest `jtml_modules`.
27. ✅ Media/graphics first slice: docs/examples, `canvas`/SVG-friendly element parity, `file`/`dropzone` bindings, runtime file previews, drag/drop dispatch, reactive audio/video controllers, media client actions, SVG-first `graphic`/`bar`/`dot`/`line`/`path`/`polyline`/`polygon`/`group` aliases, accessible SVG bar charts, `scene3d` 3D mounts/state bindings, host `spec.update(...)`, and media/3D lint warnings.
28. Next: richer chart primitives, 3D renderer packages, and Studio media inspectors.
28. Next: remote registry, semantic versions, richer incremental LSP text sync, package-aware auto-imports, stores, richer object field/collection types, and runtime `ComponentInstance` objects once P0 runtime semantics are stable.
