# Taskboard diagnostic correctness: nested Friendly local bindings

Baseline: `origin/main` at `315e6f280169330f0f6a7528484f3f78b8d5a1f9`
(PR #37, independently confirmed MERGED). Work started on a new managed worktree
and `codex/taskboard-diagnostic-correctness`.

**Taskboard lint remains failing: 44 errors and 7 warnings.** This slice repairs
302 false undefined-name errors; it does not establish complete lint acceptance
or v0.3 release readiness. Taskboard source, browser oracles, negative fixtures,
CSP checks, budgets, and gate scripts are unchanged.

## Measured baseline and candidate

Commands, run on the original source with the rebuilt baseline and candidate:

```sh
build/jtml check tests/fixtures/production/taskboard-core/index.jtml --json
build/jtml lint tests/fixtures/production/taskboard-core/index.jtml --json
build/jtml lint tests/fixtures/production/taskboard-core/index.jtml
```

Both `check` invocations exit 0 with no diagnostics. Both Taskboard `lint`
invocations exit 1. The historical audit totals happen to reproduce exactly.

| Severity / diagnostic code | Before | After | Removed |
| --- | ---: | ---: | ---: |
| error / JTML_UNDEFINED_NAME | 328 | 26 | 302 |
| error / JTML_ERROR | 16 | 16 | 0 |
| error / JTML_ARITY | 2 | 2 | 0 |
| warning / JTML_TEMPLATE_WHILE | 6 | 6 | 0 |
| warning / JTML_UI_SURFACE_UNLABELED | 1 | 1 | 0 |
| **Errors** | **346** | **44** | **302** |
| **Warnings** | **7** | **7** | **0** |

Of the removed errors, 272 belong to TaskBoard and 30 to TaskRow. Comparing
message, severity, code, and lexical scope finds **zero new diagnostics**.
The original fixture SHA-256 is recorded in `evidence/verification.txt`.

The complete original text/JSON output is in `evidence/before-lint.*`; complete
candidate output is in `evidence/after-lint.*`. `before-inventory.tsv` and
`after-inventory.tsv` group every occurrence by code, authored-name projection,
lowered name, component, AST scope, original construct file/line/column, and
location precision. The raw lowered name is retained because generated names
can represent different lexical bindings.

Original main emitted line/column 0 for these lint diagnostics. To locate the
baseline decisions, a temporary observational linter was linked against the
same compiler: it adds the candidate context fields and corrected physical-line
map, but omits the new `localDeclaration` handling. `before-context.json` retains
all 353 original diagnostic decisions **in the same order**, asserted against
the untouched baseline output by `summarize.py`. Its counts are observations,
not a second acceptance oracle. Recreate the aggregate tables with:

```sh
python3 docs/architecture/taskboard-diagnostics/summarize.py
```

## Independent diagnosis and first incorrect layer

Minimal valid input, retained at `evidence/valid.jtml`:

```jtml
jtml 2
make Counter initial
  let count = initial
  when advance step
    let next = count + step
    count = next
  button "Advance" click advance(1)
page
  Counter 2
```

Before: two errors, for assignment to the generated `next` and its subsequent
read. After: no errors, lint exits 0. Replacing the initializer's `count` with
`absent` still exits 1 with one undefined-name error. Removing `let` from `next`
still exits 1 with two errors. Full baseline/candidate output for both deliberately
invalid controls and the valid source is saved alongside `controls.json`.

The ownership trace is:

1. `src/friendly.cpp` parses authored `let` with `parseTypedIdentifier`. For
   nested blocks it intentionally emits an assignment for Classic/native
   compatibility, losing declaration identity and the annotation in the text.
   Component expansion renames the identifier; this is not a missing author
   declaration.
2. `cli/module_loader.cpp` links selected imports and normalizes Friendly source.
   `Parser` builds an AssignmentStatement, indistinguishable from a bare write
   without the retained frontend binding site.
3. `SemanticProgram` retains component body metadata. Its component summary
   includes descendant local state names, but is a flat summary, **not a lexical
   symbol table**. Hoisting that list into lint would incorrectly make branch,
   loop, and sibling action locals visible. SemanticProject owns module symbols
   and cloned ASTs, not a complete replacement lexical binding graph.
4. Runtime Plan component body nodes retain authored `let` as state with parent
   indexes. The static production emitter already reasons about these binding
   owners. That explains why browser execution can pass while lowered-AST lint
   fails. Reconstructing another source scanner in the CLI is unnecessary.
5. `JtmlLinter::visitStmt` previously checked every AssignmentStatement target
   against existing scopes and never declared it. That causes the initial false
   error and all subsequent reads/writes in the same action.

The smallest general repair is to retain the **existing frontend declaration
identity**, rather than infer declarations from arbitrary assignments, global
name lists, generated spelling, or diagnostic text.

## Repair boundary

The frontend now records lowered line -> local name/type binding sites in its
rich lowering result. Compilation provenance carries those sites across a
Classic prefix; the parser attaches matching sites to AssignmentStatement AST
nodes. AST cloning retains the metadata, including SemanticProject AST clones.
The linter checks the initializer in the enclosing lexical environment, checks
its declared type, and introduces the local only in the current scope.

Type and arity lookup stop at the nearest lexical binding, including an untyped
local that shadows an outer typed value or function. Bare assignments and missing
initializer reads remain errors. Classic assignment semantics are unchanged.

Generated CSS can contain literal physical newlines inside a single `emitLine`
call. The old source map recorded only one entry, shifting later attribution by
hundreds of lines in this fixture. Physical-line accounting now preserves the
correct original construct. Lint JSON includes `file`, `component`, `scope`, and
`sourceExact`; text diagnostics use the original file when available. Unspanned
desugared compound-assignment children retain their enclosing construct;
unreachable warnings use the offending statement rather than its enclosing block.

Friendly attribution here is **construct attribution**, with `sourceExact=false`,
not a claim of exact variable-token source spans. Fourteen generated input
setters still have no original location/owner, and the semantic advisory has no
location: all 15 unknowns remain explicitly recorded as line/column 0. Generated
names in messages remain a limitation. The existing strict N04 exact-provenance
contract is separate and unchanged.

No generated Classic text, interpreter assignment behavior, runtime codegen,
public manifest schema, or browser fallback policy changes. Independently building
the unchanged Taskboard with baseline and candidate yields byte-for-byte identical
clean browser output in all files (`diff -qr`, exit 0).

## Classification of all remaining categories

| Group | Before | After | Assessment / next independently bounded slice |
| --- | ---: | ---: | --- |
| Erased local declaration and dependent reads/writes | 302 | 0 | Demonstrated false positives; repaired at lowering -> AST ownership boundary |
| Emitted callback names | 16 | 16 | Missing emit-binding ownership in lowered diagnostic path; `movedDown`, `movedUp`, `noteSaved`, `titleSaved`, `deleted`, `toggled` exist in component contract |
| Arguments inside emitted callback expressions | 12 | 12 | Lowering leaves authored `record`, `draftNote`, `trimmed` references while surrounding instance state is renamed; production body-plan path retains the correct owner |
| Generated input setter targets | 14 | 14 | Setter functions emitted outside component scopes; linter cannot see their component-owned targets, and generated setter origins are absent |
| Keyboard implicit event argument | 2 | 2 | `onKeyDown` gets an event in browser execution but linter's implicit-argument table omits it |
| Action while reported as template while | 6 | 6 | Element nesting depth leaks into action analysis; execution context needs its own ownership rule |
| Unlabeled semantic card | 1 | 1 | Advisory: the TaskRow card has no title/aria-label; not a name-resolution defect |

These are diagnostic/backend ownership mismatches and one advisory, not evidence
that the accepted Taskboard source has 44 genuine undefined bindings. No genuine
Taskboard binding error was independently established. The deliberately invalid
controls are genuine errors and still fail. Unsupported production shapes such
as nested declarations that collide with other persistent component storage are
still rejected by existing production ownership validation; this diagnostic
repair does not extend that supported surface.

The next slices should independently prove emit binding/argument ownership,
then generated setter ownership and origin, and separately event argument and
action/template context. A shared richer lexical Semantic IR would be a larger
follow-up, not a pretext for hoisting all component names or silently accepting
unsupported shapes in this PR.

## Permanent regressions and verification

Ten new permanent tests cover valid component actions and multiple instances;
missing initializer and bare target; branch/loop/action/component scope escape;
self-initialization and type mismatch; typed and untyped shadowing; unchanged
Classic assignment; imported valid and invalid actions with original module
locations; AST clone retention; multiline CSS attribution; and Classic-prefix
binding offsets. The tests use the actual linked compilation pipeline or rich
lowering result, not a second source scanner.

Verification results and complete logs are recorded in `evidence/verification.txt`.
Both initial native-browser full-gate timing failures remain recorded
independently of subsequent successful runs. The original conformance suite
continues to report five known native attribute-renderer discrepancies; those
are retained failures, not new semantic passes. The separate Studio `/api/export` concurrency
investigation is outside this change.
