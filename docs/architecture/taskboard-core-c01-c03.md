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

Initial-slice asset sizes before the lifecycle correction, comparing saved main compiler to
that implementation (uncompressed bytes):

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

## Lifecycle correction and independent pre-merge review — 2026-10-18

Runtime head reviewed: `13bf65f584c32918fda185c059c7bf0f179e7933`.
**Historical assessment at a74d88c: NOT READY (superseded by bounded commit recovery below).** The two original P1 reproductions pass, but the
failed-commit recovery boundary below remains blocking for the requested failure
contract. No C04 work or further runtime rewrite was undertaken during this review.

### Original P1 corrections

P1-A previously mutated retained descendants before discovering an unsupported
patch, then destroyed registrations in wrapper fallback. P1-B matched outer
identities without destroying omitted nested children. Reconciliation now renders
a detached candidate, plans every descendant before connected DOM changes,
rejects ambiguous keys/identities and unsupported retained shape changes, restores
tentative registrations/scopes/generations on rendering/preflight rejection, and
destroys omitted identities deepest first before committing the planned edits.
Reconciliation rejections are flagged and escape the full-render fallback.

The mandatory [browser oracle](../../tests/browser/keyed_lifecycle.cjs) and
[runner](../../scripts/test_keyed_lifecycle.sh) preserve both regressions: repeated
empty/nonempty text updates, retained input/counter identity and focus, conditional
hide/reorder/remount with fresh state/generations, genuine removal, overlapping
board keys, stale-handler rejection, and failed second-row preflight after an
earlier sibling was examined and a new child registered tentatively. Both pass
on `13bf65f`; the unchanged oracle fails both cases on reviewed head `7b8728d`
(retained rows/drafts for P1-A; disconnected registrations for P1-B).

Independent rendering-failure injection wrapped generated child `create` functions
before runtime initialization, captured actual nested scope references/values,
then threw after a child finished rendering during Insert. The surfaced error was
`Keyed reconciliation in Board: independent render rejection`; captured nested
scope objects and fields, connected DOM and registry were restored exactly. The
snapshot is shallow: supported render expressions assign scope fields and replace
metadata, rather than mutating borrowed nested objects. This does not establish
transactional safety for host getters or other reentrant host mutations, and it
does not roll back the parent action's already-applied state writes.

### Blocking failed-commit recovery reproduction

The commit closures run outside the rendering/preflight `try/catch`, after omitted
instances have been destroyed. Their exceptions lack `jtmlReconciliation`, so
`compileStaticComponentUpdateFunction` converts them to an unhandled update and
permits full-render fallback after partial mutation. There is no commit undo path.

This is **fault-injection evidence**: no native commit exception from unmodified
JTML source has been established. It specifically tests the requested failed-commit
boundary; ordinary C01–C03 and the two corrected P1 browser cases still pass.
The supported synchronous native-DOM path has no demonstrated commit throw:
references are selected from the current parent at insertion, candidate nodes are
detached, and generated attribute names are compiler-owned. Host method overrides
and reentrant mutations are not covered by that argument.

Smallest reproduction uses the existing clean-production `partial` fixture:

```sh
scripts/test_keyed_lifecycle.sh
```

Open `dist/keyed-lifecycle/partial/index.html` in a browser. Increment A's row `a`
to 1 and type `Unsaved` in its draft. Before clicking A's Update, execute:

```js
const insertBefore = Node.prototype.insertBefore;
Node.prototype.insertBefore = function (child, reference) {
  Node.prototype.insertBefore = insertBefore; // one failure only
  throw new DOMException('injected commit failure', 'NotFoundError');
};
```

Observed after Update: retained rows are replaced, A's draft resets to `Seed`,
both counters reset to 0, and A's two row wrappers share `Board_3__Row_19` (their
Counter wrappers also share one ID). Across both boards there are eight visible
component wrappers but six dynamic registrations. Clicking Increment in row `a`
changes row `b` to 1 while `a` stays 0. No `pageerror` is surfaced;
`directComponentStaticUpdateError.message` records `injected commit failure`.
Expected failure handling must preserve ownership/identity and surface the error;
falling back after a partially committed tree cannot satisfy that requirement.

The browser probe/log are `/tmp/jtml-pr12-commit-fault.cjs` and
`/tmp/jtml-pr12-commit-fault.log` (ephemeral). Recommended next implementation is a
bounded correction to failed-commit handling with this negative browser regression;
do not proceed to C04 or claim arbitrary transactional/reentrant DOM support.

### Dynamic attribute audit

No confirmed attribute regression was found in the inspected production paths.
Value-bearing ARIA booleans render as strings `"false"`/`"true"`; native `disabled`,
`checked` and `required` remain presence attributes. They are not dynamic boolean
expressions in this syntax; `disabled false` does not express conditional omission.
Direct JS expression wrappers normalize nullish results before `String(...)`.
Generated `create` was also invoked with actual JS null and undefined member
values: `title`/`aria-label` were omitted, `value=""` remained, and ARIA true was
`"true"`. Repeated Seed/nullish/missing-member/Seed updates produced no
`"null"`/`"undefined"` attribute text, no fallback and no browser error.

