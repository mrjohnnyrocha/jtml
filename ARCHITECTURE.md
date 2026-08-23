# JTML Architecture

This document is the compact restart map for why the repository looks the way
it does. For detailed product direction, read `ROADMAP.md` and
`docs/architecture/language-family-design.md`.

## Current Compiler/Runtime Direction

```text
Friendly source
    |
    v
AST / Expression IR
    |
    v
Semantic Program / Semantic Project
    |
    v
Runtime Plan / Runtime Project Plan
    |
    v
Static/browser execution
```

The repository is mid-transition from an interpreter-first implementation to a
compiler-owned semantic/runtime-plan architecture. The transition is valuable
and intentional; overlapping systems should not be removed without first
understanding which backend still depends on them.

## Compatibility Architecture

Classic JTML remains part of the project for compatibility, migration,
embedding, lowered artifact inspection, and regression tests. It is not the
preferred public source language for new apps.

The live interpreter and legacy runtime exist for `serve`, Studio, tutorial,
external runtime contracts, compatibility behavior, and debugging. The
`Environment`/`VarValue` runtime is useful but should not be mistaken for the
long-term browser production architecture.

Operation-record/helper fallbacks in the browser runtime and static component
emitter exist as migration/debug bridges while direct generated component
create/update/destroy paths expand. They should shrink over time, but not by
breaking unsupported shapes silently.

## Observable-First Semantics

Currently real:

- `let`, `get`, `when`, `effect`, `store`, `fetch`, `route`, components, and
  modules are recognized by Friendly syntax and/or semantic analysis.
- Semantic IR records state, derived values, actions, components, routes,
  fetches, stores, effects, imports/exports, UI primitives, modifiers, and raw
  escape hatches.
- Runtime-plan body nodes carry first-slice read/write/source metadata.
- Browser production builds use compact client manifests and static component
  modules for supported shapes.

Intended direction:

- Compiler-owned observable graph becomes the primary source of runtime truth.
- Browser-local production execution becomes independent of WebSockets.
- Live/runtime compatibility consumes the same semantics where supported.
- Generated source names should disappear from primary user diagnostics.

## Expression IR

Expression IR moves JTML away from source-string interpretation. It is built
from parser-owned expression nodes where possible and is used for:

- dependency analysis;
- value-type hints;
- runtime expression-plan JSON;
- static JS expression codegen;
- fallback evaluation.

It is not yet a full typed expression system. Object shapes, nullability,
route params, event payloads, slot payloads, and full operator validation are
future work.

## Semantic IR

The semantic layer currently owns:

- top-level and component-level state/action/derived/fetch/route/component
  records;
- usage analysis for dead state, unused derived values, and unbound actions;
- module graph records, import resolution, named exports, and re-exports;
- semantic UI/theme/modifier counts and diagnostics;
- raw CSS/HTML/extern escape-hatch reporting.

Semantic IR should be preferred over new CLI-level source scanners.

## Runtime Plan

Runtime Plan is the bridge from semantic ownership to executable backends. It
contains state, derived values, actions, fetches, routes, component
definitions, component instances, body-plan nodes, source metadata, and
expression plans.

Current limitation: some component body-plan data is still reconstructed from
lowered/encoded body source. Future work should carry richer parser/AST spans
and template structure directly into runtime plans.

## Browser Production Path

`jtml build --target browser` defaults to clean production output:

```text
dist/index.html
dist/jtml-runtime.js
dist/components/index.js
dist/app.js
```

`--compat` and `--debug` explicitly emit the legacy update-plan asset. Clean
production must remain CSP-safe: no runtime `eval`, no `new Function`, no
implicit compatibility asset, and no source-rich debug payloads in public
client manifests.

## Live Mode

Live mode is for Studio, tutorial, `serve`, internal/server-owned UI, and
runtime inspection. It is a backend, not the definition of JTML reactivity.
The goal is semantic parity where supported, not making WebSocket transport the
primary production model.

## Architectural Invariants

- Friendly JTML 2 is the public authoring center.
- Classic is compatibility infrastructure.
- JTL is design/experimental until JTML compiler/runtime is stable.
- Prefer compiler-owned AST, Expression IR, Semantic IR, Runtime Plan, or
  Semantic Project data over ad-hoc source scans.
- Browser production assets must remain CSP-safe.
- Do not introduce runtime `eval` or `new Function`.
- Keep compatibility paths explicit.
- Diagnostics should point to source file, line, column, component, semantic
  node, and remediation where possible.
- Browser/live semantic parity is a goal, but unsupported shapes should fail
  closed or use explicit fallback.

