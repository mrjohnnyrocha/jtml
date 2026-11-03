# Root derived publication: V04/V05

Baseline: `8cf30256fb59fe819bf14fa418504d4fdc905ca0`, the merge of PR #79.
Both Ubuntu/macOS CI jobs on its final head `dda8b5b` succeeded before merge.
This correction covers root derived initialization and publication in clean
browser builds; it is not full Reactive Core conformance.

## Cause and bounded correction

`applyClientDerived` previously traversed the name-keyed derived object once.
JSON objects serialize in lexical key order, so V04 evaluated `a` before `z` and
V05 evaluated `atotal` before its two inputs. The action-end DOM update saw
cached values from different state versions. Repeated render passes are not a
valid dependency scheduler, and action-local lazy reads from PR #79 do not fix
publication.

The clean compiler now certifies all root getter closures and emits one
`derivedOrder` list. Its DFS visits dependencies before appending each consumer;
vertices and cycle detection use defining module ID plus declaration identity.
Dependencies resolve in that owner or through explicit value imports. The
existing module guard still rejects unsupported shared value slots before order
construction. The serialized names identify unique already-certified physical
slots; linked JSON spelling is never used to recover lexical ownership.

The existing PR #79 proof also checks action getter reads. That stronger scalar
closure check runs first, then the publication traversal reuses its completed
vertices and visits remaining getters. Existing structured array/object getter
publication remains supported; this does not expand action-local scalar reads.
Cycles (including conditional dependency cycles), unresolved/unimported roots,
ambiguous bindings and unsupported expression plans reject before assets are
written, even without an action reading the offending getter. Previous output
is preserved. No source scanner, dependency package or second browser scheduler
is introduced.

The client manifest carries the order after clean certification; it is required
when a clean production bundle has root getters. No-getter bundles and
compat/debug/inline/live output may omit it. The closed output validator checks
its type, unique existing names, complete coverage and every dependency position
using the existing structured expression-plan walker. Both conditional branches
and all executable module projections are checked; independent components may
use any valid topological permutation. The executed path segments determine
roots, not diagnostic source/root hints. Literal application payloads are not
walked as expression nodes.
The browser uses this order in its existing derived pass, both at bootstrap and
at each existing action/effect flush. Each formula still uses the existing
structured expression plan. Derived cache writes finish before DOM work starts;
there is no per-assignment DOM flush. The tests compare actual values read back immediately after selected
`Element.textContent` setters with each independently expected phase projection,
in addition to checking cached source/derived values. This evidence covers the
single dynamic text projection inside each tested `.out` after runtime bootstrap.
It does not instrument attributes, `nodeValue`, HTML insertion, pre-bootstrap
markup or arbitrary host effects, and does not establish complete DOM atomicity.

Compat/debug and inline/live artifacts receive no order metadata and retain
their previous traversal. Parameter restoration, lazy action reads, subscription
ordering and the action-end DOM boundary are unchanged. No rollback, instance
faulting or recovery policy is selected; A6c remains pending.

## Reproducible evidence

With the existing required Chromium/Playwright environment configured:

```sh
ctest --test-dir build -R 'BrowserDerivedPublication|BrowserScalarStorage|RootScalarEffects' --output-on-failure
node tests/browser/derived_publication.cjs
node tests/browser/action_get_reads.cjs
node tests/browser/root_effects.cjs
node tests/browser/module_scalar_storage.cjs
node tests/browser/module_binding_collisions.cjs
python3 -m unittest discover -s tests/reference_model -v
python3 tests/reference_model/audit.py
```

`derived_publication.cjs` extracts original vector source and independently
authored expected values from `vectors.json`, without modifying either. It
asserts initial and post-action state, getters and DOM for V01/V03/V04/V05.
Instrumentation observes actual text and cached values at the selected setters,
including initialized startup. Each phase uses authored or independently computed
expected values, never values inferred from the observed caches. A required
negative control temporarily changes the generated root renderer to write `X`
before the correct text. The final DOM and getter caches remain correct, but the
same publication assertion must reject `X`. The synthetic asset does not grow,
production code and budgets are unchanged, and the original asset is restored. Four source/key-order permutations exercise a 12-getter chain feeding
a diamond and another consumer, with four repeated actions. An explicit imported
getter crosses module order; existing structured collection getters and
compat/debug behavior have separate controls. Browser errors/CSP violations
fail, and browser shutdown is awaited. Results include the actual browser
version and raw/gzip runtime measurements.

| Original vector, first action | PR #79 baseline | Corrected browser |
| --- | --- | --- |
| V04 (`a\|z`) | `4\|3` (FAIL) | `6\|3` (PASS) |
| V05 (`left\|right\|total`) | `3\|4\|4` (FAIL) | `3\|4\|7` (PASS) |

The new V04 test was executed against the exact saved baseline compiler and
failed at `4|3` versus `6|3`, with clean Chromium exit. Pinned PR #77 browser
observations, specification status and all model fixtures/oracles remain
historical and unchanged. The independent model's 18 tests and four audits pass
separately; none of this relabels historical executions as conformance passes.

For the identical V04 source (gzip: Node zlib level 9):

| Asset | Before raw/gzip B | After raw/gzip B |
| --- | ---: | ---: |
| Shared runtime | 272886 / 51127 | 272985 / 51160 |
| HTML + manifest | 4675 / 963 | 4700 / 978 |
| Components | 1463 / 520 | 1463 / 520 |
| App bootstrap | 2043 / 902 | 2043 / 902 |

