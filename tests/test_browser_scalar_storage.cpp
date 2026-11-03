#include "cli/commands.h"
#include "cli/browser_build_schema.h"
#include "cli/browser_build_policy.h"
#include "json.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

namespace {
namespace fs = std::filesystem;
fs::path temporary() {
    const auto path = fs::temp_directory_path() / ("jtml-scalar-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(path); return path;
}
void write(const fs::path& path, const std::string& source) { std::ofstream(path) << source; }
std::string read(const fs::path& path) { std::ifstream in(path); return {std::istreambuf_iterator<char>(in), {}}; }
std::string compile(const fs::path& input, const fs::path& output, bool compat = false, bool debug = false) {
    jtml::cli::Options options;
    options.inputFile = input.string(); options.outputFile = output.string(); options.target = "browser";
    options.compat = compat; options.debug = debug;
    try { if (jtml::cli::cmdBuild(options) != 0) return "nonzero build result"; }
    catch (const std::exception& error) { return error.what(); }
    return {};
}
nlohmann::json manifest(const fs::path& output) {
    const auto html = read(output / "index.html");
    const std::string marker = "id=\"__jtml_client_manifest\">";
    const auto begin = html.find(marker) + marker.size();
    return nlohmann::json::parse(html.substr(begin, html.find("</script>", begin) - begin));
}
void program(const fs::path& root, const std::string& entry, const std::string& dependency,
             const std::string& component = "export make Card\n  text \"Card\"\n") {
    write(root / "private.jtml", "jtml 2\n" + dependency + component);
    write(root / "index.jtml", "jtml 2\nuse Card from \"./private.jtml\"\n" + entry + "page\n  Card\n");
}
}

TEST(BrowserScalarStorage, OwnedInitializersReadsWritesAndParameterShadowing) {
    const auto output = temporary();
    const auto input = fs::path(JTML_SOURCE_DIR) / "tests/fixtures/production/module-scalar-storage/index.jtml";
    ASSERT_EQ(compile(input, output), "");
    const auto client = manifest(output);
    EXPECT_FALSE(client["state"].contains("tone"));
    EXPECT_FALSE(client["state"].contains("seed"));
    ASSERT_EQ(client["state"].size(), 4u);
    std::set<std::string> keys;
    for (const auto& module : client["project"]["modules"]) {
        const auto& plan = module["plan"];
        const auto suffix = plan["actions"].contains("readEntry") ? "Entry" : "Private";
        const std::string readName = std::string("read") + suffix;
        const auto tone = plan["actions"][readName]["body"][0]["exprPlan"]["root"].get<std::string>();
        const auto seed = plan["actions"][readName + "Seed"]["body"][0]["exprPlan"]["root"].get<std::string>();
        EXPECT_TRUE(keys.insert(tone).second); EXPECT_TRUE(keys.insert(seed).second);
        EXPECT_EQ(plan["state"][seed], tone);
        EXPECT_EQ(plan["statePlans"][seed]["root"], tone);
        const auto& set = plan["actions"][std::string("set") + suffix];
        EXPECT_EQ(set["body"][0]["lhs"], tone);
        EXPECT_EQ(set["body"][0]["exprPlan"]["left"]["root"], tone);
        const auto& shadow = plan["actions"][std::string("shadow") + suffix];
        const auto param = shadow["params"][0];
        EXPECT_NE(param, tone);
        EXPECT_EQ(shadow["body"][0]["lhs"], param);
        EXPECT_EQ(shadow["body"][0]["exprPlan"]["left"]["root"], param);
        EXPECT_EQ(shadow["body"][1]["exprPlan"]["root"], param);
        EXPECT_EQ(client["actions"][readName], plan["actions"][readName]);
        EXPECT_EQ(client["state"][tone], plan["state"][tone]);
    }
    EXPECT_EQ(keys.size(), 4u);
}

TEST(BrowserScalarStorage, UnsupportedShapesRemainTransactionalRejections) {
    const std::string state = "let tone = \"entry\"\n";
    const std::string other = "let tone = \"private\"\n";
    const std::vector<std::pair<std::string, std::string>> cases = {
        {state, "get tone = \"derived\"\n"},
        {state, "let tone = fetch \"/api\" lazy\n"},
        {state + "when collision\n  show tone\n", other + "when collision\n  show tone\n"},
        {state + "effect tone\n  show tone\n", other},
        {state + "store data\n  let value = false\n", other},
        {state + "get label = tone\n", other},
        {"let tone = {value: 1}\n", other},
        {"const tone = \"entry\"\n", other},
        {state + "when writeImported\n  hidden = \"bad\"\n", other + "let hidden = \"private\"\n"},
        {state + "when readImported\n  show hidden\n", other + "let hidden = \"private\"\n"},
        {"let tone = hidden\n", other + "let hidden = \"private\"\n"},
        {"let first = tone\n" + state, other},
        {state + "when capture\n  show __jtml_scalar_00000000000000000000\n", other},
        {state + "when nestedLocal\n  let tone = \"local\"\n  show tone\n", other},
        {state + "when callOther\n  setPrivate()\n", other + "when setPrivate\n  tone = \"other\"\n"},
        {state + "when __proto__\n  show tone\n", other},
        {state + "when conditional\n  if true\n    tone = \"other\"\n", other},
        {state, other + "when memberWrite\n  tone[0] = \"bad\"\n"},
        {state + "when alias value\n  tone = value\n", other},
    };
    for (const auto& pair : cases) {
        SCOPED_TRACE(pair.first + pair.second);
        const auto root = temporary(); program(root, pair.first, pair.second);
        const auto out = root / "out"; fs::create_directories(out);
        write(out / "index.html", "previous successful output");
        const auto error = compile(root / "index.jtml", out);
        EXPECT_NE(error, "");
        EXPECT_EQ(read(out / "index.html"), "previous successful output");
        EXPECT_FALSE(fs::exists(out / "app.js"));
        EXPECT_FALSE(fs::exists(out / "jtml-runtime.js"));
    }
    {
        const auto root = temporary();
        write(root / "private.jtml", "jtml 2\nexport let borrowed = \"private\"\nlet tone = \"private\"\nexport make Card\n  text \"Card\"\n");
        write(root / "index.jtml", "jtml 2\nuse {Card, borrowed} from \"./private.jtml\"\nlet tone = \"entry\"\nwhen writeImported\n  borrowed = \"bad\"\npage\n  Card\n");
        EXPECT_NE(compile(root / "index.jtml", root / "out"), "");
        EXPECT_FALSE(fs::exists(root / "out/index.html"));
    }
    for (const auto& component : {
             "export make Card\n  page class tone\n    text \"Card\"\n",
             "export make Card\n  page\n    text tone\n",
             "export make Card\n  let local = 0\n  text local\n"}) {
        const auto root = temporary(); program(root, state, other, component);
        EXPECT_NE(compile(root / "index.jtml", root / "out"), "");
        EXPECT_FALSE(fs::exists(root / "out/index.html"));
    }
}

TEST(BrowserScalarStorage, FreshIdentifiersAndCompatibilityContracts) {
    const auto root = temporary();
    const std::string reserved = "__jtml_scalar_00000000000000000000";
    program(root,
        "let tone = \"entry\"\nlet " + reserved + " = \"reserved\"\nwhen readEntry\n  show tone\nwhen literal\n  show \"tone\"\n",
        "let tone = \"private\"\nwhen readPrivate\n  show tone\n");
    ASSERT_EQ(compile(root / "index.jtml", root / "clean"), "");
    const auto clean = manifest(root / "clean");
    EXPECT_FALSE(clean["state"].contains(reserved));
    EXPECT_EQ(clean["actions"]["literal"]["body"][0]["exprPlan"]["value"], "tone");
    for (const bool debug : {false, true}) {
        const auto out = root / (debug ? "debug" : "compat");
        ASSERT_EQ(compile(root / "index.jtml", out, !debug, debug), "");
        const auto legacy = manifest(out);
        EXPECT_TRUE(legacy["state"].contains("tone"));
        EXPECT_TRUE(legacy["state"].contains(reserved));
        EXPECT_EQ(legacy["actions"]["readEntry"]["body"][0]["exprPlan"]["root"], "tone");
    }
    program(root, "let entryTone = \"entry\"\n", "let privateTone = \"private\"\n");
    ASSERT_EQ(compile(root / "index.jtml", root / "disjoint"), "");
    const auto disjoint = manifest(root / "disjoint");
    EXPECT_TRUE(disjoint["state"].contains("entryTone"));
    EXPECT_TRUE(disjoint["state"].contains("privateTone"));
}

TEST(BrowserScalarStorage, DecodedStringExpressionsCannotBypassBindingProof) {
    // Friendly lowers ordinary interpolation to AST expressions. Unicode escapes
    // can instead leave a string node that Expression IR later interpolates.
    const std::vector<std::string> cases = {
        "when probe\n  show \"value=\\u007b__jtml_scalar_00000000000000000001\\u007d\"\n",
        "let copy = \"value=\\u007btone\\u007d\"\n",
        "let copy = \"value=\\u007blater\\u007d\"\nlet later = \"late\"\n",
        "when probe\n  tone = \"value=\\u007bhidden\\u007d\"\n",
        "when probe tone\n  show \"value=\\u007btone\\u007d\"\n",
    };
    for (const auto& body : cases) {
        SCOPED_TRACE(body);
        const auto root = temporary();
        program(root, "let tone = \"entry\"\n" + body,
            "let tone = \"private\"\nlet hidden = \"private-only\"\n");
        const auto out = root / "out";
        fs::create_directories(out);
        write(out / "index.html", "previous successful output");
        const auto error = compile(root / "index.jtml", out);
        EXPECT_NE(error, "");
        EXPECT_NE(error.find("index.jtml:"), std::string::npos);
        EXPECT_EQ(read(out / "index.html"), "previous successful output");
        EXPECT_FALSE(fs::exists(out / "app.js"));
        EXPECT_FALSE(fs::exists(out / "jtml-runtime.js"));
    }
}


TEST(BrowserDerivedPublication, DependencyOrderIgnoresSourceAndJsonKeyOrder) {
    for (const bool reverse : {false, true}) {
        const auto root = temporary();
        const std::string declarations = reverse ? "get a = z * 2\nget z = x + 1\n" : "get z = x + 1\nget a = z * 2\n";
        write(root / "index.jtml", "jtml 2\nlet x = 1\n" + declarations + "page\n  text a\n");
        ASSERT_EQ(compile(root / "index.jtml", root / "out"), "");
        EXPECT_EQ(manifest(root / "out")["derivedOrder"], nlohmann::json({"z", "a"}));
        ASSERT_EQ(compile(root / "index.jtml", root / "compat", true), "");
        EXPECT_FALSE(manifest(root / "compat").contains("derivedOrder"));
    }
}

TEST(BrowserDerivedPublication, UnconsumedInvalidClosuresRejectBeforeWritingAssets) {
    for (const std::string formula : {"get a = b + 1\nget b = a + 1\n", "get a = missing + 1\n", "get a = mystery()\n"}) {
        const auto root = temporary();
        write(root / "index.jtml", "jtml 2\n" + formula + "page\n  text a\n");
        fs::create_directories(root / "out"); write(root / "out/index.html", "previous output");
        const auto error = compile(root / "index.jtml", root / "out");
        EXPECT_NE(error.find("index.jtml:"), std::string::npos) << error;
        EXPECT_EQ(read(root / "out/index.html"), "previous output");
        EXPECT_FALSE(fs::exists(root / "out/app.js"));
        EXPECT_FALSE(fs::exists(root / "out/jtml-runtime.js"));
    }
}

TEST(BrowserDerivedPublication, ClosedManifestRejectsIncompleteDuplicateAndUnknownOrders) {
    const auto root = temporary();
    write(root / "index.jtml", "jtml 2\nlet x = 1\nget z = x + 1\nget a = z * 2\npage\n  text a\n");
    ASSERT_EQ(compile(root / "index.jtml", root / "out"), "");
    const auto html = read(root / "out/index.html");
    const std::string marker = "id=\"__jtml_client_manifest\">";
    const auto begin = html.find(marker) + marker.size();
    const auto end = html.find("</script>", begin);
    for (const auto& order : {nlohmann::json::array(), nlohmann::json({"z", "z", "a"}),
                              nlohmann::json({"z", "unknown"}), nlohmann::json({"z", 1})}) {
        auto client = manifest(root / "out"); client["derivedOrder"] = order;
        write(root / "out/index.html", html.substr(0, begin) + client.dump() + html.substr(end));
        EXPECT_THROW(jtml::cli::validateBrowserBuildOutputSchema(root / "out", jtml::cli::BrowserBuildMode::Production), std::runtime_error);
        write(root / "out/index.html", html);
    }
}
