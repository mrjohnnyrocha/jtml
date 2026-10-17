# First adversarial Taskboard Core slice: C01–C03

Implemented against main `b4b7501` (merged PR #11), 2026-10-18. The
[acceptance contract](../release/taskboard-acceptance-contract.md) is unchanged;
its expected data and outcomes are the independent oracle. This is neither full
T0 completion nor a release/support-surface freeze.

## Application and ownership

[Taskboard source](../../tests/fixtures/production/taskboard-core/index.jtml)
instantiates two boards with separately owned ID→object maps, ordered ID lists,
and completion indexes. Canonical objects contain immutable `id`, `title`,
`completed` and `note` fields. Only the parent creation action writes records;
rows borrow records and key by immutable ID, never title or position. Both boards
start with the same IDs but distinct objects and component identities.

The completion index starts with `t-103` and remains unchanged in this slice,
which only creates open tasks. Total/complete/open/visible counts are derived
from the parent's order/index. The browser oracle independently cross-checks the
index and counts against the canonical record flags; later completion actions
must maintain that invariant rather than treating the index as an independent
truth. Each row nests a header and status component, owns title/note drafts, and
renders user titles as text.

Creation uses a native form, one submit action, associated validation feedback,
and a parent-owned monotonic ID counter. A JTML character loop trims boundary
whitespace while preserving internal whitespace. The whitespace literal contains
actual ASCII/Unicode whitespace because current JTML string escaping does not
decode JavaScript-style `\t`/`\v`/`\f` escapes. Native single-line text inputs strip
CR/LF. No host helper, method-call whitelist, compatibility mode or dynamic
compilation was added to implement trimming. A title is accepted only when the
trimmed value is nonempty; only that branch creates a record and consumes an ID.

## Independent browser evidence

[Oracle](../../tests/browser/taskboard_core.cjs) has literal expected records,
counts and IDs from the contract; it does not copy manifest output or generated
read sets. It drives actual Playwright keyboard Enter and button activation on
the generated clean assets using `file:` navigation. No application HTTP service,
routing, persistence or authentication is introduced. Existing broader browser/live
suites still use their ordinary local test servers.

| Scenario | Result | Observed outcome |
| --- | --- | --- |
| C01 | PASS | Both boards: IDs 101–104 in contract order, four separate object-backed rows, duplicate titles preserved, counts 4/3/1 and visible 4, nested Before/status output, correct draft values, escaped `<release> & CSP`, distinct sibling-owned objects/instance IDs. DOM/input references and registry captured. |
| C02 | PASS | Empty Enter, whitespace button and additional tab/NBSP Enter submissions show associated errors. Every edit reaches form state; records, counts, next ID 105, URL and retained rows/generations stay unchanged; one submit action per native submit. |
| C03 | PASS | Enter submits `  Audit runtime  ` once: exactly t-105, trimmed duplicate title, empty note, open flag, ordered fifth keyed row, counts 5/4/1 and visible 5, next ID 106. Board B stays at seed. Existing DOM/generations retained, create input/error cleared, next edit reaches state. |
| C04–C14 | UNVERIFIED | Not implemented/executed by this slice. |
| N01–N08 | UNVERIFIED | No complete negative-scenario acceptance claim; bounded safety checks above do not constitute passing these scenarios. |
| T1/T2 | UNVERIFIED | v0.4 service/application acceptance remains separate. |

The oracle also rejects unexpected fetch/XHR/WebSocket, unexpected resource
requests, page/console errors, legacy mode/globals and production fallback
telemetry. The runner scans dynamic compilation and component fallback metadata.
These checks are bounded evidence, not comprehensive security acceptance.

A separate [retained-control fixture](../../tests/fixtures/production/retained-control/index.jtml)
is a focused generic browser regression: two Editors, repeated native submits,
unchanged-value selection start/end/direction, retained owner/body markers, ARIA
boolean strings, dirty input/textarea clearing, select live value updates, and
sibling isolation. It does not implement more Taskboard scenarios.

## Earliest incorrect layers and generic repairs

1. The compiler's static element emitter mistook an attribute-only element's raw
   attribute expression for missing content. That sent otherwise static controls
   through a helper path whose attribute patch removed their binding owner.
   A fresh valid create worked, but edits after invalid submits stopped reaching
   the parent draft. Explicit empty content plans now remain empty; direct creation
   and patching use the same binding ownership metadata.
2. Generated patches could remove structural body markers, serialized boolean
   ARIA values as boolean-attribute names, and updated only the attribute rather
   than the dirty control's live value. The generic emitter preserves its body
   marker, stringifies value-bearing attributes, emits empty `value`, and changes
   the input/textarea/select property only when its value differs. Unchanged
   values therefore retain selection.
3. Keyed reconciliation refused to descend through nested components unless
   their serialized HTML matched exactly. Equivalent class/attribute serialization
   differences replaced four existing row subtrees on a fifth-row insertion.
   Reconciliation now walks matching marked descendants and requires matching
   component instance IDs before reuse. No Taskboard branch or runtime fallback
   was introduced; generic existing lifecycle/regression suites remain passing.

Changes live in the
[static emitter](../../src/jtml/runtime/static_update_plan_emitter.cpp) and
[browser runtime](../../src/browser_runtime_assets.cpp). Existing CLI code-shape
assertions were updated to require string-valued attribute code and generated
form submit metadata instead of the old helper call; CSP checks remain intact.
The new browser oracle checks behavior independently of these code-shape tests.

## Reproduction and negative controls

Use a Release build with C++17/tests enabled and Python bindings disabled, Node,
Python, Playwright 1.63.0 and a compatible Chrome headless shell. Set
`JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT` to the driver directory containing
`node_modules/playwright` and `JTML_BENCH_BROWSER` to the browser executable.
Missing browser/driver support fails the new gate; there is no optional skip.

```sh
ctest --test-dir build -R 'RuntimePlan|BrowserBuild|CliBuild|FriendlyConditional' --output-on-failure
scripts/test_taskboard_core.sh
scripts/test_production_reference.sh
JTML_REQUIRE_BROWSER_BENCH=1 JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1 scripts/benchmark_runtime.sh
JTML_REQUIRE_BROWSER_BENCH=1 JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1 scripts/verify_all.sh
```

The new runner is mandatory in the full local gate and Ubuntu/macOS CI. CI now
installs the same pinned Playwright driver/browser on both systems. Hosted CI
is separate evidence from these local runs.

Saved main compiler: `/tmp/jtml-taskboard-baseline` (ephemeral). Running the exact
new runner against it:

```sh
JTML_BIN=/tmp/jtml-taskboard-baseline JTML_TASKBOARD_OUT=/tmp/jtml-taskboard-negative scripts/test_taskboard_core.sh
```

fails the focused control assertion `select property follows state` (`one` rather
than `two`), exit 1. Taskboard scenarios are UNVERIFIED in that stopped run.
Earlier development reproduction with the original compiler passed C01/initial
invalid submissions but failed valid creation after invalid submission; adding
explicit per-edit state checks made the lost-input defect observable sooner.
Neither the required sequence nor the scenario expectations were relaxed.

A second negative control copied the final built assets to
`/tmp/jtml-taskboard-keyed-negative`, restoring only the old shared runtime for
the Taskboard (the focused control retains its repaired assets):

```sh
node tests/browser/taskboard_core.cjs /tmp/jtml-taskboard-keyed-negative
```

Focused controls/C01/C02 pass; C03 fails `retained C01 rows, controls and registry
generations survive creation`, exit 1. Final unmodified emitted assets pass both
controls and C01–C03. Debug traces of generated output were temporary and restored.

## Validation and limitations

Local macOS arm64, Apple Clang 15, CMake 3.31.3, Node 22.23.3, Python 3.14.6,
Playwright 1.63.0, Chrome Headless Shell 153.0.8010.12:

- Focused CTests: 47/47. Full gate CTests: 346/346.
- Entire production-reference suite and all nine real-browser/asset benchmarks
  pass, with unchanged budgets and clean browser exits.
- `scripts/verify_all.sh` passes with the new oracle, package/refactor/source-map/
  LSP/editor/site checks and live runtime smoke 8/8; no checks skipped.
- Shell/Node syntax, documentation links and `git diff --check` checked separately.
- Initial configure failed because the selected GoogleTest cache path was absent;
  configuration then used the existing pinned cache in the normal `build/` layout.
  This is not a fresh supported-system installation claim.
- Initial focused CTests failed 2/47 on old emitted-string/helper expectations;
  after updating those code-shape expectations and rebuilding, all 47 pass.
- Taskboard `jtml check` passes. **Taskboard `jtml lint` fails** (46 errors/5 warnings
  in the observed run), including lowered component/action-local undefined-name
  reports. The new browser gate does not suppress or count lint as passing.
  Diagnosing that separate tooling gap remains work; accurate original-source
  provenance still depends on S1–S3. No exact-span diagnostic claim is made here.

Asset sizes on the same final fixture source, comparing saved main compiler to
this implementation (uncompressed bytes):

| Fixture / asset | Main | Repaired | Delta |
| --- | --- | --- | --- |
| Taskboard HTML | 316624 | 316624 | 0 |
| Shared runtime | 271879 | 271758 | -121 |
| Taskboard component module | 134680 | 178082 | +43402 |
| Taskboard app bootstrap | 619 | 619 | 0 |
| Retained-control HTML | 87060 | 87060 | 0 |
| Retained-control component module | 28344 | 50150 | +21806 |

More control paths now emit static code rather than helper-backed parts; that
component-code increase is visible. The unchanged nine-fixture budgets all pass;
no new application budget is asserted. The older reference HTML size regression
remains 135374 bytes versus the 70000-byte microfixture comparison. Neither this
Taskboard HTML size nor that comparison is a validated real-application budget.

Local logs are ephemeral `/tmp/jtml-taskboard-*.log`. Remaining blockers include
later Taskboard interactions, ownership/lifecycle edge scenarios, linter accuracy,
source provenance, and complete application/security/installation evidence.
Recommended next implementation: C04's parent-owned edit/save/cancel and validation
path, using the unchanged oracle contract; verify the completion-index invariant
when completion actions arrive. Do not promote the full-composition matrix row
until full T0 acceptance is demonstrated.
