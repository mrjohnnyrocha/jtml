#include "cli/commands.h"
#include "json.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::filesystem::path writeTempJtml(const std::string& name, const std::string& source) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto path = std::filesystem::temp_directory_path() /
                ("jtml-cli-" + name + "-" + std::to_string(stamp) + ".jtml");
    std::ofstream out(path);
    out << source;
    return path;
}

struct CapturedCommand {
    int code = 0;
    std::string out;
    std::string err;
};

template <typename Fn>
CapturedCommand captureCommand(Fn fn) {
    std::ostringstream out;
    std::ostringstream err;
    auto* oldOut = std::cout.rdbuf(out.rdbuf());
    auto* oldErr = std::cerr.rdbuf(err.rdbuf());
    CapturedCommand result;
    try {
        result.code = fn();
    } catch (...) {
        std::cout.rdbuf(oldOut);
        std::cerr.rdbuf(oldErr);
        throw;
    }
    std::cout.rdbuf(oldOut);
    std::cerr.rdbuf(oldErr);
    result.out = out.str();
    result.err = err.str();
    return result;
}

} // namespace

TEST(CliLint, ObservableWarningsRespectFriendlyComponentStoreAndMemberRefs) {
    const auto file = writeTempJtml(
        "observable-clean",
        "jtml 2\n"
        "\n"
        "let count = 0\n"
        "get label = \"Count {count}\"\n"
        "\n"
        "store workspace\n"
        "  let theme = \"Executive\"\n"
        "  let approvals = 0\n"
        "\n"
        "  when approve\n"
        "    approvals += 1\n"
        "\n"
        "make Metric title value\n"
        "  article\n"
        "    h2 title\n"
        "    text value\n"
        "\n"
        "page\n"
        "  Metric \"Current\" label\n"
        "  text \"Theme: {workspace.theme}\"\n"
        "  text \"Approvals: {workspace.approvals}\"\n"
        "  button \"Approve\" click workspace.approve\n");

    jtml::cli::Options opts;
    opts.inputFile = file.string();
    opts.syntax = jtml::SyntaxMode::Auto;

    const auto result = captureCommand([&] { return jtml::cli::cmdLint(opts); });
    EXPECT_EQ(result.code, 0);
    EXPECT_NE(result.out.find("OK:"), std::string::npos) << result.out << result.err;
    EXPECT_EQ(result.err.find("JTML_DEAD_STATE"), std::string::npos) << result.err;
    EXPECT_EQ(result.err.find("JTML_UNUSED_DERIVED"), std::string::npos) << result.err;
    EXPECT_EQ(result.err.find("JTML_UNBOUND_ACTION"), std::string::npos) << result.err;
}

TEST(CliLint, ObservableWarningsCatchDeadStateUnusedDerivedAndUnboundAction) {
    const auto file = writeTempJtml(
        "observable-warnings",
        "jtml 2\n"
        "\n"
        "let unused = \"hidden\"\n"
        "get computed = \"not shown\"\n"
        "\n"
        "when neverCalled\n"
        "  unused = \"changed\"\n"
        "\n"
        "page\n"
        "  h1 \"Visible\"\n");

    jtml::cli::Options opts;
    opts.inputFile = file.string();
    opts.syntax = jtml::SyntaxMode::Auto;

    const auto result = captureCommand([&] { return jtml::cli::cmdLint(opts); });
    EXPECT_EQ(result.code, 0);
    EXPECT_NE(result.err.find("JTML_UNUSED_DERIVED"), std::string::npos) << result.err;
    EXPECT_NE(result.err.find("JTML_UNBOUND_ACTION"), std::string::npos) << result.err;
}

