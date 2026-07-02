# JTML Roadmap

JTL is the planned core language; JTML is its web/app dialect. Together they
should form an observable-first language family over the full web platform:
apps read like documents but behave like reactive software, while core logic
can grow toward Python-like productivity. Friendly JTML is the default web
authoring dialect, experimental `jtl 1` marks core-language experiments,
Classic remains the compatibility/lowering layer, and WebSocket/live rendering
is one backend rather than the definition of the language.

The durable architecture target is:

```text
Friendly JTML -> typed AST -> semantic IR -> observable graph -> backends
```

Backends should include static HTML, browser-local JavaScript runtime, live
server/WebSocket runtime, custom elements, and framework exports. The full
architecture thesis lives in
[`docs/architecture/observable-first-architecture-roadmap.md`](docs/architecture/observable-first-architecture-roadmap.md).
The consolidated JTML/JTL language-family design lives in
[`docs/architecture/language-family-design.md`](docs/architecture/language-family-design.md).
The near-term implementation order lives in
[`docs/roadmaps/next-priorities.md`](docs/roadmaps/next-priorities.md).

Performance positioning: live HTML/body-plan patches are useful for Studio,
dev preview, internal live apps, and server-owned UI, but they are not the
benchmark path. Framework-competitive public builds require a compiler-first
browser production target: typed AST and semantic graph to optimized body plan,
generated JS component functions, fine-grained dependency updates, keyed DOM
diffing, small runtime helpers, CSP-safe static JS assets, and a clear dev/prod
runtime split. Runtime `new Function`/eval-style compilation is only an
explicit development bridge, not the production security model.

## Phase 1: Try It In Five Minutes

- One-command demo: `jtml demo --port 8000` ✅
- Starter generator: `jtml new my_page.jtml` ✅
- Syntax checker: `jtml check my_page.jtml` ✅
- Example browser: `jtml examples` ✅
- **Interactive tutorial: `jtml tutorial --port 8000`** — split-view IDE with lesson prose, editable code, live preview, and hot-swap Run. Lessons live under `tutorial/<NN-slug>/`. ✅
- Small, polished examples that use Friendly `jtml 2`, `let`, `get`, `when`,
  `page`, event sugar, routes, fetches, stores, modules, and components. ✅
- Classic `element` / `@tag` syntax remains supported as a compatibility and
  compiler-artifact layer. ✅

## Phase 2: Reliable Reactivity

- Make Friendly `let`, `const`, `get`, assignment, arrays, dictionaries,
  store fields, object properties, and effects/subscriptions consistent. ✅
- Keep DOM bindings stable between transpiler and interpreter. ✅
- Prevent duplicate subscriber callbacks. ✅
- Keep derived values and frontend updates predictable. ✅

## Phase 3: Pleasant Frontend Bridge

- Event attributes should work with zero user JavaScript. ✅
- Friendly event sugar (`click`, `input`, `change`, `submit`, `hover`,
  `scroll`, etc.) lowers to Classic event bindings and passes useful
  arguments. ✅
- Generated HTML should include a small runtime with clear connection status and readable errors. ✅ — `data-jtml-status`, `data-jtml-message`, console reporting, WebSocket, and HTTP fallback.
- Static fallback rendering for initial values ✅ — transpiled pages now apply `window.__jtml_bindings` on DOMContentLoaded, so initial render is correct even if the WebSocket is unreachable. Events transparently fall back to `POST /api/event`.
- Keep expanding HTML parity: boolean attributes, hyphenated attributes, SVG-friendly names, void element ergonomics, and custom elements. ✅

## Phase 4: Helpful Tooling

- Clear lexer/parser/runtime errors with file, line, column, and suggestions. ✅ for lexer/parser command output; runtime errors are surfaced to the generated page status and console.
- A Friendly-preserving formatter: `jtml fmt <file> [-w]` ✅ — keeps high-level
  `jtml 2` constructs such as `fetch`, `route`, `store`, `effect`, `style`,
  and `make`, while still formatting Classic compatibility files.
- A linter for common mistakes: `jtml lint <file>` ✅ — undefined variables, unreachable code, missing subscribe/unsubscribe targets; non-zero exit on errors.
- A test command that runs example smoke tests. ✅ — `jtml test` parses, lints, and transpiles every bundled example.
- Watch mode for rebuild/reload: `jtml serve <file> --watch` ✅ — polls the source, re-transpiles on change, hot-swaps interpreter state, broadcasts `{"type":"reload"}` to every connected browser.
- Version introspection: `jtml --version` ✅
- Language catalog introspection: `jtml keywords [--json]` ✅