Components and app bootstrap are byte-identical. Runtime grows 99 raw / 33 gzip
bytes and remains below the unchanged 273,000-byte limit. The compiler reuses
its existing getter proof rather than duplicating graph evaluation in JS. The
order metadata accounts for the HTML difference. CI requires the new browser
test in the existing Ubuntu/macOS jobs; the local full gate also requires it.

## Remaining limits

- Component-local derived scheduling and implicit component-to-module reads are
  unchanged; this is a root publication correction.
- Mutable initializer source/JSON ordering (V11/V12), imported initialization
  (V21), general unresolved action reads, and full Core phase/lifetime coverage
  remain separate work. Root ordering assumes its mutable inputs initialized
  successfully; no new initializer semantics are inferred.
- Derived/module storage collisions outside the bounded scalar isolation
  contract still fail closed. Explicit imports are not general global access.
- Conditional dependencies use conservative static edges; cycles reject even
  if a branch happens to be inactive. No dynamic graph or effect scheduling
  policy is added.
- Failure-path atomicity, strict scalar/coercion policies and A6c are not ratified
  or implemented. Effects and imperative `show` retain existing behavior.

## Original local gate at `7994c73`

The final complete `scripts/verify_all.sh` run passed: 521 CTests, doctor,
examples/demos, nine mandatory clean-exit performance browser fixtures plus the
reference application, all required browser suites, editor/LSP/refactor checks,
site/static equivalence, package/relocation/C API checks and all eight live smoke
checks. No required check was skipped. Use the existing pinned site compiler:

```sh
JTML_BUILD_JOBS=8 scripts/build_site_compiler.sh
JTML_SITE_BIN="$PWD/build/site-compiler/69a2cb7a60bd12887cc434e47d22cce0e8a0a8c3/build/jtml" \
JTML_REQUIRE_BROWSER_BENCH=1 JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1 \
scripts/verify_all.sh
```

The first complete attempt stopped in the native HTTP/WS concurrency check with
`ConnectionResetError`. The exact baseline and unchanged candidate then both
passed that isolated check. One full retry passed without server/test changes;
the cause of the first reset is not established and is not claimed fixed.
Existing keyword-linter limitations, reference HTML size warning (100,009 versus
70,000 microfixture bytes), host locale warnings and five reviewed native
body-plan complete-attribute discrepancy observations remain explicit. Those
catalog observations are not newly passing Core semantics. Hosted Ubuntu/macOS
CI validates the committed candidate separately from this local gate.


## Adversarial corrections after `7994c73`

The initial all-getter pass incorrectly treated the Core value import kinds as
exhaustive. A retained `store` import now resolves, for publication only, to its
explicit defining module's lowered object binding. `shop.x` is an external state
member dependency, not an edge to an unrelated root getter named `x`. No store
name/global lookup is guessed. Missing imports and callable imports remain
unauthorized, and #79's stronger scalar action-read certification is unchanged:
a getter closure reaching an imported store is still outside that action-read
subset. This is restoration of an existing clean-browser publication contract,
not new store semantics or imported write authority.

The mandatory `imported-store-getter` fixture preserves the pre-PR browser trace
`2|1`, `3|2`, `4|3`, `5|4` through actual repeated UI dispatch. CTests separately
verify a getter named like the store member, unauthorized store access and the
unchanged action-read rejection boundary.

Production output validation now rejects missing, reversed, duplicate and
incomplete orders. The existing plan walker checks structured child shapes and
collects executed root segments, so erasing a child or replacing a path with an
unowned root cannot hide an ordering dependency. This is a compiler-side
constraint check, not another scheduler or source-string evaluator. Compat/debug
continue to accept their intentionally absent order. Tests include an alternate
valid disconnected-component order, inactive-branch edges and literal data that
resembles a plan.

Artifact boundary: the validator checks the typed plans and declarations present
in the bundle; original source/module-import authority is certified from retained
IR during compilation. It does not authenticate arbitrary rewritten assets or
reconstruct original import authority from untrusted JSON. The browser still
trusts its checked bundle; editing assets after validation or bypassing validation
is outside the certification claim. No runtime graph repair/checker was added.

These amendments change no emitted runtime bytes: `7994c73` and the corrected
candidate both measure **272,985 raw / 51,160 gzip bytes** (Node zlib level 9),
leaving 15 raw bytes below the unchanged limit. Relative to pre-PR main, the
original +99 raw / +33 gzip delta remains unchanged. Original V01/V03/V04/V05
sources, oracles, model fixtures and pinned historical observations are untouched.


Final amendment verification: all **523 CTests** passed, including 16 focused
ownership/action/effect/publication tests. The new imported-store browser trace,
actual-value observation and required transient-write negative control passed
with clean exit; all existing action-read, module ownership/storage and root
effect controls also passed. The independent model's 18 tests and all four
audits passed separately. The complete browser-required, clean-exit local gate
passed in one uninterrupted run for these amendments, including nine performance
fixtures plus reference app, pinned site, packaging/relocation and eight live
smoke checks. No required checks were skipped. The reviewed native attribute
observations and existing size/linter/locale warnings remain unchanged; the
historical reset above was not reproduced in this run and is not claimed fixed.

The emitted runtime is byte-identical to `7994c73` (SHA-256
`c2c5a083257499d736c8184a65604e1772a7aa88d373227faf8ef9be3260807a`).
Store-derived member semantics, imported-store getter reads inside the stronger
#79 action subset, component scheduling, initializer gaps and A6c remain outside
this correction. Final Ubuntu/macOS CI results are recorded on the PR, separately
from local verification.
