# JTML Element Dictionary

Status: **active reference** for Friendly JTML 2.
Scope: Categorization and simplification of HTML-style language elements within
the broader JTML v2 framework.

Core aliases such as `page`, `box`, `text`, `link`, `image`, `video`, `audio`,
`embed`, `list`, `item`, form controls, events, and `into` bindings are
implemented by the Friendly normalizer (`src/friendly.cpp`) and covered by the
test suite. Standards-based raw/custom element fallback also allows HTML,
canvas, SVG-compatible, and custom-element tags to pass through when JTML does
not have a dedicated alias yet. Planned aliases are explicitly marked as
roadmap items.

## Design Goal

JTML v2 aims to simplify the cognitive load of building interfaces. While standard HTML contains over 100 elements, many are redundant or rarely used in modern web applications. The JTML Element Dictionary curates a simplified, categorized set of core elements designed to cover 99% of app development needs, while mapping cleanly to underlying HTML and DOM structures.

Where standard HTML relies on attributes for behavior (e.g., `type="text"` vs `type="checkbox"`), JTML prefers semantic distinctions or unified bindings (`input` vs `checkbox`, using `into` for state).

## 1. Structural Elements

These elements define the macro-layout and semantic structure of an application or document.

| JTML Element | HTML Equivalent | Purpose | Example |
| --- | --- | --- | --- |
| `page` | `<main>` / `<body>` | The root container for a screen or view. Every view should start with a `page`. | `page class "app-root"` |
| `section` | `<section>` | A thematic grouping of content, typically with its own heading. | `section id "hero"` |
| `nav` | `<nav>` | A container for navigation links (sidebar, topbar). | `nav class "sidebar"` |
| `header` | `<header>` | Introductory content or navigational aids for a page/section. | `header` |
| `footer` | `<footer>` | Closing content for a page/section (copyright, links). | `footer` |
| `box` | `<div>` | A generic container for layout and grouping (replaces excessive divs). | `box class "flex-row"` |
| `article` | `<article>` | Independent, self-contained content (blog post, widget). | `article` |

## 2. Typography & Content

Elements for displaying text and data readable by the user.

| JTML Element | HTML Equivalent | Purpose | Example |
| --- | --- | --- | --- |
| `h1` - `h6` | `<h1>` - `<h6>` | Headings, defining the hierarchy of the content. | `h1 "Welcome Back"` |
| `text` | `<p>` | A standard block of text or paragraph. | `text "This is a paragraph."` |
| `span` | `<span>` | Generic inline container for phrasing content (styling parts of text). | `span class "highlight"` |
| `strong` | `<strong>` | Important text, typically rendered as bold. | `strong "Warning:"` |
| `em` | `<em>` | Emphasized text, typically rendered as italic. | `em "Note"` |
| `list` | `<ul>` / `<ol>` | A collection of items. (Use `ordered` boolean attribute for `<ol>`). | `list ordered` |
| `item` | `<li>` | An individual item within a `list`. | `item "First point"` |
| `code` | `<code>` | Inline or block code snippets. | `code "let x = 1"` |

*Note: In JTML v2, direct text insertion is often handled via the `show` keyword (e.g., `show "Hello"`) or as the first unnamed string argument to an element (e.g., `h1 "Title"`).*

## 3. Interactive Controls (Forms & Actions)

Elements that allow user input and trigger application state changes.

| JTML Element | HTML Equivalent | Purpose | Example |
| --- | --- | --- | --- |
| `button` | `<button>` | Triggers an action (e.g., submit, cancel, delete). Bind with `click`. | `button "Save" click saveAction` |
| `link` | `<a>` | Navigates to another page or section. Bind with `href`. | `link "Profile" href "/profile"` |
| `input` | `<input type="text">` | Single-line text input. Bind state with `into`. | `input "Email" into email` |
| `textarea` | `<textarea>` | Multi-line text input. Bind state with `into`. | `textarea "Notes" into notes` |
| `checkbox` | `<input type="checkbox">` | A boolean toggle (true/false). Bind state with `into`. | `checkbox "Accept Terms" into agreed` |
| `select` | `<select>` | A dropdown menu for choosing one or more options. | `select into chosenRole` |
| `option` | `<option>` | An individual choice within a `select`. | `option "Admin" value "admin"` |
| `form` | `<form>` | Wraps inputs and handles data submission. Bind with `submit`. | `form submit loginUser` |

## 4. Media & External Content

Elements for embedding non-textual assets.

