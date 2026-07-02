#include "cli/commands.h"
#include "json.hpp"

#include <gtest/gtest.h>

#include <algorithm>
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

std::filesystem::path makeTempDir(const std::string& name) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto root = std::filesystem::temp_directory_path() /
                ("jtml-cli-" + name + "-" + std::to_string(stamp));
    std::filesystem::create_directories(root);
    return root;
}

void writeTempFile(const std::filesystem::path& path, const std::string& source) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path);
    out << source;
}

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream input(path);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

size_t countOccurrences(const std::string& haystack, const std::string& needle) {
    size_t count = 0;
    size_t pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

nlohmann::json extractClientManifestJson(const std::string& html) {
    const std::string open = "<script type=\"application/json\" id=\"__jtml_client_manifest\">";
    const std::string close = "</script>";
    const auto start = html.find(open);
    EXPECT_NE(start, std::string::npos) << html;
    const auto end = html.find(close, start);
    EXPECT_NE(end, std::string::npos) << html;
    return nlohmann::json::parse(
        html.substr(start + open.size(), end - (start + open.size())));
}

std::string extractClientManifestText(const std::string& html) {
    const std::string open = "<script type=\"application/json\" id=\"__jtml_client_manifest\">";
    const std::string close = "</script>";
    const auto start = html.find(open);
    EXPECT_NE(start, std::string::npos) << html;
    const auto end = html.find(close, start);
    EXPECT_NE(end, std::string::npos) << html;
    return html.substr(start + open.size(), end - (start + open.size()));
}

struct ScopedCurrentPath {
    std::filesystem::path previous = std::filesystem::current_path();

    explicit ScopedCurrentPath(const std::filesystem::path& next) {
        std::filesystem::current_path(next);
    }

    ~ScopedCurrentPath() {
        std::filesystem::current_path(previous);
    }
};

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

TEST(CliModules, NestedRelativeImportsResolveFromImporterForCoreCommands) {
    const auto root = makeTempDir("nested-modules");
    const auto app = root / "opspulse";

    writeTempFile(app / "components" / "ui" / "card.jtml",
        "jtml 2\n"
        "export make Card title\n"
        "  box class \"card\"\n"
        "    h2 title\n"
        "    text \"Reusable card\"\n");
    writeTempFile(app / "stores" / "app-state.jtml",
        "jtml 2\n"
        "use Card from \"../components/ui/card.jtml\"\n"
        "\n"
        "store appState\n"
        "  let user = \"Ada\"\n"
        "  when clearUser\n"
        "    user = \"\"\n");
    writeTempFile(app / "pages" / "home.jtml",
        "jtml 2\n"
        "use \"../stores/app-state.jtml\"\n"
        "\n"
        "make Home\n"
        "  page\n"
        "    Card \"Welcome\"\n"
        "    text \"User: {appState.user}\"\n"
        "    button \"Clear\" click appState.clearUser\n");
    writeTempFile(app / "index.jtml",
        "jtml 2\n"
        "use \"./stores/app-state.jtml\"\n"
        "use \"./pages/home.jtml\"\n"
        "\n"
        "page\n"
        "  Home\n");

    jtml::cli::Options check;
    check.inputFile = (app / "index.jtml").string();
    check.syntax = jtml::SyntaxMode::Auto;
    const auto checked = captureCommand([&] { return jtml::cli::cmdCheck(check); });
    ASSERT_EQ(checked.code, 0) << checked.out << checked.err;

    jtml::cli::Options build;
    build.inputFile = (app / "index.jtml").string();
    build.outputFile = (root / "dist").string();
    build.target = "browser";
    build.syntax = jtml::SyntaxMode::Auto;
    const auto built = captureCommand([&] { return jtml::cli::cmdBuild(build); });
    ASSERT_EQ(built.code, 0) << built.out << built.err;
    EXPECT_TRUE(std::filesystem::exists(root / "dist" / "index.html"));

    jtml::cli::Options explain;
    explain.inputFile = (app / "index.jtml").string();
    explain.syntax = jtml::SyntaxMode::Auto;
    explain.json = true;
    const auto explained = captureCommand([&] { return jtml::cli::cmdExplain(explain); });
    ASSERT_EQ(explained.code, 0) << explained.out << explained.err;
    const auto report = nlohmann::json::parse(explained.out);
    EXPECT_EQ(report["ok"], true) << report.dump(2);
    EXPECT_GE(report["semantic"]["nodes"]["components"].get<int>(), 1);
    EXPECT_GE(report["semantic"]["nodes"]["moduleFiles"].get<int>(), 4);
    EXPECT_NE(report["semantic"]["moduleFiles"].dump().find("stores/app-state.jtml"), std::string::npos)
        << report.dump(2);
    EXPECT_NE(report["state"].dump().find("appState"), std::string::npos)
        << report.dump(2);
    EXPECT_NE(report["actions"].dump().find("appState_clearUser"), std::string::npos)
        << report.dump(2);

    const auto files = jtml::cli::collectSourceFiles(
        (app / "index.jtml").string(), jtml::SyntaxMode::Auto);
    auto contains = [&](const std::filesystem::path& expected) {
        const auto canonical = std::filesystem::weakly_canonical(expected);
        return std::find(files.begin(), files.end(), canonical) != files.end();
    };
    EXPECT_TRUE(contains(app / "index.jtml"));
    EXPECT_TRUE(contains(app / "stores" / "app-state.jtml"));
    EXPECT_TRUE(contains(app / "components" / "ui" / "card.jtml"));
    EXPECT_TRUE(contains(app / "pages" / "home.jtml"));
}

TEST(CliModules, MissingImportDiagnosticsNameImporterAndResolvedPath) {
    const auto root = makeTempDir("missing-import");
    const auto app = root / "opspulse";
    writeTempFile(app / "index.jtml",
        "jtml 2\n"
        "use \"./stores/missing.jtml\"\n"
        "page\n"
        "  h1 \"Broken import\"\n");

    jtml::cli::Options opts;
    opts.inputFile = (app / "index.jtml").string();
    opts.syntax = jtml::SyntaxMode::Auto;

    const auto result = captureCommand([&] { return jtml::cli::cmdCheck(opts); });
    EXPECT_EQ(result.code, 1);
    EXPECT_NE(result.err.find("Cannot resolve import './stores/missing.jtml'"), std::string::npos)
        << result.err;
    EXPECT_NE(result.err.find((app / "index.jtml").string()), std::string::npos)
        << result.err;
    EXPECT_NE(result.err.find((app / "stores" / "missing.jtml").string()), std::string::npos)
        << result.err;
}

TEST(CliModules, NamedImportsEmitPrivateImplementationOnlyOncePerFile) {
    const auto root = makeTempDir("named-import-private-once");
    const auto app = root / "app";
    writeTempFile(app / "components" / "common.jtml",
        "jtml 2\n"
        "let dismissed = false\n"
        "when dismissShared\n"
        "  dismissed = true\n"
        "\n"
        "export make AlertOne\n"
        "  button \"Dismiss one\" click dismissShared\n"
        "  text dismissed\n"
        "\n"
        "export make AlertTwo\n"
        "  button \"Dismiss two\" click dismissShared\n"
        "  text dismissed\n");
    writeTempFile(app / "index.jtml",
        "jtml 2\n"
        "use AlertOne from \"./components/common.jtml\"\n"
        "use AlertTwo from \"./components/common.jtml\"\n"
        "page\n"
        "  AlertOne\n"
        "  AlertTwo\n");

    jtml::cli::Options opts;
    opts.inputFile = (app / "index.jtml").string();
    opts.syntax = jtml::SyntaxMode::Auto;

    const auto result = captureCommand([&] { return jtml::cli::cmdCheck(opts); });
    EXPECT_EQ(result.code, 0) << result.out << result.err;
    EXPECT_EQ(result.err.find("Function already defined"), std::string::npos)
        << result.err;
}

TEST(CliModules, NamedImportRequiresExportedDeclaration) {
    const auto root = makeTempDir("named-import-no-export");
    const auto app = root / "app";
    writeTempFile(app / "components" / "card.jtml",
        "jtml 2\n"
        "make Card title\n"
        "  h2 title\n");
    writeTempFile(app / "index.jtml",
        "jtml 2\n"
        "use Card from \"./components/card.jtml\"\n"
        "page\n"
        "  Card \"Hello\"\n");

    jtml::cli::Options opts;
    opts.inputFile = (app / "index.jtml").string();
    opts.syntax = jtml::SyntaxMode::Auto;

    const auto result = captureCommand([&] { return jtml::cli::cmdCheck(opts); });
    EXPECT_EQ(result.code, 1);
    EXPECT_NE(result.err.find("Named import from"), std::string::npos) << result.err;
    EXPECT_NE(result.err.find("requires exported declarations"), std::string::npos)
        << result.err;
    EXPECT_NE(result.err.find("Card"), std::string::npos) << result.err;
}

TEST(CliModules, MissingNamedExportReportsAvailableExports) {
    const auto root = makeTempDir("missing-export");
    const auto app = root / "app";
    writeTempFile(app / "components" / "card.jtml",
        "jtml 2\n"
        "export make Card title\n"
        "  h2 title\n");
    writeTempFile(app / "index.jtml",
        "jtml 2\n"
        "use Panel from \"./components/card.jtml\"\n"
        "page\n"
        "  Panel \"Hello\"\n");

    jtml::cli::Options opts;
    opts.inputFile = (app / "index.jtml").string();
    opts.syntax = jtml::SyntaxMode::Auto;

    const auto result = captureCommand([&] { return jtml::cli::cmdCheck(opts); });
    EXPECT_EQ(result.code, 1);
    EXPECT_NE(result.err.find("Missing export(s)"), std::string::npos) << result.err;
    EXPECT_NE(result.err.find("Panel"), std::string::npos) << result.err;
    EXPECT_NE(result.err.find("Available exports: Card"), std::string::npos) << result.err;
}

TEST(CliModules, DuplicateExportedSymbolIsDiagnostic) {
    const auto root = makeTempDir("duplicate-export");
    const auto app = root / "app";
    writeTempFile(app / "components" / "cards.jtml",
        "jtml 2\n"
        "export make Card title\n"
        "  h2 title\n"
        "export make Card title\n"
        "  h2 title\n");
    writeTempFile(app / "index.jtml",
        "jtml 2\n"
        "use Card from \"./components/cards.jtml\"\n"
        "page\n"
        "  Card \"Hello\"\n");

    jtml::cli::Options opts;
    opts.inputFile = (app / "index.jtml").string();
    opts.syntax = jtml::SyntaxMode::Auto;

    const auto result = captureCommand([&] { return jtml::cli::cmdCheck(opts); });
    EXPECT_EQ(result.code, 1);
    EXPECT_NE(result.err.find("Duplicate exported symbol 'Card'"), std::string::npos)
        << result.err;
}

TEST(CliModules, NamedImportDoesNotExposePrivateDeclarations) {
    const auto root = makeTempDir("private-module-decl");
    const auto app = root / "app";
    writeTempFile(app / "components" / "card.jtml",
        "jtml 2\n"
        "export make Card title\n"
        "  h2 title\n"
        "make InternalCard\n"
        "  text \"Private\"\n");
    writeTempFile(app / "index.jtml",
        "jtml 2\n"
        "use Card from \"./components/card.jtml\"\n"
        "page\n"
        "  InternalCard\n");

    jtml::cli::Options opts;
    opts.inputFile = (app / "index.jtml").string();
    opts.syntax = jtml::SyntaxMode::Auto;

    const auto result = captureCommand([&] { return jtml::cli::cmdCheck(opts); });
    EXPECT_EQ(result.code, 1);
    EXPECT_NE(result.err.find("InternalCard"), std::string::npos) << result.err;
}

TEST(CliModules, NamedImportKeepsPrivateImplementationState) {
    const auto root = makeTempDir("private-module-helper");
    const auto app = root / "app";
    writeTempFile(app / "components" / "card.jtml",
        "jtml 2\n"
        "let cardTone = \"primary\"\n"
        "export make Card title\n"
        "  box class cardTone\n"
        "    h2 title\n");
    writeTempFile(app / "index.jtml",
        "jtml 2\n"
        "use Card from \"./components/card.jtml\"\n"
        "page\n"
        "  Card \"Hello\"\n");

    jtml::cli::Options opts;
    opts.inputFile = (app / "index.jtml").string();
    opts.syntax = jtml::SyntaxMode::Auto;

    const auto result = captureCommand([&] { return jtml::cli::cmdCheck(opts); });
    EXPECT_EQ(result.code, 0) << result.out << result.err;
}

TEST(CliModules, NamedImportCanUseReExportedDeclaration) {
    const auto root = makeTempDir("reexport-module");
    const auto app = root / "app";
    writeTempFile(app / "components" / "card.jtml",
        "jtml 2\n"
        "export make Card title\n"
        "  h2 title\n");
    writeTempFile(app / "components" / "index.jtml",
        "jtml 2\n"
        "export use Card from \"./card.jtml\"\n");
    writeTempFile(app / "index.jtml",
        "jtml 2\n"
        "use Card from \"./components/index.jtml\"\n"
        "page\n"
        "  Card \"Hello\"\n");

    jtml::cli::Options check;
    check.inputFile = (app / "index.jtml").string();
    check.syntax = jtml::SyntaxMode::Auto;
    const auto checked = captureCommand([&] { return jtml::cli::cmdCheck(check); });
    ASSERT_EQ(checked.code, 0) << checked.out << checked.err;

    const auto files = jtml::cli::collectSourceFiles(
        (app / "index.jtml").string(), jtml::SyntaxMode::Auto);
    auto contains = [&](const std::filesystem::path& expected) {
        const auto canonical = std::filesystem::weakly_canonical(expected);
        return std::find(files.begin(), files.end(), canonical) != files.end();
    };
    EXPECT_TRUE(contains(app / "index.jtml"));
    EXPECT_TRUE(contains(app / "components" / "index.jtml"));
    EXPECT_TRUE(contains(app / "components" / "card.jtml"));

    jtml::cli::Options explain;
    explain.inputFile = (app / "index.jtml").string();
    explain.syntax = jtml::SyntaxMode::Auto;
    explain.json = true;
    const auto explained = captureCommand([&] { return jtml::cli::cmdExplain(explain); });
    ASSERT_EQ(explained.code, 0) << explained.out << explained.err;
    const auto report = nlohmann::json::parse(explained.out);
    const auto& modules = report["semantic"]["project"]["modules"];
    ASSERT_EQ(modules.size(), 3u) << report.dump(2);
    ASSERT_EQ(modules[0]["imports"][0]["resolvedSymbols"].size(), 1u) << report.dump(2);
    EXPECT_EQ(modules[0]["imports"][0]["resolvedSymbols"][0]["name"], "Card");
    EXPECT_EQ(modules[0]["imports"][0]["resolvedSymbols"][0]["kind"], "make");
    EXPECT_EQ(modules[0]["imports"][0]["resolvedSymbols"][0]["module"], modules[2]["id"]);
}

TEST(CliPackages, AddWritesManifestLockfileAndInstallVerifies) {
    const auto root = makeTempDir("package-install");
    const auto packageDir = root / "source" / "ui-kit";
    writeTempFile(packageDir / "index.jtml",
        "jtml 2\n"
        "export make Badge label\n"
        "  badge label tone primary\n");

    ScopedCurrentPath cwd(root);

    jtml::cli::Options add;
    add.inputFile = packageDir.string();
    add.syntax = jtml::SyntaxMode::Auto;
    const auto added = captureCommand([&] { return jtml::cli::cmdAdd(add); });
    ASSERT_EQ(added.code, 0) << added.out << added.err;
    EXPECT_TRUE(std::filesystem::exists(root / "jtml_modules" / "ui-kit" / "index.jtml"));
    EXPECT_TRUE(std::filesystem::exists(root / "jtml.packages.json"));
    EXPECT_TRUE(std::filesystem::exists(root / "jtml.lock.json"));

    jtml::cli::Options install;
    install.json = true;
    const auto installed = captureCommand([&] { return jtml::cli::cmdInstall(install); });
    ASSERT_EQ(installed.code, 0) << installed.out << installed.err;
    const auto report = nlohmann::json::parse(installed.out);
    EXPECT_EQ(report["ok"], true) << report.dump(2);
    ASSERT_TRUE(report["packages"].is_array()) << report.dump(2);
    ASSERT_EQ(report["packages"].size(), 1) << report.dump(2);
    EXPECT_EQ(report["packages"][0]["name"], "ui-kit") << report.dump(2);
    EXPECT_EQ(report["packages"][0]["ok"], true) << report.dump(2);
}

TEST(CliExplain, StoreActionUnaryReadsAreSemanticStoreMembers) {
    const auto file = writeTempJtml(
        "store-unary-read",
        "jtml 2\n"
        "store appState\n"
        "  let darkMode = false\n"
        "  get themeClass = darkMode ? \"dark\" : \"light\"\n"
        "  when toggleTheme\n"
        "    darkMode = !darkMode\n"
        "page\n"
        "  main class appState.themeClass\n"
        "    button \"Theme\" click appState.toggleTheme\n");

    jtml::cli::Options opts;
    opts.inputFile = file.string();
    opts.syntax = jtml::SyntaxMode::Auto;
    opts.json = true;

    const auto explained = captureCommand([&] { return jtml::cli::cmdExplain(opts); });
    ASSERT_EQ(explained.code, 0) << explained.out << explained.err;
    const auto report = nlohmann::json::parse(explained.out);
    EXPECT_EQ(report["ok"], true) << report.dump(2);
    EXPECT_EQ(report["diagnostics"].size(), 0) << report.dump(2);
    EXPECT_NE(report["actionProfiles"].dump().find("darkMode"), std::string::npos)
        << report.dump(2);
}

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

TEST(CliExplain, JsonProjectGraphPreservesPerFileImporterOwnership) {
    const auto root = makeTempDir("explain-module-graph");
    writeTempFile(root / "stores" / "app-state.jtml",
        "jtml 2\n"
        "export store appState\n"
        "  let user = \"Ada\"\n");
    writeTempFile(root / "pages" / "dashboard.jtml",
        "jtml 2\n"
        "use appState from \"../stores/app-state.jtml\"\n"
        "export make Dashboard\n"
        "  page\n"
        "    text appState.user\n");
    writeTempFile(root / "index.jtml",
        "jtml 2\n"
        "use Dashboard from \"./pages/dashboard.jtml\"\n"
        "page\n"
        "  Dashboard\n");

    jtml::cli::Options opts;
    opts.inputFile = (root / "index.jtml").string();
    opts.syntax = jtml::SyntaxMode::Auto;
    opts.json = true;

    const auto result = captureCommand([&] { return jtml::cli::cmdExplain(opts); });
    ASSERT_EQ(result.code, 0) << result.out << result.err;
    const auto report = nlohmann::json::parse(result.out);
    const auto& modules = report["semantic"]["project"]["modules"];
    ASSERT_EQ(modules.size(), 3u) << report.dump(2);
    ASSERT_TRUE(report["semantic"]["project"].contains("issues")) << report.dump(2);
    EXPECT_TRUE(report["semantic"]["project"]["issues"].empty()) << report.dump(2);
    ASSERT_EQ(modules[0]["imports"].size(), 1u) << report.dump(2);
    ASSERT_EQ(modules[1]["imports"].size(), 1u) << report.dump(2);
    ASSERT_EQ(modules[2]["exports"].size(), 1u) << report.dump(2);
    EXPECT_TRUE(modules[0]["semantic"]["components"].empty()) << report.dump(2);
    EXPECT_EQ(modules[0]["imports"][0]["importer"], modules[0]["id"]);
    EXPECT_EQ(modules[0]["imports"][0]["resolved"], modules[1]["id"]);
    ASSERT_EQ(modules[0]["imports"][0]["resolvedSymbols"].size(), 1u) << report.dump(2);
    EXPECT_EQ(modules[0]["imports"][0]["resolvedSymbols"][0]["name"], "Dashboard");
    EXPECT_EQ(modules[0]["imports"][0]["resolvedSymbols"][0]["kind"], "make");
    EXPECT_EQ(modules[0]["imports"][0]["resolvedSymbols"][0]["module"], modules[1]["id"]);
    EXPECT_EQ(modules[0]["imports"][0]["span"]["line"], 2);
    EXPECT_EQ(modules[0]["imports"][0]["span"]["column"], 1);
    EXPECT_EQ(modules[1]["imports"][0]["importer"], modules[1]["id"]);
    EXPECT_EQ(modules[1]["imports"][0]["resolved"], modules[2]["id"]);
    ASSERT_EQ(modules[1]["imports"][0]["resolvedSymbols"].size(), 1u) << report.dump(2);
    EXPECT_EQ(modules[1]["imports"][0]["resolvedSymbols"][0]["name"], "appState");
    EXPECT_EQ(modules[1]["imports"][0]["resolvedSymbols"][0]["kind"], "store");
    EXPECT_EQ(modules[1]["imports"][0]["resolvedSymbols"][0]["module"], modules[2]["id"]);
    EXPECT_EQ(modules[1]["imports"][0]["span"]["line"], 2);
    EXPECT_EQ(modules[1]["imports"][0]["span"]["column"], 1);
    EXPECT_EQ(modules[2]["exports"][0]["name"], "appState");
    EXPECT_EQ(modules[2]["exports"][0]["kind"], "store");
    ASSERT_EQ(modules[1]["semantic"]["components"].size(), 1u) << report.dump(2);
    ASSERT_EQ(modules[2]["semantic"]["stores"].size(), 1u) << report.dump(2);
    EXPECT_EQ(modules[1]["semantic"]["components"][0], "Dashboard");
    EXPECT_EQ(modules[2]["semantic"]["stores"][0], "appState");
    ASSERT_TRUE(modules[0].contains("ir")) << report.dump(2);
    EXPECT_TRUE(modules[0]["ir"]["available"]) << report.dump(2);
    EXPECT_EQ(modules[0]["ir"]["syntax"], "friendly+import-stubs");
    EXPECT_GE(modules[0]["ir"]["totalNodeCount"].get<int>(), 1);
    EXPECT_NE(modules[0]["ir"]["topLevelNodes"].dump().find("ImportStatement"),
              std::string::npos) << report.dump(2);
    EXPECT_TRUE(modules[1]["ir"]["available"]) << report.dump(2);
    EXPECT_NE(modules[1]["ir"]["topLevelNodes"].dump().find("JtmlElement"),
              std::string::npos) << report.dump(2);
    EXPECT_NE(modules[1]["ir"]["nodeCounts"].dump().find("ImportStatement"),
              std::string::npos) << report.dump(2);
    EXPECT_TRUE(modules[2]["imports"].empty()) << report.dump(2);
    ASSERT_TRUE(report.contains("runtimeProjectPlan")) << report.dump(2);
    const auto& runtimeProject = report["runtimeProjectPlan"];
    EXPECT_EQ(runtimeProject["sourceOfTruth"], "SemanticProject retained per-file AST + semantic IR");
    EXPECT_EQ(runtimeProject["entry"], modules[0]["id"]);
    ASSERT_EQ(runtimeProject["modules"].size(), 3u) << report.dump(2);
    EXPECT_TRUE(runtimeProject["modules"][0]["astAvailable"]) << report.dump(2);
    EXPECT_TRUE(runtimeProject["modules"][1]["astAvailable"]) << report.dump(2);
    ASSERT_EQ(runtimeProject["modules"][1]["plan"]["componentDefinitions"].size(), 1u)
        << report.dump(2);
    EXPECT_EQ(runtimeProject["modules"][1]["plan"]["componentDefinitions"][0]["name"],
              "Dashboard");
    ASSERT_EQ(runtimeProject["modules"][2]["plan"]["semantic"]["stores"].size(), 1u)
        << report.dump(2);
    EXPECT_EQ(runtimeProject["modules"][2]["plan"]["semantic"]["stores"][0], "appState");
}

TEST(CliExplain, TextReportsSemanticProjectModulesImportsAndExports) {
    const auto root = makeTempDir("explain-module-text");
    writeTempFile(root / "stores" / "app-state.jtml",
        "jtml 2\n"
        "export store appState\n"
        "  let user = \"Ada\"\n");
    writeTempFile(root / "pages" / "dashboard.jtml",
        "jtml 2\n"
        "use appState from \"../stores/app-state.jtml\"\n"
        "export make Dashboard\n"
        "  page\n"
        "    text appState.user\n");
    writeTempFile(root / "index.jtml",
        "jtml 2\n"
        "use Dashboard from \"./pages/dashboard.jtml\"\n"
        "page\n"
        "  Dashboard\n");

    jtml::cli::Options opts;
    opts.inputFile = (root / "index.jtml").string();
    opts.syntax = jtml::SyntaxMode::Auto;

    const auto result = captureCommand([&] { return jtml::cli::cmdExplain(opts); });
    ASSERT_EQ(result.code, 0) << result.out << result.err;
    EXPECT_NE(result.out.find("Project modules (3):"), std::string::npos) << result.out;
    EXPECT_NE(result.out.find("[entry]"), std::string::npos) << result.out;
    EXPECT_NE(result.out.find("Dashboard from \"./pages/dashboard.jtml\""), std::string::npos)
        << result.out;
    EXPECT_NE(result.out.find("[make Dashboard]"), std::string::npos) << result.out;
    EXPECT_NE(result.out.find("appState from \"../stores/app-state.jtml\""), std::string::npos)
        << result.out;
    EXPECT_NE(result.out.find("[store appState]"), std::string::npos) << result.out;
    EXPECT_NE(result.out.find("exports: make Dashboard"), std::string::npos) << result.out;
    EXPECT_NE(result.out.find("exports: store appState"), std::string::npos) << result.out;
    EXPECT_NE(result.out.find("semantic: 1 component(s)"), std::string::npos) << result.out;
    EXPECT_NE(result.out.find("semantic: 1 store(s)"), std::string::npos) << result.out;
    EXPECT_NE(result.out.find("ir:"), std::string::npos) << result.out;
    EXPECT_NE(result.out.find("typed node"), std::string::npos) << result.out;
    EXPECT_NE(result.out.find("friendly+import-stubs"), std::string::npos) << result.out;
    EXPECT_NE(result.out.find("JtmlElement:template"), std::string::npos)
        << result.out;
    EXPECT_EQ(result.out.find("Project issues"), std::string::npos) << result.out;
}

TEST(CliExplain, JsonReportsRecoverableImportedModuleParseIssues) {
    const auto root = makeTempDir("explain-module-parse");
    writeTempFile(root / "broken.jtml",
        "jtml 2\n"
        "export make Broken\n"
        "  text \"unterminated\n");
    writeTempFile(root / "index.jtml",
        "jtml 2\n"
        "use \"./broken.jtml\"\n"
        "page\n"
        "  text \"Entry still explains\"\n");

    jtml::cli::Options opts;
    opts.inputFile = (root / "index.jtml").string();
    opts.syntax = jtml::SyntaxMode::Auto;
    opts.json = true;

    const auto result = captureCommand([&] { return jtml::cli::cmdExplain(opts); });
    ASSERT_EQ(result.code, 0) << result.out << result.err;
    const auto report = nlohmann::json::parse(result.out);
    const auto& project = report["semantic"]["project"];
    ASSERT_EQ(project["modules"].size(), 2u) << report.dump(2);

    const auto& broken = project["modules"][1];
    EXPECT_EQ(broken["path"], std::filesystem::weakly_canonical(root / "broken.jtml").generic_string())
        << report.dump(2);
    EXPECT_EQ(broken["ir"]["available"], false) << report.dump(2);
    EXPECT_FALSE(broken["ir"]["parseError"].get<std::string>().empty()) << report.dump(2);

    ASSERT_EQ(project["issues"].size(), 1u) << report.dump(2);
    const auto& issue = project["issues"][0];
    EXPECT_EQ(issue["code"], "JTML_MODULE_PARSE") << report.dump(2);
    EXPECT_EQ(issue["module"], broken["id"]) << report.dump(2);
    EXPECT_EQ(issue["path"], broken["path"]) << report.dump(2);
    EXPECT_EQ(issue["resolvedPath"], broken["path"]) << report.dump(2);
    EXPECT_NE(issue["message"].get<std::string>().find("Cannot parse module"),
              std::string::npos) << report.dump(2);
    EXPECT_GE(issue["line"].get<int>(), 1) << report.dump(2);
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
    ASSERT_TRUE(report.contains("runtimePlan")) << report.dump(2);
    EXPECT_EQ(report["runtimePlan"]["sourceOfTruth"], "typed AST + semantic IR");
    EXPECT_NE(report["runtimePlan"]["state"].dump().find("\"name\":\"token\""), std::string::npos)
        << report.dump(2);
    EXPECT_NE(report["runtimePlan"]["state"].dump().find("\"name\":\"users\""), std::string::npos)
        << report.dump(2);
    EXPECT_NE(report["runtimePlan"]["actions"].dump().find("\"name\":\"workspace_approve\""),
              std::string::npos) << report.dump(2);
    ASSERT_EQ(report["runtimePlan"]["routes"].size(), 1u) << report.dump(2);
    EXPECT_EQ(report["runtimePlan"]["routes"][0]["component"], "Home");
    ASSERT_EQ(report["runtimePlan"]["fetches"].size(), 1u) << report.dump(2);
    EXPECT_EQ(report["runtimePlan"]["fetches"][0]["name"], "users");
    ASSERT_EQ(report["runtimePlan"]["componentDefinitions"].size(), 1u) << report.dump(2);
    EXPECT_EQ(report["runtimePlan"]["componentDefinitions"][0]["name"], "Home");
    EXPECT_NE(report["runtimePlan"]["componentDefinitions"][0]["bodySource"].get<std::string>().find("let open = true"),
              std::string::npos) << report.dump(2);
    EXPECT_NE(report["runtimePlan"]["componentDefinitions"][0]["bodyPlan"].dump().find("\"kind\":\"action\""),
              std::string::npos) << report.dump(2);
    EXPECT_NE(report["runtimePlan"]["componentDefinitions"][0]["bodyPlan"].dump().find("\"renderRoot\":true"),
              std::string::npos) << report.dump(2);
    ASSERT_EQ(report["runtimePlan"]["componentInstances"].size(), 1u) << report.dump(2);
    EXPECT_EQ(report["runtimePlan"]["componentInstances"][0]["role"], "route");
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

TEST(CliDoctor, JsonReportsReadinessTiersAndVerificationGates) {
    jtml::cli::Options opts;
    opts.json = true;

    const auto result = captureCommand([&] { return jtml::cli::cmdDoctor(opts); });
    ASSERT_EQ(result.code, 0) << result.out << result.err;
    const auto report = nlohmann::json::parse(result.out);

    EXPECT_EQ(report["enterpriseReady"], false);
    EXPECT_NE(report["readiness"].get<std::string>().find("not enterprise-ready"),
              std::string::npos);
    EXPECT_NE(report["architectureSourceOfTruth"].get<std::string>().find("semantic IR"),
              std::string::npos);
    ASSERT_TRUE(report.contains("runtimeCapabilities")) << report.dump(2);
    EXPECT_EQ(report["runtimeCapabilities"]["runtimeAssembly"]["browserRuntimeEmitterWrapper"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["runtimeAssembly"]["browserRuntimeAssetChunks"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["moduleSystem"]["relativeImportsFromImporter"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["moduleSystem"]["recoverableModuleParseIssues"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["moduleSystem"]["sourceFirstImportIssueSpans"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["moduleSystem"]["moduleScopedComponentIdentity"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["browserLocalFirstSlice"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["slots"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["nestedComponentCalls"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["actionWhile"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["templateWhileActionOnly"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["commonAttributes"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["simpleActionArguments"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["actionLocalDeclarations"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["bodyPlanActionExpressionPlans"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["canonicalRuntimeExpressionPlans"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["memberWriteDependencyRoots"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["memberAssignmentDirectMutation"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["memberAssignmentDeepDictCreation"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["memberAssignmentDeepArrayCreation"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["liveLoopValueParity"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["metadataDrivenLeafPatches"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["metadataDrivenTextAndAttributePatches"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["cachedCompiledUpdatePlans"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["compiledPatchOperations"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["precompiledPatchOperationShapes"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["precompiledCompositeExpressions"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["compiledUpdateFunctions"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["indexedCompiledUpdateFunctions"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["generatedProductionUpdateFunctions"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["staticComponentCreateUpdateModules"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["componentModulePlanIndexAsset"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["legacyUpdatePlanCompatibilityAsset"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["directStaticComponentCreateHtml"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["directStaticContainerCreateHtml"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["directStaticControlFlowCreateHtml"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["directStaticSlotCreateHtml"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["directStaticNestedComponentCreateHtml"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["helperIndependentStaticUpdateFunctions"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["containerAttributePatchOperations"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["controlFlowRegionPatchOperations"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["directStaticControlFlowRegionPatches"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["slotRegionPatchOperations"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["nestedComponentPatchOperations"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["directStaticSlotRegionPatches"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["directStaticNestedComponentPatches"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["legacyHeuristicPatchFallback"], false);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["keyedListRegionMarkers"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["keyedListLifecycleTelemetry"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["keyedForRegionPatch"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["keyedListBelowWrapperReconciliation"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["keyedListChildMarkerReconciliation"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["keyedListPrunesRemovedDynamicChildren"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["liveBodyPlanPatchTelemetry"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["liveBodyPlanExpressionPlans"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["liveBodyPlanPrimaryTransportContract"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["sourceFirstBodyPlanColumns"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["browserSourceFirstFallbackContext"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["browserSourceFirstCreateFallbackContext"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["liveSourceFirstFallbackContext"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["liveInterpreterParity"], false);
    EXPECT_EQ(report["runtimeCapabilities"]["directComponentExecution"]["fullParity"], false);
    EXPECT_EQ(report["runtimeCapabilities"]["contractFirstJtlApis"]["planned"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["contractFirstJtlApis"]["implemented"], false);
    ASSERT_TRUE(report["runtimeCapabilities"].contains("performanceTarget")) << report.dump(2);
    EXPECT_EQ(report["runtimeCapabilities"]["performanceTarget"]["benchmarkPath"],
              "compiler-first browser production target");
    EXPECT_EQ(report["runtimeCapabilities"]["performanceTarget"]["liveHtmlPatchPath"],
              "dev/internal runtime backend");
    EXPECT_EQ(report["runtimeCapabilities"]["performanceTarget"]["bodyPlanReadWriteMetadata"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["performanceTarget"]["typedExpressionDependencyAnalysis"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["performanceTarget"]["memberSubscriptReadPaths"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["performanceTarget"]["cspSafeDefaultUpdatePlans"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["performanceTarget"]["staticUpdatePlanBuildArtifact"],
              "jtml-update-plans.js");
    EXPECT_EQ(report["runtimeCapabilities"]["performanceTarget"]["browserRuntimeBuildArtifact"],
              "jtml-runtime.js");
    EXPECT_EQ(report["runtimeCapabilities"]["performanceTarget"]["componentModuleBuildArtifact"],
              "components/index.js");
    EXPECT_EQ(report["runtimeCapabilities"]["performanceTarget"]["appBootstrapBuildArtifact"],
              "app.js");
    EXPECT_EQ(report["runtimeCapabilities"]["performanceTarget"]["staticUpdatePlanPrecomputedIndexes"],
              true);
    EXPECT_EQ(report["runtimeCapabilities"]["performanceTarget"]["staticComponentModules"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["performanceTarget"]["componentModulePlanIndex"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["performanceTarget"]["legacyUpdatePlanCompatibilityAsset"], true);
    EXPECT_EQ(report["runtimeCapabilities"]["performanceTarget"]["benchmarkSmoke"],
              "scripts/benchmark_runtime.sh");
    EXPECT_NE(report["runtimeCapabilities"]["performanceTarget"]["browserAssetBudgets"].get<std::string>().find("byte budgets"),
              std::string::npos);
    EXPECT_EQ(report["runtimeCapabilities"]["performanceTarget"]["dynamicGeneratedUpdateFunctions"],
              "explicit opt-in bridge only");
    EXPECT_EQ(report["runtimeCapabilities"]["performanceTarget"]["optimizedJsCompiler"], "planned");
    EXPECT_NE(report["runtimeCapabilities"]["performanceTarget"]["keyedListDiffing"].get<std::string>().find("key/index markers"),
              std::string::npos);
    EXPECT_NE(report["runtimeCapabilities"]["performanceTarget"]["keyedListDiffing"].get<std::string>().find("lifecycle telemetry"),
              std::string::npos);
    EXPECT_NE(report["runtimeCapabilities"]["performanceTarget"]["keyedListDiffing"].get<std::string>().find("keyed for-region patching"),
              std::string::npos);
    EXPECT_NE(report["runtimeCapabilities"]["performanceTarget"]["keyedListDiffing"].get<std::string>().find("below-wrapper element/text reconciliation"),
              std::string::npos);
    EXPECT_NE(report["runtimeCapabilities"]["performanceTarget"]["keyedListDiffing"].get<std::string>().find("child body-node marker reconciliation"),
              std::string::npos);
    EXPECT_NE(report["runtimeCapabilities"]["performanceTarget"]["prodDevRuntimeSplit"].get<std::string>().find("external jtml-runtime.js is primary"),
              std::string::npos);
    EXPECT_NE(report["runtimeCapabilities"]["performanceTarget"]["liveBodyPlanTransport"].get<std::string>().find("body-plan primary"),
              std::string::npos);
    EXPECT_NE(report["runtimeCapabilities"]["performanceTarget"]["keyedListDiffing"].get<std::string>().find("removed nested dynamic child pruning"),
              std::string::npos);
    EXPECT_NE(report["runtimeCapabilities"]["performanceTarget"]["fineGrainedUpdates"].get<std::string>().find("indexed compiled update functions"),
              std::string::npos);
    EXPECT_NE(report["runtimeCapabilities"]["performanceTarget"]["fineGrainedUpdates"].get<std::string>().find("generated production update-function source"),
              std::string::npos);
    EXPECT_NE(report["runtimeCapabilities"]["performanceTarget"]["fineGrainedUpdates"].get<std::string>().find("parsed expression dependency metadata"),
              std::string::npos);
    EXPECT_NE(report["runtimeCapabilities"]["performanceTarget"]["fineGrainedUpdates"].get<std::string>().find("canonical runtime expression plans"),
              std::string::npos);
    EXPECT_NE(report["runtimeCapabilities"]["performanceTarget"]["fineGrainedUpdates"].get<std::string>().find("container patch operation"),
              std::string::npos);
    EXPECT_NE(report["runtimeCapabilities"]["performanceTarget"]["fineGrainedUpdates"].get<std::string>().find("safe control-flow create/patch operation shapes"),
              std::string::npos);
    EXPECT_NE(report["runtimeCapabilities"]["performanceTarget"]["fineGrainedUpdates"].get<std::string>().find("direct slot/nested component node create/patch contracts"),
              std::string::npos);
    EXPECT_NE(report["runtimeCapabilities"]["performanceTarget"]["fineGrainedUpdates"].get<std::string>().find("direct region replacement for safe if/keyed-for regions"),
              std::string::npos);
    EXPECT_NE(report["runtimeCapabilities"]["performanceTarget"]["fineGrainedUpdates"].get<std::string>().find("legacy heuristic patch fallback removed"),
              std::string::npos);

    const std::string tiers = report["stabilityTiers"].dump();
    EXPECT_NE(tiers.find("stable"), std::string::npos);
    EXPECT_NE(tiers.find("first_slice"), std::string::npos);
    EXPECT_NE(tiers.find("experimental"), std::string::npos);
    EXPECT_NE(tiers.find("first browser-local direct component body-plan execution"),
              std::string::npos);
    EXPECT_NE(tiers.find("component body-plan parity"), std::string::npos);
    EXPECT_NE(tiers.find("compiler-first browser production target"), std::string::npos);
    EXPECT_NE(tiers.find("jtl 1"), std::string::npos);
    EXPECT_NE(tiers.find("contract-first JTL API modules"), std::string::npos);

    const std::string gates = report["verificationGates"].dump();
    EXPECT_NE(gates.find("scripts/verify_all.sh"), std::string::npos);
    EXPECT_NE(gates.find("scripts/benchmark_runtime.sh"), std::string::npos);
    EXPECT_NE(gates.find("ctest --test-dir build --output-on-failure"), std::string::npos);
    EXPECT_NE(gates.find("asan-ubsan"), std::string::npos);

    const std::string targets = report["nextArchitectureTargets"].dump();
    EXPECT_NE(targets.find("complete direct ComponentInstance body-plan parity"),
              std::string::npos);
    EXPECT_NE(targets.find("OpenAPI generation"), std::string::npos);
    EXPECT_NE(targets.find("Studio content externalization"), std::string::npos);
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
    ASSERT_TRUE(std::filesystem::exists(outDir / "jtml-runtime.js"));
    ASSERT_TRUE(std::filesystem::exists(outDir / "components" / "index.js"));
    ASSERT_TRUE(std::filesystem::exists(outDir / "app.js"));
    ASSERT_TRUE(std::filesystem::exists(outDir / "jtml-update-plans.js"));
    std::ifstream input(htmlPath);
    const std::string html((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    EXPECT_NE(html.find("id=\"__jtml_client_manifest\""), std::string::npos) << html;
    EXPECT_NE(html.find("<script src=\"./jtml-runtime.js\" defer></script>"),
              std::string::npos) << html;
    EXPECT_NE(html.find("<script src=\"./components/index.js\" defer></script>"),
              std::string::npos) << html;
    EXPECT_NE(html.find("<script src=\"./app.js\" defer></script>"),
              std::string::npos) << html;
    EXPECT_EQ(html.find("<script src=\"./jtml-update-plans.js\" defer></script>"),
              std::string::npos) << html;
    EXPECT_EQ(html.find("const browserLocalRuntime = true;"), std::string::npos) << html;
    EXPECT_EQ(html.find("Browser-local runtime active"), std::string::npos) << html;

    const std::string componentModule = readTextFile(outDir / "components" / "index.js");
    EXPECT_NE(componentModule.find("CSP-safe static component module seed"), std::string::npos)
        << componentModule;
    EXPECT_NE(componentModule.find("\"assetRole\":\"component-module\""), std::string::npos)
        << componentModule;
    EXPECT_NE(componentModule.find("\"mode\":\"csp-safe static component modules\""), std::string::npos)
        << componentModule;
    EXPECT_NE(componentModule.find("window.__jtml_static_component_plan_index = plans"),
              std::string::npos) << componentModule;
    EXPECT_EQ(componentModule.find("window.__jtml_static_update_plans = plans"),
              std::string::npos) << componentModule;
    EXPECT_EQ(componentModule.find("\"bodyPlan\""), std::string::npos)
        << componentModule;
    EXPECT_NE(componentModule.find("window.__jtml_static_component_modules"), std::string::npos)
        << componentModule;
    EXPECT_EQ(componentModule.find("new Function"), std::string::npos) << componentModule;

    const std::string runtimeAsset = readTextFile(outDir / "jtml-runtime.js");
    EXPECT_NE(runtimeAsset.find("const browserLocalRuntime = true;"), std::string::npos)
        << runtimeAsset;
    EXPECT_NE(runtimeAsset.find("Browser-local runtime active"), std::string::npos)
        << runtimeAsset;
    const std::string appAsset = readTextFile(outDir / "app.js");
    EXPECT_NE(appAsset.find("productionAssets"), std::string::npos) << appAsset;

    const std::string updatePlans = readTextFile(outDir / "jtml-update-plans.js");
    EXPECT_NE(updatePlans.find("CSP-safe static update plan/function seed"), std::string::npos)
        << updatePlans;
    EXPECT_NE(updatePlans.find("\"assetRole\":\"legacy-update-plan\""), std::string::npos)
        << updatePlans;
    EXPECT_NE(updatePlans.find("\"mode\":\"csp-safe static update plans\""), std::string::npos)
        << updatePlans;
    EXPECT_EQ(updatePlans.find("window.__jtml_static_component_plan_index = plans"),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("window.__jtml_static_update_plans"), std::string::npos)
        << updatePlans;
    EXPECT_NE(updatePlans.find("window.__jtml_static_update_functions"), std::string::npos)
        << updatePlans;
    EXPECT_NE(updatePlans.find("window.__jtml_static_component_modules"), std::string::npos)
        << updatePlans;
    EXPECT_NE(updatePlans.find("window.jtml.staticUpdatePlans = plans"), std::string::npos)
        << updatePlans;
    EXPECT_NE(updatePlans.find("window.jtml.staticUpdateFunctions = functions"), std::string::npos)
        << updatePlans;
    EXPECT_NE(updatePlans.find("window.jtml.staticComponentModules = modules"), std::string::npos)
        << updatePlans;
    EXPECT_NE(updatePlans.find("staticUpdatePlansAsset = true"), std::string::npos)
        << updatePlans;
    EXPECT_NE(updatePlans.find("staticUpdateFunctionsAsset = true"), std::string::npos)
        << updatePlans;
    EXPECT_NE(updatePlans.find("staticComponentModulesAsset = true"), std::string::npos)
        << updatePlans;
    EXPECT_NE(updatePlans.find("\"staticUpdateFunctions\":true"), std::string::npos)
        << updatePlans;
    EXPECT_NE(updatePlans.find("\"componentCount\":0"), std::string::npos)
        << updatePlans;
    EXPECT_EQ(updatePlans.find("new Function"), std::string::npos) << updatePlans;
    EXPECT_EQ(updatePlans.find("bodySource"), std::string::npos) << updatePlans;
    EXPECT_EQ(updatePlans.find("bodyHex"), std::string::npos) << updatePlans;
}

TEST(CliBuild, BrowserTargetEmbedsRuntimeProjectPlanForModules) {
    const auto root = makeTempDir("browser-project-plan");
    writeTempFile(root / "stores" / "app-state.jtml",
        "jtml 2\n"
        "export store appState\n"
        "  let user = \"Ada\"\n");
    writeTempFile(root / "pages" / "dashboard.jtml",
        "jtml 2\n"
        "use appState from \"../stores/app-state.jtml\"\n"
        "export make Summary label\n"
        "  text label\n"
        "export make Shell title\n"
        "  card title title tone neutral\n"
        "    slot\n"
        "export make NestedHost label\n"
        "  Shell label\n"
        "    text label\n"
        "export make StatusBadge label\n"
        "  badge label tone \"good\" title label\n"
        "export make ActionButton label\n"
        "  let last = \"\"\n"
        "  when choose value\n"
        "    last = value\n"
        "  button label+\"!\" click choose(label==\"Open\"?\"yes\":\"no\")\n"
        "export make ActivityFeed\n"
        "  let visible = true\n"
        "  let items = [\"Build\", \"Ship\"]\n"
        "  if visible\n"
        "    for item in items key item\n"
        "      text item\n"
        "  else\n"
        "    text \"Hidden\"\n"
        "export make Dashboard pageTitle\n"
        "  let currentTitle = pageTitle\n"
        "  when rename nextTitle\n"
        "    currentTitle = nextTitle\n"
        "  panel title currentTitle\n"
        "    text appState.user\n"
        "    button currentTitle click rename(currentTitle)\n");
    writeTempFile(root / "index.jtml",
        "jtml 2\n"
        "use { Dashboard, Summary, Shell, NestedHost, StatusBadge, ActionButton, ActivityFeed } from \"./pages/dashboard.jtml\"\n"
        "let nestedTitle = \"Nested\"\n"
        "page\n"
        "  Dashboard \"Home\"\n"
        "  Summary \"Ready\"\n"
        "  NestedHost nestedTitle\n"
        "  StatusBadge \"Stable\"\n"
        "  ActionButton \"Open\"\n"
        "  ActivityFeed\n");

    const auto outDir = std::filesystem::temp_directory_path() /
                        ("jtml-browser-project-plan-" +
                         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    jtml::cli::Options opts;
    opts.inputFile = (root / "index.jtml").string();
    opts.outputFile = outDir.string();
    opts.syntax = jtml::SyntaxMode::Auto;
    opts.target = "browser";

    const auto result = captureCommand([&] { return jtml::cli::cmdBuild(opts); });
    ASSERT_EQ(result.code, 0) << result.out << result.err;

    const auto htmlPath = outDir / "index.html";
    ASSERT_TRUE(std::filesystem::exists(htmlPath));
    ASSERT_TRUE(std::filesystem::exists(outDir / "jtml-update-plans.js"));
    const std::string html = readTextFile(htmlPath);
    EXPECT_EQ(html.find("mergeProjectManifest"), std::string::npos) << html;
    EXPECT_EQ(html.find("runtimeManifestSource"), std::string::npos) << html;
    EXPECT_EQ(html.find("projectManifest"), std::string::npos) << html;
    EXPECT_NE(html.find("components/index.js"), std::string::npos) << html;
    EXPECT_NE(html.find("jtml-runtime.js"), std::string::npos) << html;
    EXPECT_NE(html.find("app.js"), std::string::npos) << html;
    EXPECT_EQ(html.find("<script src=\"./jtml-update-plans.js\" defer></script>"),
              std::string::npos) << html;
    ASSERT_TRUE(std::filesystem::exists(outDir / "jtml-runtime.js"));
    ASSERT_TRUE(std::filesystem::exists(outDir / "components" / "index.js"));
    ASSERT_TRUE(std::filesystem::exists(outDir / "app.js"));
    const std::string runtimeAsset = readTextFile(outDir / "jtml-runtime.js");
    EXPECT_NE(runtimeAsset.find("mergeProjectManifest"), std::string::npos)
        << runtimeAsset;
    EXPECT_NE(runtimeAsset.find("runtimeManifestSource"), std::string::npos)
        << runtimeAsset;
    EXPECT_NE(runtimeAsset.find("projectManifest"), std::string::npos)
        << runtimeAsset;
    EXPECT_EQ(countOccurrences(html, "id=\"__jtml_client_manifest\""), 1u) << html;
    EXPECT_EQ(html.find(root.string()), std::string::npos) << html;
    const auto manifest = extractClientManifestJson(html);
    EXPECT_EQ(manifest.dump().find("bodySource"), std::string::npos)
        << manifest.dump(2);
    EXPECT_EQ(manifest.dump().find("bodyHex"), std::string::npos)
        << manifest.dump(2);
    ASSERT_TRUE(manifest.contains("project")) << manifest.dump(2);
    const auto& project = manifest["project"];
    EXPECT_EQ(project["sourceOfTruth"], "runtime client manifest");
    ASSERT_EQ(project["modules"].size(), 3u) << project.dump(2);
    for (const auto& module : project["modules"]) {
        ASSERT_TRUE(module.contains("executable")) << project.dump(2);
        if (module["executable"].get<bool>()) {
            ASSERT_TRUE(module.contains("plan")) << project.dump(2);
            EXPECT_FALSE(module["plan"].contains("semantic")) << project.dump(2);
        } else {
            EXPECT_FALSE(module.contains("plan")) << project.dump(2);
        }
    }
    EXPECT_TRUE(manifest["componentInstances"].is_array()) << manifest.dump(2);

    const std::string componentModule = readTextFile(outDir / "components" / "index.js");
    EXPECT_NE(componentModule.find("\"mode\":\"csp-safe static component modules\""),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("\"assetRole\":\"component-module\""),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("window.__jtml_static_component_plan_index = plans"),
              std::string::npos) << componentModule;
    EXPECT_EQ(componentModule.find("window.__jtml_static_update_plans = plans"),
              std::string::npos) << componentModule;
    EXPECT_EQ(componentModule.find("\"bodyPlan\""),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("\"staticCreateCoverage\":\"direct-text-button-element-container-control-flow-slot-nested-create-first-slice\""),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("\"rootCreateOperations\""),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("\"unsafeRootCreateEntries\""),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("\"partsPlan\""),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("\"expressionPlan\""),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("\"sourceColumn\""),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("\"contentPlan\""),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("\"exprPlan\""),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("\"kind\":\"conditional\""),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("\"kind\":\"binary\""),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("\"operator\":\"+\""),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("\"operator\":\"==\""),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("function(scope){"),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("Object.prototype.hasOwnProperty.call(scope"),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("el.textContent = String(value == null ? '' : value);"),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("function addAttr(name, value)"),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("addAttr('title', (function(scope){"),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("addAttr('data-jtml-direct-node','panel')"),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("el.tagName.toLowerCase() !== 'button'"),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("const actionArgs = [(function(scope){"),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("data-jtml-direct-component-args"),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("static-production-direct-create-function"),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("function jtml_static_escape_html(value)"),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("function jtml_static_attrs_to_string(attrs)"),
              std::string::npos) << componentModule;
    EXPECT_EQ(componentModule.find("if (!h) return jtml_static_component_create_fallback_"),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("return '<p data-jtml-direct-body-node=\""),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("return '<button' + jtml_static_attrs_to_string(next)"),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("return '<span' + jtml_static_attrs_to_string(next)"),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("return '<section' + jtml_static_attrs_to_string(next)"),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("children.join('') + '</section>'"),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("data-jtml-direct-region=\"if\""),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("data-jtml-direct-region=\"for\""),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("for (let itemIndex = 0; itemIndex < values.length; itemIndex += 1)"),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("childScope['item'] = values[itemIndex]"),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("data-jtml-direct-list-key"),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("renderStaticComponentSlotNode(instance, definition, scope"),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("renderStaticComponentNestedNode(instance, definition, scope"),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("patchStaticComponentNestedNode(instance, definition"),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("next['data-jtml-direct-component-args'] = JSON.stringify(actionArgs);"),
              std::string::npos) << componentModule;
    EXPECT_EQ(componentModule.find("renderStaticComponentElementNode"),
              std::string::npos) << componentModule;
    EXPECT_EQ(componentModule.find("renderStaticComponentTextNode"),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("function jtml_static_component_patch_"),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("recordStaticCreateFallback(instance, definition, plan"),
              std::string::npos) << componentModule;
    EXPECT_EQ(componentModule.find("if (!plan || !h) return { handled: false, patched: 0 };"),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("function affectedEntries()"),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("h && h.componentPlanAffectedEntries ? h.componentPlanAffectedEntries(plan, changed) : affectedEntries()"),
              std::string::npos) << componentModule;
    EXPECT_EQ(componentModule.find("patchStaticComponentTextNode"),
              std::string::npos) << componentModule;
    EXPECT_EQ(componentModule.find("patchStaticComponentElementNode"),
              std::string::npos) << componentModule;
    EXPECT_EQ(componentModule.find("patchStaticComponentRegionNode"),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("el.textContent = String(value == null ? '' : value);"),
              std::string::npos) << componentModule;
    EXPECT_NE(componentModule.find("renderStaticComponentCreateOperations(instance"),
              std::string::npos) << componentModule;
    const std::string updatePlans = readTextFile(outDir / "jtml-update-plans.js");
    EXPECT_NE(updatePlans.find("\"mode\":\"csp-safe static update plans\""),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("\"assetRole\":\"legacy-update-plan\""),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("\"componentCount\":"), std::string::npos)
        << updatePlans;
    EXPECT_NE(updatePlans.find("\"bodyPlan\""), std::string::npos)
        << updatePlans;
    EXPECT_NE(updatePlans.find("\"reads\""), std::string::npos)
        << updatePlans;
    EXPECT_NE(updatePlans.find("\"writes\""), std::string::npos)
        << updatePlans;
    EXPECT_NE(updatePlans.find("\"entriesByRead\""), std::string::npos)
        << updatePlans;
    EXPECT_NE(updatePlans.find("\"unsafeEntries\""), std::string::npos)
        << updatePlans;
    EXPECT_NE(updatePlans.find("\"staticPatchCoverage\":\"text-region-slot-nested-element-first-slice\""),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("\"staticCreateCoverage\":\"direct-text-button-element-container-control-flow-slot-nested-create-first-slice\""),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("\"partsPlan\""),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("\"expressionPlan\""),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("\"sourceColumn\""),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("\"contentPlan\""),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("\"exprPlan\""),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("\"kind\":\"conditional\""),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("\"kind\":\"binary\""),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("\"operator\":\"+\""),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("\"operator\":\"==\""),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("function(scope){"),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("Object.prototype.hasOwnProperty.call(scope"),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("el.textContent = String(value == null ? '' : value);"),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("function addAttr(name, value)"),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("addAttr('title', (function(scope){"),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("addAttr('data-jtml-direct-node','panel')"),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("el.tagName.toLowerCase() !== 'button'"),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("const actionArgs = [(function(scope){"),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("data-jtml-direct-component-args"),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("function jtml_static_escape_html(value)"),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("function jtml_static_attrs_to_string(attrs)"),
              std::string::npos) << updatePlans;
    EXPECT_EQ(updatePlans.find("if (!h) return jtml_static_component_create_fallback_"),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("return '<p data-jtml-direct-body-node=\""),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("return '<button' + jtml_static_attrs_to_string(next)"),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("return '<span' + jtml_static_attrs_to_string(next)"),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("return '<section' + jtml_static_attrs_to_string(next)"),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("children.join('') + '</section>'"),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("data-jtml-direct-region=\"if\""),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("data-jtml-direct-region=\"for\""),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("for (let itemIndex = 0; itemIndex < values.length; itemIndex += 1)"),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("childScope['item'] = values[itemIndex]"),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("data-jtml-direct-list-key"),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("renderStaticComponentSlotNode(instance, definition, scope"),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("renderStaticComponentNestedNode(instance, definition, scope"),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("patchStaticComponentNestedNode(instance, definition"),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("function jtml_static_component_update_"),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("recordStaticCreateFallback(instance, definition, plan"),
              std::string::npos) << updatePlans;
    EXPECT_EQ(updatePlans.find("if (!plan || !h) return { handled: false, patched: 0 };"),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("function affectedEntries()"),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("h && h.componentPlanAffectedEntries ? h.componentPlanAffectedEntries(plan, changed) : affectedEntries()"),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("function jtml_static_component_create_"),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("static-production-update-function"),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("staticComponentModuleCount = Object.keys(modules).length"),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("create: jtml_static_component_create_"),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("update: jtml_static_component_update_"),
              std::string::npos) << updatePlans;
    EXPECT_NE(updatePlans.find("plans.staticUpdateFunctionCount = Object.keys(functions).length"),
              std::string::npos) << updatePlans;
    EXPECT_EQ(updatePlans.find("new Function"), std::string::npos)
        << updatePlans;
    EXPECT_EQ(updatePlans.find(root.string()), std::string::npos)
        << updatePlans;
}

TEST(CliBuild, BrowserClientManifestEscapesScriptBreakingTextAndOmitsExplainFields) {
    const auto file = writeTempJtml(
        "manifest-script-safety",
        "jtml 2\n"
        "let message = \"</script><script>window.__bad=1</script>\"\n"
        "make Notice\n"
        "  text \"</script><script>window.__bad=1</script>\"\n"
        "page\n"
        "  Notice\n");
    const auto outDir = std::filesystem::temp_directory_path() /
                        ("jtml-manifest-script-safety-" +
                         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    jtml::cli::Options buildOpts;
    buildOpts.inputFile = file.string();
    buildOpts.outputFile = outDir.string();
    buildOpts.syntax = jtml::SyntaxMode::Auto;
    buildOpts.target = "browser";
    const auto buildResult = captureCommand([&] { return jtml::cli::cmdBuild(buildOpts); });
    ASSERT_EQ(buildResult.code, 0) << buildResult.out << buildResult.err;

    const std::string html = readTextFile(outDir / "index.html");
    EXPECT_EQ(countOccurrences(html, "id=\"__jtml_client_manifest\""), 1u) << html;
    const auto manifestText = extractClientManifestText(html);
    EXPECT_EQ(manifestText.find("</script><script>window.__bad=1"), std::string::npos)
        << manifestText;
    EXPECT_NE(manifestText.find("\\u003c/script\\u003e\\u003cscript\\u003ewindow.__bad=1\\u003c/script\\u003e"),
              std::string::npos) << manifestText;
    const auto manifest = extractClientManifestJson(html);
    ASSERT_TRUE(manifest["state"]["message"].is_string()) << manifest.dump(2);
    EXPECT_EQ(manifest["state"]["message"].get<std::string>().find("</script><script>window.__bad=1"),
              std::string::npos);
    EXPECT_NE(manifest["state"]["message"].get<std::string>().find("\\u003c/script\\u003e"),
              std::string::npos);
    EXPECT_EQ(manifest.dump().find("bodySource"), std::string::npos);
    EXPECT_EQ(manifest.dump().find("bodyHex"), std::string::npos);

    jtml::cli::Options explainOpts;
    explainOpts.inputFile = file.string();
    explainOpts.syntax = jtml::SyntaxMode::Auto;
    explainOpts.json = true;
    const auto explainResult = captureCommand([&] { return jtml::cli::cmdExplain(explainOpts); });
    ASSERT_EQ(explainResult.code, 0) << explainResult.out << explainResult.err;
    const auto explain = nlohmann::json::parse(explainResult.out);
    ASSERT_TRUE(explain["runtimePlan"].contains("componentDefinitions"));
    ASSERT_FALSE(explain["runtimePlan"]["componentDefinitions"].empty());
    EXPECT_TRUE(explain["runtimePlan"]["componentDefinitions"][0].contains("bodySource"));
    EXPECT_TRUE(explain["runtimePlan"]["componentDefinitions"][0].contains("bodyHex"));
}
