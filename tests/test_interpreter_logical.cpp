#include "jtml/expression_ir.h"
#include "jtml/friendly.h"
#include "jtml/interpreter.h"
#include "jtml/lexer.h"
#include "jtml/parser.h"
#include <gtest/gtest.h>
#include <iostream>
#include <set>
#include <sstream>

namespace {
class NativeLogical : public ::testing::Test {
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
    void run(const std::string& source) {
        Lexer lexer(jtml::friendlyToClassic("jtml 2\n" + source));
        auto tokens = lexer.tokenize();
        ASSERT_TRUE(lexer.getErrors().empty());
        Parser parser(std::move(tokens));
        auto program = parser.parseProgram();
        ASSERT_TRUE(parser.getErrors().empty());
        interpreter.interpret(program);
    }
    auto expression(const std::string& source) {
        Lexer lexer(source);
        Parser parser(lexer.tokenize());
        auto ast = parser.parseStandaloneExpression();
        if (!lexer.getErrors().empty() || !parser.getErrors().empty() || !ast)
            throw std::runtime_error("invalid logical test expression: " + source);
        return ast;
    }
    auto eval(const std::string& source) {
        auto ast = expression(source);
        return interpreter.evaluateExpression(ast.get(), interpreter.getCurrentEnvironment());
    }
    auto value(const std::string& name) {
        auto env = interpreter.getCurrentEnvironment();
        return env->getVariable({env->instanceID, name});
    }
    void boolean(const std::string& source, bool expected) {
        const auto result = eval(source);
        ASSERT_TRUE(result && result->isBool()) << source;
        EXPECT_EQ(result->getBool(), expected) << source;
        EXPECT_EQ(result->toString(), expected ? "true" : "false") << source;
    }
};

TEST_F(NativeLogical, ConstructorOverloadControl) {
    // Both non-null pointers select bool, even the one spelling "false".
    // Logical evaluation must pass the actual bool, not a C string pointer.
    const JTML::VarValue pointer("false"), text(std::string("false")), logical(false);
    ASSERT_TRUE(pointer.isBool()); EXPECT_TRUE(pointer.getBool());
    ASSERT_TRUE(text.isString()); EXPECT_EQ(text.toString(), "false");
    ASSERT_TRUE(logical.isBool()); EXPECT_FALSE(logical.getBool());
}

TEST_F(NativeLogical, BooleanTruthTablesAndType) {
    boolean("!true", false); boolean("!false", true);
    boolean("!!true", true); boolean("!!false", false);
    for (const bool left : {false, true}) for (const bool right : {false, true}) {
        const std::string a = left ? "true" : "false", b = right ? "true" : "false";
        boolean(a + " && " + b, left && right);
        boolean(a + " || " + b, left || right);
    }
    boolean("(2 < 1) || ((3 >= 2) && !(4 == 4))", false);
    boolean("(2 > 1) && ((3 < 2) ? false : !false)", true);
}

TEST_F(NativeLogical, ExistingNativeTruthinessIsPreserved) {
    // Native compatibility policy, not a claim of JS/nonboolean coercion parity.
    for (const auto& source : {"0", "\"\"", "\"false\"", "\"0\""}) boolean("!(" + std::string(source) + ")", true);
    for (const auto& source : {"2", "\"true\"", "\"hello\""}) boolean("!(" + std::string(source) + ")", false);
    boolean("0 && 2", false); boolean("2 || 0", true);
}

TEST_F(NativeLogical, InactiveErrorsAreSkippedAndActiveErrorsStillThrow) {
    boolean("false && (1 / 0 > 0)", false);
    boolean("true || (1 / 0 > 0)", true);
    boolean("false && missing", false);
    boolean("true || missing", true);
    EXPECT_THROW(eval("true && (1 / 0 > 0)"), std::runtime_error);
    EXPECT_THROW(eval("false || (1 / 0 > 0)"), std::runtime_error);
    EXPECT_THROW(eval("(1 / 0 > 0) && false"), std::runtime_error);
    EXPECT_THROW(eval("(1 / 0 > 0) || true"), std::runtime_error);
}

TEST_F(NativeLogical, SideEffectsAreLazyOrderedAndExactlyOnce) {
    run("let trace = \"\"\n"
        "when yes\n  trace = trace + \"T\"\n  return 1 == 1\n"
        "when no\n  trace = trace + \"F\"\n  return 1 == 0\n");
    boolean("no() && yes()", false); EXPECT_EQ(value("trace")->toString(), "F");
    boolean("yes() || no()", true); EXPECT_EQ(value("trace")->toString(), "FT");
    boolean("yes() && no()", false); EXPECT_EQ(value("trace")->toString(), "FTTF");
    boolean("no() || yes()", true); EXPECT_EQ(value("trace")->toString(), "FTTFFT");
    boolean("no() && (yes() || no())", false); EXPECT_EQ(value("trace")->toString(), "FTTFFTF");
}

TEST_F(NativeLogical, DependencyCollectionRemainsConservativeWithoutExecution) {
    run("let enabled = false\nlet x = 1\nlet calls = 0\n"
        "when bump\n  calls += 1\n  return true\n"
        "get result = enabled && (x > 2)\nget guarded = enabled && bump()\n");
    EXPECT_EQ(value("calls")->getNumber(), 0);
    auto env = interpreter.getCurrentEnvironment();
    const auto info = env->variables.at({env->instanceID, "result"});
    std::set<std::string> reads;
    // Quiescent borrowed state: no workers/adapters run in this fixture.
    for (const auto& key : info->dependencies) reads.insert(key.varName);
    EXPECT_EQ(reads, (std::set<std::string>{"enabled", "x"}));
    auto ir = jtml::buildExpressionIr(expression("enabled && (x > 2)").get());
    ASSERT_TRUE(ir); EXPECT_EQ(ir->valueType, jtml::ExpressionValueType::Boolean);
    const auto irReads = jtml::expressionIrDependencies(*ir);
    EXPECT_EQ((std::set<std::string>(irReads.begin(), irReads.end())), reads);
    run("x = 3\n"); EXPECT_FALSE(value("result")->getBool()); EXPECT_EQ(value("calls")->getNumber(), 0);
    run("enabled = true\n"); EXPECT_TRUE(value("result")->getBool()); EXPECT_EQ(value("calls")->getNumber(), 1);
    run("enabled = false\nx = 1\n"); EXPECT_FALSE(value("result")->getBool()); EXPECT_EQ(value("calls")->getNumber(), 1);
}

TEST_F(NativeLogical, InactiveComputedIndexIsCollectedWithoutEvaluation) {
    run("let enabled = false\nlet index = 0\nlet items = [7]\n"
        "get result = enabled && (items[index / 0] > 0)\n");
    ASSERT_TRUE(value("result")->isBool()); EXPECT_FALSE(value("result")->getBool());
    run("index = 1\n"); EXPECT_FALSE(value("result")->getBool());
}
} // namespace