Comparison with saved main `b4b7501` preserved native boolean and ARIA behavior.
That older helper path kept empty `title`/`aria-label` attributes for missing
members; the current static path omits them. Empty `value` intentionally remains
to clear live controls. This is the existing static attribute-map policy, not a
new null-to-string regression. No emitter/runtime change was justified by these
probes. Non-direct literal/path plan branches were inspected separately; they are
not evidence of an end-to-end supported source shape emitting nullish strings.
Probe sources/logs: `/tmp/jtml-pr12-attribute-update.jtml`,
`/tmp/jtml-pr12-attribute-update.cjs`, `/tmp/jtml-pr12-attribute-update.log`, and
`/tmp/jtml-pr12-independent.cjs` (ephemeral).

### Actual validation and sizes

- Fresh review rerun at `13bf65f`: focused CTests 47/47; both lifecycle fixtures
  PASS; retained-control and C01/C02/C03 PASS. Existing scenario tests unchanged.
- Recorded final runtime full gate: 346/346 CTests; complete production reference;
  all nine browser/asset benchmarks with clean exits; package/refactor/source-map/
  LSP/editor/site checks and live smoke 8/8 PASS, no checks skipped. Log:
  `/tmp/jtml-pr12-verify-all-final.log`. This does not override the fault-test FAIL.
- Earlier full gate `/tmp/jtml-pr12-verify-all.log` aborted at live smoke 4/8,
  `examples/friendly_store.jtml`, exit 134 / Abort trap 6, with an empty store log.
  A settled-build rerun passed 8/8. Cause remains unestablished; record this as an
  intermittent smoke failure, not a diagnosed or fixed defect.
- Shared runtime: **271758 → 269151 bytes (-2607)** from reviewed `7b8728d` to
  `13bf65f`; unchanged 272000-byte budget. Taskboard HTML 316624, components
  178082, bootstrap 619 bytes unchanged. Lifecycle partial HTML/components:
  109360/99032; nested: 105426/99594; both bootstrap 619 bytes. No budget raised.
