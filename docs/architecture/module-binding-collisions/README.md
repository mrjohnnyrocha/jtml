# Clean browser module binding collision guard

Baseline: `50e1427ced568b00f56cb1e97d24f2084ab9aed4` (main after PR #74).
This is a fail-closed compatibility guard, not module-scoped storage or reactive
module capture. It does not fix OpsPulse Theme or module-to-component reads.

## Identity and first loss

SemanticProject retains source modules and per-file AST provenance.
RuntimeProjectPlan retains executable module IDs and plans, alongside a duplicate
linked projection. `runtimePlanBindingsObjectToJson` and the corresponding action
object emitter in `src/runtime_plan_json.cpp` turn linked vectors into name-keyed
objects: repeated linked names overwrite here, before reaching the browser.
The module envelopes still contain separate records, but browser
`mergeRuntimePlanIntoManifest` / `mergeProjectManifest` merge them by name without
an owner-qualified storage address. `bootstrapClientManifest`, action assignment
and derived evaluation then use one clientState dictionary.

The original counterexample is permanent in
`tests/fixtures/production/module-binding-collision/`. Private and entry modules
both declare tone. Baseline builds successfully; real Chromium clicks produce
entry -> private-updated -> entry-updated -> private-updated in the same runtime
key. See `evidence/chromium-before.json`. Both new negative build tests fail
against baseline; their assertion logs are retained in `evidence/`.

## Guard and categories

`validateBrowserRuntimeBindingOwnership` in `cli/browser_build_policy.cpp`
validates the retained executable module plans before transpilation or asset
writes. The linked projection is not counted again. Identity is module ID plus
runtime namespace/name; repeated records owned by the same module are not a
cross-module collision. Reexports do not create an additional declaring module.

- State, derived values and fetch results share the value namespace/clientState.
  Cross-category collisions are rejected too.
- Root actions and fetch-refresh aliases share action/event dispatch. A refresh
  alias can intercept an action before executeClientAction, so they are checked
  together even though their function tables are separate.
- Value names and action names are distinct namespaces and may agree.
- Components remain module-qualified; their definitions/instances are not checked
  as global bindings. Route paths are ordered match records, and route parameters
  are router inputs, not module-private root declarations. Fetch groups are
  intentional groups, not unique bindings. This guard does not invent uniqueness
  rules for those categories or change legacy inline/extern/media behavior.

All bindings in executable module plans enter the runtime regardless of UI usage,
so an unused private declaration in an imported executable module still matters.
An unimported file is not in the module graph. Non-executable module projections
are skipped just as the runtime skips their plan payload; the clean build loader
rejects failed module parsing before this validator. No source-name scanner or
reachability guess grants/excludes ownership.

RuntimePlanBinding and RuntimePlanAction retain declaration SourceSpan internally
for diagnostics. They are not added to client JSON. Diagnostics name the binding,
both categories and module paths; state/derived/action original line/column are
included when available. Fetch records currently lack retained declaration spans,
so those diagnostics report module paths. Remediation is distinct declaration
names and updated references; the compiler does not rename public bindings.

The guard applies to default/--prod browser builds. Explicit compatibility/debug
builds retain their migration behavior and are not a workaround that provides
module isolation.

## Regression and output safety

`tests/browser/module_binding_collisions.cjs` requires the unsafe clean build to
reject and verifies that a preexisting index.html is unchanged and no runtime,
component or app assets are published. A disjoint-name version then executes in
actual Chromium: private changes leave entryTone intact; entry changes leave
privateTone intact. Browser errors must be zero and browser.close() must complete.
The script is required by verify_all.sh and Ubuntu/macOS CI.

C++ coverage includes state/state, derived/derived, action/action, mixed
state/derived, state/fetch, action/refresh, disjoint names, separate namespaces,
linked/module duplication, non-executable projections, private/reexport owners,
unimported files and rejection over a previously successful output. Validation
runs before any asset writes, so previous published output remains intact; a
fresh rejected output can contain an empty directory only.

The prior module-read matrix is retained as historical unsupported evidence in
`../component-module-reads/evidence/`. Its intentionally failing C/E probe is
not a required green-gate test. All 28 fixture sources, lowered output and real
Chromium observations are preserved there.

## Remaining work

True defining-module state needs compiler binding identities carried through
initializers, derived plans, actions, mutations and subscriptions, followed by
read-only component capture with shadowing and lifecycle rules. Store-authored
derived member naming is independent. Neither is implemented by this guard.
Browser runtime and component JavaScript payload delta is zero: runtime storage
and emission are unchanged. Verification results and final candidate SHA are
recorded in the focused PR; local gate and hosted CI are distinct evidence.
