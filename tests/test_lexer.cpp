// tests/test_lexer.cpp — exercises the lexer's public contract: every source
// that lexes cleanly should produce non-empty tokens and zero errors; every
// malformed source should surface at least one error via `getErrors()`.
#include "jtml/lexer.h"
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