| JTML Element | HTML Equivalent | Purpose | Example |
| --- | --- | --- | --- |
| `image` | `<img>` | Embeds an image. Requires `src` and ideally `alt`. | `image src "/logo.png" alt "Logo"` |
| `video` | `<video>` | Embeds video content. Prefer `controls`; use `into` for browser-side playback state/actions. | `video src "/demo.mp4" controls into player` |
| `audio` | `<audio>` | Embeds audio content. Prefer `controls`; use `into` for browser-side playback state/actions. | `audio src "/sound.mp3" controls into playback` |
| `embed` | `<iframe>` | Embeds external HTML contexts. Include `title` for accessibility. | `embed src "https://example.com" title "Preview"` |
| `file` | `<input type="file">` | Lets the user select a local file. Bind with `into` to receive file metadata and preview URL. | `file "Choose image" accept "image/*" into selected` |
| `dropzone` | `<input type="file" multiple data-jtml-dropzone>` | Multiple file input with drag/drop dispatch. Bind with `into` to receive an array of file objects. | `dropzone "Drop media" accept "image/*,video/*" into assets` |
| `canvas` | `<canvas>` | Raw drawing surface. Use `extern` or host code for drawing until first-class graphics land. | `canvas id "chart" width "800" height "320"` |
| `graphic` | `<svg role="img">` | AI-friendly accessible SVG root for charts and diagrams. | `graphic aria-label "Diagram"` |
| `bar` | `<rect>` | Rectangular SVG shape, useful for bars and blocks. | `bar x "10" y "20" width "40" height "80"` |
| `dot` | `<circle>` | Circular SVG shape, useful for points and markers. | `dot cx "80" cy "40" r "8"` |
| `line` | `<line>` | SVG line shape for axes, connectors, and diagrams. | `line x1 "0" y1 "0" x2 "100" y2 "40"` |
| `path` | `<path>` | SVG path shape for curves and custom marks. | `path d "M0 80 C40 10 80 120 120 40"` |
| `polyline`, `polygon` | `<polyline>`, `<polygon>` | SVG point-list shape aliases. | `polyline points "0,0 50,40 100,10"` |
| `group` | `<g>` | SVG grouping container. | `group class "series"` |
| `chart` | `<svg data-jtml-chart>` | Runtime-rendered accessible SVG chart from JTML state or fetch data. | `chart bar data rows by label value total` |
| `scene3d` | `<canvas data-jtml-scene3d>` | 3D renderer mount with fallback, host hook, and optional `into` state. | `scene3d "Product" scene model camera orbit into sceneState` |
| `svg` and SVG child tags | `<svg>` etc. | Standards-based vector graphics through raw element fallback. | `svg width "200" height "120"` |

Planned media/graphics aliases live in `docs/media-graphics-roadmap.md`:
richer chart, 3D scene graph, and media-processing primitives are still roadmap items.

## 5. JTML Native Directives

These are not standard HTML elements, but are treated structurally similarly in JTML to handle control flow and components.

| JTML Concept | Purpose | Example |
| --- | --- | --- |
| `make Component` | Defines a reusable custom element. | `make Card title` |
| `Component` | Invokes a custom element (starts with Uppercase). | `Card "Revenue"` |
| `slot` | Renders children passed to a component. | `slot` |
| `show` | Renders text, variables, or expressions. | `show "{count} items"` |
| `if` / `else` | Conditional rendering of elements/content. | `if loggedIn` ... `else` ... |
| `for ... in` | List rendering (mapping over arrays). | `for user in users` |

## Element Shorthands & Bindings

To keep JTML clean and friendly, elements support standardized shorthands for their most common attributes:

1. **Text Content**: The first string literal or expression after a tag is treated as its text content or `placeholder` (for inputs).
   - *JTML*: `button "Click Me"`
   - *HTML*: `<button>Click Me</button>`

   - *JTML*: `input "Username"`
   - *HTML*: `<input placeholder="Username" />`

2. **State Binding (`into`)**: Binds an input element's value to a variable, and auto-generates the `change`/`input` listener.
   - *JTML*: `input "Email" into emailState`
   - *Meaning*: the browser runtime keeps `emailState` synchronized with the input value.

3. **Event Binding**: Lowercase event names directly bind to variables or expressions.
   - *JTML*: `button "Submit" click submitForm`
   - *Meaning*: the compiler lowers this to the Classic `onClick` event binding used by the runtime.

4. **Boolean Attributes**: Standalone words without values are treated as `true`.
   - *JTML*: `input required disabled`
   - *Meaning*: `<input required disabled />`
