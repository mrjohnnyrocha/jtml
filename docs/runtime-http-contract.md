# JTML Runtime HTTP Contract

`jtml serve` and `jtml dev` expose a small JSON API next to the rendered page.
The browser runtime uses this API as its WebSocket fallback, and external tools
can use it to inspect or drive a running JTML app.

## Endpoints

### `GET /api/health`

Returns the CLI version and the runtime contract.

```json
{
  "ok": true,
  "version": "0.1.0-dev",
  "contract": {
    "endpoints": {
      "health": "/api/health",
      "bindings": "/api/bindings",
      "state": "/api/state",
      "components": "/api/components",
      "componentDefinitions": "/api/component-definitions",
      "runtime": "/api/runtime",
      "event": "/api/event",
      "componentAction": "/api/component-action"
    }
  }
}
```

### `GET /api/bindings`

Returns the current browser binding snapshot. The shape matches
`window.__jtml_bindings` and WebSocket `populateBindings` messages.

```json
{
  "ok": true,
  "bindings": {
    "content": {},
    "attributes": {},
    "conditions": {},
    "loops": {},
    "state": {}
  }
}
```

`bindings.state` is the browser-facing client state snapshot. It includes
user-authored top-level values such as `email`, `auth`, and fetch placeholders,
while generated binding internals like `expr_*` and `attr_*` are omitted. The
rendered page receives the same object as `window.__jtml_bindings`, so initial
fetch bodies and route guards can read state before the first event round-trip.

### `GET /api/state`

Returns runtime variables and functions visible in the current app instance.
Values are serialized as JSON when possible.

```json
{
  "ok": true,
  "state": {
    "variables": {
      "count": { "instance": 1, "kind": "normal", "value": 0 }
    },
    "functions": {
      "increment": { "instance": 1, "arity": 0, "params": [], "async": false }
    }
  }
}
```

### `GET /api/runtime`

Returns `contract`, `bindings`, and `state` in one response. Use this for IDEs,
test runners, previews, and framework hosts that want a single snapshot.

### `GET /api/components`

Returns the runtime component instance registry. This is the same component
array exposed under `state.components`, split out for devtools and tests that
only need component boundaries.

```json
{
  "ok": true,
  "components": [
    {
      "id": "Counter_1",
      "component": "Counter",
      "role": "component",
      "instance": 1,
      "sourceLine": 12,
      "params": { "label": "First" },
      "locals": {
        "count": { "lowered": "__Counter_1_count", "kind": "normal", "value": 0 },
        "add": { "lowered": "__Counter_1_add", "function": true, "arity": 0 }
      }
    }
  ]
}
```

### `GET /api/component-definitions`

Returns the component definition registry preserved from Friendly `make`
declarations. This is the migration bridge toward non-expanded runtime
`ComponentInstance` execution: tools can inspect the original component
contract even while the renderer still emits compatibility-expanded DOM.

```json
{
  "ok": true,
  "componentDefinitions": [
    {
      "name": "Counter",
      "sourceLine": 2,
      "params": ["label"],
      "body": "0:let count = 0\n0:when add\n2:count += 1\n0:box\n2:show count\n"
    }
  ]
}
```

### `POST /api/event`

Dispatches a browser event without WebSocket.

```json
{
  "elementId": "attr_1",
  "eventType": "onClick",
  "args": ["increment()"]
}
```

On success:

```json
{
  "ok": true,
  "bindings": {},
  "state": {},
  "contract": {}
}
```

On failure:

```json
{
  "ok": false,
  "error": "No onClick binding for element attr_1"
}
```

### `POST /api/component-action`

Dispatches an action through the owning component instance environment. This
lets tests, devtools, and framework hosts address a component by
`data-jtml-instance` instead of finding a DOM event binding first. Any local
state mutations recalculate bindings in that same instance before the response
is returned.

```json
{
  "componentId": "Counter_1",
  "action": "add",
  "args": []
}
```

On success the response includes fresh `bindings`, `state`, `components`, and
`contract`.

## CORS

Runtime API responses include permissive CORS headers so local development
tools, browser previews, and framework shells can inspect the app from another
localhost port.

## Stability

This is the first stable slice of the runtime contract. Future additions should
be additive: existing endpoint names and response keys should remain valid.
