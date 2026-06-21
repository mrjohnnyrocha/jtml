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
      "renderedComponents": "/api/rendered-components",
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

Returns `contract`, `bindings`, `state`, `renderedComponents`, and component
definitions in one response. Use this for IDEs, test runners, previews, and
framework hosts that want a single snapshot.

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
      },
      "runtime": {
        "mode": "semantic-instance",
        "ownsEnvironment": true,
        "ready": true,
        "environmentId": "Counter_1",
        "definition": "Counter",
        "state": ["count"],
        "actions": ["add"],
        "derived": [],
        "effects": []
      }
    }
  ]
}
```

### `GET /api/component-definitions`

Returns the component definition registry preserved from Friendly `make`
declarations. This is the migration bridge toward non-expanded runtime
`ComponentInstance` execution: tools can inspect the original component
contract and semantic runtime plan even while the renderer still emits
compatibility-expanded DOM.

```json
{
  "ok": true,
  "componentDefinitions": [
    {
      "moduleId": 0,
      "name": "Counter",
      "sourceLine": 2,
      "params": ["label"],
      "bodyNodeCount": 4,
      "rootTemplateNodeCount": 1,
      "slotCount": 0,
      "localState": ["count"],
      "localActions": ["add"],
      "localDerived": [],
      "localEffects": [],
      "eventBindings": ["add"],
      "runtimePlan": {
        "mode": "semantic-instance",
        "moduleId": 0,
        "ownsEnvironment": true,
        "state": ["count"],
        "actions": ["add"],
        "derived": [],
        "effects": [],
        "bodyNodeCount": 4,
        "rootTemplateNodeCount": 1,
        "slotCount": 0,
        "hasSlot": false,
        "bodyPlan": [
          {
            "definitionModule": null,
            "kind": "template",
            "name": "box",
            "renderRoot": true
          }
        ]
      },
      "body": "0:let count = 0\n0:when add\n2:count += 1\n0:box\n2:show count\n"
    }
  ]
}
```

`moduleId` identifies the module that authored the component definition.
Nested component-call body-plan nodes carry `definitionModule` when the target
component was resolved through a module import/export boundary; `null` means the
node is a platform element, unresolved, or compatibility-only.

### `GET /api/rendered-components`

Returns the first-slice live body-plan render surface. Components whose
body-plan templates are supported include `renderedHtml`; unsupported or empty
renders explicitly keep `renderedHtmlSupported: false` and
`fallback: "compatibility-dom"`. Supported component wrappers may already be
rendered from this body-plan HTML in the initially served document and marked
with `data-jtml-live-body-plan-transport="body-plan"` plus a
`data-jtml-live-body-plan-rendered-hash` value. The browser runtime compares
that hash with future `/api/rendered-components` payloads, so already-current
server-rendered body-plan components do not need a startup DOM patch. The
endpoint remains available for fallback refreshes, `/api/event`, and
`/api/component-action`, while leaving the older binding-based DOM path in
place for unsupported fallback shapes. Supported rendered buttons include
`data-jtml-direct-component-*` action attributes so rendered live templates can
call `/api/component-action` without falling back to generated compatibility
events.

```json
{
  "ok": true,
  "mode": "live-body-plan",
  "supported": true,
  "fallback": "compatibility-dom",
  "components": [
    {
      "id": "Card_1",
      "component": "Card",
      "supported": true,
      "fallback": "",
      "renderedHtmlSupported": true,
      "renderedHtml": "<section class=\"jtml-card\">...</section>"
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
  "renderedComponents": {},
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

On success the response includes fresh `bindings`, `state`,
`renderedComponents`, `components`, and `contract`. Browser-rendered component
templates and patched live body-plan templates both use this endpoint for
direct component action dispatch where the body-plan action subset is
supported.

## CORS

Runtime API responses include permissive CORS headers so local development
tools, browser previews, and framework shells can inspect the app from another
localhost port.

## Stability

This is the first stable slice of the runtime contract. Future additions should
be additive: existing endpoint names and response keys should remain valid.
