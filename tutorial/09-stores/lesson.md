# 9. Stores For Shared State

Use local `let` state for a page or component. Use `store name` when several parts of an app need the same state.

A store is a named namespace:

```jtml
store auth
  let user = "Ada"

  when logout
    let user = ""
```

Read store fields with `auth.user` and call store actions with `auth.logout`. Store field updates are reactive, so conditionals and text that read `auth.user` update after the action runs.
