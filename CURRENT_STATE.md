# JTML Current State

Status: v0.3 DEVELOPMENT BASELINE / RESEARCH

The repository is being prepared for disciplined v0.3 development. Read this
snapshot before selecting a scoped change; this baseline does not add language
features or declare production readiness.

## Friendly component parameter capture

The original Parent count=100 / Child count=10 reproducer now returns **200** in
expanded state, native rendered HTML and clean-production browser DOM, with its
source and independent oracle unchanged. Friendly argument/local binding is
simultaneous and preserves existing inline-content syntax roles. Native runtime
and the PR #31 environment repair are unchanged. Validation: 204/204 focused tests,
437/437 CTests, nine clean-exit browser benchmarks and full local gate PASS,
no checks skipped. Two separately reproduced grouped/global argument backend
counterexamples remain FAIL / KNOWN DEFECT, plus the five unchanged native
attribute-renderer observations. See [causes, evidence and review limits](docs/architecture/friendly-parameter-capture.md).

## Native nested environment repair

The original differential matrix is now 22/22 PASS, resolving the two native
nested-parameter diagnostic defects without changing their source or oracles.
Expanded child state, sibling isolation and repeated remount/reset have additional
regressions. PR #31 verification: 432/432 CTests, nine clean-exit browser benchmarks
and complete predeploy gate PASS, no checks skipped. The Friendly parameter
capture found then is repaired by the subsequent slice above; the five
supplemental native attribute-renderer observations remain known failures.
Original strict conformance passes; supplemental strict mode still exits nonzero
for separately catalogued backend boundaries.
See [ownership, evidence and limits](docs/architecture/native-nested-environments.md).

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

## Taskboard negative validation — 2026-10-19

N01–N03 and N05–N08 pass independent clean-production Chromium checks. **N04
remains BLOCKED**: unsupported writes reject before runnable output, but linked
and module diagnostics disagree on original line/column. Strict provenance
acceptance exits 2; **full T0 is incomplete**. The approved contract and previous
oracles are unchanged. See [negative validation evidence](docs/architecture/taskboard-t0-negative-validation.md).

Generic repairs cover structured action tokenization and deferred asset startup.
The startup correction exposed duplicate keyed component paths; generated `for`
creation now shares reconciliation's scoped key path, with exception-safe cleanup.
Original lifecycle/commit tests and a new two-owner/two-loop regression pass.
Runtime is **271564/272000** bytes (436-byte margin); compressed asset growth is
recorded. Repaired full local gate **PASS**, **362/362 CTests**, all nine clean-exit
benchmarks and live smoke **8/8**, no verification skips. N04 strict acceptance
still exits 2. Final-head hosted CI is recorded separately on the PR. Native
store instability, linter limits and source provenance remain unresolved.

## Taskboard Core functional C09–C14 — 2026-10-18

The object-backed Taskboard now passes **C01–C14** on clean production assets:
conditional probes/Help lifecycle, parent-owned deletion, monotonic restart IDs,
reorder boundaries, Complete/Open filtering and distinct empty states. Generic
repairs address removal focus, action-local invalidation and duplicate linked /
module instance projections. No acceptance scenario, previous oracle or budget
was weakened. See [functional evidence](docs/architecture/taskboard-core-c09-c14.md).

After integrating PR #15 main `7a848bb`: full local gate **PASS**, **361/361
CTests**, all nine required browser benchmarks
with clean exits, production reference, lifecycle/commit recovery, literal/native
regressions and live smoke **8/8**, no skips. Runtime **271969/272000** bytes (+340);
HTML/module raw, gzip and Brotli growth and remaining duplication are recorded.
Taskboard check passes; lint fails **232 errors / five warnings**. N01–N08, full
T0 acceptance, source provenance and native store instability remain unresolved.
PR #15's server/security/adapter/interpreter code and tests match main; real
server security integration passes independently and in the gate. Client manifest
root/module preservation audit passes; existing clean imported-component and
compat top-level parameter limitations remain visible in the evidence. Hosted
Ubuntu/macOS CI is separate final-head evidence on the implementation PR.

## Browser literals and live nested calls — 2026-10-18

Source literals now decode consistently through Friendly lowering, the lexer,
Expression IR reparsing and browser source evaluators while retaining JSON-safe
escaping. Clean/compat Chromium checks cover exact reactive values and XSS
negatives; malformed source fails with diagnostics. Diagnostic comments escape
hostile literal text. Live nested/recursive calls use exception-safe per-interpreter
frames with a 128-frame bound and explicit overlapping-entry rejection; this does
not certify general concurrent interpreter or detached-async lifetime safety.