## Phase 5: End-User Distribution

- Installable CLI. ✅ — `cmake --install` plus CPack `.tar.gz` packaging.
- Versioned language reference. ✅ — `docs/reference/language-reference.md` and `site/reference.html`.
- Canonical mini-reference. ✅ — `jtml keywords` / `jtml keywords --json`
  expose the Friendly JTML 2 keyword catalog; README, reference docs, Studio,
  VS Code grammar, and LSP completions are guarded against drift.
- Example gallery. ✅ — `site/examples.html`.
- Minimal deployment story for static transpiled pages and live server-backed pages. ✅ — `docs/tooling/deployment.md` and `site/deploy.html`.
- Local-first package/dev workflow. ✅ — `jtml new app`, `jtml add`,
  `jtml install`, `jtml dev <file|app/>`, `jtml build <file|app/> --out dist`.

## Phase 6: Production Language And Studio

- `fetch` with loading/data/error/stale/attempts/hasData plus browser-local
  metadata (`status`, `ok`, `url`, `method`, `updatedAt`), retry, timeout,
  refresh, lazy loading, and invalidation. ✅
- Hash routing with params, wildcard fallback, layout, route load hooks,
  active links, guards, and `activeRoute`. ✅
- Scoped Friendly `style` blocks under `[data-jtml-app]`. ✅
- Runtime component-instance bridge with per-instance state/action metadata,
  semantic runtime plans, `/api/components`, `/api/component-definitions`, and
  component action dispatch. ✅
- Studio as the language home: examples, tutorials, docs, compiler artifacts,
  diagnostics, linter, fixer, formatter, export, local version snapshots,
  autosaved drafts, and command palette. ✅
- Media and graphics competitiveness track: standards media aliases,
  file/dropzone bindings, reactive audio/video controllers, SVG-first
  graphics, accessible bar charts, raw SVG/canvas/custom-element fallback,
  `extern` bridges, and first-slice `scene3d` renderer mounts. ✅ First slice
  shipped; Studio media inspectors and richer renderer packages remain planned.
- Remaining enterprise-grade architecture step: replace source-expanded
  components with direct non-expanded `ComponentInstance` execution while
  keeping the existing metadata/runtime contract stable.

## Enterprise Readiness Position

JTML is enterprise-relevant, but it is not enterprise-ready yet. The conceptual
architecture is now credible: Friendly JTML, typed AST, semantic IR,
observable graph, runtime plans, and multiple backend targets. The
implementation is still a hybrid transition: compatibility expansion, embedded
Studio shell content, monolithic source files, first-slice browser runtime
parity, and incomplete governance remain active risks.

`jtml doctor --json` is the machine-readable readiness surface. It reports:

- local toolkit checks;
- required verification gates;
- stable, first-slice, and experimental feature tiers;
- current enterprise readiness (`enterpriseReady: false`);
- next architecture targets.

Enterprise readiness requires the next platform step:

```text
functional monolith -> modular platform with stable internal contracts
```

Near-term platform hardening targets:

1. Direct non-expanded `ComponentInstance` template execution.
2. Browser-local runtime parity with the live runtime.
3. Contract-first JTL API design for governed backend operations, kept planned
   until runtime/module semantics are safer.
4. Studio content externalized from embedded C++ literals.
5. Internal module boundaries for Friendly lowering, semantic IR, runtime,
   emitters, LSP, Studio, interop, and package tooling.
6. Security, release, compatibility, deprecation, benchmark, and contribution
   policies.

Stable today means "usable with tests and compatibility expectations." First
slice means "implemented and valuable, but still being hardened."
Experimental means "available for exploration and tool contracts, but not yet
a production promise."

## Modularization And Platform Boundaries

Modularization is now incremental rather than a one-time folder shuffle. The
destination folders exist for syntax, semantic, runtime, emit, tooling,
interop, CLI command/service/studio ownership, and test fixtures. Files move
only when the target boundary owns a stable contract and regression coverage.

