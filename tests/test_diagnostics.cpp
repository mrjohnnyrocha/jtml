#include "jtml/diagnostic.h"
#include "jtml/friendly.h"
#include "jtml/lexer.h"
#include "jtml/linter.h"
#include "jtml/parser.h"

#include <gtest/gtest.h>

TEST(Diagnostics, ExtractsLineAndColumnFromMessages) {
    auto diagnostic = jtml::diagnosticFromMessage(
        "Parser Error: Unexpected token at line 7, column 12");

    EXPECT_EQ(diagnostic.severity, jtml::DiagnosticSeverity::Error);
    EXPECT_EQ(diagnostic.code, "JTML_PARSE");
    EXPECT_EQ(diagnostic.line, 7);
    EXPECT_EQ(diagnostic.column, 12);
    EXPECT_FALSE(diagnostic.hint.empty());
}

TEST(Diagnostics, ClassifiesEventActionRepairs) {
    auto diagnostic = jtml::diagnosticFromMessage(
        "Expected action after event 'click' at line 3");

    EXPECT_EQ(diagnostic.code, "JTML_EVENT_ACTION");
    EXPECT_EQ(diagnostic.line, 3);
    EXPECT_NE(diagnostic.hint.find("`when`"), std::string::npos);
    EXPECT_NE(diagnostic.example.find("button"), std::string::npos);
}

TEST(Diagnostics, SplitsErrorBlocksIntoStructuredDiagnostics) {
    auto diagnostics = jtml::diagnosticsFromMessageBlock(
        "Lexer Error: unterminated string at line 2\n"
        "Parser Error: Expected element body at line 4, column 5\n");

    ASSERT_EQ(diagnostics.size(), 2u);
    EXPECT_EQ(diagnostics[0].code, "JTML_LEXER");
    EXPECT_EQ(diagnostics[0].line, 2);
    EXPECT_EQ(diagnostics[1].code, "JTML_PARSE");
    EXPECT_EQ(diagnostics[1].line, 4);
    EXPECT_EQ(diagnostics[1].column, 5);
}

TEST(Diagnostics, LinterDiagnosticsCarryRepairMetadata) {
    LintDiagnostic diagnostic{
        LintDiagnostic::Severity::Error,
        "undefined variable: userName",
    };

    auto enriched = jtml::diagnosticFromMessage(
        diagnostic.message,
        jtml::DiagnosticSeverity::Error);

    EXPECT_EQ(enriched.code, "JTML_UNDEFINED_NAME");
    EXPECT_FALSE(enriched.hint.empty());
    EXPECT_FALSE(enriched.example.empty());
}

TEST(Diagnostics, ParserAddsLocationToExpressionErrors) {
    Lexer lex("define value = @\\\\\n");
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty());
    Parser parser(std::move(tokens));
    (void)parser.parseProgram();
    ASSERT_FALSE(parser.getErrors().empty());

    auto diagnostic = jtml::diagnosticFromMessage(parser.getErrors().front());
    EXPECT_EQ(diagnostic.code, "JTML_PARSE");
    EXPECT_EQ(diagnostic.line, 1);
    EXPECT_GT(diagnostic.column, 0);
}

TEST(Diagnostics, FriendlySourceMapPointsClassicLinesBackToFriendlyLines) {
    const auto lowered = jtml::friendlyToClassicWithSourceMap(
        "jtml 2\n"
        "\n"
        "// comment\n"
        "\n"
        "page\n"
        "  show @\n");

    ASSERT_NE(lowered.classicSource.find("show @\\\\"), std::string::npos);

    Lexer lex(lowered.classicSource);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty());
    Parser parser(std::move(tokens));
    (void)parser.parseProgram();
    ASSERT_FALSE(parser.getErrors().empty());

    auto diagnostic = jtml::diagnosticFromMessage(parser.getErrors().front());
    ASSERT_GT(diagnostic.line, 0);
    ASSERT_LT(static_cast<size_t>(diagnostic.line), lowered.classicLineToFriendlyLine.size());
    EXPECT_EQ(lowered.classicLineToFriendlyLine[static_cast<size_t>(diagnostic.line)], 6);

    auto diagnostics = jtml::diagnosticsFromMessageBlock(parser.getErrors().front());
    jtml::remapDiagnosticLines(diagnostics, lowered.classicLineToFriendlyLine);
    ASSERT_FALSE(diagnostics.empty());
    EXPECT_EQ(diagnostics.front().line, 6);
    EXPECT_NE(diagnostics.front().message.find("line 6"), std::string::npos);
}
