# 8. Async Data With Fetch

Real apps load data. Friendly JTML gives data loading a first-class form:

```jtml
let users = fetch "/api/users"
```

The value is reactive and shaped like:

- `users.loading`
- `users.data`
- `users.error`
- `users.stale`
- `users.attempts`

In Studio and `jtml serve`, `/api/users` is a built-in local demo endpoint so the example works offline. In a real app, point `fetch` at your own API.

Use `if users.loading` for loading UI, `for user in users.data` for arrays, and `if users.error` for failure UI.
