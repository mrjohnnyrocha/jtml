# Imported-state initialization

Merged #81 (`137667272c0128490059a5316916d237dfc39a17`) initializes original
V21 with only `seed = 7` and leaves `{{copy}}` in the DOM. The runtime
falls back to object enumeration when the compiler emits `stateOrder: null`
for a foreign initializer dependency.

The compiler retains resolved direct import edges and sorts them by source span.
A depth-first traversal visits each dependency once before its importers and
keeps independent imports in textual order. It visits the entry graph first
and then any remaining modules. Each executable module contributes its existing
declaration vector in source order. A foreign read requires retained named
import authority or an existing whole-file import scope, and its defining owner
must precede the consumer. Reexports keep the original defining owner. The
existing browser bootstrap evaluates the resulting flat `stateOrder` once.

The source resolver rejects import cycles. The planner also rejects cycles and
missing edges in its retained graph. Compiler-side artifact validation checks
effective first-wins plans and their projections for coverage, missing roots,
reversed edges, cycles, orphan plans and conflicting executable module owners.
The artifact records the resulting order, while source import authority and
textual tie ordering are proved by the compiler before emission. Existing
top-level fallback projections remain supported when a module is inactive.

Repeated lowered slots and state without retained ownership still use the
uncertified `null` compatibility boundary from #81. That boundary applies to
the whole project and can retain legacy ordering for unrelated initializers.
Named imports, barrel imports, whole-file imports and stores with unique slots
now participate in the ordered plan. Compat/debug retain legacy bootstrap.

Runtime headroom comes from removing four private functions with no calls or
exports: `pruneDynamicComponentListItem`, `componentChangeAffectsNode`,
`renderComponentExpression` and `keyedListTemplateChildren`. The active keyed
child removal and subtree pruning paths remain covered. The emitted runtime
moves from 272,973 to 271,440 raw bytes and from 51,162 to 50,950 gzip level 9
bytes. The cap remains 273,000 raw bytes, with 1,560 bytes available.

The existing required mutable-initialization Chromium suite reads original
V21 alongside V01/V03/V04/V05/V11/V12. The import diamond checks shared
transitive dependencies, textual import order, a local chain and derived
publication. The CTest planner probe exercises disconnected modules, cycles and
missing edges. Additional artifact checks exercise duplicate owners and the
existing malformed-projection controls.
