# JTML AI Authoring Contract

This document is the stable contract for AI systems that generate or edit JTML.
The goal is simple: produce readable `jtml 2` code that compiles, formats, lints,
and can be maintained by a human.

## Default Dialect

Always generate Friendly JTML unless the user explicitly asks for Classic JTML.
Classic is a compatibility and artifact dialect; it is useful for migration,
embedding, and debugging lowered output, but it is not the default authoring
surface.

```jtml
jtml 2

let count = 0

when add
  count += 1

page
  h1 "Counter"
  button "Add" click add
```

Do not generate raw JavaScript. Do not generate framework code. JTML should stay
readable as a document.

## Canonical Keywords

Use these forms:

| Concept | Use |
| --- | --- |
| mutable state | `let name = value` |
| constants | `const name = value` |
| derived values | `get name = expression` |
| actions/events | `when actionName` |
| root UI | `page` |
| text output | `show expression` or `text "..."` |
| components | `make ComponentName param` |
| shared state | `store name` |
| async data | `let result = fetch "/api/url"` |
| routing | `route "/path" as Component` |
| styles | `style` block |
| side effects | `effect variable` |

Avoid synonyms and clever rewrites. One obvious spelling is better than many.

## Preferred Element Vocabulary

Prefer the Friendly element dictionary:

| Friendly | HTML |
| --- | --- |
| `page` | `main` |
| `box` | `div` |
| `text` | `p` |
| `link` | `a` |
| `image` | `img` |
| `video` | `video` |
| `audio` | `audio` |
| `embed` | `iframe` |
| `list` / `item` | `ul` / `li` |
| `list ordered` | `ol` |
| `checkbox` | `input type="checkbox"` |

Use raw HTML tag names only when no Friendly alias exists.

## State And Events

Prefer small, named actions:

```jtml
let email = ""
let saved = false

when save
  if email != ""
    saved = true

page
  input "Email" into email
  button "Save" click save
```

Use `into` for form inputs. Use `click`, `input`, `change`, or `submit` event
sugar instead of Classic `onClick=...` in Friendly files.

## Components

Components must start with an uppercase letter. Keep component parameters plain
and explicit.

```jtml
make Card title
  box class "card"
    h2 title
    slot

page
  Card "Revenue"
    text "$12k"
```

Do not rely on shared local names between component calls. Each instance should
be understandable on its own.

## Modules

Use modules when an app naturally grows beyond one file. Keep reusable
components under `components/`, `modules/`, or a local package installed under
`jtml_modules/`, then import them with `use`.

```jtml
use Card from "./components/card.jtml"

page
  Card "Revenue"
    text "$12k"
```

Imported Friendly modules are resolved relative to the importing file before
lowering, so imported components can be called like local components. Bare
package imports such as `use Button from "ui-kit"` resolve through the nearest
`jtml_modules/ui-kit/index.jtml` or `jtml_modules/ui-kit/package.jtml`.

For reusable modules, export the public API and keep helpers private:

```jtml
// components/card.jtml
jtml 2
export make Card title
  box class "card"
    h2 title
    slot
```

Named imports only pull matching exported top-level declarations when a module
uses `export`. Use side-effect imports only for setup modules that are meant to
evaluate all top-level declarations.

## Fetch

Every fetch UI should handle loading and error states.

```jtml
let users = fetch "/api/users" timeout 2500 retry 2 stale keep refresh reloadUsers

when saveUser
  let saved = true
  invalidate users

page
  button "Reload" click reloadUsers
  button "Save" click saveUser
  if users.loading
    text "Loading..."
  else
    for user in users.data
      text "{user.name}"
  if users.error
    text "Error: {users.error}"
```

For route-only data, prefer lazy fetches:

```jtml
let users = fetch "/api/users" lazy stale keep

route "/users" as Users load users
```

For writes, use explicit request options:

```jtml
let login = fetch "/api/login" method "POST" body { email: email } cache "no-store" credentials "include"
```

## Media And Graphics

For media today, generate standards-based elements. Always include useful
`alt` text for `image`; use `controls` for audio/video unless the user asks for
another playback model.