- **Directory layout**: `include/jtml/` for every public header, `src/` for every `.cpp`, `third_party/` for vendored single-header deps, `cli/` for the `jtml` binary split by command, `cmake/` for reusable helpers, `tests/` for the unit suite, `.github/workflows/` for CI.
- **Header naming**: dropped the `jtml_` prefix and CamelCase. `include/jtml/{ast,lexer,parser,interpreter,transpiler,formatter,linter,value,environment,array,dict,function,renderer,websocket_server,instance_id_generator}.h`.
- **Source naming**: every `.cpp` in `src/` is snake_case and mirrors its header.
- **Third-party isolation**: `jtml_third_party` INTERFACE target in CMake owns the `third_party/` + Homebrew include paths; `jtml_core` inherits them transitively.
- **CLI split**: `cli/main.cpp` does arg parsing + dispatch only; each command is its own translation unit (`cmd_basic`, `cmd_fmt_lint`, `cmd_serve`, `cmd_tutorial`). The tutorial's embedded HTML shell is isolated in `cli/tutorial_shell.cpp`.
- **Runtime ownership slice**: the static browser update-plan asset emitter now
  lives under `include/jtml/runtime/` and `src/jtml/runtime/`, while the old
  top-level include remains as a compatibility shim. CMake groups core sources
  by syntax, semantic, runtime, and tooling ownership so future movement is
  explicit and reviewable.
- **Namespace (deferred)**: the CLI already lives in `namespace jtml::cli`. The core library still uses global and `JTMLInterpreter::*` names; wrapping the entire core in `namespace jtml` is a single atomic refactor planned for the next sprint (it touches every AST reference in every file).

## Current Focus

The current focus is the semantic-core transition:

- P0 semantic correctness: typed AST tightening, explicit attribute
  classification, literal/reactive separation, source spans, and source-first
  diagnostics. ✅ First slice: `JtmlAttributeKind` now classifies
  literal/boolean/reactive/event/special/passthrough attributes before
  transpiler/interpreter runtime binding.
- P1 observable graph: make `jtml explain` consume a real semantic model rather
  than ad-hoc parser guesses. ✅ First slice: state/constants/derived/actions,
  fetches, routes, effects, stores, components, imports, attribute kinds, and
  dependency edges are now emitted from `jtml::analyzeSemanticProgram`.
  ✅ Cleanup slice: observable lint/explain warnings now come from
  `analyzeSemanticUsage` instead of CLI source-token scans. ✅ Manifest slice:
  route and fetch browser-local manifests are now emitted from
  `semantic.routeRecords` / `semantic.fetchRecords`, with DOM markers retained
  as compatibility fallbacks. ✅ Component graph slice: semantic explain now
  exposes structured `componentDefinitions` and `componentInstances`, and the
  browser component registry consumes those records before DOM marker fallback.
  ✅ Live-backend alignment slice: the interpreter now registers runtime
  component definitions/instances from the same semantic records first, using
  lowered DOM marker scans only as a compatibility fallback. ✅ Runtime-plan
  slice: component definitions now carry local state/actions/derived/effects,
  event bindings, slot/body shape, and a `runtimePlan`; component instances
  expose their owning runtime environment and validate actions against that
  semantic definition before dispatch. ✅ Typed body-plan slice: runtime
  component body nodes now expose child indices, assignment operators, and
  expression payloads through the shared RuntimePlan JSON and live interpreter
  component-definition state.
