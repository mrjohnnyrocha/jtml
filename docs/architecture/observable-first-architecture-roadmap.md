# Observable-First Architecture Roadmap

JTML should become an observable-first UI language over the full web platform,
not another frontend framework and not a WebSocket-only runtime.

The durable pipeline is:

```text
Friendly JTML
  -> typed AST
  -> semantic IR
  -> observable graph
  -> component instance graph
  -> style/theme resolver
  -> backends
```

The current interpreter, transpiler, and WebSocket renderer are valuable
backends. They are not the language foundation.

## Product Identity

JTML is for apps that read like documents but behave like reactive software.

It should win on low boilerplate, visible state, native app primitives, simple
styling, built-in fetch/route/form/action semantics, AI-friendly generation,
explainable dependency graphs, and clean escape hatches to HTML, CSS,
JavaScript, and browser APIs.

The honest claim is:

> JTML makes the common UI path simpler while preserving access to the full web
> platform when needed.

## Architectural Priority

### P0: Semantic Correctness

- Typed AST.
- Attribute classifier. First implementation is in place.
- Literal vs reactive attribute separation. First implementation is in place
  for transpiler, interpreter, and `jtml explain`.
- Better interpolation semantics.
- Better source spans.
- Stable Friendly-to-IR lowering.
- No runtime guessing of language meaning.

Success criteria:

- `style "..."` does not become `0.style`.
- `class "card"` does not become reactive state.
- SVG, `aria-*`, and `data-*` attributes survive as platform attributes.
- Quoted strings with spaces survive.
- Component params preserve source meaning.

### P1: Observable Graph

Build an explicit graph of:

- state nodes;
- derived nodes;
- action nodes;
- fetch nodes;
- route nodes;
- effect nodes;
- import/module nodes;
- output binding nodes;
- component call and instance nodes.

Current first slice:

- `jtml::analyzeSemanticProgram` extracts semantic node families from the AST
  plus Friendly source fallbacks for high-level constructs erased by lowering.
- `jtml explain --json` reports `semantic.nodes`, `semantic.attributes`,
  `semantic.dependencies`, `semantic.routeRecords`, `semantic.fetchRecords`,
  `semantic.componentDefinitions`, `semantic.componentInstances`, and top-level semantic
  lists such as `imports`.
- `jtml::analyzeSemanticUsage` derives observable state, bound actions, dead
  state, unused derived values, and unbound actions from the semantic graph so
  lint/explain no longer rely on CLI source-token scans.
- Dependency edges currently cover derived expressions, action reads/writes,
  effect subscriptions, route-to-component rendering, route fetch loading,
  component definitions/instances, module imports, conditions, loops, UI
  output, reactive attributes, and event triggers.
- Browser-local manifests and the live interpreter now both consume semantic
  route/fetch/component records first, keeping lowered DOM marker scans as
  compatibility fallbacks rather than primary ownership.
- The public keyword surface has a shared catalog exposed by
  `jtml keywords --json`, consumed by docs, editor grammar checks, Studio
  highlighting, and LSP completions so the mini-reference is tool-owned rather
  than prose-owned.

Success criteria:

```sh
jtml explain app.jtml
```

reports useful state, action, fetch, route, import, component, and warning
information.

### P2: Component Instance Model

Move beyond source expansion as semantic truth:

- component definitions;
- instances;
- props;
- slots;
- local scopes;
- emitted events;
- local state;
- instance-level diagnostics.

