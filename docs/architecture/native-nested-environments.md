# Native nested component environments

Baseline: `c059289` (merged PR #30). This repair changes only native lexical
attachment, binding ownership and invalidation; no Friendly lowering, browser
runtime, worker admission/lifetime, locks or independent oracle is changed.

## Causes and repair

Expanded child environments were initially attached to the global scope even
though lowered parameter expressions reference the enclosing component's local
names. Attach them at their actual element lexical boundary before declarations
execute. Traversal enters an enclosing component before its children and restores
the prior scope on exit; environments only point towards enclosing scopes.
Reset drops the old component graph under the existing execution lock/generation
policy. This adds no retained dependency callbacks or child pointers.

Public component aliases and lowered names share VarInfo, but dependencies were
registered under the requesting child's instance ID. Resolve lexical variables
to their actual owner and canonical lowered CompositeKey for dependency capture
and lazy evaluation. Return the current value after lazy recomputation. Component
initialization checks its own declarations, allowing a child to shadow parent
state. Dirty propagation requires the exact key/owner rather than a matching
public name; a child's `result` must never dirty a nonexistent parent key.

Before an owner clears dirty state, transfer invalidation to derived bindings
in descendant scopes using their canonical dependency keys. Process ancestors
before descendants, including active function scopes. Transfer only marks here:
evaluating ancestors recursively from a side-effecting function assignment would
reenter an unfinished derived expression. Existing scope-local drains retain
that execution boundary. For-loop callbacks now update their captured scope.

## Independent before/after evidence

The unchanged original 22-case matrix has **20 PASS / 2 KNOWN DEFECT** on the
frozen baseline and **22 PASS / 0 defects** after repair. All authored sources,
source hashes and expected traces are unchanged. Only the two catalog entries
were manually removed; their old signatures remain in
`tests/conformance/history/pr28-known-discrepancies.json`.

- `nested-parameter-minimal`: expected parent x=2, rendered child=4. These values
  already worked; baseline incorrectly diagnosed undefined `__Probe_2_x` and a
  failed child derive. After repair values match and diagnostics are empty.
- `nested-parameter-retention`: expected parent pairs (2,2), (3,2), (4,2), child
  pairs (4,4), (6,4), (8,4). Baseline emitted undefined-parent diagnostics and
  updates to nonexistent instance `result` keys. After repair the same native
  and clean-production browser traces pass without those diagnostics.

Run `node tests/conformance/runner.cjs --strict --minimize` with the established
Playwright environment. To reproduce before, set JTML_BIN and
JTML_CONFORMANCE_PROBE to independently built baseline binaries. Catalog mode
rejects new failures and unexpected resolutions; strict mode compares real traces
with authored expectations. Oracle self-tests and fault injection into the two
actual repaired reports reject new diagnostic failures.

The native regression suite additionally observes expanded child state, not just
its separately rendered view. Nine tests cover lexical initialization, canonical
keys, parent/global writes, same-name shadowing, conditional removal/remount,
transitive sibling ownership, reset, 64 parent/child pairs, and a side-effecting
derived write that executes exactly once and invalidates its child. Seven original
new tests linked against the frozen baseline core fail 7/7; all nine pass after.
Two independent native/browser fixtures cover deep sibling updates and repeated
conditional lifecycle transitions, including DOM identity where retained.

## Separate known failure (not repaired)

`tests/conformance/fixtures/nested/known-parameter-capture.jtml` declares Parent
count=100, passes count to Child, and Child declares count=10 and result=value*2.
Independent expanded-state expectation is **200**; both baseline and candidate
produce **20**. Friendly lowering captures the child's count when substituting
the parent parameter expression (`__Child_1_count * 2`). The dynamically rendered
child view is independently 200; do not mistake it for the expanded state.

`tests/browser/nested_environments.cjs` reproduces this as **FAIL / KNOWN DEFECT**
against an exact source/oracle/failure catalog. `--strict` exits nonzero. Changed
results, new diagnostics, altered oracles and unexpected resolution require
review. No diagnostic is silenced and no missing binding is invented. The
separate native attribute renderer's five known failures remain unchanged.

## Review boundaries and verification

Scope discovery scans registered/active lexical ancestors without a persistent
cross-environment ownership index. The bounded 64-pair regression checks
correctness and isolation; it is not a claim of arbitrary-scale performance.
The existing synchronous execution/async serialization policy remains in force;
this does not establish unrestricted concurrent access through borrowed mutable
Environment pointers. Self-mutating cyclic reactive programs gain no new contract.

Stable-candidate validation: 37/37 focused tests; 432/432 complete CTests;
22/22 original strict conformance cases; two added native/browser cases; oracle
and catalog fault-injection controls; strict N04/provenance; production reference,
Taskboard C01–C14/N01–N08, keyed lifecycle, local-server security and async tests;
all nine execution benchmarks with browserExit=clean; full verify_all.sh exit 0.
No checks skipped (omitting legacy update assets is the production contract).
The supplemental capture strict run exits 2 as required. The five native
attribute renderer observations remain FAIL / KNOWN DEFECT, not semantic passes.
The pre-existing reference HTML size warning remains visible; budgets unchanged.
Browser runtime is byte-identical: 271633 bytes, SHA256
`da88340a15ab9fb1557d67f8c6109f7c376fe7e2ac523b05dcef03020d86b011`.

Local evidence: `/tmp/jtml-nested-baseline/`,
`/tmp/jtml-nested-review-original/`, `/tmp/jtml-nested-seven-before.log`,
`/tmp/jtml-nested-review-expanded3.log`, `/tmp/jtml-nested-review-supplement/`,
`/tmp/jtml-nested-review-catalog-negative.log`, and
`/tmp/jtml-nested-full-gate.log`. These temporary logs are supplemental;
checked-in fixtures, independent oracles and historical signatures reproduce
the evidence. Exact-head hosted CI is recorded separately in the PR.
