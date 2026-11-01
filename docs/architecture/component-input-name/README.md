# Component input accessible names

Base: merged main `325bae128aaaa729d4876159099e63fce9fe59c4` (PR #67).

## Existing syntax and defect

`input "Email" into email required type "email"` is valid Friendly syntax.
`FriendlySyntax.PageElementsEventsAndInputLowerToClassic` already requires its
lowering to `@input placeholder="Email" value=email ... required`.
Friendly `translateElement` classifies the initial quoted token as inline content,
then emits placeholder for input/textarea, or aria-label for file/dropzone aliases.
This correction preserves that contract; it does not invent an aria-label on text
inputs. Chromium uses their placeholder as the fallback accessible name.

The original body-plan source survives Semantic IR and Runtime Plan. However,
`compileStaticElementParts` collected that same token as child content. The static
input creator emits a void element, losing its text and accessible name; textarea
incorrectly received text content instead of its authored placeholder. Explicit
`aria-label` and `placeholder` attributes remained correct.

The compiler now maps the classified initial inline token to the same attribute
as Friendly lowering. Attribute order is preserved so explicit overrides win.
Both static create and retained patch use these compiled parts. No listener,
ownership, binding capability, source classifier, runtime expression evaluator,
public schema or browser runtime code changes are needed.

## Real Chromium evidence

Run `node tests/browser/component_input_name.cjs` with the existing mandatory
`JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT` and `JTML_BENCH_BROWSER` settings.
The fixture uses local writable input state and two isolated Editors in both plain
and layout routes, plus a root input control. Strict script CSP applies.

- Unchanged-main-equivalent compiler: **2/6, exit 1** (`before.json`). This is the
  preserved release compiler from PR #67; its source tree equals merged main.
- Candidate: **6/6, exit 0** (`after.json`), Chromium 151.0.7922.34, no page exceptions.
- Covers literal input names, textarea placeholders, existing file aliases,
  parameterized explicit placeholders, explicit name/hint overrides, repeated edits,
  mirror updates, sibling isolation, root behavior and retained DOM/focus/cursor.
- **41/41 focused CTests pass**. Existing route-slot ownership, input-state-sync,
  strict-CSP and explicit-input Chromium suites exit zero.

Enterprise source and audit remain unchanged: **8/11, exit 1**, improved from the
merged baseline's 7/11. Email is accessible again. Its unauthorized implicit module
write and the two dependent token/audit assertions remain NO-GO. No authentication
service is invented (`enterprise-after.json`).

Exploratory controls also found that bare parameter text is not the existing
inline-input syntax; quoted interpolation encounters a separate Friendly lowering
parse error. Parameter hints use the established `placeholder caption` form here.
An optional compatibility-layout build encounters the existing slot ownership schema
rejection. These independent limitations are not changed or suppressed. This suite
requires clean production for both route variants; existing compat/debug input suites
remain required by the full gate.

## Delivery

Shared runtime is byte-identical to main: **272,370 raw / 50,835 gzip bytes**,
within the unchanged **273,000-byte** ceiling. Runtime growth: zero.
The focused correction is GO after the final committed candidate passes one complete
local gate (including nine clean-exit benchmarks) and exact-commit Ubuntu/macOS CI.
Those results are recorded on the PR; Enterprise application readiness remains NO-GO.
Reactive event arguments, Enterprise source and all ownership contracts are excluded.
