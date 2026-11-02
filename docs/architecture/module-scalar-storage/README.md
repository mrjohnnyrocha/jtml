# Compiler-owned scalar module storage

Baseline: `2d290ce88002867a37c1f60634544d70750c368c` (PR #75).
This slice supports previously rejected colliding scalar module declarations in
clean browser builds. It does **not** fix OpsPulse Theme, store-derived member
semantics, or implicit component-to-module reactive reads.

## Implementation and identity

`lowerBrowserScalarStorage` in `src/browser_scalar_storage.cpp` uses the retained
SemanticProject module ASTs and RuntimeProjectPlan owners, before client JSON
serialization. The identity is a declaration in its defining module, not a name
in the linked projection. Original provenance must match that module. Reexports
retain the declaring AST; they do not allocate a second declaration.

The pass certifies a closed lexical subset and clones its AST. It allocates fresh
internal identifiers for root declarations and action parameters, reserving all
authored declarations/parameters first. Only variable-expression nodes and plain
assignment targets resolved in that lexical scope are rewritten. Parameters
shadow module declarations. Unresolved/imported references fail certification;
they cannot capture another module or a guessed internal identifier. Literals
are never searched/replaced. This follows the compiler-owned fresh-identifier
principle used by `planExplicitInputs`, without borrowing its writable-component
contract or altering source-language syntax.

Initializer resolution permits only earlier declarations in the same module.
Fixed-width increasing keys preserve that order in the existing name-sorted JSON
objects. Actions resolve against their own module plus their own parameters.
Rebuilt Expression IR and Runtime Plan action records carry those same keys into
reads, writes and initialization. The linked state/action projection is replaced
from these certified per-module records before its first lossy object emission in
`runtime_plan_json.cpp`; it is never used to recover identity. Module manifest
merging and the existing runtime then see distinct storage addresses.

No JavaScript runtime changes, aliases, global scope merging, eval, or new
execution subsystem are needed. Keys need not stay stable across builds and do
not encode numeric module IDs. `window.jtml.state` still exposes inspectable
storage; this is lexical isolation, not confidentiality. No supported source-name
inspection contract changes: these colliding clean builds previously failed.
Collision-free builds return before lowering; compatibility/debug paths never
invoke it.

## Deliberately closed subset

Certification applies to the entire colliding execution program, transactionally:

- Mutable scalar lets, scalar literal/unary/binary expressions, earlier local
  initializer reads, distinct synchronous root actions, straight-line show and
  plain assignments. Assignment/initializer types must prove scalar.
- Parameters may shadow module declarations; unconstrained parameter assignment
  into module state is rejected. Nested local declarations are not yet supported.
- Static control components/root markup, distinct root action events and literal
  scalar event arguments. Component-local state/parameters are conservatively
  excluded in collision builds, with no change to collision-free applications.
- No derived/fetch/effect/subscription/store/extern/route/raw-HTML consumers,
  imported value bindings, calls/member/subscript expressions, implicit component
  state reads, writable UI bindings, or action-name collisions.

A failed proof leaves all original plans intact. The PR #75 guard still rejects
unsupported collisions before assets are written, with both owners and original
locations plus the proof failure. Previously published assets remain untouched.
The original `module-binding-collision` fixture still rejects because its UI
implicitly reads `tone`. Broader storage/reactivity support needs owner-qualified
consumer semantics before this allowlist can expand.

## Executable proof

The new C++ positive regression fails against baseline libraries: see
`evidence/cpp-before.txt`. The freshly built exact-baseline CLI also rejects the
new two-module fixture before emitting assets (`evidence/baseline-rejection.txt`).
There is no baseline Chromium execution for that rejected build.

`tests/browser/module_scalar_storage.cjs` compiles and launches real Chromium for
two modules, a private component reexport, and three modules. The third module's
path sorts before the entry to perturb project ordering. JTML `show` console
output proves independent initializers, initializer copies, four alternating
writes per owner in both orders, parameter shadowing, preserved other-owner
values, and fresh initialization after reload. Browser exceptions/console errors
are prohibited and browser.close is awaited. No component-state read is used as
an oracle. Recorded successful output is in `evidence/chromium-after.json`.

C++ checks assert consistency across linked/module JSON, fresh-name hygiene,
unchanged compat/debug contracts, disjoint bindings, and transactional rejection
of unsupported consumers, imported/unresolved references, forward initializer
references, nested locals, calls and member writes. Existing PR #75 tests retain
unsupported collision, reexport and rollback coverage. Both browser suites are
required by the local gate and CI.

## Payload

`evidence/payload.json` compares exact-baseline and candidate compilers using the
same disjoint-name control. All four output files are byte-identical. Gzip uses
level 9 and total gzip is the sum of separately compressed assets.

| Asset | Baseline disjoint raw/gzip | Candidate colliding raw/gzip |
| --- | ---: | ---: |
| Runtime | 272370 / 50835 | 272370 / 50835 |
| Components | 23080 / 2706 | 23080 / 2706 |
| Bootstrap | 2043 / 905 | 2043 / 905 |
| HTML + manifest | 23789 / 2444 | 27137 / 2472 |
| Total | 321282 / 56890 | 324630 / 56918 |

Runtime/component/bootstrap delta is zero. The equivalent colliding fixture adds
3348 raw / 28 gzip bytes to HTML; the baseline cannot emit this colliding fixture,
so this is a disjoint-control comparison, not an invented successful baseline.
The 273000-byte runtime budget is unchanged. Final committed-candidate gate and
hosted CI results belong in the PR; they are separate verification evidence.
