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
    for (const auto& order : {nlohmann::json::array(), nlohmann::json({"z"}), nlohmann::json({"a", "z"}), nlohmann::json({"z", "z", "a"}),
                              nlohmann::json({"z", "unknown"}), nlohmann::json({"z", 1})}) {
        auto client = manifest(root / "out"); client["derivedOrder"] = order;
        write(root / "out/index.html", html.substr(0, begin) + client.dump() + html.substr(end));
        EXPECT_THROW(jtml::cli::validateBrowserBuildOutputSchema(root / "out", jtml::cli::BrowserBuildMode::Production), std::runtime_error);
        write(root / "out/index.html", html);
    }
    auto missing = manifest(root / "out"); missing.erase("derivedOrder");
    write(root / "out/index.html", html.substr(0, begin) + missing.dump() + html.substr(end));
    EXPECT_THROW(jtml::cli::validateBrowserBuildOutputSchema(root / "out", jtml::cli::BrowserBuildMode::Production), std::runtime_error);
}

TEST(BrowserDerivedPublication, ImportedStoreIdentityPreservesPublicationNotScalarActionAuthority) {
    const auto root = temporary();
    const auto source = fs::path(JTML_SOURCE_DIR) / "tests/fixtures/production/imported-store-getter/index.jtml";
    ASSERT_EQ(compile(source, root / "positive"), "");
    EXPECT_EQ(manifest(root / "positive")["derivedOrder"], nlohmann::json({"label"}));
    write(root / "private.jtml", "jtml 2\nexport store shop\n  let x = 1\n  when step\n    x = x + 1\nexport make Card\n  text \"Card\"\n");
    write(root / "index.jtml", "jtml 2\nuse shop from \"./private.jtml\"\nget x = label + 1\nget label = shop.x + 1\npage\n  text x\n");
    ASSERT_EQ(compile(root / "index.jtml", root / "member-leaf"), "");
    EXPECT_EQ(manifest(root / "member-leaf")["derivedOrder"], nlohmann::json({"label", "x"}));
    for (const auto& body : {std::string("use Card from \"./private.jtml\"\nget label = shop.x + 1\n"),
         std::string("use shop from \"./private.jtml\"\nlet saved = 0\nget label = shop.x + 1\nwhen read\n  saved = label\n")}) {
        write(root / "index.jtml", "jtml 2\n" + body + "page\n  text \"Control\"\n");
        fs::create_directories(root / "negative"); write(root / "negative/index.html", "previous output");
        const auto error = compile(root / "index.jtml", root / "negative");
        EXPECT_NE(error.find("binding `shop`"), std::string::npos) << error;
        EXPECT_EQ(read(root / "negative/index.html"), "previous output");
        EXPECT_FALSE(fs::exists(root / "negative/app.js"));
    }
}

TEST(BrowserDerivedPublication, OrderValidationUsesStructuredEdgesIncludingInactiveBranches) {
    const auto root = temporary();
    write(root / "index.jtml", "jtml 2\nlet x = 1\nlet flag = false\nget a = flag ? z : w\nget z = x + 1\nget w = x + 2\nget other = {kind: \"path\", segments: [\"a\"]}\npage\n  text a\n");
    ASSERT_EQ(compile(root / "index.jtml", root / "out"), "");
    const auto html = read(root / "out/index.html");
    const std::string marker = "id=\"__jtml_client_manifest\">";
    const auto begin = html.find(marker) + marker.size(), end = html.find("</script>", begin);
    auto client = manifest(root / "out");
    client["derivedOrder"] = {"other", "w", "z", "a"}; // A different valid permutation.
    // Literal application data is not an expression node or dependency.
    client["derivedPlans"]["other"] = {{"kind", "literal"}, {"value", {{"kind", "path"}, {"segments", {"a"}}}}};
    auto check = [&]() { write(root / "out/index.html", html.substr(0, begin) + client.dump() + html.substr(end));
        jtml::cli::validateBrowserBuildOutputSchema(root / "out", jtml::cli::BrowserBuildMode::Production); };
    EXPECT_NO_THROW(check());
    client["derivedOrder"] = {"other", "w", "a", "z"};
    EXPECT_THROW(check(), std::runtime_error); // z is required even with flag=false.
    client["derivedOrder"] = {"other", "w", "z", "a"};
    client["derivedPlans"]["a"]["whenFalse"]["segments"] = {"a"};
    EXPECT_THROW(check(), std::runtime_error); // Inspect executed segments, not source/root hints.
    client["derivedPlans"]["a"]["whenFalse"]["segments"] = {"notOwned"};
    EXPECT_THROW(check(), std::runtime_error);
    client["derivedPlans"]["a"].erase("whenFalse");
    EXPECT_THROW(check(), std::runtime_error); // No hidden fallback from a missing child.
    for (const bool linked : {true, false}) {
        client = nlohmann::json::parse(html.substr(begin, end - begin));
        auto& projection = linked ? client["project"]["linkedPlan"] : client["project"]["modules"][0]["plan"];
        if (!linked) client["project"]["modules"][0].erase("executable"); // Runtime default is executable.
        projection["derived"]["extra"] = client["derived"]["a"];
        projection["derivedPlans"]["extra"] = client["derivedPlans"]["a"];
        EXPECT_THROW(check(), std::runtime_error); // Missing executable getter.
        client["derivedOrder"] = {"other", "w", "z", "extra", "a"};
        EXPECT_NO_THROW(check());
        client["derivedOrder"] = {"extra", "other", "w", "z", "a"};
        EXPECT_THROW(check(), std::runtime_error); // Reversed edge in that projection.
        if (!linked) {
            client["project"]["modules"][0]["executable"] = false;
            client["derivedOrder"] = {"other", "w", "z", "a"};
            EXPECT_NO_THROW(check()); // Explicitly disabled modules contribute no getters.
        }
    }
}


