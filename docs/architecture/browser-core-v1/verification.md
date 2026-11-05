# Browser Core component-order admission verification

Base: remote merged main `bc5350db56220c3c5f68993ed22711ba1c1a883c`, checked
twice during work. Sources and artifacts are isolated in
`/private/tmp/jtml-browser-core-v1` (`/tmp` is the macOS alias). The original
checkout is unchanged. No PR #89 source/branch is included.

## Audit and contract reconciliation

All 24 original source-file maps were built unchanged. Accepted programs ran
in strict-script-CSP Chromium 151.0.7922.34. [baseline.json](baseline.json)
retains actual state/DOM/show observations, diagnostics, hashes and supplemental
probes. Final classifications: 15 bounded PASS, seven REJECTED BY DESIGN and
two PENDING POLICY (V19 numeric/failure behavior; V22 import authority).

Initially V22 was called a confirmed target mismatch after actual Step changed
imported seed 7 -> 9. The observation and first classification are retained.
Full-gate inspection subsequently found action_get_reads.cjs explicitly requires
named/reexported import writes. The proposed E_IMPORT_WRITE guard failed that
unchanged oracle and was removed. No existing import-write test, oracle or
production contract changed. The full-profile audit remains nonzero while that
conflict and V19 are pending; it is not a waived green CI check.

The six existing root read/publication/initialization/effect/capture suites pass
on baseline. Their limits on logical commits and arbitrary observer atomicity
remain explicit.

## Selected prerequisite: incorrect output becomes a located rejection

- Original component-forward-chain.jtml compiles on baseline, renders
  `z * 2.000000000000000|2` initially and `4|3` after Step, rather than `4|2`
  and `6|3`. Exact source and observed scope are preserved independently.
- The new targeted browser suite exits 1 against the baseline compiler because
  this wrong-output source is accepted. That negative is not an acceptance pass.
- Candidate compilation rejects at component-forward-chain.jtml:4:3 with
  E_COMPONENT_DERIVED_ORDER, component context and dependency-order remediation.
  No runnable assets appear; a previous four-asset build remains byte-identical.
- Moving z before a is the supported remediation. Strict-CSP HTTP Chromium
  verifies `4|2` -> `6|3` -> `8|4` -> `10|5`, an independent sibling fixed at
  `4|2`, retained DOM and clean browser/server exit, without injected state,
  mocked API, unexpected errors or CSP violations.
- 21 focused CTests pass. New controls cover forward/self/cyclic/inactive-arm
  edges, property-name identity, root graph ordering and unchanged compat/debug.

Only retained component binding/read metadata is checked in clean production.
No JS runtime code or scheduler changes. Component action-read freshness remains
a separate reproduced defect.

Original catalog SHA-256:
`964a220479561c77ab1f428c570f64c8eb5a75fa10691cc81087672818c27e8f`.
Shared runtime remains 272217 bytes, SHA-256
`fb3aaa87300a3800b190e87f84c81b52870c12f4fa8f16946e4bda13f59f5d5d`:
783 bytes below the unchanged 273000-byte cap. Existing oracles are unchanged.

## Reproduction

Configure real Chromium/Playwright through the repository's existing
JTML_BENCH_BROWSER and JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT variables:

```sh
cmake -S . -B build -DJTML_BUILD_TESTS=ON -DJTML_BUILD_PYTHON=OFF
cmake --build build --parallel
ctest --test-dir build -R 'BrowserComponentDerivedAdmission|BrowserDerivedPublication|BrowserScalarStorage|BrowserMutableInitialization' --output-on-failure
node tests/browser/component_derived_order.cjs
JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1 scripts/verify_all.sh
# Intentionally nonzero while the proposed profile has pending policy.
node tests/browser/browser_core_v1.cjs
```

JTML_BIN selects the baseline compiler; JTML_COMPONENT_ORDER_OUT separates its
negative artifacts. The targeted suite is mandatory in CI/full gate. The full
profile audit reports mismatches without waivers.

## Retained failed attempts

The first full gate stopped at 534/536 CTests: two exact-source-path assertions
compared `/tmp/...` fixture definitions with `/private/tmp/...` canonical
diagnostics. No assertions changed. A local CMake project include canonicalizes
only the generated JTML_SOURCE_DIR fixture-root definition using file(REAL_PATH).
Hosted ordinary checkouts do not need this configuration. Log:
`/tmp/jtml-core-full-gate.log`; include `/tmp/jtml-core-physical-source.cmake`.

The second gate correctly rejected the withdrawn import-write guard against
action_get_reads.cjs's existing named-write positive. Log:
`/tmp/jtml-core-final-gate.log`. This was a real contract conflict, not an
infrastructure failure. The final slice preserves that source/test verbatim.

The website compiler stays independently pinned to
`69a2cb7a60bd12887cc434e47d22cce0e8a0a8c3`. Final complete-gate results are
recorded below; exact-head hosted CI is separate evidence.

## Final complete local gate

The final component-order candidate passes scripts/verify_all.sh, exit 0:
536/536 CTests; all nine required browser performance fixtures plus the
production reference with clean exits; targeted component-order acceptance;
unchanged Core/module/input/route/CSP/lifecycle suites; ReleaseDesk's five real
HTTP/SQLite API tests and browser filtering/persistence/failure/recovery;
Taskboard C01-C14 and negatives; editor/LSP/refactor checks; pinned website,
static equivalence, package/relocation/C API checks; live smoke 8/8. No
verification check skipped and no budget/assertion was relaxed.

Log `/tmp/jtml-core-order-full-gate.log`, SHA-256:
`349c872798ea1f3ba722ee9861a68b43d5040f9589bd4df79279d6fb1835907c`.
The independently retained native five-attribute discrepancy observations and
reference HTML warning (100027 versus 70000 microfixture bytes) remain explicit
failures/limitations, not new semantic passes. Locale warnings also remain.

The separate full-profile audit exits 1 as required, reports V19/V22 pending,
and retains the V22 catalog mismatch. [candidate.json](candidate.json) records
the complete classification list and targeted browser results. A separate
owner-action control passes alongside the 15 original passing vectors. V11's
four generated assets remain byte-identical to baseline. Hosted exact-head
Ubuntu/macOS CI is pending PR execution, rather than inferred from this run.

## Readiness

NO-GO for Browser Core v1: component action freshness, V22 contract reconciliation
and unapproved failure policy remain. The prerequisite establishes a fail-closed
component getter-order boundary; it does not promise a general component graph,
rollback or read-only imports.
