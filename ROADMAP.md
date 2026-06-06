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
3. Studio content externalized from embedded C++ literals.
4. Internal module boundaries for Friendly lowering, semantic IR, runtime,
   emitters, LSP, Studio, interop, and package tooling.
5. Security, release, compatibility, deprecation, benchmark, and contribution
   policies.

Stable today means "usable with tests and compatibility expectations." First
slice means "implemented and valuable, but still being hardened."
Experimental means "available for exploration and tool contracts, but not yet
a production promise."

## Modularization (done this sprint)

- **Directory layout**: `include/jtml/` for every public header, `src/` for every `.cpp`, `third_party/` for vendored single-header deps, `cli/` for the `jtml` binary split by command, `cmake/` for reusable helpers, `tests/` for the unit suite, `.github/workflows/` for CI.
- **Header naming**: dropped the `jtml_` prefix and CamelCase. `include/jtml/{ast,lexer,parser,interpreter,transpiler,formatter,linter,value,environment,array,dict,function,renderer,websocket_server,instance_id_generator}.h`.
- **Source naming**: every `.cpp` in `src/` is snake_case and mirrors its header.
- **Third-party isolation**: `jtml_third_party` INTERFACE target in CMake owns the `third_party/` + Homebrew include paths; `jtml_core` inherits them transitively.
- **CLI split**: `cli/main.cpp` does arg parsing + dispatch only; each command is its own translation unit (`cmd_basic`, `cmd_fmt_lint`, `cmd_serve`, `cmd_tutorial`). The tutorial's embedded HTML shell is isolated in `cli/tutorial_shell.cpp`.
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
  semantic definition before dispatch.
- P2 component instances: keep the existing runtime-plan bridge stable while
  moving beyond source expansion as the semantic truth.
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

Priority order after the semantic usage cleanup is:

1. Semantic styling and UI primitives.
2. Browser-local production runtime.
3. Interop and full web-platform escape hatches.
4. Direct non-expanded component template execution where those slices expose
   weak ownership.
5. Studio/docs hardening on top of the same semantics.

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
  moved from `src/transpiler.cpp` into `src/browser_runtime_emitter.cpp` behind
  `jtml::emitBrowserRuntimeScript()`. This is the first concrete step toward
  separating HTML emission, browser runtime emission, and runtime planning.
- Client manifest emitter split. ✅ — browser-local manifest generation moved
  behind `jtml::emitClientManifestScript()` in
  `src/client_manifest_emitter.cpp`, with shared expression serialization in
  `src/expression_source.cpp`. `src/transpiler.cpp` now delegates both runtime
  script and manifest generation.
