# Defining-module scalar component capture

The baseline is PR #86 merge
`efd738ecc560c896201385fc93f8ccafd96396fc`.
Unchanged Reactive Core V10 compiles and maintains root values
`tone="private", label="private!"`, then
`tone="private!", label="private!!"`.
Its component text is empty at both observations in strict-CSP Chromium.

The component definition retains its module ID through Runtime Project Plan.
Its expression plan reads the lexical name `label`, but the runtime instance
scope originally contained only parameters and local declarations. Generated
text functions therefore found no value. Root publication already computed
the getter correctly.

The initial clean-browser slice covers components defined in imported modules.
The compiler certifies free scalar reads in component text and
component-local initializer/formula expressions. It resolves the defining
module's declaration first, certifies scalar getter dependency closures through
retained import authority, and emits a lexical-name-to-storage-slot map on
the owned component definition. Existing module ownership guards must prove
unique slots. Captures with colliding module storage remain unsupported.
Parameters, local declarations, action parameters and iterators retain their
lexical precedence.

Each instance holds its own scalar capture snapshots. Root state/derived/effect
publication refreshes those snapshots and passes changed lexical names into
the existing generated component update function. Creation reads current root
values before initializing component-owned state. Local derived values continue
through their existing evaluation path. The active registry and existing
teardown determine which instances receive updates. Keyed reorder retains
component state and DOM.

Captured names enter the existing borrowed-binding certification. Component
actions cannot assign them, and nested declarations cannot overwrite a capture
through the shared instance scope. Scalar copies introduce no mutable aliases.
A runtime shape guard rejects a value that ceases to be scalar.

Explicit template `into` and controller bindings retain their separately
validated bridge and value-shape contract. Structured object capture, general
closures, entry-defined component captures, attribute-only captures and action-only captures are outside this
initial slice. Component parameters remain an independently supported way to
pass values. Compatibility/debug receive their existing definitions without
capture maps. A6 failure and recovery policy remains open.

Run `node tests/browser/component_module_capture.cjs` with the required
Chromium and Playwright environment. The suite preserves original V10/V09
source and oracles, drives real controls, instruments generated updates and
create/destroy calls, checks instance generations and keyed DOM identity,
and verifies source-located rejection before asset emission with previous
bundles preserved byte for byte.
