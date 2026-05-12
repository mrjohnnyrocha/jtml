# JTML AI-Native Implementation Roadmap

This is the execution plan for making JTML widely usable and especially strong
as an AI generation and editing target.

## North Star

JTML should be the language where an AI can generate a real web app, a human can
read it, the formatter can canonicalize it, the linter can repair common errors,
and Studio can preview and explain the compiler artifacts.

## Phase 1: Canonical Authoring Loop

Status: shipped first production loop; keep tightening with every feature.

1. Source-preserving Friendly formatter.
   - CLI `jtml fmt` detects Friendly input and preserves `jtml 2` constructs.
   - Studio Format uses the same Friendly-preserving path.
   - Tests protect `fetch`, `store`, `route`, `style`, `effect`, and component
     constructs from being lowered during formatting.

2. AI authoring contract.
   - Keep `docs/ai-authoring-contract.md` short enough for prompt context.
   - Every AI-facing command should follow it.
   - Bundled examples and tutorial lessons use the same idioms, with
     `examples/keywords.jtml` kept as an intentional Classic compatibility
     fixture for syntax that Friendly does not yet own directly.

3. Studio compiler transparency.
   - Show source, lowered Classic JTML, generated HTML, diagnostics, runtime
     preview, local snapshots/history, autosaved drafts, and command palette.
   - Keep examples aligned with currently supported language features.

## Phase 2: Repairable Diagnostics

Status: first slice implemented.

1. Add structured diagnostic fields:
   - `line`
   - `column`
   - `code`
   - `message`
   - `hint`
   - `example`
   - Shipped in the shared `jtml::Diagnostic` contract and mirrored through
     linter diagnostics.

2. Improve parser/friendly errors:
   - Missing action after `click`.
   - Invalid `fetch` option.
   - Bad route declaration.
   - Route parameter/component parameter mismatch.
   - Undefined uppercase component.
   - Bad indentation.
   - First slice classifies existing error text into repair categories.
   - Shipped Friendly source-map slice: Friendly parse diagnostics from the
     lowered Classic parser are remapped to the original Friendly source line
     in `jtml check --json` and `jtml lsp`.

3. Surface diagnostics everywhere:
   - `jtml check`
   - `jtml lint`
   - Studio diagnostics list
   - LSP diagnostics and code actions
   - `jtml check --json`, `jtml lint --json`, and Studio API error responses
     now include structured diagnostic arrays.

## Phase 3: AI Commands That Use The Contract

Status: first slices exist (`generate`, `explain`, `suggest`, `fix`).

1. Make `jtml generate` emit contract-compliant Friendly JTML.
2. Make `jtml explain` describe:
   - state
   - derived values
   - actions
   - components
   - routes
   - fetches
   - stores
   - Shipped richer text and JSON output with these categories plus style
     blocks and linter diagnostics.
3. Add `jtml fix`:
   - Runs check/lint.
   - Applies safe mechanical repairs.
   - Leaves risky changes as suggestions.
   - Shipped first slice: inserts missing Friendly `jtml 2` headers, replaces
     tabs with spaces, trims trailing whitespace, ensures final newline, and
     reports any remaining parse diagnostics in the shared JSON shape.
4. Add `jtml refactor`:
   - Shipped first slice: `jtml refactor rename <file|dir> --from <name>
     --to <newname> [-w] [--json]`. Backed by `jtml::renameSymbolInSource`
     in `include/jtml/refactor.h`, the canonical string- and comment-aware
     scanner. Same semantics as `textDocument/rename`: word-boundary only,
     never edits inside `"..."` / `'...'` literals or `//` comments.
     Default writes the rewrite to stdout (pipeable preview); `-w` writes
     in place; `--json` emits machine-readable edit positions in
     0-based original-source line/column ranges.
   - Shipped workspace mode: when the positional argument is a directory,
     the rename walks every `.jtml` file recursively, skipping the same
     directories the LSP workspace scan does (`build/`, `dist/`,
     `node_modules/`, `out/`, `target/`, `.git/`, dotfiles). Preview mode
     prints a per-file occurrence summary plus a totals line; `-w`
     applies the rewrite to each affected file.
   - Shipped LSP consolidation: `cli/cmd_lsp.cpp::renameFor` now delegates
     to `jtml::renameSymbolInSource`, so the CLI and the language server
     share one implementation byte-for-byte. `RenameEdit` columns are
     0-based original-source ranges so the same record turns into an LSP
     `TextEdit` with no translation.
   - Extract component.
   - Add loading/error states.
   - Add wildcard route.