- [Ubuntu/macOS CI on runtime head 13bf65f](https://github.com/mrjohnnyrocha/jtml/actions/runs/39130173055)
  passed both platforms, including mandatory lifecycle and Taskboard browser runs.
  CI does not exercise the failed-commit injection or the full local smoke gate.

All 32 acceptance scenarios and support-matrix evidence references remain intact.
C04–C14, N01–N08 and T1/T2 remain UNVERIFIED. Linter accuracy (observed 46 errors /
5 warnings), S1–S3 source provenance, general slot behavior, and complete T0
application acceptance remain limitations. The full T0 claim is not supported by
these selected passes; mandatory preview gates remain independent of T0 completion.

## Bounded commit recovery — 2026-10-18

This supersedes the failed-commit blocker recorded at `a74d88c` above. The correction
is confined to the keyed DOM commit path and its browser oracle; no C04, semantic
conformance, source-span or linter implementation was added.

### Trace and support decision

The original one-shot `Node.prototype.insertBefore` probe reproduced on saved
`a74d88c`. Rendering first tentatively registers new children/refreshes scopes.
Planning changes no connected DOM. Previously omitted registrations were then
destroyed, nested edits were applied, and the insertion threw outside the rollback
catch. The unflagged exception entered static-update fallback, replacing retained
rows and generating duplicate identities and misrouted handlers.

A new late-region probe exposes the timing independently: after nested edits but
before/after the native row insertion, partial Insert has 10 tentative registrations
(8 old plus the new row/Counter), while nested Update has already destroyed two
Counters and has 6 registrations on the old runtime. After fallback, the old
partial fixture has 10 wrappers but only 6 unique IDs/registrations; nested has 6
wrappers but only 5 unique IDs/registrations. All four cases fail the new oracle.

No supported-program native commit exception has been demonstrated. Within the
inspected native-element path, desired new nodes are detached; retained nodes are
direct children of the planned parent; insertion references are read from that
parent immediately before insertion; rendering rejects duplicate identities and
unsupported retained structural changes before commit; and attribute names come
from the compiler. No asynchronous work interleaves those operations. Invalid
attribute syntax used in an exploratory probe was rejected by the parser.
This argument is bounded to that path, not a proof about arbitrary host code,
custom-element reactions, memory exhaustion or external DOM interference.

The accepted recovery contract is a synchronous exception during a planned native
DOM edit, including a throw after the edit has taken effect, **provided subsequent
native DOM operations remain usable and host code does not independently mutate
the managed tree**. Host overrides are the deterministic test mechanism, not a
new public interop surface. Persistent throwing methods, adversarial getters,
reentrant tree edits and arbitrary custom-element/host side effects remain outside
this contract. Recovery failure is surfaced explicitly with `recovery failed`;
there is no claim that such hostile mutation leaves a consistent tree.

### Small implementation

Each applied edit records its inverse before mutation: the prior text value,
prior attributes on a shallow detached element, or the prior child-node references
and order. On failure, the journal is replayed in reverse, then the existing
scope/instance/generation/list-metadata snapshot is restored and retained focus /
selection recovered. Undo continues if one inverse fails, and the original error
plus recovery failure is reported. No replacement HTML or cloned live component
subtree is mounted during recovery.

Only after the full DOM commit and retained-focus restoration succeed are omitted
old registrations destroyed, deepest first. A wrapped Error carries the original
exception as its cause and `jtmlReconciliation`, preventing full-render fallback.
Wrapping is necessary because `DOMException.message` is read-only. This is a local
undo list, not a general transaction framework; parent action state writes remain
applied, and the regression verifies that directly retrying that same action works.
The earlier shallow scope snapshot and pure-render limitations still apply.

### Independent real-browser regression

[Commit failure oracle](../../tests/browser/keyed_commit.cjs) runs through the
existing [mandatory lifecycle runner](../../scripts/test_keyed_lifecycle.sh),
therefore in both CI platforms and the full local gate. It uses both unchanged
lifecycle fixtures, injecting exactly one late keyed-region insertion exception
before and after the actual native insertion. Partial Insert exercises tentative
new registrations; conditional hide/reorder exercises omitted children after prior
DOM removal. Each case requires:

- Evidence that the fault point was reached after earlier DOM edits, with no old
  destruction at that point on the repaired runtime.
- Exactly one surfaced reconciliation error and no wrapper-fallback telemetry.
- Exact previous DOM serialization and connected element references, scope-related
  list metadata, registry owners/generations and no duplicate component IDs.
- Draft `Unsaved`, focus and selection retained; A's Counter increments 1→2 exactly
  once, while B and the other row remain unchanged.
- Successful direct retry: new row/Counter generations start at 1, or removed
  Counters are destroyed and later remount fresh at 0; final DOM/registry bijection.

All four pass after repair. The unchanged final oracle fails all four against
saved `a74d88c` (expected exit 1); both original P1 fixtures still pass on that
negative control, separating the remaining boundary from the earlier repairs.
Earlier independent `7b8728d` P1 negative controls and main/old-runtime C01–C03
controls remain recorded above and unchanged. The original lifecycle oracle,
Taskboard oracle/fixture, all 32 acceptance requirements and support matrix are
byte-for-byte unchanged by this correction.

### Validation, failure evidence and assets

Focused CTests pass 47/47. The final full gate passes 346/346 CTests, the complete
production-reference suite, retained-control and C01–C03, both original lifecycle
regressions, all four commit-failure cases, all nine required browser/asset
benchmarks with clean exits, package/refactor/source-map/LSP/editor/site checks,
and live smoke 8/8. No checks were skipped or budgets raised.

The first candidate full gate failed at `friendly_store` live smoke 4/8, status
134 / Abort trap 6, again with an empty store log. Unlike the earlier run, macOS
provided a crash report: SIGABRT in `malloc`'s `free_list_checksum_botch`, reached
while `FunctionDeclarationNode::toString()` was executing. This identifies a heap
corruption detection point, not the corrupting operation. It does not establish
that the browser journal caused or repaired the native issue.

A separate five-start comparison reproduced a native startup crash on the saved
**pre-change `a74d88c` binary** (4/5 ready, one SIGSEGV / exit -11, empty log);
the candidate was ready 5/5. The settled candidate full-gate retry passed 8/8.
This independently confirms a pre-existing intermittent native startup defect;
its corrupting operation remains undiagnosed, and the successful retry does not
make it fixed. No retry, skip or smoke assertion was added to hide it.

Local evidence is ephemeral: `/tmp/jtml-pr12-journal-focused.log`,
`/tmp/jtml-pr12-journal-verify.log` (failed),
`/tmp/jtml-pr12-journal-verify-retry.log` (passed),
`/tmp/jtml-pr12-commit-negative-final.log`,
`/tmp/jtml-pr12-commit-positive-final.log`, and
`/tmp/jtml-pr12-store-probe.log`. Crash reports were
`jtml-2026-10-18-154102.ips` and `jtml-pr12-a74d88c-2026-10-18-154231.ips`
in the local macOS diagnostic directory.

Shared runtime grows **269151 → 270406 bytes (+1255)** from `a74d88c`, still below
the unchanged **272000-byte** budget and **1352 bytes below** reviewed `7b8728d`.
Taskboard HTML/components/bootstrap remain 316624/178082/619 bytes; partial
lifecycle HTML/components 109360/99032 and nested 105426/99594, bootstrap 619.
No static emitter or component-module code changed for this correction.

The one-shot commit-failure blocker is repaired. The scoped merge assessment is
**MERGE READY**, subject to final-head Ubuntu/macOS CI confirmation in the PR.
That assessment distinguishes the repaired fault-injection-only boundary from the
pre-existing native startup defect, which remains visible follow-up work. General
host transactions, later Taskboard scenarios, linter/source provenance and full
T0 support remain unverified. Do not merge automatically.