TEST(BrowserMutableInitialization, SourceOrderIsExplicitAndIndependentOfJsonKeys) {
    const auto root = temporary();
    write(root / "index.jtml", "jtml 2\nlet z = 2\nlet a = z + 1\nget result = a + z\npage\n  text result\n");
    ASSERT_EQ(compile(root / "index.jtml", root / "out"), "");
    EXPECT_EQ(manifest(root / "out")["stateOrder"], nlohmann::json({"z", "a"}));
    EXPECT_EQ(manifest(root / "out")["derivedOrder"], nlohmann::json({"result"}));
    ASSERT_EQ(compile(root / "index.jtml", root / "compat", true), "");
    EXPECT_FALSE(manifest(root / "compat").contains("stateOrder"));
}

TEST(BrowserMutableInitialization, RejectionIsSourceLocatedAndPreservesEveryAsset) {
    const auto root = temporary();
    write(root / "index.jtml", "jtml 2\nlet z = 2\nlet a = z + 1\npage\n  text a\n");
    ASSERT_EQ(compile(root / "index.jtml", root / "out"), "");
    const std::vector<std::string> names = {"index.html", "app.js", "jtml-runtime.js", "components/index.js"};
    std::vector<std::string> previous; for (const auto& name : names) previous.push_back(read(root / "out" / name));
    for (const auto& item : std::vector<std::pair<std::string, std::string>>{
        {"let first = later + 1\nlet later = 2\n", "E_INIT_FORWARD"},
        {"let self = self + 1\n", "E_INIT_FORWARD"},
        {"get derived = 2\nlet x = derived\n", "E_INIT_DERIVED"},
        {"let x = mystery()\n", "E_INIT_UNSUPPORTED"},
        {"let x = unknown\n", "E_INIT_UNRESOLVED"}}) {
        write(root / "index.jtml", "jtml 2\n" + item.first + "page\n  text \"Never\"\n");
        const auto error = compile(root / "index.jtml", root / "out");
        EXPECT_NE(error.find(item.second), std::string::npos) << error;
        EXPECT_NE(error.find("index.jtml:"), std::string::npos) << error;
        for (size_t i = 0; i < names.size(); ++i) EXPECT_EQ(read(root / "out" / names[i]), previous[i]);
    }
}

TEST(BrowserMutableInitialization, ForeignOwnersAreOrderedAndRepeatedSlotsKeepTheirBoundary) {
    const auto root = temporary();
    write(root / "private.jtml", "jtml 2\nlet dummy = 1\nexport let aaSeed = 7\n");
    write(root / "index.jtml", "jtml 2\nuse aaSeed from \"./private.jtml\"\nlet zzCopy = aaSeed + 1\npage\n  text zzCopy\n");
    ASSERT_EQ(compile(root / "index.jtml", root / "foreign"), "");
    EXPECT_EQ(manifest(root / "foreign")["stateOrder"], nlohmann::json({"dummy", "aaSeed", "zzCopy"}));
    write(root / "private.jtml", "jtml 2\nlet aaSeed = 7\nexport make Card\n  text \"Card\"\n");
    write(root / "index.jtml", "jtml 2\nuse \"./private.jtml\"\nlet zzCopy = aaSeed + 1\npage\n  text zzCopy\n");
    ASSERT_EQ(compile(root / "index.jtml", root / "side-effect"), "");
    EXPECT_EQ(manifest(root / "side-effect")["stateOrder"], nlohmann::json({"aaSeed", "zzCopy"}));
    write(root / "index.jtml", "jtml 2\nuse Card from \"./private.jtml\"\nlet zzCopy = aaSeed + 1\npage\n  Card\n");
    EXPECT_NE(compile(root / "index.jtml", root / "unimported").find("E_INIT_UNRESOLVED"), std::string::npos);
    write(root / "index.jtml", "jtml 2\nlet slot = {}\nlet slot = {ready: true}\npage\n  text \"Ready\"\n");
    ASSERT_EQ(compile(root / "index.jtml", root / "lowered"), "");
    EXPECT_TRUE(manifest(root / "lowered")["stateOrder"].is_null());
}

