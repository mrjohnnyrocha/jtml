# Canonical component scene3d rendering

Base: clean merged main `47b547806e742b2b0ae2ca38b459b7efd1ec0e07`.
Branch: `codex/media-renderer`. This correction excludes the preserved route-slot
ownership candidate and its large audit-evidence commit.

## Failure and language contract

The unchanged-main [Chromium baseline](before.json) exits **1** with
`Unescaped string delimiter`. The independent Media fixture retains the original
Enterprise Media definition/state and routes it directly, without a layout.
The accompanying smaller fixture exercises direct static scenes separately.
Neither fixture supplies an external renderer or fake application services.

`scene3d` is valid documented Friendly syntax. The existing frontend translates
it to an accessible `canvas` with scene/camera/controls/renderer data attributes,
an optional controller name and dimensions. The runtime's existing host bridge
uses these attributes and draws its documented fallback when no host is installed.
This patch does not add Three.js or claim real 3D rendering/authentication.

Component definition metadata retained the original Friendly element line in its
Runtime Plan. The generic static/runtime element parsers then treated scene
modifiers as text. A partial static Media plan reached runtime child rendering
with a malformed content expression:

```text
"Interactive launch room" scene launchRoom camera orbit orbit renderer "three"
```

It threw during startup. A simple direct static scene could instead silently
render the wrong element/content, so absence of the exception alone is insufficient.
This is a lowering/renderer representation mismatch, not invalid author syntax.

## Bounded correction

- Reuse **the existing Friendly scene parser** to provide canonical canvas words.
  Do not duplicate scene grammar in the runtime or add an interpreter/fallback.
- Runtime Plan builds the original semantic read/write set, then canonicalizes
  the render node and clears its content expression: a scene canvas is an
  attribute-only element. Group each attribute expression so interpolated labels
  cannot be split into syntax/attributes. Static and runtime consumers receive
  the same canonical plan and use their existing element paths.
- Preserve source line/column and original provenance. The authored write set
  remains available for ownership policy after rendering normalization. An
  imported scene definition cannot turn a text-input capability into a scene
  controller: a regression requires pre-emission rejection with caller and
  original definition locations.
- Match Friendly's existing attribute namespace rule in both component parsers:
  semantic UI modifiers apply only to semantic UI primitives. Plain canvas
  width/height remain HTML attributes. Without this correction the accessible
  canvas rendered at its default dimensions instead of the authored dimensions.

No module-name binding fallback, writable route capability, router/lifecycle
change, dynamic-event dependency correction or production-schema extension is
included. The original Enterprise source is untouched. Root scene syntax and its
existing controller bridge are retained; this is not a new mutable-input contract.

## Focused evidence

[After](after.json): real Chromium 151.0.7922.34 exits **0**, all five checks pass:

1. The Media scene is the actual accessible canvas with exact scene/camera/
   controls/renderer/controller values, not malformed text or a custom tag.
2. Dimensions are 720×420; the existing no-host fallback/controller reports real
   canvas state without installing a fake renderer.
3. Strict script CSP, clean production assets and the runtime ceiling hold.
4. The minimal static scenes preserve reactive labels, sibling-local state,
   canvas/input identity, focus and selection through a readonly parameter update.
5. Root scene rendering, dimensions and the established controller bridge work.

Seven focused CTests pass, including canonical-plan/source/write preservation,
existing lowering/lint/formatter coverage and the imported binding rejection.
The existing strict-CSP suite, clean/compat literal/XSS suite, clean/compat/debug
input synchronization, global parameter ownership, nine explicit-input Chromium
checks and all 22 negative cases pass. [Compiler trace](compiler-trace.json)
retains the canonical plan and original source span.

Runtime **272,992 → 272,989 raw (-3)**, **50,893 → 50,903 gzip (+10)**.
The **273,000-byte ceiling is unchanged**, leaving 11 raw bytes. Combining the
existing parser accumulator declarations offsets the namespace predicate's small
raw cost; no validations or runtime behaviors were removed.

```sh
# Supply real Chromium/Playwright paths; the browser regression cannot skip.
node tests/browser/media_renderer.cjs
ctest --test-dir build -R 'Scene3D|ExplicitInput' --output-on-failure
JTML_REQUIRE_BROWSER_BENCH=1 JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1 scripts/verify_all.sh
```

This committed snapshot records focused evidence. One complete gate on the exact
final commit, all nine required clean-exit benchmarks, and exact-head Ubuntu/macOS
CI are recorded on the focused PR before a GO decision. No automatic merge,
release or deployment is authorized. The preserved ownership worktree remains
unchanged until this renderer prerequisite is merged, then can be resumed on
updated main without including the audit-evidence commit in its implementation PR.
