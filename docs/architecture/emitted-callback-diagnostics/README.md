# Emitted callback diagnostic ownership

Baseline: `460a9d6004feb77e6e589129e30ac315e2b2813f` (merged PR #38), fetched
before creating a new isolated worktree and branch. **Taskboard lint still fails.**
This slice repairs callback declarations, not callback argument lowering.

## Independent diagnosis

The unchanged `tests/fixtures/production/taskboard-core/index.jtml` reproduces
44 errors and 7 warnings on current main. The two requested categories have
independent causes:

1. **16 callback-binding errors:** `collectComponentDefs` parses authored
   `emits` declarations, including payload arity/types. `componentDefinitionLine`
   retains them as compiler metadata. `analyzeSemanticProgram` produces
   `SemanticComponentDefinition.emits`, `emitArity`, `emitPayloads`, and
   `emitPayloadTypes`; `planComponentDefinition` copies those contracts into
   Runtime Plan. The browser dispatcher uses them to validate and forward
   emitted events to the owning instance's handler. Expanded AST calls still
   use the logical emit name. The old linter collects ordinary declarations,
   then enters a generic element scope without declaring these callbacks.
   Consequently a valid `done()` is reported as an undefined function.
2. **12 argument-binding errors:** `substituteIdentifiers` assigns element
   syntax roles to standalone call tokens such as `saved(record.id, draftNote)`.
   The translator later treats that same line as an expression statement.
   Syntax-role tokens are not substituted, leaving authored parameter/local
   names inside calls while surrounding instance bindings are renamed. This
   defect also occurs with a normal declared function and no `emits` at all.

A zero-payload emit reproduces the first cause without an argument expression;
a call to an already defined ordinary function reproduces the second without
an emit declaration. The four minimal fixtures and exact control results are
recorded in [evidence.json](evidence.json):

- [Binding valid](../../../tests/fixtures/diagnostics/emitted-callbacks/binding-valid.jtml):
  declared zero-payload `done`, wired to a parent action. Lint changes from one
  false undefined-function error to exit 0 with no diagnostics.
- [Binding invalid](../../../tests/fixtures/diagnostics/emitted-callbacks/binding-invalid.jtml):
  same contract, but calls undeclared `absent`. Still exits 1 with one error.
- [Argument valid](../../../tests/fixtures/diagnostics/emitted-callbacks/argument-valid.jtml):
  known ordinary `receive` called with a component parameter's member. Still
  reports one false undefined `record` error, independently of emit ownership.
- [Argument invalid](../../../tests/fixtures/diagnostics/emitted-callbacks/argument-invalid.jtml):
  calls that same ordinary function with genuinely absent `absent.id`. Still
  reports one undefined-name error. Both argument fixtures retain two existing
  usage warnings; no clean-lint claim is made for the deferred valid fixture.

Both valid fixtures compile with the baseline clean browser target. This is
not an application/oracle change to accommodate diagnostics.

## Selected correction

The linter reads contracts from canonical Semantic IR once per lint invocation.
When entering a component instance, it declares only that component's emitted
callbacks in that component's lexical scope, with their actual payload arity.
An owner tag prevents an enclosing component's emit callbacks from being
inherited by a nested component. Normal action/block bindings continue to win
in the nearest lexical scope. Ordinary DOM children retain their component
owner. Sibling/page scopes never receive those declarations.

Imported definitions come from the existing linked AST/semantic pipeline;
there is no new module/source scanner or global pool of callback names. Ambiguous
same-named definitions are not unioned to grant extra callbacks. Genuine
undefined callbacks and payload argument names remain errors. Calls still
visit all argument expressions, and arity validation remains active.

Friendly lowering, source spans, Runtime Plan, payload type contracts, browser
and native runtimes, and production assets are unchanged. Static payload type
inference is not expanded; existing runtime payload type validation is retained.
All retained Taskboard diagnostics match their original messages, codes, lexical
scope, component, source location, and precision flag. Construct attribution is
still not an exact variable-token span; generated setter provenance remains
outside this repair.

## Exact before/after results

Both revisions pass `check` (exit 0, no diagnostics). Taskboard `lint` still
exits 1 after the repair.

| Diagnostic category | Before | After |
| --- | ---: | ---: |
| Callback bindings / JTML_ERROR | 16 | 0 |
| Callback argument bindings / JTML_UNDEFINED_NAME | 12 | 12 |
| Generated setter ownership / JTML_UNDEFINED_NAME | 14 | 14 |
| Keyboard event arity / JTML_ARITY | 2 | 2 |
| Action/template while / JTML_TEMPLATE_WHILE | 6 | 6 |
| Unlabeled card / JTML_UI_SURFACE_UNLABELED | 1 | 1 |
| **Errors** | **44** | **28** |
| **Warnings** | **7** | **7** |

Removed: `movedDown` 4, `movedUp` 4, `noteSaved` 2, `titleSaved` 2, `deleted` 2,
`toggled` 2. Exactly 16 errors removed; zero new diagnostics. The full original
Taskboard source and independent expected outcomes are unchanged.

## Why argument lowering is deferred

An initial argument-role prototype removed the 12 argument errors, but a
preservation control exposed a regression: substituting an object-literal
component argument into `receive(record.id)` produces a literal followed by
member access that the Classic parser cannot consume. The same source passes
the baseline browser build because production uses the authored component body.
The control passes an inline `Sender { "id": "task-1" }`; the prototype
produces `receive({ "id": "task-1" }.id)` in Classic. Simply changing the call's
binding role is therefore not a general preservation repair; it also needs correct expression representation/postfix handling.

That prototype and its tests were discarded before committing. A partial gate
on the discarded prototype was stopped, not counted as a pass. The selected
callback-declaration repair leaves lowering byte-identical to main and avoids
that broader change. No argument errors are suppressed or reclassified.

The next slice should retain the argument expressions and lexical ownership
through lowering, with positive and negative controls for bound records,
object-literal arguments, action locals, grouping/indexing, and source positions.
Generated setters, keyboard implicit arguments, action/template while context,
and unrelated Studio concurrency are separate work.

## Regression coverage and reproduction

Six permanent regressions cover the valid/invalid fixture pair, cloning and
linter reuse, known payload arity and genuinely missing arguments, nested/sibling/
page isolation, distinct component contracts, shadowing of outer functions and
by action parameters, imported component isolation, and original module positions.
Existing semantic/event/runtime payload tests and independent oracles are retained.

Build each revision in a separate checkout using:

```sh
cmake -S . -B build -DJTML_BUILD_TESTS=ON -DJTML_BUILD_PYTHON=OFF
cmake --build build --parallel 8
ctest --test-dir build -R 'Linter.*(Callback|Callbacks|ActionBindingsShadow)|CliLint.ImportedEmitContracts' --output-on-failure
ctest --test-dir build -R 'Linter|CliLint|Semantic|SourcePropagation|SourceOrigin|FriendlySyntax.*(Emit|Event|Component|Binding|Typed)' --output-on-failure
```

To re-measure without hiding expected lint failures, run from each checkout and
save output under a temporary directory. For baseline controls, pass the fixture
paths from this branch to the baseline binary (the new fixtures are absent from
baseline main):

```sh
out="$(mktemp -d /tmp/jtml-emits-evidence.XXXXXX)"
build/jtml check tests/fixtures/production/taskboard-core/index.jtml --json > "$out/check.json"
lint_exit=0
build/jtml lint tests/fixtures/production/taskboard-core/index.jtml --json > "$out/lint.json" || lint_exit=$?
echo "lint exit=$lint_exit" # 1 on baseline and candidate
for fixture in tests/fixtures/diagnostics/emitted-callbacks/*.jtml; do
  lint_exit=0
  build/jtml lint "$fixture" --json > "$out/$(basename "$fixture").json" || lint_exit=$?
  echo "$fixture exit=$lint_exit"
done
```

The [verification record](verification.txt) retains commands, versions, hashes,
results, and known limitations. Raw build/verification logs stay outside Git;
[evidence.json](evidence.json) is the concise permanent measurement record.