## Phase 4: Production Language Hardening

Status: ongoing.

1. Runtime component instances.
   - Shipped bridge: component calls, route components, and layout components
     emit explicit instance metadata (`component`, `role`, `params`,
     `locals`, source line), and the browser runtime exposes
     `window.__jtml_components` plus `window.jtml.getComponentInstances()`.
   - Shipped interpreter registry: component wrappers are semantic boundaries;
     local `let`/`when` declarations execute inside per-instance runtime
     environments, element bindings/events resolve through the owning
     instance, component-local state/actions remain isolated, and `/api/state`
     reports the component instances and their locals.
   - Shipped definition registry: each Friendly `make Component` now emits a
     hidden runtime definition marker, and the interpreter/C ABI/HTTP API expose
     original component contracts through `componentDefinitions` /
     `/api/component-definitions`.
   - Shipped runtime tooling API: `/api/components` exposes the registry
     directly and `/api/component-action` dispatches local component actions by
     instance id.
   - Next: replace compatibility source expansion with non-expanded component
     AST execution. The runtime regression smoke (see "Runtime Regression
     Smoke" below) now pins per-instance isolation through
     `POST /api/component-action`, providing a safety net for that rewrite.
   - Preserve current `data-jtml-instance` DevTools marker.

2. Fetch hardening.
   - Shipped: `method`, JSON `body`, `cache`, `credentials`, `refresh`,
     `timeout`, `retry`, explicit `stale keep|clear` behavior, and
     action-scoped `invalidate fetchName` revalidation after mutation actions.
   - Shipped route data first slice: `fetch ... lazy` registers a fetch without
     starting it, and `route "/path" as Page load fetchName` triggers it when
     the route matches.
   - Next: URL interpolation from route params and richer cache invalidation
     groups.

3. Routing hardening.
   - Shipped: params, wildcard, active links, guards, redirects, `activeRoute`.
   - Shipped: route layouts with `route "/path" as Page layout AppLayout`,
     injecting route content into the layout `slot`, plus `load fetchName`
     route-level lazy data loading.
   - Next: nested layouts and route-param-aware URL interpolation.

4. Stores.
   - Shipped: shared dictionary state and namespaced actions.
   - Next: module imports and linter awareness for store fields/actions.

5. Type annotations.
   - Shipped: accepted/erased first slice.
   - Shipped: linter checks obvious primitive mismatches for annotated
     `let`/`const`/`get` values and emits `JTML_TYPE_MISMATCH` diagnostics.
   - Shipped: linter checks ordinary action/function call arity and emits
     `JTML_ARITY`, while accounting for browser value arguments on input-style
     events.
   - Next: object field and collection element checks.

## Phase 5: Project And Ecosystem

Status: first slice implemented.

1. `jtml new app my-app`
   - Shipped as `jtml new app <dir>`.
2. Standard folders:
   - `pages/`
   - `components/`
   - `stores/`
   - `assets/`
   - Shipped first slice with `components/`, `stores/`, and `assets/`.
3. `jtml dev .`
4. `jtml build . --out dist`
5. VS Code/LSP diagnostics.
   - Shipped editor-support first slice: the bundled VS Code grammar now
     highlights core Friendly JTML 2 declarations (`jtml 2`, `make`, `when`,
     `route ... layout`, `extern ... from`) and keywords, and snippets now
     generate canonical Friendly app, component, fetch, route, store, effect,
     style, input-binding, and extern patterns.
   - Shipped CLI-backed editor loop: the VS Code extension runs
     `jtml check --json` and `jtml lint --json` for live diagnostics, exposes
     document formatting through `jtml fmt`, and provides a safe-fix command
     backed by `jtml fix -w --json`. (Now the LSP fallback path; the LSP
     transport below is the default.)
   - Shipped native LSP transport in the VS Code extension: launches
     `jtml lsp` via `vscode-languageclient` and routes diagnostics,
     formatting, completion, hover, definition, references, rename, code
     actions, signature help, document highlights, and workspace symbol
     search through the language server. Falls back to the CLI-shell path
     when the npm dependency is missing or `jtml.languageServer.enabled`
     is `false`. Adds a `JTML: Restart Language Server` command and hot
     toggling via the configuration setting.
   - Shipped native LSP foundation: `jtml lsp` speaks JSON-RPC/LSP over stdio
     with initialize, full text sync, parse/lint diagnostics, and
     whole-document formatting. See `docs/language-server.md`.
   - Shipped LSP intelligence first slices: generic Friendly completions,
     same-file symbol completions, keyword/user-symbol hover docs, document
     symbols for state, actions, components, routes, stores, effects, and
     externs, plus same-file go-to-definition.
   - Shipped cross-file LSP intelligence: definition, hover, and completion
     walk `use` / `import` graphs relative to the open document, with
     transitive resolution and cycle guarding. Imported completion items
     carry their origin file in `detail` so editors can disambiguate.
   - Shipped workspace-wide LSP intelligence: `workspace/symbol` queries
     scan the workspace root (recursive, with `build`/`dist`/`node_modules`
     skipped) plus the module graph of every open document, and
     `textDocument/prepareRename` + `textDocument/rename` rewrite a symbol
     across every file in that scan. Rename is string- and comment-aware
     and only rewrites identifiers reachable from the open document or its
     imports, so keywords and tag names are never touched.
   - Shipped `textDocument/references` using the same string- and
     comment-aware scanner that powers rename, with
     `context.includeDeclaration` honoured so editors can show usages
     without the original definition line.
   - Shipped `textDocument/codeAction` with `source.fixAll.jtml` umbrella
     and per-diagnostic quick-fixes for `JTML_FIX_HEADER`, `JTML_FIX_TABS`,
     `JTML_FIX_TRAILING_SPACE`, `JTML_FIX_FINAL_NEWLINE`, and
     `JTML_INDENTATION`. The action carries a `WorkspaceEdit` produced by
     `jtml::fixSource`, so editors apply mechanical repairs in one gesture.
   - Shipped targeted event-binding repair: `JTML_EVENT_ACTION` diagnostics
     such as `button "Save" click` now offer a quick-fix that infers an
     action name from the label, wires the event, and appends a matching
     `when actionName` block.
   - Shipped `textDocument/signatureHelp` and
     `textDocument/documentHighlight`. Signature help walks the same
     module graph as definition/hover, so calls to imported `make`,
     `when`, or `function` declarations show their parameter list and
     advance `activeParameter` across commas. Document highlights reuse
     the rename-grade reference scanner so they never highlight inside
     strings or comments.
   - Shipped `textDocument/selectionRange`: a nested SelectionRange tree
     per cursor position that walks word → trimmed line → full line →
     enclosing indentation blocks → whole document. Drives
     editor "expand selection" / "shrink selection" against Friendly
     JTML's indent hierarchy.
   - Next: richer incremental diagnostics and full incremental text sync.
6. Local-first package/component registry.

## Phase 6: JTML Code Modularization

Status: in progress.

1. Friendly `use Component from "./file.jtml"` and named imports
   `use { foo, bar } from "./mod.jtml"`.
2. Module graph resolved relative to the importing file with cycle detection.
   ✅ Linter walks the graph (`JtmlLinter::resolveImport`) and the LSP
   walks the same graph for definition, hover, and completion.
   ✅ Bare package imports resolve through nearest `jtml_modules/<name>/index.jtml`
   or `package.jtml`.
3. Per-file scopes for state, components, and stores; only `use`-declared
   names cross the file boundary.
   ✅ First explicit-export slice shipped: Friendly `export` marks public
   top-level declarations and named imports include only matching exports when
   a module opts in.
4. Linter walks the module graph so cross-file references are no longer
   flagged as undefined. ✅
5. LSP cross-file go-to-definition, hover, and completion through `use` /
   `import`. Imported completion items advertise their origin module so
   editors and AI tooling can disambiguate same-named symbols. ✅
6. Watch mode (`jtml dev`, `jtml serve --watch`) tracks every file in the
   graph, not just the entrypoint.
7. Local package install first slice.
   ✅ `jtml add <path|name> [--from path]` installs local `.jtml` files or
   package directories into `jtml_modules/`, records `jtml.packages.json`, and
   writes deterministic `jtml.lock.json` package/file fingerprints for review
   and reproducible team installs. ✅ `jtml install [--json]` restores missing
   local packages from the manifest when sources are available and verifies
   installed package fingerprints against the lockfile for CI.

## Phase 7: Language Interoperability

Status: first slice implemented.

1. Stable HTTP contract for `jtml serve`: `GET /` returns the page,
   `GET /api/health`, `/api/bindings`, `/api/state`, and `/api/runtime`
   expose machine-readable runtime data, and `POST /api/event` accepts
   `{elementId, eventType, args}` and returns `{ok, bindings, state|error}`.
   Documented under `docs/runtime-http-contract.md` so any backend or tool
   (Node, Python, Go, Rust, IDEs, test runners) can drive a JTML page.
2. Browser-side extern actions so JTML pages can call user-supplied JavaScript
   without inline scripts.
   - Shipped first slice: `extern notify from "host.notify"` emits a runtime
     marker, and event calls such as `click notify("Saved")` are intercepted
     client-side as `window.host.notify("Saved")`.
3. Mountable JTML page as a custom element so React, Vue, or Svelte hosts can
   embed a JTML island.
4. Small C ABI so any language with FFI can host the runtime; keep
   `jtml_engine` (pybind11) as the reference embedding.
   - Shipped first slice: `include/jtml/c_api.h` and `libjtml` expose
     `jtml_render`, `jtml_load`, `jtml_bindings`, `jtml_state`,
     `jtml_components`, `jtml_dispatch`, and `jtml_component_action`.
     Documented under `docs/embedding-c-api.md`.
5. `jtml export react` / `jtml export vue` lowerings that wrap transpiled HTML
   + bindings in a framework-native component for incremental adoption.
   - Shipped first slice as `jtml export <file> --target html|react|vue -o out`.
   - Added standards-based `--target custom-element` / `web-component`, emitting
     a `<jtml-app>` custom element for React, Vue, Svelte, Angular, or plain HTML hosts.

## Runtime Regression Smoke

`scripts/verify_all.sh` finishes with a Node-driven HTTP smoke that boots the
real binaries against the bundled examples and pins the production-language
paths surfaced through `docs/runtime-http-contract.md`:

1. **fetch GET** — `bindings.state.<name>` is seeded with `{data, error,
   loading}` before any client mutation, so SSR-time conditionals against
   fetch placeholders never crash.
2. **fetch POST** — same shape for POST fetches AND `GET /` returns 200,
   exercising nested conditions like `if login.data.user`.
3. **Routes + iframe-safe link interceptor** — `data-jtml-link="true"` is
   present on `link to "..."` anchors and the rendered runtime contains the
   `startLinkBindings` click interceptor that prevents Studio's `srcdoc`
   iframe from resolving hash anchors against the parent's base URL.
4. **Stores** — `bindings.state.auth` carries user-authored fields
   (`user`, `token`) through SSR.
5. **Component isolation** — two `Counter` instances boot with independent
   `count = 0`, and `POST /api/component-action` on instance 1 leaves
   instance 2 untouched (the canonical safety net for Phase 4.1's planned
   replacement of compatibility source expansion with non-expanded
   component AST execution).
6. **CSS hex colours in `style` blocks** — `#d8d4c9` round-trips through
   the Friendly-to-Classic lowering without being stripped or normalised.
7. **Studio `/api/run`** — the same transpiler reaches Studio previews,
   so the link interceptor and `data-jtml-link` markers surface in the
   iframe-rendered HTML the Studio shell embeds.

The block is gated on `node` and exits the script with a clear `FAIL: <label>`
message on regression. Artifacts are kept under `dist/runtime-smoke/` so any
failure can be inspected after the fact.

## Phase 8: Studio As The Language Home

Status: first production pass shipped.

1. `jtml studio` boots into a `00-welcome` lesson authored in JTML rather
   than Markdown + HTML, so JTML dogfoods itself as a content platform.
2. Every lesson under `tutorial/` is rewritten in canonical Friendly JTML so
   what users read is what `jtml fmt` produces.
3. Studio now carries a hub, categorized examples, tutorial lessons, docs,
   command palette, diagnostics, artifacts, formatter, fixer, linter, exporter,
   autosaved drafts, and versioned local history.
4. The Studio shell remains in `cli/studio_shell.cpp` for now;
   replacing it with a JTML-authored shell is a Phase 9 item once components
   and modularization can comfortably express the shell itself.
5. Studio is the canonical entry point linked from `site/index.html` and from
   `jtml --version --help`.

## Phase 9: Media And Graphics

Status: first slice in progress.

JTML needs a visual-interface lane so AI-generated apps can go beyond forms,
lists, and dashboards into media review, creative tools, educational diagrams,
charts, annotated screenshots, simple simulations, and audio/video workflows.

1. Document current support clearly.
   - Shipped surface today: `image`, `video`, `audio`, `embed`, raw HTML/SVG
     element fallback, scoped `style`, and `extern` for host-provided browser
     media APIs.
   - Keep proposed syntax clearly marked as planned until implemented.
2. Add examples and Studio samples for media basics.
   - Image gallery with accessible `alt` text.
   - Audio/video player with ordinary controls.
   - Canvas/SVG host bridge using `extern`.
3. Add file and dropzone bindings.
   - ✅ First slice shipped: `file "Choose image" accept "image/*" into selected`
     and `dropzone "Drop media" accept "image/*" into assets`.
   - Runtime state includes `name`, `type`, `size`, `lastModified`,
     `preview`, and `url`. Future metadata enrichment should add `width`,
     `height`, `duration`, and `error` when available.
4. Add reactive media controllers.
   - ✅ First slice shipped: `video src movie controls into player` and
     `audio src clip controls into playback`.
   - Exposes browser-side state such as `player.currentTime`,
     `player.duration`, `player.paused`, `player.ended`, `player.volume`,
     and client actions such as `player.play`, `player.pause`,
     `player.toggle`, `player.seek(seconds)`, and `player.setVolume(value)`.
5. Add SVG-first declarative graphics.
   - ✅ First slice shipped: `graphic aria-label "Chart"` lowers to
     accessible SVG, with `bar`, `dot`, `line`, `path`, `polyline`, `polygon`,
     and `group` lowering to standard SVG shape tags.
   - Next syntax: SVG text, axes, legends, and richer chart primitives.
   - Default to accessible SVG for static export and predictable rendering.
6. Add chart primitives after graphics.
   - ✅ First slice shipped: `chart bar data rows by month value total`
     lowers to accessible SVG metadata and the browser runtime renders bars
     from state or fetch results.
   - Next syntax: axes, legends, line charts, grouped/stacked bars, richer
     scale controls, and static export helpers.
7. Add 3D and highly customized interface mounts.
   - ✅ First slice shipped: `scene3d "Product" scene productScene camera orbit`
     lowers to an accessible canvas mount.
   - Runtime contract: call `window.jtml3d.render(canvas, spec)` when a host
     renderer exists; otherwise draw a visible fallback.
   - ✅ `scene3d ... into sceneState` publishes renderer metadata and
     fallback/host status; host renderers can call `spec.update(...)`.
   - ✅ First production lints warn on missing scene dimensions and unknown
     renderer names.
   - Next syntax: renderer packages, scene children such as `model`, `light`,
     `camera`, and Studio inspectors.
7. Keep heavyweight processing optional.
   - Image resize/crop/filter can start in browser canvas for small assets.
   - Video transcoding and advanced audio analysis should use explicit
     server/CLI integrations rather than hidden core dependencies.

## Verification Gates

Every phase should pass:

```sh
cmake --build build --target jtml_tests jtml_cli -j4
ctest --test-dir build --output-on-failure
scripts/verify_all.sh
```

For Studio changes, also smoke:

```sh
jtml studio --port 8123
curl -fsS http://localhost:8123/
curl -fsS -X POST http://localhost:8123/api/run \
  -H 'Content-Type: application/json' \
  --data '{"code":"jtml 2\n\npage\n  h1 \"Hello\"\n"}'
```
