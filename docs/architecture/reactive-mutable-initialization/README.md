# Source-ordered mutable initialization (PR #81)

Baseline: merged #80, `f9bf3e2061fa7c512e7771917cc2836bee9b4be8`.
This correction targets original Reactive Core V11/V12. It does not ratify new
scalar, interpolation or failure policies, or claim full Core conformance.
The authored vectors, oracles and independent evaluator are unchanged.

## Reproduced defect and correction

The exact original V11 source compiled on the baseline, but clean Chromium
retained only `z=2`; `.out` displayed the unresolved interpolation instead of
`3|5`. The name-keyed JSON state dictionary put `a` before `z`. Original V12
also compiled and rendered `never`, instead of rejecting a forward initializer.

Clean production now obtains `stateOrder` from executable module
`RuntimePlanBinding` vectors after scalar storage lowering. Declaration positions
are indexed by defining module ID and lowered binding name. Only dependencies
on earlier state declarations in that same owner receive an ordered certificate.
The browser consumes this explicit order with its existing expression evaluator,
then initializes getters through #80's unchanged `derivedOrder`. Initialization
still assigns state directly, without intermediate DOM commits or action dispatch.

The runtime change moves the existing compiled-state-plan copy before evaluation
and reuses that private table, removing a redundant local lookup table. It adds
no scheduler, evaluator, source execution, or per-assignment DOM update. The
bootstrap still evaluates state entries without compiled plans using its existing
fallback behavior. No rollback or error-recovery policy is introduced.

## Supported subset and compatibility boundary

The source-order guarantee applies to projects whose executable root state has
unique physical slots and whose initializer dependencies are all earlier state
in the defining module. Existing structured literal, unary, binary, conditional,
member, subscript, range, array and object plans are reused; this is not a new
strict-scalar type restriction. Stores and constants remain existing lowered
state. A conditional's statically retained dependencies must all satisfy the rule,
including inactive branches.

Clean compilation rejects local self/later references with source-located
`E_INIT_FORWARD`, getter reads before derived initialization with
`E_INIT_DERIVED`, unsupported plans with `E_INIT_UNSUPPORTED`, and unresolved or
unauthorized references with `E_INIT_UNRESOLVED`. This happens before writing
assets; an existing successful four-asset bundle is preserved byte for byte.
A local initializer cannot gain access to another module's private declaration
through a named component import.

**Imported initialization sequencing (V21) is not implemented.** An authorized
foreign initializer dependency retains the pre-existing initialization behavior
for the **whole project**, represented by `stateOrder: null`. Authorization uses
retained named-import owner identities, including stores, or explicit legacy
whole-file import edges. It does not search unrelated global names or compare
source positions across owners. An imported getter remains rejected. Previously
working explicit-import, barrel and whole-file initializer programs remain
working. V21 still compiles but displays unresolved `{{copy}}` on both baseline
and candidate; this is a known failure, not a conformance pass.

Repeated physical state slots introduced by existing lowering (for example scene
`into` defaults) and linked-only state without retained ownership also retain
whole-project legacy initialization with `stateOrder: null`. In particular, this
PR does **not** guarantee V11 ordering for a project that also crosses one of
these boundaries. Unique local declarations are still checked for forward/self
references; repeated slot records are not reinterpreted as separate lexical
bindings. Extending ordered initialization across these boundaries needs separate
work. This conservative boundary preserves existing applications without claiming
an import scheduler or changing their initialization semantics.

Compat/debug/inline manifests omit the new order and retain their prior behavior,
including their existing V11/V12 failures. This is a clean-browser correction.

## Artifact validation

A clean bundle with state must contain `stateOrder`. An array must contain every
executable state slot exactly once and put every structured initializer state
dependency earlier than its consumer. Missing, reversed, duplicate, incomplete,
and unknown-name arrays reject. This reuses the existing typed-plan dependency
walker, including executable-module filtering. It also validates the effective
state/initializer-plan view using the browser's independent first-wins merges:
root, linked plan, then executable modules in manifest array order. Ordered
certificates reject orphan state-plan entries in any executable projection;
shadowed declaration plans still must satisfy dependency ordering. Legitimate
duplicate projections remain valid. JSON dictionary ordering is not an
initialization certificate.

Null is an explicit **uncertified compatibility boundary**, not proof of ordered
initialization. Artifact validation checks supplied plans; it cannot authenticate
the original source or prove that a manually substituted null marker was emitted
by the compiler. Resolved source ownership/order is the compiler's responsibility.

## Adversarial review and reproducible checks

The review compared baseline and candidate in real Chromium for legacy whole-file
imports, barrel imports, and object literal data shaped like a plan node. All
retain their prior successful DOM values. It caught a whole-file import false
rejection in the first implementation; retaining the actual side-effect module
edges corrected it, with permanent compile/browser regressions. A named-component
import of private initializer state rejects, inactive-branch forward reads reject,
and original V21 retains its known failure.

Run the focused checks from the repository root after building:

