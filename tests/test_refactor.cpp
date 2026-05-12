// tests/test_refactor.cpp — pins the canonical rename scanner.
//
// The scanner must:
//   * rewrite every word-boundary occurrence;
//   * leave string literals (both `"` and `'`) untouched;
//   * leave `//` line comments untouched;
//   * never edit sub-strings of a larger identifier;
//   * be a no-op for unchanged inputs (empty oldName, equal old/new, missing).
#include "jtml/refactor.h"

#include <gtest/gtest.h>

using jtml::renameSymbolInSource;

TEST(Refactor, RenamesWordBoundaryOccurrences) {
    const std::string src =
        "jtml 2\n"
        "let count = 0\n"
        "when add\n"
        "  count += 1\n"
        "\n"
        "page\n"
        "  show count\n";
    const auto result = renameSymbolInSource(src, "count", "total");
    EXPECT_TRUE(result.changed);
    EXPECT_EQ(result.edits.size(), 3u);
    EXPECT_NE(result.source.find("let total = 0"), std::string::npos);
    EXPECT_NE(result.source.find("total += 1"), std::string::npos);
    EXPECT_NE(result.source.find("show total"), std::string::npos);
    EXPECT_EQ(result.source.find("count"), std::string::npos);
}

TEST(Refactor, SkipsStringLiteralsAndComments) {
    const std::string src =
        "jtml 2\n"
        "// count is a comment word, do not touch\n"
        "let count = 0\n"
        "page\n"
        "  show \"count is in a string\"\n"
        "  show 'count single quoted'\n"
        "  show count\n";
    const auto result = renameSymbolInSource(src, "count", "total");
    EXPECT_TRUE(result.changed);
    // Exactly two edits: the `let` declaration and the bare `show count` site.
    EXPECT_EQ(result.edits.size(), 2u);
    EXPECT_NE(result.source.find("let total = 0"), std::string::npos);
    EXPECT_NE(result.source.find("show total\n"), std::string::npos);
    // The comment and the string literals are preserved verbatim.
    EXPECT_NE(result.source.find("// count is a comment word"), std::string::npos);
    EXPECT_NE(result.source.find("\"count is in a string\""), std::string::npos);
    EXPECT_NE(result.source.find("'count single quoted'"), std::string::npos);
}

TEST(Refactor, RespectsIdentifierBoundaries) {
    const std::string src =
        "let counter = 0\n"   // `count` is a sub-string here, must not match
        "let count = 1\n"
        "show count_total\n"  // also sub-string territory
        "show count\n";
    const auto result = renameSymbolInSource(src, "count", "total");
    EXPECT_TRUE(result.changed);
    EXPECT_EQ(result.edits.size(), 2u);
    EXPECT_NE(result.source.find("let counter = 0"), std::string::npos);
    EXPECT_NE(result.source.find("show count_total"), std::string::npos);
    EXPECT_NE(result.source.find("let total = 1"), std::string::npos);
    EXPECT_NE(result.source.find("show total\n"), std::string::npos);
}

TEST(Refactor, IsNoOpForEqualOrEmptyNames) {
    const std::string src = "let count = 0\n";
    auto same = renameSymbolInSource(src, "count", "count");
    EXPECT_FALSE(same.changed);
    EXPECT_EQ(same.source, src);

    auto emptyOld = renameSymbolInSource(src, "", "total");
    EXPECT_FALSE(emptyOld.changed);
    EXPECT_EQ(emptyOld.source, src);

    auto missing = renameSymbolInSource(src, "missing", "renamed");
    EXPECT_FALSE(missing.changed);
    EXPECT_EQ(missing.source, src);
}

TEST(Refactor, EditColumnsAreOriginalSourceRanges) {
    // Edits must report 0-based [startColumn, endColumn) ranges into the
    // ORIGINAL line, covering exactly `oldName`. This is the contract the
    // LSP rename relies on when delegating to renameSymbolInSource — the
    // same record gets shipped as an LSP TextEdit without translation.
    const std::string src = "let count = count + 1\n";
    const auto result = renameSymbolInSource(src, "count", "x");
    ASSERT_TRUE(result.changed);
    ASSERT_EQ(result.edits.size(), 2u);
    EXPECT_EQ(result.edits[0].line, 0);
    EXPECT_EQ(result.edits[0].startColumn, 4);
    EXPECT_EQ(result.edits[0].endColumn, 9);
    EXPECT_EQ(result.edits[1].line, 0);
    EXPECT_EQ(result.edits[1].startColumn, 12);
    EXPECT_EQ(result.edits[1].endColumn, 17);
    // Each edit span equals oldName.size() regardless of newName length.
    for (const auto& e : result.edits) {
        EXPECT_EQ(e.endColumn - e.startColumn, 5);  // "count".size()
    }
}

TEST(Refactor, PreservesTrailingNewline) {
    const std::string withNl = "let count = 0\nshow count\n";
    auto a = renameSymbolInSource(withNl, "count", "total");
    EXPECT_TRUE(a.changed);
    ASSERT_FALSE(a.source.empty());
    EXPECT_EQ(a.source.back(), '\n');

    const std::string withoutNl = "let count = 0\nshow count";
    auto b = renameSymbolInSource(withoutNl, "count", "total");
    EXPECT_TRUE(b.changed);
    ASSERT_FALSE(b.source.empty());
    EXPECT_NE(b.source.back(), '\n');
}