```jtml
jtml 2

let rows = [{ "label": "Free", "total": 12 }, { "label": "Pro", "total": 22 }]

page
  image src "/assets/product.png" alt "Product screenshot"
  video src "/assets/demo.mp4" controls poster "/assets/demo-poster.jpg"
  audio src "/assets/intro.mp3" controls
  file "Choose image" accept "image/*" into selectedImage
  dropzone "Drop media" accept "image/*,video/*" into assets
  video src "/assets/walkthrough.mp4" controls into player
  button "Play" click player.play
  button "Pause" click player.pause
  graphic aria-label "Revenue bars" width "320" height "120" viewBox "0 0 320 120"
    bar x "20" y "40" width "70" height "60" fill "#0f766e"
    dot cx "250" cy "55" r "12" fill "#111827"
    line x1 "20" y1 "104" x2 "300" y2 "104" stroke "#475569" stroke-width "2"
    path d "M20 90 C90 20 180 120 300 40" fill "none" stroke "#9333ea" stroke-width "3"
  chart bar data rows by label value total label "Revenue" color "#2563eb"
  scene3d "Product preview" scene productScene camera orbit controls orbit renderer "three" into sceneState width "640" height "360"
```

Do not invent unimplemented media-processing syntax in runnable code. For
custom drawing, charting, waveform rendering, image editing, or video tooling
today, use `extern` and make the host dependency explicit:

```jtml
extern drawChart from "app.drawChart"

page
  canvas id "chart" aria-label "Revenue chart" width "800" height "320"
  button "Draw chart" click drawChart("chart")
```

When proposing future media features, use the planned vocabulary from
`docs/media-graphics-roadmap.md`: media `into` controllers, `graphic`,
`chart`, and `scene3d`.

## Routing

Always include a fallback route in multi-route apps.

```jtml
route "/" as Home
route "/user/:id" as UserProfile
route "*" as NotFound
```

Route parameters must match component parameters exactly:

```jtml
route "/user/:id" as UserProfile

make UserProfile id
  page
    h1 "User {id}"
```

Use `link "Label" to "/path"` for navigation. Use `active-class` for nav state
and `guard` for protected routes.

## Stores

Use stores for shared state only. Local page/component state should stay local.

```jtml
store auth
  let user = ""
  let token = ""

  when logout
    let user = ""
    let token = ""

page
  button "Logout" click auth.logout
```

Store actions are called through the store-qualified form, such as `auth.logout`.

## Styles

Prefer scoped `style` blocks. Keep selectors simple and close to the component
or page that uses them.

```jtml
style
  .card
    padding: 16px
    border: 1px solid #ddd
    border-radius: 8px
```

Avoid giant global style sheets in generated examples.

## Types

Use optional type annotations when they clarify app contracts. The first
linting slice checks obvious primitive mismatches.

```jtml
let count: number = 0
const title: string = "Dashboard"
get readyLabel: string = ready ? "Ready" : "Loading"
```

Supported primitive names are `number`, `string`, `bool`, `array`, `object`,
and `any`.

## Repair Loop

After generating or editing JTML, run:

```sh
jtml fix file.jtml -w
jtml fmt file.jtml -w
jtml check file.jtml
jtml lint file.jtml
jtml build file.jtml --out dist
```

In Studio, use:

1. Format
2. Run
3. Inspect Diagnostics
4. Inspect Classic/HTML Artifacts

Machine-readable repair tools should prefer JSON diagnostics:

```sh
jtml explain file.jtml --json
jtml check file.jtml --json
jtml fix file.jtml --json
jtml lint file.jtml --json
```

Diagnostics use this stable shape:

```json
{
  "severity": "error",
  "code": "JTML_PARSE",
  "message": "Parser Error: Expected element body at line 4, column 5",
  "line": 4,
  "column": 5,
  "hint": "Check the surrounding JTML syntax and indentation.",
  "example": "page\n  h1 \"Hello\"\n  button \"Save\" click save"
}
```

## Common Mistakes To Avoid

- Missing `jtml 2` header.
- Calling an action that has no `when actionName`.
- Route params that do not match component params.
- Fetch UI without loading/error states.
- Images without meaningful `alt` text.
- Audio/video without controls unless there is an explicit custom controller.
- Shared app state placed in globals when it belongs in a `store`.
- Using Classic terminators (`\\`, `#`) in Friendly code.
- Formatting Friendly source through a Classic-only formatter.

## Output Requirements For AI

When asked to produce code:

1. Output complete files, not fragments, unless the user asks for a snippet.
2. Prefer one file until the app naturally needs modules.
3. Include a runnable `page`.
4. Include minimal scoped styles for usable UI.
5. Include loading/error states for data.
6. Include `route "*" as NotFound` for routed apps.
7. Use current media elements for runnable code; mention planned media/graphics
   syntax only as roadmap text.
8. Keep code direct and boring. JTML should be easy to read after the AI leaves.
