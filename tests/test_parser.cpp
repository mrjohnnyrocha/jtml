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
