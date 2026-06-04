# JTML Demos

This folder contains larger JTML applications that are meant to feel closer to production surfaces than the focused examples in `examples/`.

## Enterprise Operations Suite

`index.jtml` is a single-file Friendly JTML app that demonstrates:

- routes, route guards, route parameters, and layouts
- stores, effects, derived values, typed declarations, and component-local state
- GET fetch, POST fetch, lazy route loading, refresh handles, and invalidation
- forms, inputs, textarea, select, checkbox, buttons, and links
- media primitives: image, audio, video, file, dropzone, canvas, SVG graphics, chart, and 3D host surfaces
- external host actions through `extern`

Run it locally:

```bash
./build/jtml serve demos/index.jtml --port 8030
```

Build it as a static artifact:

```bash
./build/jtml build demos --out dist/demos
```

This demo intentionally describes a full-stack-ready shape using the current JTML runtime HTTP contracts. Future server blocks, database bindings, SSR, and native packaging can attach to the same app structure.

## OpsPulse Reference Surface

`example.jtml` is a second, denser enterprise workspace used to stress the language and tooling. It covers a dashboard shell, nested component composition, route-backed pages, fetch resources, operational forms, media upload state, charts, settings flows, and observable-first lint checks.

It is intentionally kept as a source-quality demo rather than the default built artifact. Validate it with:

```bash
./build/jtml check demos/example.jtml
./build/jtml lint demos/example.jtml
```
