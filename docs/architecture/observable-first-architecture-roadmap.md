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
The remaining architectural step is direct non-expanded component template
execution.

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
