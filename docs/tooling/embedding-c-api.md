# JTML C ABI

JTML ships a small C ABI so native hosts can embed the renderer and runtime
without shelling out to the `jtml` CLI. The ABI is intentionally narrow:
compile a source string, hold an opaque runtime context, inspect JSON snapshots,
and dispatch events/actions.

## Header And Library

Release archives include:

```text
include/jtml/c_api.h
lib/libjtml.*
```

The public handle is opaque:

```c
typedef struct jtml_context jtml_context;
```

All strings returned by the API are heap allocated by JTML. Release them with
`jtml_free`.

## Static Render

```c
char* html = 0;
char* error = 0;

if (!jtml_render("jtml 2\npage\n  h1 \"Hello\"\n", &html, &error)) {
  fprintf(stderr, "%s\n", error);
  jtml_free(error);
  return 1;
}

puts(html);
jtml_free(html);
```

`jtml_render` accepts Friendly or Classic syntax and returns a complete HTML
document.

## Runtime Context

Use a context when the host needs bindings, state, components, or dispatch:

```c
jtml_context* ctx = jtml_create();
char* html = 0;
char* error = 0;

if (!jtml_load(ctx, source, &html, &error)) {
  fprintf(stderr, "%s\n", error);
  jtml_free(error);
  jtml_destroy(ctx);
  return 1;
}

char* state = jtml_state(ctx);
char* bindings = jtml_bindings(ctx);
char* components = jtml_components(ctx);
char* definitions = jtml_component_definitions(ctx);

jtml_free(state);
jtml_free(bindings);
jtml_free(components);
jtml_free(definitions);
jtml_free(html);
jtml_destroy(ctx);
```

`jtml_state`, `jtml_bindings`, `jtml_components`, and
`jtml_component_definitions` return the same JSON shapes as the HTTP runtime
API.

## Dispatch

Events mirror `POST /api/event`:

```c
char* updated = 0;
char* error = 0;

if (!jtml_dispatch(ctx, "attr_1", "onClick", "[]", &updated, &error)) {
  fprintf(stderr, "%s\n", error);
  jtml_free(error);
}
jtml_free(updated);
```

Component actions mirror `POST /api/component-action`:

```c
char* updated = 0;
char* error = 0;

if (!jtml_component_action(ctx, "Counter_1", "add", "[]", &updated, &error)) {
  fprintf(stderr, "%s\n", error);
  jtml_free(error);
}
jtml_free(updated);
```

`args_json` must be a JSON array. Pass `NULL` or `""` for an empty array.

## Stability Contract

The C ABI is additive:

- Existing function names and argument order should not change.
- Returned JSON keys should follow the HTTP runtime contract and only grow
  additively.
- Hosts must treat `jtml_context` as opaque.
- Hosts must release returned strings with `jtml_free`, never `free`.