Final local full gate passes **357/357 CTests**, all nine clean-exit browser
benchmarks, complete production reference, C01–C08/lifecycle/commit recovery and
live smoke **8/8**, without skips. Runtime is **271629/272000** bytes (+515).
The known native store crash and reference HTML size regression remain visible;
no repair of either is claimed. See [repair evidence](docs/architecture/literal-fidelity-and-function-calls.md)
for pre-fix failures, early candidate failures, negative controls and limits.
Hosted Ubuntu/macOS CI is recorded separately in PR #14. No C09–C14 work or merge.

## Taskboard interactive workflow C04–C08 — 2026-10-18

The existing object-backed application now verifies parent-owned title editing,
native note save, immutable-ID keyboard reordering, completion and genuine
Open/All unmount/remount. Independent C04–C08 acceptance and the original C01–C03 /
lifecycle/commit-recovery oracles pass. Generic repairs address scalar ownership,
emit-only rendering, static keydown binding, computed assignment indices and
focus after removal. No contract or budget changed; later scenarios remain
unverified. See [workflow evidence](docs/architecture/taskboard-core-c04-c08.md).

Final local gate passes 348/348 CTests, full production reference, all nine clean
browser benchmarks and live smoke 8/8 without skips. Taskboard check passes;
lint still fails (182 errors/five warnings). Shared runtime is 271114/272000 bytes.
The native store crash remains unresolved; a separate clean investigation
worktree's five healthy main startups are not proof of a fix. Hosted final-head
CI is recorded separately in the implementation PR. No automatic merge.

## Taskboard Core C01–C03 — 2026-10-18

The first adversarial slice starts from merged PR #11 (`b4b7501`). Two independent
object-backed boards, immutable-ID keyed rows, nested components, derived counts,
text escaping and native create/validation pass the independent C01–C03 browser
oracle. Full T0 and C04–C14/N01–N08 remain unverified; the approved contract and
support matrix are unchanged. Generic static control emission and nested keyed
reconciliation repairs preserve binding owners, live input values and retained
component identity. No compatibility fallback or Taskboard compiler special case
was introduced.

Local Release validation: focused CTests 47/47, full CTests 346/346, complete
production reference suite, all nine required browser/asset benchmarks and
`scripts/verify_all.sh` pass with no skipped checks and clean browser exits;
live smoke 8/8 on the settled-build rerun. The earlier full gate aborted in
`friendly_store` smoke (exit 134); cause remains unestablished. Shared runtime
after bounded commit recovery is 270406/272000 bytes; no budgets changed. Two
negative controls fail on old control code / old keyed runtime respectively.
Taskboard lint still fails on lowered component/action-local names; it is not
counted as a pass. See the [implementation evidence](docs/architecture/taskboard-core-c01-c03.md)
for scenario results, failures, asset deltas, source-provenance limitations and
reproduction. Hosted CI is separate evidence, not implied by local success.

Independent review found a fault-injection-only failed-commit boundary at
`a74d88c`; no native commit exception from valid supported JTML source was
demonstrated. The bounded repair journals applied text/attribute/child-list edits,
restores them and tentative registry/scope metadata on failure, flags the original
error to prevent wrapper fallback, and destroys omitted registrations only after
successful commit. Four mandatory real-browser cases test one-shot exceptions
before/after insertion, exact ownership/identity/focus recovery and direct retry;
all pass, while saved `a74d88c` fails all four. Original P1 and C01–C03 oracles
remain unchanged. Persistent or reentrant hostile host mutation is unsupported;
failed recovery is reported rather than represented as a transaction guarantee.

The first candidate full gate again aborted at `friendly_store` smoke 4/8. macOS
reported malloc free-list checksum corruption while function text was formatted;
the corrupting operation remains unknown. Five independent startups reproduced a
native crash in saved pre-change `a74d88c` (4/5 ready, one SIGSEGV); the candidate
was ready 5/5. The settled full-gate retry passed, including smoke 8/8, with no
skips. This pre-existing native startup issue remains unfixed follow-up work.
See the implementation note for exact negative controls, logs, sizes and the
bounded support decision. Scoped assessment: MERGE READY, subject to final-head
Ubuntu/macOS CI recorded in PR #12. No automatic merge or C04 work.

## Nested component parameter propagation — 2026-10-14

