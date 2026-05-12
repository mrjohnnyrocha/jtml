# JTML Language Reference

Version: 0.1.0-dev

JTML is a compact syntax for building reactive HTML pages. It keeps HTML's
element and attribute model, then adds state, derived values, actions, events,
components, modules, async data, routing, stores, effects, tooling, and a small
runtime contract.

## Files

JTML source files use `.jtml`.

Friendly JTML 2 is the canonical authoring dialect. Start a file with `jtml 2`:

```jtml
jtml 2

let count = 0

when add
  count += 1

page
  h1 "Counter"
  show "Count: {count}"
  text "Doubled: {doubled}"
  button "Add" click add
```

Friendly syntax is indentation-based and lowers through the Classic AST/runtime
pipeline for compatibility. The current production authoring surface includes
`use`, `export`, `let`, `const`, `get`, `when`, `make`, `slot`, `page`,
`show`, element aliases, `style`, `fetch`, `route`, `layout`, `load`, `guard`,
`redirect`, `store`, `effect`, `extern`, `if`/`else`, `for`, `while`,
`try`/`catch`/`finally`, `return`, `throw`, `break`, `continue`, common DOM
event shorthands, and `input ... into name` bindings. Expressions support
interpolated strings (`"Hello {name}"`), ternaries (`condition ? a : b`),
arrays, dictionaries, dotted reads, index reads, function calls, and compound
assignment.

Classic JTML remains supported for older files and for generated compiler
artifacts. Use `--syntax classic` to force Classic parsing, or `jtml migrate
old.jtml -o new.jtml` to convert most Classic files to Friendly.

## Friendly JTML 2 vs Classic

Use **Friendly JTML 2** for new source files, tutorials, examples, AI-generated
code, Studio editing, and application code. It is indentation-based, shorter,
and designed to be read by humans and generated reliably by AI.

Use **Classic JTML** when maintaining older files, inspecting the Studio
Artifacts panel, testing the compatibility runtime, embedding JTML through the
C ABI, or debugging the compiler's lowered representation. Classic is stable;
it is not the recommended spelling for new apps.

| Need | Choose | Why |
| --- | --- | --- |
| New app/page/component | Friendly `jtml 2` | Canonical public dialect |
| AI-authored code | Friendly `jtml 2` | Fewer tokens, fewer delimiters, clearer intent |
| Existing old `.jtml` file | Classic or `jtml migrate` | Preserve compatibility, migrate when ready |
| Compiler/runtime artifact inspection | Classic | This is the lowered internal compatibility form |
| Embedding/runtime integration | Either input, Classic artifact for debugging | Both lower to the same AST/runtime |

Friendly syntax includes async data:

```jtml
let users = fetch "/api/users"

page
  if users.loading
    text "Loading..."
  else
    for user in users.data
      text "{user.name}"
  if users.error
    text "Could not load users: {users.error}"
```

`fetch` creates a reactive browser-side value shaped as `{ loading, data,
error, stale, attempts }`. It supports simple GET fetches, JSON request bodies
for non-GET calls, browser cache/credentials policies, timeout, retry, stale
data policy, and a `refresh` action that re-triggers the fetch client-side
without a server round-trip:

```jtml
let login = fetch "/api/login" method "POST" body { email: email } cache "no-store" credentials "include"

let posts = fetch "/api/posts" timeout 2500 retry 2 stale keep refresh reloadPosts

when savePost
  let saved = true
  invalidate posts

page
  button "Reload" click reloadPosts
  button "Save" click savePost
  for post in posts.data
    text "{post.title}"
```

`timeout` is in milliseconds. `retry 2` means "try once, then retry up to two
more times". `stale keep` preserves the previous successful `data` value during
refresh/loading and after an error; `stale clear` is the default. Use
`invalidate posts` inside a `when` action to refresh one or more named fetches
after the action has dispatched. That is the mutation-friendly form: the action
runs first, then the browser revalidates the listed fetch bindings.

