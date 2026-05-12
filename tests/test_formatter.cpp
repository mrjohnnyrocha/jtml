// tests/test_formatter.cpp — the formatter's defining contract is
// idempotence: format(parse(format(parse(source)))) == format(parse(source)).
// If we can reach that fixed point in two applications on every example,
// the formatter is canonical.
#include "jtml/formatter.h"
#include "jtml/lexer.h"
#include "jtml/parser.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>

namespace {

std::string readFile(const std::string& path) {
    std::ifstream ifs(path);
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

std::string formatSource(const std::string& src) {
    Lexer lex(src);
    auto tokens = lex.tokenize();
    if (!lex.getErrors().empty()) return {};
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    if (!parser.getErrors().empty()) return {};
    jtml::JtmlFormatter fmt;
    return fmt.format(program);
}

TEST(Formatter, IdempotentOnTrivialProgram) {
    std::string src = "define x = 1\\\\\n";
    auto pass1 = formatSource(src);
    auto pass2 = formatSource(pass1);
    EXPECT_EQ(pass1, pass2) << "formatter must reach a fixed point in one pass";
}

TEST(Formatter, FormatsConditionalExpression) {
    std::string src = "define label = ok?\"yes\":\"no\"\\\\\n";
    auto formatted = formatSource(src);
    EXPECT_NE(formatted.find("define label = ok ? \"yes\" : \"no\"\\\\"), std::string::npos);
}

TEST(Formatter, IdempotentOnAllBundledExamples) {
    // Tests run from the build directory; examples live next to the source.
    namespace fs = std::filesystem;
    fs::path examples = fs::path(__FILE__).parent_path().parent_path() / "examples";
    ASSERT_TRUE(fs::exists(examples))
        << "examples/ not found at " << examples;

    int checked = 0;
    for (const auto& entry : fs::directory_iterator(examples)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".jtml") continue;

        std::string src = readFile(entry.path().string());
        std::string pass1 = formatSource(src);
        if (pass1.empty()) continue; // parse error -- not formatter's fault
        std::string pass2 = formatSource(pass1);

        EXPECT_EQ(pass1, pass2)
            << "formatter not idempotent on " << entry.path().filename();
        ++checked;
    }
    EXPECT_GT(checked, 0) << "no examples were exercised";
}

TEST(Formatter, ProducesReParsableOutput) {
    std::string src =
        "define x = 1\\\\\n"
        "derive y = x * 2\\\\\n"
        "function double(n)\\\\\n"
        "    return n * 2\\\\\n"
        "\\\\\n"
        "element main\\\\\n"
        "    show y\\\\\n"
        "#\n";

    std::string formatted = formatSource(src);
    ASSERT_FALSE(formatted.empty());

    // Formatter output must re-parse cleanly.
    Lexer lex(formatted);
    auto tokens = lex.tokenize();
    EXPECT_TRUE(lex.getErrors().empty());
    Parser parser(std::move(tokens));
    (void)parser.parseProgram();
    EXPECT_TRUE(parser.getErrors().empty())
        << "formatted output did not re-parse: \n" << formatted;
}

} // namespace
