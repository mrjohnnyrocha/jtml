#include "jtml/friendly.h"
#include "jtml/interpreter.h"
#include "jtml/lexer.h"
#include "jtml/parser.h"
#include <gtest/gtest.h>
#include <iostream>
#include <sstream>

namespace {
class NativeComponentEnvironment : public ::testing::Test {
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
    auto parse(const std::string& source) {
        Lexer lexer(jtml::friendlyToClassic(source));
        auto tokens = lexer.tokenize();
        if (!lexer.getErrors().empty()) throw std::runtime_error("component test lexer error");
        Parser parser(std::move(tokens));
        auto program = parser.parseProgram();
        if (!parser.getErrors().empty()) throw std::runtime_error("component test parser error");
        return program;
    }
    void run(const std::string& source) {
        auto program = parse(source);
        transpiler.transpile(program);
        interpreter.interpret(program);
    }
    auto snapshot() { return nlohmann::json::parse(interpreter.getStateJSON()); }
    void action(const std::string& id, const std::string& name) {
        std::string bindings, error;
        ASSERT_TRUE(interpreter.dispatchComponentAction(id, name, nlohmann::json::array(), bindings, error)) << error;
    }
};

TEST_F(NativeComponentEnvironment, ExpandedNestedInitializationKeepsLexicalLookup) {
    run("jtml 2\nmake Child value\n  get result = value * 2\n  p result\n"
        "make Parent\n  let x = 2\n  Child x\npage\n  Parent\n");
    const auto state = snapshot();
    bool found = false;
    for (const auto& component : state["components"]) if (component["component"] == "Child") {
        ASSERT_EQ(component["locals"]["result"]["value"], 4); found = true;
    }
    EXPECT_TRUE(found);
}

TEST_F(NativeComponentEnvironment, ChildDirtyKeysDoNotPolluteSameNamedParent) {
    auto parent = std::make_shared<JTML::Environment>(nullptr, 42);
    auto child = std::make_shared<JTML::Environment>(parent, 43);
    parent->defineVariable({42, "result"}, std::make_shared<JTML::VarValue>(2.0));
    child->defineVariable({43, "result"}, std::make_shared<JTML::VarValue>(4.0));
    child->setVariable({43, "result"}, std::make_shared<JTML::VarValue>(6.0));
    EXPECT_EQ(parent->nameToId.count({43, "result"}), 0u);
    EXPECT_TRUE(parent->dirtyVars.empty());
    EXPECT_DOUBLE_EQ(parent->getVariable({42, "result"})->getNumber(), 2);
    EXPECT_DOUBLE_EQ(child->getVariable({43, "result"})->getNumber(), 6);
}
} // namespace

TEST_F(NativeComponentEnvironment, ParentActionsInvalidateExpandedChildState) {
    run("jtml 2\nmake Child value\n  get result = value * 2\n  p result\n"
        "make Parent\n  let x = 2\n  when step\n    x += 1\n  Child x\npage\n  Parent\n");
    const auto initial = snapshot();
    std::string parent;
    for (const auto& c : initial["components"]) if (c["component"] == "Parent") parent = c["id"];
    ASSERT_FALSE(parent.empty());
    action(parent, "step");
    const auto after = snapshot();
    for (const auto& c : after["components"]) if (c["component"] == "Child") {
        EXPECT_EQ(c["locals"]["result"]["value"], 6);
    }
}

TEST_F(NativeComponentEnvironment, GlobalOwnerWritesUseTheSameInvalidationBoundary) {
    run("jtml 2\nlet x = 2\nmake Child value\n  get result = value * 2\n"
        "  when step\n    x += 1\n  p result\npage\n  Child x\n");
    const auto initial = snapshot();
    std::string child;
    for (const auto& c : initial["components"]) if (c["component"] == "Child") child = c["id"];
    ASSERT_FALSE(child.empty()); action(child, "step");
    const auto after = snapshot();
    EXPECT_EQ(after["variables"]["x"]["value"], 3);
    for (const auto& c : after["components"]) if (c["component"] == "Child") {
        EXPECT_EQ(c["locals"]["result"]["value"], 6);
    }
}

