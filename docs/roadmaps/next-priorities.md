# JTML Next Priorities

Status: active implementation order after the observable-first cleanup slice.

The next work should keep following the same rule:

```text
Friendly JTML -> typed AST -> semantic IR -> observable graph -> backends
```

Honest maturity rule: JTML is enterprise-relevant, not enterprise-ready. New
features should either strengthen semantic ownership, improve browser/live
runtime parity, or remove monolithic/embedded implementation debt. `jtml
doctor --json` reports the current stable/first-slice/experimental tiers and
the verification gates expected before release work.

## 1. Semantic Styling And UI Primitives

Status: first slice implemented. `theme`, semantic UI primitives, utility
modifier classes, generated CSS, `metric`, and semantic IR primitive/theme
counts are available. The remaining work is to harden and expand the design
system.

Why first:

- The current language has scoped `style` blocks and raw HTML/CSS access, but
  production apps still need a JTML-native way to express visual intent.
- Studio examples, tutorials, and AI-generated apps will improve immediately if
  they can use stable primitives instead of repeated hand-written CSS.
- It strengthens the semantic IR because layout, tone, spacing, theme, and
  component surfaces become analyzable instead of opaque strings.

Target syntax direction:

```jtml
jtml 2

theme
  color primary "#2563eb"
  color surface "#ffffff"
  space md 12
  radius md 12

page
  shell
    sidebar
      navlink "Dashboard" to "/"
    content
      panel title "Usage"
        grid cols 4 gap md
          metric "Users" users.data.total "Active" tone good
          metric "Latency" usage.data.latency "Median" tone warn
```

Implementation slices:

1. ✅ Add first semantic counts/edges for theme tokens and UI primitives.
2. ✅ Lower primitives to normal HTML plus generated classes.
3. ✅ Keep raw `style` and raw CSS as escape hatches.
4. ✅ Add first lints for impossible visual combinations: `cols` outside
   `grid`, and `tone` on structural layout primitives.
5. ✅ Expose the semantic UI surface through `jtml explain --json` as
   `semantic.ui`, including authored theme token counts separately from
   generated CSS token references, while preserving the older
   `semantic.uiModifiers` key for tool compatibility.
6. ✅ Studio sample migration shipped across the embedded Explorer set:
   basics, data/state, navigation, media/graphics, and composition examples now
   use `theme`, `panel`, `stack`, `grid`, `card`, `metric`, `toolbar`, `alert`,
   `loading`, and `error` where appropriate, with regression tests that parse,
   lint, transpile, and assert semantic UI coverage.
7. ✅ Expand primitives into a documented, versioned component-kit surface:
   `jtml ui [--json]` now exposes the canonical primitive, modifier, and theme
   token catalog used by docs, tests, Studio examples, and AI/tooling prompts.
8. ✅ First catalog-backed visual lint shipped: `jtml lint` now warns on
   unsupported semantic UI modifier values such as `cols 9`, `gap huge`, or
   `tone loud`.
9. ✅ First accessibility-oriented primitive lint shipped: semantic surfaces and
   overlays now warn when `panel`, `card`, `modal`, `drawer`, or `toast` lack
   `title` or `aria-label`.
10. ✅ First form/navigation semantic lint shipped: `field` now warns without
    an input-like control, `tabs` warns without a `tab`, and `tab` warns
    without an action or route target.
11. ✅ First overlay behavior lint shipped: `modal` and `drawer` now warn
    without a close/dismiss/cancel/hide action. Next: extend this into keyboard
    semantics, focus behavior, and form submission contracts.
12. ✅ First semantic accessibility defaults shipped: `modal`/`drawer`,
    `alert`, `error`, `toast`, `loading`, `empty`, `tabs`, and `tab` now lower
    with sensible platform roles/live-region attributes. Overlays are focusable
    with `tabindex="-1"`, `loading` exposes busy state, and `tab` uses
    `type="button"` to avoid accidental form submission. Ordinary clicked
    buttons now also default to `type="button"` unless the author explicitly
    sets a type. Next: add richer keyboard behavior on top of those stable roles.
13. ✅ Field label semantics shipped: `semantic.ui.uses` now separates
    `hasControl` from `hasLabel`, and `jtml lint` reports
    `JTML_UI_FIELD_UNLABELED` when a field wraps a control without a usable
    label.

## 2. Browser-Local Production Runtime

Status: first two slices implemented. `--target browser` emits a client
manifest for state, derived values, and actions. The runtime can now execute
local actions, recompute derived values, apply conditions/loops, run fetch
markers, handle route links/params/guards, and apply reactive attributes such
as `image src selected.preview` without a WebSocket. Browser-side image
processing markers now write their result back into observable client state.