TEST(BrowserMutableInitialization, InvalidCertificatesCannotClaimOrderedInitialization) {
    const auto root = temporary();
    write(root / "index.jtml", "jtml 2\nlet z = 2\nlet a = z + 1\npage\n  text a\n");
    ASSERT_EQ(compile(root / "index.jtml", root / "out"), "");
    const auto html = read(root / "out/index.html");
    const std::string marker = "id=\"__jtml_client_manifest\">";
    const auto begin = html.find(marker) + marker.size(), end = html.find("</script>", begin);
    for (const auto& order : {nlohmann::json::array(), nlohmann::json({"a", "z"}), nlohmann::json({"z", "z", "a"}), nlohmann::json({"z"}), nlohmann::json({"z", "unknown"})}) {
        auto client = nlohmann::json::parse(html.substr(begin, end - begin)); client["stateOrder"] = order;
        write(root / "out/index.html", html.substr(0, begin) + client.dump() + html.substr(end));
        EXPECT_THROW(jtml::cli::validateBrowserBuildOutputSchema(root / "out", jtml::cli::BrowserBuildMode::Production), std::runtime_error);
    }
    auto missing = nlohmann::json::parse(html.substr(begin, end - begin)); missing.erase("stateOrder");
    write(root / "out/index.html", html.substr(0, begin) + missing.dump() + html.substr(end));
    EXPECT_THROW(jtml::cli::validateBrowserBuildOutputSchema(root / "out", jtml::cli::BrowserBuildMode::Production), std::runtime_error);
}

TEST(BrowserMutableInitialization, EffectiveAndProjectedInitializerPlansCannotHideDependencies) {
    const auto root = temporary();
    const auto vectors = nlohmann::json::parse(read(fs::path(JTML_SOURCE_DIR) / "docs/spec/reactive-core-v1/vectors.json"));
    for (const auto& vector : vectors.at("vectors")) if (vector.at("id") == "V11")
        write(root / "index.jtml", vector.at("files").at("index.jtml").get<std::string>());
    ASSERT_EQ(compile(root / "index.jtml", root / "out"), "");
    const auto html = read(root / "out/index.html");
    const std::string marker = "id=\"__jtml_client_manifest\">";
    const auto begin = html.find(marker) + marker.size(), end = html.find("</script>", begin);
    const auto original = nlohmann::json::parse(html.substr(begin, end - begin));
    auto reject = [&](const nlohmann::json& client) {
        write(root / "out/index.html", html.substr(0, begin) + client.dump() + html.substr(end));
        EXPECT_THROW(jtml::cli::validateBrowserBuildOutputSchema(root / "out", jtml::cli::BrowserBuildMode::Production), std::runtime_error);
    };
    // Exact adversarial mutation: browser keeps root statePlans.a, although
    // state.a is supplied by a module with a contradictory, shadowed plan.
    auto client = original;
    client["stateOrder"] = {"a", "z"}; client["state"].erase("a");
    client["project"]["modules"][0]["plan"]["statePlans"]["a"] = {{"kind", "literal"}, {"value", 3}};
    reject(client);
    client = original;
    client["statePlans"]["orphan"] = {{"kind", "literal"}, {"value", 3}};
    reject(client);
    // All declaration projections still matter, including ones that lose the
    // browser's first-wins merge. Do not certify a contradictory dependency.
    const auto lateRead = nlohmann::json{{"kind", "path"}, {"segments", {"a"}}};
    client = original;
    client["project"]["modules"][0]["plan"]["statePlans"]["z"] = lateRead;
    reject(client);
    client = original;
    client["project"]["linkedPlan"]["state"]["z"] = original["state"]["z"];
    client["project"]["linkedPlan"]["statePlans"]["z"] = lateRead;
    reject(client);
    client = original;
    client["project"]["linkedPlan"]["statePlans"]["orphan"] = {{"kind", "literal"}, {"value", 3}};
    reject(client);
    client = original;
    auto duplicate = client["project"]["modules"][0];
    duplicate["id"] = 999;
    client["project"]["modules"].push_back(duplicate);
    reject(client);
}

