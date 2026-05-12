# 3. Actions and Events

Friendly actions are declared with `when actionName`. You attach them to DOM events using lowercase event sugar: `click`, `input`, `change`, `submit`.

When an event fires in the browser, JTML sends a message to the running interpreter, the action runs, and any mutated state is pushed back to the page. You write **no JavaScript** — the language and the runtime handle the wiring.

Click the buttons on the right. Notice that `doubled` stays in sync with `count` automatically because it's a `get` value.
