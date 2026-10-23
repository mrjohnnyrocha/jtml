# JTML Restart Backlog

> Historical implementation/backlog notes below are not release acceptance.
> The current v0.3 proposal, measured blockers and support contract are in
> [v0.3 readiness](docs/releases/v0.3-readiness.md).

This is not a roadmap. It is the first practical backlog for restarting after
hibernation. Keep it short and executable.

## 1. Verify version/release contract

- Objective: keep CMake, CLI, C API, `doctor`, docs, and package metadata on
  one visible version.
- Reason: version drift makes freeze/restart health ambiguous.
- Relevant files: `CMakeLists.txt`, `cmake/JtmlVersion.cmake`, `cli/util.cpp`,
  `src/c_api.cpp`, `README.md`, `docs/reference/language-reference.md`.
- Definition of done: `./build/jtml --version`, `./build/jtml doctor --json`,
  and C API version tests agree.
- Dependencies: none.
- Size: S.

## 2. Complete source-span propagation

- Objective: carry source file, line, column, component, semantic node, and
  remediation through parser, Friendly lowering, semantic analysis, runtime
  plan, browser compiler, and live runtime errors.
- Reason: source-first diagnostics are essential for AI-native repair and
  production trust.
- Relevant files: `include/jtml/ast.h`, `src/parser.cpp`, `src/friendly.cpp`,
  `src/semantic.cpp`, `src/runtime_plan.cpp`, `src/runtime_plan_json.cpp`,
  `cli/browser_build_schema.cpp`, `src/interpreter.cpp`.
- Definition of done: fixtures with imports, comments, blank lines, nested
  components, actions, attributes, and browser/live failures report stable
  original-source locations.
- Dependencies: item 1.
- Size: L.

## 3. Extend TypeEnvironment toward semantic validation

- Objective: turn current expression value-type hints into a real semantic
  validation layer for params, local state, fetches, route params, forms, slot
  payloads, and event payloads.
- Reason: typed semantic feedback is the next core compiler advantage.
- Relevant files: `include/jtml/expression_ir.h`, `src/expression_ir.cpp`,
  `include/jtml/semantic.h`, `src/semantic.cpp`, `src/runtime_plan_json.cpp`.
- Definition of done: linter/check diagnostics catch common type/write/access
  mistakes without runtime execution.
- Dependencies: item 2 preferred.
- Size: L.

## 4. Reduce common-path operation/helper fallback

- Objective: make static component modules generate direct create/update/destroy
  paths for the most common component, slot, nested component, keyed list, and
  action shapes.
- Reason: compatibility fallback must become explicit and rare in production.
- Relevant files: `src/jtml/runtime/static_update_plan_emitter.cpp`,
  `src/browser_runtime_assets.cpp`, `tests/fixtures/performance/`.
- Definition of done: clean browser production fixtures no longer depend on
  operation-record paths for the supported common shapes.
- Dependencies: items 2 and 3 improve safety.
- Size: XL.

## 5. Add browser/live parity tests

- Objective: compare browser-local and live behavior for slots, emitted events,
  routes, fetch, media, keyed lists, nested components, forms, and effects.
- Reason: the same semantics must not diverge silently across backends.
- Relevant files: `tests/test_friendly.cpp`, `tests/test_cli_commands.cpp`,
  `tests/fixtures/performance/`, `scripts/benchmark_browser_runtime.sh`.
- Definition of done: new parity fixtures fail when either backend drifts.
- Dependencies: item 2.
- Size: L.

## 6. Harden forms and bindings

- Objective: define and test input, textarea, checkbox, radio, select, file,
  submit/reset, validation, focus, cursor, and IME behavior.
- Reason: real apps fail fastest in forms.
- Relevant files: `src/friendly.cpp`, `src/semantic.cpp`,
  `src/browser_runtime_assets.cpp`, `src/interpreter.cpp`, tests/fixtures.
- Definition of done: browser and live fixtures prove state updates without
  unwanted element replacement, focus loss, or invalid writes.
- Dependencies: items 2 and 5.
- Size: L.

## 7. Resolve or quarantine OpsPulse

- Objective: decide whether `demos/opspulse/` is canonical modular demo,
  experimental work, or archive material.
- Reason: it was dirty at freeze time and should not confuse future maintainers.
- Relevant files: `demos/opspulse/`.
- Definition of done: README and code agree with current module capabilities,
  or the folder is clearly marked experimental.
- Dependencies: none.
- Size: M.

## 8. Establish minimal CI/release gates

- Objective: encode the local gate in repository automation.
- Reason: restart and release should not depend on memory.
- Relevant files: `.github/` if added, `scripts/verify_all.sh`,
  `scripts/benchmark_runtime.sh`, docs.
- Definition of done: CI runs configure, build, CTest, `verify_all`, and browser
  benchmark when Chrome/Chromium is available or explicitly required.
- Dependencies: stable local gate.
- Size: M.

## 9. Split monoliths after boundaries stabilize

- Objective: reduce large files only when ownership boundaries are proven by
  tests and docs.
- Reason: premature splitting would obscure behavior still shared by multiple
  backends.
- Relevant files: `src/friendly.cpp`, `src/interpreter.cpp`,
  `src/browser_runtime_assets.cpp`,
  `src/jtml/runtime/static_update_plan_emitter.cpp`,
  `src/runtime_plan_json.cpp`.
- Definition of done: extracted modules have clear ownership, no broad
  formatting churn, and focused regression tests.
- Dependencies: items 2-6.
- Size: XL.