The production nested-heading regression now passes and is mandatory in
`scripts/test_production_reference.sh`. The static update selector previously
suppressed descendants after selecting an attribute-only container patch.
It now separates selected nodes from selected subtree patches, preserving
nested text/attribute updates without replacing component DOM or adding fallback
paths. The expanded browser scenario completes 22 assertions over five actions,
including repeated changes, two nested component levels, sibling isolation,
retained DOM/instance identity, stable registry ownership, and no network/errors.
Restoring the old suppression in generated assets fails the first Rename.

Local macOS Release validation used main `e1b9fb2`, Chrome Headless Shell
153.0.8010.12, Playwright 1.63.0, and cached CMake dependencies. Focused CTests
passed 55/55; full CTests passed 335/335. The production reference suite, all nine
real-browser/asset benchmarks, and `scripts/verify_all.sh` passed with no skipped
checks; all nine browser benchmark processes exited cleanly. Existing budgets
were unchanged. The initial out-of-tree `/tmp` build failed doctor's layout
CTest because it searches ancestors for repository assets; the normal `build/`
layout resolved that test without a code or assertion change.

The reference app's existing 135395-byte HTML size regression remains reported.
An additional `if title == "After"` probe still has an empty Runtime Plan read
set and stale conditional DOM; that separate dependency limitation is documented
with the root cause and regression contract in
[the implementation note](docs/architecture/nested-component-parameter-propagation.md).
These local results do not establish hosted CI or production readiness.

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

## Conditional dependency correctness — 2026-10-17

