# AGENTS.md — JTML repository

## Build & test

```sh
cmake -S . -B build -DJTML_BUILD_TESTS=ON -DJTML_BUILD_PYTHON=OFF
cmake --build build --parallel
ctest --test-dir build --output-on-failure   # GoogleTest, auto-discovered
```

`scripts/verify_all.sh` is the full predeploy gate (cmake+build+tests+CLI smoke+LSP+package).

Dependencies: WebSocket++ and Boost.System via Homebrew (`/opt/homebrew`). Vendored single-header deps under `third_party/` (nlohmann/json, cpp-httplib).

## CLI commands

Binary: `build/jtml`. Key subcommands: `check`, `fix`, `fmt`, `lint`, `transpile`, `build`, `export`, `serve`, `studio`, `tutorial`, `demo`, `test`, `explain`, `refactor`, `doctor`, `lsp`, `new`, `add`, `install`, `dev`. Use `--json` flag for machine-readable output. `explain --json` is the main semantic introspection endpoint.

## Repo structure

```
include/jtml/   public headers (mirror src/ layout)
src/            implementation
cli/            jtml binary, one .cpp per command group
tests/          CTest/GoogleTest suite
examples/       runnable .jtml files
tutorial/       lesson content for jtml tutorial
studio/         externalized content (samples/, reference/, sidebar/)
docs/           language + tooling docs
third_party/    vendored headers (nlohmann/json, cpp-httplib)
cmake/          CMake helpers (JtmlVersion)
editors/vscode/ VS Code extension
```

## Code conventions

- C++17, public headers in `include/jtml/`, `.cpp` files in `src/` — snake_case names matching headers.
- Friendly JTML 2 is the canonical source dialect; Classic (`define`, `\\`) is compatibility/lowering only.
- CMake source groups by ownership: `JTML_SYNTAX_SOURCES`, `JTML_SEMANTIC_SOURCES`, `JTML_RUNTIME_SOURCES`, `JTML_TOOLING_SOURCES` in `CMakeLists.txt:43-88`.
- CLI: `cli/main.cpp` parses args + dispatches; each subcommand handler in `cli/cmd_*.cpp`.
- Module ownership boundaries: `include/jtml/runtime/`, `include/jtml/semantic/`, `include/jtml/syntax/`, `include/jtml/emit/`, `include/jtml/tooling/`, `include/jtml/interop/` exist for incremental modularization.

## Architecture direction

The durable pipeline is:

```text
Friendly JTML -> typed AST -> semantic IR -> observable graph -> runtime plan -> backends
```

Current priority order:

1. Observable graph and runtime-plan correctness.
2. Browser-local production runtime without WebSocket dependency.
3. Direct body-plan component execution.
4. Static browser component create/update modules.
5. Live runtime parity over the same body-plan contract.
6. Compatibility DOM as explicit fallback only.
7. Interop/export/JTL backend API work after the frontend architecture is stable.

Do not make Classic the public design center. Classic remains required for
compatibility, migration, C API behavior, and regression tests, but new
architecture should flow from Friendly/semantic/runtime-plan ownership.

Do not add new CLI-level source scanners for semantic questions. Prefer typed
AST, semantic IR, `SemanticProject`, or `RuntimePlan`.

Do not introduce runtime `eval` / `new Function` as the production path.
Dynamic generated update functions are an explicit development bridge only.
Production browser builds should prefer CSP-safe external assets such as
`components/index.js`, `app.js`, and the legacy compatibility alias
`jtml-update-plans.js`.

## Current runtime compiler bridge

`jtml build --target browser --out dist` emits:

```text
dist/index.html
dist/jtml-runtime.js
dist/components/index.js
dist/app.js
dist/jtml-update-plans.js
```

`components/index.js` is the current compiler-first component bridge.
`jtml-update-plans.js` remains a legacy alias for older tooling while the
runtime split is completed. The component module exposes:

- `window.__jtml_static_update_plans`
- `window.__jtml_static_update_functions`
- `window.__jtml_static_component_modules`

The runtime should prefer static component create/update modules first, then
interpreted static plans, then runtime plan compilation. Compatibility DOM is
only for unsupported shapes and migration paths.