Expansion can remain an execution lowering temporarily, but the semantic graph
must understand real components. Current progress: semantic component
definition/instance records are available to `jtml explain`, browser-local
manifests, and live runtime registration. Component definitions now expose
params, local state, local derived values, local actions, local effects, event
bindings, slot presence, body/root-template counts, source line, encoded body
payload, and a semantic `runtimePlan`. Component instances expose their owning
runtime environment and validate local action dispatch through that definition.
The first browser-local direct body-plan execution slice can now render common
component templates, initialize local state, recompute derived values, render
simple `if`, `else`, `for`, and guarded action-body `while` nodes, run simple
local assignment actions plus action-local `let` / `get` declaration nodes,
carry authored slot plans, render slots and nested component calls, preserve
common literal attributes and semantic UI modifiers, pass simple action
arguments, and re-render the owning instance. Nested browser-local component
calls also get addressable runtime identities for local action dispatch. The
live interpreter now executes simple component body-plan assignment actions,
action-local `let` / `get` declarations, including compound `+=`, `-=`, `*=`,
`/=`, and `%=`,
exposes `renderedHtml` for supported body-plan templates through `/api/state`
and `/api/rendered-components`, injects supported component wrappers from
body-plan HTML into the initially served document, skips the first redundant
client patch, preserves direct component action metadata on rendered buttons,
bridges those actions back through `/api/component-action`, marks
unsupported/empty renders explicitly with `renderedHtmlSupported: false`, and
carries module-aware component identity through runtime
definitions/instances before falling back to name-only compatibility lookup.
Component body-plan nodes now also carry `definitionModule`, so imported
nested component calls inside encoded component bodies can target the module
that exported the component instead of falling back to global name lookup.
Browser-local and live body-plan renderers also preserve a broader first-slice
platform attribute surface for forms, media, SVG, `aria-*`, `data-*`, quoted
space-containing values, and Friendly aliases such as `link`, `navlink`,
`image`, `file`, `dropzone`, and `checkbox`. The live body-plan renderer now
also retains dynamic nested component instances for supported nested calls,
routes nested local actions through `/api/component-action`, evaluates nested
params/state as real values instead of display text, and uses loop-aware nested
identity so repeated children rendered from `for` keep separate local state.
After each supported top-level live or browser-local body-plan render, stale
dynamic nested descendants that were not rendered in the current pass are
pruned, so removed loop/branch children can no longer receive stale
component-action dispatches.
Browser-local action argument parsing now uses the same top-level parser for
quoted strings with spaces.
Nested component calls now have a first emitted-event bridge and explicit
contract metadata: `make Child emits picked` exposes emitted events through
semantic/runtime JSON, and `make Child emits picked(item)` also exposes
`emitArity` plus `emitPayloads` metadata. `make Child emits picked(item: string)`
also exposes `emitPayloadTypes`, and browser-local/live direct dispatch enforce
simple payload types before forwarding. `Child on picked choose("preset")`
maps child direct
actions such as `picked("Ada")` to parent body-plan actions in both
browser-local and live runtimes, forwarding preset args before emitted args.
When a component declares emits, `on event handler` names are validated against
that list; when emitted payload arity and the parent handler signature are both
known, arity is checked during Friendly lowering.
Action body lines such as `incBy(2)` and `picked("Ada Lovelace")` now become
body-plan `call` nodes, so browser-local and live direct execution can compose
component actions or emit declared component events without going through the
expanded compatibility DOM.
Body-plan nodes now expose `reads` and `writes` metadata. Real expression
fragments are parsed and walked as expression ASTs, so member/subscript access
like `users.data[activeIndex].name` records the precise read path plus `users`
and `activeIndex` instead of guessed token names. That is not yet a generated JS
compiler, but it is the required update surface for the compiler-first
production browser target:
state changes should eventually touch only dependent text, attributes, derived
values, and keyed list regions. Member/subscript writes also record the owning
observable root for invalidation, and browser-local/live direct actions can now
mutate existing dict/object properties, create missing dict/object and
non-negative array path containers, and update array/dict subscript targets
without compatibility dispatch. Richer assignment targets remain planned. Live
body-plan `for` loops now share the browser value model for arrays, strings,
dict/object values, and scalar singletons in both rendering and action
execution. The same nodes now carry authored body source
lines, and unsupported direct component action fallback records preserve
component/action context, component definition line, body line, node text, and
fallback errors when compatibility also fails. Browser-local fallback telemetry
now carries the same authored action/source context on
`window.jtml.directComponentFallbacks`.
Browser-local direct component actions now consume this metadata for the first
fine-grained-update-adjacent optimization: when action writes do not intersect
the rendered-read dependency closure, the runtime skips a full component
rerender and exposes the decision as `window.jtml.directComponentLastAction`.
The next slices are also in place: simple affected leaf body-plan nodes are
marked with `data-jtml-direct-body-node`, and direct rendering records the
managed attributes for those leaves. The browser runtime now tries an
in-place text/attribute patch first, then replaces the leaf from the body plan,
then falls back to a full body-plan component rerender. Those simple leaf
updates are now compiled into a cached per-component update plan keyed by
module/name/body shape, so repeated component actions do not have to rediscover
the same patchable body-plan nodes. The cached entries now carry explicit
text/button/element patch operations, which keeps the hot action path closer to
the eventual generated JS update-function shape. These operations now carry
precompiled element-part and click-invocation shapes, leaving runtime evaluation
for only the dynamic values. Cached plans now own generated browser
update-function source plus an indexed executable update function keyed by
rendered reads, matching the dependency-routed call boundary that the
compiler-first browser target can later emit as static JS. Dynamic execution of
that generated source is disabled by default for CSP-safe browser builds and is
only an explicit development bridge; a conservative interpreted updater remains
the default runtime fallback. If the operation plan cannot patch safely, the runtime records
source-line fallback telemetry before using a full body-plan rerender.
Structured container elements with children now have a conservative compiled
attribute-only patch operation, so changing a parent `class`, semantic modifier,
or platform attribute does not force child DOM replacement when the children
themselves are unaffected.
Control-flow regions are also anchored now: `if` and `for` body-plan nodes can
compile to direct region-replacement operations instead of forcing a whole
component rerender when the condition or collection changes. Unsafe cases such
as render-time `while`, richer keyed list lifecycle beyond current key
reporting/pruning, or complex unsupported child structure fall back to full body-plan
rerender or explicit compatibility dispatch before any silent direct execution
is allowed.
The older recursive heuristic patch fallback has been removed from the
browser-local direct component path, so unsupported updates have a simpler and
more auditable sequence: compiled patch operation, body-plan node replacement,
then full direct component rerender.
Direct `for` regions now emit per-item key/index markers, so the next keyed
DOM lifecycle slice has stable identity hooks for insert, reorder, and delete
operations instead of opaque list HTML. The runtime also records first-slice
list lifecycle telemetry for inserted, removed, and moved keys; optimized
keyed DOM diffing is still a planned compiler/runtime slice. Compiled `for`
region operations now use those keys for a conservative patch path that reuses
and reorders item wrappers, updates item contents, reports retained/inserted/
removed/moved key sets, prunes removed nested dynamic children for keyed list
items, and fails closed to whole-region replacement when keys are duplicate or
unsafe.
Nested component call wrappers now carry stable body-node anchors and compile
to explicit nested-component patch operations, allowing direct replacement of a
composed child call when parent state changes affect it.
Slot insertion sites now carry stable direct `slot` region anchors and compile
as region patch operations, making slot updates visible to the same body-plan
patch contract as `if` and `for`.
Live body-plan rendering also exposes patch telemetry for patched/current,
unsupported, and missing component records, which gives Studio and parity tests
an explicit comparison surface while live compatibility DOM continues to shrink.