This is the next architectural step after styling starts to stabilize.

Why second:

- JTML must not be defined by WebSockets.
- Real public deployments should work as static files with local browser
  reactivity.
- The semantic usage API and observable graph give this emitter a clearer
  source of truth than the old interpreter-first model.
- Performance competitiveness requires a compiler-first production browser
  target. The current manifest/body-plan runtime is a bridge toward generated
  JS component functions, fine-grained updates, keyed DOM diffing, runtime size
  budgets, and a dev/prod split. Live HTML patches remain for Studio, dev,
  internal live apps, and server-owned UI rather than framework benchmarks.

Target:

```sh
jtml build app.jtml --target browser --out dist
```

Output:

```text
dist/index.html
dist/jtml-runtime.js
dist/components/index.js
dist/app.js
dist/jtml-update-plans.js
dist/assets/...
```

Current browser builds emit `dist/index.html`, `dist/jtml-runtime.js`,
`dist/components/index.js`, `dist/app.js`, and a legacy
`dist/jtml-update-plans.js` alias. `jtml-runtime.js` is now primary for browser
builds instead of duplicating the runtime inline. `components/index.js` now owns
the production `window.__jtml_static_component_plan_index`; the legacy
`jtml-update-plans.js` asset keeps `window.__jtml_static_update_plans` and
source-rich `bodyPlan` metadata for compatibility/debug tooling. The component
module no longer publishes the legacy update-plan global or carries source-rich
body-plan payloads. It carries component read indexes, unsafe-entry lists, root
create operations, precompiled element `partsPlan` records, first-slice
text/region/nested/element patch operations, and first-slice static component
create/update modules without relying on runtime `eval` / `new Function`.
Supported root text, button, leaf element, direct-safe container nodes, and
safe `if`/keyed-`for` control-flow regions now emit escaped HTML construction in
`components/index.js`; nested-component, slot, and unsupported shapes still use
helper/fallback create paths. Static update functions also emit direct per-node
patch cases for text, button, element, container-attribute, safe control-flow
region, and nested-component nodes before falling back to operation records.
The runtime-plan layer now owns the canonical expression-plan producer for
literals, booleans, numbers, null, simple dot paths, unary `!`,
binary/comparison/logical operators, and ternary conditionals; static component
emission consumes that same API instead of maintaining a private expression
parser. Those plans travel through text/element patch operations, element
content, attributes, modifiers, component action arguments, body-plan
conditions, `for` collection and key expressions, nested component parameter
evaluation, and browser-local body-plan action assignments/local declarations
before falling back to the generic browser expression evaluator. Static
component modules now emit direct JS expression functions for simple and
first-slice composite plans, generated
text/button/leaf-element/container/control-flow create functions that do not
require runtime helper availability unless they hit a fallback shape, and generated
text, element attribute/content, button label/action-argument, and safe region
patches that update DOM directly without a global runtime-helper requirement
before falling back to runtime patch helpers. Runtime-plan read
analysis also recognizes value-taking attributes such as `title selected` as
reads of `selected` instead of treating `selected` as a standalone boolean
attribute.
`rootCreateOperations` and per-entry operation payloads remain as fallback/debug
metadata instead of the primary path. The browser runtime prefers those static
modules/plans when
available and falls back to runtime plan compilation only when no matching
static plan exists. `scripts/benchmark_runtime.sh` now provides a first-slice
smoke benchmark over `tests/fixtures/performance/` with browser asset size
budgets, production/legacy asset-shape checks, and a `control_flow` fixture that
asserts safe `if`/keyed-`for` regions, keyed list markers, and direct safe-region
patching are generated in `components/index.js`: 50 KB for `index.html`, 260
KB for `jtml-runtime.js`, 180 KB for `components/index.js`, 20 KB for
`app.js`, and 180 KB for the legacy `jtml-update-plans.js`, all meant to
tighten as direct generated update modules keep replacing operation-record
patch helpers.

Implementation slices:

1. ✅ Emit state cells, derived recomputation, action dispatch, and DOM
   bindings for browser-local builds.
