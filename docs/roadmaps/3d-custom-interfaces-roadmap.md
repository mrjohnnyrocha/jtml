# JTML 3D And Custom Interfaces Roadmap

JTML should support highly customized interfaces without becoming a heavy
framework clone or bundling a mandatory graphics engine. The core language
should provide stable declarative mount points, accessibility, state handoff,
tooling, and fallbacks. Specialized rendering should live in host code,
packages, or optional adapters.

## Current Surface

```jtml
jtml 2

page
  scene3d "Product configurator" scene productScene camera orbit controls orbit renderer "three" into sceneState width "960" height "540"
  text "Renderer status: {sceneState.status}"
```

`scene3d` lowers to:

- `<canvas data-jtml-scene3d>`
- an accessible `aria-label`
- renderer metadata such as `data-jtml-scene`, `data-jtml-camera`,
  `data-jtml-controls`, and `data-jtml-renderer`
- optional controller metadata through `data-jtml-scene3d-controller`
- a browser runtime hook: `window.jtml3d.render(canvas, spec)`
- reactive state for `into sceneState`: `scene`, `camera`, `controls`,
  `renderer`, `status`, `hostRendered`, `width`, and `height`
- a visible canvas fallback when no host renderer is installed

The `spec` object contains `scene`, `camera`, `controls`, `renderer`,
`controller`, `state`, and an `update(nextState)` callback. Hosts can map
`scene` to Three.js, Babylon, WebGPU, custom WebGL, a native shell renderer, or
a test double, then publish hover, selection, loading, camera, and renderer
status back into JTML with `spec.update(...)`.

## Design Position

1. **Core stays portable.** The standalone `jtml` binary should not require
   Node, npm, Three.js, WebGPU, or native graphics SDKs.
2. **Declarative first.** JTML authors should describe the mount and data flow;
   renderer details stay behind a stable host/package contract.
3. **Fallback is mandatory.** A JTML page should remain understandable in
   Studio, static exports, CI screenshots, and browsers without a 3D engine.
4. **State must stay visible.** Renderer adapters should consume JTML state and
   publish meaningful state back later, not hide application logic in scripts.
5. **Advanced engines are packages.** `jtml add jtml-three`, `jtml add
   jtml-babylon`, or a custom host shell should provide heavy capabilities.

## Next Slices

| Slice | Purpose |
| --- | --- |
| `scene3d ... into sceneState` | Shipped first controller slice for renderer status and host updates |
| Asset manifests | Model/texture/audio asset references with deterministic build output |
| Renderer packages | Optional adapters for Three.js, Babylon, WebGPU, and native shells |
| Scene children | AI-friendly `model`, `light`, `camera`, `material`, `orbit-controls` syntax |
| Studio inspector | Scene metadata, renderer status, fallback status, and state preview |
| Linter rules | First slice warns on missing dimensions and unknown renderers; huge inline assets still planned |

This keeps JTML broadly usable while making it credible for product
configurators, education, simulations, dashboards, game-like tools, CAD-lite
interfaces, and immersive AI-generated apps.
