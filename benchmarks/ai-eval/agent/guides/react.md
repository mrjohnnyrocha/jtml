# React stack guide v1

## Toolchain and files
Use React 19.2.0 and React DOM 19.2.0 with JSX and Vite 7.2.2. The application
entries are `index.html` and `src.jsx`. Mount with `createRoot` from
`react-dom/client`; import React for JSX. Build with `vite build --outDir dist`.
Output is a browser-local application. Do not edit generated files. Evaluation
uses the provisioned Vite config rather than arbitrary candidate config plugins.

## State and computed output
Declare writable component state with `const [count, setCount] = useState(0)`.
Compute derived values during render, for example `const doubled = count * 2`.
Expressions can include arithmetic, comparisons, boolean operators, ternaries and
template strings. Use the setter to assign state from an event handler. Derived
values are recomputed when React renders; they are not writable storage. Use
separate state when displayed output should represent an earlier snapshot rather
than track current edits. Render strings as text, including `<` and `&`.

## Components and ownership
Define a function component such as `function Meter({title}) { ... }` and render
it as `<Meter title="Example"/>`. Use `createRoot` to mount the application once.
Each instance owns its `useState` values. Props supply input values; avoid mutating
props. Prefer a small reusable component for repeated interactive UI. Two instances
of a component must have independent local state. Declare component types outside
render functions so updates preserve component and DOM identity.

## Events and actions
Connect a named handler with a prop such as `<button onClick={increment}>`.
Use `onSubmit` for a native form and call `event.preventDefault()` in the handler.
Use `type="submit"` for the submit button and `type="button"` for other controls
inside a form. Handled submission must not navigate. Event handlers may assign
component state or call other named handlers. Prefer simple named functions over
inline imperative DOM manipulation. Functional setter updates are useful when a
new state value depends on its preceding value.

## Inputs and accessibility
For controlled text inputs, supply a string `value={draft}` and update it in
`onChange`, using `event.target.value`. State assignments then update the displayed
value. Use a native wrapping `<label>` or `htmlFor`/`id` association; an explicit
`aria-label` gives a predictable accessible name. Keep the value a string rather
than switching between undefined and a string. Stable element positions and keys
help preserve keyboard focus. Do not replace a text control during normal edits.

## Rendering and conditions
Use JSX with native elements such as `<h1>`, `<p>` and `<section>`.
Render text as `{expression}` and attributes such as `aria-label="Details"`.
Conditional UI may use `condition && <Control/>` or a ternary. Keep application
state in the owning component so conditional controls initialize from that state.
Use event handlers and setters for UI changes. No external library or backend is
needed for browser-local tasks; direct HTML insertion is unnecessary for text.

## Local checks and browser
Build, serve `dist/` on loopback and write your own Playwright checks if useful.
Node, React/Vite dependencies and Playwright/Chromium are preinstalled for both
stacks in the same image. Shell commands run from `/work`; `node_modules` points
to read-only `/opt/deps/node_modules`. `vite` is on PATH. A command has a 60-second
maximum and may be terminated sooner by the trial wall limit. File edits, builds
and self-tests consume trial time. There is no network access. The independent
evaluator is unavailable during the session.

## Boundaries
Implement application logic in the assigned stack. Do not use external scripts,
remote APIs, `eval` or `new Function`. Preserve build diagnostics in shell output
and work within the task contract. Use its observable requirements to decide
behavior. Avoid `dangerouslySetInnerHTML` for user-supplied strings. This guide
contains no acceptance test code or reference application.