2. ✅ Apply browser-local reactive attributes and media/image processing state.
3. ✅ Support fetch nodes with fuller browser-local state parity: fetch values
   now keep the stable `{ loading, data, error, stale, attempts, hasData }`
   core plus `status`, `ok`, `url`, `method`, and `updatedAt` metadata; stale
   UI is only marked stale after a previous successful response exists;
   `window.jtml` exposes named fetch refresh hooks for devtools and demos.
   Fetch URLs now interpolate browser-local state at request time, so route
   params such as `{id}` work with `route "/user/:id" as User load user`.
   Fetch invalidation groups now let larger apps write `fetch ... group name`,
   `invalidate group name`, or `invalidate all` instead of wiring every fetch
   by hand. Fetch cache identity and timed revalidation controls now let
   authors write `fetch ... key expr dedupe every 30000 background`, giving
   browser-local fetches explicit request identity, duplicate-request skipping,
   and interval revalidation that pauses on hidden tabs unless background
   refresh is explicitly requested.
4. ✅ Bridge hash routing and fetches toward graph/runtime ownership: browser-local builds
   now emit a compiler route manifest, and the browser runtime collects a live
   route registry from that manifest when available. It publishes
   `window.jtml.routeManifest`, `window.jtml.routes`, `getRoutes()`,
   `getCurrentRoute()`, `navigate(path)`, `activeRouteName`, and route
   lifecycle events. The semantic IR now also exposes structured
   `routeRecords` with path, component, params, and route-load fetches, and
   browser-local route manifest emission consumes those semantic records instead
   of running its own lowered-AST route scan. The semantic IR also exposes
   structured `fetchRecords` with URL, method, body expression, refresh action,
   cache, credentials, timeout, retry, stale, group, key, dedupe, timed
   revalidation, background, and lazy metadata; browser-local
   fetch registration now starts from those records and falls back to DOM markers
   only for compatibility. The semantic IR now also exposes structured
   `componentDefinitions` and `componentInstances`, giving explain/tooling a
   stable component graph without parsing lowered marker attributes.
   Browser-local component manifests now start from those records and fall back
   to DOM marker scans only for compatibility.
5. ✅ Align `jtml serve` with the same graph for component metadata: the live
   interpreter now registers runtime component definitions and instances from
   semantic records first, falling back to lowered DOM marker scans only for
   compatibility. Component definitions and instances now also expose semantic
   runtime plans: local state/actions/derived/effects, event bindings, slot/body
   shape, owning environment ids, and semantic action validation are available
   through browser manifests, `/api/state`, `/api/components`, and
   `/api/component-definitions`. Next slice: render component templates directly
   from non-expanded component definitions while keeping this manifest/runtime
   contract stable.
6. Add parity tests comparing browser-local output and live runtime behavior.

## 3. Interop And Escape Hatches

Interop should follow the browser-local runtime, not precede it.

Why third:

- JTML’s promise is a simpler semantic layer over the full web platform, not a
  smaller prison.
- Once the browser runtime is real, `extern`, custom elements, canvas, SVG,
  WebGL, Monaco, Chart.js, Mapbox, and Three.js bridges have a stable target.

Implementation slices:

1. ✅ Formalize `extern action from "host.path"` in semantic IR with
   `semantic.nodes.externs`, `extern:<name> --calls--> <window.path>` edges,
   and lint review warnings.
2. Add typed host hooks for canvas/SVG/custom elements.
3. ✅ Add safe raw `html` and `css` blocks as explicit escape hatches,
   semantic `rawCssBlocks` / `rawHtmlBlocks` counts, and lint warnings.
4. Document package and asset boundaries.
5. Add export targets only after the runtime contract is stable.

## 4. Component Instance Semantics

Component isolation exists as a runtime bridge today, but semantic ownership
should become direct instead of expansion-derived.

Why fourth:

- Component instance semantics are essential for large apps, diagnostics,
  Studio inspectors, and framework exports.
- The current metadata bridge is useful, so this can be done incrementally.

Implementation slices:

1. ✅ Add first-class `ComponentDefinition` and `ComponentInstance` semantic
   records.
2. ✅ Preserve first-slice component-owned semantics in `ComponentDefinition`:
   params, local state, local derived values, local actions, local effects,
   event bindings, body payload, body/root-template counts, source line, slot
   presence, and a semantic runtime plan. These now appear in
   `jtml explain --json`, browser-local component manifests, and live runtime
   component definition responses.
3. ✅ Make `jtml explain` and browser/live runtime registries consume component
   records before compatibility marker scans.
4. Preserve emitted custom events/slots as explicit authoring syntax once the
   component API grows beyond implicit `slot` insertion and DOM event bindings.
5. Gradually move runtime execution toward direct non-expanded component
   template rendering.

## 5. Studio And Docs As Product Surface

Studio remains the user-facing proof of the language.

Implementation slices:

