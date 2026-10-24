# Global component arguments and native public-name ownership

Base: `27b3187` (PRs 34–36 merged). The frozen PR34 binary was built from
`287af40`, whose tree is identical to merged `0a468d5`; the later main changes
are CI and Studio distribution resources, not the compiler/runtime paths here.

## Independent failure and repair

The original `tests/conformance/fixtures/nested/known-global-parameter-alias.jtml`
and its authored 200 oracle are unchanged. Global count=100 is passed as value;
Probe owns a separate count=10 and computes value*2.

| Observation | Frozen baseline | Candidate |
| --- | --- | --- |
| Native numeric expanded result | 20 | 200 |
| Native rendered result | 20 | 200 |
| Clean-production browser DOM | NaN | 200 |
| Local count | 10 | 10 |

The authored expanded expression `count * 2` is correct. There are two distinct
backend losses at the component boundary:

- Native attachment installed public aliases directly into the canonical lexical
  environment. Later recalculation resolved the caller's `count` through the
  callee's presentation alias. Public body-plan rendering/actions now use a
  non-owning name projection over the canonical records; compiled ASTs retain
  their lexical storage and actual CompositeKeys. Projections never own dirty
  graph traversal. A projection points to its canonical parent; no back-pointer
  or mutable value copy is introduced.
- Browser instance initialization copied serialized parameter source text as a
  value (`"count" * 2`). Friendly now transports lossless argument expressions
  separately from legacy display metadata, through Semantic IR and Runtime Plan
  into existing Expression IR plans. The browser evaluates them in caller client
  state, preserving numeric values and quoted strings. Global updates refresh
  parameters and use existing compiled patches, retaining state and DOM. Action
  parameters are restored before refreshing caller arguments.

Native argument refresh uses the existing parser/evaluator, after caller derived
values are current. Only instances that actually acquired a caller scope are
admitted; uninstantiated loop templates are not evaluated as globals. Nested
body-plan instances keep their existing caller-scope path. Reset removes admission
along with instances. Structured references refresh by identity as well as value;
public-name views borrow existing records without defining new reactive owners.
All calls remain under the existing interpreter execution lock; async lifetime,
function frames and server adapters are unchanged.

Production accepts only the existing directly analyzable Expression IR contract.
Unknown argument calls reject before runnable assets with original call-site
file/line/column (CLI regression: line 6, column 3). There is no runtime parser,
eval, new Function or compatibility fallback in the browser. Compiler transport
hex is omitted from browser HTML. Explicit Classic metadata without argument
plans keeps its existing contract. Only touched runtime routines were compacted
to keep the runtime at **271855 / 272000 bytes**, with budgets unchanged.

## Reproduction and controls

```sh
scripts/test_semantic_conformance.sh
node tests/browser/nested_environments.cjs --strict
node tests/browser/global_parameters.cjs
ctest --test-dir build -R 'NativeComponentEnvironment|GlobalArgument' --output-on-failure
```

Use the pinned Playwright/browser environment documented by CI. `JTML_BIN` and
`JTML_CONFORMANCE_PROBE` select frozen binaries; `JTML_CONFORMANCE_OUT` isolates
reports. Every runner persists source, lowering, plans and independent traces.
The new global-update fixture fails on frozen baseline with NaN/string initial
values. The unchanged catalog first rejected the original case's unexpected
resolution. Its reviewed 20/NaN signature and original source/oracle hashes are
preserved in `tests/conformance/history/pr34-nested-known-discrepancies.json`.
Only that entry was manually retired: **1 resolved, 0 remaining nested catalog
entries**. All six nested cases and the original capture probe are mandatory.
An injected 201 expectation still fails the comparison and is rejected by the
empty catalog. No original source, arithmetic expectation or remaining renderer
signature was changed.

Expanded regressions cover numeric values, initial derived caller values,
grouping/indexing, quoted text, repeated writes and resets, local-name collisions,
sibling isolation, init-once state, retained DOM, public global `runAction` and
real component button events under strict CSP. Public DOM is the browser oracle;
private scope values in the inherited suite remain supplemental diagnostics.

## Boundaries and review

Five native complete-attribute renderer observations remain FAIL / KNOWN DEFECT
in their separate suite. Broad release readiness and arbitrary expression support
are not claimed. An existing top-level inline event boundary was also observed
on the frozen baseline: `when step` plus a page-level `button "step" click step`
emits inline `onclick` and produces `script-src-attr` under `script-src 'self'`.
This repair does not change that path; the new CSP test uses the public global
action adapter and real DOM events for components. No CSP assertion was relaxed.

Critical review concentrated on lexical versus presentation lookup, parameter
refresh ordering, uninstantiated loop templates, canonical dirty ownership,
repeated reset, reference identity, callback/worker locks and retained browser
updates. No parser, general coercion, native function execution, renderer,
security policy, lifecycle fallback or budget was redesigned. Existing native
fan-out, function/async/security, provenance and component lifecycle regressions
remain required alongside conformance.

Stable-tree verification: **276/276 focused tests**, **443/443 complete CTests**,
all independent semantic suites and strict nested conformance, **nine clean-exit
browser benchmarks**, production reference, Taskboard C01–C14 and negative cases,
strict N04/provenance, keyed lifecycle, local-server security, async ownership,
literal fidelity/CSP, package/editor/LSP checks, extracted Studio distribution
checks and eight live-runtime smoke checks: **PASS**. The complete
`scripts/verify_all.sh` finished successfully with no validation checks skipped.
The existing reference microfixture size observation and Studio cwd/install-doc
limitations remain printed by their suites; neither is treated as repaired.

Hosted CI is a separate exact-head check, requested once after PR publication;
pending jobs are UNVERIFIED, not PASS.
