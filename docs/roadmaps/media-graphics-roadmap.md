# JTML Media And Graphics Roadmap

Status: first slice in progress.

JTML already supports ordinary web media through Friendly element aliases:
`image`, `video`, `audio`, `embed`, `file`, `dropzone`, media controllers
through `video/audio ... into name`, SVG-first `graphic`/`bar`/`dot`
aliases, accessible `chart`, and `scene3d` canvas mounts, plus raw
canvas/SVG-compatible tags and `extern` browser bridges. That is enough for
documents and many product pages, but it is not enough for modern interactive interfaces,
creative tools, learning apps, dashboards, game-like experiences, AI-generated
editors, or media-heavy products.

The goal is to make media and graphics feel like JTML: declarative first,
reactive by default, readable by non-specialists, and easy for AI systems to
generate correctly.

## What Works Today

Use standard media elements and attributes:

```jtml
jtml 2

page
  image src "/assets/photo.jpg" alt "Team photo"
  video src "/assets/demo.mp4" controls
  audio src "/assets/intro.mp3" controls
  embed src "https://example.com/widget" title "Widget"
  graphic aria-label "Revenue bars" width "320" height "120" viewBox "0 0 320 120"
    bar x "20" y "40" width "70" height "60" fill "#0f766e"
    dot cx "250" cy "55" r "12" fill "#111827"
    line x1 "20" y1 "104" x2 "300" y2 "104" stroke "#475569" stroke-width "2"
    path d "M20 90 C90 20 180 120 300 40" fill "none" stroke "#9333ea" stroke-width "3"
  chart bar data revenue by month value total label "Revenue by month" color "#2563eb"
  scene3d "Product scene" scene productScene camera orbit controls orbit renderer "three" into sceneState width "640" height "360"
```

Use scoped styles for presentation:

```jtml
style
  image
    max-width: 100%
    border-radius: 12px
```

Use `extern` when a host page needs to provide custom browser behavior, such as
drawing on a canvas, opening a native picker, running a Web Audio graph, or
calling a specialized media library:

```jtml
jtml 2

extern drawWaveform from "studio.media.drawWaveform"

page
  canvas id "waveform" width "800" height "180"
  button "Draw waveform" click drawWaveform("waveform")
```

This keeps JTML source clean while allowing production hosts to bridge advanced
browser APIs today.

Use first-slice file bindings for local assets:

```jtml
jtml 2

let selectedImage = ""
let assets = []

page
  file "Choose image" accept "image/*" into selectedImage
  dropzone "Drop media files" accept "image/*,video/*,audio/*" into assets
  if selectedImage
    image src selectedImage.preview alt selectedImage.name
```

The browser runtime maps selected files to `{ name, type, size, lastModified,
preview, url }` objects. `dropzone` also handles drag/drop on the generated
file input and dispatches the same `change` binding as manual selection.

Use first-slice media controllers for playback state:

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
`muted`, `volume`, `playbackRate`, `readyState`, and `src`. The first action
bridge handles `play`, `pause`, `toggle`, `seek(seconds)`, and
`setVolume(0..1)`.

## Why This Matters

Media and graphics unlock categories that plain HTML templates cannot win:

- product explainers with real screenshots, video, and annotated callouts
- data dashboards with charts, maps, diagrams, and live visual state
- creative tools for cropping, filtering, annotating, timeline editing, and
  asset review
- educational apps with interactive diagrams, audio playback, and simulations
- AI-native app generation where an agent can produce a complete interface
  without inventing JavaScript drawing code

## Design Principles

1. **Start with HTML parity.** Existing `image`, `video`, `audio`, `canvas`,
   and SVG/custom element output must stay standards-based.
2. **Keep media state reactive.** Playback, selected files, dimensions,
   upload progress, and processing results should be ordinary JTML values.
3. **Make advanced work explicit.** Camera, microphone, local files, and heavy
   processing need clear syntax and user-visible permission boundaries.
4. **Prefer declarative graphics.** Charts, shapes, diagrams, and timelines
   should be authorable as JTML structures instead of raw JavaScript.
5. **Leave escape hatches.** `extern`, custom elements, and exported wrappers
   remain the path for WebGL, WebGPU, Three.js, Web Audio, ffmpeg.wasm, and
   professional media SDKs.

## Target Surface

These are proposed forms, not all implemented yet.

### Media Elements

```jtml
image src avatar.url alt avatar.name fit cover
video src lesson.video controls poster lesson.poster into player
audio src clip.url controls into playback
```

The `into` binding would expose a reactive media controller:

```jtml
show "Time: {player.currentTime} / {player.duration}"
show "Paused: {player.paused}"
button "Play" click player.play
button "Pause" click player.pause
```

### File And Drop Inputs

```jtml
file "Choose image" accept "image/*" into selectedImage
dropzone accept "image/*,video/*" multiple into assets

page
  if selectedImage
    image src selectedImage.preview alt selectedImage.name
```

Target state shape:

```txt
{
  name,
  type,
  size,
  width?,
  height?,
  duration?,
  preview,
  data?,
  error?
}
```

### Image Processing

```jtml
let thumb = image selectedImage resize 512 512 fit cover
let cropped = image selectedImage crop x y width height
let filtered = image selectedImage filter grayscale amount 0.8

page
  image src thumb.preview alt "Thumbnail"
```

First implementation should generate browser-side processing for small images
with `<canvas>`, then expose CLI/server pipelines for production builds.

### Audio Processing

```jtml
let waveform = audio clip analyze waveform
let transcript = audio clip transcribe endpoint "/api/transcribe"

page
  waveform data waveform.samples height 120
  text transcript.text
```

JTML should not invent a full DSP language. It should provide reactive handles
for common app tasks and let `extern` or backend APIs handle specialized work.

