# JTML Documentation

This folder is the shareable documentation set for JTML. The root stays small:
start here, then follow the section folders.

## Read First

| Document | Use it for |
| --- | --- |
| [`../README.md`](../README.md) | Project overview, build/run commands, and the fastest path into Studio. |
| [`../ROADMAP.md`](../ROADMAP.md) | Current product direction and phase-level status. |
| [`architecture/observable-first-architecture-roadmap.md`](architecture/observable-first-architecture-roadmap.md) | Architecture thesis: Friendly source, typed AST, semantic IR, observable graph, and multiple backends. |
| [`architecture/language-family-design.md`](architecture/language-family-design.md) | Consolidated JTML/JTL design: dialect boundaries, semantic core, interop, and enterprise goals. |
| [`roadmaps/next-priorities.md`](roadmaps/next-priorities.md) | The immediate implementation order after the semantic cleanup slice. |
| [`reference/language-reference.md`](reference/language-reference.md) | The user-facing language reference for Friendly JTML 2 and the compatibility backend. |
| [`reference/ai-authoring-contract.md`](reference/ai-authoring-contract.md) | Rules for AI systems generating or editing runnable JTML. |

The compact machine-readable language surface is available from the CLI:

```sh
jtml keywords --json
jtml ui --json
jtml doctor --json
```

Use `jtml doctor --json` for the current machine-readable readiness contract:
local toolkit checks, verification gates, stable/first-slice/experimental
tiers, and current enterprise-readiness status.

## Folder Map

| Folder | Contents |
| --- | --- |
| [`reference/`](reference/) | Stable language, element, and AI authoring references. |
| [`architecture/`](architecture/) | Product and compiler architecture RFCs. |
| [`roadmaps/`](roadmaps/) | Active implementation tracks and longer-horizon competitiveness plans. |
| [`tooling/`](tooling/) | Runtime API, deployment, LSP, and embedding contracts. |
| [`archive/`](archive/) | Historical implementation notes and repo hygiene snapshots. |

## Studio Content

Studio should load product content from files rather than long embedded C++
literals wherever practical. Playground examples live under
[`../studio/samples/`](../studio/samples/) with a `manifest.json`; `jtml studio`
serves them through `/api/studio/samples` and keeps the embedded shell list only
as a fallback. The Studio mini-reference now follows the same pattern through
[`../studio/reference/catalog.json`](../studio/reference/catalog.json) and
`/api/studio/reference`. Sidebar labels and pinned templates live in
[`../studio/sidebar/catalog.json`](../studio/sidebar/catalog.json) and are
served through `/api/studio/sidebar`. Follow this pattern for future larger
prose blocks.

## Reference

| Document | Use it for |
| --- | --- |
| [`reference/language-reference.md`](reference/language-reference.md) | Canonical runnable syntax. |
| [`reference/jtml-element-dictionary.md`](reference/jtml-element-dictionary.md) | Friendly element names, HTML mapping, and current media element guidance. |
| [`reference/ai-authoring-contract.md`](reference/ai-authoring-contract.md) | AI-safe idioms, current feature status, and generation rules. |

## Architecture

| Document | Use it for |
| --- | --- |
| [`architecture/observable-first-architecture-roadmap.md`](architecture/observable-first-architecture-roadmap.md) | Semantic IR and observable-graph dependency order. |
| [`architecture/language-family-design.md`](architecture/language-family-design.md) | How JTL core and JTML web/app syntax should evolve together without splitting the architecture into scattered plans. |
| [`architecture/studio-redesign-proposal.md`](architecture/studio-redesign-proposal.md) | Product/design RFC for Studio as the main JTML home and learning hub. |

## Roadmaps

| Document | Status |
| --- | --- |
| [`roadmaps/next-priorities.md`](roadmaps/next-priorities.md) | Active near-term order: semantic styling, browser-local/runtime parity, component semantics, platform modularization, and docs/Studio hardening. |
| [`roadmaps/jtml-competitive-features-roadmap.md`](roadmaps/jtml-competitive-features-roadmap.md) | Competitiveness roadmap across language, runtime, Studio, tooling, media, and ecosystem. |
| [`roadmaps/ai-native-implementation-roadmap.md`](roadmaps/ai-native-implementation-roadmap.md) | AI-native diagnostics, repairs, LSP, Studio, modularization, interop, and media. |
| [`roadmaps/media-graphics-roadmap.md`](roadmaps/media-graphics-roadmap.md) | Media and graphics track. |
| [`roadmaps/3d-custom-interfaces-roadmap.md`](roadmaps/3d-custom-interfaces-roadmap.md) | 3D and highly customized interface strategy. |

## Tooling And Runtime

| Document | Use it for |
| --- | --- |
| [`tooling/deployment.md`](tooling/deployment.md) | Static HTML, live runtime, packaging, predeploy artifacts, and production hosting notes. |
| [`tooling/runtime-http-contract.md`](tooling/runtime-http-contract.md) | Stable JSON endpoints exposed by `jtml serve` and `jtml dev`. |
| [`tooling/language-server.md`](tooling/language-server.md) | LSP features and editor integration contract. |
| [`tooling/embedding-c-api.md`](tooling/embedding-c-api.md) | Native embedding through the C ABI. |

## Archive

| Document | Use it for |
| --- | --- |
| [`archive/jtml-friendly-grammar-and-implementation-plan.md`](archive/jtml-friendly-grammar-and-implementation-plan.md) | Historical implementation note for Friendly syntax lowering and Classic compatibility. Not the product direction. |
| [`archive/repo-file-audit.md`](archive/repo-file-audit.md) | Local repository hygiene snapshot and generated-folder policy. |

## Status Vocabulary

- **Implemented** means the syntax or tool is available in the CLI/runtime and
  should be safe to use in examples.
- **First slice** means the feature is usable, but intentionally narrower than
  the final design.
- **Hardened** means the feature has runtime/tooling tests and is part of the
  production path, while follow-up refinements may remain.
- **Planned** means it belongs on the roadmap but should not be generated in
  runnable examples unless explicitly marked experimental.
- **Experimental** means the feature may appear in demos, tooling contracts, or
  internal roadmap work, but is not yet a production compatibility promise.

JTML is enterprise-relevant but not enterprise-ready yet. The enterprise lane
is tracked through `ROADMAP.md`, `roadmaps/next-priorities.md`, and
`jtml doctor --json`; the priority is reducing compatibility bridges and
monolithic implementation debt while strengthening semantic IR ownership. The
current component lane has first-slice browser-local body-plan execution; full
component parity is still planned before expanded compatibility output can be
treated as optional.

## Authoring Policy

Friendly JTML 2 is the default documentation dialect. Use Classic syntax only
inside explicitly labeled compatibility/backend sections, compiler artifact
notes, or migration guidance from older files. Runnable examples should pass:

```sh
./build/jtml check path/to/file.jtml
./build/jtml lint path/to/file.jtml
./build/jtml fmt path/to/file.jtml
```

For media, graphics, styling, and interop, keep implemented syntax separate
from planned primitives so users and AI agents do not confuse roadmap language
with runnable code.
