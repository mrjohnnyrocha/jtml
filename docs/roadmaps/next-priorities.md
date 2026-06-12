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

Target:

```sh
jtml build app.jtml --target browser --out dist
```

Output:

```text
dist/index.html
dist/app.js
dist/jtml-runtime.js
dist/assets/...
```

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

Status: planned next hardening lane. This is the enterprise-readiness lane, not
a new feature family.

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
6. ✅ First browser runtime emitter split: the generated browser/live runtime
   script now lives behind `jtml::emitBrowserRuntimeScript()` in
   `src/browser_runtime_emitter.cpp`, leaving `src/transpiler.cpp` focused on
   HTML/manifest emission while keeping the public `JtmlTranspiler` API stable.
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
   state, recompute derived values, render simple `if` and `for` nodes, execute
   simple local assignment actions, and re-render the owning instance. This is
   the first real break from expanded compatibility DOM as the component render
   surface.
30. Complete direct component parity: add body-plan `else`, slots, nested
   component calls, semantic UI modifiers/attributes, action arguments, and the
   matching live-interpreter path over the same body-plan contract.
31. Move larger Studio prose blocks out of `cli/studio_shell.cpp` using the
   same catalog endpoint pattern.
32. Add security, compatibility, deprecation, contribution, benchmark, and
   release-policy docs once the internal contracts stop moving every slice.

## Decision

Do semantic styling, browser-local runtime parity, direct component template
execution, and platform modularization in a balanced loop. Interop and export
targets should follow once the browser/local runtime contract is stable enough
that external hosts are not binding to transitional behavior.
