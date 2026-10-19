# Taskboard T0 negative validation — N01–N08

Base: main `bed8741` (merged PR #17). The approved 32-scenario contract,
support matrix, C01–C14 browser oracles, both original lifecycle oracles and exact
scope-leak fixture are unchanged. **T0 remains incomplete: N04 is BLOCKED.**

| Scenario | Result | Independent evidence |
| --- | --- | --- |
| N01 | PASS | Duplicate, missing, null, numeric, empty, whitespace, array and object IDs reject visibly before canonical writes. A valid-first/invalid-second batch cannot partially commit. Map/object references, values, order, next ID, DOM and generations remain unchanged. A valid equivalent batch is the positive control; duplicate titles remain valid. |
| N02 | PASS | An explicit ID-change request is rejected. Delete 103, then supply 103 again: the issued-ID ledger rejects reuse without changing surviving records, object references, order, registry or allocation. Native creation also consults the ledger. |
| N03 | PASS | Actual Taskboard property/index/owned-alias writes, independent selected-ID/suffix/fallback/guard changes, equivalent root replacement, retained unsaved draft/caret/counter and subsequent single submit. A separate two-owner fixture exercises arithmetic operands, logical conditions, inactive branches and nested attributes. Required roots are independently asserted in Expression IR, Runtime Plan and generated update selection. |
| N04 | **BLOCKED** | Five unsupported ownership/scope/call shapes plus the unchanged original scope leak reject before runnable assets, with component/remediation evidence. All five audited cases emit conflicting linked/module coordinates. Strict original-provenance acceptance exits **2**; no S1–S3 completion is claimed. |
| N05 | PASS | Native create/title-edit/note-submit/remount round-trips quotes, ampersand, angle brackets, script-closing/hostile image text, actual U+2028/U+2029, non-ASCII text, emoji and literal backslashes. Exact canonical/input/text equality; no injected element, execution or request. This application exposes no data-controlled URL. |
| N06 | PASS | Twenty Open/All cycles, twenty completion/filter cycles and twenty Help lifetimes. Exact expected 26 live dynamic instances/eight emitted-event owners at rest, three-component removal, fresh remount generations, 40 single-dispatch note saves, unchanged delegated listener counts and inert detached controls both before and after ID remount. |
| N07 | PASS | Unrelated row/fallback writes retain exact input DOM, backward multi-character selection, focus and drafts. Filtering moves focus to Open; remount leaves it there. Deletion selects next/previous edit controls. Existing C14 covers empty-board create focus. |
| N08 | PASS | Real HTTP response CSP: `default-src 'none'; script-src 'self'; style-src 'unsafe-inline'; connect-src 'none'; img-src 'none'; base-uri 'none'; form-action 'none'`. Exactly four assets, formal prod mode, generated initial creation, no live inline event handlers, legacy globals/assets, dynamic compilation, patch fallback, errors, CSP violations or unexpected activity. |

The [browser oracle](../../tests/browser/taskboard_negative.cjs) supplies independent
JSON data through a native action's serialized argument boundary, never by editing
canonical state or invoking private runtime functions. Actual JTML in
[TaskBoard](../../tests/fixtures/production/taskboard-core/index.jtml) owns validation,
scalar record copies, canonical records/order and the issued-ID ledger. Its ID
domain matches the application's `t-<digits>` allocator; IDs are never titles or
positions. The extra panel is explicitly a test-data boundary, not HTTP/import/
persistence functionality or a general external schema service.

[Dependency browser checks](../../tests/browser/taskboard_dependencies.cjs) and
`RuntimePlan.AdversarialTaskboardDependenciesCoverDynamicOperands` use independently
specified bindings/values, not emitted read sets as expected results.

## Generic defects and repaired keyed regression

1. **Structured invocation tokenization:** component word splitting treated spaces
   inside nested array/object action arguments as separators. The rendered action
   was truncated to `receive([{`; clicking did nothing. Static splitting now tracks
   bracket depth. Runtime word splitting reuses its existing quote/bracket-aware
   comma scanner with a whitespace mode, removing duplicate scanning code. The
   [minimal action fixture](../../tests/fixtures/production/action-arguments/index.jtml)
   passes clean and explicit compat browser tests. Pure emitted-scanner checks cover
   nesting, quoted spaces, escapes, commas and existing bare-word behavior.
2. **Deferred startup:** a deferred runtime executes while the document is
   interactive, before the later deferred component module. Immediate bootstrap
   selected generic initial rendering and serialized structured arguments as text.
   Deferred initial startup now waits for DOMContentLoaded, after the declared
   assets. Inline live callers keep their existing ready-state behavior. This is
   the supported emitted asset layout, not a promise about arbitrary host script
   insertion or reordering.
3. **Static keyed creation:** that startup correction exposed a latent compiler
   defect. Generated `for` child creation omitted the scoped render path already
   used by reconciliation. The first full gate failed the unchanged `partial` and
   `nested` lifecycle oracles: rows shared IDs such as `Board_3__Row_19`, leaving
   duplicate wrappers with one registration. A new two-board/two-loop fixture
   reproduced **16 wrappers but only four unique IDs** before repair.

   Generated creation now uses the existing key sanitizer and a small synchronous
   `withComponentRenderPath` helper to push the loop path around child creation and
   pop it in `finally`. No ownership or registry-cleanup rule is relaxed. Initial
   creation and reconciliation use the same paths. The
   [new browser oracle](../../tests/browser/static_keyed_path.cjs) checks two boards
   with overlapping group/row IDs, nested counters, three reorders, exact DOM/
   registry bijection, retained generations/drafts/focus, removal, fresh remount,
   once-only destruction and inert old controls. The
   [helper regression](../../tests/browser/component_render_path.cjs) checks nested
   return/throw and preservation of an outer owner after an inner exception. This
   guarantees scoped path restoration, not general render transactions.

The original [lifecycle/commit runner](../../scripts/test_keyed_lifecycle.sh) now
also requires the new regressions. Its original browser oracles are unchanged;
both lifecycle cases and all four bounded commit-recovery cases pass after repair.
No Taskboard-specific compiler branch, implicit compatibility, server security,
Interpreter ownership, async worker or source-span implementation was added.

## N04 blocker and strict reproduction

```sh
out="$(mktemp -d)"
python3 tests/test_taskboard_rejections.py build/jtml "$out" --require-provenance
# Current expected exit: 2. Every rejection runs; n04-results.json records BLOCKED.
```

Minimal checked-in reproducer:
[borrowed-alias.jtml](../../tests/fixtures/production/taskboard-rejections/borrowed-alias.jtml).
The offending write is original **line 9, column 5**, in `Candidate`:

```text
linked:   write to borrowed value `alias.inner.score` at line 8:5
module:   write to borrowed value `alias.inner.score` at .../borrowed-alias.jtml:9:5
```

Other audited conflicts: borrowed loop **9:9 versus 10:9**, unknown RHS and
computed target **6:5 versus 7:5**, nested collision **7:7 versus 8:7**. The module
coordinates match these fixtures, but a correct secondary location does not make
contradictory diagnostics accurate. Approximate linked locations are not accepted
as original-source provenance. S1–S3 remain a dependency; the next implementation
step is to repair that diagnostic provenance boundary with original-source spans.

The normal runner executes fail-closed checks and reports the provenance debt;
its zero exit is **not** N04 or T0 acceptance. Its JSON report is retained at
`dist/taskboard-negative-suite/n04-results.json`. The strict command above is the
explicit readiness check. No case is skipped or silently converted to PASS.

## Negative controls and validation record

Pre-fix controls fail the unchanged oracles at intended assertions:

- Saved main compiler output plus repaired runtime: structured action remains
  truncated; expected text/count do not appear.
- Repaired component output plus pre-fix deferred startup runtime: initial creation
  is not generated static creation.
- Old emitted scanner: structured invocation splits at inner whitespace.
- Old static-for compiler plus repaired runtime: **four unique IDs versus 16**.
- Temporary JTML model variants permitting duplicate, mutated or reused IDs fail
  the corresponding visible-error/state assertion. An initially invalid mutation
  control was rejected at compile time; only its corrected scalar-copy variant
  reaching the intended browser assertion is counted.
- All four previously established PR #17 negative controls still detect their
  intended focus/priority/no-op/projection defects.

Focused CTests **14/14**, full CTests **362/362**, C01–C14, the negative runner
and all lifecycle/commit/path checks pass on the repaired tree. The repaired
`scripts/verify_all.sh` **PASS** includes all nine mandatory benchmarks with clean
browser exits, production reference, server security, clean/compat literal and
native timeout regressions, tooling/package/site/editor checks and live smoke
**8/8**, with no verification skips or raised budgets. N04's normal-run BLOCKED
report and strict exit 2 remain distinct from regression-gate success. Native
store instability was not reproduced during this repaired gate; no fix is claimed.
Local macOS arm64 Release, Node 22.23.3, Playwright 1.63.0, Chrome headless shell
153.0.8010.12, Python bindings off. Final-head hosted CI is recorded on the PR. The first full gate's lifecycle failure is retained as evidence;
no passing rerun erases it. Native `friendly_store` instability, linter limitations,
broad slot/import limitations and A01–A10 remain unresolved/unverified.

## Asset measurements

Raw / gzip level 9 / Brotli quality 11 bytes, using existing Node zlib:

| Asset | Main (`bed8741`) | Repaired tree |
| --- | ---: | ---: |
| HTML | 735287 / 60078 / 26844 | 995307 / 79179 / 34649 |
| Component module | 327403 / 23522 / 8478 | 347467 / 25345 / 8815 |
| Shared runtime | 271969 / 50037 / 40502 | 271564 / 50051 / 40582 |
| Bootstrap | 619 / 312 / 248 | 619 / 312 / 248 |

Runtime: **405 fewer raw bytes**, **436 bytes under the unchanged 272000 budget**.
Scanner deduplication first reduced it to 271261; deferred startup made 271372;
scoped keyed creation made 271564. Compressed runtime grows 14 gzip / 80 Brotli
bytes; raw reduction is not advertised as a compressed/network saving. Application
validation/probe data increases HTML by 260020 raw / 19101 gzip / 7805 Brotli bytes
and component code by 20064 raw / 1823 gzip / 337 Brotli bytes. Existing duplicate
linked/module definitions remain; no unrelated output refactor conceals growth.
The reference HTML microfixture comparison remains an explicit known warning.

Local logs: `/tmp/jtml-pr18-{focused-final,core-final,negatives-final}.log`,
`/tmp/jtml-pr18-lifecycle-repaired.log`, `/tmp/jtml-pr18-verify.log` (failed first
gate), `/tmp/jtml-pr18-verify-repaired.log`,
`/tmp/jtml-pr18-{before-keyed-path,keyed-compiler-control,n04-strict-final}.log`
and `/tmp/jtml-pr18-control-*.log`. Hosted final-head CI is separate evidence on
the PR. No automatic merge and no complete T0 claim.
