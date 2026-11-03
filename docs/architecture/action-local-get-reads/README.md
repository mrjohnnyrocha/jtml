# Action-local scalar `get` reads (V03)

This correction targets clean-browser synchronous root actions. It does not
ratify Reactive Core v1, change its authored oracles, or select an A6c failure
policy. Compatibility/debug and other backends retain their previous behavior.

## Cause and correction

Previously `runClientStatements` evaluated a getter read from `clientState`'s
cached derived value. `applyClientDerived` ran only at the final action flush.
In V03, `x = 1; saved = twice` therefore saved zero before `twice` became two.

A private action read scope now recursively evaluates getter expression plans
against current working state. Assignments still mutate the original storage;
subsequent reads, conditions, `show`, and computed write indices use that scope.
Getter evaluation neither updates caches nor performs DOM work. Parameter reads
use the invocation frame; a getter's dependencies use defining-module storage,
masking temporary parameter slots with their saved root values. Reading a getter
whose name is also a parameter follows these two distinct scopes.

The existing structured expression evaluator is shared with the old root
expression paths. No generated source is executed, and no `eval` or
`new Function` is introduced. Parameter restoration, final derived flush,
effect ordering and the existing single action-end DOM update are unchanged.
The global derived scheduler is deliberately unchanged.

`validateActionGetReads` certifies consumed getter closures from retained module
plans, import kinds and defining-module identities before output is written.
Unresolved/unimported roots, cycles, unsupported plans, known collection/null
results, ambiguous value/get names, reserved storage, and action-local declarations
that shadow root storage are rejected with source locations. Only explicitly
imported values authorize cross-module reads; importing a same-named component
does not. Unknown expression result types must yield a scalar at execution;
missing getter dependencies throw instead of reaching the legacy namespace.
Existing module ownership guards run first. These bounded rejection rules do
not establish new block-local scope or error-recovery semantics.

## Reproduction and evidence

`tests/browser/action_get_reads.cjs` compiles V03 and V01 directly from their
original `vectors.json` sources, using independently authored DOM expectations.
It requires real Chromium, strict script CSP and clean shutdown. It also covers
repeated two-write actions, chained getters, condition/`show` reads, computed
write indices, parameter masking, explicit imported ownership, and transactional
compiler rejection controls. CI and the local full gate require this test.

The exact main baseline `0468e690299673861b31e674d23603226e430f3a` produced V03
`0|2` after dispatch. The candidate produces `2|2`, including repeated dispatch.
Compat/debug still produce `0|2`. Existing root-effect/batching and module
ownership Chromium controls pass independently.

For the identical V03 source, `index.html`, `components/index.js` and `app.js`
are byte-identical between baseline and candidate. Shared runtime payload:

| Asset | Baseline | Candidate | Change |
| --- | ---: | ---: | ---: |
| Raw runtime | 272,370 B | 272,886 B | +516 B |
| Gzip runtime (Node zlib, level 9) | 50,893 B | 51,127 B | +234 B |

The existing 273,000-byte raw-runtime budget is unchanged.

The separate independent reference model remains unchanged: 18 tests and all
four source/schema/AST audits pass. Model results are not browser conformance
claims. The pinned PR #77 observations remain historical baseline evidence.

## Remaining global-ordering failures

The optional `--document-gaps` mode executes the original V04/V05 sources and
compares them with unchanged authored expectations. It deliberately exits one
on a mismatch and is not part of the required zero-exit gate.

| Vector, first dispatch | Authored expectation | Browser observation | Status |
| --- | --- | --- | --- |
| V04 (`a|z`) | `6|3` | `4|3` | FAIL: global derived ordering |
| V05 (`left|right|total`) | `3|4|7` | `3|4|4` | FAIL: global derived ordering |

Lazy action reads do not fix publication order, initial global derived ordering,
component scheduling, or full Core conformance. Effects and imperative `show`
retain their existing semantics; no rollback or recovery is promised.

## Local gate result

All 518 CTests, nine required clean-exit browser benchmarks, application/CSP
suites, editor/LSP checks, packaging/relocation checks and eight live smoke
checks passed. The full gate was invoked once with required browser execution
and clean exit. That invocation stopped at the website's frozen compiler-output
hash comparison because its default selected the modified compiler. The unchanged
site-through-smoke suffix then passed using `JTML_SITE_BIN` set to the exact
existing website compiler pin `69a2cb7a60bd12887cc434e47d22cce0e8a0a8c3`.
This was a resumed gate, not one uninterrupted zero-exit invocation. No required
check was skipped. Existing keywords-linter limitations, the 100,009-byte
reference HTML warning and host locale warnings remain; budgets and assertions
were not changed. JavaScript syntax and `git diff --check` also passed.