Current static component modules emit direct escaped HTML create functions for
supported root text, button, leaf element, and direct-safe container nodes;
region, nested-component, control-flow, slot, and unsupported shapes still use
helper/fallback create paths.
Static update functions now try generated per-node patch cases that patch
direct text, button, element, and region shapes before falling back to operation
records. The older `rootCreateOperations` and per-entry operation payloads
remain as fallback/debug metadata while the compiler path expands. The next
compiler step has started: expression plans for literals, booleans, numbers,
null, simple dot paths, unary `!`, binary/comparison/logical operators, and
ternary conditionals now flow through text/element operations, element content,
attributes, modifiers, component action arguments, body-plan conditions, `for`
collection/key expressions, and nested component parameters.
Static modules also emit direct JS expression functions for simple and
first-slice composite plans, and simple generated text/button/leaf-element/container
create functions that do not require runtime helper availability unless they
hit a fallback shape, plus generated text, element attribute/content, and button
label/action-arg patches update DOM directly without a global runtime-helper
requirement. Runtime-plan read analysis also treats
value-taking attributes such as `title selected` as reads of `selected`, even
when the value token is also a valid boolean attribute name. Continue with
richer action-body expression precompilation, stronger keyed reconciliation,
and broader direct DOM generation.

For `jtml build --target browser`, `jtml-runtime.js` is now the primary runtime
asset. Inline browser runtime remains for `transpile --target browser`, live
serve/studio paths, and compatibility while those flows get their own asset
mode.

Remaining high-value runtime work:

- Broaden static component modules into fuller generated update JS with fewer
  operation-record fallbacks.
- Precompile richer action bodies. Literal/simple-path and first-slice
  composite expression plans already cover component module `expressionPlan`,
  `contentPlan`, `exprPlan`, `argPlans`, body-plan conditions, loop
  collection/key plans, and nested params.
- Strengthen keyed list lifecycle and diagnostics.
- Add browser/live parity checks for slots, events, routes, fetch, media, and
  keyed lists.
- Remove embedded runtime duplication once `jtml-runtime.js` is the primary
  production runtime asset.
- Add benchmark fixtures and enforce performance budgets.

## Test quirks

- Single `jtml_tests` binary (links `jtml_c`, `jtml_cli_lib`, GTest). FetchContent downloads GTest at configure time.
- Test fixtures in `tests/fixtures/`.
- `JTML_SOURCE_DIR` compile definition available for tests needing repo root paths.
- `gtest_discover_tests` auto-enumerates test cases; use `ctest -R <filter>` for focused runs.

## Performance checks

Use `scripts/benchmark_runtime.sh` for first-slice runtime/compiler budgets.
It builds browser targets from `tests/fixtures/performance/`, verifies that
the split browser assets are emitted, and fails if generated assets exceed the
current size budgets. Defaults are intentionally conservative while the runtime
is still being split: 50 KB for `index.html`, 260 KB for
`jtml-runtime.js`, 180 KB for `components/index.js`, 20 KB for `app.js`, and
180 KB for the legacy `jtml-update-plans.js`. Treat these budgets as
guardrails, not final benchmarks; tighten runtime/component budgets as direct
generated modules replace body-plan helpers.

## VS Code extension

`editors/vscode/` ships syntax highlighting, LSP client, and CLI fallback. Config keys: `jtml.executablePath`, `jtml.diagnostics.enabled`, `jtml.languageServer.enabled`. Prefers native `jtml lsp` when available.

## Important notes

- `jtml fix` only applies safe repairs (missing header, tabs, trailing whitespace, final newline). `jtml fmt` is idempotent source-preserving.
- `jtml lint` exits non-zero on errors (undefined vars, unreachable code, type mismatches, action arity).
- `jtml build --target browser --out dist` for static deployment; `jtml serve --watch` for live dev loop.
- `jtml doctor --json` is the machine-readable readiness contract.
- `scripts/verify_all.sh` is the full local predeploy gate. Run it before
  claiming a roadmap slice complete when CMake, runtime assets, CLI behavior,
  examples, demos, packaging, or smoke behavior changed.
- No CI workflows checked in yet; `.github/` may be absent.
- `.claude/worktrees/` in gitignore — local tooling caches.
- `jtml test` smoke-tests every bundled example (parse + lint + transpile).
