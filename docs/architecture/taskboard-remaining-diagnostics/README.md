# Remaining Taskboard diagnostic correctness

Baseline: merged PR #39, `95e7bf0fceb39fa1f5d934e65170acdd88b00695`.
The existing isolated worktree uses `codex/taskboard-remaining-diagnostics`.
The unchanged Taskboard now passes lint with **0 errors / 1 advisory**, from
**28 errors / 7 warnings**. This is bounded diagnostic acceptance, not v0.3
release readiness or a complete type/provenance implementation.

## Independent observations

The baseline was rebuilt and measured before edits. The original Taskboard,
C01–C14, N01–N08, strict N04, conformance oracles, gate scripts and budgets are
unchanged. Taskboard SHA-256 remains
`dd59999cc5c351c809dbc4990fb045ceef89e1912999dc44a8d053b68fdc50da`.

| Category | Baseline | Final |
| --- | ---: | ---: |
| Callback argument binding / `JTML_UNDEFINED_NAME` | 12 errors | 0 |
| Generated input setter ownership / `JTML_UNDEFINED_NAME` | 14 errors | 0 |
| Keyboard payload / `JTML_ARITY` | 2 errors | 0 |
| Action while / `JTML_TEMPLATE_WHILE` | 6 warnings | 0 |
| Unlabeled card / `JTML_UI_SURFACE_UNLABELED` | 1 advisory | 1 advisory |
| **Total** | **28 errors / 7 warnings** | **0 errors / 1 advisory** |

`before.json` and `final.json` contain compact diagnostic inventories, including
original file/line/column, component and lexical scope when available. Intermediate
inventories retain the independently measured progression: keyboard/context
**26/1**, callback binding **14/1**, setter ownership **0/1**. `after-b.json`
records the initial setter candidate's diagnostic result; its compatibility issue
was repaired before final verification. It is not evidence that that intermediate
implementation passed the gate. `evidence.json` records baseline/final controls,
fixture hash and asset comparisons.

## First incorrect layers and corrections

**Callback arguments.** Friendly lowering classified `receive(record.id)` as an
element token rather than a standalone expression. Its arguments missed the
existing simultaneous caller/component binding pass, while the production body
plan retained the authored references. Standalone call/property expressions now
use the same expression role as the corresponding lowering production. Existing
binding rules still protect strings, member names and dictionary keys; inserted
caller arguments are not rebound through the callee's locals.

PR #39's preservation control exposed `{ "id": "task-1" }.id` after substitution.
The Classic parser previously attached postfix access only to bare identifiers,
returning early for objects, grouped expressions and function calls. Its existing
postfix construction is now a separate AST step after parsing any primary base.
No alternate expression evaluator or new runtime code is introduced. Standalone
parsing also rejects recovery ASTs when `consume()` has recorded an error; malformed
postfix syntax must not become an executable expression plan.

**Generated setters.** Friendly lowering appended every input setter to module
scope, losing the component closure and original input location. Component helpers
now collect at the nearest component boundary and emit inside its declaration
scope, after the DOM subtree. They close over that component's state even when
the input is nested in containers. Lint predeclares direct component functions for
forward references, but still analyzes each body in its actual lexical owner.
Helpers do not become global/sibling bindings. Ordinary module helpers retain
the existing native/live contract. Emission uses the original input line for
source attribution, including generated assignments. There is no generated-name
exemption. Existing production validation still requires simple local writable
`let` targets and rejects unsupported `into` shapes.

**Keyboard arity.** Lint omitted implicit keyboard arguments. Value-carrying
events reuse the existing attribute classifier; direct-component keydown contributes
one structured event payload and scroll keeps its existing value payload. The
normal explicit-plus-implicit arity check still rejects zero/excess arguments;
ordinary function and click calls retain their arity checks. This does not extend
keyboard backend support to additional invocation shapes.

**Action/template context.** Element nesting previously made every action's
`while` look like rendered template control flow. Lint now tracks action execution
context separately. A template-level while immediately after an action still warns.

**Accessibility.** TaskRow's `card` at source line 66 has neither `title` nor
`aria-label`. The existing semantic surface advisory is appropriate; a future
authoring change can give each card a meaningful accessible name. The unchanged
oracle retains this advisory. Its semantic diagnostic still has no precise
location; generated/helper attribution is construct-level (`sourceExact=false`),
not a claim of token-exact provenance. Strict N04 is a separate unchanged contract.

## Permanent controls

Seven minimal source controls are in
`tests/fixtures/diagnostics/taskboard-remaining/`; the existing emitted-callback
argument valid/invalid pair remains independent and unchanged. The baseline
incorrectly rejected valid keyboard handlers and accepted a zero-parameter one;
the final candidate reverses both results. Missing setter state and missing
callback arguments remain errors. Genuine template while retains its warning.
Some tiny fixtures also retain unrelated semantic usage advisories; their exact
counts are in `evidence.json`.

Nine added CTests cover these boundaries: keyboard aliases and explicit arguments;
action/template adjacency; object/grouped/indexed/nested-call arguments; literal,
member and dictionary-key preservation; local/sibling isolation; AST cloning and
setter origin; original imported-file/line/column/component attribution; malformed
postfix rejection and span extent; and native input-event dispatch into two separate
component closures. Existing parser, binding, production/native/live tests remain
unchanged.

## Reproduction

```sh
cmake -S . -B build -DJTML_BUILD_TESTS=ON -DJTML_BUILD_PYTHON=OFF
cmake --build build --parallel 8
ctest --test-dir build -R 'Linter|Parser\.|FriendlySyntax.ComponentInputHelpers|FriendlyMedia.FileInput|CliLint.Imported' --output-on-failure
build/jtml check tests/fixtures/production/taskboard-core/index.jtml --json
build/jtml lint tests/fixtures/production/taskboard-core/index.jtml --json
build/jtml lint tests/fixtures/diagnostics/taskboard-remaining/keyboard-invalid.jtml --json
build/jtml lint tests/fixtures/diagnostics/taskboard-remaining/setter-invalid.jtml --json
build/jtml lint tests/fixtures/diagnostics/emitted-callbacks/argument-invalid.jtml --json
ctest --test-dir build --output-on-failure
JTML_REQUIRE_BROWSER_BENCH=1 JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1 scripts/verify_all.sh
```

The three invalid lint commands exit 1. Template warnings do not cause a nonzero
lint exit. Use an executable Chrome/Chromium and Playwright installation as
specified in `verification.md`; a gate exit code alone is not proof of coverage.
