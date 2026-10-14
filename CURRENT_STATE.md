# JTML Current State

Status: v0.3 DEVELOPMENT BASELINE / RESEARCH

The repository is being prepared for disciplined v0.3 development. Read this
snapshot before selecting a scoped change; this baseline does not add language
features or declare production readiness.

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

Do not try to resolve this migration during baseline maintenance. Future work
should continue moving common production paths toward static/browser execution
and leave compatibility/live paths as explicit backends.

## PR #7 macOS routing harness repair — 2026-10-14

The failed macOS run `38339524926` reached the direct-route iframe check after
335 CTests and the reference app/keyed-action/form-submit checks passed. Its
`load` event timed out at 3 seconds. With the same Playwright 1.63.0 and Chrome
Headless Shell 153.0.8010.12 locally, two instrumented runs instead completed
navigation: index and all three generated JS assets returned HTTP 200, and the
child router resolved `/about`. However, the CDP parent remained `about:blank`
with opaque origin `null`, making the HTTP child's `contentDocument` null.
A `<base>` supplies URL resolution, not a document origin. The original runner
log lacks request/frame traces, so its precise load-event stall is not proven;
the independent origin failure makes that iframe check invalid for this loader.

The CDP path now creates a fresh page, installs the generated `index.html`, sets
its actual document URL hash to `#/about` before evaluating any generated script,
and uses the same loader for the real generated assets. Startup must emit exactly
one ready event and a single `/about` route event, with the correct visible DOM,
metadata, and no errors. A temporary wrong-initial-hash control failed the direct
load assertion. Real HTTP/native-browser runs retain the iframe navigation check,
with cleanup on failure. All 20 routing assertions and existing budgets remain.

Local focused suite (`scripts/test_production_reference.sh` with
`JTML_BENCH_BROWSER` pointing to the pinned headless shell and
`JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT` to Playwright 1.63.0) passed the reference app,
keyed actions, form submit, and routing. The expected nested-heading failure and
135395-byte reference HTML size regression remain reported, not hidden.

## Verified Development Baseline — 2026-10-12

Starting commit: `5e0761925fcdd748268d3049d613fb767e1dbd45` (clean checkout).
This supersedes the 2026-08-23 freeze results. No compiler/runtime behavior or
performance budgets were changed for this baseline.

Environment: macOS 14.4.1 (23E224), arm64, Apple Clang 15.0.0, CMake 3.31.3,
Unix Makefiles, Release, Homebrew Boost 1.85.0 and WebSocket++ 0.8.2, Python 3.12.3,
Node 22.23.3, Google Chrome 154.0.8037.98. The existing build/dependency cache
was reused; this is not evidence of a fresh network dependency installation.
For an empty build directory, add `-DCMAKE_BUILD_TYPE=Release` to reproduce
this configuration. CI uses Ninja on Ubuntu/macOS; those hosted environments
need their own run.

Commands executed from the repository root:

```sh
cmake -S . -B build -DJTML_BUILD_TESTS=ON -DJTML_BUILD_PYTHON=OFF
cmake --build build --parallel
ctest --test-dir build --output-on-failure
ctest --test-dir build --output-on-failure --parallel
./build/jtml doctor --json
JTML_REQUIRE_BROWSER_BENCH=1 scripts/benchmark_runtime.sh
JTML_REQUIRE_BROWSER_BENCH=1 scripts/benchmark_browser_runtime.sh
JTML_REQUIRE_BROWSER_BENCH=1 JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1 scripts/benchmark_browser_runtime.sh
scripts/verify_all.sh
JTML_BROWSER_BENCHMARK=0 scripts/verify_all.sh
```

The initial sandboxed browser/full-gate attempts failed at localhost socket
binding (`PermissionError: [Errno 1] Operation not permitted`). Browser and
live-runtime checks require execution outside that sandbox; do not describe
those initial attempts as passes or disable browser checks to obtain success.

### Results

