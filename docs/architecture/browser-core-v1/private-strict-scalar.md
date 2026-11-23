# Private strict scalar profile

The build test executable `jtml_browser_private_compiler SOURCE OUTPUT` selects
the private browser runtime during a clean browser build. The compiler proves
the authored subset before emitting the existing four assets. The experimental
runtime keeps the same manifest and application bootstrap schema. The separate
`--authority-only` test invocation retains the import authority experiment
for fixtures that exercise broader application features.

## Admitted source

- Friendly modules with the existing private named import and reexport rules.
- Source-ordered scalar state and acyclic getters. Each cell keeps its initial
  kind. The kinds are finite numbers, Booleans and plain printable ASCII
  strings with unescaped source spelling. Numeric literal values must be safe integers. Negation and checked
  arithmetic can produce other finite numbers.
- Resolved simple reads, `-`, `!`, homogeneous arithmetic and equality,
  numeric ordering, Boolean `&&` and `||`, and Boolean ternaries with
  same-kind arms. String addition requires two strings in evaluated formulas.
- Synchronous root assignments, `show`, Boolean action branches, and the
  existing acyclic root effect slice. Root action parameters are numeric.
  Named button clicks take exactly the declared arguments, each a direct
  numeric literal or a resolved numeric read.
- Inert markup and text projections over literals and simple scalar reads.
  Text concatenation requires a string operand at every join. Put computed
  projections in checked getters. Rendered root conditions are Boolean
  literals or reads.
- Root component instances with numeric literals or reads as parameters,
  including root conditional mounting. Component state and getters are
  source ordered with the existing certified capture boundary. Component
  actions are zero-argument, straight-line owned assignments with plain
  assignment or numeric increment by addition.

Collections, ranges, member and index access, loops, general calls, nested
action locals, computed click arguments, arithmetic projections, nested
component instances, slot and callback forwarding, reactive attributes,
stores, fetch, input bridges, routing, host execution and raw HTML are
outside this profile. Existing experimental browser suites cover several
of those paths separately.

## Failure observations

Every accepted initializer or getter uses the existing checked evaluator.
The first synchronous boot fault sets the private diagnostic to `FAILED`
with an `initialization` invocation, leaves the publication revision at
zero, and revokes subsequent dispatch. The diagnostic may retain initialized
source state or partial DOM. This contract covers synchronous boot for
admitted source programs. Asynchronous loading, navigation, arbitrary host
callbacks and rollback are outside the subset. A successful action that
changes an unrendered local cell can leave the publication revision unchanged.

The inventory reads the unchanged V01–V24 catalog. Its V19 experiment checks
the authored P3 alternative and preserves the historical pending-policy
classification in the source catalog. The bounded profile leaves complete
S1 and Browser Core v1 certification open.
