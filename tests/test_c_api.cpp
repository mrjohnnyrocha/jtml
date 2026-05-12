#include "jtml/c_api.h"
#include "json.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {
struct CStr {
    char* value = nullptr;
    ~CStr() { jtml_free(value); }
    char** out() { return &value; }
    std::string str() const { return value ? std::string(value) : std::string(); }
};
}

TEST(CApi, RenderReturnsHtmlForFriendlySource) {
    CStr html;
    CStr error;

    ASSERT_EQ(jtml_render(
                  "jtml 2\n"
                  "page\n"
                  "  h1 \"Hello from C\"\n",
                  html.out(),
                  error.out()),
              1)
        << error.str();

    EXPECT_NE(html.str().find("Hello from C"), std::string::npos);
    EXPECT_TRUE(error.str().empty());
}

TEST(CApi, ReportsCompileErrorsWithoutThrowingAcrossBoundary) {
    CStr html;
    CStr error;

    EXPECT_EQ(jtml_render(nullptr, html.out(), error.out()), 0);
    EXPECT_TRUE(html.str().empty());
    EXPECT_NE(error.str().find("source is null"), std::string::npos);
}

TEST(CApi, LoadsRuntimeAndDispatchesComponentActions) {
    jtml_context* ctx = jtml_create();
    ASSERT_NE(ctx, nullptr);

    CStr html;
    CStr error;
    ASSERT_EQ(jtml_load(
                  ctx,
                  "jtml 2\n"
                  "make Counter label\n"
                  "  let count = 0\n"
                  "  when add\n"
                  "    count += 1\n"
                  "  box\n"
                  "    h2 label\n"
                  "    show count\n"
                  "page\n"
                  "  Counter \"First\"\n"
                  "  Counter \"Second\"\n",
                  html.out(),
                  error.out()),
              1)
        << error.str();

    CStr componentsRaw;
    componentsRaw.value = jtml_components(ctx);
    auto components = nlohmann::json::parse(componentsRaw.str());
    ASSERT_EQ(components.size(), 2);
    EXPECT_EQ(components[0]["locals"]["count"]["value"], 0);
    EXPECT_EQ(components[1]["locals"]["count"]["value"], 0);

    CStr definitionsRaw;
    definitionsRaw.value = jtml_component_definitions(ctx);
    auto definitions = nlohmann::json::parse(definitionsRaw.str());
    ASSERT_EQ(definitions.size(), 1);
    EXPECT_EQ(definitions[0]["name"], "Counter");
    EXPECT_EQ(definitions[0]["params"][0], "label");

    CStr bindings;
    CStr actionError;
    ASSERT_EQ(jtml_component_action(
                  ctx,
                  components[1]["id"].get<std::string>().c_str(),
                  "add",
                  "[]",
                  bindings.out(),
                  actionError.out()),
              1)
        << actionError.str();
    EXPECT_FALSE(bindings.str().empty());

    CStr stateRaw;
    stateRaw.value = jtml_state(ctx);
    auto state = nlohmann::json::parse(stateRaw.str());
    ASSERT_EQ(state["components"].size(), 2);
    EXPECT_EQ(state["components"][0]["locals"]["count"]["value"], 0);
    EXPECT_EQ(state["components"][1]["locals"]["count"]["value"], 1);

    jtml_destroy(ctx);
}
