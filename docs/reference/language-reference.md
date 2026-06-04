# JTML Language Reference

Version: 0.1.0-dev

JTML is the web/app dialect of the planned JTL language family. JTML is a
compact syntax for building reactive HTML pages: it keeps HTML's element and
attribute model, then adds state, derived values, actions, events, components,
modules, async data, routing, stores, effects, tooling, and a small runtime
contract.

## Files

JTML source files use `.jtml`. Experimental core-language files can use `.jtl`
and the `jtl 1` header while the JTL core language is being designed.

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

Friendly syntax is indentation-based. Today it still executes through a
Classic-compatible backend for runtime stability, but the public language model
is Friendly source plus semantic IR and the observable graph. The current
production authoring surface includes
`use`, `export`, `let`, `const`, `get`, `when`, `make`, `slot`, `page`,
`show`, element aliases, `style`, `fetch`, `route`, `layout`, `load`, `guard`,
`redirect`, `store`, `effect`, `extern`, `if`/`else`, `for`, `while`,
`try`/`catch`/`finally`, `return`, `throw`, `break`, `continue`, common DOM
event shorthands, and `input ... into name` bindings. Expressions support
interpolated strings (`"Hello {name}"`), ternaries (`condition ? a : b`),
arrays, dictionaries, dotted reads, index reads, function calls, and compound
assignment.

For core-language experiments without web UI, start with `jtl 1`:

```jtl
jtl 1

let total = 0
get doubled = total * 2

when add amount
  total += amount
```

Current status: `jtl 1` lowers through the same Friendly pipeline as `jtml 2`.
It is the first compatibility slice for the future core language, not yet a
separate general-purpose runtime.

## Friendly Keyword Index

Use this as the compact, copyable mini-reference for the public language
surface. Friendly JTML 2 is the source dialect; compatibility keywords are
listed separately because they are backend/migration vocabulary, not the
recommended spelling for new apps.

| Area | Keywords and forms |
| --- | --- |
| file/version | `jtml 2`, experimental `jtl 1` |
| state and values | `let`, `const`, `get`, `show`, `true`, `false` |
| actions and effects | `when`, `effect`, `redirect`, `refresh`, `invalidate`, `extern` |
| control flow | `if`, `else`, `for`, `in`, `while`, `break`, `continue`, `try`, `catch`, `finally`, `return`, `throw` |
| components and modules | `make`, `slot`, `export`, `use`, component calls such as `Card "Title"` |
| async data | `fetch`, `method`, `body`, `cache`, `credentials`, `timeout`, `retry`, `stale`, `keep`, `lazy` |
| routes | `route`, `layout`, `load`, `guard`, `require`, `activeRoute`, `activeRouteName` |
| forms/events | `into`, `click`, `input`, `change`, `submit`, `hover`, `scroll`, `focus`, `blur`, `keyup`, `keydown`, `key-up`, `key-down`, `dragover`, `drop`, `dblclick`, `double-click` |
| common elements | `page`, `link`, `navlink`, `text`, `box`, `checkbox`, `list`, `ordered`, `item` |
| media | `image`, `video`, `audio`, `embed`, `file`, `dropzone` |
| graphics | `canvas`, `svg`, `graphic`, `group`, `bar`, `dot`, `line`, `path`, `polyline`, `polygon`, `chart`, `scene3d` |
| semantic UI | `theme`, `app`, `shell`, `topbar`, `sidebar`, `content`, `panel`, `card`, `grid`, `stack`, `cluster`, `split`, `toolbar`, `tabs`, `tab`, `alert`, `badge`, `modal`, `drawer`, `toast`, `loading`, `error`, `empty`, `field`, `metric`, `spacer` |
| UI modifiers | `cols`, `gap`, `pad`, `radius`, `shadow`, `tone`, `align`, `justify`, `width`, `surface` |
| style/interop escape hatches | `style`, `css raw`, `html raw` |
| media helpers | `timeline`, `animate`, `resize`, `crop`, `filter`, `axis`, `legend`, `stacked`, `duration`, `easing`, `autoplay`, `repeat` |

Compatibility/backend keywords remain supported for older files and lowered
artifacts: `element`, `@tag`, `define`, `derive`, `unbind`, `function`,
`async`, `subscribe`, `unsubscribe`, `object`, `derives`, `import`, `main`,
`then`, and `except`.

Classic JTML remains supported as a compatibility backend for older files,
generated compiler artifacts, migration, and embedding. Use `--syntax classic`
to force compatibility parsing, or `jtml migrate old.jtml -o new.jtml` to
convert most Classic files to Friendly.

