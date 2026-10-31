# Bounded caller-authorized text binding contract

This is a new browser-local language feature. Existing `input into name` remains
component-local. Enterprise is unchanged and remains NO-GO for edited-email login.

```jtml
let email = "seed@example.test"
make EmailEditor label bind value
  page
    input into value aria-label label
    text "Draft: {value}"
page
  EmailEditor "Account" bind value to email
```

`bind value` declares exactly one input-binding parameter, following ordinary
value parameters and preceding optional `emits`. Every call must explicitly pass
`bind value to target`. It is not an ordinary positional argument. Omission,
unknown/duplicate bindings, a matching local declaration and binding forwarding
are errors. Ordinary components, value parameters and local state keep their
existing semantics. An ordinary value parameter does not become writable.

The initial subset grants text-input authority only. `input into value` reads
and writes the caller's authorized root string state. Other assignments to a
binding parameter, object/member/collection targets, non-text controls, nested
component forwarding and route/layout parameter binding are unsupported. Caller
state must be a simple mutable `let` owned by the entry module with a literal
string initializer. Imported components may receive this capability; imported
state, constants, derived state, borrowed values and ambiguous cross-module names
cannot be targets. Sharing the same target between two explicitly bound instances
is intentional; otherwise siblings remain isolated. Callers can revoke authority
by removing the instance. Remounting receives current owner state.

Semantic IR must carry declarations and explicit arguments separately from value
parameters. Runtime Plan must prove caller ownership from retained module AST,
reject unsupported shapes with original call/definition locations, and construct
an input action owned by that module. The instance receives the compiler-resolved
action identity and a compiled read argument plan. Browser listeners must invoke
that action, never search globals or resolve an authored target name. The existing
root executor commits writes and effects; existing argument updates patch retained
controls without resetting unchanged values/focus/selection. No separate action
interpreter, mutable global fallback or parent-state traversal is permitted.

Unsupported execution backends must reject the feature, rather than silently
lower it to implicit outer writes. Clean public schemas must admit only the
specific generated capability field. Runtime growth must fit the existing
273,000-byte budget without dropping assertions or other supported behavior.

Implementation is conditional on proving these invariants within the current
compiler. If current ownership/runtime representation cannot carry this contract
without weakening isolation, retain this as a proposal and report the blocker.


Production privacy preserves the existing numeric `sourceLine`/`sourceColumn`
execution-node coordinates in public body/slot plans. They already participate in
static-plan identity on main. This feature does not remove them or change hashing.
The capability's original locations, binding arguments/declarations and raw
binding-argument markers do not enter the public capability projection.