1. Keep docs loaded from this organized folder structure.
2. Add a docs index view that mirrors `docs/README.md`.
3. Keep tutorial notebook mode as the default learning mode.
4. Move compatibility IR behind diagnostics, not primary authoring.
5. ✅ Make every embedded Studio example use the same semantic styling system
   where normal UI is being demonstrated.
6. ✅ Add a canonical language catalog and `jtml keywords --json`; keep README,
   reference docs, Studio mini-reference, VS Code grammar, and LSP completions
   aligned with that catalog instead of independent keyword folklore.

## 6. Platform Modularization And Governance

Status: active incremental hardening lane. This is the enterprise-readiness
lane, not a new feature family. The repo now has destination folders for the
major ownership areas, but files should move only when the boundary is already
clear and tests cover the contract.

Why now:

- `friendly.cpp`, `interpreter.cpp`, `transpiler.cpp`, and
  `cli/studio_shell.cpp` still carry too many responsibilities.
- Studio examples/docs/reference content should become external data consumed
  by the shell, not long embedded literals.
- The language needs internal contracts that a larger team can reason about:
  semantic IR, runtime components, browser runtime, emitters, diagnostics, LSP,
  package tooling, and Studio product surface.

Target internal shape:

```text
src/
  core/
  friendly/
  semantic/
  runtime/
    browser/
    live/
    components/
    fetch/
    routes/
  emit/
  interop/
  studio/
  lsp/
```

Implementation slices:

1. ✅ Keep `jtml doctor --json` as the readiness and governance status
   contract.
2. ✅ First Studio content externalization slice: playground samples now live in
   `studio/samples/manifest.json` plus `.jtml` files and are served through
   `/api/studio/samples`; the embedded shell list remains only as a fallback.
3. ✅ First Studio reference externalization slice: the mini-reference now lives
   in `studio/reference/catalog.json`, is served through
   `/api/studio/reference`, and renders into the Studio Reference panel while
   keeping fallback markup in the shell.
4. ✅ First Studio sidebar catalog slice: sample category labels and pinned
   template names now live in `studio/sidebar/catalog.json` and are served
   through `/api/studio/sidebar`.
5. Split semantic UI and language catalogs into reusable data/loader surfaces
   consumed by CLI, docs, Studio, LSP, and lint.
6. ✅ Browser runtime emitter/asset split: the generated browser/live runtime
   script now lives behind `jtml::emitBrowserRuntimeScript()`, and that emitter
   is a small parameterizing wrapper around owned runtime asset chunks in
   `src/browser_runtime_assets.cpp`. `src/transpiler.cpp` stays focused on
   HTML/manifest emission while the public `JtmlTranspiler` API remains stable.
7. ✅ First client manifest emitter split: the browser-local manifest now lives
   behind `jtml::emitClientManifestScript()` in
   `src/client_manifest_emitter.cpp`, and shared expression serialization moved
   to `src/expression_source.cpp`. `src/transpiler.cpp` is now primarily the
   HTML emitter and delegates runtime script plus manifest generation.
8. ✅ Runtime plan extraction: browser-local runtime data now flows through a
   typed `jtml::RuntimePlan` before JSON manifest serialization. This keeps the
   existing manifest shape stable while giving the next backend slices a shared
   state/derived/action/fetch/route/component plan to consume.
9. ✅ Runtime plan adoption slice: `jtml explain --json` exposes `runtimePlan`,
   component definition plans carry decoded body source, and the live
   interpreter now registers component definitions/instances from the same plan
   consumed by browser-local manifest generation.
10. ✅ Component body plan slice: `RuntimePlan` component definitions now expose
   structured body nodes (`state`, `derived`, `action`, `assignment`,
   `effect`, `slot`, `template`) plus indent, parent-index, and render-root
   metadata through `runtimePlan.componentDefinitions[].bodyPlan`,
   browser-local manifests, and live `/api/component-definitions`.
   Next: use that body plan for direct non-expanded component instance
   execution and live/browser parity checks.
11. ✅ SemanticProject contract cleanup: module imports now use explicit
   `InvalidSemanticModuleId` for unresolved edges, retain attempted
   `resolvedPath`, and resolve through the shared `resolveJtmlModulePath()`
   module resolver instead of a simplified graph-local path joiner.
12. ✅ RuntimePlan JSON serializer split: core owns
   `jtml::runtimePlanToJson()` and `jtml::runtimePlanBodyPlanToJson()`; CLI
   explain and browser manifest body-plan emission now consume the shared
   serializer instead of duplicating JSON shapes.
