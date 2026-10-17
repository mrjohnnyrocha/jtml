# Nested component parameter updates

The production `nested-heading` regression originally dispatched `Board.rename`
and propagated `heading` into `Summary.title`, but left its `h2` at `Before`.
The generated `Summary` selector selected `panel title title` and then suppressed
its descendant heading because an ancestor was already selected. A
`container-attrs` patch only updates attributes; it does not update descendants.
This was the first incorrect layer, in the static update emitter rather than
nested instance parameter assignment.

The selector now distinguishes selected nodes (`seen`) from selected subtree
patches (`subtreeSeen`). Attribute-only containers are selected without claiming
the subtree. Ancestor traversal still walks through those containers, so a
selected region or nested component continues to suppress work that its own
patch covers. Runtime parameter assignment, component creation, and fallback
paths are unchanged.

`tests/fixtures/production/reference-app/regressions/nested-heading.jtml` now
covers two sibling Boards, reactive and fixed sibling Summaries, two intervening
attribute containers, and a nested Detail component. Five actions exercise
Before -> After -> Before -> After -> Final and an independent sibling update.
The browser scenario requires 22 assertions, including text and attributes,
DOM/instance identity, stable registry ownership, no patch fallback, and no
network or browser errors. `scripts/test_production_reference.sh` requires this
scenario to pass. Restoring the old ancestor suppression in generated assets
fails the first Rename with heading values still at Before.

## Conditional dependency follow-up

An additional conditional probe in Summary:

```jtml
make Summary title
  panel title title
    h2 title
    if title == "After"
      p "Changed"
    else
      p "Original"
```

produced an `if` Runtime Plan node with `reads: []` despite an expression plan
reading `title`. With Board passing a mutable heading, Rename updated the heading
and attributes after this fix but left the conditional at Original. This
was outside the attribute-only ancestor suppression fix. The follow-up
[conditional reactivity slice](conditional-reactivity.md) repairs the missing
reads and adds a separate mandatory browser matrix. The nested-heading scenario
continues to cover its original text/attribute and identity contract.
