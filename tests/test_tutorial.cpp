// tests/test_tutorial.cpp — pins the Studio's lesson set to the same gate the
// Studio applies to user code: every `tutorial/*/code.jtml` must parse, lint
// without errors, and round-trip through `jtml fmt` idempotently. The first
// (default) lesson must also be the JTML-authored language home.
#include "jtml/formatter.h"
#include "jtml/friendly.h"
#include "jtml/friendly_formatter.h"
#include "jtml/lexer.h"
#include "jtml/linter.h"
#include "jtml/parser.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>

namespace {

std::string readFile(const std::filesystem::path& path) {
    std::ifstream ifs(path);
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

std::filesystem::path tutorialDir() {
    // Tests run from the build directory; the repo lives two levels up.
    return std::filesystem::path(__FILE__).parent_path().parent_path() / "tutorial";
}

std::string formatRoundTrip(const std::string& source) {
    const std::string normalized = jtml::normalizeSourceSyntax(source);
    Lexer lex(normalized);
    auto tokens = lex.tokenize();
    if (!lex.getErrors().empty()) return {};
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    if (!parser.getErrors().empty()) return {};
    if (jtml::isFriendlySyntax(source))
        return jtml::formatFriendlySource(source);
    jtml::JtmlFormatter fmt;
    return fmt.format(program);
}

int countLintErrors(const std::string& source) {
    const std::string normalized = jtml::normalizeSourceSyntax(source);
    Lexer lex(normalized);
    auto tokens = lex.tokenize();
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    jtml::JtmlLinter linter;
    int errors = 0;
    for (const auto& d : linter.lint(program))
        if (d.severity == LintDiagnostic::Severity::Error) ++errors;
    return errors;
}

TEST(Tutorial, FirstLessonIsJtmlAuthoredLanguageHome) {
    namespace fs = std::filesystem;
    fs::path dir = tutorialDir();
    ASSERT_TRUE(fs::exists(dir)) << "tutorial/ not found at " << dir;

    std::vector<fs::path> entries;
    for (const auto& e : fs::directory_iterator(dir))
        if (e.is_directory()) entries.push_back(e.path());
    std::sort(entries.begin(), entries.end());
    ASSERT_FALSE(entries.empty());

    // The Studio sorts directories lexicographically; `00-welcome` must be
    // the default landing page so JTML dogfoods itself as a content platform.
    EXPECT_EQ(entries.front().filename().string(), "00-welcome")
        << "first lesson must be the JTML-authored welcome lesson";

    const std::string code = readFile(entries.front() / "code.jtml");
    EXPECT_TRUE(jtml::isFriendlySyntax(code))
        << "the language home lesson must be Friendly JTML";
}

TEST(Tutorial, EveryLessonParsesAndLintsAndFormatsIdempotently) {
    namespace fs = std::filesystem;
    int checked = 0;
    for (const auto& entry : fs::directory_iterator(tutorialDir())) {
        if (!entry.is_directory()) continue;
        fs::path codePath = entry.path() / "code.jtml";
        if (!fs::exists(codePath)) continue;
        const std::string slug = entry.path().filename().string();
        const std::string source = readFile(codePath);

        EXPECT_EQ(countLintErrors(source), 0)
            << slug << ": lint must report zero errors";

        const std::string pass1 = formatRoundTrip(source);
        ASSERT_FALSE(pass1.empty()) << slug << ": formatter rejected lesson";
        const std::string pass2 = formatRoundTrip(pass1);
        EXPECT_EQ(pass1, pass2)
            << slug << ": formatter must reach a fixed point";
        ++checked;
    }
    EXPECT_GT(checked, 0) << "no lessons were exercised";
}

} // namespace