12a. ✅ First runtime-folder ownership slice: the CSP-safe static update-plan
   asset emitter now lives under `include/jtml/runtime/` and
   `src/jtml/runtime/`, with the old top-level include retained as a
   compatibility shim. CMake now groups core sources by syntax, semantic,
   runtime, and tooling ownership so future moves can happen boundary by
   boundary instead of as a risky whole-repo shuffle.
13. ✅ Per-file SemanticProject ownership slice: `SemanticProject` now accepts
   `SemanticModuleSource` records, and `jtml explain --json` builds project
   imports from collected source files so nested imports are attributed to the
   module that authored them.
14. ✅ Semantic project export/API slice: `SemanticProgram` captures top-level
   `export` and `export use` records, `SemanticProject` attaches those exports
   to the owning module, and project graph JSON is emitted by
   `jtml::semanticProjectToJson()` from core instead of `cmd_basic.cpp`.
15. ✅ RuntimePlan typed body graph slice: component body nodes now expose
   parent and child edges, source head/name/text, assignment operators, and
   expression payloads in the shared RuntimePlan JSON and live interpreter
   component-definition state.
16. ✅ Per-file semantic ownership slice: `SemanticProject` modules now retain
   their own semantic summaries, exported declarations populate the same
   module-local state/action/component/store buckets, and `explain --json`
   exposes those summaries beside imports and exports.
17. ✅ Semantic project diagnostics slice: `analyzeSemanticProject()` reports
   unresolved imports and missing named exports from the core module graph, and
   project JSON exposes those issues for `explain`, Studio, and future LSP use.
18. ✅ Human-readable project explain slice: default `jtml explain` now reports
   semantic project modules, importer-owned imports, exported declarations,
   per-module semantic summaries, and project diagnostics so users can debug
   modular apps without reaching for JSON first.
19. ✅ Source-first import span slice: `SemanticImport` records now retain
   `sourceLine` / `sourceColumn`, `SemanticProject` imports copy those
   coordinates into their span, and project graph JSON exposes the importer
   location for future diagnostics, Studio, and LSP use.
20. ✅ Per-file module IR slice: `SemanticModuleSource` and `SemanticProject`
   now retain a typed structural IR summary for each module that can be parsed
   in isolation, including syntax mode, top-level AST node order, typed node
   counts, total node count, and parse errors. `jtml explain` exposes the same
   summary in text and JSON.
21. ✅ Import-aware isolated parse slice: explain-time module IR now retries
   Friendly modules with explicit imported-component stubs and reports syntax
   as `friendly+import-stubs`, allowing entry files with imported route or
   component references to produce typed structure without whole-graph
   compatibility expansion.
22. ✅ Retain full per-file typed AST ownership in `SemanticModuleSource`:
   module sources and project modules now carry a cloned typed AST record beside
   their compact IR summary. The record stays copyable through shared ownership,
   keeps the `friendly+import-stubs` marker when synthetic import stubs are used,
   and gives future diagnostics/linker/runtime passes real per-file syntax
   ownership instead of source-only rescans.
23. ✅ Imported symbol identity slice: `SemanticModuleImport` now records
   resolved imported symbols and export kinds after module linking, and
   `jtml explain` / `jtml explain --json` expose that `Dashboard` is a `make`
   component while `appState` is a `store`.
24. ✅ Re-export identity slice: project import identity now follows
   `export use ... from ...` barrel modules to the ultimate exported
   module/kind, so a `Card` imported through `components/index.jtml` still
   resolves as `make Card` from the component implementation file.
25. ✅ Re-export diagnostics slice: `analyzeSemanticProject()` now reports
   unresolved re-export targets and re-export cycles as semantic project
   issues instead of letting malformed barrel modules collapse into anonymous
   missing-symbol behavior.
26. ✅ First retained-AST runtime slice: `RuntimeProjectPlan` now consumes
   `SemanticProject` retained per-file ASTs and emits module-scoped runtime
   plans through `jtml explain --json`, preserving the old linked `runtimePlan`
   while giving backends a module-owned execution surface.
27. ✅ Browser manifest project-plan slice: browser-target transpile/build now
   embeds the `RuntimeProjectPlan` under the client manifest `project` key,
   while preserving existing linked-manifest fields for runtime compatibility.
28. ✅ Browser runtime project-consumption slice: browser-local startup now
   merges the embedded semantic project plan into the active runtime manifest.
   Module-owned component definitions can replace linked compatibility
   definitions, while state/actions/fetches/routes fill missing runtime
   surface area conservatively.