- P1 module graph hardening: ✅ Relative imports now resolve from the directory
  of each importing file across nested Friendly graphs, and the shared loader is
  exercised by `check`, `build --target browser`, `explain --json`, and source
  file collection. Missing import diagnostics now include the specifier,
  importer, and attempted resolved path. ✅ Named imports now require exported
  declarations, missing exports report available public names, duplicate
  exports are diagnostics, and private declarations stay private under named
  imports. ✅ Re-export barrels such as `export use Card from "./card.jtml"`
  forward public declarations through the same module loader. ✅ Repo cleanup:
  CLI module graph ownership moved out of `cli/util.cpp` into
  `cli/module_loader.*`, and local package install/lockfile ownership moved out
  of `cli/cmd_basic.cpp` into `cli/package_manager.*`. ✅ First semantic
  ownership slice: `jtml explain --json` now reports structured
  `semantic.importRecords` plus `semantic.moduleFiles` sourced from the shared
  loader. ✅ Project contract slice: unresolved import edges use an explicit
  invalid module id, attempted `resolvedPath` is retained, project graph JSON is
  core-owned, and each semantic module now exposes public `exportRecords`.
  ✅ Per-file semantic ownership slice: `SemanticProject` modules now retain a
  compact `SemanticProgram` summary for their own file, exported declarations
  contribute to the same module-level semantic buckets, and `explain --json`
  exposes those per-module summaries. ✅ Project diagnostics slice:
  `analyzeSemanticProject` now reports unresolved imports and missing named
  exports from the core module graph, and `explain --json` exposes those
  project issues. ✅ Human explain slice: default `jtml explain` now reports
  project modules, importer-owned imports, exported declarations, per-module
  semantic summaries, and semantic project issues so module debugging is not
  JSON-only. ✅ Source span slice: semantic import records now retain
  `sourceLine` / `sourceColumn`, project imports copy those coordinates into
  their span, and JSON project graphs expose the importer location for future
  source-first diagnostics. ✅ Import issue source diagnostics slice:
  unresolved imports, missing named exports, and unresolved/cyclic re-export
  issues now carry the importing module path plus authored import line/column.
  ✅ Per-file IR slice: `SemanticModuleSource` and
  `SemanticProject` now retain a typed structural IR summary for each module
  when it can be parsed in isolation, and project JSON/default explain expose
  top-level AST node order, typed node counts, syntax mode, and parse errors.
  ✅ Import-aware isolated parse slice: explain-time per-file IR now retries
  Friendly modules with explicit imported-component stubs and marks the syntax
  as `friendly+import-stubs`, so route/component entry files can expose typed
  structure without falling back to whole-graph compatibility expansion.
  ✅ Per-file AST ownership slice: `SemanticModuleSource` and `SemanticProject`
  now retain copyable cloned typed AST records beside their compact IR summaries,
  preserving the `friendly+import-stubs` marker where needed for honest tooling.
  ✅ Imported symbol identity slice: project imports now expose resolved imported
  symbols with their export kinds, so explain/JSON can say `appState` resolves
  to a `store` and `Dashboard` resolves to a `make` component. ✅ Re-export
  identity slice: barrel modules such as `export use Card from "./card.jtml"`
  now resolve imported symbol identity to the ultimate exported module/kind
  instead of stopping at `re-export`. ✅ Re-export diagnostics slice:
  `analyzeSemanticProject` now distinguishes unresolved re-export targets and
  re-export cycles from plain missing imports, so semantic tooling can report
  bad barrel chains without inventing source scans. Remaining module work: use
  retained per-file ASTs in linker/runtime passes, and add recoverable project
  loading for editor/explain flows that want JSON even when the filesystem
  loader rejects an import cycle early. ✅ First retained-AST runtime slice:
  `RuntimeProjectPlan` now builds module-scoped runtime plans from
  `SemanticProject` retained per-file ASTs, and `explain --json` exposes
  `runtimeProjectPlan` beside the legacy linked `runtimePlan`. ✅ Browser
  manifest project-plan slice: `jtml transpile/build --target browser` now
  attaches the same `RuntimeProjectPlan` to the client manifest under a
  backward-compatible `project` key, giving browser-local backends a
  module-owned runtime surface while preserving current linked manifest fields.
  ✅ Browser runtime project-consumption slice: browser-local startup now
  merges the embedded semantic project plan into its active manifest, so
  module-owned component definitions, fetches, routes, actions, and state are
  visible to production execution instead of being explain-only metadata.
  ✅ Manifest hardening slice: runtime planning now exposes separate explain
  and client serializers. Explain JSON stays source-rich for tooling; browser
  manifests are compact execution data, omit source paths/body payloads, escape
  script-breaking text, mark `friendly+import-stubs` modules as tooling-only,
  and carry module-aware component definition identities.
- P1 repo modularization skeleton: ✅ Production-grade destination folders now
  exist for `syntax`, `semantic`, `runtime`, `emit`, `tooling`, `interop`, CLI
  command/service/studio ownership, and test fixtures. Current flat files move
  into those buckets incrementally after tests cover each boundary.