TEST(CliExplain, JsonReportsObservableDepthAndStoreActions) {
    const auto file = writeTempJtml(
        "observable-explain",
        "jtml 2\n"
        "\n"
        "let token = \"ok\"\n"
        "let users = fetch \"/api/users\" group people key token dedupe every 30000 background\n"
        "css raw\n"
        "  third-party-card { display: block; }\n"
        "extern notify from \"host.notify\"\n"
        "\n"
        "store workspace\n"
        "  let approvals = 0\n"
        "  when approve\n"
        "    approvals += 1\n"
        "\n"
        "make Home\n"
        "  let open = true\n"
        "  when toggle\n"
        "    open = !open\n"
        "  page\n"
        "    box class \"approval-card\" aria-label \"Approval queue\"\n"
        "      text \"Approvals: {workspace.approvals}\"\n"
        "    for user in users.data\n"
        "      text user.name\n"
        "    html raw \"<third-party-card></third-party-card>\"\n"
        "    button \"Toggle\" click toggle\n"
        "    button \"Approve\" click workspace.approve\n"
        "\n"
        "route \"/\" as Home\n");

    jtml::cli::Options opts;
    opts.inputFile = file.string();
    opts.syntax = jtml::SyntaxMode::Auto;
    opts.json = true;

    const auto result = captureCommand([&] { return jtml::cli::cmdExplain(opts); });
    ASSERT_EQ(result.code, 0) << result.out << result.err;
    const auto report = nlohmann::json::parse(result.out);
    EXPECT_EQ(report["depth"]["label"], "full-featured");
    EXPECT_EQ(report["issues"]["unboundActions"].size(), 0u) << report.dump(2);
    EXPECT_NE(report["observable"]["actions"].dump().find("approve"), std::string::npos)
        << report.dump(2);
    EXPECT_GE(report["semantic"]["attributes"]["literal"].get<int>(), 1);
    EXPECT_GE(report["semantic"]["attributes"]["passthrough"].get<int>(), 1);
    EXPECT_GE(report["semantic"]["attributes"]["event"].get<int>(), 1);
    EXPECT_GE(report["semantic"]["nodes"]["state"].get<int>(), 1);
    EXPECT_GE(report["semantic"]["nodes"]["fetches"].get<int>(), 1);
    EXPECT_GE(report["semantic"]["nodes"]["routes"].get<int>(), 1);
    EXPECT_GE(report["semantic"]["nodes"]["components"].get<int>(), 1);
    EXPECT_GE(report["semantic"]["nodes"]["stores"].get<int>(), 1);
    EXPECT_EQ(report["semantic"]["nodes"]["externs"].get<int>(), 1);
    EXPECT_EQ(report["semantic"]["nodes"]["styleBlocks"].get<int>(), 1);
    EXPECT_EQ(report["semantic"]["nodes"]["rawCssBlocks"].get<int>(), 1);
    EXPECT_EQ(report["semantic"]["nodes"]["rawHtmlBlocks"].get<int>(), 1);
    ASSERT_EQ(report["semantic"]["routeRecords"].size(), 1u) << report.dump(2);
    EXPECT_EQ(report["semantic"]["routeRecords"][0]["path"], "/");
    EXPECT_EQ(report["semantic"]["routeRecords"][0]["component"], "Home");
    ASSERT_EQ(report["semantic"]["fetchRecords"].size(), 1u) << report.dump(2);
    EXPECT_EQ(report["semantic"]["fetchRecords"][0]["name"], "users");
    EXPECT_EQ(report["semantic"]["fetchRecords"][0]["url"], "/api/users");
    EXPECT_EQ(report["semantic"]["fetchRecords"][0]["method"], "GET");
    EXPECT_EQ(report["semantic"]["fetchRecords"][0]["group"], "people");
    EXPECT_EQ(report["semantic"]["fetchRecords"][0]["cacheKeyExpr"], "token");
    EXPECT_EQ(report["semantic"]["fetchRecords"][0]["revalidateMs"], "30000");
    EXPECT_TRUE(report["semantic"]["fetchRecords"][0]["dedupe"]);
    EXPECT_TRUE(report["semantic"]["fetchRecords"][0]["background"]);
    ASSERT_EQ(report["semantic"]["componentDefinitions"].size(), 1u) << report.dump(2);
    EXPECT_EQ(report["semantic"]["componentDefinitions"][0]["name"], "Home");
    EXPECT_NE(report["semantic"]["componentDefinitions"][0]["localState"].dump().find("open"),
              std::string::npos) << report.dump(2);
    EXPECT_NE(report["semantic"]["componentDefinitions"][0]["localActions"].dump().find("toggle"),
              std::string::npos) << report.dump(2);
    EXPECT_NE(report["semantic"]["componentDefinitions"][0]["eventBindings"].dump().find("toggle"),
              std::string::npos) << report.dump(2);
    ASSERT_EQ(report["semantic"]["componentInstances"].size(), 1u) << report.dump(2);
    EXPECT_EQ(report["semantic"]["componentInstances"][0]["component"], "Home");
    EXPECT_EQ(report["semantic"]["componentInstances"][0]["role"], "route");
    EXPECT_GE(report["semantic"]["dependencies"].size(), 1u);
    EXPECT_EQ(report["semantic"]["sourceOfTruth"], "typed AST -> semantic analysis -> observable graph");
}