TEST(BrowserMutableInitialization, LegitimateDuplicateAndInactiveProjectionsRemainValid) {
    const auto root = temporary();
    write(root / "index.jtml", "jtml 2\nlet z = 2\nlet a = z + 1\npage\n  text a\n");
    ASSERT_EQ(compile(root / "index.jtml", root / "out"), "");
    const auto html = read(root / "out/index.html");
    const std::string marker = "id=\"__jtml_client_manifest\">";
    const auto begin = html.find(marker) + marker.size(), end = html.find("</script>", begin);
    auto client = nlohmann::json::parse(html.substr(begin, end - begin));
    ASSERT_EQ(client["state"], client["project"]["modules"][0]["plan"]["state"]);
    ASSERT_EQ(client["statePlans"], client["project"]["modules"][0]["plan"]["statePlans"]);
    EXPECT_NO_THROW(jtml::cli::validateBrowserBuildOutputSchema(root / "out", jtml::cli::BrowserBuildMode::Production));
    // Redundant linked declarations are legal; identical keys are not orphans.
    client["project"]["linkedPlan"]["state"] = client["state"];
    client["project"]["linkedPlan"]["statePlans"] = client["statePlans"];
    auto inactive = client["project"]["modules"][0]; inactive["id"] = 999; inactive["executable"] = false;
    inactive["plan"]["statePlans"]["orphan"] = {{"kind", "literal"}, {"value", 3}};
    inactive["plan"]["statePlans"]["z"] = {{"kind", "path"}, {"segments", {"a"}}};
    client["project"]["modules"].push_back(inactive);
    write(root / "out/index.html", html.substr(0, begin) + client.dump() + html.substr(end));
    EXPECT_NO_THROW(jtml::cli::validateBrowserBuildOutputSchema(root / "out", jtml::cli::BrowserBuildMode::Production));
}

TEST(BrowserMutableInitialization, BarrelStoreInitializersUseDefiningOwner) {
    const auto root = temporary();
    write(root / "private.jtml", "jtml 2\nexport store shop\n  let items = [{x: 7}]\n");
    write(root / "barrel.jtml", "jtml 2\nexport use shop from \"./private.jtml\"\n");
    write(root / "index.jtml", "jtml 2\nuse shop from \"./barrel.jtml\"\nlet zzCopy = shop.items[0].x + 1\npage\n  text zzCopy\n");
    ASSERT_EQ(compile(root / "index.jtml", root / "out"), "");
    EXPECT_EQ(manifest(root / "out")["stateOrder"], nlohmann::json({"shop", "zzCopy"}));
    EXPECT_NO_THROW(jtml::cli::validateBrowserBuildOutputSchema(root / "out", jtml::cli::BrowserBuildMode::Production));
}

TEST(BrowserMutableInitialization, ImportGraphRejectsCyclesAndMissingEdges) {
    jtml::RuntimeProjectPlan project;
    project.entry = 0;
    project.modules.resize(3);
    for (size_t i = 0; i < project.modules.size(); ++i) {
        auto& module = project.modules[i];
        module.id = i;
        module.path = "module-" + std::to_string(i);
        module.clientExecutable = true;
        module.plan.state.push_back({"slot" + std::to_string(i), "1", {}});
    }
    project.modules[0].initializationImports = {1};
    // The disconnected module is still executable, so it must be initialized.
    ASSERT_NO_THROW(jtml::cli::validateBrowserRuntimeBindingOwnership(project));
    ASSERT_TRUE(project.stateOrder);
    EXPECT_EQ(*project.stateOrder, (std::vector<std::string>{"slot1", "slot0", "slot2"}));

    auto diagnostic = [](jtml::RuntimeProjectPlan& plan) {
        try { jtml::cli::validateBrowserRuntimeBindingOwnership(plan); }
        catch (const std::runtime_error& error) { return std::string(error.what()); }
        return std::string{};
    };
    project.modules[1].initializationImports = {0};
    EXPECT_NE(diagnostic(project).find("E_MODULE_CYCLE"), std::string::npos);
    project.modules[1].initializationImports = {42};
    EXPECT_NE(diagnostic(project).find("E_INIT_UNRESOLVED"), std::string::npos);
}
