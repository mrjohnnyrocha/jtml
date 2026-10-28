# Imported Friendly components in clean production

Baseline: main `d2b31c2e33b0a2f613a3b354d9b503897dc981b6` after PR #51.
This correction uses the existing loader, source provenance, SemanticProject and
RuntimeProjectPlan. It does not add a new module system or enable production
compatibility assets. Published RC.2 is unchanged.

## Failure and ownership

At baseline, the modular generator first fails named-import validation because
its Card lacks `export`. Adding that keyword lets check/lint pass, but a clean
browser build still fails with `Unknown Friendly JTML component 'Card'`. The
existing `examples/friendly_module_component.jtml` reproduces the same production
failure without any generator edits.

The linked AST already has real imported declarations, validated by the module
loader. Browser project planning independently lowered each raw source file;
that discarded the declarations needed to resolve component calls. Compatibility
could synthesize placeholders, which clean production correctly prohibited.

Production module planning now lowers each module with its loader-resolved
declaration context. It then projects top-level AST nodes back to their original
owning file using source provenance. Imported declarations are context, not local
definitions: import/export records still come from the module's own source, and
SemanticProject resolves each instance/body node to the real defining module,
including re-exports. Compiler-generated stylesheet nodes have no executable
module ownership and remain part of the separately emitted linked document.
Other missing ownership attribution fails closed.

Static nested-component generation similarly needs the real cross-module
definition context. It now resolves against module-qualified definitions from the
project while emitting only each module's owned definitions. Generated child
calls select the real static module; no fake component or runtime operation-record
fallback is introduced.

The transitive fixture also exposed blank-line truncation in named export
selection. Blank/comment-only lines inside a selected declaration no longer end
its indented body. This retains actions and templates after a blank line without
changing export visibility or duplicate-import/export validation.

## Caller-owned slots

Compiling successfully was insufficient: static creation of a root component
could drop its caller's expanded slot content. Friendly lowering now marks that
supplied DOM with a transparent slot boundary. Generated static slot creation and
patching preserve the caller-owned markup from the connected instance. Nested
components continue using their existing supplied HTML contract.

This preserves caller expression/event metadata, default/named-slot separation
and caller action ownership. Markers are selected within the owning instance;
descendant component slots are not mistaken for its own. Slots do not invoke a
generic operation-plan renderer to initialize this path. The shared runtime
JavaScript is unchanged; no eval, new Function, compatibility update-plan asset
or source-rich operation metadata is added to production component modules.

## Reproduce and verify

With a compiler built from this branch, use unused output names:

```sh
./build/jtml new app /tmp/jtml-imported-starter
./build/jtml check /tmp/jtml-imported-starter/index.jtml
./build/jtml lint /tmp/jtml-imported-starter/index.jtml
./build/jtml build /tmp/jtml-imported-starter --target browser --out /tmp/jtml-imported-dist
./build/jtml build examples/friendly_module_component.jtml --target browser --out /tmp/jtml-imported-example
python3 -m http.server 8000 --bind 127.0.0.1 --directory /tmp/jtml-imported-dist
```

Open localhost:8000. The two Card headings must have their full prop values;
the first slot shows Count: 0 and Add one changes it to Count: 1. All four clean
assets are present and `jtml-update-plans.js` is absent.

Automated compiler coverage checks real definition ownership, transitive imports
and re-exports, complete export bodies, the generated starter, and fail-closed
missing modules/exports, private declarations, duplicate imports/exports and
unknown components. Invalid inputs must emit no runnable production assets.

```sh
ctest --test-dir build -R 'CliBuild.*(Imported|Modular)' --output-on-failure
JTML_BIN="$PWD/build/jtml" node tests/browser/imported_components.cjs
JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1 bash scripts/verify_all.sh
```

The browser commands require the repository's normal
`JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT` and `JTML_BENCH_BROWSER` variables. The new
suite is mandatory in the full local gate and Ubuntu/macOS CI. It verifies the
generated starter, unchanged existing example and four-module fixture under
`script-src 'self'`, including interaction, prop updates, slot ownership,
independent component state, retained identity and reload. It rejects unexpected
console/CSP errors, network/API/WebSocket dependencies and component fallback
telemetry. Execution guards throw if generated create/update functions invoke
interpreter/fallback helpers; shared DOM text markers are not used as a proxy.

## Evidence and boundaries

Raw baseline reproductions, candidate builds, CTest logs, Chromium screenshots
and traces are retained separately under `/tmp/jtml-imported-production.C0pZzn`.
Failed development attempts remain there; they are not presented as passes.
The separate onboarding branch and `/tmp/jtml-first-user.d6M2fw` were preserved;
all 12 original onboarding file hashes matched the pre-correction snapshot.

This is a focused imported-component correction, not a new release or a claim of
universal module/type/source-span support. Unsupported production shapes continue
to fail closed. Existing native-renderer discrepancies and reference HTML-size
overage are not resolved or reclassified here.

Final local gate: **PASS**, **480/480 CTests**, all **nine performance benchmarks
with clean browser exits**, the production reference, guarded imported-component
browser suite, CSP/lifecycle/input/Taskboard/provenance/security regressions,
site/editor/LSP/package relocation/install checks and live smoke **8/8**.
No verification checks were skipped. Shared runtime remains **271961/272000**
bytes. The known reference HTML overage is **99952/70000** bytes; budgets and
assertions are unchanged. Python bindings and new sanitizer runs are outside
this verification. Local macOS and hosted Ubuntu/macOS CI remain separate evidence.

The compact [verification record](imported-friendly-components/verification.json)
binds implementation/test files, the executed compiler, screenshots, browser
trace and complete gate log by hash. Large temporary artifacts remain outside Git.
The earlier full-gate failure from a DOM-marker test is retained: generated text
also uses that marker, so it could not distinguish interpreted rendering. The
final browser oracle instead guards actual interpreter/fallback helper execution.