- P2 component instances: keep the existing runtime-plan bridge stable while
  moving beyond source expansion as the semantic truth. ✅ First browser-local
  direct body-plan execution slice: component instances can now render common
  templates from `RuntimePlan.componentDefinitions[].bodyPlan`, initialize
  per-instance state from planned `let` nodes, recompute planned `get` nodes,
  render simple `if` / `else` / `for` body-plan nodes, run simple local
  assignment actions plus action-local `let` / `get` declaration nodes, carry
  authored slot plans on component instances, render
  slots and nested component calls, preserve common static/literal attributes,
  pass simple action arguments, and re-render the owning instance without
  depending on the expanded compatibility DOM as the rendered surface. Nested
  browser-local component calls now register addressable runtime identities, so
  nested local actions can find and rerender their owning component. ✅
  Live-interpreter body-plan action slice: the live runtime can execute simple
  component assignment actions plus action-local `let` / `get` declarations,
  guarded `if`/`else`, array `for`, and guarded `while` action bodies with
  arguments over the same body-plan contract, preserves numeric/string
  compound-assignment semantics for `+=`, `-=`, `*=`, `/=`, and `%=`, fails closed to the compatibility function for
  unsupported bodies, and has browser/live metadata parity coverage. ✅ Live
  template-rendering surface: `/api/state` now exposes
  first-slice `renderedHtml` generated from the same body plan for common
  templates, slots, and nested component calls. ✅ Direct semantic UI rendering:
  browser-local and live body-plan renderers now preserve UI primitives and
  modifiers as `jtml-*` classes plus `data-jtml-ui*` attributes instead of
  leaking modifier words into text. ✅ Live body-plan transport patch: `jtml
  serve` exposes `/api/rendered-components`, event responses carry
  `renderedComponents`, and the browser runtime patches supported component
  wrappers from body-plan HTML while failing closed to compatibility DOM. ✅
  Initial live body-plan transport ownership: supported wrappers are injected
  from body-plan HTML into the initially served document and marked with
  `data-jtml-live-body-plan-transport="body-plan"` plus a stable rendered
  hash, so the browser can verify current markup without a startup DOM patch
  and uses `/api/rendered-components` for refresh/action updates. ✅ Live nested
  body-plan instances: supported nested live components
  now retain dynamic runtime identity, nested local actions dispatch through
  `/api/component-action`, nested params/state initialize as runtime values, and
  repeated nested children inside `for` loops keep separate local state.
  Stale nested descendants are pruned after supported live and browser-local
  body-plan renders, so removed loop/branch children cannot keep accepting
  component actions.
  ✅ First keyed-list lifecycle slice: Friendly component loops may now use
  `for item in items key item.id`; the compatibility parser still receives a
  plain `for`, while the semantic body plan carries the key expression and
  browser/live nested component IDs prefer the key over the item index, so
  local state follows reordered items and removed keyed descendants are pruned.
  ✅ First named-slot slice: component definitions can use `slot header`,
  default `slot`, and `slot footer`; call sites provide matching `slot name`
  blocks; compatibility expansion injects only the selected group; and
  browser/live body-plan renderers select named/default slot content from the
  same `slotPlan`.
  ✅ First emitted component-event bridge and explicit `emits` contract:
  component definitions can declare `make Child emits picked`, named payloads
  with `make Child emits picked(item)`, or first-slice typed payload metadata
  with `make Child emits picked(item: string)`; semantic analysis, runtime plans,
  browser manifests, explain JSON, and live component definition JSON expose
  emitted events plus `emitArity`, `emitPayloads`, and `emitPayloadTypes`.
  Nested component calls can use
  `Child on picked choose` or `Child on picked choose("preset")`; child direct
  actions such as `picked("Ada")` route to the mapped parent body-plan action
  in browser-local and live runtimes, forwarding preset handler args before
  emitted args. Declared emits validate `on event handler` names, and declared
  payload arity is checked against known parent action signatures.
  ✅ First body-plan action-call node: action body lines such as `incBy(2)` and
  `picked("Ada Lovelace")` are represented as `kind: "call"` instead of
  template fallback. Browser-local and live direct execution evaluate call
  arguments, dispatch to local component actions when present, or emit declared
  component events when no local action exists.
  Browser-local direct action arguments now use top-level parsing for quoted
  strings with spaces, and browser-local/live action parameters are restored
  after direct execution so temporary call-frame values do not leak into
  component state. Unsupported live body-plan action fallback reasons now keep
  component/action context, component definition line, and nearby authored node
  text when compatibility cannot take over; browser-local fallback telemetry now
  records the same authored action/source context on
  `window.jtml.directComponentFallbacks`. When compatibility fallback also
  fails, the source-first body-plan context is preserved beside the fallback
  error. Body-plan nodes now expose authored body source lines plus first
  read/write dependency metadata for future fine-grained browser update code.
  Browser-local direct component actions now use that metadata for a first
  rerender gate: writes that do not affect rendered reads can skip a full
  component rerender, while derived read dependencies are followed
  conservatively and action telemetry is exposed for tooling. Simple affected
  leaf nodes now carry direct body-plan DOM markers and managed-attribute
  metadata. Body-plan read dependencies now come from the parsed expression AST
  for real expressions, so object/member/subscript expressions track precise
  member/subscript read paths plus their owning state roots without string-token
  dependency guesses. Member/subscript writes now also record their owning
  observable root for update invalidation, and the
  browser/live direct action paths can mutate existing dict/object properties,
  create missing dict/object and non-negative array path containers, and update
  array/dict subscript targets without compatibility dispatch. Richer
  assignment targets remain planned. Live body-plan `for` loops now share the
  browser value model for arrays, strings, dict/object values, and scalar
  singletons in both rendering and action execution. The browser runtime
  tries in-place text/attribute patches first, then leaf replacement from the
  body plan, then full body-plan component rerender before any compatibility
  path is considered. Those simple updates are now compiled into a cached
  per-component update plan keyed by
  module/name/body shape, and each patchable node becomes an explicit
  text/button/element patch operation with precompiled element-part and
  click-invocation shape. When an operation cannot safely patch, the runtime
  records fallback telemetry with the affected body-plan node and source
  line/column.
  Cached plans now own generated browser update-function source plus an indexed
  executable update function keyed by rendered reads, giving the runtime the
  same dependency-routed call boundary that the production compiler can later
  emit directly as static JS. Dynamic execution of generated source is disabled
  by default for CSP-safe browser builds and must be explicitly opted into as a
  development bridge; the conservative interpreted updater is the default
  runtime fallback. Browser production builds now emit first-slice split
  assets: `jtml-runtime.js`, `components/index.js`, `app.js`, and the legacy
  `jtml-update-plans.js` alias, with `jtml-runtime.js` now primary for browser
  builds instead of duplicating the runtime inline. The component module now
  owns a production `window.__jtml_static_component_plan_index`, while the
  legacy `jtml-update-plans.js` asset keeps `window.__jtml_static_update_plans`
  plus source-rich `bodyPlan` metadata for compatibility/debug tooling. The
  component module no longer publishes the legacy update-plan global or carries
  source-rich body-plan payloads. The component module carries precomputed
  component read indexes, unsafe-entry lists, root create operations,
  precompiled element `partsPlan` records, first-slice text/region/nested/element
  patch operations, and first-slice static component create/update modules.
  Supported root text, button, leaf element, direct-safe container nodes,
  safe `if`/keyed-`for` control-flow regions, and first-slice slot/nested
  component anchors now emit escaped HTML construction in `components/index.js`;
  unsupported shapes still use helper/fallback create paths. Static update
  functions try direct per-node patch cases for text, button, element,
  container-attribute, safe control-flow region, slot, and nested-component
  nodes before falling back to operation records.
  The runtime-plan layer now owns the canonical expression-plan producer and
  prefers parsed JTML expression AST nodes before falling back to the legacy
  string planner. Literal, boolean, number, null, dot-path, unary `!`,
  binary/comparison/logical, ternary, composite string, array/object literal,
  member/subscript, and call-shaped plans carry `producer: "ast"` when the parser owns them; static
  component emission consumes that same API instead of maintaining a private
  expression parser. Those plans flow through text/element patch operations,
  element content, attributes, modifiers, component action arguments,
  body-plan conditions, `for` collection/key expressions, nested component
  parameter evaluation, and browser-local body-plan action assignments/local
  declarations before generic browser expression evaluation is used as
  fallback.
  Static component modules now emit direct JS expression functions for the
  simple and first-slice composite cases, generated
  text/button/leaf-element/container/control-flow/slot/nested create functions
  that do not require generic runtime region helpers unless they hit a fallback
  shape, and generated text, element attribute/content, button
  label/action-argument, safe region, slot, and nested patch cases that update
  DOM directly before falling back to runtime patch helpers. Slot create
  functions now have a raw default/named slot HTML fast path, slot patch
  functions perform their own marked-region DOM replacement, and nested
  component patches first try true parameter/body updates inside the retained
  nested wrapper before wrapper replacement fallback.
  Runtime-plan read analysis now recognizes
  value-taking attributes such as `title selected` as reads of `selected`, even
  when the value token is also a valid boolean attribute name.
  Body-plan nodes now carry `sourceLine` and `sourceColumn` through the
  semantic runtime plan, live `/api/state`, static component/update-plan
  metadata, browser fallback telemetry, and HTTP runtime diagnostic context.
  `rootCreateOperations` and per-entry operation payloads remain as
  fallback/debug metadata rather than the primary path. The runtime prefers
  those static modules/plans before
  compiling equivalent plan indexes in the browser.
  `scripts/benchmark_runtime.sh` now provides first-slice browser asset budgets,
  production/legacy asset-shape checks, real headless-browser click benchmarks
  through `scripts/benchmark_browser_runtime.sh` when Chrome/Chromium is
  available, and a `control_flow` fixture that
  asserts safe `if`/keyed-`for` regions, keyed list markers, and direct
  safe-region patching are generated in `components/index.js`. The `composition`
  fixture also asserts in-place nested parameter/body patch telemetry in the
  browser runtime benchmark. This path has an executable guardrail: 50 KB
  for `index.html`, 260 KB for `jtml-runtime.js`, 180 KB for
  `components/index.js`, 20 KB for `app.js`, and 180 KB for the legacy
  `jtml-update-plans.js`, to tighten as direct generated update modules keep
  replacing operation-record patch helpers.
  Structured container elements
  with children can now patch their own compiled attributes in place without
  disturbing child DOM. `if` and `for` nodes now render stable body-plan region
  anchors and can be replaced directly from compiled patch operations when
  their condition or collection changes. The older recursive in-place patch
  heuristic has been removed from this path: unsupported updates now go from
  compiled operation to body-plan node replacement to full direct component
  rerender. Direct `for` regions now emit per-item key/index markers, giving
  the next keyed DOM diff slice stable item identity, and the runtime records
  first-slice list lifecycle telemetry for inserted, removed, and moved keys.
  Compiled `for` region patch operations now try a conservative keyed patch
  that reuses and reorders existing list item wrappers by key, updates item
  contents with below-wrapper element/text reconciliation where the body-plan
  shape matches, reconciles child elements by stable body-node markers below
  retained wrappers, reports retained/inserted/removed/moved key sets, prunes
  removed nested dynamic children for keyed list items, and fails closed to
  whole-region replacement when keys are unsafe.
  Static create fallback paths now record source-first component/plan context in
  `window.jtml.directComponentCreateFallback`, so unsupported shapes are
  debuggable instead of disappearing into generic runtime helpers. Nested
  component call wrappers now carry stable body-node anchors and compile
  to explicit nested-component patch operations, so parent state changes that
  affect a nested call can replace that nested body-plan node without rerendering
  the entire parent component. Slot insertion sites now carry stable
  `data-jtml-direct-region="slot"` anchors and compile as slot-specific patch
  operations. Action-body `while` remains supported for explicit state-changing
  control flow, while render/template `while` is now linted as
  `JTML_TEMPLATE_WHILE`; authors should use `for` for visible repetition.
  Fully optimized keyed list DOM diffing still fails closed to a full body-plan
  rerender.
  Live body-plan rendering now publishes patch telemetry for patched/current,
  unsupported, and missing component records, giving Studio and parity tests a
  stable comparison surface while live compatibility DOM is still shrinking.
  Remaining parity work: broadening the supported
  body-plan subset until compatibility DOM is only an explicit fallback path or
  absent from production browser builds,
  improving source spans beyond typed emitted-event diagnostics, stronger
  keyed list lifecycle, richer path-assignment semantics, and broader
  behavior parity checks.
