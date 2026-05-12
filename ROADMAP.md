# JTML Roadmap

JTML should be a tiny, Friendly-first reactive HTML language for small
interactive pages, prototypes, teaching, AI-authored apps, and server-backed
UI. It should stay smaller and more auditable than React/Vue/Angular stacks,
while covering the production basics: data loading, routing, scoped styles,
components, media/graphics, tooling, Studio, and static/live deployment.

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

## Phase 5: End-User Distribution

- Installable CLI. ✅ — `cmake --install` plus CPack `.tar.gz` packaging.
- Versioned language reference. ✅ — `docs/language-reference.md` and `site/reference.html`.
- Example gallery. ✅ — `site/examples.html`.
- Minimal deployment story for static transpiled pages and live server-backed pages. ✅ — `docs/deployment.md` and `site/deploy.html`.
- Local-first package/dev workflow. ✅ — `jtml new app`, `jtml add`,
  `jtml install`, `jtml dev <file|app/>`, `jtml build <file|app/> --out dist`.

## Phase 6: Production Language And Studio

- `fetch` with loading/data/error/stale/attempts, retry, timeout, refresh,
  lazy loading, and invalidation. ✅
- Hash routing with params, wildcard fallback, layout, route load hooks,
  active links, guards, and `activeRoute`. ✅
- Scoped Friendly `style` blocks under `[data-jtml-app]`. ✅
- Runtime component-instance bridge with per-instance state/action metadata,
  `/api/components`, `/api/component-definitions`, and component action
  dispatch. ✅
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

## Modularization (done this sprint)

- **Directory layout**: `include/jtml/` for every public header, `src/` for every `.cpp`, `third_party/` for vendored single-header deps, `cli/` for the `jtml` binary split by command, `cmake/` for reusable helpers, `tests/` for the unit suite, `.github/workflows/` for CI.
- **Header naming**: dropped the `jtml_` prefix and CamelCase. `include/jtml/{ast,lexer,parser,interpreter,transpiler,formatter,linter,value,environment,array,dict,function,renderer,websocket_server,instance_id_generator}.h`.
- **Source naming**: every `.cpp` in `src/` is snake_case and mirrors its header.
- **Third-party isolation**: `jtml_third_party` INTERFACE target in CMake owns the `third_party/` + Homebrew include paths; `jtml_core` inherits them transitively.
- **CLI split**: `cli/main.cpp` does arg parsing + dispatch only; each command is its own translation unit (`cmd_basic`, `cmd_fmt_lint`, `cmd_serve`, `cmd_tutorial`). The tutorial's embedded HTML shell is isolated in `cli/tutorial_shell.cpp`.
- **Namespace (deferred)**: the CLI already lives in `namespace jtml::cli`. The core library still uses global and `JTMLInterpreter::*` names; wrapping the entire core in `namespace jtml` is a single atomic refactor planned for the next sprint (it touches every AST reference in every file).

## Current Focus

The current focus is production hardening: keep Friendly JTML as the only
default authoring dialect, keep Classic as a stable compatibility layer, make
every bundled example/tutorial/doc match the AI authoring contract, and continue
tightening component/runtime semantics until larger apps feel boringly reliable.
The next competitiveness lane is deepening media and graphics: asset input,
media state, charts, diagrams, `scene3d` host packages, and declarative visuals
that remain readable, portable, and AI-friendly.

The shareable documentation map lives in `docs/README.md`. Feature documents
should use the same status language as this roadmap: implemented, first slice,
hardened, or planned.
