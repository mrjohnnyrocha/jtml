# JTML Documentation

This folder is the shareable documentation set for JTML. It is organized as a
stable flat folder so links stay simple for GitHub, Studio, the website, and
packaged CLI releases.

## Start Here

| Document | Use it for |
| --- | --- |
| [`../README.md`](../README.md) | Project overview, build/run commands, and the fastest path into Studio. |
| [`../ROADMAP.md`](../ROADMAP.md) | Current product direction and phase-level status. |
| [`language-reference.md`](language-reference.md) | The user-facing language reference for Friendly JTML 2 and Classic compatibility. |
| [`ai-authoring-contract.md`](ai-authoring-contract.md) | Rules for AI systems generating or editing runnable JTML. |
| [`jtml-element-dictionary.md`](jtml-element-dictionary.md) | Friendly element names, HTML mapping, and current media element guidance. |

## Product Roadmaps

| Document | Status |
| --- | --- |
| [`jtml-competitive-features-roadmap.md`](jtml-competitive-features-roadmap.md) | Active competitiveness roadmap across language, runtime, Studio, tooling, media, and ecosystem. |
| [`ai-native-implementation-roadmap.md`](ai-native-implementation-roadmap.md) | Active AI-native implementation plan for diagnostics, repairs, LSP, Studio, modularization, interop, and media. |
| [`media-graphics-roadmap.md`](media-graphics-roadmap.md) | Media and graphics track. Current runnable code can use `image`, `video/audio ... into controller`, `embed`, `file`, `dropzone`, `graphic`/`bar`/`dot`/`line`/`path`/`polyline`/`polygon`, `chart bar data rows by label value total`, raw canvas/SVG/custom elements, and `extern`. |
| [`3d-custom-interfaces-roadmap.md`](3d-custom-interfaces-roadmap.md) | 3D and highly customized interface strategy: `scene3d`, renderer packages, host hooks, fallbacks, and Studio inspectors. |
| [`studio-redesign-proposal.md`](studio-redesign-proposal.md) | Product/design RFC for making Studio the main JTML home and learning hub. |

## Tooling And Runtime

| Document | Use it for |
| --- | --- |
| [`deployment.md`](deployment.md) | Static HTML, live runtime, packaging, predeploy artifacts, and production hosting notes. |
| [`runtime-http-contract.md`](runtime-http-contract.md) | Stable JSON endpoints exposed by `jtml serve` and `jtml dev`. |
| [`language-server.md`](language-server.md) | LSP features and editor integration contract. |
| [`embedding-c-api.md`](embedding-c-api.md) | Native embedding through the C ABI. |

## Implementation Notes

| Document | Use it for |
| --- | --- |
| [`jtml-friendly-grammar-and-implementation-plan.md`](jtml-friendly-grammar-and-implementation-plan.md) | Historical and technical plan for Friendly syntax lowering and compatibility with Classic JTML. |
| [`repo-file-audit.md`](repo-file-audit.md) | Local repository hygiene snapshot and generated-folder policy. |

## Current Status Vocabulary

- **Implemented** means the syntax or tool is available in the CLI/runtime and
  should be safe to use in examples.
- **First slice** means the feature is usable, but intentionally narrower than
  the final design.
- **Hardened** means the feature has runtime/tooling tests and is part of the
  production path, while follow-up refinements may remain.
- **Planned** means it belongs on the roadmap but should not be generated in
  runnable examples unless explicitly marked experimental.

## Authoring Policy

Friendly JTML 2 is the default documentation dialect. Use Classic syntax only
when explaining compatibility, compiler artifacts, or migration from older
files. Runnable examples should pass:

```sh
./build/jtml check path/to/file.jtml
./build/jtml lint path/to/file.jtml
./build/jtml fmt path/to/file.jtml
```

For media and graphics, document the current standards-based surface separately
from planned declarative primitives so users and AI agents do not confuse the
roadmap with runnable syntax.
