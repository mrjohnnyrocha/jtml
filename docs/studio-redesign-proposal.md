# JTML Studio — Redesign Proposal

Status: Accepted product RFC, partially shipped.

This document describes the desired Studio product shape. Shipped language and
runtime status is tracked in `../ROADMAP.md`,
`jtml-competitive-features-roadmap.md`, and
`ai-native-implementation-roadmap.md`.
Owner: Studio surface (`cli/studio_shell.cpp`, `cli/cmd_serve.cpp`).
Scope: incremental, ships in small PRs behind feature flags where possible.

This document captures the issues we hit in the current Studio, what we
already fixed, and a forward plan that keeps the surface honest to the
language while making the day‑to‑day experience faster, calmer, and more
discoverable.

## 1. Goals

1. **Trustworthy preview.** Every sample, lesson, and saved file must
   behave the same in the preview pane as it will on a deployed page.
2. **One‑file mental model.** Authors write Friendly JTML 2; Studio shows
   the lowered Classic only as a diagnostic surface, never as a target.
3. **Tight feedback loop.** Edit ➜ preview must feel instant on a laptop
   battery: no full reloads, no flash of unhydrated placeholders, no
   navigation away from the Studio shell.
4. **AI‑agent friendly.** A coding agent should be able to drive Studio
   end‑to‑end through stable HTTP endpoints and Friendly source files
   without scraping the UI.

Non‑goals: shipping a multi‑file project manager, in‑browser package
management, or a Friendly source debugger UI. Those belong in future
work or a separate IDE integration.

## 2. What we just fixed (May 2026)

| Symptom | Root cause | Fix |
| --- | --- | --- |
| `No bindings for element: attr_N` after toggling an `if/else`. | `Interpreter::interpretIfElement` only walked the initially‑taken branch, so the inactive branch's element IDs never reached `globalEnv->bindings`. | Walk both branches at registration time using the same element‑child dispatch as `interpretElement`. Regression locked in by `FriendlyConditional.BothBranchesRegisterEventBindings` in `tests/test_friendly.cpp`. |
| Route links inside the preview iframe could navigate the Studio shell itself. | Friendly `link "..." to "/x"` lowered to a plain `<a href>`. | Route‑aware lowering emits `data-jtml-href` and the router intercepts clicks (`src/friendly.cpp`, `src/transpiler.cpp`). |
| Component preview text flashed lowered locals like `__Counter_1_count`. | The SSR placeholder contained the lowered name; hydration depended on a full bindings snapshot which arrived a tick later. | Bindings are inlined as `window.__jtml_bindings = …` before the runtime script tag, so first paint is hydrated. See `withInitialBindings` in `cli/cmd_serve.cpp`. |
| `Counter`, `Form`, `Dashboard` Studio samples mixed Classic-style snippets into Friendly examples. | Samples were ported by hand and never linted. | Samples now use `jtml 2` Friendly examples and reserve lowered Classic for compiler artifacts. |

These are the immediate user‑visible wins. Everything below is the next
slice.

## 3. Diagnosed remaining friction

### 3.1 Sample registry duplication

`cli/studio_shell.cpp` ships 11 samples inline as JavaScript template
strings. Each sample lives twice in the repo (here, plus the matching
`examples/*.jtml` or `tutorial/*/code.jtml`). Drift between the two
copies is what produced the original `let X = X + N` / `div` regressions.

**Proposal.** Build samples at compile time from the canonical
`examples/` and `tutorial/` `.jtml` files via a small generator that emits
`studio_samples.inc.cpp`. The inline JS becomes a generated table:

```cpp
struct StudioSample {
    const char* name;
    const char* label;
    const char* category;
    const char* code;
};
extern const StudioSample kStudioSamples[];
```

Wins: a single source of truth, `verify_all.sh` already smoke‑tests every
`examples/*.jtml`, and edits flow naturally through PR review.

### 3.2 Preview rebuild cost

`POST /api/run` rebuilds the full HTML *and* re‑interprets the program
from scratch on every keystroke. On a small sample this is fine (~25ms),
but on `examples/dashboard.jtml` it climbs above 120ms and starts to
feel sticky.

**Proposal.**

1. Debounce the editor → run loop in the shell (already done at 280ms).
2. In `cmd_serve.cpp` `/api/run`, reuse the existing `interpreter` via
   `Interpreter::reload(program)` when the AST shape is unchanged, and
   only rebuild fully when component/route declarations change.
3. Add a `Cache-Control: private, max-age=0` + ETag on the served HTML
   so the iframe can short‑circuit identical reloads.