Use `lazy` when a fetch should be registered but not started immediately.
Route declarations can then load it only when the route matches:

```jtml
let users = fetch "/api/users" lazy stale keep

route "/users" as Users load users
```

The browser runtime handles simple dotted reads such as `users.loading`,
`users.data`, `login.data.user`, and `user.name` inside loop templates. Missing
nested data is falsey while a fetch is still loading, so `if login.data.user`
can safely render its fallback until the response arrives.

Friendly routes map hash paths to components:

```jtml
route "/" as Home
route "/user/:id" as UserProfile
route "*" as NotFound

make Home
  page
    h1 "Home"
    link "Open user 42" to "/user/42"

make UserProfile id
  page
    h1 "User"
    show id

make NotFound
  page
    h1 "Not found"
```

Route parameters must match component parameters in order. `link ... to "/path"`
lowers to a router-owned hash target such as `data-jtml-href="#/path"` with an
inert default `href`, so local single-page routes work inside Studio previews,
static exports, and embedded pages. Use `route "*" as Component` as the last
route for a not-found fallback.

Routes can load lazy fetches when they become active:

```jtml
let users = fetch "/api/users" lazy stale keep

route "/users" as Users load users
```

Routes can be wrapped with a zero-parameter layout component:

```jtml
make AppLayout
  box
    nav
      link "Home" to "/"
    slot

route "/" as Home layout AppLayout
```

The route page is injected at the layout's `slot`, keeping shared navigation
and shell UI outside each route component.

`link` supports an optional `active-class` argument that adds a CSS class when
the link's path matches the current route:

```jtml
link "Home" to "/" active-class "active"
link "Dashboard" to "/dashboard" active-class "active"
```

The browser runtime updates the class on every hash change. This replaces
manual `if activeRoute == "/"` guards in nav templates.

`activeRoute` is a built-in reactive variable that always holds the current
normalized hash path (e.g. `"/"`, `"/dashboard"`). Any element with a
`data-jtml-expr` binding that reads `activeRoute` updates automatically.

Route guards block navigation when a variable is falsy:

```jtml
guard "/dashboard" require token
guard "/admin" require isAdmin else "/login"
```

The optional `else "/path"` redirects the user when the guard fails. The
compiler emits a `<meta>` element; the browser intercepts hash navigation
before the route section becomes visible. Guards fire on every hash change.

`redirect "/path"` navigates to a route programmatically from inside a `when`
block without a server round-trip. The compiler emits a `<meta>` marker that
the browser runtime reads on load; clicking the triggering button is
intercepted client-side before the server is involved:

```jtml
when goHome
  redirect "/"

page
  button "Back to home" click goHome
```

## Media And Graphics

JTML currently supports standards-based media embedding through Friendly
element aliases:

```jtml
jtml 2

page
  image src "/assets/photo.jpg" alt "Team photo"
  video src "/assets/demo.mp4" controls poster "/assets/demo.jpg"
  audio src "/assets/intro.mp3" controls
  embed src "https://example.com/widget" title "Widget"
```

Use `image` for `<img>`, `video` for `<video>`, `audio` for `<audio>`, and
`embed` for `<iframe>`. Standard attributes such as `src`, `alt`, `controls`,
`poster`, `width`, `height`, `autoplay`, `muted`, `loop`, and `preload` lower
as ordinary HTML attributes when written in Friendly element syntax.

File inputs are also available. `file` lowers to `<input type="file">`, and
`dropzone` lowers to a multiple file input with a `data-jtml-dropzone` marker.
Both work with `into`; the browser runtime stores selected files as objects
with `name`, `type`, `size`, `lastModified`, `preview`, and `url`.

```jtml
jtml 2

let selected = ""
let assets = []

page
  file "Choose image" accept "image/*" into selected
  dropzone "Drop media" accept "image/*,video/*" into assets
  if selected
    image src selected.preview alt selected.name
  text "Assets: {assets.length}"
```

