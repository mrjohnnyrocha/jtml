# AGENTS.md — JTML repository

## Start here

Read `CURRENT_STATE.md` for verified health and blockers, `ARCHITECTURE.md` for
ownership and invariants, and `RESTART.md` for the restart checklist. Detailed
implementation history belongs in `docs/architecture/`, not this instruction file.
Do not treat research milestones or `doctor` success as production readiness.

## Architectural rules

- Friendly JTML 2 is canonical. Classic remains compatibility, migration,
  lowering, embedding/C API, and regression infrastructure. JTL is experimental.
- Preserve compiler ownership: Friendly -> AST / Expression IR -> Semantic IR /
  observable graph -> Runtime Plan -> backends. Prefer `SemanticProject` and
  `RuntimePlan` over new CLI source scanners for semantic questions.
- Prioritize observable/runtime-plan correctness and browser-local execution;
  live/WebSocket and legacy `Environment` are supported backends, not the public
  production architecture. Do not remove them without backend regression proof.
- Never ship runtime `eval` or `new Function`. Keep compatibility explicit and
  unsupported production shapes fail-closed with source-first remediation.
- Preserve original source file/line/column and component context in diagnostics.
  TypeEnvironment and source-span propagation remain incomplete.

## Browser contract

`jtml build <source> --target browser --out dist` defaults to clean production:
`index.html`, `jtml-runtime.js`, `components/index.js`, and `app.js`.

- Clean output must omit `jtml-update-plans.js`, compatibility asset references,
  synthetic imported-component stubs, legacy update-plan globals, source-rich
  debug payloads, and operation-record fallback metadata in component modules.
- `--compat` explicitly enables migration assets; `--debug` adds diagnostics and
  legacy assets, not an implicit production fallback. `window.jtml.browserBuild`
  is the formal mode/asset contract; `productionMode` is a compatibility alias.
- Prefer generated static component create/update paths. Preserve direct slot
  rendering, retained nested-component updates, keyed identity/lifecycle cleanup,
  and input focus/cursor behavior. Component `into` targets must be simple local
  writable `let` names; invalid bindings fail closed.
- Browser policy belongs in `cli/browser_build_policy.cpp`, copying in
  `cli/browser_build_assets.cpp`, and closed public schema validation in
  `cli/browser_build_schema.cpp`; do not accumulate policy in `browser_build.cpp`.
- Runtime chunks are JavaScript-only. Inline callers add script wrappers at the
  emission boundary; never derive production JS by stripping HTML wrappers.
- Directory builds copy `public/`, `assets/`, and selected root web metadata;
  symlinks are intentionally skipped.

## Build and verification

C++17 and CMake are required. CMake fetches pinned WebSocket++ 0.8.2,
standalone Asio 1.30.2, and GoogleTest during configure, so network access or a
populated dependency cache is needed. JSON and httplib headers are vendored in
`third_party/`. Python bindings are optional and disabled in the baseline below.

```sh
cmake -S . -B build -DJTML_BUILD_TESTS=ON -DJTML_BUILD_PYTHON=OFF
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/jtml doctor --json
scripts/verify_all.sh
```

- `scripts/verify_all.sh` is the full local predeploy gate: build, CTest, doctor,
  examples/demos, browser asset and execution benchmarks, package/refactor/LSP
  checks, editor validation, site/package artifacts, and live runtime smoke.
  Run it before claiming a slice complete when build/runtime/CLI/examples/demo/
  packaging/smoke behavior changes. Report skipped checks and failures explicitly.
- Full coverage needs Python 3, Node.js, curl, Chrome/Chromium, and permission to
  launch browsers and bind local ports. Some script sections skip missing tools;
  a zero exit alone does not prove all checks executed.
- `scripts/benchmark_runtime.sh` checks asset budgets and generated contracts,
  then invokes `scripts/benchmark_browser_runtime.sh` against built fixtures.
  Standalone browser runs may skip; `JTML_REQUIRE_BROWSER_BENCH=1` makes browser
  execution mandatory (set by the full gate). `JTML_BENCH_BROWSER` selects a
  browser. Do not weaken assertions or silently raise budgets.
- `browserExit=terminated-after-result` proves results, not clean shutdown. Use
  `JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1` to require clean browser exit.
- CTest auto-discovers GoogleTest cases in `jtml_tests`; use `ctest --test-dir
  build -R <filter> --output-on-failure` for focused runs. Fixtures live in
  `tests/fixtures/`; `JTML_SOURCE_DIR` provides the repo root to C++ tests.

## CI

`.github/workflows/ci.yml` runs on pushes and PRs to `main`: Ubuntu and macOS,
Release, Ninja, Python bindings off. It builds, runs parallel CTest and `jtml test`,
creates site/package artifacts, lints examples with four explicit exclusions,
and checks formatter idempotence. It does **not** run `verify_all.sh`, browser
benchmarks, the standalone doctor check, or the full LSP/live smoke gate. Local
verification and a green CI run are distinct evidence; do not claim equivalence.

## Code navigation and conventions

- Headers: `include/jtml/`; implementation: `src/` and `src/jtml/`; use matching
  snake_case filenames and existing ownership boundaries.
- `CMakeLists.txt` groups syntax, semantic, browser production, live runtime,
  legacy runtime, and tooling sources. Keep additions in their owning group.
- `cli/main.cpp` dispatches to `cli/cmd_*.cpp`; reuse compiler services.
- Examples: `examples/`; lessons: `tutorial/`; Studio content: `studio/`;
  language/tooling docs: `docs/`; VS Code extension: `editors/vscode/`.
- `build/jtml explain <source> --json` provides semantic introspection;
  `doctor --json` provides machine-readable readiness. Use supported `--json`
  modes for tooling. `jtml test` smoke-tests bundled examples.
- `jtml fix` applies safe repairs; `jtml fmt` must remain source-preserving and
  idempotent. `jtml lint` exits nonzero on errors.
