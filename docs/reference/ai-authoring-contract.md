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

Use these forms. For the full machine-readable keyword catalog, run
`jtml keywords --json`; it is the canonical mini-reference used by tooling.

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
| styles | `theme`, semantic UI primitives, or `style` block |
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

Use modules when an app naturally grows beyond one file, but keep the current
first-slice status in mind. Relative imports are now resolved from the
importing file and are suitable for `check`, `build`, `serve --watch`, `dev`,
and `explain`. Full enterprise-grade module boundaries are still in progress.
Keep reusable components under `components/`, `modules/`, or a local package
installed under `jtml_modules/`, then import them with `use`.

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

Named imports are export-filtered. A module imported with
`use Card from "./components/card.jtml"` must contain `export make Card`;
missing exports and duplicate exported names are diagnostics, and non-exported
helpers are not pulled into the importing file. Use side-effect imports only
for setup modules that are meant to evaluate all top-level declarations.
Use `export use Card from "./card.jtml"` for small barrel modules that
re-export public declarations. Remaining module work is deeper per-file
semantic ownership and imported store identity polish.

## Fetch

Every fetch UI should handle loading and error states. For production data,
prefer explicit stale/retry/revalidation behavior instead of bare fetches.

```jtml
let users = fetch "/api/users" timeout 2500 retry 2 stale keep group people key "users" dedupe every 30000 refresh reloadUsers
let teams = fetch "/api/teams" group people lazy

when saveUser
  let saved = true
  invalidate group people

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
let users = fetch "/api/users/{id}" key id dedupe lazy stale keep

route "/users/:id" as Users load users
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
  chart line data rows by label values total forecast series "Actual,Forecast" colors "#2563eb,#9333ea" legend grid max 100 ticks 5 export svg csv
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
[`../roadmaps/media-graphics-roadmap.md`](../roadmaps/media-graphics-roadmap.md):
media `into` controllers, `graphic`,
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

Prefer semantic UI primitives for app chrome, panels, cards, grids, and
dashboard metrics:

```jtml
jtml 2

theme
  color primary "#155e75"
  space md 14

page
  shell
    sidebar
      navlink "Dashboard" to "/"
    content
      panel title "Usage" pad lg shadow md
        grid cols 2 gap md
          metric "Users" users.total "Active" tone good
```

Use modifiers where they match the primitive. `cols` belongs on `grid`;
`tone` belongs on content surfaces such as `panel`, `card`, `metric`, `alert`,
`badge`, or `toast`, not structural layout primitives such as `shell` or
`content`. `jtml lint` reports `JTML_UI_COLS_ON_NON_GRID` and
`JTML_UI_TONE_ON_LAYOUT` when generated code crosses those boundaries. Labeled
semantic surfaces are preferred: write `panel title "Usage"` or add
`aria-label`; overlays can use `modal title "Confirm"` or
`drawer title "Filters"`. Form and navigation primitives should be wired, not
decorative: `field` should wrap an input-like control and include visible text,
`aria-label`, `title`, `placeholder`, or another clear label; `tabs` should
contain `tab` children, and each `tab` should trigger an action or route target.
`modal` and `drawer` should include an obvious close/dismiss/cancel/hide
action. Do not manually add default platform roles unless overriding them:
`modal`/`drawer`, `alert`, `error`, `toast`, `loading`, `empty`, `tabs`, and
`tab` already lower to sensible accessibility roles and safe button defaults.
For ordinary action buttons, write `button "Save" click save`; JTML adds
`type="button"` automatically. Inside `form submit action`, use a plain
`button "Submit"` for the form submit button, or write an explicit `type submit`
when a clicked button should intentionally submit the form.

Use `style` when a primitive or modifier cannot express the design.

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

Use `css raw` only for trusted third-party or host-owned surfaces that need
unscoped selectors:

```jtml
css raw
  third-party-widget { display: block; }
```

Use `html raw` only for trusted custom elements or literal host markup. Do not
use it for ordinary JTML elements:

```jtml
page
  html raw "<third-party-widget data-mode=\"demo\"></third-party-widget>"
```

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
4. Inspect Compatibility/HTML Artifacts

Machine-readable repair tools should prefer JSON diagnostics:

```sh
jtml explain file.jtml --json
jtml check file.jtml --json
jtml fix file.jtml --json
jtml lint file.jtml --json
```

`jtml explain --json` includes `semantic.nodes`, `semantic.dependencies`,
`semantic.attributes`, `semantic.routeRecords`, `semantic.fetchRecords`,
`semantic.componentDefinitions`, `semantic.componentInstances`, top-level
semantic lists such as `imports`, and observable issue data produced from the
semantic graph. AI
agents should use these fields to confirm what the program declares, what
depends on what, which routes/components/fetch loads define the app shape,
which modules are part of the app shape, and whether attributes are being
understood as literal, boolean, reactive, event, special runtime metadata, or
passthrough platform attributes. Static platform attributes such as
`class "card"`, `style "padding: 12px"`, `aria-label "Close"`, and `data-*`
literals should not become generated reactive bindings.
Raw and host escape hatches appear as `semantic.nodes.rawCssBlocks`,
`semantic.nodes.rawHtmlBlocks`, and `semantic.nodes.externs`; `jtml lint`
reports `JTML_RAW_CSS_ESCAPE_HATCH`, `JTML_RAW_HTML_ESCAPE_HATCH`, and
`JTML_EXTERN_ESCAPE_HATCH` warnings so agents can flag them for human review.

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
