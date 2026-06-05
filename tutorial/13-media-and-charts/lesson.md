# 13. Media, Charts, and Graphics

JTML handles media and data visualisation declaratively — the same way it handles state.

<!-- studio:playground -->

## File and drop inputs

The `file` alias creates a file input. The selected file becomes JTML state via `into`:

```jtml
let photo = ""
page
  file "Choose photo" accept "image/*" into photo
  if photo
    image src photo.preview alt photo.name
```

The bound variable is an object with `name`, `type`, `size`, `preview` (data URL), and `url`.

`dropzone` wraps the same binding in a drag-and-drop zone:

```jtml
let assets = []
page
  dropzone "Drop files here" accept "image/*,video/*" into assets
  p "{assets.length} files"
```

## Image processing roadmap

Image processing is planned for the browser runtime. The target shape is:

```jtml
let photo = ""
let thumb = image photo resize 256 256 fit cover

page
  file "Pick image" accept "image/*" into photo
  if thumb.preview
    image src thumb.preview alt "Thumbnail"
```

Planned operations include `crop x y w h` and filters such as `grayscale`, `blur`, `brightness`, `contrast`, `sepia`, `invert`, and `saturate`.

## Charts

`chart bar` and `chart line` render accessible SVG from any array of objects:

```jtml
let data = [{"month":"Jan","sales":12,"expenses":7}, {"month":"Feb","sales":18,"expenses":9}]

page
  chart bar data data by month value sales label "Sales" color "#0f766e"
  chart line data data by month values sales expenses series "Sales,Expenses" colors "#0f766e,#b42318" legend grid max 20 ticks 5 annotate "Launch" at "Feb" value sales export svg csv
```

Current chart options:

| Option | Effect |
|--------|--------|
| `color "#hex"` | Bar or line colour |
| `values sales expenses` | Render multiple numeric fields |
| `series "Sales,Expenses"` | Label each series in the legend |
| `colors "#0f766e,#b42318"` | Colour each series |
| `label "..."` | Accessible chart label |
| `axis x/y label "..."` | Axis labels |
| `grid` | Grid lines |
| `legend` | Series legend |
| `stacked` | Stack multi-series bar segments |
| `min 0`, `max 100`, `ticks 5` | Stable chart scale and tick count |
| `annotate "Launch" at "Feb" value sales` | Mark a row/series point |
| `export svg png csv` | Add browser-local chart export buttons |

Use `graphic`, `path`, `line`, `dot`, and `svgtext` when you need custom vector
visuals or annotations that go beyond the native chart helper.

## Media controls

Use `video` and `audio` when you want standards-based browser controls:

```jtml
page
  video src "/demo.mp4" controls poster "/poster.jpg"
  audio src "/intro.mp3" controls
```

Reactive media controllers expose browser-side state and actions:

```jtml
page
  video src "/demo.mp4" controls into player
  button "Play" click player.play
  button "Pause" click player.pause
```

Use native controls for normal playback and controller actions when the UI
needs its own buttons, scrubbers, or telemetry.

## Timeline animations roadmap

Timeline animation syntax is planned. The target shape is:

```jtml
let progress = 0
let opacity  = 0

timeline intro duration 800 easing ease-out
  animate progress from 0 to 100
  animate opacity  from 0 to 1

page
  button "Play" click intro.play
  button "Pause" click intro.pause
  button "Reset" click intro.reset
  p "Progress: {progress}%"
```

Until that lands, model simple interactions with `when` actions and state updates, as shown in the lesson code.

## What to try

Edit the code on the right to:

1. Change the chart colour.
2. Adjust the `path` in the vector graphic.
3. Change the progress step from 25 to 10.
4. Add a second progress bar with a different state variable.