Starting from main `1208ca7` (nested parameter PR #8 integrated), the conditional
reactivity slice repairs complete-expression Runtime Plan reads, both-branch
static region dependencies/ownership, and conservative mutable object propagation.
Owned member/index writes invalidate aliases; unsupported borrowed mutations and
unanalyzable production conditions fail closed with source/component remediation.
The precise bounded expression/write contract and conservative ownership limits
are documented in [conditional reactivity](docs/architecture/conditional-reactivity.md).
This is not a freeze of the complete v0.3 surface.

The exact new fixture fails on the original PR #8 compiler at the first Advance.
With the repair, all 341 CTests, the complete production reference suite, all nine
real browser benchmarks and `scripts/verify_all.sh` pass. The mandatory conditional
scenario performs 28 actions/86 assertions, including independently varied
operands, aliases, both branches, repeated writes, siblings and component identity.
Pinned Chrome Headless Shell 153.0.8010.12/Playwright 1.63.0 exits cleanly for every
browser fixture. No validation was skipped and no budget increased. Shared runtime
is 271796/272000 bytes; large-keyed-reorder compatibility plan is 177847/180000 bytes.

The initial full gate caught an update-plan budget failure (185911/180000 bytes)
from incorrectly promoted row-local key dependencies; correcting Runtime Plan
scope, rather than changing the budget, resolved it. The existing composed
reference HTML size limitation remains (135374 bytes versus its 70000-byte
microfixture comparison). Local verification does not establish production or
release readiness; hosted CI is separate evidence.

## PR #10 review repairs — 2026-10-17

The reviewed head `6384d34` had three reproduced production defects: an unchanged
conditional branch replaced and unregistered interactive children; iterator
bindings bypassed borrowed ownership; and unsupported RHS/computed-target calls
passed assignment validation, becoming literal text or a no-op.

The follow-up retains active branches through generated static descendant patches,
orders transition destruction before creation, resolves ownership by lexical
binding identity (including iterators and aliases), and validates complete static
assignment expressions/operators/ownership before clean production generation.
Conservative invalidation and all existing budgets remain intact.

The original compiler fails the new lifecycle identity assertion; all three new
CTests fail against the reviewed backend. The repaired version passes 344/344
CTests, the complete reference suite (new lifecycle 22 actions/32 assertions and
owned-loop 3 actions/5 assertions, plus the existing conditional matrix), all nine
real-browser/asset/CSP benchmarks and `scripts/verify_all.sh` with live smoke 8/8.
No checks were skipped; all benchmark browsers exited cleanly. Shared runtime is
271879/272000 bytes; large keyed compatibility output remains 177847/180000 bytes.
The existing reference HTML size limitation is still visible. See the
[architecture note](docs/architecture/conditional-reactivity.md) for root causes,
negative controls, supported contracts and conservative ownership limitations.
Hosted CI against the final pushed head is separate evidence; no merge or release
is implied by these local results.

## PR #10 shared declaration scope repair

Review head `5a56947` incorrectly accepted a nested `let item = input` followed
by a write to an outer owned `item`: the runtime shared storage while ownership
analysis assumed separate lexical bindings. Clean production now fails closed
for nested state/derived declarations that collide with another component binding,
including sibling branches/actions, parameters and iterators. Use unique local
names. Runtime/live/compatibility semantics and all budgets are unchanged; see the
[scope trace and contract](docs/architecture/conditional-reactivity.md#shared-declaration-scope-repair-reviewed-head-5a56947).

The exact reported fixture must fail compilation before assets are emitted.
Both new CTests and the new production-reference guard fail against `5a56947`.
The supported unique-name remediation runs in real Chrome alongside owned loop
writes: 6 actions/14 assertions cover nested branches, repeated updates, unchanged
borrowed parent state and retained component identity/registry ownership.
Final local checks pass: 346/346 CTests, complete production reference suite,
all nine real-browser/asset/CSP benchmarks and `scripts/verify_all.sh` (live smoke
8/8), with no skipped checks and clean browser exits. All three prior PR #10
regressions remain covered and passing. Runtime remains 271879/272000 bytes;
large-keyed compatibility output remains 177847/180000 bytes. The existing
135374-byte reference HTML limitation remains visible. Hosted CI is separate.

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

## Local serve / Studio security defaults — 2026-10-18

The local-server security branch integrates merged PR #14 without changes to
its string decoding or interpreter function execution. HTTP and WebSocket bind
to IPv4 loopback; pre-routing Host/Origin/Fetch Metadata checks protect mutations
independently of CORS, whose wildcard grant has been removed. Native clients
without Origin remain supported and unauthenticated. Studio preview sockets use
the configured HTTP + 80 port.

HTTP runtime handlers, Studio POSTs, WebSocket callbacks and watch reloads share
recursive adapter serialization for supported synchronous execution. Focused
policy/function-call tests (8/8) and real HTTP/WebSocket/browser security tests
pass with no skips; old-server negative controls reproduce foreign execution,
foreign WebSocket upgrade and network-interface exposure. Detached async workers
remain outside synchronization and lifetime ownership; this is follow-up work,
not a general Interpreter thread-safety claim. The known intermittent native
`friendly_store` crash remains unresolved. The full local gate passed once on the combined tree: 359/359 CTests,
production references, Taskboard C01–C08, lifecycle and PR #14 literal tests,
all nine browser benchmarks with clean exit, package/site/LSP checks and live
smoke 8/8. No validation was skipped; the store crash did not reproduce in this
run and remains unresolved. Final-head hosted CI is separate PR evidence. See the
[security contract](docs/tooling/local-server-security.md) for exact boundaries.

## Owned interpreter async lifetime — 2026-10-19

Async functions now enqueue explicitly owned work and remain deferred. Live
workers serialize with HTTP/WS/watch adapters; headless instances use separate
execution locks. Reset/reload finishes active execution and cancels pending
old-generation work with diagnostics. Normal destruction closes admission and
drains admitted work and finite descendants outside execution locks, then joins
the WebSocket thread. Stop-before-start requests are retained. Reentrant
lifecycle replacement/drain and self/locked destruction are explicitly rejected;
borrowed mutable environments/values still require host serialization/lifetime
coordination. See the [lifetime and public-operation audit](docs/architecture/async-worker-lifetime.md).

Local evidence on frozen implementation: 374/374 CTests; ASan/UBSan 19/19 and
separate TSan 19/19 with halt-on-error; actual HTTP/WS async execution in serve,
Studio and watch; full local gate including all nine clean-exit browser
benchmarks, production references, Taskboard C01–C14, security/lifecycle/literal
regressions, package/site/LSP checks and live smoke 8/8. No validation skipped or
budgets raised. Negative controls reproduce the old unowned-worker boundary and
lost early-stop request. Earlier fixture/setup failures and a post-PASS TSan
finalization timeout remain documented rather than counted as passes. The native
store crash remains unresolved and did not occur in the final gate. Hosted CI
is separate final-head PR evidence. No Taskboard/browser rendering implementation
changed; N01–N08 remain outside this work.

The owner audit also fixes C API replacement: it drains previous work before
new compilation, surfaces old worker errors through the existing error result,
and destroys the previous Interpreter before releasing its transpiler. A saved
old-owner-order candidate reproduces ASan heap-use-after-free in
`getNodeBinding`; the bounded C API regression now passes both sanitizers.
Native renderer teardown no longer writes shared diagnostic output, fixing the
TSan-proven overlap with a replacement runtime. Browser rendering is unchanged.