Performance note: `/api/rendered-components` and live HTML patches are the
right path for Studio, dev preview, internal live apps, debugging, and
server-owned UI. They are not the benchmark path. Public/performance builds
should move toward `jtml build --target browser --prod`: static assets,
generated component functions, precompiled expressions, keyed DOM diffing,
tiny runtime helpers, and no live patch endpoint requirement. Browser builds
now emit a first CSP-safe `jtml-update-plans.js` static seed with component
body-plan read/write metadata, precomputed read indexes, unsafe-entry lists,
and first-slice text/region/nested patch operations. The browser runtime
prefers those static plans when available and compiles equivalent plan indexes
at runtime only as a bridge/fallback. The asset is a bridge toward generated
modules, not the final optimized compiler target.

Remaining architectural work: broaden the supported body-plan subset until the
expanded compatibility DOM is needed only for explicit fallback cases or can be
removed from production browser builds entirely, keep extending
source-first diagnostics beyond current emitted-event payload messages and
module import issue spans, harden
rich attribute/modifier parity for advanced platform APIs, broaden
keyed/reordered collection lifecycle semantics beyond the first explicit `key`
tail slice, and keep expanding browser/live behavior parity checks beyond the
current component metadata, initial render, route-link, platform-attribute,
nested-instance, event, and action
bridge coverage.

### P3: Semantic Styling

JTML should write visual intent first, raw CSS only when needed.

Primary layers:

- built-in UI primitives: `app`, `shell`, `topbar`, `sidebar`, `content`,
  `panel`, `card`, `metric`, `grid`, `stack`, `cluster`, `tabs`, `modal`,
  `form`, `field`, `table`, `loading`, `error`, `empty`;
- utility modifiers: `gap`, `pad`, `radius`, `shadow`, `tone`, `surface`,
  `align`, `justify`, `cols`, `width`;
- theme tokens;
- `css raw` escape hatch.

### P4: Browser-Local Runtime

Production reactivity must not require WebSockets.

```sh
jtml build app.jtml --target browser --out dist
```

should emit deployable static files with browser-local state cells, derived
recomputation, DOM bindings, events, fetch state, and routes.

### P5: Live Backend On The Same Graph

`jtml serve app.jtml` should consume the same semantics as browser-local build,
with server-owned state and WebSocket/HTTP sync as an explicit backend.

### P6: Interop And Escape Hatches

Support raw HTML, raw CSS, `extern` JavaScript, custom elements, Canvas, SVG,
WebGL, browser APIs, npm/browser libraries, and custom event bridging.

### P7: Export Targets

Backends should include static HTML, browser JS runtime, custom element, plain
JS module, React component, and Vue component exports.

### P8: Product Hardening

Docs, tutorials, examples, component kit, LSP, test runner, CI, security review,
deployment guide, performance benchmarks, and accessibility checks.

## Core Rule

```text
observable graph first, runtime second
semantic UI first, raw platform escape hatches second
browser-local production by default, live WebSocket mode when chosen
```