29. ✅ First direct component body-plan execution slice: browser-local
   component instances now render common templates from
   `RuntimePlan.componentDefinitions[].bodyPlan`, initialize per-instance local
   state, recompute derived values, render simple `if`, `else`, and `for`
   nodes, execute simple local assignment actions plus action-local `let` /
   `get` declaration nodes, carry authored slot plans, render slots and nested
   component calls, preserve common attributes, pass simple action arguments,
   and re-render the owning instance. This is the first
   real break from expanded compatibility DOM as the component render surface.
30. ✅ Manifest safety/runtime identity slice: explain serializers and client
   serializers are now separate. Browser manifests omit absolute source paths,
   body source/hex payloads, semantic summaries, and parse metadata; embedded
   JSON is script-escaped; `friendly+import-stubs` modules are tooling-only for
   client plans; component lookup carries module identity before falling back
   to name-only compatibility.
31. ✅ Live body-plan action parity slice: the live interpreter now exposes
   slot plans, executes simple component body-plan assignment actions plus
   action-local `let` / `get` declarations, guarded `if`/`else`, array `for`,
   and guarded `while` action bodies with action arguments, preserves
   numeric/string `+=` semantics plus numeric `-=`, `*=`, `/=`, and `%=`, fails closed to compatibility dispatch for
   unsupported action bodies, and has a browser/live metadata parity
   regression.
32. ✅ Live body-plan template surface slice: `/api/state` component records now
   include `renderedHtml` generated from the same body plan for common
   templates, parameter expressions, slots, and nested component calls. This is
   a tooling/parity surface; the live DOM transport still has compatibility
   fallback.
33. ✅ Direct nested ownership and semantic UI render parity slice:
   browser-local nested component calls now register dynamic runtime identities
   so nested local actions can target their owning instance; browser-local and
   live body-plan renderers preserve semantic UI primitives plus modifiers as
   `jtml-*` classes and `data-jtml-ui*` attributes.
34. ✅ Live body-plan transport patch slice: `jtml serve` exposes
   `/api/rendered-components`, `/api/event` and `/api/component-action`
   responses carry `renderedComponents`, and the browser runtime patches
   supported live component wrappers from body-plan HTML while keeping
   compatibility DOM fallback for unsupported shapes.
35. ✅ Recoverable project diagnostics slice: `jtml explain` now falls back to
   a standalone entry parse when imported modules fail, uses a recoverable
   source-file collector for tooling, preserves unavailable module IR with
   source coordinates, and reports `JTML_MODULE_PARSE` as a semantic project
   issue instead of losing the module graph. Import issue records for
   unresolved imports, missing named exports, and bad re-export chains now also
   carry the importing module path plus authored import line/column.
36. ✅ Module-aware live component identity slice: interpreter runtime
   component definitions and instances now carry `moduleId` /
   `definitionModule`, `/api/state` and `/api/component-definitions` expose
   those identities, and live nested/component action lookup prefers
   `(moduleId, componentName)` before name-only compatibility fallback. Project
   runtime-plan tests now pin duplicate exported component names across modules
   so a `Card` import resolves to its authored module instead of whichever
   global `Card` appears first.
37. ✅ Import-aware nested body-plan identity slice: component body-plan nodes
   now carry `definitionModule`, so imported nested component calls inside
   encoded component bodies bind to the module that exported the target
   component. Browser-local and live nested renderers consume that field before
   parent-module/name-only fallback.
38. ✅ Live rendered component action bridge: buttons emitted by the live
   body-plan renderer now preserve direct component action metadata and simple
   action arguments. The browser runtime routes patched live button clicks to
   `/api/component-action`, applies returned bindings/render patches, and pins
   this with browser/live parity regressions.
39. ✅ Body-plan render support truthfulness slice: live component state and
   `/api/rendered-components` now expose `renderedHtmlSupported`, so components
   with action/state semantics but no renderable template explicitly stay on
   compatibility DOM fallback instead of reporting template parity.
40. ✅ Initial live body-plan transport ownership slice: `jtml serve` now
   injects supported component wrappers from body-plan HTML into the initially
   served document, marks them with `data-jtml-live-body-plan-transport="body-plan"`
   and a stable rendered hash, and lets the browser runtime verify current
   markup without a startup DOM patch while still using `/api/rendered-components`
   for event/action updates. The same slice pins route-link parity so
   `link ... to` keeps hash-link interception metadata after live body-plan
   rendering.
41. ✅ Rich platform attribute parity slice: browser-local and live body-plan
   renderers now share a broader attribute/boolean surface for forms, media,
   SVG, `aria-*`, `data-*`, quoted values with spaces, and Friendly aliases
   such as `link`, `navlink`, `image`, `file`, `dropzone`, and `checkbox`.
   The input aliases default to the expected platform `type` where the author
   did not provide one.