TEST_F(NativeComponentEnvironment, SameNamedChildStateShadowsParentAndRemountsFresh) {
    run("jtml 2\nmake Child value\n  let count = 10\n  get result = value * 2\n"
        "  when bump\n    count += 1\n"
        "  box\n    p class \"child-count\" count\n    p class \"child-result\" result\n    button \"bump\" click bump\n"
        "make Parent\n  let count = 100\n  let input = 3\n  let enabled = true\n"
        "  when toggle\n    enabled = !enabled\n"
        "  box\n    if enabled\n      Child input\npage\n  Parent\n");
    std::string parent;
    const auto initial = snapshot();
    for (const auto& c : initial["components"]) if (c["component"] == "Parent") parent = c["id"];
    ASSERT_FALSE(parent.empty());
    auto html = [&] {
        const auto state = snapshot();
        for (const auto& c : state["components"]) if (c["id"] == parent) return c["renderedHtml"].get<std::string>();
        return std::string{};
    };
    auto childId = [&] {
        const auto text = html(); const std::string marker = "data-jtml-direct-component-id=\"";
        const auto begin = text.find(marker);
        if (begin == std::string::npos) return std::string{};
        const auto start = begin + marker.size(); return text.substr(start, text.find('"', start) - start);
    };
    for (int cycle = 0; cycle < 3; ++cycle) {
        const auto child = childId(); ASSERT_FALSE(child.empty());
        EXPECT_NE(html().find(">10</p>"), std::string::npos);
        action(child, "bump"); EXPECT_NE(html().find(">11</p>"), std::string::npos);
        const auto current = snapshot();
        for (const auto& c : current["components"]) if (c["id"] == parent) EXPECT_EQ(c["locals"]["count"]["value"], 100);
        action(parent, "toggle"); EXPECT_TRUE(childId().empty());
        std::string bindings, error;
        EXPECT_FALSE(interpreter.dispatchComponentAction(child, "bump", nlohmann::json::array(), bindings, error));
        action(parent, "toggle");
    }
}

TEST_F(NativeComponentEnvironment, TransitiveDerivedOwnersStayIsolatedAcrossSiblings) {
    run("jtml 2\nmake Leaf value\n  get triple = value * 3\n  p triple\n"
        "make Child value\n  get doubled = value * 2\n  Leaf doubled\n"
        "make Parent\n  let x = 2\n  get next = x + 1\n  when step\n    x += 1\n"
        "  Child x\n  Child next\npage\n  Parent\n  Parent\n");
    std::vector<std::string> parents;
    const auto initial = snapshot();
    for (const auto& c : initial["components"]) if (c["component"] == "Parent") parents.push_back(c["id"]);
    ASSERT_EQ(parents.size(), 2u);
    auto values = [&](const std::string& component, const std::string& local) {
        std::vector<double> result;
        const auto state = snapshot();
        for (const auto& c : state["components"]) if (c["component"] == component) {
            EXPECT_TRUE(c["locals"][local]["value"].is_number());
            result.push_back(c["locals"][local]["value"].get<double>());
        }
        std::sort(result.begin(), result.end()); return result;
    };
    EXPECT_EQ(values("Leaf", "triple"), (std::vector<double>{12,12,18,18}));
    action(parents[0], "step");
    EXPECT_EQ(values("Child", "doubled"), (std::vector<double>{4,6,6,8}));
    EXPECT_EQ(values("Leaf", "triple"), (std::vector<double>{12,18,18,24}));
    action(parents[1], "step");
    EXPECT_EQ(values("Leaf", "triple"), (std::vector<double>{18,18,24,24}));
    action(parents[0], "step");
    EXPECT_EQ(values("Leaf", "triple"), (std::vector<double>{18,24,24,30}));
}

TEST_F(NativeComponentEnvironment, ResetReleasesOldGraphAndReinitializesNestedBindings) {
    const std::string source = "jtml 2\nmake Child value\n  get result = value * 2\n  p result\n"
        "make Parent\n  let x = 2\n  when step\n    x += 1\n  Child x\npage\n  Parent\n";
    for (int cycle = 0; cycle < 3; ++cycle) {
        run(source);
        const auto initial = snapshot(); std::string parent;
        for (const auto& c : initial["components"]) {
            if (c["component"] == "Parent") parent = c["id"];
            if (c["component"] == "Child") EXPECT_EQ(c["locals"]["result"]["value"], 4);
        }
        ASSERT_FALSE(parent.empty()); action(parent, "step");
        const auto changed = snapshot();
        for (const auto& c : changed["components"]) if (c["component"] == "Child") EXPECT_EQ(c["locals"]["result"]["value"], 6);
        interpreter.reset();
        const auto cleared = snapshot(); EXPECT_TRUE(cleared["components"].empty()); EXPECT_TRUE(cleared["variables"].empty());
    }
}

// A bounded fan-out exercises canonical invalidation without duplicating values
// into the parent. Only the selected owner and its descendants may change.
TEST_F(NativeComponentEnvironment, ManySiblingOwnersStayCurrentAndIsolated) {
    std::string source = "jtml 2\nmake Child value\n  get result = value * 2\n  p result\n"
        "make Parent\n  let x = 2\n  when step\n    x += 1\n  Child x\npage\n";
    constexpr size_t count = 64;
    for (size_t i = 0; i < count; ++i) source += "  Parent\n";
    run(source);
    std::vector<std::string> parents;
    const auto initial = snapshot();
    for (const auto& c : initial["components"]) if (c["component"] == "Parent") parents.push_back(c["id"]);
    ASSERT_EQ(parents.size(), count);
    for (size_t i : {size_t{0}, count / 2, count - 1}) action(parents[i], "step");
    const auto state = snapshot();
    size_t children = 0, changed = 0;
    for (const auto& c : state["components"]) if (c["component"] == "Child") {
        ++children;
        const double value = c["locals"]["result"]["value"].get<double>();
        EXPECT_TRUE(value == 4 || value == 6);
        if (value == 6) ++changed;
    }
    EXPECT_EQ(children, count); EXPECT_EQ(changed, 3u);
}

