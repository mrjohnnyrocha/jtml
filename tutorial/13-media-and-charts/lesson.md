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

`chart bar` renders accessible SVG from any array of objects:

```jtml
let data = [{"month":"Jan","sales":12}, {"month":"Feb","sales":18}]

page
  chart bar data data by month value sales label "Sales" color "#0f766e"
```

Current chart options:

| Option | Effect |
|--------|--------|
| `color "#hex"` | Bar or line colour |
| `label "..."` | Accessible chart label |

Line charts, axes, grid, legends, stacked bars, and explicit dimensions are roadmap items. Use `graphic`, `path`, `line`, and `dot` when you need custom vector visuals today.

## Media controls

Use `video` and `audio` when you want standards-based browser controls:

```jtml
page
  video src "/demo.mp4" controls poster "/poster.jpg"
  audio src "/intro.mp3" controls
```

Reactive media controllers such as `player.play`, `player.pause`, and
`player.currentTime` are on the runtime hardening roadmap. Keep production
examples on native controls or host extern actions until that slice lands.

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