```sh
ctest --test-dir build -R 'BrowserMutableInitialization|BrowserActionGet|BrowserDerived|BrowserScalarStorage|BrowserModuleBinding|RootScalarEffects' --output-on-failure
python3 -m unittest discover -s tests/reference_model -v
python3 tests/reference_model/audit.py
# Use installed Playwright/Chromium paths; no dependency changes are required.
JTML_BENCH_BROWSER=/path/to/chromium \
JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT=/path/to/playwright-package-root \
node tests/browser/mutable_initialization.cjs
```

The new browser suite is required in the existing Ubuntu/macOS CI jobs and in
`verify_all.sh`. It reads original V11/V12 and V01/V03/V04/V05 sources/oracles;
tests conflicting lexical/source permutations, two 24-declaration chains,
one-time mutable initialization after an action, self/get/call/unresolved
rejections, preservation of a working bundle, stores/imports and legacy modes.
It requires real Chromium, strict `script-src 'self'`, no browser errors, and
clean shutdown. Existing action reads, derived publication, effects, ownership,
parameter and lifecycle suites remain mandatory. These tests do not establish
complete DOM atomicity beyond their observed paths.

## Payload comparison

Both compilers built the same original V11 source at the same absolute path.
Sizes below use Node `zlib.gzipSync(bytes, {level: 9})`; other zlib versions can
produce different compressed sizes.

| Asset | #80 raw / gzip9 | Candidate raw / gzip9 |
| --- | ---: | ---: |
| Shared runtime | 272,985 / 51,160 | 272,973 / 51,162 |
| V11 index | 3,373 / 793 | 3,396 / 802 |
| V11 components | 1,463 / 520 | 1,463 / 520 |
| V11 app | 2,043 / 902 | 2,043 / 902 |

Runtime delta: **-12 raw, +2 gzip bytes**, leaving **27 raw bytes** under the
unchanged 273,000-byte limit. Component/app bytes are identical. The small
headroom remains a maintenance constraint; it is not grounds to relax the budget
or introduce risky architectural changes.

## Initial PR verification result

The initial local browser-required `scripts/verify_all.sh` passed: 527/527 CTests,
nine performance browser fixtures plus the production reference app with
`browserExit=clean`, the required browser regressions, doctor, examples/demos,
package/site/editor/LSP checks, local-server integration and all eight live-runtime
smokes. No required check was skipped. The frozen website artifact used its
existing pinned compiler `69a2cb7a60bd12887cc434e47d22cce0e8a0a8c3` via
`JTML_SITE_BIN`; it was not rebuilt with altered website contracts.

The independent model's 18 tests and four source/AST audits passed separately;
the focused compiler group passed 20/20. `git diff --check` and Node syntax
validation passed. Chromium was 151.0.7922.34 on macOS arm64. Browser launch and
local ports required host permissions. Existing linter/demo limitations, locale
warnings, the reference-app size warning, and five explicitly retained native
body-plan attribute discrepancies remain in the broader gate's output. Those
catalog observations are not new semantic passes and are not fixed by this PR.
Hosted Ubuntu/macOS CI is separate evidence from this full local gate.

## P2 artifact-validation correction

The adversarial review found that a root initializer plan without a matching root
state entry could escape projection checks while winning the browser's merge.
Starting from original V11, reversing `stateOrder` to `a,z`, deleting root
`state.a` and replacing only module `statePlans.a` with literal 3 passed the old
validator. The browser still selected root `statePlans.a` (`z+1`), initialized it
before `z`, and left `a`, `result` and their DOM unresolved.

The compiler-side validator now checks the effective executable dictionaries and
rejects orphan plans for non-null certificates, in addition to all declaration
projections. The exact mutation now rejects with both an orphan-plan diagnostic
and `z -> a` order violation. No runtime code changed in this correction. Tests
also reject orphan linked plans and contradictory module/linked dependency edges,
accept normal compiler duplicates and redundant identical linked projections,
and ignore non-executable modules as the browser does. Missing/reversed/duplicate/
incomplete order controls remain mandatory. A barrel-imported store containing
structured values retains `stateOrder: null` and renders 8 in required Chromium.

Null remains the documented uncertified compatibility marker. Validation does
not authenticate original source text or prove semantic equivalence of source
strings and plans; this correction validates the graph actually selected from
known executable artifact plans. It does not implement V21 or alter initialization
semantics, the independent model, or authored vector/oracle results.

For the P2 amendment, the original V11 build produces byte-identical HTML,
runtime, component and app assets compared with pinned PR #81 head
`e0f7568b370dfc6268fe66ae91292992bf7d27eb`. Shared runtime remains **272,973 raw /
51,162 gzip9 bytes**, SHA-256
`68c42345a4e3d165e739d381265bb9134970308339c5c70e41811373e13d3b3e`.
The unchanged cap is 273,000 raw bytes (27 bytes available).

Amendment-focused verification: 22 CTests, all 18 independent-model tests and
four source/AST audits pass. Real Chromium initialization, action-read,
derived-publication and root-effects suites pass with clean shutdown. The
existing Ubuntu/macOS job runs the exact malformed-artifact regression through
CTest and the additional barrel-store case through its mandatory initialization
browser step. Final full-gate and hosted-CI results are recorded on PR #81.