`video` and `audio` can also bind browser playback state with `into`. This is
a browser-side controller: it works in previews and static exports without
inventing a server media object.

```jtml
jtml 2

page
  video src "/assets/demo.mp4" controls into player
  text "Paused: {player.paused}"
  text "Time: {player.currentTime} / {player.duration}"
  button "Play" click player.play
  button "Pause" click player.pause
  button "Restart" click player.seek(0)
```

The controller state includes `currentTime`, `duration`, `paused`, `ended`,
`muted`, `volume`, `playbackRate`, `readyState`, and `src`. First-slice
client actions are `play`, `pause`, `toggle`, `seek(seconds)`, and
`setVolume(0..1)`.

For custom graphics today, use raw HTML/SVG-compatible elements or an explicit
host bridge with `extern`:

```jtml
jtml 2

extern drawChart from "dashboard.drawChart"

page
  graphic aria-label "Revenue bars" width "320" height "120" viewBox "0 0 320 120"
    bar x "20" y "40" width "70" height "60" fill "#0f766e"
    dot cx "250" cy "55" r "12" fill "#111827"
    line x1 "20" y1 "104" x2 "300" y2 "104" stroke "#475569" stroke-width "2"
    path d "M20 90 C90 20 180 120 300 40" fill "none" stroke "#9333ea" stroke-width "3"
  chart bar data revenue by month value total label "Revenue by month" color "#2563eb"
  scene3d "Product scene" scene productScene camera orbit controls orbit renderer "three" into sceneState width "640" height "360"
  text "Renderer status: {sceneState.status}"
  canvas id "sales-chart" aria-label "Sales chart" width "800" height "320"
  svg aria-label "Simple bars" width "320" height "120"
    rect x "20" y "40" width "70" height "60" fill "#0f766e"
  button "Render chart" click drawChart("sales-chart")
```

`graphic` lowers to accessible SVG with `role "img"`. `bar`, `dot`, `line`,
`path`, `polyline`, `polygon`, and `group` lower to standard SVG shape tags for
AI-friendly chart and diagram authoring. Raw SVG tags remain available when you
need exact SVG vocabulary.

`chart bar data rows by label value total` lowers to an accessible SVG bar
chart. `data` points at an array-like JTML value or a fetch result's `.data`;
`by` names the label field; `value` names the numeric field. The browser
runtime renders the bars whenever state or fetched data changes.

`scene3d` lowers to an accessible `<canvas>` mount with `data-jtml-scene3d`.
Core JTML intentionally does not bundle Three.js, WebGPU, or a physics engine.
Instead, hosts and packages can provide `window.jtml3d.render(canvas, spec)`.
When no host renderer is installed, the runtime draws a visible fallback so the
page remains understandable and testable. Add `into sceneState` to expose
`scene`, `camera`, `controls`, `renderer`, `status`, `hostRendered`, `width`,
and `height` as reactive JTML state. Host renderers can call
`spec.update({ selected: "part-1" })` to publish richer scene state.

This is the current production escape hatch for Canvas, WebGL, Web Audio,
Three.js, ffmpeg-backed processing, maps, charting SDKs, and specialized media
libraries. Image processing and richer chart primitives are planned in
`docs/media-graphics-roadmap.md`.

Friendly effects run a block when a variable changes:

```jtml
let count = 0
let message = "Idle"

effect count
  let message = "Count changed"
```

Effects lower to subscription machinery. Effect bodies should update existing
state or call existing actions.

Friendly external actions let a host page provide browser-side behavior without
putting JavaScript inside JTML:

```jtml
extern notify from "host.notify"

page
  button "Notify" click notify("Saved")
```

The compiler emits a `data-jtml-extern-action` marker. The browser runtime
intercepts `notify(...)` before server dispatch and calls
`window.host.notify(...)`. If `from` is omitted, JTML uses the action name as
the window path.