### 3.3 "Lowered Classic" panel is a distraction

The Artifacts panel still ships a `Lowered Classic` tab as the default.
This contradicts goal #2 — Friendly is the language; Classic is an
internal compiler artefact. New users open Studio, see Classic, and ask
"which one should I learn?".

**Proposal.**

- Default the Artifacts panel to `HTML` (the actual runtime output).
- Move `Lowered Classic` behind a `Diagnostics` tab alongside
  `Bindings JSON`, `State JSON`, and `Token stream`.
- Keep the reference text in the Help panel that explains the contract:
  Friendly is canonical; Classic is the compatibility target.

### 3.4 Error surface

Today, a syntax error renders as a red banner above the preview, but the
underlying `diagnostics` array from `/api/run` already carries line and
column information. The editor doesn't underline the offending range.

**Proposal.**

- Wire the existing `diagnostics` array into the CodeMirror gutter so
  errors show as red squiggles with hover tooltips.
- Mirror the same diagnostics into the VS Code extension via the LSP
  contract already implemented in `cli/cmd_lsp.cpp`.

### 3.5 Stateful URLs

Studio's mode (file vs lesson vs doc) lives in `localStorage` only. A
shared link to "show this lesson" or "open this sample" is not possible.

**Proposal.** Reflect Studio mode in the URL hash, e.g.
`#/sample/counter`, `#/lesson/03-events`, `#/file/<name>`. The hub‑page
keeps its current quick‑pick UI but every link becomes shareable.

### 3.6 Mobile / narrow viewport

The four‑panel layout collapses below ~900px to a single column with
overlapping bottom panels. This makes Studio unusable on a phone or in a
side‑by‑side workflow on a laptop.

**Proposal.** Below 1024px, collapse to a single‑pane tabbed view
(Editor / Preview / Diagnostics) with a header switcher. Keep keyboard
shortcuts (`⌘1`, `⌘2`, `⌘3`) bound to the tabs.

## 4. AI‑agent surface

Studio already exposes:

- `POST /api/run` — `{ code }` → `{ ok, html, classic, diagnostics }`.
- `POST /api/event` — `{ elementId, eventType, args }` → `{ ok, bindings }`.
- `GET  /api/bindings` — current bindings snapshot.
- `GET  /api/state` — full interpreter state.

This is enough for an agent to author Friendly source and verify it
without driving a browser. Two additions would close the loop:

1. **`POST /api/save`** — persist a buffer to disk under `samples/` so
   an agent can iterate against `verify_all.sh`.
2. **`GET /api/manifest`** — list the available samples, lessons, and
   docs with their slugs so agents can introspect Studio without
   scraping HTML.

Both endpoints stay local‑only (bound to `127.0.0.1`) and require a
header token issued by the running CLI session, mirroring the existing
event endpoint contract.

## 5. Phasing

| Phase | Scope | Risk |
| --- | --- | --- |
| 1 | Generate `studio_samples.inc.cpp` from `examples/` + `tutorial/`; remove inline duplicates. Default Artifacts → HTML. URL‑hash routing. | Low. Cosmetic + build wiring. |
| 2 | CodeMirror diagnostics gutter. `/api/manifest` + `/api/save`. Mobile single‑pane layout. | Medium. Touches editor integration but is additive. |
| 3 | Smarter `/api/run` (reload‑vs‑rebuild). ETag on served HTML. Bindings delta protocol over the existing WebSocket. | Higher. Needs careful regression coverage in `scripts/verify_all.sh`. |

Each phase ships behind a `--studio-experimental=<flag>` switch on the
CLI so we can canary without forking the binary.

## 6. Open questions

- Should the Studio shell move to TypeScript with a real build step, or
  stay as the hand‑written JS inside `studio_shell.cpp`? Today the
  zero‑build approach is a feature for embedding; revisiting it is only
  worth it if we want to share UI with the VS Code extension.
- Do we want a "share preview" mode where the running interpreter is
  exposed read‑only over a signed URL? Useful for bug reports, dangerous
  without auth.
- Do we need a Friendly source debugger (step through `when`/`effect`
  blocks)? Probably not until the language has user‑facing exceptions
  worth stepping through.

## 7. Acceptance

This proposal lands when:

- `scripts/verify_all.sh` still passes end‑to‑end.
- 153/153 unit tests still pass, plus new regression tests for any new
  endpoint or routing behaviour.
- The samples in `examples/`, `tutorial/`, and the Studio sample picker
  are byte‑identical (verified by the new generator step).
- Studio's default first‑run experience surfaces Friendly only, with
  Classic available as a clearly labelled diagnostic.
