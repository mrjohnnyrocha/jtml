# 2. State and Derived Values

`let` creates a mutable reactive variable. `get` creates a value that is automatically recomputed whenever its dependencies change. This is the core reactivity of JTML: you never manually recompute derived values, the runtime does it for you.

In the example:

- `count` is state.
- `doubled` is derived from `count`.
- `message` is derived from `count` and `doubled`.

Every interpolation `{...}` is wired up as a binding, so when the underlying variable updates, only the affected DOM nodes re-render. Reactivity is a first-class language feature — no virtual DOM, no hooks, no effects.

Try changing `let count = 0` to `let count = 7` and pressing **Run**.
