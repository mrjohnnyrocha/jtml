// tests/test_lexer.cpp — exercises the lexer's public contract: every source
// that lexes cleanly should produce non-empty tokens and zero errors; every
// malformed source should surface at least one error via `getErrors()`.
#include "jtml/lexer.h"
#include "jtml/expression_source.h"
#include <gtest/gtest.h>

namespace {

std::vector<Token> tokenize(const std::string& src) {
    Lexer lex(src);
    auto tokens = lex.tokenize();
    EXPECT_TRUE(lex.getErrors().empty()) << "Unexpected lex errors";
    return tokens;
}

TEST(Lexer, EmitsTokensForTrivialProgram) {
    auto tokens = tokenize("define count = 0\\\\\n");
    ASSERT_FALSE(tokens.empty());
    // First token should be `define`.
    EXPECT_EQ(tokens.front().type, TokenType::DEFINE);
}

TEST(Lexer, StringLiteralIsSingleToken) {
    auto tokens = tokenize("show \"Hello, world\"\\\\\n");
    bool sawString = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::STRING_LITERAL) {
            EXPECT_EQ(t.text, "Hello, world");
            sawString = true;
        }
    }
    EXPECT_TRUE(sawString);
}

TEST(Lexer, TerminatorIsRecognised) {
    // `\\` is the JTML statement terminator.
    auto tokens = tokenize("define x = 1\\\\\n");
    bool sawTerminator = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::STMT_TERMINATOR) { sawTerminator = true; break; }
    }
    EXPECT_TRUE(sawTerminator);
}

TEST(Lexer, ConditionalQuestionMarkIsRecognised) {
    auto tokens = tokenize("define label = ok ? \"yes\" : \"no\"\\\\\n");
    bool sawQuestion = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::QUESTION) { sawQuestion = true; break; }
    }
    EXPECT_TRUE(sawQuestion);
}

TEST(Lexer, UnterminatedStringIsReported) {
    Lexer lex("show \"hello\n");
    (void)lex.tokenize();
    EXPECT_FALSE(lex.getErrors().empty());
}

} // namespace

// Source serialization is a round trip, not a second interpretation of the
// contents. Exercise every byte needing JSON escaping plus raw UTF-8.
TEST(Lexer, StringEscapesRoundTripWithoutLosingLiteralBackslashes) {
    const std::vector<std::string> values = {
        "Tom & Jerry <tag>", "\"quotes\" and 'apostrophes'", "C:\\tmp\\new",
        "line\nnext\r\ttab\b\f", "literal \\n \\u0026", "café 雪 😀",
        std::string("\0\x01\x1f", 3), "\\", "", "</script><img onerror=\"x\">"
    };
    for (const auto& value : values) {
        const auto source = jtml::jsonString(value);
        EXPECT_EQ(jtml::decodeStringLiteral(source), value);
        auto tokens = tokenize(source);
        ASSERT_EQ(tokens.front().type, TokenType::STRING_LITERAL);
        EXPECT_EQ(tokens.front().text, value);
        EXPECT_EQ(jtml::decodeStringLiteral(jtml::jsonString(tokens.front().text)), value);
    }
    EXPECT_EQ(jtml::decodeStringLiteral(R"("\u0026\u003c\u003e\uD83D\uDE00")"), "&<>😀");
    EXPECT_EQ(jtml::decodeStringLiteral(R"('single \' quote and "double"')"), "single ' quote and \"double\"");
    EXPECT_EQ(jtml::decodeStringLiteral("\"raw\n\twhitespace\""), "raw\n\twhitespace");
}

TEST(Lexer, MalformedStringEscapesFailWithSourceLocation) {
    for (const auto& source : {R"("\q")", R"("\u12xz")", R"("\uD800")", R"("\uDC00")", R"("dangling\")", R"("unterminated)"}) {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        ASSERT_FALSE(lexer.getErrors().empty()) << source;
        EXPECT_EQ(tokens.front().type, TokenType::ERROR);
        EXPECT_EQ(tokens.front().line, 1);
        EXPECT_EQ(tokens.front().column, 1);
    }
}