Friendly stores group shared state into a dictionary-like value:

```jtml
store auth
  let user: string = "Ada"
  let token = "abc"

  when logout
    let user = ""
    let token = ""

page
  show auth.user
  button "Logout" click auth.logout
```

Store fields lower to one shared dictionary, so `auth.user` and `auth.token`
are ordinary reactive reads. Actions declared inside the store compile to
collision-proof generated functions, and UI code can call them with the
store-qualified form such as `auth.logout`.

Friendly type annotations are optional and erased at compile time:

```jtml
let count: number = 0
const name: string = "Ada"
get label: string = "Hello {name}"

when save email: string age: number
  let count: number = count + 1

make Badge title: string
  text title
```

Types are optional and erased at compile time. The linter reports obvious
primitive mismatches with `JTML_TYPE_MISMATCH` and checks ordinary
action/function arity with `JTML_ARITY`, including browser-provided input event
values. Richer object field and collection checks remain planned.

Friendly component calls isolate local state and actions at source-expansion
time:

```jtml
make Counter label
  let count = 0
  when add
    count += 1
  box
    h2 label
    show count
    button "+" click add

page
  Counter "First"
  Counter "Second"
```

Each `Counter` call receives its own generated names for `count` and `add`.
This prevents accidental shared state between repeated components. Each call is
also wrapped in a `<div data-jtml-instance="Counter_N">` element so instance
boundaries are visible in browser DevTools and scriptable from the outside.
The wrapper also carries `data-jtml-component`, `data-jtml-instance-id`,
`data-jtml-component-params`, and `data-jtml-component-locals`. On load, the
browser runtime publishes these as `window.__jtml_components` and
`window.jtml.getComponentInstances()`, then emits `jtml:components-ready`.
The interpreter also executes each wrapper inside a runtime component
environment. Local values, actions, element bindings, event handlers, and dirty
recalculation resolve through the owning instance, so repeated components keep
independent state even when tests or tools dispatch events directly. Runtime
state is reported through `/api/state`, including local values and actions.
Tools can fetch the registry directly from `/api/components` and can call local
actions through `/api/component-action`, for example
`{"componentId":"Counter_2","action":"add","args":[]}`. This is the
compatibility bridge toward non-expanded runtime component AST execution.
The compiler also preserves each original `make Component` contract as a hidden
definition marker. Tools can read those definitions through `/api/state`
(`componentDefinitions`), `/api/component-definitions`, or the C ABI
`jtml_component_definitions`.

Relative imports are resolved relative to the importing file, parsed once per
compile graph, and cycles are reported as errors. Bare package imports such as
`use Button from "ui-kit"` resolve through the nearest
`jtml_modules/ui-kit/index.jtml` or `jtml_modules/ui-kit/package.jtml`.
`jtml add` installs local packages into `jtml_modules/`, records
`jtml.packages.json`, and writes `jtml.lock.json` with deterministic package
and file fingerprints so teams can review and commit reproducible installs.
Run `jtml install` in CI or on another machine to restore missing local package
directories from `jtml.packages.json` when sources are available and verify the
installed fingerprints against `jtml.lock.json`.
`jtml serve --watch` watches imported files as well as the entry file.

Friendly modules can mark their public surface with `export`:

```jtml
// components/card.jtml
jtml 2
export make Card title
  box class "card"
    h2 title
    slot

let privateDebugLabel = "not imported by name"
```

Named imports (`use Card from "./components/card.jtml"` and
`use { Card } from "./components/card.jtml"`) include only matching exported
top-level declarations when a module uses `export`. Side-effect imports
(`use "./setup.jtml"`) keep the compatibility behavior and evaluate the whole
file. Modules with no `export` declarations also keep compatibility behavior
for now, so existing apps continue to build while larger projects can opt into
explicit public APIs.

## Classic Compatibility

Classic syntax is the compatibility layer used by older files and by the
compiler artifact inspector. Every statement ends with `\\` unless it is an
element close marker:

