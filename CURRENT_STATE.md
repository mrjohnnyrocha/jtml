# JTML Current State

Status: HIBERNATED / RESEARCH

Development is intentionally paused at a restartable research state. The
project is not abandoned, but future work should begin by reading this snapshot
instead of reconstructing the architecture from commit history or scattered
roadmaps.

## Project Status

JTML is a C++17 language/toolchain for Friendly JTML 2, the primary web/app
authoring dialect. Classic JTML remains supported as compatibility, migration,
embedding, and lowered-artifact infrastructure. JTL is a design/experimental
future core/backend direction, not a mature standalone implementation.

The strongest current product thesis is:

```text
JTML is a semantic, observable-first UI language for apps that read like
documents but behave like reactive software.
```

The broader differentiation is:

```text
AI-friendly source -> compiler-owned semantic graph -> explainable, safe,
deployable web apps.
```

Do not read the current state as production or enterprise readiness. It is an
advanced research/tooling platform with meaningful first slices.

## Current Architecture

```text
Friendly JTML 2 source
    |
    v
Friendly lowering + parser/AST + Expression IR
    |
    v
Semantic Program / Semantic Project
    |
    v
Runtime Plan / Runtime Project Plan
    |
    +--> browser production backend
    +--> live interpreter / WebSocket backend
    +--> tooling / explain / lint / doctor
    +--> Classic compatibility / embedding backend
```

Primary source locations:

| Area | Location |
| --- | --- |
| Friendly frontend | `src/friendly.cpp`, `include/jtml/friendly.h` |
| AST/parser | `include/jtml/ast.h`, `include/jtml/parser.h`, `src/parser.cpp` |
| Expression IR | `include/jtml/expression_ir.h`, `src/expression_ir.cpp` |
| Semantic IR | `include/jtml/semantic.h`, `src/semantic.cpp` |
| Module graph | `include/jtml/semantic/module_graph.h`, `src/jtml/semantic/module_graph.cpp` |
| Runtime plan | `include/jtml/runtime_plan.h`, `src/runtime_plan.cpp`, `src/runtime_plan_json.cpp` |
| Browser backend | `src/browser_runtime_assets.cpp`, `src/browser_runtime_emitter.cpp`, `src/client_manifest_emitter.cpp` |
| Static component emitter | `src/jtml/runtime/static_update_plan_emitter.cpp` |
| Browser build CLI service | `cli/browser_build.cpp`, `cli/browser_build_policy.cpp`, `cli/browser_build_assets.cpp`, `cli/browser_build_schema.cpp` |
| Live interpreter | `src/interpreter.cpp`, `include/jtml/interpreter.h` |
| Legacy runtime | `src/jtml/legacy_runtime/`, `include/jtml/environment.h`, `include/jtml/value.h` |
| CLI | `cli/main.cpp`, `cli/cmd_*.cpp` |

## [IMPLEMENTED]

- Friendly JTML 2 source dialect for common page, state, action, component,
  route, fetch, style, semantic UI, media, and event syntax.
- Classic compatibility parsing/lowering/runtime paths for existing files,
  migration, C API, regression tests, and debugging lowered artifacts.
- CMake C++17 build with syntax, semantic, browser-production, live-runtime,
  legacy-runtime, tooling, CLI, C API, test, and package targets.
- CLI commands including `check`, `lint`, `fmt`, `fix`, `build`, `serve`,
  `studio`, `tutorial`, `test`, `explain`, `doctor`, `keywords`, `ui`, `lsp`,
  `new`, `add`, `install`, and `dev`.
- Parser/AST source spans as a first slice.
- Expression IR with dependency extraction, simple value-type propagation, and
  CSP-safe static JS expression emission for supported shapes.
- Semantic analysis, semantic usage report, module graph, relative import
  resolution, named exports, re-exports, and semantic project records.
- Browser build mode split: clean production by default, explicit `--compat`
  and `--debug` modes, split browser assets, schema checks, and no shipped
  runtime `eval` / `new Function`.
- Live HTTP/WebSocket runtime endpoints for Studio, tutorial, serve, and
  external tooling.
- Studio/tutorial content and VS Code extension first slices.

## [PARTIAL]

- Fetch lifecycle, retry/cache/invalidation, and browser-local data behavior.
- Routes, params, guards, layouts, and browser/live parity.
- Stores and effects as observable app primitives.
- Semantic UI/theme, modifiers, accessibility defaults, and generated CSS.
- Media, charts, SVG, canvas, and 3D mount points.
- Component body-plan execution.
- Static component modules and direct create/update/destroy paths.
- Expression `TypeEnvironment`; it is not a full typechecker.
- Source-first diagnostics; spans are not yet end-to-end for every lowering and
  runtime/compiler error.
- Browser/live parity across slots, nested components, events, forms, fetch,
  routes, media, and keyed lists.
- Forms and `into` binding semantics.
- Browser benchmarks and lifecycle/focus/keyed-list proof fixtures.

## [EXPERIMENTAL]

- `jtl 1` files through the Friendly pipeline.
- First-slice browser-local component body-plan execution.
- Package/local registry commands and remote-registry direction.
- Framework/custom-element export boundaries.
- Advanced media/graphics/3D interfaces.

## [PLANNED]

- Mature standalone JTL parser.
- Mature JTL type system.
- JTL standard library.
- JTL backend runtime.
- Contract-first backend/API system with operation contracts, policies,
  OpenAPI generation, adapters, and typed JTML integration.
- Full compiler-owned typed semantic IR.
- Full browser production compiler with generated component create/update
  functions for common app shapes.
- Enterprise-grade CI, security, performance, release, and deployment gates.

## Known Architecture Transition

Multiple reactivity/execution models coexist today:

1. Legacy `Environment` dependency graph for the live interpreter and
   compatibility runtime.
2. Semantic dependency/usage analysis for lint/explain/tooling.
3. Runtime-plan reads/writes for body-plan execution.
4. Browser/static component scopes for the compiler-first production path.

The intended dominant direction is compiler-owned semantics:

```text
Friendly source -> AST / Expression IR -> Semantic IR -> Runtime Plan ->
static/browser component execution
```

Do not try to resolve this migration during freeze maintenance. Future work
should continue moving common production paths toward static/browser execution
and leave compatibility/live paths as explicit backends.

## Build/Test Status

Last freeze verification: 2026-08-23.

Results:

```text
cmake configure: passed
cmake build: passed
ctest: passed, 332/332 tests
scripts/verify_all.sh: passed
browser runtime benchmark: passed with local Google Chrome
doctor: ok, version 0.1.0-dev, enterpriseReady false
```

Required commands:

```sh
cmake -S . -B build -DJTML_BUILD_TESTS=ON -DJTML_BUILD_PYTHON=OFF
cmake --build build --parallel
ctest --test-dir build --output-on-failure
scripts/verify_all.sh
./build/jtml doctor --json
```

Update this section after every release-equivalent freeze check.

## Canonical Examples

Best large demo:

- `demos/index.jtml` - broad single-file demo covering app-scale JTML concepts.

Focused examples:

- `examples/friendly_counter.jtml`
- `examples/friendly_semantic_ui.jtml`
- `examples/friendly_fetch.jtml`
- `examples/friendly_routes.jtml`
- `examples/friendly_component_isolation.jtml`

Do not currently treat these as canonical production evidence:

- `demos/opspulse/` - active uncommitted demo work at freeze time. It contains
  substantial edits and should be resolved or quarantined before being used as
  a stable modular-app reference.
- Older Classic examples - useful for compatibility regression, not public
  authoring guidance.
