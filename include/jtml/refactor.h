// refactor.h
//
// Source-level refactoring primitives for JTML.
//
// The first slice exposes `renameSymbolInSource`, a string- and comment-aware
// rewrite of every word-boundary occurrence of an identifier in a single
// source file. The same scanner powers `textDocument/rename` in the language
// server and `jtml refactor rename` in the CLI, so both paths stay
// behaviourally identical.
//
// Unlike `fixSource`, refactors here are user-driven semantic edits rather
// than mechanical repairs. Callers provide the symbol name; the function
// guarantees it never edits text inside string literals or `//` comments,
// and never edits sub-strings of a larger identifier.
#pragma once

#include <string>
#include <vector>

namespace jtml {

struct RenameEdit {
    int line = 0;        // 0-based, original source line.
    int startColumn = 0; // 0-based original-source column, inclusive.
    int endColumn = 0;   // 0-based original-source column, exclusive.
                         // The replaced span [startColumn, endColumn) covers
                         // exactly `oldName` in the original line; combined
                         // with `newName` this is directly usable as an LSP
                         // TextEdit by the language server.
};

struct RenameResult {
    std::string source;          // The rewritten source. Equal to the input when no edits applied.
    std::vector<RenameEdit> edits; // Every applied edit, in source order.
    bool changed = false;        // True iff `edits` is non-empty.
};

// Rewrite every word-boundary occurrence of `oldName` to `newName` in `source`.
// Skips occurrences inside `"..."` / `'...'` string literals and `//` line
// comments. Empty `oldName` or `newName == oldName` returns the input unchanged.
RenameResult renameSymbolInSource(const std::string& source,
                                  const std::string& oldName,
                                  const std::string& newName);

} // namespace jtml