| Check | Verified result |
| --- | --- |
| Configure/build | Exit 0; Release CLI, C API, tests, and architecture link checks built |
| Sequential CTest after rebuild | 332/332 passed, 0 failed; 2.77 seconds |
| Parallel CTest (CI-style) | 332/332 passed, 0 failed; 1.62 seconds |
| `doctor --json` | Exit 0; `ok: true`, `version: 0.1.0-dev`, `enterpriseReady: false` |
| Example smoke | 41/41 examples passed |
| Asset benchmark | All 9 fixtures passed clean/compat asset contracts and unchanged budgets; runtime 269338 bytes against 272000-byte budget |
| Required browser benchmark outside sandbox | Exit 0; 9/9 fixtures passed; 200 interactions each, 201 for each large keyed fixture; all `terminated-after-result` |
| Full predeploy gate outside sandbox | `JTML_BROWSER_BENCHMARK=0 scripts/verify_all.sh` exits 0: required 9-fixture browser coverage still runs, package/refactor/source-map/LSP/editor checks and site/package artifacts pass, live runtime smoke 8/8 passes; no skip messages |
| Strict clean-exit benchmark | Exit 1 on `static_counter`: result produced but Chrome did not exit cleanly |
| CI lint shell block | Exit 0 for 37 examples; same 4 exclusions retained |
| CI formatter shell block | Exit 0 for all 41 examples, including relative imports |
| Shell syntax / whitespace | Both extracted workflow shell blocks and benchmark scripts pass `bash -n`; `git diff --check` passes |
| Missing-browser gate probe | With browser discovery stubbed unavailable in a temporary script copy: optional mode exits 0 with SKIP; required mode exits 1 |

The CI shell blocks were extracted verbatim from `.github/workflows/ci.yml` and
executed with Bash from the repository root. Raw local logs are in
`/tmp/jtml-baseline/` (ephemeral); normal gate artifacts are under ignored `dist/`.
An early pre-rebuild CTest discovery cache listed 334 tests; the rebuilt binary
and regenerated discovery consistently report 332. Use post-build counts.

### Reproducibility repairs

- CI's multiline shell `case` patterns did not parse (`bash -n` exit 2). Added
  line continuations, preserving all four existing lint exclusions.
- CI formatter pass two used `/tmp`, losing relative imports (first failure:
  `friendly_advanced_form.jtml` -> `./imported_keywords.jtml`). Unique temporary
  files now stay beside examples and are removed by a trap. Errors remain visible.
- Required browser benchmarks previously returned success when no browser was
  installed, and `JTML_BROWSER_BENCHMARK=0` could bypass them. Required mode now
  fails for a missing browser and takes precedence over that optional skip flag.
- `AGENTS.md` now describes actual CI coverage and durable rules. Removed stale
  source-group line numbers, repetitive implementation history, and contradictory
  production fallback instructions. Browser ownership details live in
  `ARCHITECTURE.md`; historical milestones remain in the architecture roadmap.

### Known limitations and architectural risks

- CI is present but is not the full predeploy gate: it omits browser benchmarks,
  standalone doctor, and the full LSP/live runtime checks. Its four lint exclusions
  are unchanged; cross-file resolution exists, so the former blanket explanation
  for those exclusions was misleading. Audit coverage separately.
- The browser harness uses virtual time and reported `duration=0.00ms`; its DOM,
  identity, cleanup, and asset assertions are useful proof, but these measurements
  do not establish real wall-clock performance. No timing budgets were raised.
- Chrome can return benchmark DOM without exiting within the 15-second timeout.
  `browserExit=terminated-after-result` is not a clean-process-exit pass. Require
  `JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1` when validating that stronger contract.
- The full script conditionally skips some Python/Node checks when tools are
  absent. Record installed tools and actual coverage, not just exit status.
- Multiple execution/dependency models, lowering-derived body-plan structure,
  incomplete source spans/types, form semantics, and browser/live parity remain
  the architecture risks described above. This task does not resolve them.
- Python bindings, sanitizers, coverage, and a fresh Linux build were not tested.
  `doctor` readiness is self-reported metadata, not proof of enterprise readiness.

Update these results when the baseline is reproduced; keep failures and skipped
checks visible rather than treating historical success as current evidence.

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

- `demos/opspulse/` - tracked modular demo (20 files at this baseline), covered
  by the local gate. The historical uncommitted-work warning no longer describes
  this clean starting checkout; passing smoke checks does not establish full
  browser/live parity or production readiness.
- Older Classic examples - useful for compatibility regression, not public
  authoring guidance.
