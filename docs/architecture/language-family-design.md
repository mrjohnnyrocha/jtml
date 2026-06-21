# JTML / JTL Language Family Design

Status: design authority for how JTML and the planned JTL core language should
evolve together. Keep implementation checklists in `docs/roadmaps/`; keep this
file focused on durable language shape.

## Thesis

JTML should not become "another frontend framework." It should be the web/app
dialect of a small observable-first language family:

```text
JTL core language
  + JTML web/app dialect
  -> typed AST
  -> semantic IR
  -> observable graph
  -> backends
```

JTML's product identity remains:

```text
apps that read like documents but behave like reactive software
```

JTL is the planned core logic dialect: Python-like productivity, indentation,
low boilerplate, readable data and control flow, strong tooling, and explicit
semantic graphs. JTL should grow only where it strengthens JTML's architecture
or opens clear non-web use cases; it should not distract from making JTML
excellent as a web/app language.

## Dialects

| Dialect | Header | File | Purpose | Current status |
| --- | --- | --- | --- | --- |
| JTML web/app | `jtml 2` | `.jtml` | UI, pages, components, routes, fetch, semantic UI, media, browser/runtime tooling | Canonical public authoring dialect |
| JTL core | `jtl 1` | `.jtl` | Logic-first experiments, future functions/tests/modules/general computation | Experimental header; currently lowers through Friendly JTML pipeline |
| Compatibility backend | none / Classic syntax | `.jtml` | Migration, generated artifacts, embedding, regression tests | Supported backend layer, not preferred authoring style |

The user-facing rule:

- use `jtml 2` for apps and UI;
- use `jtl 1` only for core-language experiments;
- inspect compatibility output when debugging compilers or migrations, but do
  not teach it as the default language.

## Maturity Contract

The language family is not enterprise-ready yet. It is enterprise-relevant:
the architecture has a credible path toward larger teams, stronger tooling,
and multiple backends, but the implementation still contains compatibility
bridges and monolithic source files that must be reduced before promising
large-team stability.

Use these labels consistently:

| Label | Meaning |
| --- | --- |
| stable | Implemented, tested, documented, and expected to remain compatible within JTML 2 |
| first slice | Implemented and valuable, but still being hardened or expanded |
| experimental | Available for exploration or tooling contracts, but not a production promise |
| planned | Designed direction with no compatibility commitment yet |

Current examples:

- stable: Friendly JTML 2 basics, Classic compatibility backend, CLI
  check/lint/fmt/test/build/serve, keyword and semantic UI catalogs;
- first slice: fetch cache/invalidation, route guards/layouts, semantic UI,
  media/charts/scene3d, Studio as local hub;
- experimental: `jtl 1`, component `runtimePlan` bridge before direct
  non-expanded template execution, advanced browser runtime parity,
  framework/export boundaries.

`jtml doctor --json` mirrors this contract for tools and CI.

## Compilation Model

Python is parsed and executed by a bytecode interpreter. It is productive, but
pure Python loops can be slow; the ecosystem wins by calling optimized
C/C++/Fortran/CUDA libraries.

JTL/JTML should not promise magic speed. Its advantage should be semantic
ownership:

```text
source
  -> typed AST
  -> semantic IR
  -> observable/dataflow graph
  -> backend emitter/runtime
```

Because the compiler understands state, derived values, actions, effects,
fetches, routes, components, modules, UI primitives, and outputs, tooling can
explain and transform programs more reliably than source-token scanners or
runtime guesses.

Primary backend targets:

- static HTML plus browser-local JavaScript runtime;
- live server/WebSocket runtime;
- custom elements;
- framework exports;
- C API/native embedding;
- future WASM/native execution for core JTL;
- Python/JavaScript/native interop.

WebSocket is a backend, not the definition of reactivity.

## Contract-First Backend Operations (Planned)

JTML should remain the production-facing web/app dialect. A future JTL backend
track should target contract-first governed operations for internal tools:
approval consoles, incident consoles, AI-governance controls, cost-control
panels, deployment controls, and other apps where UI actions call real backend
systems with audit and policy requirements.

The planned sequence is:

1. `type` and `error` declarations for request/response contracts.
2. API/operation signatures that can generate OpenAPI.
3. Policy and validation hooks for governed actions.
4. Runtime adapters for HTTP handlers and host-language integration.
5. Typed JTML integration so `fetch` and a future `call` primitive consume the
   same contracts.
