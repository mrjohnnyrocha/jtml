// tests/test_parser.cpp — verifies the parser produces expected AST shapes
// for a handful of canonical inputs, and that malformed inputs surface via
// `getErrors()` without throwing.
#include "jtml/lexer.h"
#include "jtml/parser.h"
#include <gtest/gtest.h>

namespace {

std::vector<std::unique_ptr<ASTNode>> parseOk(const std::string& src) {
    Lexer lex(src);
    auto tokens = lex.tokenize();
    EXPECT_TRUE(lex.getErrors().empty()) << "unexpected lex errors";
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    EXPECT_TRUE(parser.getErrors().empty()) << "unexpected parse errors";
    return program;
}

TEST(Parser, DefineStatementProducesDefineNode) {
    auto prog = parseOk("define count = 0\\\\\n");
    ASSERT_EQ(prog.size(), 1u);
    EXPECT_EQ(prog[0]->getType(), ASTNodeType::DefineStatement);
}

TEST(Parser, DefineStatementKeepsOptionalTypeAnnotation) {
    auto prog = parseOk("define count: number = 1\\\\\n");
    ASSERT_EQ(prog.size(), 1u);
    const auto& def = static_cast<const DefineStatementNode&>(*prog[0]);
    EXPECT_EQ(def.identifier, "count");
    EXPECT_EQ(def.declaredType, "number");
}

TEST(Parser, ConditionalExpressionParses) {
    auto prog = parseOk("define label = ok ? \"yes\" : \"no\"\\\\\n");
    ASSERT_EQ(prog.size(), 1u);
    ASSERT_EQ(prog[0]->getType(), ASTNodeType::DefineStatement);
    const auto& def = static_cast<const DefineStatementNode&>(*prog[0]);
    ASSERT_TRUE(def.expression);
    EXPECT_EQ(def.expression->getExprType(), ExpressionStatementNodeType::Conditional);
    EXPECT_EQ(def.expression->toString(), "(ok ? yes : no)");
}

TEST(Parser, StandaloneExpressionParsesAndRejectsTrailingTokens) {
    Lexer lex("users.data[index].name");
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << "unexpected lex errors";
    Parser parser(std::move(tokens));
    auto expression = parser.parseStandaloneExpression();
    ASSERT_TRUE(expression);
    EXPECT_TRUE(parser.getErrors().empty()) << "unexpected parse errors";
    EXPECT_EQ(expression->getExprType(), ExpressionStatementNodeType::ObjectPropertyAccess);

    Lexer badLex("users.data extra");
    auto badTokens = badLex.tokenize();
    ASSERT_TRUE(badLex.getErrors().empty()) << "unexpected lex errors";
    Parser badParser(std::move(badTokens));
    auto badExpression = badParser.parseStandaloneExpression();
    EXPECT_FALSE(badExpression);
    EXPECT_FALSE(badParser.getErrors().empty());
}

TEST(Parser, StandaloneRangeExpressionParses) {
    Lexer lex("start..end");
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << "unexpected lex errors";
    Parser parser(std::move(tokens));
    auto expression = parser.parseStandaloneExpression();
    ASSERT_TRUE(expression);
    EXPECT_TRUE(parser.getErrors().empty()) << "unexpected parse errors";
    ASSERT_EQ(expression->getExprType(), ExpressionStatementNodeType::Range);
    const auto& range = static_cast<const RangeExpressionStatementNode&>(*expression);
    ASSERT_TRUE(range.start);
    ASSERT_TRUE(range.end);
    EXPECT_EQ(range.start->getExprType(), ExpressionStatementNodeType::Variable);
    EXPECT_EQ(range.end->getExprType(), ExpressionStatementNodeType::Variable);
    EXPECT_EQ(expression->sourceSpan.startColumn, 1);
    EXPECT_GT(expression->sourceSpan.endColumn, expression->sourceSpan.startColumn);
}

TEST(Parser, CompoundAssignmentParsesAsAssignment) {
    auto prog = parseOk(
        "define count = 1\\\\\n"
        "count += 2\\\\\n");
    ASSERT_EQ(prog.size(), 2u);
    ASSERT_EQ(prog[1]->getType(), ASTNodeType::AssignmentStatement);
    const auto& assign = static_cast<const AssignmentStatementNode&>(*prog[1]);
    ASSERT_TRUE(assign.rhs);
    EXPECT_EQ(assign.rhs->getExprType(), ExpressionStatementNodeType::Binary);
    EXPECT_EQ(assign.rhs->toString(), "(count + 2.000000000000000)");
}

TEST(Parser, StatementsCarrySourceSpans) {
    auto prog = parseOk(
        "define count = 0\\\\\n"
        "element main\\\\\n"
        "    show count\\\\\n"
        "#\n");
    ASSERT_EQ(prog.size(), 2u);
    EXPECT_TRUE(prog[0]->sourceSpan.valid());
    EXPECT_EQ(prog[0]->sourceSpan.startLine, 1);
    EXPECT_EQ(prog[0]->sourceSpan.startColumn, 1);
    EXPECT_GE(prog[0]->sourceSpan.endColumn, prog[0]->sourceSpan.startColumn);

    ASSERT_EQ(prog[1]->getType(), ASTNodeType::JtmlElement);
    const auto& el = static_cast<const JtmlElementNode&>(*prog[1]);
    EXPECT_TRUE(el.sourceSpan.valid());
    EXPECT_EQ(el.sourceSpan.startLine, 2);
    EXPECT_EQ(el.sourceSpan.startColumn, 1);
    ASSERT_EQ(el.content.size(), 1u);
    EXPECT_TRUE(el.content[0]->sourceSpan.valid());
    EXPECT_EQ(el.content[0]->sourceSpan.startLine, 3);
    EXPECT_GT(el.content[0]->sourceSpan.startColumn, 1);
}

TEST(Parser, ExpressionsCarrySourceSpans) {
    auto prog = parseOk(
        "define count = 0\\\\\n"
        "define label = count > 1 ? \"many\" : \"one\"\\\\\n"
        "element main\\\\\n"
        "    show label\\\\\n"
        "#\n");
    ASSERT_EQ(prog.size(), 3u);

    ASSERT_EQ(prog[1]->getType(), ASTNodeType::DefineStatement);
    const auto& def = static_cast<const DefineStatementNode&>(*prog[1]);
    ASSERT_TRUE(def.expression);
    EXPECT_TRUE(def.expression->sourceSpan.valid());
    EXPECT_EQ(def.expression->sourceSpan.startLine, 2);
    EXPECT_GT(def.expression->sourceSpan.startColumn, 1);
    EXPECT_EQ(def.expression->sourceSpan.endLine, 2);
    EXPECT_GT(def.expression->sourceSpan.endColumn,
              def.expression->sourceSpan.startColumn);

    ASSERT_EQ(prog[2]->getType(), ASTNodeType::JtmlElement);
    const auto& el = static_cast<const JtmlElementNode&>(*prog[2]);
    ASSERT_EQ(el.content.size(), 1u);
    ASSERT_EQ(el.content[0]->getType(), ASTNodeType::ShowStatement);
    const auto& show = static_cast<const ShowStatementNode&>(*el.content[0]);
    ASSERT_TRUE(show.expr);
    EXPECT_TRUE(show.expr->sourceSpan.valid());
    EXPECT_EQ(show.expr->sourceSpan.startLine, 4);
    EXPECT_GT(show.expr->sourceSpan.startColumn, 1);
}

TEST(Parser, SourceSpanColumnsCountUtf8CodepointsNotBytes) {
    auto prog = parseOk(
        "show \"Olá\" + label\\\\\n");
    ASSERT_EQ(prog.size(), 1u);
    ASSERT_EQ(prog[0]->getType(), ASTNodeType::ShowStatement);
    const auto& show = static_cast<const ShowStatementNode&>(*prog[0]);
    ASSERT_TRUE(show.expr);
    EXPECT_TRUE(show.expr->sourceSpan.valid());
    EXPECT_EQ(show.expr->sourceSpan.startLine, 1);
    EXPECT_EQ(show.expr->sourceSpan.startColumn, 6);
    EXPECT_EQ(show.expr->sourceSpan.endLine, 1);
    EXPECT_EQ(show.expr->sourceSpan.endColumn, 19);
}

TEST(Parser, ElementWithChildrenParses) {
    auto prog = parseOk(
        "element main\\\\\n"
        "    show \"hi\"\\\\\n"
        "#\n");
    ASSERT_EQ(prog.size(), 1u);
    ASSERT_EQ(prog[0]->getType(), ASTNodeType::JtmlElement);
    const auto& el = static_cast<const JtmlElementNode&>(*prog[0]);
    EXPECT_EQ(el.tagName, "main");
    EXPECT_EQ(el.content.size(), 1u);
    EXPECT_EQ(el.content[0]->getType(), ASTNodeType::ShowStatement);
}

TEST(Parser, FunctionDeclarationParses) {
    auto prog = parseOk(
        "define count = 0\\\\\n"
        "function inc()\\\\\n"
        "    count = count + 1\\\\\n"
        "\\\\\n");
    ASSERT_EQ(prog.size(), 2u);
    EXPECT_EQ(prog[1]->getType(), ASTNodeType::FunctionDeclaration);
    const auto& fn = static_cast<const FunctionDeclarationNode&>(*prog[1]);
    EXPECT_EQ(fn.name, "inc");
}

TEST(Parser, UnterminatedElementReportsError) {
    // Missing `#` close marker.
    Lexer lex("element main\\\\\n    show \"hi\"\\\\\n");
    auto tokens = lex.tokenize();
    Parser parser(std::move(tokens));
    (void)parser.parseProgram();
    EXPECT_FALSE(parser.getErrors().empty());
}

} // namespace
