#include "jtml/expression_ir.h"
#include "jtml/friendly.h"
#include "jtml/interpreter.h"
#include "jtml/lexer.h"
#include "jtml/parser.h"
#include <gtest/gtest.h>
#include <cmath>
#include <iostream>
#include <sstream>

namespace {
class NativeUnaryNumeric : public ::testing::Test {
protected:
    JtmlTranspiler transpiler;
    Interpreter interpreter{transpiler, InterpreterConfig{0, false}};
    std::ostringstream errors;
    std::streambuf* previous = nullptr;
    void SetUp() override { previous = std::cerr.rdbuf(errors.rdbuf()); }
    void TearDown() override {
        std::cerr.rdbuf(previous);
        EXPECT_TRUE(errors.str().empty()) << errors.str();
    }
    auto expression(const std::string& source) {
        Lexer lexer(source);
        Parser parser(lexer.tokenize());
        auto ast = parser.parseStandaloneExpression();
        if (!lexer.getErrors().empty() || !parser.getErrors().empty() || !ast)
            throw std::runtime_error("invalid unary test expression: " + source);
        return ast;
    }
    auto eval(const std::string& source) {
        auto ast = expression(source);
        return interpreter.evaluateExpression(ast.get(), interpreter.getCurrentEnvironment());
    }
    void number(const std::string& source, double expected) {
        const auto actual = eval(source);
        ASSERT_TRUE(actual && actual->isNumber()) << source;
        EXPECT_DOUBLE_EQ(actual->getNumber(), expected) << source;
    }
    void run(const std::string& source) {
        Lexer lexer(jtml::friendlyToClassic("jtml 2\n" + source));
        auto tokens = lexer.tokenize();
        ASSERT_TRUE(lexer.getErrors().empty());
        Parser parser(std::move(tokens));
        auto program = parser.parseProgram();
        ASSERT_TRUE(parser.getErrors().empty());
        interpreter.interpret(program);
    }
    auto value(const std::string& name) {
        auto env = interpreter.getCurrentEnvironment();
        return env->getVariable({env->instanceID, name});
    }
};

TEST_F(NativeUnaryNumeric, DirectRepeatedGroupedAndNestedArithmetic) {
    number("-1", -1); number("-(-1)", 1); number("-(-(-1))", -1);
    number("-(2 + 3)", -5); number("(-2 + 3) * -(4 - 1)", -3);
    number("-1 + 1", 0); number("-0.125", -0.125); number("-(-1.25) / 2", 0.625);
}

TEST_F(NativeUnaryNumeric, PositiveAndNegativeZeroRetainNumericSign) {
    const auto negative = eval("-0"), positive = eval("-(-0)");
    ASSERT_TRUE(negative->isNumber()); ASSERT_TRUE(positive->isNumber());
    EXPECT_EQ(negative->getNumber(), 0.0); EXPECT_TRUE(std::signbit(negative->getNumber()));
    EXPECT_EQ(positive->getNumber(), 0.0); EXPECT_FALSE(std::signbit(positive->getNumber()));
    number("-0 + 0", 0); number("-(-0) + 0", 0);
}

TEST_F(NativeUnaryNumeric, NumericOperandsNeverRoundTripThroughDisplayText) {
    run("let input = 0\n");
    auto env = interpreter.getCurrentEnvironment();
    // Store exact doubles independently of source parsing/formatting.
    for (double operand : {1.2345678901234567, 0.000000123456789, 123456789012345.0}) {
        env->setVariable({env->instanceID, "input"}, std::make_shared<JTML::VarValue>(operand));
        number("-input", -operand); number("-(-input)", operand);
    }
}

TEST_F(NativeUnaryNumeric, DerivedUpdatesAndTernaryBranchesRemainNumeric) {
    run("let x = 1.25\nlet enabled = true\nget inverted = -x\n"
        "get result = enabled ? inverted + 1 : -(-x)\n");
    ASSERT_TRUE(value("inverted")->isNumber()); EXPECT_DOUBLE_EQ(value("inverted")->getNumber(), -1.25);
    ASSERT_TRUE(value("result")->isNumber()); EXPECT_DOUBLE_EQ(value("result")->getNumber(), -0.25);
    run("x = 2.5\n"); EXPECT_DOUBLE_EQ(value("result")->getNumber(), -1.5);
    run("enabled = !enabled\n"); EXPECT_DOUBLE_EQ(value("result")->getNumber(), 2.5);
    run("x = -0.5\n"); EXPECT_DOUBLE_EQ(value("result")->getNumber(), -0.5);
    run("enabled = !enabled\n"); EXPECT_DOUBLE_EQ(value("result")->getNumber(), 1.5);
}

TEST_F(NativeUnaryNumeric, AstAndIrTypeBoundariesAreExplicit) {
    auto ast = expression("-1.25");
    ASSERT_EQ(ast->getExprType(), ExpressionStatementNodeType::Unary);
    const auto& unary = static_cast<const UnaryExpressionStatementNode&>(*ast);
    EXPECT_EQ(unary.op, "-");
    ASSERT_EQ(unary.right->getExprType(), ExpressionStatementNodeType::NumberLiteral);
    auto operand = jtml::buildExpressionIr(unary.right.get());
    ASSERT_TRUE(operand); EXPECT_EQ(operand->valueType, jtml::ExpressionValueType::Number);
    auto ir = jtml::buildExpressionIr(ast.get());
    ASSERT_TRUE(ir);
    // Existing IR does not type unary minus. Do not pretend Unknown means String,
    // or add compiler support to a native evaluator repair.
    EXPECT_EQ(ir->kind, jtml::ExpressionIrKind::Source);
    EXPECT_EQ(ir->valueType, jtml::ExpressionValueType::Unknown);
    auto control = jtml::buildExpressionIr(expression("0 - 1.25").get());
    ASSERT_TRUE(control); EXPECT_EQ(control->valueType, jtml::ExpressionValueType::Number);
    number("-1.25", -1.25); number("0 - 1.25", -1.25);
}

TEST_F(NativeUnaryNumeric, InvalidOperandDiagnosticsRemainExplicit) {
    for (const std::string source : {"-true", "-\"\"", "-\"invalid\"", "-[]", "-{}"}) {
        errors.str(""); errors.clear();
        try { eval(source); FAIL() << "expected rejection of " << source; }
        catch (const std::runtime_error& error) { EXPECT_STREQ(error.what(), "Invalid numeric format in unary '-'"); }
        EXPECT_EQ(errors.str(), "[Warning] Using unary '-' on a non-numeric value.\n") << source;
    }
    errors.str(""); errors.clear();
}

TEST_F(NativeUnaryNumeric, ExistingNumericStringConversionRetainsWarning) {
    number("-\"1.25\"", -1.25);
    EXPECT_EQ(errors.str(), "[Warning] Using unary '-' on a non-numeric value.\n");
    errors.str(""); errors.clear();
}
} // namespace
