# Explicit caller-authorized text input

Focused acceptance and the complete local gate pass. Exact-final-commit rerun
and hosted CI results are recorded separately on the focused PR. Work remains on
`codex/component-module-input`, based on clean main after PR #64,
`cd8ad63f72565cf4e8faaac47074d11d3304b64b`.

The [contract](contract.md) was written before implementing the new syntax.
This is a new, explicit opt-in feature; it does not change the existing rule that
ordinary component `into` targets must be local writable `let` state. Enterprise
is unchanged and remains unsupported for edited-email sign-in.

## Implemented candidate and ownership evidence

```jtml
let email = "seed@example.test"
make Editor label bind value
  page
    input into value aria-label label
    text "Draft: {value}"
page
  Editor "Account" bind value to email
```

The current subset supports one text-input binding parameter, explicitly passed
by an entry-module caller to a uniquely owned literal-string `let`. Imported
component definitions are supported. Imported state, borrowed/collection targets,
ordinary parameters, derived/immutable values, forwarding, route/layout injection
and action assignments to the binding parameter do not grant write authority.
Native/live and inline execution without a retained Runtime Project Plan reject.

The [ownership chain](ownership-chain.json) contains Semantic IR declarations and
explicit arguments, retained module Runtime Plans, generated actions and original
caller locations. The browser manifest receives only `inputActions`, mapping an
instance's binding parameter to a compiler-issued action identity, and its normal
compiled read argument. The listener invokes the existing root action executor;
it never resolves an authored global variable name or searches enclosing scopes.
Writes therefore use the existing state/effect flush and retained component
argument updates. No separate interpreter or global mutable fallback is added.

Generated action identities and setter parameters use one fresh-identifier
allocator. Before allocation, it reserves declared bindings, root action
parameters, component parameters/local bindings, body-plan reads/writes and action
parameters, and semantic/event references. Each generated identifier is inserted
back into that set. For any finite occupied namespace, suffix search terminates;
its result is disjoint from authored identifiers and prior allocations. There is
no special-case alternate spelling for a particular user name.
`CliBuild.ExplicitInputParametersAreFreshAcrossAuthoredNamespaces` reserves many
candidate suffixes across root state, root parameters, component locals/parameters
and authored action references, then checks freshness and the generated write
expression. Chromium also edits an owner named `__jtml_input_value` while checking
that similarly named sentinels remain unchanged and parameters do not leak.

The new closed-schema regression rejects unknown capability fields, leaked
provenance, non-string/missing actions, mismatched owners/instances/read paths,
missing parameters and a setter parameter that captures its target. This does
not expand ordinary component write permissions.

## Focused results

- [Unchanged-main new-syntax baseline](before.json): build exits 1 before assets.
  The retained compiler at `c417c69` has the identical source tree to the requested
  merged main (`981cc6a71a0c6336e5a6e1058746fceed5d3d347`).
- Candidate compiler and explicit fixture build successfully.
- [Chromium candidate](candidate-browser.json): **9/9 pass**. Imported
  definitions, owner state, effects, mirrors, repeated changes, focus/selection,
  local shadows, siblings, root input, remounting, hygienic parameters, explicit
  sign-in/sign-out and protected-route denial pass. The privacy assertion preserves
  the exact baseline coordinate exception described below.
- [Negative matrix](negative-matrix.json): **22/22 pass**, checking the intended
  rejection reason, exact original file/line/column, and absence of emitted
  production assets. Imported definition errors include the original definition
  location. Native build/transpile/live execution reject before execution.
- All **495/495 CTests pass** with local-port permission, including the two new
  hygiene/schema tests. An earlier sandboxed run failed server startup because it
  could not bind a local port; the complete authorized rerun passed.
- [Assets](assets.json): runtime **272,925 → 272,992 raw (+67)**, gzip
  **50,841 → 50,893 (+52)**. Ceiling remains **273,000**, leaving **8 bytes**.
  The runtime addition is input-action dispatch; repeated defensive array checks
  were factored into one helper without removing their validation.

## Precise baseline coordinate exception

