# JTML Repo File Audit

Last audited: 2026-05-11.

This audit records repository hygiene policy and the latest local verification
snapshot. Exact generated-file counts vary by machine because `build/`, `dist/`,
temporary package outputs, and local agent/tooling folders are intentionally
not part of the source contract.

## Source Areas

| Area | Classification |
| --- | --- |
| `include/jtml/` | Owned public C/C++ headers. |
| `src/` | Owned core language, runtime, formatter, linter, and export implementation. |
| `cli/` | Owned CLI commands, Studio shell, tutorial shell, and server integration. |
| `tests/` | Owned CTest/GoogleTest unit suite. |
| `examples/` | Owned runnable JTML examples; Friendly `jtml 2` is the default. |
| `tutorial/` | Owned lesson prose and lesson code for `jtml tutorial` and Studio learning surfaces. |
| `docs/` | Owned shareable documentation set. Start at `docs/README.md`. |
| `site/` | Owned predeploy static website source for the future public domain. |
| `editors/vscode/` | Owned VS Code extension assets. |
| `scripts/` | Owned build, package, verification, and helper scripts. |
| `third_party/` | Vendored single-header dependencies with upstream/license notices. |
| `.github/workflows/` | Owned CI workflow definitions. |
| `cmake/` | Owned reusable CMake helpers. |

## Generated Or Local Areas

| Area | Policy |
| --- | --- |
| `build/` | Generated CMake output; safe to recreate. |
| `dist/` | Generated release/site artifacts; safe to recreate. |
| `.claude/` and other local agent/tooling folders | Local workspace state; not product source. |
| temporary package/test outputs | Recreated by scripts and tests. |

## Checks Performed

- Classified generated/local/vendor files separately from owned source.
- Confirmed generated areas are treated as local outputs rather than canonical
  source.
- Confirmed vendored headers identify their upstream/license headers:
  `cpp-httplib` and `nlohmann/json`.
- Validated repository JSON files parse.
- Confirmed shell scripts under `scripts/` are executable.
- Checked local quoted C/C++ includes resolve.
- Checked every `src/*.cpp`, `cli/*.cpp`, and `tests/*.cpp` file is present in
  CMake or the tests CMake file.
- Checked every tutorial folder has both `lesson.md` and `code.jtml`.
- Checked tutorial code parses, lints, and formats idempotently.
- Checked every non-fixture `.jtml` file starts with `jtml 2`; intentional
  Classic fixtures remain documented as compatibility/stress inputs.
- Checked Markdown and static-site links for local consistency.

## Latest Verification Snapshot

The latest full local predeploy verification passed after the media, graphics,
chart runtime, SVG shape, and `scene3d` state-binding slices:

```text
scripts/verify_all.sh
153/153 unit tests passed
39 bundled examples smoke-tested
site artifact built
release directory and archive built
7/7 runtime smoke checks passed
```

`examples/keywords.jtml` still emits the expected linter warning because it is
kept as a Classic compatibility/stress fixture.

## Documentation Coherence Policy

- `README.md` is the public quick-start.
- `ROADMAP.md` is the phase-level product status.
- `docs/README.md` is the documentation table of contents and status
  vocabulary.
- `docs/language-reference.md` is the source of truth for runnable syntax.
- Roadmaps may describe planned syntax, but they must label it as planned so
  Studio, examples, tutorials, and AI agents do not present it as runnable.
