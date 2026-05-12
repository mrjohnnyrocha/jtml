# 10. Routes And Navigation

JTML routes are local, hash-based SPA routes. A route maps a path to a component:

```jtml
route "/user/:id" as UserProfile
```

`link "Ada" to "/user/ada"` navigates to `#/user/ada`. Route parameters become component arguments, so `:id` is passed to `make UserProfile id`.

Use `route "*" as NotFound` as a fallback. Layout routes are also available with `route "/path" as Page layout Shell`.