### Video Processing

```jtml
let preview = video movie thumbnail at 3.5
let trimmed = video movie trim start 10 end 35

page
  image src preview.preview alt "Video thumbnail"
```

Browser-first support should focus on metadata, thumbnails, poster generation,
playback state, and upload progress. Heavy transcoding belongs in a server or
explicit toolchain.

### Declarative Graphics

```jtml
graphic width 640 height 360 viewbox "0 0 640 360"
  rect x 0 y 0 width 640 height 360 fill "#101820"
  circle cx progress cy 180 r 24 fill "#0f766e"
  text x 24 y 40 fill "white" "Progress: {progress}%"
```

Initial target: lower to SVG for deterministic rendering, accessibility, and
static export. Later targets can include Canvas for dense scenes and WebGL for
3D.

### Charts

```jtml
chart bar data revenue by month value total
  axis x label "Month"
  axis y label "Revenue"
  color "#0f766e"
```

The first implemented chart slice is `chart bar data rows by label value total`
with optional `label`, `width`, `height`, `viewBox`, and `color`. It compiles
to accessible SVG metadata and the browser runtime renders bars from local
state, store state, or fetch results. Later chart syntax should add explicit
axis, legend, scale, stacked/grouped bars, line charts, and static export
helpers. The linter should keep requiring labels and sensible fallbacks.

### Animation And Timelines

```jtml
let progress = 0

timeline intro duration 1200 easing ease-out
  animate progress from 0 to 100

page
  button "Replay" click intro.play
  text "Progress: {progress}%"
```

Animations should update JTML state, not hide state in an opaque script.

### 3D And Immersive Interfaces

```jtml
scene3d "Product configurator" scene productScene camera orbit controls orbit renderer "three" into sceneState width "960" height "540"
text "Renderer status: {sceneState.status}"
```

The first implemented 3D slice is intentionally a renderer mount, not a bundled
engine. The compiler emits an accessible canvas with `data-jtml-scene3d`; the
runtime calls `window.jtml3d.render(canvas, spec)` when a host renderer exists
and otherwise draws a visible fallback. This keeps the JTML binary portable and
lets teams choose Three.js, Babylon, WebGPU, custom WebGL, or native shell
renderers per product. With `into sceneState`, the runtime publishes renderer
metadata and fallback/host status into JTML state, and host renderers can call
`spec.update(...)` to publish selection, camera, loading, or hover state.

## CLI And Tooling Targets

| Command | Purpose |
| --- | --- |
| `jtml media info <file>` | Print metadata for images/audio/video |
| `jtml media optimize <in> -o <out>` | Lossless/safe production optimization |
| `jtml media thumbnail <video> -o image.jpg` | Generate thumbnails |
| `jtml graphics export <file.jtml> -o out.svg` | Export declarative graphics |
| `jtml lint` media rules | Missing `alt`, unlabeled charts, huge inline assets |

The CLI should avoid bundling heavyweight codecs by default. Use optional
helpers or external tools when a feature requires them.

## Studio Targets

Studio should grow a media/graphics workbench:

- asset browser for `assets/`
- preview panes for images, audio, video, SVG, and generated thumbnails
- drag-and-drop file input demos
- chart and graphic examples
- live inspector for media state (`currentTime`, `duration`, selected file
  metadata, upload progress)
- export buttons for optimized assets and SVG graphics

## Priority

| Feature | Effort | Impact | Priority |
| --- | --- | --- | --- |
| Media docs + examples | Small | High | Shipped first slice |
| Better media element aliases and attributes | Small | High | Shipped first slice |
| File/dropzone bindings | Medium | High | Shipped first slice |
| Reactive media controller with `into` | Medium | High | Shipped first slice |
| SVG-first `graphic` primitives | First shape slice shipped | High | Shipped first slice |
| Accessible chart primitives | First bar-chart slice shipped | High | P1 |
| 3D scene mount contract | First `scene3d` slice shipped | High | Shipped first slice |
| 3D state binding | `scene3d ... into state` shipped | High | Shipped first slice |
| Browser image metadata + thumbnails | Medium | Medium | P2 |
| Image resize/crop/filter helpers | Large | Medium | P2 |
| Audio waveform/metadata helpers | Large | Medium | P2 |
| Video thumbnail/metadata helpers | Large | Medium | P2 |
| WebGL/3D helpers | Very large | Differentiator | P3 |

## Implementation Order

1. ✅ Document current media support and add canonical examples.
2. ✅ Extend the Friendly element dictionary with `canvas`, `svg`, and
   media-safe attributes while preserving raw HTML fallback.
3. ✅ Add `file` and `dropzone` aliases with `into` bindings and typed selected
   file state.
4. ✅ Add media controller bindings for `audio/video ... into player`.
5. ✅ Add SVG-first `graphic` lowering for accessible SVG plus friendly
   `bar`, `dot`, `line`, `path`, `polyline`, `polygon`, and `group` aliases.
6. ✅ Add accessible `chart bar data rows by label value total` backed by the
   browser SVG renderer.
7. Add SVG text, axes, legends, and richer chart primitives backed by the same
   SVG renderer.
8. ✅ Add `scene3d` canvas mounts with host renderer contract and fallback.
9. ✅ Add `scene3d ... into sceneState`, host `spec.update(...)`, and first
   production lints for dimensions and renderer names.
10. Add Studio media/graphics examples and inspectors.
11. Add optional CLI media commands behind explicit dependency checks.

## Non-Goals For The First Pass

- no hidden dependency on Node/npm
- no mandatory ffmpeg bundle in the core CLI
- no auto-upload of user media
- no opaque generated JavaScript for ordinary charts or diagrams
- no mandatory 3D engine in the core binary
- no camera/microphone access without explicit user-authored syntax and browser
  permission flow
