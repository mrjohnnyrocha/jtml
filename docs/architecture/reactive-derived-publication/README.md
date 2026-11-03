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

The client manifest carries the optional order only after clean certification.
The closed schema checks its type, unique existing names and complete coverage.
The browser uses this order in its existing derived pass, both at bootstrap and
at each existing action/effect flush. Each formula still uses the existing
structured expression plan. Derived cache writes finish before DOM work starts;
there is no per-assignment DOM flush. The successful synchronous publication
path exposes coherent caches at every observed DOM text write. This does not
promise a transaction for arbitrary host effects or external DOM observers.

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
Instrumentation observes cached values at actual DOM text writes, including
startup; expectations are calculated independently, never from those observed
caches. Four source/key-order permutations exercise a 12-getter chain feeding
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

## Complete local gate

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