- P3 semantic styling: ✅ First slice. `theme`, UI primitives, utility
  modifiers, generated stylesheet, semantic primitive/theme counts, the
  structured `jtml explain --json` `semantic.ui` contract, and raw CSS escape
  hatch are now available. Next: richer primitives, Studio sample migration,
  and a documented component kit.
- P4 browser-local production runtime: `jtml build --target browser --out dist`
  should run local reactivity without WebSockets.
- P5 live backend on the same graph: `jtml serve` remains valuable for Studio,
  tutorials, internal tools, and server-owned state, but shares semantics with
  browser-local build.
- P6 interop and escape hatches: formalize `extern`, raw HTML/CSS, canvas/SVG,
  custom elements, WebGL/Three.js, and framework/package boundaries after the
  browser-local runtime has a stable backend contract.
- P7 contract-first governed operations: keep JTML focused on web/app UI while
  planned JTL API modules define backend operation contracts for internal
  tools such as approval consoles, incident consoles, AI governance, cost
  controls, and deployment controls. Intended phases: `type`/`error`/API
  operation signatures, OpenAPI generation, policy/validation hooks, runtime
  adapters, typed JTML `fetch`/future `call` integration, and enterprise
  hardening. The `api`, `operation`, `policy`, and `call` syntax is not
  implemented yet.