42. ✅ Live nested component instance ownership slice: the live body-plan
   renderer now retains dynamic nested component instances for supported
   rendered components, routes their local buttons through `/api/component-action`,
   evaluates nested `let`/param values as real runtime values, and gives nested
   components inside `for` loops loop-aware identities so repeated children keep
   separate local state. Browser-local action-argument parsing now uses the same
   top-level argument splitting contract for quoted strings with spaces.
43. ✅ Live nested component cleanup slice: after each supported top-level
   body-plan render, the interpreter prunes stale dynamic nested descendants
   that were not rendered in the current pass. Removed loop/branch children can
   no longer receive stale `/api/component-action` dispatches.
44. ✅ First keyed/reordered lifecycle slice: Friendly component loops may now
   use `for item in items key item.id`. Classic compatibility lowering strips
   the key tail, while the semantic body plan keeps `keyExpression`; browser
   and live nested component IDs prefer that key over the loop index so local
   state follows reordered items. Browser-local direct renders now also prune
   stale dynamic descendants after each body-plan render, matching the live
   cleanup path for removed keyed children.
45. ✅ First named/multiple slot insertion slice: component definitions may use
   named `slot header` / `slot footer` sites plus the default `slot`; call
   sites provide matching `slot name` blocks; compatibility expansion injects
   only selected slot groups; and browser/live body-plan renderers select from
   the same `slotPlan`.
46. ✅ First emitted component-event bridge and `emits` contract: component
   definitions may declare `make Child emits picked cancelled`, named payloads
   with `make Child emits picked(item)`, or first-slice typed payload metadata
   with `make Child emits picked(item: string)`. Semantic analysis, runtime
   plans, browser manifests, explain JSON, and live component-definition JSON
   expose the emitted-event list, `emitArity` map, `emitPayloads`, and
   `emitPayloadTypes` metadata. Browser-local and live direct component
   dispatch enforce simple emitted payload types before forwarding to parent
   handlers.
   Nested component
   calls may use `Child on picked choose` or `Child on picked choose("preset")`;
   buttons or other direct component actions that fire `picked(...)` on the
   child route through the explicit parent mapping when no local child action
   exists. If a component declares emits, `on event handler` is validated
   against that list; if a payload arity is declared and the parent action is
   known, preset handler args plus emitted args are checked against the parent
   action signature. Browser-local and live body-plan paths both append emitted
   args after any preset handler args and re-render the owning parent instance.
47. ✅ Nested named-slot plan parity: nested component calls inside component
   body plans now carry a reindexed authored `slotPlan`, so default and named
   slots work when a component composes another component. Browser-local and
   live body-plan renderers both use this cloned slot plan instead of relying
   on compatibility-expanded DOM.
48. ✅ First body-plan action-call node: component action bodies can now contain
   direct calls such as `incBy(2)` and `picked("Ada Lovelace")`. The runtime
   plan marks these as `kind: "call"` instead of misclassifying them as
   templates. Browser-local and live direct execution first dispatch to local
   component actions with evaluated arguments; if no local action exists, the
   same node can emit a declared component event through the existing `emits`
   contract. This reduces compatibility fallback pressure for composed
   component actions.
