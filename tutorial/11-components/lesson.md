# 11. Components And Isolation

Components start with `make` and are called with an uppercase name:

```jtml
make Counter label
  let count = 0
  ...

page
  Counter "One"
  Counter "Two"
```

Each component call gets isolated local state and actions. Clicking the first counter below does not change the second counter.

Use `slot` when a component should accept children from its caller.
