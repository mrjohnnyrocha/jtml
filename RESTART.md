# Restarting JTML

If you are returning to JTML after months or years, do not start coding immediately.

The repository remains a research toolchain with a verified development baseline
in `CURRENT_STATE.md`. First rebuild the mental model, reproduce current health,
then choose one small task.

## Read First

1. `CURRENT_STATE.md`
2. `ARCHITECTURE.md`
3. `README.md`
4. `ROADMAP.md`
5. `docs/README.md`
6. `docs/roadmaps/next-priorities.md`
7. `docs/architecture/language-family-design.md`
8. `docs/reference/language-reference.md`
9. `AGENTS.md`

## Mental Model

1. Friendly JTML 2 is the canonical source language.
2. Classic JTML is compatibility/lowering/backend infrastructure.
3. JTL is future/experimental, not a mature separate language yet.
4. The intended pipeline is Friendly source -> AST / Expression IR -> Semantic
   IR -> Runtime Plan -> backends.
5. Browser production mode is the important long-term public deployment path.
6. Live/WebSocket mode is for Studio, tutorial, local serve, and server-owned
   UI, not the definition of reactivity.
7. The legacy `Environment` runtime still exists and should not be deleted
   casually.
8. Semantic analysis should answer tooling questions before runtime backends.
9. Runtime-plan/body-plan execution is the migration bridge from interpreter
   semantics to generated component execution.
10. Static component modules are the current compiler-first browser bridge.
11. Operation/helper fallbacks are transitional.
12. Source-first diagnostics and TypeEnvironment are not finished.
13. Do not add JTL backend/API syntax before JTML's compiler/runtime path is
   boringly stable.

## Build

```sh
cmake -S . -B build -DJTML_BUILD_TESTS=ON -DJTML_BUILD_PYTHON=OFF
cmake --build build --parallel
```

## Test

```sh
ctest --test-dir build --output-on-failure
scripts/verify_all.sh
./build/jtml doctor --json
```

## Canonical Run/Demo

```sh
./build/jtml --version
./build/jtml doctor
./build/jtml demo --port 8000
./build/jtml serve examples/friendly_counter.jtml --port 8001 --watch
./build/jtml build demos/index.jtml --target browser --out dist/restart-demo
```

## Source Map

| Area | Files |
| --- | --- |
| Friendly frontend | `src/friendly.cpp`, `include/jtml/friendly.h` |
| AST/parser | `include/jtml/ast.h`, `include/jtml/parser.h`, `src/parser.cpp` |
| Expression IR | `include/jtml/expression_ir.h`, `src/expression_ir.cpp` |
| Semantic layer | `include/jtml/semantic.h`, `src/semantic.cpp` |
| Module graph | `include/jtml/semantic/module_graph.h`, `src/jtml/semantic/module_graph.cpp` |
| Runtime plan | `include/jtml/runtime_plan.h`, `src/runtime_plan.cpp`, `src/runtime_plan_json.cpp` |
| Static component emitter | `include/jtml/runtime/static_update_plan_emitter.h`, `src/jtml/runtime/static_update_plan_emitter.cpp` |
| Browser runtime assets | `src/browser_runtime_assets.cpp`, `src/browser_runtime_emitter.cpp`, `src/client_manifest_emitter.cpp` |
| Browser build service | `cli/browser_build.cpp`, `cli/browser_build_policy.cpp`, `cli/browser_build_assets.cpp`, `cli/browser_build_schema.cpp` |
| Live interpreter | `src/interpreter.cpp`, `include/jtml/interpreter.h` |
| Legacy runtime | `src/jtml/legacy_runtime/`, `include/jtml/environment.h`, `include/jtml/value.h` |
| CLI | `cli/main.cpp`, `cli/cmd_*.cpp`, `cli/util.cpp` |

## Historical Traps

- Classic vs Friendly: do not teach Classic as the default source language.
- JTL vision vs implementation: `jtl 1` exists, but full JTL does not.
- Multiple reactivity models coexist because the project is mid-transition.
- Fallback/helper paths are migration bridges, not automatically the desired
  architecture.
- Some docs and examples can drift; verify with code, tests, and `doctor`.
- `demos/opspulse/index.jtml` is the canonical single-file showcase. Its sibling
  modules are not its dependency graph; consult `docs/application-inventory.md`
  before treating gate success as browser application acceptance.
- Versioning must remain consistent across CMake, CLI, C API, docs, and
  `doctor`.

## Do Not Redesign Immediately

Large files and overlapping runtime systems are frustrating, but they represent
real transitional ownership. Do not split or rewrite the parser, interpreter,
browser runtime, or Friendly lowering before proving which tests and backends
depend on each behavior.

## First Recommended Task

After reproducing the freeze health, the first substantive restart task should
be end-to-end source-span propagation from original Friendly source through
AST, Expression IR, Semantic IR, Runtime Plan, browser production diagnostics,
and live/runtime errors.

This should precede major TypeEnvironment expansion and deeper compatibility
fallback removal because source-first diagnostics are the project’s most
important AI-native safety contract.

