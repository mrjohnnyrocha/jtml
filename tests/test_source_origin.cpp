#include <gtest/gtest.h>
#include "jtml/source_origin.h"
#include "cli/module_loader.h"
#include <filesystem>
#include <fstream>
#include <chrono>

namespace {
struct SourceFixture {
    std::filesystem::path directory = std::filesystem::temp_directory_path() /
        ("jtml-origin-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    SourceFixture() { std::filesystem::create_directories(directory); }
    ~SourceFixture() { std::filesystem::remove_all(directory); }
    std::string write(const char* name, const std::string& source) {
        auto path = directory / name;
        std::ofstream(path, std::ios::binary) << source;
        return std::filesystem::weakly_canonical(path).string();
    }
};
void expectOrigin(const jtml::MappedSource& text, size_t begin, size_t count,
                  const std::string& file, size_t originalOffset, size_t line, size_t column) {
    auto range = text.exactRange(begin, begin + count);
    ASSERT_TRUE(range.has_value());
    EXPECT_EQ(range->source->file, file);
    EXPECT_EQ(range->begin, originalOffset);
    EXPECT_EQ(range->end, originalOffset + count);
    EXPECT_EQ(range->source->coordinates(range->begin), std::make_pair(line, column));
    EXPECT_EQ(text.text().substr(begin, count), range->source->text.substr(originalOffset, count));
}
}

TEST(SourceOrigin, CopiedSyntheticAndDiscontinuousRangesAreDistinct) {
    auto a = jtml::MappedSource::original("a", "same same");
    auto b = jtml::MappedSource::original("b", "same same");
    auto result = a.slice(5, 4);
    result.append(b.slice(0, 4));
    result.appendSynthetic("same");
    expectOrigin(result, 0, 4, "a", 5, 1, 6);
    expectOrigin(result, 4, 4, "b", 0, 1, 1);
    EXPECT_FALSE(result.exactRange(0, 8));
    EXPECT_FALSE(result.exactRange(8, 12));
    EXPECT_FALSE(result.exactRange(0, 0));
    EXPECT_FALSE(result.exactRange(0, 99));
    auto contiguous = a.slice(0, 2);
    contiguous.append(a.slice(2, 2));
    expectOrigin(contiguous, 0, 4, "a", 0, 1, 1);
    contiguous.append(a.slice(5, 4));
    EXPECT_FALSE(contiguous.exactRange(0, 8));
    EXPECT_THROW(a.slice(10, 1), std::out_of_range);
}

TEST(SourceOrigin, UnicodeCrLfTabsAndMissingFinalNewline) {
    SourceFixture f;
    const auto file = f.write("input.jtml", "jtml 2\r\n\r\n\ttext \"é😀\"\r\ntext \"last\"");
    auto unit = jtml::cli::loadCompilationSource(file, jtml::SyntaxMode::Friendly);
    EXPECT_EQ(unit.source.text(), "\n\ttext \"é😀\"\ntext \"last\"\n");
    expectOrigin(unit.source, 1, 7, file, 10, 3, 1);
    // UTF-8 byte offset 19 is the emoji; one column for the preceding é.
    auto original = unit.source.exactRange(1, 8)->source;
    EXPECT_EQ(original->coordinates(19), std::make_pair(size_t(3), size_t(9)));
    EXPECT_FALSE(unit.source.exactRange(unit.source.size() - 1, unit.source.size()));
}

TEST(SourceOrigin, NamedImportsReexportsAndRepeatedSpellingsRetainFiles) {
    SourceFixture f;
    const auto a = f.write("a.jtml", "jtml 2\nexport make A\n  text \"same\"\nexport make Omitted\n  text \"no\"\n");
    const auto b = f.write("b.jtml", "jtml 2\nexport make B\n  text \"same\"\n");
    f.write("bridge.jtml", "jtml 2\nexport use A from \"./a.jtml\"\n");
    const auto root = f.write("index.jtml", "jtml 2\nuse A from \"./bridge.jtml\"\nuse B from \"./b.jtml\"\npage\n  A\n  B\n");
    auto unit = jtml::cli::loadCompilationSource(root, jtml::SyntaxMode::Friendly);
    EXPECT_EQ(unit.source.text(), "make A\n  text \"same\"\nmake B\n  text \"same\"\npage\n  A\n  B\n");
    expectOrigin(unit.source, 0, 6, a, 14, 2, 8);
    expectOrigin(unit.source, 7, 13, a, 21, 3, 1);
    expectOrigin(unit.source, 21, 6, b, 14, 2, 8);
    expectOrigin(unit.source, 28, 13, b, 21, 3, 1);
    EXPECT_FALSE(unit.source.exactRange(0, 27));
    EXPECT_EQ(unit.source.text().find("Omitted"), std::string::npos);
}

TEST(SourceOrigin, ClassicImportsRetainRangesAndFriendlyLoweringIsExplicitlySynthetic) {
    SourceFixture f;
    auto child = f.write("child.jtml", "define x = 1\\\\\r\n");
    auto root = f.write("root.jtml", "import \"./child.jtml\"\\\\\nshow x\\\\\n");
    auto unit = jtml::cli::loadCompilationSource(root, jtml::SyntaxMode::Classic);
    EXPECT_EQ(unit.source.text(), "define x = 1\\\\\nshow x\\\\\n");
    expectOrigin(unit.source, 0, 14, child, 0, 1, 1);
    f.write("friendly.jtml", "jtml 2\nlet x = 1\n");
    root = f.write("mixed.jtml", "import \"./friendly.jtml\"\\\\\nshow x\\\\\n");
    unit = jtml::cli::loadCompilationSource(root, jtml::SyntaxMode::Classic);
    ASSERT_FALSE(unit.source.mapping().empty());
    EXPECT_EQ(unit.source.mapping().front().source, nullptr);
    EXPECT_FALSE(unit.source.exactRange(0, 1));
}

TEST(SourceOrigin, BorrowedAliasOriginalLineSurvivesRemovedHeader) {
    const std::string file = std::filesystem::weakly_canonical(
        std::filesystem::path(JTML_SOURCE_DIR) / "tests/fixtures/production/taskboard-rejections/borrowed-alias.jtml").string();
    auto unit = jtml::cli::loadCompilationSource(file, jtml::SyntaxMode::Friendly);
    // Independent oracle: original write is 9:5. Loader removes the header,
    // so unannotated linked text sees 8:5, reproducing the historical boundary.
    const auto offset = unit.source.text().find("alias.inner.score =");
    ASSERT_NE(offset, std::string::npos);
    EXPECT_EQ(std::count(unit.source.text().begin(), unit.source.text().begin() + offset, '\n') + 1, 8);
    const auto origin = unit.source.exactRange(offset, offset + 17);
    ASSERT_TRUE(origin);
    EXPECT_EQ(origin->source->file, file);
    EXPECT_EQ(origin->source->coordinates(origin->begin), std::make_pair(size_t(9), size_t(5)));
}

TEST(SourceOrigin, SameNamesAndRepeatedImportsDoNotChangeOriginalIdentity) {
    SourceFixture f;
    auto left = f.write("left.jtml", "jtml 2\nmake Candidate\n  text \"same\"\n");
    auto right = f.write("right.jtml", "jtml 2\nmake Candidate\n  text \"same\"\n");
    auto root = f.write("root.jtml", "jtml 2\nuse \"./left.jtml\"\nuse \"./right.jtml\"\nuse \"./left.jtml\"\n");
    const auto unit = jtml::cli::loadCompilationSource(root, jtml::SyntaxMode::Friendly);
    EXPECT_EQ(unit.source.text(), "make Candidate\n  text \"same\"\nmake Candidate\n  text \"same\"\n");
    expectOrigin(unit.source, 0, 14, left, 7, 2, 1);
    expectOrigin(unit.source, 29, 14, right, 7, 2, 1);
    EXPECT_FALSE(unit.source.exactRange(0, unit.source.size()));
    // The loader preserves identity; duplicate definition legality belongs to
    // the existing semantic/lowering stage, not this mapping test.
}