A [common-fixture comparison](production-field-comparison.json) builds the same
ordinary local-input source on unchanged main and on this candidate. Its public
manifest, HTML, component modules and bootstrap are byte-identical. Existing
numeric `sourceLine` and `sourceColumn` fields occur directly on public
`componentDefinitions[].bodyPlan[]` execution nodes. The existing body-plan schema
also permits those coordinates on `componentInstances[].slotPlan[]` nodes.
Both existing static-plan key implementations use the coordinates. They are
preserved unchanged: no coordinate removal, hash change or new resolver fallback
is part of this feature.

The [previous overbroad assertion](historical-overbroad-privacy-check.json) rejected
these baseline execution-coordinate fields as if they were newly introduced
capability provenance. The [baseline evidence](coordinate-baseline.json) records
that mistaken NO-GO and its disposition. The user explicitly authorized correcting
only that new assertion. It now permits integer, nonnegative coordinates only in
those pre-existing execution-plan positions. Coordinates elsewhere still fail.
The existing production schema and security assertions have not been weakened.

New public fields are limited to the per-instance `inputActions` string map,
which refers to compiler-issued one-argument assignment actions in the owning
module. Source/authorization evidence remains internal or in `explain` output:
`bindingOrigin`, `bindingArguments`, `bindingParams`, source paths/spans, original
locations/ranges, raw binding-argument markers and source-rich body payloads are
still rejected by the focused test. An additional raw-HTML inspection found the
new `data-jtml-binding-params` declaration marker; it was removed at browser
emission and is now explicitly rejected alongside the raw argument marker. That
real new leak was fixed, not included in the baseline coordinate exception.
Generated component modules must not contain
coordinate keys either; that pre-existing restriction is preserved.

The [closed-schema regression](../../../tests/test_cli_commands.cpp) rejects
unknown capability fields, leaked provenance and inconsistent action/read/owner
records. No genuinely new data leak is converted to a pass. Final review also found that
the new call parser rejected an ordinary positional argument named `bind`; it now
consumes ordinary parameters before interpreting binding directives, and the
hygiene regression covers that valid call. No write authority is added by a
positional argument. The allocator,
22-case negative matrix and compiler authority proof are unchanged by the
acceptance correction.

The complete local gate, all nine required clean-exit benchmarks and hosted
Ubuntu/macOS CI must pass on the final commit before a GO recommendation. Their
exact final-commit results and log hashes are recorded on the PR separately from
this focused evidence snapshot. No automatic merge, release or deployment is
included.

## Enterprise remains NO-GO

The original detailed audit was rerun in real Chromium before and after the
candidate: [before](enterprise-before.json), [after](enterprise-after.json).
It still exits 1 with **8 pass / 3 fail**. Typing Email does not update module
`loginEmail`; the two expectations for the typed probe email consequently fail.
Sign-out and guard denial pass after PR #64. No Enterprise source or original
audit assertion changed, and no authentication API was mocked.

A separate quoted-label interpolation defect was encountered while constructing
the independent fixture: substituting a label containing spaces inside an
interpolated string generated malformed Classic output. The fixture uses a
literal mirror label to avoid that unrelated shape. That defect is not fixed or
counted as a feature pass.

See [verification status](verification.json) for executed and outstanding checks.

## Resumed complete gate

The [local gate record](gate.json) exits 0 with 495/495 CTests, the complete
9-check positive and 22-case negative suites, all nine performance fixtures and
the reference browser check with clean exits, packaging/tooling checks and all
eight live smoke checks. No tool-dependent section skipped. This snapshot records
the candidate before committing; the exact committed rerun and hosted Ubuntu/macOS
results are recorded on the PR.

The first gate run with installed Chrome 156 stopped on the existing strict-CSP
test's console assertion: `/favicon.ico` returned 404. The same unmodified test
failed with the unchanged-main compiler and identical request. No assertion or
compiler change was made. Playwright 1.62.1's pinned Chromium headless shell
151.0.7922.34 passes the complete gate. Both run hashes are retained. The gate
still reports the independent reference-app HTML-size overage and five native
attribute-renderer discrepancies; neither is converted into feature acceptance.