## Friendly JTML 2 vs Compatibility Backend

Use **Friendly JTML 2** for new source files, tutorials, examples, AI-generated
code, Studio editing, and application code. It is indentation-based, shorter,
and designed to be read by humans and generated reliably by AI.

Use the **compatibility backend** when maintaining older files, inspecting
lowered artifacts, testing migration, embedding JTML through the C ABI, or
debugging the compiler's lowered representation. It is stable; it is not the
recommended spelling for new apps.

| Need | Choose | Why |
| --- | --- | --- |
| New app/page/component | Friendly `jtml 2` | Canonical public dialect |
| Core-language experiment | Experimental `jtl 1` | Shared Friendly pipeline today; planned JTL core later |
| AI-authored code | Friendly `jtml 2` | Fewer tokens, fewer delimiters, clearer intent |
| Existing old `.jtml` file | Compatibility backend or `jtml migrate` | Preserve compatibility, migrate when ready |
| Compiler/runtime artifact inspection | Compatibility IR | This is the lowered backend form |
| Embedding/runtime integration | Either input, compatibility artifact for debugging | Friendly remains canonical; backend artifacts help host debugging |

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

`fetch` creates a reactive browser-side value shaped as:

```jtml
{
  loading,
  data,
  error,
  stale,
  attempts,
  hasData,
  status,
  ok,
  url,
  method,
  updatedAt
}
```

The stable common fields are `loading`, `data`, `error`, `stale`, `attempts`,
and `hasData`; browser-local builds also expose request metadata for
diagnostics, retry UI, and future devtools. Fetch supports simple GET requests,
JSON request bodies for non-GET calls, browser cache/credentials policies,
timeout, retry, stale data policy, and a `refresh` action that re-triggers the
fetch client-side without a server round-trip:

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
refresh/loading and after an error; `stale clear` is the default. The runtime
sets `hasData` after a successful response and sets `stale` only when preserved
data exists, which keeps first-load UI honest. Use `invalidate posts` inside a
`when` action to refresh one or more named fetches after the action has
dispatched. That is the mutation-friendly form: the action runs first, then the
browser revalidates the listed fetch bindings.

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

For custom browser tooling, `window.jtml.fetches` exposes the registered fetch
functions by name and `window.jtml.refreshFetch("users")` refreshes a specific
fetch node. This is an escape hatch for demos, diagnostics, and devtools; app
code should prefer `refresh actionName` and `invalidate fetchName`.

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
normalized hash path (e.g. `"/"`, `"/dashboard"`). `activeRouteName` holds the
matched component name when a route matches. Any element with a
`data-jtml-expr` binding that reads either value updates automatically.

For custom browser tooling, the runtime publishes a route registry:

```js
window.jtml.routes
window.jtml.routeManifest
window.jtml.getRoutes()
window.jtml.getCurrentRoute()
window.jtml.navigate("/dashboard")
```

`routeManifest` comes from the compiler-emitted browser manifest. `routes` is
the live runtime table collected after the DOM is ready. The browser also
dispatches `jtml:routes-ready` after the route table is collected and
`jtml:route-change` whenever routing is applied. These are devtools and Studio
hooks; app code should usually use `link ... to` and `redirect`.

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

## Semantic Styling And UI Primitives

JTML supports scoped CSS through `style`, and first-slice semantic styling
through `theme` plus built-in UI primitives. These primitives lower to normal
HTML, generated classes, and CSS variables under `[data-jtml-app]`.

```jtml
jtml 2

theme
  color primary "#155e75"
  color surface "#ffffff"
  color background "#f6f7fb"
  space md 14
  radius md 12

page
  shell
    sidebar
      navlink "Dashboard" to "/" active-class "active"
    content
      panel title "Usage" pad lg shadow md
        grid cols 2 gap md
          metric "Users" users.total "Active" tone good
          card tone primary
            h2 "Ready"
```

Implemented primitives in the first slice:

```text
app shell topbar sidebar content panel card grid stack cluster split toolbar
tabs tab alert badge modal drawer toast loading error empty field metric spacer
navlink
```

The canonical machine-readable catalog is available from:

```sh
jtml ui --json
```

It reports each primitive's category, lowered HTML role, common modifiers, and
description, plus supported modifier values and theme token kinds. Treat this
CLI output as the source of truth for Studio/reference surfaces, editor
plugins, and AI authoring prompts.

Implemented modifiers:

```text
cols gap pad radius shadow tone align justify width surface
```

The most stable modifiers today are `cols`, `gap`, `pad`, `radius`, `shadow`,
and `tone`. `metric "Label" value "Caption" tone good` is a compact primitive
for dashboard-style values. `jtml explain --json` reports the visual surface in
`semantic.ui` with primitive names, primitive-use facts, modifiers, theme token
counts, style block counts, and escape-hatch counts. Use
`semantic.ui.authorThemeTokens` for the tokens written in the source `theme`
block; `semantic.ui.themeTokens` includes generated CSS token references for
compatibility with earlier tooling. The compatibility key
`semantic.uiModifiers` is also preserved. `semantic.ui.uses` reports whether a
primitive has title text, `aria-label`, a usable label, form controls, action
bindings, route targets, tab children, or dismiss actions. `jtml lint` warns on obvious
mismatches such as `cols` outside `grid`, `tone` on structural layout
primitives, unsupported modifier values such as `gap huge`, unlabeled semantic
surfaces/overlays such as `panel` or `modal` without `title` or `aria-label`,
modal/drawer overlays without a close-like action, `field` without a control or
usable label, `tabs` without a `tab`, and `tab` without an action or route
target. Raw scoped CSS remains available when the semantic surface is not
expressive enough:

Several primitives also lower with accessibility defaults: `modal` and
`drawer` emit `role="dialog"`, `aria-modal="true"`, and `tabindex="-1"`;
`alert` and `error` emit `role="alert"`; `toast`, `loading`, and `empty` emit
`role="status"` plus `aria-live="polite"`; `loading` also emits
`aria-busy="true"`; `tabs` emits `role="tablist"`; and `tab` emits
`role="tab"` plus `type="button"`.

```jtml
style
  .custom-widget
    display: grid
    gap: 12px
```

`style` selectors are scoped under `[data-jtml-app]`. When a host integration
needs unscoped CSS, use the explicit escape hatch:

```jtml
css raw
  .third-party-widget { display: grid; }
  .third-party-widget iframe { border: 0; }
```

Prefer `theme`, semantic primitives, and scoped `style` before `css raw`.
Unscoped CSS is for third-party widgets, custom elements, and host-owned
surfaces where JTML should not rewrite selectors.
`jtml lint` reports `JTML_RAW_CSS_ESCAPE_HATCH` so production reviews can see
that unscoped CSS is present.

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

Reactive media attributes are evaluated in browser-local builds as well as
live previews. For example, `image src selected.preview alt selected.name`
keeps the real `src` and `alt` attributes synchronized with JTML state after a
file input changes.

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
[`../roadmaps/media-graphics-roadmap.md`](../roadmaps/media-graphics-roadmap.md).

First-slice browser image processing is available through derived image
helpers:

```jtml
let thumb = image photo resize 256 256 fit cover
let cropped = image photo crop 20 20 160 120
let gray = image photo filter grayscale amount 1
```

Each helper produces an object shaped like
`{ preview, loading, error, width, height }`, so the result can drive ordinary
JTML attributes:

```jtml
image src thumb.preview alt "Thumbnail"
```

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
the window path. `jtml explain --json` reports these as
`semantic.nodes.externs` with `extern:<name> --calls--> <window.path>` graph
edges, and `jtml lint` emits `JTML_EXTERN_ESCAPE_HATCH` so production reviews
can check host-provided behavior explicitly.

For custom elements or host widgets that need literal markup, use `html raw`.
This intentionally bypasses JTML escaping, so only use it for trusted markup:

```jtml
css raw
  third-party-widget { display: block; min-height: 120px; }

page
  h1 "Interop"
  html raw "<third-party-widget data-mode=\"demo\"></third-party-widget>"
```

`html raw` may be written as a quoted one-line payload or as an indented raw
block. It lowers to a compatibility raw element and is emitted directly by the
HTML backend. `jtml lint` reports `JTML_RAW_HTML_ESCAPE_HATCH` because raw
HTML bypasses JTML escaping and should be reviewed before production use.

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

## Compatibility Backend

Classic syntax is the backend compatibility layer used by older files and by
the compiler artifact inspector. Every statement ends with `\\` unless it is an
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

The Friendly keyword index near the top of this file is the canonical public
reserved-word reference. Compatibility/backend keywords are reserved in
statement and expression positions for Classic files and lowered artifacts.
Keyword-shaped names can still appear as element or attribute names when the
parser is clearly reading HTML-shaped syntax, for example
`label for "email"`.