6. Enterprise hardening: auth/session boundaries, audit logs, rate limits,
   observability, test fixtures, and deployment guidance.

This is design direction only. The `api`, `operation`, `policy`, and `call`
syntax is not implemented yet; it should wait until browser-local runtime,
module identity, and component-instance semantics are safer.

## Core Semantics

The shared language family should keep these concepts stable:

| Concept | Meaning |
| --- | --- |
| `let` | mutable observable state |
| `const` | immutable value |
| `get` | derived value with dependencies |
| `when` | observable action, often triggered by UI/events |
| `effect` | reactive side effect tied to state changes |
| `use` / `export` | module boundary |
| optional types | tooling and lint surface, erased where a backend does not need them |

Planned JTL-only or JTL-first concepts:

```jtl
jtl 1

fn sum values
  let total = 0
  for value in values
    total += value
  return total

test "sum works"
  expect sum([1, 2, 3]) == 6
```

Design distinctions:

- `fn` is an ordinary reusable function.
- `when` is an observable action with runtime identity and side effects.
- `get` is a dependency-tracked derived value.
- `effect` reacts to dependency changes.
- `test` and `expect` are executable specs for tooling and CI.

This distinction matters because enterprise tooling needs to know whether a
block is pure-ish computation, UI-triggered mutation, derived data, or a side
effect.

## JTML Web/App Surface

JTML adds web-specific semantics on top of the shared core:

- `page`, `make`, `slot`;
- `fetch`, `refresh`, `invalidate`;
- `route`, `layout`, `guard`, `redirect`, `navlink`;
- forms and browser events;
- semantic UI primitives such as `shell`, `panel`, `grid`, `metric`, `card`;
- `theme`, scoped `style`, `css raw`, `html raw`;
- media and graphics primitives;
- `extern` host actions and future browser/platform bridges.

The preferred style direction is semantic intent first:

```jtml
jtml 2

theme
  color primary "#2563eb"
  space md 12
  radius md 12

page
  shell
    sidebar
      navlink "Dashboard" to "/"
    content
      panel title "Usage" pad lg shadow md
        grid cols 3 gap md
          metric "Users" users.total "Active" tone good
```

Raw CSS, raw HTML, custom elements, Canvas, WebGL, and JavaScript libraries must
remain available through explicit escape hatches. JTML should be a simpler
authoring layer over the full web platform, not a smaller prison.

## Interop Strategy

Machine learning, scientific computing, database clients, cloud SDKs, and
specialized platform APIs should be interop-first.

Early direction:

```jtl
jtl 1

extern predict from python "./model.py:predict"
let result = predict(features)
```

Browser/server model inference:

```jtl
jtl 1

extern runModel from "host.onnx.run"
let output = runModel(input)
```

Native tensor types, kernels, and training loops are not first-era goals. They
only make sense after the language has stable modules, optional types, package
boundaries, interop, benchmarks, and clear execution targets.

## Enterprise Requirements

Large-team viability requires:

- stable typed AST and semantic IR;
- source spans and source-first diagnostics;
- observable usage analysis;
- module graph and package manager;
- optional types and static linting;
- formatter, fixer, refactorer, explain, and test tooling;
- LSP and editor integrations;
- browser-local production runtime;
- live runtime on the same graph;
- security model for raw HTML/CSS/externs;
- package, deployment, performance, and observability docs;
- regression tests for docs, examples, Studio, and runtime parity.

## Implementation Order

Implementation belongs in roadmap files, but the design dependency order is:

1. Keep `jtml 2` excellent as the public web/app dialect.
2. Keep `jtl 1` as an experimental header backed by the same semantic core.
3. Continue replacing source scans and compatibility-marker ownership with
   typed AST and semantic IR.
4. Harden browser-local runtime and live runtime parity.
5. Add direct component-instance execution.
6. Add `fn`, `test`, and `expect` only when they can be represented in the
   shared semantic graph.
7. Design contract-first JTL backend APIs only as semantic contracts first,
   not as ad-hoc runtime syntax.
8. Grow interop after runtime contracts are stable.

The strategic guardrail:

```text
JTML first, semantic core always, JTL only when it strengthens the family.
```
