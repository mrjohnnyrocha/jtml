# 13. Media, Charts, and Animation

JTML handles media, data visualisation, and animation declaratively — the same way it handles state.

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

## Image processing

`let name = image src resize W H [fit cover|contain|fill]` processes an image in the browser using canvas and exposes the result as `name.preview`:

```jtml
let photo = ""
let thumb = image photo resize 256 256 fit cover

page
  file "Pick image" accept "image/*" into photo
  if thumb.preview
    image src thumb.preview alt "Thumbnail"
```

Also available: `crop x y w h` and `filter grayscale|blur|brightness|contrast|sepia|invert|saturate amount N`.

## Charts

`chart bar` and `chart line` render accessible SVG from any array of objects:

```jtml
let data = [{"month":"Jan","sales":12}, {"month":"Feb","sales":18}]

page
  chart bar data data by month value sales label "Sales" color "#0f766e"
    axis x label "Month"
    axis y label "Sales ($k)"
    grid
    legend
```

Chart options:

| Option | Effect |
|--------|--------|
| `axis x label "..."` | X-axis label below the chart |
| `axis y label "..."` | Y-axis label rotated left |
| `grid` | Horizontal grid lines at tick marks |
| `legend` | Coloured legend key |
| `stacked` | Stack bars (bar charts only) |
| `color "#hex"` | Bar or line colour |
| `width N height N` | SVG dimensions in px |

## Media controllers

`into` on `video` or `audio` exposes reactive playback state:

```jtml
page
  video src "/demo.mp4" controls into player
  p "Time: {player.currentTime} / {player.duration}"
  button "Play"  click player.play
  button "Pause" click player.pause
  button "Restart" click player.seek(0)
```

## Timeline animations

`timeline name duration N [easing E] [autoplay] [repeat]` creates a reactive animation controller. Child `animate var from A to B` statements drive state on each frame:

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

Easing options: `linear`, `ease-in`, `ease-out`, `ease-in-out`.
Add `repeat` to loop. Add `autoplay` to start on page load.

## What to try

Edit the code on the right to:

1. Change the chart type from `bar` to `line`.
2. Add `axis x label "Month"` to the chart.
3. Increase the timeline duration to 2000 and add `repeat`.
4. Add a second `animate` line for a different variable.