TEST(CliExplain, TextReportsComponentOwnedSemantics) {
    const auto file = writeTempJtml(
        "component-owned-text",
        "jtml 2\n"
        "\n"
        "make Toggle label\n"
        "  let open = false\n"
        "  get status = open ? \"open\" : \"closed\"\n"
        "  when toggle\n"
        "    open = !open\n"
        "  effect open\n"
        "    open = open\n"
        "  box\n"
        "    button label click toggle\n"
        "    text status\n"
        "\n"
        "page\n"
        "  Toggle \"Menu\"\n");

    jtml::cli::Options opts;
    opts.inputFile = file.string();
    opts.syntax = jtml::SyntaxMode::Auto;

    const auto result = captureCommand([&] { return jtml::cli::cmdExplain(opts); });
    ASSERT_EQ(result.code, 0) << result.out << result.err;
    EXPECT_NE(result.out.find("Component definitions (1):"), std::string::npos)
        << result.out;
    EXPECT_NE(result.out.find("+ Toggle(label)"), std::string::npos) << result.out;
    EXPECT_NE(result.out.find("state: open"), std::string::npos) << result.out;
    EXPECT_NE(result.out.find("derived: status"), std::string::npos) << result.out;
    EXPECT_NE(result.out.find("actions: toggle"), std::string::npos) << result.out;
    EXPECT_NE(result.out.find("effects: open"), std::string::npos) << result.out;
    EXPECT_NE(result.out.find("event bindings: toggle"), std::string::npos) << result.out;
    EXPECT_NE(result.out.find("Component instances (1):"), std::string::npos)
        << result.out;
    EXPECT_NE(result.out.find("Toggle_1 : Toggle"), std::string::npos) << result.out;
    EXPECT_NE(result.out.find("locals: open->__Toggle_1_open"), std::string::npos)
        << result.out;
}

TEST(CliKeywords, JsonReportsCanonicalFriendlyCatalog) {
    jtml::cli::Options opts;
    opts.json = true;

    const auto result = captureCommand([&] { return jtml::cli::cmdKeywords(opts); });
    ASSERT_EQ(result.code, 0) << result.out << result.err;
    const auto report = nlohmann::json::parse(result.out);

    EXPECT_EQ(report["dialect"], "Friendly JTML 2");
    EXPECT_NE(report["sourceOfTruth"].get<std::string>().find("Friendly is canonical"),
              std::string::npos);
    EXPECT_GE(report["friendlyGroups"].size(), 8u);
    const std::string friendly = report["friendlyKeywords"].dump();
    EXPECT_NE(friendly.find("fetch"), std::string::npos);
    EXPECT_NE(friendly.find("route"), std::string::npos);
    EXPECT_NE(friendly.find("theme"), std::string::npos);
    EXPECT_NE(friendly.find("scene3d"), std::string::npos);
    EXPECT_NE(friendly.find("activeRouteName"), std::string::npos);
    EXPECT_EQ(friendly.find("define"), std::string::npos);

    const std::string compatibility = report["compatibilityBackendKeywords"].dump();
    EXPECT_NE(compatibility.find("define"), std::string::npos);
    EXPECT_NE(compatibility.find("function"), std::string::npos);
}