Priority order after the semantic usage cleanup and import-resolution hardening is:

1. Finish module/export semantic boundaries on top of the now-correct graph.
2. Browser-local production runtime parity.
3. Direct non-expanded component template execution where those slices expose
   weak ownership.
4. Interop and full web-platform escape hatches.
5. Studio/docs hardening on top of the same semantics and module graph.

In parallel, keep Friendly JTML as the only default authoring dialect, keep the
Classic-compatible backend stable for migration/embedding/artifacts, make every bundled
example/tutorial/doc match the AI authoring contract, and continue tightening
component/runtime semantics until larger apps feel boringly reliable.

The shareable documentation map lives in `docs/README.md`. Feature documents
should use the same status language as this roadmap: implemented, first slice,
hardened, or planned.

## Latest Platform Slice

- Studio sample externalization first slice. ✅ — playground examples now live
  under `studio/samples/manifest.json` plus `.jtml` files. `jtml studio`
  serves them via `/api/studio/samples`; the embedded shell list remains as a
  fallback only.
- Studio reference externalization first slice. ✅ — the mini-reference now
  lives in `studio/reference/catalog.json`, is served via
  `/api/studio/reference`, and renders into the Reference panel while the
  embedded markup remains as a no-network fallback. Sidebar content and larger
  Studio prose are the next shell-content extraction targets.
