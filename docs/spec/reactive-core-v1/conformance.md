# Conformance vector contract and index

[vectors.json](vectors.json) contains **24 independent examples**, each with its
complete source-file map, entry module, explicit initial source/derived state,
ordered external action entries, and complete expected post-step state, derived
values, root notifications, effect invocations, imperative outputs and DOM text.
Expected rejection has no initial state or DOM; a runtime fault retains the last
committed snapshot and emits no successful commit. Expectations follow the
[normative draft](../reactive-core-v1.md), not current implementation output.

**All full target-conformance cases are pending** human ratification and the future
independent model/phase adapter. Some bounded snapshot aspects are already
implemented and tested. Non-executable target capabilities and observed
mismatches are explicitly pending; they are not xfails, waived tests or new gates.
Nothing here weakens `verify_all.sh`, CTest, browser assertions, or existing CI.

## Machine-readable conventions

- `files` maps relative filenames to original Friendly JTML 2 bytes. Extract them
  unchanged before compiling. `main` means `index.jtml`; `private` means
  `private.jtml`. These labels identify defining modules, not linked aliases.
- `main::name` is a declaration identity in this small catalog. A production
  harness must independently resolve it; it must not assume the emitted key is
  `name`. `main::Card@1::local` and `@2` denote distinct mount generations.
- `expected_initial` is commit 0 for an accepted project. Each step with
  `outcome: commit` is exactly one subsequent successful semantic commit, even
  when only a temporary parameter was assigned. Fault/reject steps emit none.
- State maps are **complete for the vector's mutable `let` cells in active
  generations**; derived maps contain its `get` outputs. They are not patches.
  Component parameters follow the source's invocation expressions and are not
  extra mutable state entries; temporary action parameters are absent.
  The final state is the last successful snapshot, or initial snapshot if there
  are no actions. Error programs have null state/derived/DOM and empty events.
- `.out` selects ordered visible text observations. Compare trimmed textContent
  of each matched wrapper; do not normalize whitespace *inside* scalar strings.
  V16's empty array means the component observer no longer exists. Extra runtime
  debugging/global keys are not extra semantic declarations.
- `notifications` is an ordered list such as `E1:main::x=2`: one root subscription
  delivery with the watched scalar value at delivery, before the callback body.
  E1/E2 label effect declarations in their own module in textual order. A vector
  with no root effects has `[]`; this says nothing about render dependency edges.
- `effects` records callback invocation order. `show` records scalar values emitted
  only in that initial/step phase; it is not a cumulative log. Neither list may be
  inferred from final DOM. V02's show output is an existing callback-count witness;
  a missing general notification observation hook must be marked unverified.
- `created`, `destroyed`, and `forbidden_notifications` are lifecycle obligations
  in addition to the scalar/DOM snapshot. They refer to logical generations, not
  exact browser IDs, registry indexes or physical patch counts.
- `baseline.classification` uses the four categories defined in
  [gaps.md](gaps.md). `baseline.execution` records what was actually exercised;
  `target_conformance: pending` always refers to the stronger complete proposal.
  Symbolic error codes classify failures; they are not a promise that today's
  CLI emits these exact identifiers.

No vector depends on another vector's state, fixtures, initialization or actions.
There are no imported test helpers that decide values. All action arguments are
explicit scalar literals. Formula/source names are chosen to expose ordering,
not to make the production name-sorted iteration accidentally correct.

## Index and independently authored terminal oracle

Full initial/intermediate states and event lists live in JSON. The table is a
reading aid and does not replace them. T/V/I/P are **baseline** bounded-aspect
classifications, not blanket Core conformance claims.