TEST(CliUi, JsonReportsSemanticUiCatalog) {
    jtml::cli::Options opts;
    opts.json = true;

    const auto result = captureCommand([&] { return jtml::cli::cmdUi(opts); });
    ASSERT_EQ(result.code, 0) << result.out << result.err;
    const auto report = nlohmann::json::parse(result.out);

    EXPECT_EQ(report["dialect"], "Friendly JTML 2");
    EXPECT_NE(report["sourceOfTruth"].get<std::string>().find("Semantic UI"),
              std::string::npos);
    EXPECT_GE(report["primitives"].size(), 12u);
    EXPECT_GE(report["modifiers"].size(), 8u);

    const std::string primitives = report["primitives"].dump();
    EXPECT_NE(primitives.find("\"panel\""), std::string::npos);
    EXPECT_NE(primitives.find("\"metric\""), std::string::npos);
    EXPECT_NE(primitives.find("\"toolbar\""), std::string::npos);
    EXPECT_NE(primitives.find("\"dialog\""), std::string::npos);

    const std::string modifiers = report["modifiers"].dump();
    EXPECT_NE(modifiers.find("\"cols\""), std::string::npos);
    EXPECT_NE(modifiers.find("\"tone\""), std::string::npos);
    EXPECT_NE(modifiers.find("\"danger\""), std::string::npos);

    const std::string tokens = report["themeTokenKinds"].dump();
    EXPECT_NE(tokens.find("color"), std::string::npos);
    EXPECT_NE(tokens.find("shadow"), std::string::npos);
}

TEST(CliLint, RawEscapeHatchesAreVisibleWarnings) {
    const auto file = writeTempJtml(
        "raw-escape-hatches",
        "jtml 2\n"
        "css raw\n"
        "  third-party-card { display: block; }\n"
        "extern notify from \"host.notify\"\n"
        "page\n"
        "  html raw \"<third-party-card></third-party-card>\"\n"
        "  button \"Notify\" click notify(\"Saved\")\n");

    jtml::cli::Options opts;
    opts.inputFile = file.string();
    opts.syntax = jtml::SyntaxMode::Auto;

    const auto result = captureCommand([&] { return jtml::cli::cmdLint(opts); });
    EXPECT_EQ(result.code, 0);
    EXPECT_NE(result.err.find("JTML_RAW_CSS_ESCAPE_HATCH"), std::string::npos) << result.err;
    EXPECT_NE(result.err.find("JTML_RAW_HTML_ESCAPE_HATCH"), std::string::npos) << result.err;
    EXPECT_NE(result.err.find("JTML_EXTERN_ESCAPE_HATCH"), std::string::npos) << result.err;
}

TEST(CliLint, SemanticUiModifierWarningsAreVisible) {
    const auto file = writeTempJtml(
        "semantic-ui-modifiers",
        "jtml 2\n"
        "page\n"
        "  card cols 3\n"
        "    text \"Cards do not own grid columns\"\n"
        "  shell tone danger\n"
        "    content\n"
        "      text \"Tone belongs on content surfaces\"\n");

    jtml::cli::Options opts;
    opts.inputFile = file.string();
    opts.syntax = jtml::SyntaxMode::Auto;

    const auto result = captureCommand([&] { return jtml::cli::cmdLint(opts); });
    EXPECT_EQ(result.code, 0);
    EXPECT_NE(result.err.find("JTML_UI_COLS_ON_NON_GRID"), std::string::npos) << result.err;
    EXPECT_NE(result.err.find("JTML_UI_TONE_ON_LAYOUT"), std::string::npos) << result.err;
}

TEST(CliLint, SemanticUiInvalidModifierValuesAreVisible) {
    const auto file = writeTempJtml(
        "semantic-ui-invalid-values",
        "jtml 2\n"
        "page\n"
        "  grid cols 9 gap huge\n"
        "    card tone loud\n"
        "      text \"Invalid visual tokens\"\n");

    jtml::cli::Options opts;
    opts.inputFile = file.string();
    opts.syntax = jtml::SyntaxMode::Auto;

    const auto result = captureCommand([&] { return jtml::cli::cmdLint(opts); });
    EXPECT_EQ(result.code, 0);
    EXPECT_NE(result.err.find("JTML_UI_INVALID_MODIFIER_VALUE"), std::string::npos)
        << result.err;
    EXPECT_NE(result.err.find("expected one of"), std::string::npos) << result.err;
}

