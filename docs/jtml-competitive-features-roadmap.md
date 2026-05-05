# JTML Competitive Features Roadmap

Status: active implementation roadmap.

JTML is currently a reactive templating language with a compact syntax and a native CLI. To compete with React, Vue, Angular, Next.js, Nuxt, and SvelteKit, the project needs production features in four tiers.

## Tier 1: Must-Have

| Feature | Status |
| --- | --- |
| `fetch` async data | Implemented first slice + hardening: Friendly `let name = fetch "url"` creates `{ loading, data, error }`, supports `method` plus JSON `body`, `refresh actionName` wires a client-side re-fetch action, and browser-side re-rendering for simple `show`/`if`/`for` bindings |
| client-side `route` | Implemented first slice: Friendly `route "/path" as Component` expands to hash-routed sections, supports `:param` bindings, wildcard not-found routes, and `link ... to "/path"` lowers to hash navigation |
| scoped `style` blocks | Implemented first slice: Friendly `style` lowers to CSS scoped under `[data-jtml-app]` |
| true component isolation | Implemented first slice: repeated Friendly component calls get per-instance source-level names for local state/actions; full runtime `ComponentInstance` remains planned |

## Tier 2: Differentiators

| Feature | Status |
| --- | --- |
| built-in `store` | Implemented first slice: Friendly `store name` lowers to shared dictionary state with `name.field` reads, store-field assignments, and namespaced store actions such as `auth.logout` |
| `effect` reactive side effects | Implemented first slice: Friendly `effect variable` lowers to a generated subscription function |
| zero-config dev server | Implemented first slice: `jtml dev <file.jtml|app/>` hot-reloads and serves static assets |
| AI-native authoring | Implemented first slice: `jtml generate`, `jtml explain`, and `jtml suggest` local helpers |

## Tier 3: Ecosystem

Package management, richer type-aware diagnostics, VS Code diagnostics, and an LSP remain planned after the P0/P1 language/runtime pieces stabilize.

## Tier 4: Moonshots

Full-stack one-file JTML remains a future track. It should build on stable routing, fetch, stores, and server/runtime separation rather than shortcut around them.

## Priority Matrix

| Feature | Effort | Impact | Priority |
| --- | --- | --- | --- |
| `fetch` async data | Medium | Critical | P0 |
| Client-side routing | Medium | Critical | P0 |
| Scoped styles | Small | Critical | P0 |
| Component isolation | Large | Critical | P0 |
| Built-in `store` | Medium | High | P1 |
| `effect` reactive | Small | High | P1 |
| Zero-config dev | Small | High | P1 |
| AI-native tooling | Medium | High | P1 |
| Package manager | Large | Medium | P2 |
| Type annotations | Small first slice shipped; richer checking remains | Medium | P2 |
| IDE / LSP | Large | Medium | P2 |
| Full-stack mode | Very large | Moonshot | P3 |

## Implementation Order

1. ✅ Ship scoped styles, `jtml dev`, static asset builds, and AI-native helper commands so the authoring loop feels complete.
2. ✅ Harden `fetch` first slice: `refresh actionName` option wires a client-side re-fetch action; browser runtime intercepts the action and avoids a server round-trip. `redirect "/path"` Friendly keyword added for programmatic hash navigation from action bodies.
3. Studio revamped: 8 Friendly-2.0 samples (counter, form, dashboard, fetch, store, effects, routes, components), categorized sidebar, full reference panel covering every language feature, updated keyword highlighting.
4. Next: harden routing beyond the first slice — nested layouts, route guards, and `active-route` reactive value.
5. Next: replace source-expanded component isolation with runtime `ComponentInstance` objects and per-instance environments.
6. Revisit stores, type annotations, packages, and LSP once P0 runtime semantics are stable.