| ID | Example and action sequence | Expected final source / derived / events / DOM | Baseline |
| --- | --- | --- | --- |
| V01 | x=1; Step, Step | x=3; no derived or notifications; DOM `Value 3` | T |
| V02 | x=0/hits=0; one action writes x=1 then 2 | x=2/hits=1; E1 once watching 2; show 2; DOM `2\|1` | V |
| V03 | write x=1 then save twice in same action | x=1/saved=2; twice=2; no notification; DOM `2\|2` | I |
| V04 | chain x→z→a; Step twice | x=3; z=4/a=8; no notification; DOM `8\|4` | I |
| V05 | diamond x→zleft/zright→atotal; Step twice | x=3; left=4/right=6/total=10; no notification; DOM `4\|6\|10` | I |
| V06 | module x=10, effect x, shadow(3) twice | x=10/hits=0; no notification; each show 4; DOM `10\|0` | T |
| V07 | x=1; same-value assignment twice | x=1/hits=2; E1 once per entry; DOM `1\|2` | T |
| V08 | conditional true?left:right; Right, Toggle, Left, Toggle | true/left=5/right=11; chosen=5; no notification; DOM `5` | T |
| V09 | two private `tone`s; Private, Entry, Read private, Read entry | private!/entry! independently; shows private!, entry!; static DOM controls | T |
| V10 | imported Card reads defining module label; Step | entryTone=entry/private tone=private!; private label=private!!; DOM private!! | P |
| V11 | let z=2 then a=z+1; initial only | z=2/a=3; result=5; no notification; DOM `3\|5` | I |
| V12 | first reads later let | pre-execution E_INIT_FORWARD; no state, notification or DOM | I |
| V13 | mutually dependent getters | pre-execution E_DERIVED_CYCLE; no state, notification or DOM | I |
| V14 | action reads missing identifier | pre-execution E_UNRESOLVED; no state, notification or DOM | I |
| V15 | getter calls unknown mystery() | pre-execution E_UNSUPPORTED_EXPR; no state, notification or DOM | I |
| V16 | Local, Toggle off, Bump root, Toggle on | count=1; new local=0/twice=2; no dead delivery; DOM `2\|0` | V |
| V17 | root effect x→y→z; Step | x=1/y=2/z=3; E1 then E2 watching 1 then 2; DOM `1\|2\|3` | V |
| V18 | potential effect x↔y feedback | pre-execution E_EFFECT_CYCLE; no state, notification or DOM | T |
| V19 | x=1; assign 1/0 | E_SCALAR_DOMAIN fault, no commit; last valid x=1/DOM `Value 1`; instance faulted | P |
| V20 | write x=1/show twice, x=2/show twice, effect observes | x=2/seen=4/twice=4; action shows 2,4; E1 watches 2; DOM only `2\|4\|4` | I |
| V21 | importer initializes copy=seed+1 | private seed=7/main copy=8; no notification; DOM `8` | I |
| V22 | imported seed assigned by importer | pre-execution E_IMPORT_WRITE; no state, notification or DOM | P |
| V23 | decoded interpolation guesses generated key | pre-execution E_UNCERTIFIED_EXPR; no state, notification or DOM | T |
| V24 | component-local effect | pre-execution E_UNSUPPORTED_EFFECT; no state, notification or DOM | T |

V10 is pending because implicit defining-module capture is not yet a supported
clean-browser contract. V19 and V22 additionally depend on proposed policy A5/A6;
current acceptance is not called a regression against an already established
language guarantee. V24 deliberately remains a rejection; lifecycle render
observers in V16 do not covertly enable component-local effects.

## Reproduce a source without a new runtime or gate

From the repository root, extract a vector (example V03):

```sh
python3 - <<'PY'
import json
from pathlib import Path
catalog = json.loads(Path('docs/spec/reactive-core-v1/vectors.json').read_text())
case = next(v for v in catalog['vectors'] if v['id'] == 'V03')
root = Path('/tmp/jtml-core-V03')
for name, source in case['files'].items():
    path = root / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(source)
print(root / case['entry'])
PY
./build/jtml build /tmp/jtml-core-V03/index.jtml --target browser --out /tmp/jtml-core-V03/dist
python3 -m http.server 8000 --bind 127.0.0.1 --directory /tmp/jtml-core-V03/dist
```

Open index.html in real Chromium, record `.out` text, click Step once, and record
again. The normative result is `2|2`; pinned-baseline observation is `0|2`.
An automated browser adapter must use the same literal source and real button
entry, not injected state or direct effect invocation. Other button labels are
literal in each vector source; V08 uses Right/Left/Toggle, V09 uses Entry/Private
and explicit Read controls, and V16 uses Local/Toggle/Bump.

The [baseline observations](baseline-observations.json) record all these actual
builds and snapshots. Collection success is not semantic PASS. Re-run any desired
case independently; do not wire the currently failing target assertions into a
required green gate with an “expected failure” waiver. The future adapter must
also record the [reference-model trace contract](reference-model.md) to claim
phase, notification, atomicity and lifetime conformance.

## Missing regressions before a complete v1 claim

The catalog covers the requested first contract, not a proof. Add long/permuted
chains and diamonds, source-order tie effects across reexports, same-value
write-back-to-original actions, exact arity errors, action-parameter names matching
other modules, inactive-arm cycles, signed-zero equality and finite-domain errors,
fault after an already emitted show, and observer cleanup on nested/sibling
remounts. Add independent read-only-capture shadowing and unimported-file controls
when capture becomes executable. A future mandatory suite must distinguish
`PASS`, `FAIL`, `UNSUPPORTED`, `UNVERIFIED`, and `MODEL_LIMIT`; none may be silently
collapsed to PASS.