TEST(CliLint, SemanticUiAccessibilityWarningsAreVisible) {
    const auto file = writeTempJtml(
        "semantic-ui-accessibility",
        "jtml 2\n"
        "page\n"
        "  panel\n"
        "    text \"Needs a label\"\n"
        "  modal\n"
        "    text \"Needs a label too\"\n");

    jtml::cli::Options opts;
    opts.inputFile = file.string();
    opts.syntax = jtml::SyntaxMode::Auto;

    const auto result = captureCommand([&] { return jtml::cli::cmdLint(opts); });
    EXPECT_EQ(result.code, 0);
    EXPECT_NE(result.err.find("JTML_UI_SURFACE_UNLABELED"), std::string::npos)
        << result.err;
    EXPECT_NE(result.err.find("JTML_UI_OVERLAY_UNLABELED"), std::string::npos)
        << result.err;
    EXPECT_NE(result.err.find("JTML_UI_OVERLAY_WITHOUT_DISMISS"), std::string::npos)
        << result.err;
}

TEST(CliLint, SemanticUiFormAndTabWarningsAreVisible) {
    const auto file = writeTempJtml(
        "semantic-ui-form-tabs",
        "jtml 2\n"
        "page\n"
        "  field\n"
        "    text \"No input yet\"\n"
        "  tabs\n"
        "    text \"No tab yet\"\n"
        "  tab \"Loose tab\"\n");

    jtml::cli::Options opts;
    opts.inputFile = file.string();
    opts.syntax = jtml::SyntaxMode::Auto;

    const auto result = captureCommand([&] { return jtml::cli::cmdLint(opts); });
    EXPECT_EQ(result.code, 0);
    EXPECT_NE(result.err.find("JTML_UI_FIELD_WITHOUT_CONTROL"), std::string::npos)
        << result.err;
    EXPECT_NE(result.err.find("JTML_UI_TABS_EMPTY"), std::string::npos)
        << result.err;
    EXPECT_NE(result.err.find("JTML_UI_TAB_WITHOUT_ACTION"), std::string::npos)
        << result.err;
}

TEST(CliLint, SemanticUiFieldLabelWarningIsVisible) {
    const auto file = writeTempJtml(
        "semantic-ui-field-label",
        "jtml 2\n"
        "let email = \"\"\n"
        "page\n"
        "  field\n"
        "    input into email\n");

    jtml::cli::Options opts;
    opts.inputFile = file.string();
    opts.syntax = jtml::SyntaxMode::Auto;

    const auto result = captureCommand([&] { return jtml::cli::cmdLint(opts); });
    EXPECT_EQ(result.code, 0);
    EXPECT_NE(result.err.find("JTML_UI_FIELD_UNLABELED"), std::string::npos)
        << result.err;
}

TEST(CliExplain, TextReportsSemanticUiPrimitivesAndModifiers) {
    const auto file = writeTempJtml(
        "semantic-ui-explain",
        "jtml 2\n"
        "theme\n"
        "  color primary \"#2563eb\"\n"
        "page\n"
        "  panel title \"Usage\" pad lg shadow md\n"
        "    grid cols 2 gap md\n"
        "      metric \"Users\" 42 \"Active\" tone good\n");

    jtml::cli::Options opts;
    opts.inputFile = file.string();
    opts.syntax = jtml::SyntaxMode::Auto;

    const auto result = captureCommand([&] { return jtml::cli::cmdExplain(opts); });
    ASSERT_EQ(result.code, 0) << result.out << result.err;
    EXPECT_NE(result.out.find("Semantic UI:"), std::string::npos) << result.out;
    EXPECT_NE(result.out.find("primitives: grid, metric, panel"), std::string::npos)
        << result.out;
    EXPECT_NE(result.out.find("- grid cols 2"), std::string::npos) << result.out;
    EXPECT_NE(result.out.find("- metric tone good"), std::string::npos) << result.out;
    EXPECT_NE(result.out.find("author theme tokens:"), std::string::npos) << result.out;
}

