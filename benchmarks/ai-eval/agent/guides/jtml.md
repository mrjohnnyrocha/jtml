# JTML stack guide v1

## Toolchain and files
Use Friendly JTML 2. The application entry is `main.jtml`, beginning with `jtml 2`.
Indentation defines bodies. The provided compiler is pinned to the campaign's
source commit and binary hash. Build with `jtml build main.jtml --target browser
--out dist`. Output is a browser-local application. Do not edit generated files.

## State and computed output
Declare writable state with `let count = 0`. Declare a computed value with
`get doubled = count * 2`. Expressions can include arithmetic, comparisons,
boolean operators, ternaries and interpolated strings such as `"Count: {count}"`.
Assign local state by name inside actions. Derived values are recomputed from
state dependencies; they are not writable storage. Use separate writable state
when a displayed result should represent an earlier snapshot rather than track
current edits. Strings should remain text, including characters like `<` and `&`.

## Components and ownership
Define a component with `make Meter title`; indent its state, actions and UI.
Instantiate it with `Meter "Example"`. A `page` body contains the application UI.
Each component owns its local state. Plain parameters supply input values; avoid
mutating a parameter. Prefer a small reusable component for repeated interactive
UI. Two instances of a component must have independent local writable state.

## Events and actions
Define a named action with `when increment` and an indented assignment such as
`count += 1`. Connect it with `button "Increment" click increment`. A native form
uses `form submit commit` with a named action and a `button "Commit" type "submit"`.
Use `type "button"` for non-submit controls inside a form. Handled form submission
must not navigate. Action arguments and event shorthands are compiler-owned;
prefer simple named actions over embedding JavaScript.

## Inputs and accessibility
A text input can use `input into draft aria-label "Draft"`. Its target must be a
simple component-local writable `let`. This binds text entry to state and state
assignments back to the displayed value. A native `label "Draft"` may wrap a
control; an explicit `aria-label` gives a predictable accessible name. Text controls
should retain identity and keyboard focus during updates. Do not infer broad
numeric, checkbox, select or file coercion guarantees from text input support.

## Rendering and conditions
Use `text expression`, `h1 "Heading"`, `section` and other native element names.
Attributes use name/value pairs, for example `section aria-label "Details"`.
Conditional UI uses `if condition` and an indented body, optionally `else`.
Keep application state in the owning component; conditional controls should read
that state when created. Use named actions and normal state assignments for UI
changes. No external library or backend is needed for browser-local tasks.

## Local checks and browser
Run `jtml check main.jtml` for diagnostics. Build, serve `dist/` on loopback and
write your own Playwright checks if useful. Node, React/Vite dependencies and
Playwright/Chromium are preinstalled for both stacks in the same image. Shell
commands run from `/work`; `node_modules` points to read-only `/opt/deps/node_modules`.
A command has a 60-second maximum and may be terminated sooner by the trial wall
limit. File edits, builds and self-tests consume trial time. There is no network
access. The independent evaluator is unavailable during the session.

## Boundaries
Implement application logic in the assigned stack. Do not use external scripts,
remote APIs, `eval` or `new Function`. Source diagnostics may describe unsupported
shapes; preserve those diagnostics in your shell output and work within the task
contract. Use the task's observable requirements to decide behavior. This guide
contains no acceptance test code or reference application.