TEST_F(NativeComponentEnvironment, SideEffectingDerivedWritesInvalidateChildrenWithoutReentry) {
    run("jtml 2\nlet calls = 0\nwhen bump\n  calls += 1\n  return true\n"
        "make Child value\n  get result = value * 2\n  p result\n"
        "make Parent\n  let enabled = false\n  get gate = enabled && bump()\n"
        "  when step\n    enabled = true\n  Child calls\npage\n  Parent\n");
    const auto initial = snapshot(); std::string parent;
    for (const auto& c : initial["components"]) if (c["component"] == "Parent") parent = c["id"];
    ASSERT_EQ(initial["variables"]["calls"]["value"], 0);
    ASSERT_FALSE(parent.empty()); action(parent, "step");
    const auto state = snapshot();
    EXPECT_EQ(state["variables"]["calls"]["value"], 1);
    for (const auto& c : state["components"]) if (c["component"] == "Child") EXPECT_EQ(c["locals"]["result"]["value"], 2);
}

TEST_F(NativeComponentEnvironment, CallerCountIsNotCapturedByChildCount) {
    run("jtml 2\nmake Child value\n  let count = 10\n  get result = value * 2\n  p result\n"
        "make Parent\n  let count = 100\n  Child count\npage\n  Parent\n");
    const auto state = snapshot();
    size_t children = 0;
    for (const auto& c : state["components"]) {
        if (c["component"] == "Child") {
            ++children; EXPECT_EQ(c["locals"]["count"]["value"], 10);
            EXPECT_EQ(c["locals"]["result"]["value"], 200);
        }
        if (c["component"] == "Parent") {
            EXPECT_EQ(c["locals"]["count"]["value"], 100);
            EXPECT_NE(c["renderedHtml"].get<std::string>().find(">200</p>"), std::string::npos);
        }
    }
    EXPECT_EQ(children, 1u);
}

TEST_F(NativeComponentEnvironment, GlobalArgumentsDoNotCapturePublicLocalAliases) {
    const std::string source =
        "jtml 2\nlet count = 100\nmake Probe value\n  let count = 10\n"
        "  get result = value * 2\n  when bump\n    count += 1\n"
        "  box\n    p class \"out-result\" result\npage\n  Probe count\n  Probe (count + 1)\n";
    for (int mount = 0; mount < 3; ++mount) {
        interpreter.reset();
        run(source);
        auto state = snapshot();
        ASSERT_EQ(state["components"].size(), 2u);
        const std::string first = state["components"][0]["id"];
        action(first, "bump");
        state = snapshot();
        EXPECT_EQ(state["components"][0]["locals"]["count"]["value"], 11);
        EXPECT_EQ(state["components"][1]["locals"]["count"]["value"], 10);
        for (int count : {101, 99, 103}) {
            auto global = interpreter.getCurrentEnvironment();
            global->setVariable({global->instanceID, "count"}, std::make_shared<JTML::VarValue>(double(count)));
            state = snapshot();
            for (size_t i = 0; i < 2; ++i) {
                const auto result = state["components"][i]["locals"]["result"]["value"];
                EXPECT_TRUE(result.is_number());
                EXPECT_EQ(result, (count + static_cast<int>(i)) * 2);
                EXPECT_NE(state["components"][i]["renderedHtml"].get<std::string>().find(">" + std::to_string((count + i) * 2) + "</p>"), std::string::npos);
            }
        }
    }
}

TEST_F(NativeComponentEnvironment, GlobalArgumentsPreserveQuotedValuesAndDerivedCallerTypes) {
    run("jtml 2\nlet x = 100\nget next = x + 1\nmake Probe value label\n"
        "  let x = 10\n  get result = value * 2\n  box\n    p result\n    p label\n"
        "page\n  Probe next \"Revenue & growth\"\n");
    auto state = snapshot();
    EXPECT_EQ(state["components"][0]["locals"]["result"]["value"], 202);
    EXPECT_NE(state["components"][0]["renderedHtml"].get<std::string>().find("Revenue &amp; growth"), std::string::npos);
    auto global = interpreter.getCurrentEnvironment();
    global->setVariable({global->instanceID, "x"}, std::make_shared<JTML::VarValue>(102.0));
    state = snapshot();
    EXPECT_EQ(state["components"][0]["locals"]["result"]["value"], 206);
}