49. Complete direct/live component parity: keep expanding the supported
   body-plan subset until the live compatibility DOM transport is needed only
   for explicit fail-closed fallback cases. Shipped follow-up: direct live and
   browser-local action execution restores top-level and nested action
   parameters plus loop iterators instead of leaking temporary bindings, and
   direct body-plan fallback reasons now preserve component/action context,
   component definition line, authored body line, and nearby authored body-plan
   text; when compatibility fallback also fails, the source-first body-plan
   context is preserved beside the compatibility error. Browser-local fallback
   records now carry the same authored action/source context and are exposed on
   `window.jtml.directComponentFallbacks`. Typed
   emitted-event diagnostics also name the authored payload and component
   definition line. Body-plan nodes now expose source-line plus first
   read/write dependency metadata, giving future optimized browser compilation
   a direct update surface. Browser-local direct component actions now use this
   metadata for a conservative rerender gate: if action writes do not affect
   rendered reads, the runtime can skip a full component rerender and expose
   `directComponentLastAction` telemetry for Studio/compiler diagnostics.
   Simple affected leaf body-plan nodes now carry direct DOM markers and
   managed-attribute metadata. Body-plan read dependencies now come from parsed
   expression ASTs for real expressions, so member/subscript expressions track
   precise read paths plus their owning state roots instead of relying on broad
   token scans. Member and
   subscript writes now also include their owning observable root for update
   invalidation. Browser-local and live direct actions can now mutate existing
   dict/object properties, create missing dict/object and non-negative array
   path containers, and update array/dict subscript targets without
   compatibility dispatch; richer assignment targets remain planned. Live
   body-plan `for` loops now share the browser value model for arrays, strings,
   dict/object values, and scalar singletons in both rendering and action
   execution. The browser
   runtime tries in-place text/attribute updates first, then leaf replacement
   from the body plan, then full body-plan rerender before compatibility is
   considered. It also caches a compiled per-component update plan for those
   simple leaves, keyed by module/name/body shape, so repeated actions use a
   compiler-shaped update surface instead of rediscovering patchable nodes.
   Patchable leaves are now represented as explicit text/button/element patch
   operations. Those operations carry precompiled element-part and
   click-invocation shapes, and each cached plan now owns generated browser
   update-function source plus an indexed executable update function keyed by
   rendered reads; unsafe operation exits record source-line fallback telemetry,
   and a conservative interpreted updater remains as the fallback when
   generated functions are unavailable. Structured
   container body-plan nodes can now patch their own compiled attributes in
   place without touching children, keeping more common layout/state updates on
   the direct body-plan path. `if` and `for` nodes now render stable region
   anchors and compile to direct region-replacement operations when their
   condition or collection changes. The old recursive heuristic patch fallback
   is now removed from this browser-local path; unsupported shapes fall through
   to body-plan node replacement or full direct component rerender instead of
   another source-shaped patch scan. Direct `for` regions now emit per-item
   key/index markers and first-slice lifecycle telemetry for inserted, removed,
   and moved keys, preparing the optimized keyed DOM diff without claiming it is
   complete. Compiled `for` region patch operations now try a conservative
   keyed patch that reuses/reorders item wrappers by key and updates item
   contents, reports retained/inserted/removed/moved key sets, and prunes
   removed nested dynamic children for keyed list items, falling back to region
   replacement for duplicate/unsafe keys. Next
   nested component call wrappers now carry stable body-node anchors and compile
   to explicit nested-component patch operations, reducing full-parent rerenders
   when parent state affects composed children. Slot insertion sites now carry
   stable `slot` region anchors and compile as region patch operations.
   Action-body `while` remains part of the body-plan action contract, but
   render/template `while` is linted as `JTML_TEMPLATE_WHILE`; rendered
   repetition should use `for` so browser/live runtimes can diff visible
   children predictably. Next gaps: broader
   keyed collection diff semantics, richer path-assignment semantics, richer
   action expressions beyond local declarations/calls/path mutation, advanced
   media/graphics attributes, and broader browser/live behavior parity checks.
   Live body-plan rendering also reports patch telemetry for patched/current,
   unsupported, and missing component records so parity checks can compare live
   behavior against browser-local body-plan execution.
50. Add compiler-first browser production target slices: build on the current
   CSP-safe `jtml-update-plans.js` asset, its precomputed read indexes, and
   first-slice static create/update modules; ✅ first expression-plan payloads
   now cover literals/simple paths in text, attributes, modifiers, element
   content, component action arguments, conditions, `for` collection/key
   expressions, and nested params; static modules now emit direct expression
   functions and direct simple text DOM patches; next precompile richer action
   bodies, broaden static update functions beyond the current body-plan patch
   helpers, expand text/attribute/button patches into keyed region updates, emit full
   static JS runtime assets for production, add benchmark fixtures/budgets
   beyond the current smoke script, and keep the current manifest interpreter
   plus opt-in dynamic generated-function bridge as dev and compatibility
   fallback.
51. Move larger Studio prose blocks out of `cli/studio_shell.cpp` using the
   same catalog endpoint pattern.
52. Add contract-first JTL backend API design docs and prototypes only after
   runtime/module semantics are stable. Target governed internal operations:
   approval consoles, incident consoles, AI governance, cost controls, and
   deployment controls. Planned phases are contracts/types/errors, OpenAPI,
   policies/validation, runtime adapters, JTML `fetch`/future `call`
   integration, and enterprise hardening.
53. Add security, compatibility, deprecation, contribution, benchmark, and
   release-policy docs once the internal contracts stop moving every slice.

## Decision

Do semantic styling, browser-local runtime parity, direct component template
execution, and platform modularization in a balanced loop. Contract-first JTL
backend APIs, interop, and export targets should follow once the browser/local
runtime contract is stable enough that external hosts are not binding to
transitional behavior.