TEST(CliExplain, JsonReportsSemanticUiSurface) {
    const auto file = writeTempJtml(
        "semantic-ui-json",
        "jtml 2\n"
        "theme\n"
        "  color primary \"#2563eb\"\n"
        "  space md 12\n"
        "page\n"
        "  panel title \"Usage\" pad lg shadow md\n"
        "    grid cols 2 gap md\n"
        "      metric \"Users\" 42 \"Active\" tone good\n");

    jtml::cli::Options opts;
    opts.inputFile = file.string();
    opts.syntax = jtml::SyntaxMode::Auto;
    opts.json = true;

    const auto result = captureCommand([&] { return jtml::cli::cmdExplain(opts); });
    ASSERT_EQ(result.code, 0) << result.out << result.err;
    const auto report = nlohmann::json::parse(result.out);
    ASSERT_TRUE(report["semantic"].contains("ui")) << report.dump(2);
    const auto ui = report["semantic"]["ui"];
    EXPECT_NE(ui["primitives"].dump().find("panel"), std::string::npos) << report.dump(2);
    EXPECT_NE(ui["primitives"].dump().find("grid"), std::string::npos) << report.dump(2);
    EXPECT_NE(ui["primitives"].dump().find("metric"), std::string::npos) << report.dump(2);
    EXPECT_EQ(ui["authorThemeTokens"].get<int>(), 2);
    EXPECT_GE(ui["themeTokens"].get<int>(), ui["authorThemeTokens"].get<int>());
    EXPECT_GE(ui["styleBlocks"].get<int>(), 1);
    EXPECT_NE(ui["uses"].dump().find("\"primitive\":\"panel\""), std::string::npos)
        << report.dump(2);
    EXPECT_NE(ui["uses"].dump().find("\"hasTitle\":true"), std::string::npos)
        << report.dump(2);
    EXPECT_NE(ui["uses"].dump().find("\"hasLabel\""), std::string::npos)
        << report.dump(2);
    EXPECT_NE(ui["uses"].dump().find("\"hasTabChild\""), std::string::npos)
        << report.dump(2);
    EXPECT_NE(ui["modifiers"].dump().find("\"modifier\":\"cols\""), std::string::npos)
        << report.dump(2);
    EXPECT_NE(ui["modifiers"].dump().find("\"modifier\":\"tone\""), std::string::npos)
        << report.dump(2);
}

TEST(CliBuild, BrowserTargetWritesLocalRuntimeManifest) {
    const auto file = writeTempJtml(
        "browser-build",
        "jtml 2\n"
        "\n"
        "let count = 0\n"
        "get label = \"Count {count}\"\n"
        "when add\n"
        "  count += 1\n"
        "page\n"
        "  text label\n"
        "  button \"Add\" click add\n");

    const auto outDir = std::filesystem::temp_directory_path() /
                        ("jtml-browser-build-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    jtml::cli::Options opts;
    opts.inputFile = file.string();
    opts.outputFile = outDir.string();
    opts.syntax = jtml::SyntaxMode::Auto;
    opts.target = "browser";

    const auto result = captureCommand([&] { return jtml::cli::cmdBuild(opts); });
    EXPECT_EQ(result.code, 0) << result.out << result.err;

    const auto htmlPath = outDir / "index.html";
    ASSERT_TRUE(std::filesystem::exists(htmlPath));
    std::ifstream input(htmlPath);
    const std::string html((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    EXPECT_NE(html.find("id=\"__jtml_client_manifest\""), std::string::npos) << html;
    EXPECT_NE(html.find("const browserLocalRuntime = true;"), std::string::npos) << html;
    EXPECT_NE(html.find("Browser-local runtime active"), std::string::npos) << html;
}
