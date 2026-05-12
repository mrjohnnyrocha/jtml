// src/refactor.cpp — implementation of jtml::renameSymbolInSource.
//
// The scanner is intentionally simple and conservative: it walks the source
// character by character, tracks string-literal and `//` comment state, and
// only rewrites occurrences that match the identifier exactly with word
// boundaries on both sides. This is the same shape as the LSP rename scanner;
// keeping a single canonical implementation in core lets the CLI and the
// language server stay byte-for-byte identical.
#include "jtml/refactor.h"

#include <cctype>
#include <sstream>

namespace jtml {

namespace {

bool isIdentifierChar(char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) lines.push_back(line);
    // Preserve a trailing empty line if the source ended with `\n`, so we can
    // round-trip through join() without dropping it.
    if (!text.empty() && text.back() == '\n') lines.push_back("");
    return lines;
}

std::string joinLines(const std::vector<std::string>& lines, bool trailingNewline) {
    std::string out;
    for (size_t i = 0; i < lines.size(); ++i) {
        out += lines[i];
        const bool isLast = (i + 1 == lines.size());
        if (!isLast) out.push_back('\n');
    }
    if (trailingNewline && !out.empty() && out.back() != '\n') out.push_back('\n');
    return out;
}

} // namespace

RenameResult renameSymbolInSource(const std::string& source,
                                  const std::string& oldName,
                                  const std::string& newName) {
    RenameResult result;
    result.source = source;
    if (oldName.empty() || oldName == newName) return result;

    const bool trailingNewline = !source.empty() && source.back() == '\n';
    auto lines = splitLines(source);

    for (size_t li = 0; li < lines.size(); ++li) {
        std::string& line = lines[li];
        std::string rewritten;
        rewritten.reserve(line.size());
        bool inString = false;
        char stringQuote = 0;
        for (size_t i = 0; i < line.size(); ) {
            const char ch = line[i];
            if (inString) {
                if (ch == '\\' && i + 1 < line.size()) {
                    rewritten.push_back(ch);
                    rewritten.push_back(line[i + 1]);
                    i += 2;
                    continue;
                }
                if (ch == stringQuote) inString = false;
                rewritten.push_back(ch);
                ++i;
                continue;
            }
            if (ch == '"' || ch == '\'') {
                inString = true;
                stringQuote = ch;
                rewritten.push_back(ch);
                ++i;
                continue;
            }
            if (ch == '/' && i + 1 < line.size() && line[i + 1] == '/') {
                rewritten.append(line, i, std::string::npos);
                i = line.size();
                break;
            }
            const bool boundaryStart = (i == 0) || !isIdentifierChar(line[i - 1]);
            if (boundaryStart &&
                i + oldName.size() <= line.size() &&
                line.compare(i, oldName.size(), oldName) == 0) {
                const size_t end = i + oldName.size();
                const bool boundaryEnd = (end == line.size()) || !isIdentifierChar(line[end]);
                if (boundaryEnd) {
                    // Edits report ORIGINAL-source line/column ranges so the
                    // same record can be turned into an LSP TextEdit by the
                    // language server without any further bookkeeping.
                    RenameEdit edit;
                    edit.line = static_cast<int>(li);
                    edit.startColumn = static_cast<int>(i);
                    edit.endColumn = static_cast<int>(end);
                    result.edits.push_back(edit);
                    rewritten.append(newName);
                    i = end;
                    continue;
                }
            }
            rewritten.push_back(ch);
            ++i;
        }
        line = std::move(rewritten);
    }

    result.changed = !result.edits.empty();
    if (result.changed) result.source = joinLines(lines, trailingNewline);
    return result;
}

} // namespace jtml
