# JTML for VS Code

Syntax highlighting, bracket matching, and snippets for the [JTML](https://github.com/mrjohnnyrocha/jtml) reactive HTML language.

## Features

- Syntax highlighting for Friendly JTML 2 files, including `jtml 2`, `let`, `get`, `when`, `make`, `page`, `route`, `layout`, `fetch`, `store`, `effect`, `extern`, `style`, element shorthands, strings, numbers, operators, and Classic fallback syntax.
- Snippets for production-oriented Friendly constructs: app starter, components, actions, fetch loading/error states, POST fetch options, routes, route layouts, stores, effects, scoped styles, input bindings, and extern host actions.
- **Native Language Server (`jtml lsp`)** powering diagnostics, formatting, completion, hover, go-to-definition, find-references, rename, code actions (Run jtml fix, source.fixAll.jtml), signature help, document highlights, selection range (expand/shrink selection along the indent hierarchy), and workspace symbol search across the module graph (`use` / `import` chains, transitively).
- `JTML: Apply Safe Fixes`, backed by `jtml fix -w --json` — complements the LSP code action when you prefer a keybinding.
- `JTML: Restart Language Server` to re-launch the server after CLI updates.
- Auto-closing brackets and quotes.
- Line comments with `//`.

The extension launches the LSP automatically when `vscode-languageclient` is
installed (declared as a dependency). When the dependency is missing or the
user opts out via `jtml.languageServer.enabled = false`, the extension
gracefully falls back to the legacy CLI-shell diagnostics/format path so it
always works against any `jtml` build.

## Settings

- `jtml.executablePath`: path to the JTML CLI. Leave empty to auto-detect `./build/jtml` or use `jtml` from `PATH`. Used both by the LSP launcher and the CLI fallback.
- `jtml.languageServer.enabled`: launch `jtml lsp` for native LSP features. Default `true`.
- `jtml.diagnostics.enabled`: enable or disable CLI-backed diagnostics. Only used when the language server is disabled.
- `jtml.diagnostics.onType`: refresh CLI-backed diagnostics as you edit using a temporary sibling file. Only used when the language server is disabled.

## Installation (local development)

Until the extension is published to the Marketplace:

Open `editors/vscode` in VS Code, install its declared development dependencies
with `npm install`. Loading the source in an Extension Development Host still
requires a local VS Code launch configuration; that interactive workflow has not
been verified by this audit. A folder
is not an installable VSIX; no Marketplace package or packaged-extension install
is claimed here.

The source manifest is 0.2.0; it is separate from the planned v0.3 CLI preview.
Project-owned editor files use Apache-2.0; dependencies retain their own terms.
See the repository LICENSE and THIRD_PARTY_NOTICES.md. A resolved VSIX dependency
and installation audit is still required before publishing an extension.

## Roadmap

- Debug Adapter Protocol integration for stepping through `.jtml` programs.
- Richer incremental diagnostics and incremental text sync from the LSP.
- `jtml refactor`-backed code actions (extract component, add wildcard route).

See the top-level `ROADMAP.md` for the project-wide plan.
