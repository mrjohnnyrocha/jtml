#include "jtml/expression_ir.h"
#include "jtml/friendly.h"
#include "jtml/interpreter.h"
#include "jtml/lexer.h"
#include "jtml/parser.h"
#include "jtml/semantic.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <iostream>
#include <set>
#include <sstream>

namespace {
auto parse(const std::string& source) {
    Lexer lexer(jtml::friendlyToClassic("jtml 2\n" + source));
    auto tokens = lexer.tokenize();
    if (!lexer.getErrors().empty()) throw std::runtime_error("test source lexer failure");
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    if (!parser.getErrors().empty()) throw std::runtime_error("test source parser failure");
    return program;
}

class NativeTernaryDependencies : public ::testing::Test {
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
    void run(const std::string& source) { interpreter.interpret(parse(source)); }
    auto value(const std::string& name) {
        auto env = interpreter.getCurrentEnvironment();
        return env->getVariable({env->instanceID, name});
    }
    void number(const std::string& name, double expected) {
        auto current = value(name);
        ASSERT_TRUE(current && current->isNumber()) << name;
        EXPECT_DOUBLE_EQ(current->getNumber(), expected) << name;
    }
    std::set<std::string> dependencies(const std::string& name) {
        // Borrowed environment inspection is confined to quiescent synchronous
        // tests: no async workers, adapters or concurrent callbacks exist here.
        const auto env = interpreter.getCurrentEnvironment();
        const auto info = env->variables.at({env->instanceID, name});
        std::set<std::string> names;
        for (const auto& key : info->dependencies) {
            EXPECT_EQ(key.instanceID, env->instanceID);
            names.insert(key.varName);
        }
        return names;
    }
};

TEST_F(NativeTernaryDependencies, MinimalAndNonconditionalControl) {
    run("let x = 1\nget result = true ? x : 0\nget control = (x) + 0\n");
    number("result", 1); number("control", 1);
    run("x = 2\n");
    number("result", 2); number("control", 2);
    EXPECT_EQ(dependencies("result"), (std::set<std::string>{"x"}));
}

TEST_F(NativeTernaryDependencies, BothBranchesAndConditionRemainSubscribed) {
    run("let enabled = true\nlet left = 2\nlet right = 9\nget result = enabled ? left : right\n");
    EXPECT_EQ(dependencies("result"), (std::set<std::string>{"enabled", "left", "right"}));
    run("right = 11\n"); number("result", 2);
    run("left = 5\n"); number("result", 5);
    run("enabled = false\n"); number("result", 11);
    run("left = 8\n"); number("result", 11);
    run("right = 13\n"); number("result", 13);
    run("enabled = true\n"); number("result", 8);
}

TEST_F(NativeTernaryDependencies, NestedGroupingAndDerivedInvalidationChain) {
    run("let enabled = true\nlet inner = false\nlet x = 2\nlet y = 4\nlet z = 9\n"
        "get selected = (enabled ? ((inner ? (x) : (y))) : (z))\nget result = selected * 2\n");
    EXPECT_EQ(dependencies("selected"), (std::set<std::string>{"enabled", "inner", "x", "y", "z"}));
    number("result", 8);
    run("x = 3\n"); number("result", 8);
    run("inner = true\n"); number("result", 6);
    run("enabled = false\n"); number("result", 18);
    run("z = 10\n"); number("result", 20);
    run("enabled = true\n"); number("result", 6);
}

TEST_F(NativeTernaryDependencies, MemberRootsAndComputedConditionalIndices) {
    run("let enabled = true\nlet index = 0\nlet other = 1\nlet items = [2, 5]\n"
        "let item = { left: 7, right: 9 }\n"
        "get selected = items[(enabled ? index : other)]\n"
        "get result = (enabled ? item.left : item.right) + selected\n");
    const auto selected = dependencies("selected");
    for (const auto& root : {"items", "enabled", "index", "other"}) EXPECT_EQ(selected.count(root), 1u);
    const auto result = dependencies("result");
    for (const auto& root : {"enabled", "item", "selected"}) EXPECT_EQ(result.count(root), 1u);
    number("result", 9);
    run("item.left = 8\n"); number("result", 10);
    run("index = 1\n"); number("result", 13);
    run("enabled = false\n"); number("result", 14);
    run("other = 0\n"); number("result", 11);
    run("items[0] = 3\n"); number("result", 12);
    run("item.right = 10\n"); number("result", 13);
}

TEST_F(NativeTernaryDependencies, GatheringNeverExecutesInactiveBranches) {
    run("let enabled = true\nlet x = 1\nlet calls = 0\n"
        "when bump\n  calls += 1\n  return 9\n"
        "get result = enabled ? x : bump()\n");
    number("calls", 0); number("result", 1);
    run("x = 2\n"); number("calls", 0); number("result", 2);
    run("enabled = false\n"); number("calls", 1); number("result", 9);
    run("enabled = true\n"); number("calls", 1); number("result", 2);
}

TEST_F(NativeTernaryDependencies, InactiveIndexExpressionsAreNotEvaluated) {
    run("let enabled = true\nlet index = 0\nlet items = [7]\n"
        "get result = enabled ? 2 : items[(index / 0)]\n");
    number("result", 2);
    const auto reads = dependencies("result");
    for (const auto& root : {"enabled", "index", "items"}) EXPECT_EQ(reads.count(root), 1u);
    run("index = 1\n"); number("result", 2);
}

TEST_F(NativeTernaryDependencies, CompilerAndNativeAgreeOnRequiredRoots) {
    const std::string source = "let enabled = true\nlet x = 1\nlet y = 2\nget result = (enabled ? x : y)\n";
    auto program = parse(source);
    const auto& derive = static_cast<const DeriveStatementNode&>(*program.back());
    auto ir = jtml::buildExpressionIr(derive.expression.get());
    ASSERT_TRUE(ir);
    EXPECT_EQ(ir->kind, jtml::ExpressionIrKind::Conditional);
    ASSERT_EQ(ir->children.size(), 3u);
    const auto irReads = jtml::expressionIrDependencies(*ir);
    const auto semantic = jtml::analyzeSemanticProgram(program, "jtml 2\n" + source);
    interpreter.interpret(program);
    const auto native = dependencies("result");
    for (const auto& root : {"enabled", "x", "y"}) {
        EXPECT_NE(std::find(irReads.begin(), irReads.end(), root), irReads.end());
        EXPECT_EQ(native.count(root), 1u);
        EXPECT_TRUE(std::any_of(semantic.dependencies.begin(), semantic.dependencies.end(), [&](const auto& edge) {
            return edge.from == "result" && edge.to == root && edge.kind == "derives";
        }));
    }
}
} // namespace

TEST_F(NativeTernaryDependencies, GatheringNeverExecutesInactiveTrueBranch) {
    run("let enabled = false\nlet x = 1\nlet calls = 0\n"
        "when bump\n  calls += 1\n  return 9\n"
        "get result = enabled ? bump() : x\n");
    number("calls", 0); number("result", 1);
    run("x = 2\n"); number("calls", 0); number("result", 2);
    run("enabled = true\n"); number("calls", 1); number("result", 9);
    run("enabled = false\n"); number("calls", 1); number("result", 2);
}
