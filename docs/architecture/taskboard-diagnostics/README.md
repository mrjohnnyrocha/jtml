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

The permanent snapshots are `evidence/before-diagnostics.jsonl` (353 decisions)
and `evidence/after-diagnostics.jsonl` (51 decisions). Each JSON line records one
diagnostic in discovery order: severity, code, full message, component, lexical
scope, repository-relative source file, line, column, and `sourceExact`. Repeated
hints/examples and duplicate text/TSV renderings are omitted. These ledgers retain
all information needed to group counts by name, scope, and source construct.
[summary.json](evidence/summary.json), [command-status.json](evidence/command-status.json),
and [controls.json](evidence/controls.json) retain the aggregate
counts, actual exit statuses, fixture hash, and valid/invalid control results.

Original main emitted line/column 0 for these lint diagnostics. The **before**
ledger preserves the unique temporary observer's source/scope attribution: the
observer added context fields and corrected physical-line mapping but omitted
`localDeclaration` handling. All 353 decisions matched the untouched baseline's
message, severity, code, and discovery order. That historical equality and its
sequence hash are recorded in `evidence/historical-runs.json`. It is an
observation, not a second acceptance oracle. Baseline CLI output can be regenerated
from the recorded main revision; its additional observational attribution is
preserved here because the temporary observer is not a permanent compiler mode.

Verify the ledgers against the saved aggregate summary without rewriting files:

```sh
python3 docs/architecture/taskboard-diagnostics/summarize.py --check
```

To reproduce raw outputs, build the recorded baseline in a disposable checkout
and compare it with the repaired revision. Run from the repository root; output
and build artifacts stay outside version control:

```sh
baseline=315e6f280169330f0f6a7528484f3f78b8d5a1f9
baseline_dir="$(mktemp -d /tmp/jtml-lint-baseline.XXXXXX)"
git archive "$baseline" | tar -x -C "$baseline_dir"
cmake -S "$baseline_dir" -B "$baseline_dir/build" -DJTML_BUILD_TESTS=ON -DJTML_BUILD_PYTHON=OFF
cmake --build "$baseline_dir/build" --parallel 8
cmake -S . -B build -DJTML_BUILD_TESTS=ON -DJTML_BUILD_PYTHON=OFF
cmake --build build --parallel 8
out="$(mktemp -d /tmp/jtml-lint-evidence.XXXXXX)"
source=tests/fixtures/production/taskboard-core/index.jtml
for stage in before after; do
  binary="$baseline_dir/build/jtml"
  if [ "$stage" = after ]; then binary="$PWD/build/jtml"; fi
  for command in check lint; do
    lint_exit=0
    "$binary" "$command" "$source" --json > "$out/$stage-$command.json" || lint_exit=$?
    echo "$stage $command exit=$lint_exit" # check=0, lint=1 for both revisions
  done
  for fixture in valid missing-initializer missing-target; do
    lint_exit=0
    "$binary" lint "docs/architecture/taskboard-diagnostics/evidence/$fixture.jtml" --json \
      > "$out/$fixture-$stage.json" || lint_exit=$?
    echo "$stage $fixture exit=$lint_exit"
  done
done
```

The fixed baseline revision and unchanged source bytes define reproducibility;
absolute checkout paths and repeated remediation prose are not acceptance data.
See `evidence/verification.txt` for focused/full-gate commands, tool versions,
results, known failures, and asset comparison commands.

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
still exits 1 with two errors. The source fixtures and exact baseline/candidate
exit and error counts are retained in `controls.json`; the commands above
regenerate their raw output.

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

Verification results and reproduction commands are recorded in
`evidence/verification.txt`. `evidence/historical-runs.json` preserves the unique
failed-run observations, recorded benchmark measurements, and archive hashes.
Both initial native-browser full-gate timing failures remain recorded
independently of subsequent successful runs. The original conformance suite
continues to report five known native attribute-renderer discrepancies; those
are retained failures, not new semantic passes. The separate Studio `/api/export` concurrency
investigation is outside this change.

## Evidence retention review

The evidence-only follow-up keeps the root-cause analysis, all counts and known
limitations, the three minimal source fixtures, compact ordered diagnostic
ledgers, machine-readable summaries, verification results, source/log hashes,
and tool/reproduction commands. Implementation and permanent regression tests
are unchanged from `36b6450`.

Removed from the final PR diff: verbose diagnostic JSON with repeated advice,
duplicate text and TSV outputs, generated control/check outputs, the focused-test
transcript, and full compressed verification/build transcripts. The compiler and
existing gates reproduce their relevant conclusions. Unique baseline observer
attribution and historical timing-failure observations remain in the compact
snapshots; no failure is reclassified as a pass.

The original raw archives remain recoverable from the immutable implementation
commit if a historical transcript is needed. `historical-runs.json` records their
paths and SHA-256 hashes; for example:

```sh
git show 36b64502459295207bd74963a723201eb8294a1b:docs/architecture/taskboard-diagnostics/evidence/full-gate.log.gz > /tmp/jtml-full-gate.log.gz
gzip -dc /tmp/jtml-full-gate.log.gz > /tmp/jtml-full-gate.log
```
