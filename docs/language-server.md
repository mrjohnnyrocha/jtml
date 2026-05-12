# JTML Language Server

`jtml lsp` runs a minimal Language Server Protocol server over stdio.

It is the editor-neutral foundation for JTML tooling. Editors can launch the
native binary directly:

```sh
jtml lsp
```

## Implemented

- `initialize`
- `shutdown`
- `exit`
- full text sync through `textDocument/didOpen`, `textDocument/didChange`, and
  `textDocument/didSave`
- diagnostics through `textDocument/publishDiagnostics`
- whole-document formatting through `textDocument/formatting`
- generic Friendly JTML completions plus same-file and imported user-defined
  symbols through `textDocument/completion`. Imported items advertise their
  origin module in `detail` so editors can disambiguate same-named symbols.
- keyword + user-symbol hover documentation through `textDocument/hover`,
  including imported symbols (annotated with the origin file).
- outline symbols through `textDocument/documentSymbol`. Indexed declarations
  cover Friendly `make Component`, lowercase `make foo a b` function-style
  declarations, classic `function foo(args)`, `when`, `let` / `get` / `const`,
  `route ... as`, `effect`, `store`, and `extern`.
- cross-file go-to-definition through `textDocument/definition` that walks
  `use` / `import` graphs relative to the open document, with transitive
  resolution and cycle guarding.
- workspace symbol search through `workspace/symbol`. Scans the workspace
  root recursively (skipping `build`, `dist`, `node_modules`) plus the
  module graph of every open document, preferring open-document text over
  on-disk content.
- prepare-rename and rename through `textDocument/prepareRename` /
  `textDocument/rename`. Rewrites a symbol across every file reachable
  from the open document. The scanner is string- and comment-aware, so
  keywords and tag names are never edited.
- references through `textDocument/references`, sharing the rename
  scanner. `context.includeDeclaration` is honoured.
- code actions through `textDocument/codeAction`, with a
  `source.fixAll.jtml` umbrella plus per-diagnostic quick-fixes for
  `JTML_FIX_HEADER`, `JTML_FIX_TABS`, `JTML_FIX_TRAILING_SPACE`,
  `JTML_FIX_FINAL_NEWLINE`, and `JTML_INDENTATION`. Edits are produced by
  `jtml::fixSource` and shipped as `WorkspaceEdit`s.
- `JTML_EVENT_ACTION` code actions for incomplete event bindings such as
  `button "Save" click`. The LSP infers an action name from the button label,
  wires the event, and appends a matching `when actionName` block.
- signature help through `textDocument/signatureHelp`. Walks the module
  graph for parameter info on `make`, `when`, and `function` declarations,
  and advances `activeParameter` across commas in the call site.
- document highlights through `textDocument/documentHighlight`, reusing
  the rename-grade reference scanner so highlights never appear inside
  strings or comments.
- selection range through `textDocument/selectionRange`. Returns a nested
  SelectionRange tree per requested position whose layers walk Friendly
  JTML's indentation hierarchy: word at cursor → trimmed line → full line
  → innermost indented block → enclosing blocks → whole document. AI
  editors drive "expand selection" / "shrink selection" with this tree.

Diagnostics use the same compiler path as the CLI:

1. parse with Friendly/Classic auto-detection
2. publish parse diagnostics immediately if parsing fails
3. run the linter when parsing succeeds

Friendly parse diagnostics are remapped from lowered Classic line numbers back
to the original Friendly source lines before they are published.

Formatting uses `jtml fmt` semantics, including Friendly-preserving formatting
for `jtml 2` source.

## Current Limits

- Text sync is full-document sync, not incremental patches.
- Diagnostics use the current structured diagnostic contract, but some Friendly
  source spans still come from the lowered/parser stage.

## Client Notes

The server accepts `file://` documents. For open buffers, it writes a temporary
sibling `.jtml` file while compiling so relative `use` imports resolve the same
way they do from the real source file.