- Studio sidebar catalog externalization first slice. ✅ — sample category
  labels and pinned templates now live in `studio/sidebar/catalog.json` and are
  served via `/api/studio/sidebar`. Larger Studio prose blocks remain the next
  content extraction target.
- Browser runtime emitter split. ✅ — the generated browser/live runtime script
  moved out of the transpiler and then out of the emitter body itself:
  `src/browser_runtime_emitter.cpp` is now a small parameterizing wrapper around
  owned runtime asset chunks in `src/browser_runtime_assets.cpp`. This is the
  first concrete step toward separating HTML emission, browser runtime assembly,
  runtime asset ownership, and runtime planning.
- Client manifest emitter split. ✅ — browser-local manifest generation moved
  behind `jtml::emitClientManifestScript()` in
  `src/client_manifest_emitter.cpp`, with shared expression serialization in
  `src/expression_source.cpp`. `src/transpiler.cpp` now delegates both runtime
  script and manifest generation.
- Runtime plan extraction. ✅ — browser-local runtime data now has a typed
  `jtml::RuntimePlan` built from AST + semantic IR before JSON manifest
  serialization. `emitClientManifestScript(program)` remains compatible, while
  `emitClientManifestScript(plan)` gives future browser/live/component
  backends the same runtime contract without re-scanning source or rebuilding
  ad-hoc manifest state.
- Runtime plan adoption slice. ✅ — `jtml explain --json` now exposes the
  runtime plan directly, component definitions carry decoded body source in the
  plan, and the live interpreter registers component definitions/instances from
  `RuntimePlan` instead of independently rebuilding semantic component records.
  Next: execute component templates directly from the plan, with source
  expansion retained only as a compatibility backend.
- Component body plan slice. ✅ — component definitions in `RuntimePlan` now
  carry a structured body plan derived from decoded component source
  (`state`, `derived`, `action`, `assignment`, `effect`, `slot`, and
  `template` nodes with indent, parent-index, and render-root metadata).
  `jtml explain --json`, browser-local manifests, and live
  `/api/component-definitions` expose this as
  `runtimePlan.componentDefinitions[].bodyPlan`, giving direct component
  execution, Studio, LSP, and parity tooling a semantic component-body surface
  instead of requiring each backend to parse encoded source strings.
- SemanticProject contract cleanup. ✅ — module graph records now use an
  explicit `InvalidSemanticModuleId` sentinel instead of letting unresolved
  imports default to entry module `0`, expose the attempted `resolvedPath`, and
  reuse the same `resolveJtmlModulePath()` resolver as the loader for relative
  and bare-package paths. The graph is still a linked-program provenance view;
  per-file AST ownership remains the next project-graph milestone.
- RuntimePlan JSON serializer split. ✅ — the `RuntimePlan` JSON contract moved
  into `src/runtime_plan_json.cpp`; `jtml explain --json` and browser manifest
  body-plan emission now consume the shared core serializer instead of owning
  duplicate command/backend serializers.
- AST-owned expression planning slice. ✅ — runtime expression planning now
  accepts expression AST nodes directly, and the string entry point parses JTML
  expression syntax into AST before using the older source-string fallback. The
  same plan API is therefore available to semantic/runtime JSON, static
  component emission, browser-local body-plan execution, and live body-plan
  parity work. Array/object literals, member access, subscripts, and call-shaped
  expressions now preserve structured AST-owned plan metadata instead of
  collapsing to opaque source strings. The next milestone is a dedicated typed
  expression IR that can replace the remaining fallback parser helpers entirely.
- Per-file SemanticProject ownership slice. ✅ — `SemanticProject` now has a
  `SemanticModuleSource` API so module imports can be attached to the file that
  authored them instead of always being attributed to the entrypoint.
  `jtml explain --json` now builds its project graph from collected source
  files and reports nested importer/resolved edges for real multi-file apps.
  This is still not a full linker with retained per-file ASTs, but it closes
  the misleading "all imports belong to entry" gap.