```jtml
define name = "Ada"\\
show "Hello, " + name\\
```

## Elements

The preferred element syntax is `@tag`:

```jtml
@main class="profile" data-page="home"\\
    @h1\\
        show "Hello"\\
    #
#
```

The explicit form is also supported:

```jtml
element main class="profile"\\
    show "Hello"\\
#
```

`#` closes the current non-void element. Void HTML elements such as `input`, `img`, `br`, `hr`, `meta`, and `link` can be written as one-line elements:

```jtml
@input id="email" type="email" required\\
@img src="/logo.png" alt="Logo"\\
```

Custom and SVG-style names are accepted:

```jtml
@my-widget data-mode="compact"\\
    show "Custom element"\\
#

@svg:path data-vector="ok"\\
    show "SVG-friendly name"\\
#
```

## Attributes

Attribute names can include hyphens and colons. Keyword-shaped names such as `for` are allowed in element and attribute positions.

```jtml
@label for="email" aria-label="Email"\\
    show "Email"\\
#
```

Boolean attributes can omit `=`:

```jtml
@input disabled required\\
```

Event attributes call JTML functions without user JavaScript:

```jtml
function save()\\
    show "Saved"\\
\\

@button onClick=save()\\
    show "Save"\\
#
```

Supported browser event attributes include `onClick`, `onInput`, `onChange`, `onKeyUp`, `onMouseOver`, and `onScroll`.

## State

`define` creates mutable state:

```jtml
define count = 0\\
```

`const` creates immutable state:

```jtml
const limit = 10\\
```

Assignment updates an existing mutable variable:

```jtml
count = count + 1\\
count += 1\\
```

Compound assignment supports `+=`, `-=`, `*=`, `/=`, and `%=`. The parser lowers
these to ordinary assignments, so tooling and runtime behavior stay predictable.

## Derived Values

`derive` creates a reactive value from other values:

```jtml
define count = 0\\
derive doubled = count * 2\\
show "Doubled: " + doubled\\
```

Derived values update when their dependencies update.

Conditional expressions keep production UI copy and attributes compact:

```jtml
derive message = saved ? "Saved" : "Unsaved"\\
@button disabled=saved\\
    show saved ? "Done" : "Save"\\
#
```

## Collections

Arrays and dictionaries are first-class values:

```jtml
define names = ["Ada", "Grace", "Linus"]\\
define user = {"name": "Ada", "role": "developer"}\\

show names[0]\\
show user["name"]\\
```

## Control Flow

Blocks use statement terminators around their bodies:

```jtml
if (count > 0)\\
    show "Positive"\\
\\
else\\
    show "Zero"\\
\\
```

Loops:

```jtml
for (name in names)\\
    show name\\
\\

while (count < 10)\\
    count = count + 1\\
\\
```

## Functions

Functions can take parameters and return values:

```jtml
function greet(name)\\
    return "Hello, " + name\\
\\
```

`async function` dispatches calls without blocking the caller:

```jtml
async function refresh()\\
    show "Refreshing"\\
\\
```

## Objects

Objects group fields and methods. Inheritance uses `derives from`:

```jtml
object Person\\
    define name = "Ada"\\
\\

object Admin derives from Person\\
    define role = "admin"\\
\\
```

## Imports

Imports evaluate another JTML file in the current runtime:

```jtml
import "shared.jtml"\\
```

## Subscriptions

Subscriptions call a function when a variable changes:

```jtml
function changed(value)\\
    show "Changed: " + value\\
\\

subscribe changed to count\\
unsubscribe changed from count\\
```

## Reserved Keywords

```text
element show define derive unbind store for if const in break continue throw
else while try except then return function to subscribe from unsubscribe
object derives async import main true false
```

These words are reserved in statement and expression positions. They can still be used as element or attribute names when the parser is clearly reading HTML-shaped syntax.
