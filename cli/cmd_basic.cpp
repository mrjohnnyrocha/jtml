// cli/cmd_basic.cpp — commands that don't own servers or multi-pass
// pipelines: examples, new, check, interpret, transpile. Each stays short
// and self-contained.
#include "commands.h"
#include "diagnostic_json.h"

#include "jtml/interpreter.h"
#include "jtml/linter.h"
#include "jtml/transpiler.h"
#include "json.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

namespace jtml::cli {
using jtml::JtmlLinter;

namespace {

bool pathIsInside(const std::filesystem::path& path, const std::filesystem::path& parent) {
    auto rel = std::filesystem::relative(path, parent);
    return rel.empty() || (rel.native().rfind("..", 0) != 0 && rel != ".");
}

std::string trimCopy(std::string s) {
    auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

std::vector<int> friendlyLineMapForFile(const Options& o) {
    try {
        const std::string source = readFile(o.inputFile);
        const bool friendly =
            o.syntax == SyntaxMode::Friendly ||
            (o.syntax == SyntaxMode::Auto &&
             (isFriendlySyntax(source) || looksLikeFriendlySyntax(source)));
        if (!friendly) return {};
        return friendlyToClassicWithSourceMap(source).classicLineToFriendlyLine;
    } catch (...) {
        return {};
    }
}

void copyBuildAssets(const std::filesystem::path& inputDir,
                     const std::filesystem::path& outDir) {
    if (!std::filesystem::is_directory(inputDir)) return;

    const auto absInput = std::filesystem::weakly_canonical(inputDir);
    const auto absOut = std::filesystem::absolute(outDir).lexically_normal();
    int copied = 0;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(absInput)) {
        const auto src = entry.path();
        if (pathIsInside(src, absOut)) continue;
        if (entry.is_directory()) continue;
        if (!entry.is_regular_file()) continue;
        if (src.extension() == ".jtml") continue;

        auto rel = std::filesystem::relative(src, absInput);
        auto dest = outDir / rel;
        std::filesystem::create_directories(dest.parent_path());
        std::filesystem::copy_file(src, dest, std::filesystem::copy_options::overwrite_existing);
        ++copied;
    }

    if (copied > 0) {
        std::cout << "Copied " << copied << " asset(s) from " << inputDir.string() << "\n";
    }
}

struct SourceInsights {
    std::vector<std::string> state;
    std::vector<std::string> constants;
    std::vector<std::string> derived;
    std::vector<std::string> actions;
    std::vector<std::string> components;
    std::vector<std::string> routes;
    std::vector<std::string> fetches;
    std::vector<std::string> stores;
    std::vector<std::string> effects;
    int styleBlocks = 0;
};

std::vector<std::string> words(const std::string& line) {
    std::vector<std::string> out;
    std::istringstream input(line);
    std::string word;
    while (input >> word) out.push_back(word);
    return out;
}

std::string stripInlineComment(const std::string& line) {
    char quote = '\0';
    for (size_t i = 0; i < line.size(); ++i) {
        char ch = line[i];
        if (quote != '\0') {
            if (ch == '\\' && i + 1 < line.size()) ++i;
            else if (ch == quote) quote = '\0';
            continue;
        }
        if (ch == '"' || ch == '\'') {
            quote = ch;
            continue;
        }
        if (ch == '/' && i + 1 < line.size() && line[i + 1] == '/') return line.substr(0, i);
        if (ch == '#' && (i == 0 || std::isspace(static_cast<unsigned char>(line[i - 1])))) {
            return line.substr(0, i);
        }
    }
    return line;
}

std::string cleanIdentifier(std::string token) {
    while (!token.empty() && (token.back() == '\\' || token.back() == '(' ||
                              token.back() == ')' || token.back() == ',' ||
                              token.back() == ':')) {
        token.pop_back();
    }
    const auto colon = token.find(':');
    if (colon != std::string::npos) token = token.substr(0, colon);
    return token;
}

SourceInsights inspectSource(const std::string& source) {
    SourceInsights insights;
    std::istringstream input(source);
    std::string raw;
    while (std::getline(input, raw)) {
        std::string line = trimCopy(stripInlineComment(raw));
        if (line.empty() || line == "jtml 2") continue;
        auto tokens = words(line);
        if (tokens.empty()) continue;

        const std::string head = tokens[0];
        auto second = [&]() -> std::string {
            return tokens.size() > 1 ? cleanIdentifier(tokens[1]) : std::string{};
        };

        if ((head == "let" || head == "define") && tokens.size() > 1) {
            std::string name = second();
            insights.state.push_back(name);
            if (line.find(" fetch ") != std::string::npos ||
                line.find("= fetch ") != std::string::npos) {
                insights.fetches.push_back(name);
            }
        } else if (head == "const" && tokens.size() > 1) {
            insights.constants.push_back(second());
        } else if ((head == "get" || head == "derive") && tokens.size() > 1) {
            insights.derived.push_back(second());
        } else if ((head == "when" || head == "function") && tokens.size() > 1) {
            insights.actions.push_back(second());
        } else if (head == "make" && tokens.size() > 1) {
            insights.components.push_back(second());
        } else if (head == "route" && tokens.size() >= 4) {
            insights.routes.push_back(tokens[1] + " -> " + cleanIdentifier(tokens[3]));
        } else if (head == "store" && tokens.size() > 1) {
            insights.stores.push_back(second());
        } else if (head == "effect" && tokens.size() > 1) {
            insights.effects.push_back(second());
        } else if (head == "style") {
            ++insights.styleBlocks;
        }
    }
    return insights;
}

nlohmann::json listJson(const std::vector<std::string>& values) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& value : values) out.push_back(value);
    return out;
}

void printInsightList(const std::string& label, const std::vector<std::string>& values) {
    std::cout << "- " << label << ": ";
    if (values.empty()) {
        std::cout << "none\n";
        return;
    }
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << values[i];
    }
    std::cout << "\n";
}

} // namespace

// Boilerplate a brand-new user's first page. Kept small and idiomatic so
// `jtml new` produces a file that cleanly lexes, parses, and runs.
static const char* kStarterProgram = R"(define count = 0\\
derive doubled = count * 2\\

function increment()\\
    count = count + 1\\
\\

function reset()\\
    count = 0\\
\\

element main style="max-width: 680px; margin: 48px auto; padding: 32px; font-family: Arial, sans-serif"\\
    element h1\\
        show "JTML Counter"\\
    #

    element p\\
        show "A tiny reactive page: define state, derive values, show expressions, and call functions from events."\\
    #

    element section\\
        element strong\\
            show "Count: " + count\\
        #

        element div\\
            show "Doubled: " + doubled\\
        #

        element div\\
            element button onClick=increment()\\
                show "Add one"\\
            #
            element button onClick=reset()\\
                show "Reset"\\
            #
        #
    #
#
)";

static const char* kStarterAppIndex = R"(jtml 2

use Card from "./components/card.jtml"

style
  .app
    max-width: 920px
    margin: 48px auto
    padding: 0 24px
    font-family: system-ui, sans-serif
    display: grid
    gap: 18px
  .hero
    display: grid
    gap: 8px
  .grid
    display: grid
    grid-template-columns: repeat(auto-fit, minmax(220px, 1fr))
    gap: 14px
  .card
    border: 1px solid #d7dce2
    border-radius: 8px
    padding: 18px
    background: white

let count = 0

when add
  count += 1

page class "app"
  section class "hero"
    h1 "JTML app"
    text "A modular Friendly JTML starter with components, state, actions, styles, and zero config."
  section class "grid"
    Card "Interactive state"
      text "Count: {count}"
      button "Add one" click add
    Card "Next steps"
      list
        item "Edit index.jtml"
        item "Add components under components/"
        item "Run jtml dev ."
)";

static const char* kStarterAppCard = R"(jtml 2

make Card title
  box class "card"
    h2 title
    slot
)";

static const char* kStarterAppStore = R"(jtml 2

store app
  let user = ""
  let ready = true

  when reset
    let user = ""
)";

void createStarterApp(const Options& o) {
    namespace fs = std::filesystem;
    if (o.outputFile.empty()) usage();
    fs::path root = o.outputFile;
    if (fs::exists(root) && !o.force) {
        throw std::runtime_error(
            "Refusing to overwrite existing app directory: " + root.string() +
            " (use --force)");
    }

    fs::create_directories(root / "components");
    fs::create_directories(root / "stores");
    fs::create_directories(root / "assets");
    writeFile((root / "index.jtml").string(), kStarterAppIndex, true);
    writeFile((root / "components" / "card.jtml").string(), kStarterAppCard, true);
    writeFile((root / "stores" / "app.jtml").string(), kStarterAppStore, true);
    writeFile((root / "assets" / ".gitkeep").string(), "", true);

    std::cout << "Created JTML app " << root.string() << "\n"
              << "Try it with: cd " << root.string() << " && jtml dev . --port 8000\n";
}

bool isValidPackageName(const std::string& name) {
    if (name.empty() || name == "." || name == "..") return false;
    for (char ch : name) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (std::isalnum(c) || ch == '-' || ch == '_' || ch == '.') continue;
        return false;
    }
    return true;
}

std::string packageNameFromPath(const std::filesystem::path& source) {
    std::string name = source.filename().string();
    if (source.has_extension() && source.extension() == ".jtml") {
        name = source.stem().string();
    }
    return name;
}

std::string fnv1aHex(const std::string& input) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (unsigned char ch : input) {
        hash ^= ch;
        hash *= 1099511628211ULL;
    }
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

nlohmann::json packageLockEntry(const std::filesystem::path& packageDir,
                                const std::filesystem::path& source,
                                const std::string& packageName) {
    namespace fs = std::filesystem;
    std::vector<fs::path> files;
    for (const auto& entry : fs::recursive_directory_iterator(packageDir)) {
        if (entry.is_regular_file()) files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());

    nlohmann::json fileEntries = nlohmann::json::array();
    std::string combined;
    for (const auto& file : files) {
        const fs::path rel = fs::relative(file, packageDir);
        const std::string content = readFile(file.string());
        const std::string fingerprint = fnv1aHex(content);
        combined += rel.generic_string() + "\n" + fingerprint + "\n";
        fileEntries.push_back({
            {"path", rel.generic_string()},
            {"size", static_cast<std::uintmax_t>(fs::file_size(file))},
            {"fingerprint", fingerprint}
        });
    }

    fs::path entry = "index.jtml";
    if (!fs::exists(packageDir / entry) && fs::exists(packageDir / "package.jtml")) {
        entry = "package.jtml";
    }

    return {
        {"name", packageName},
        {"path", (fs::path("jtml_modules") / packageName).generic_string()},
        {"source", source.string()},
        {"entry", entry.generic_string()},
        {"fingerprint", fnv1aHex(combined)},
        {"files", fileEntries}
    };
}

void installLocalPackage(const std::filesystem::path& source,
                         const std::string& packageName,
                         bool force,
                         bool jsonOutput,
                         bool quiet = false) {
    namespace fs = std::filesystem;
    if (!isValidPackageName(packageName)) {
        throw std::runtime_error(
            "Invalid package name `" + packageName +
            "`. Use letters, numbers, dots, hyphens, or underscores.");
    }
    if (!fs::exists(source)) {
        throw std::runtime_error(
            "Package source not found: " + source.string() +
            ". Registry installs are planned; today `jtml add` installs local files or directories.");
    }

    fs::path absoluteSource = fs::absolute(source).lexically_normal();
    if (fs::is_directory(absoluteSource) &&
        !fs::exists(absoluteSource / "index.jtml") &&
        !fs::exists(absoluteSource / "package.jtml")) {
        throw std::runtime_error(
            "Local package directory must contain index.jtml or package.jtml: " +
            absoluteSource.string());
    }
    if (fs::is_regular_file(absoluteSource) && absoluteSource.extension() != ".jtml") {
        throw std::runtime_error("Local package file must be a .jtml file: " +
                                 absoluteSource.string());
    }

    const fs::path modulesDir = fs::current_path() / "jtml_modules";
    const fs::path dest = modulesDir / packageName;
    if (fs::exists(dest) && !force) {
        throw std::runtime_error(
            "Package already installed: " + packageName + " (use --force)");
    }

    fs::create_directories(modulesDir);
    if (fs::exists(dest)) fs::remove_all(dest);
    fs::create_directories(dest);

    if (fs::is_directory(absoluteSource)) {
        fs::copy(absoluteSource, dest,
                 fs::copy_options::recursive | fs::copy_options::overwrite_existing);
    } else {
        fs::copy_file(absoluteSource, dest / "index.jtml",
                      fs::copy_options::overwrite_existing);
    }

    const fs::path manifestPath = fs::current_path() / "jtml.packages.json";
    nlohmann::json manifest = {
        {"version", 1},
        {"packages", nlohmann::json::object()}
    };
    if (fs::exists(manifestPath)) {
        try {
            std::ifstream in(manifestPath);
            in >> manifest;
            if (!manifest.contains("packages") || !manifest["packages"].is_object()) {
                manifest["packages"] = nlohmann::json::object();
            }
            manifest["version"] = manifest.value("version", 1);
        } catch (...) {
            throw std::runtime_error("Cannot parse existing package manifest: " +
                                     manifestPath.string());
        }
    }

    manifest["packages"][packageName] = {
        {"path", (fs::path("jtml_modules") / packageName).generic_string()},
        {"source", absoluteSource.string()}
    };
    writeFile(manifestPath.string(), manifest.dump(2) + "\n", true);

    const fs::path lockPath = fs::current_path() / "jtml.lock.json";
    nlohmann::json lock = {
        {"version", 1},
        {"packages", nlohmann::json::object()}
    };
    if (fs::exists(lockPath)) {
        try {
            std::ifstream in(lockPath);
            in >> lock;
            if (!lock.contains("packages") || !lock["packages"].is_object()) {
                lock["packages"] = nlohmann::json::object();
            }
            lock["version"] = lock.value("version", 1);
        } catch (...) {
            throw std::runtime_error("Cannot parse existing package lockfile: " +
                                     lockPath.string());
        }
    }
    lock["packages"][packageName] = packageLockEntry(dest, absoluteSource, packageName);
    writeFile(lockPath.string(), lock.dump(2) + "\n", true);

    if (jsonOutput && !quiet) {
        std::cout << nlohmann::json({
            {"ok", true},
            {"package", packageName},
            {"path", dest.string()},
            {"manifest", manifestPath.string()},
            {"lockfile", lockPath.string()}
        }).dump(2) << "\n";
    } else if (!quiet) {
        std::cout << "Installed JTML package `" << packageName << "` at "
                  << dest.string() << "\n"
                  << "Locked package fingerprint in " << lockPath.string() << "\n"
                  << "Import it with: use Name from \"" << packageName << "\"\n";
    }
}

nlohmann::json readJsonFile(const std::filesystem::path& path,
                            const std::string& label) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("Missing " + label + ": " + path.string());
    }
    try {
        std::ifstream in(path);
        nlohmann::json value;
        in >> value;
        return value;
    } catch (...) {
        throw std::runtime_error("Cannot parse " + label + ": " + path.string());
    }
}

nlohmann::json verifyPackageLock(bool restoreFromManifest) {
    namespace fs = std::filesystem;
    const fs::path root = fs::current_path();
    const fs::path manifestPath = root / "jtml.packages.json";
    const fs::path lockPath = root / "jtml.lock.json";
    nlohmann::json restored = nlohmann::json::array();

    if (restoreFromManifest && fs::exists(manifestPath)) {
        const auto manifest = readJsonFile(manifestPath, "package manifest");
        if (manifest.contains("packages") && manifest["packages"].is_object()) {
            for (const auto& item : manifest["packages"].items()) {
                const std::string name = item.key();
                const std::string source = item.value().value("source", "");
                if (source.empty() || !fs::exists(source)) continue;
                installLocalPackage(source, name, true, false, true);
                restored.push_back(name);
            }
        }
    }

    const auto lock = readJsonFile(lockPath, "package lockfile");
    if (!lock.contains("packages") || !lock["packages"].is_object()) {
        throw std::runtime_error("Package lockfile has no object `packages`: " +
                                 lockPath.string());
    }

    nlohmann::json packages = nlohmann::json::array();
    bool ok = true;
    for (const auto& item : lock["packages"].items()) {
        const std::string name = item.key();
        const auto& expected = item.value();
        const fs::path packageDir = root / expected.value(
            "path", (fs::path("jtml_modules") / name).generic_string());

        nlohmann::json result = {
            {"name", name},
            {"path", packageDir.string()},
            {"ok", false}
        };
        if (!fs::exists(packageDir)) {
            result["error"] = "package directory is missing";
            ok = false;
            packages.push_back(result);
            continue;
        }

        const auto actual = packageLockEntry(
            packageDir,
            expected.value("source", ""),
            name);
        result["expectedFingerprint"] = expected.value("fingerprint", "");
        result["actualFingerprint"] = actual.value("fingerprint", "");
        result["fileCount"] = actual["files"].size();

        if (actual.value("fingerprint", "") != expected.value("fingerprint", "")) {
            result["error"] = "package fingerprint mismatch";
            ok = false;
        } else {
            result["ok"] = true;
        }
        packages.push_back(result);
    }

    return {
        {"ok", ok},
        {"manifest", manifestPath.string()},
        {"lockfile", lockPath.string()},
        {"restored", restored},
        {"packages", packages}
    };
}

int cmdExamples(const Options&) {
    std::cout << "JTML examples:\n";
    std::filesystem::path examplesDir = "examples";
    if (!std::filesystem::exists(examplesDir)) {
        std::cout << "  (no examples directory found from this working directory)\n";
        return 0;
    }
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(examplesDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".jtml") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    for (const auto& f : files) std::cout << "  " << f.string() << "\n";
    std::cout << "\nTry:\n"
              << "  jtml demo --port 8000\n"
              << "  jtml tutorial --port 8000\n";
    return 0;
}

int cmdDoctor(const Options& o) {
    namespace fs = std::filesystem;
    struct Check {
        std::string label;
        fs::path path;
        bool executable = false;
    };

    const std::vector<Check> checks = {
        {"core headers", "include/jtml"},
        {"core sources", "src"},
        {"CLI sources", "cli"},
        {"unit tests", "tests"},
        {"examples", "examples"},
        {"tutorial lessons", "tutorial"},
        {"VS Code editor files", "editors/vscode"},
        {"documentation", "docs"},
        {"predeploy website", "site"},
        {"build script", "scripts/build_site.sh", true},
        {"package script", "scripts/package_cli.sh", true},
        {"verification script", "scripts/verify_all.sh", true},
    };

    bool ok = true;
    nlohmann::json results = nlohmann::json::array();
    if (!o.json) std::cout << "JTML doctor\n";
    for (const auto& check : checks) {
        bool exists = fs::exists(check.path);
        bool pass = exists;
        if (check.executable && exists) {
            auto perms = fs::status(check.path).permissions();
            pass = (perms & fs::perms::owner_exec) != fs::perms::none;
        }
        ok = ok && pass;
        results.push_back({
            {"label", check.label},
            {"path", check.path.string()},
            {"requiredExecutable", check.executable},
            {"ok", pass},
        });
        if (!o.json) {
            std::cout << (pass ? "OK   " : "MISS ") << check.label
                      << " (" << check.path.string() << ")\n";
        }
    }

    if (o.json) {
        nlohmann::json out = {
            {"ok", ok},
            {"version", versionString()},
            {"checks", results},
        };
        std::cout << out.dump(2) << "\n";
        return ok ? 0 : 1;
    }

    if (!ok) {
        std::cout << "Doctor found missing local toolkit pieces.\n";
        return 1;
    }
    std::cout << "Local toolkit shape is complete. For full verification run scripts/verify_all.sh\n";
    return 0;
}

int cmdGenerate(const Options& o) {
    const std::string description = o.inputFile.empty() ? "an interactive JTML page" : o.inputFile;
    std::cout
        << "# AI prompt\n"
        << "Generate Friendly JTML v2 for: " << description << "\n"
        << "Use only JTML syntax: jtml 2, style, use/export, let, get, when, make, page, show, "
           "if/else, for/while, and element lines like button \"Save\" click save.\n"
        << "Prefer small readable state, scoped style blocks, and no JavaScript.\n\n"
        << "# Starter\n"
        << "jtml 2\n"
        << "style\n"
        << "  .app\n"
        << "    max-width: 960px\n"
        << "    margin: 48px auto\n"
        << "    padding: 32px\n"
        << "    font-family: system-ui, sans-serif\n"
        << "  .panel\n"
        << "    border: 1px solid #d7dce2\n"
        << "    padding: 24px\n"
        << "    background: white\n\n"
        << "let title = " << nlohmann::json(description).dump() << "\n"
        << "let ready = true\n\n"
        << "when refresh\n"
        << "  let ready = true\n\n"
        << "page class \"app\"\n"
        << "  section class \"panel\"\n"
        << "    h1 title\n"
        << "    if ready\n"
        << "      text \"Ready to customize.\"\n"
        << "    button \"Refresh\" click refresh\n";
    return 0;
}

int cmdExplain(const Options& o) {
    const std::string source = readFile(o.inputFile);
    const SourceInsights insights = inspectSource(source);
    std::vector<std::unique_ptr<ASTNode>> program;
    {
        SilenceStdout silence;
        program = parseProgramFromFile(o.inputFile, o.syntax);
    }

    int elements = 0, functions = 0, values = 0, control = 0;
    for (const auto& node : program) {
        switch (node->getType()) {
            case ASTNodeType::JtmlElement: ++elements; break;
            case ASTNodeType::FunctionDeclaration: ++functions; break;
            case ASTNodeType::DefineStatement:
            case ASTNodeType::DeriveStatement: ++values; break;
            case ASTNodeType::IfStatement:
            case ASTNodeType::ForStatement:
            case ASTNodeType::WhileStatement: ++control; break;
            default: break;
        }
    }
    JtmlLinter linter;
    auto diagnostics = linter.lint(program);
    if (o.json) {
        int errors = 0;
        for (const auto& d : diagnostics) {
            if (d.severity == LintDiagnostic::Severity::Error) ++errors;
        }
        std::cout << nlohmann::json{
            {"ok", errors == 0},
            {"file", o.inputFile},
            {"summary", {
                {"topLevelStatements", program.size()},
                {"astElements", elements},
                {"astFunctions", functions},
                {"astValues", values},
                {"astControlFlow", control},
                {"styleBlocks", insights.styleBlocks},
            }},
            {"state", listJson(insights.state)},
            {"constants", listJson(insights.constants)},
            {"derived", listJson(insights.derived)},
            {"actions", listJson(insights.actions)},
            {"components", listJson(insights.components)},
            {"routes", listJson(insights.routes)},
            {"fetches", listJson(insights.fetches)},
            {"stores", listJson(insights.stores)},
            {"effects", listJson(insights.effects)},
            {"diagnostics", lintDiagnosticsToJson(diagnostics)},
        }.dump(2) << "\n";
        return errors == 0 ? 0 : 1;
    }

    std::cout << "JTML explanation: " << o.inputFile << "\n";
    std::cout << "- Parsed " << program.size() << " top-level statement(s).\n";
    std::cout << "- Values/computed values: " << values << "\n";
    std::cout << "- Actions/functions: " << functions << "\n";
    std::cout << "- Page/component roots: " << elements << "\n";
    std::cout << "- Top-level control flow: " << control << "\n";

    printInsightList("State", insights.state);
    printInsightList("Constants", insights.constants);
    printInsightList("Derived values", insights.derived);
    printInsightList("Actions", insights.actions);
    printInsightList("Components", insights.components);
    printInsightList("Routes", insights.routes);
    printInsightList("Fetch resources", insights.fetches);
    printInsightList("Stores", insights.stores);
    printInsightList("Effects", insights.effects);
    std::cout << "- Style blocks: " << insights.styleBlocks << "\n";

    if (diagnostics.empty()) {
        std::cout << "- Linter: no issues found.\n";
    } else {
        std::cout << "- Linter: " << diagnostics.size() << " issue(s).\n";
        for (const auto& d : diagnostics) {
            std::cout << "  " << (d.severity == LintDiagnostic::Severity::Error ? "error" : "warning")
                      << ": " << d.message << "\n";
        }
    }
    return 0;
}

int cmdSuggest(const Options& o) {
    std::vector<std::unique_ptr<ASTNode>> program;
    {
        SilenceStdout silence;
        program = parseProgramFromFile(o.inputFile, o.syntax);
    }

    JtmlLinter linter;
    auto diagnostics = linter.lint(program);
    std::cout << "JTML production suggestions: " << o.inputFile << "\n";
    if (!diagnostics.empty()) {
        std::cout << "- Fix lint diagnostics first; they are the highest-signal production risks.\n";
    }
    std::cout << "- Use `jtml fmt " << o.inputFile << " -w` before review.\n";
    std::cout << "- Use Friendly `style` blocks for page-local CSS instead of long inline style attributes.\n";
    std::cout << "- Prefer `make` components for repeated UI and keep state names local and explicit.\n";
    std::cout << "- Run `jtml dev " << std::filesystem::path(o.inputFile).parent_path().string()
              << "` for hot reload, then `jtml build "
              << std::filesystem::path(o.inputFile).parent_path().string()
              << " --out dist` before publishing.\n";
    std::cout << "- For AI edits, run `jtml generate \"describe the component\"` and paste the prompt into your model.\n";
    return diagnostics.empty() ? 0 : 1;
}

int cmdNew(const Options& o) {
    if (o.inputFile == "app") {
        createStarterApp(o);
        return 0;
    }
    if (o.outputFile.empty()) usage();
    writeFile(o.outputFile, kStarterProgram, o.force);
    std::cout << "Created " << o.outputFile << "\n"
              << "Try it with: jtml serve " << o.outputFile << " --port 8000\n";
    return 0;
}

int cmdAdd(const Options& o) {
    if (o.inputFile.empty()) usage();
    std::filesystem::path source = o.fromName.empty()
        ? std::filesystem::path(o.inputFile)
        : std::filesystem::path(o.fromName);
    std::string packageName = o.fromName.empty()
        ? packageNameFromPath(source)
        : o.inputFile;
    installLocalPackage(source, packageName, o.force, o.json);
    return 0;
}

int cmdInstall(const Options& o) {
    const auto result = verifyPackageLock(true);
    if (o.json) {
        std::cout << result.dump(2) << "\n";
    } else {
        if (result.value("ok", false)) {
            std::cout << "JTML packages verified from "
                      << result.value("lockfile", "jtml.lock.json") << "\n";
        } else {
            std::cout << "JTML package verification failed:\n";
            for (const auto& pkg : result["packages"]) {
                if (pkg.value("ok", false)) continue;
                std::cout << "  - " << pkg.value("name", "?") << ": "
                          << pkg.value("error", "unknown error") << "\n";
            }
        }
    }
    return result.value("ok", false) ? 0 : 1;
}

int cmdCheck(const Options& o) {
    try {
        {
            // Parse inside a silenced scope so the parser's debug logging does
            // not leak into the result line.
            SilenceStdout silence;
            (void)parseProgramFromFile(o.inputFile, o.syntax);
        }
        if (o.json) {
            std::cout << nlohmann::json{
                {"ok", true},
                {"file", o.inputFile},
                {"diagnostics", nlohmann::json::array()},
            }.dump(2) << "\n";
            return 0;
        }
        std::cout << "OK: " << o.inputFile << "\n";
        return 0;
    } catch (const std::exception& e) {
        auto diagnostics = jtml::diagnosticsFromMessageBlock(
            e.what(), jtml::DiagnosticSeverity::Error);
        const auto lineMap = friendlyLineMapForFile(o);
        if (!lineMap.empty()) {
            jtml::remapDiagnosticLines(diagnostics, lineMap);
        }
        if (o.json) {
            std::ostringstream remappedMessage;
            for (const auto& diagnostic : diagnostics) {
                remappedMessage << diagnostic.message << "\n";
            }
            auto out = nlohmann::json{
                {"ok", false},
                {"error", remappedMessage.str().empty() ? std::string(e.what()) : remappedMessage.str()},
                {"diagnostics", diagnosticsToJson(diagnostics)},
            };
            out["file"] = o.inputFile;
            std::cout << out.dump(2) << "\n";
        } else {
            for (const auto& diagnostic : diagnostics) {
                std::cerr << o.inputFile << ": "
                          << jtml::diagnosticSeverityName(diagnostic.severity)
                          << "[" << diagnostic.code << "]";
                if (diagnostic.line > 0) {
                    std::cerr << ":" << diagnostic.line;
                    if (diagnostic.column > 0) std::cerr << ":" << diagnostic.column;
                }
                std::cerr << ": " << diagnostic.message << "\n";
                if (!diagnostic.hint.empty()) {
                    std::cerr << "  hint: " << diagnostic.hint << "\n";
                }
            }
        }
        return 1;
    }
}

int cmdInterpret(const Options& o) {
    auto program = parseProgramFromFile(o.inputFile, o.syntax);
    JtmlTranspiler transpiler;
    Interpreter interpreter(transpiler);
    interpreter.interpret(program);
    return 0;
}

int cmdTranspile(const Options& o) {
    auto program = parseProgramFromFile(o.inputFile, o.syntax);
    JtmlTranspiler transpiler;
    std::string html = transpiler.transpile(program);

    if (!o.outputFile.empty()) {
        std::ofstream ofs(o.outputFile);
        if (!ofs.is_open())
            throw std::runtime_error("Cannot open output file: " + o.outputFile);
        ofs << html;
        std::cout << "Wrote transpiled HTML to " << o.outputFile << "\n";
    } else {
        std::cout << html << "\n";
    }
    return 0;
}

int cmdBuild(const Options& o) {
    if (o.inputFile.empty() || o.outputFile.empty()) usage();

    std::filesystem::path input = o.inputFile;
    const bool inputWasDirectory = std::filesystem::is_directory(input);
    std::filesystem::path inputDir = inputWasDirectory ? input : input.parent_path();
    if (std::filesystem::is_directory(input)) {
        input /= "index.jtml";
    }
    if (!std::filesystem::exists(input)) {
        throw std::runtime_error("Build input not found: " + input.string());
    }

    std::filesystem::path outDir = o.outputFile;
    std::filesystem::create_directories(outDir);
    std::filesystem::path outFile = outDir / "index.html";

    auto program = parseProgramFromFile(input.string(), o.syntax);
    JtmlTranspiler transpiler;
    std::string html = transpiler.transpile(program);

    std::ofstream ofs(outFile);
    if (!ofs.is_open()) {
        throw std::runtime_error("Cannot write build output: " + outFile.string());
    }
    ofs << html;

    std::cout << "Built " << input.string() << " -> " << outFile.string() << "\n";
    if (inputWasDirectory) {
        copyBuildAssets(inputDir, outDir);
    }
    return 0;
}

std::string jsTemplateLiteral(const std::string& value) {
    std::ostringstream out;
    out << '`';
    for (char ch : value) {
        if (ch == '`' || ch == '\\') out << '\\';
        if (ch == '$') out << '\\';
        out << ch;
    }
    out << '`';
    return out.str();
}

std::string exportReactComponent(const std::string& html) {
    std::ostringstream out;
    out << "const html = " << jsTemplateLiteral(html) << ";\n\n"
        << "export default function JtmlApp() {\n"
        << "  return (\n"
        << "    <iframe\n"
        << "      title=\"JTML app\"\n"
        << "      srcDoc={html}\n"
        << "      sandbox=\"allow-scripts allow-forms allow-same-origin\"\n"
        << "      style={{ width: \"100%\", minHeight: \"100vh\", border: 0 }}\n"
        << "    />\n"
        << "  );\n"
        << "}\n";
    return out.str();
}

std::string exportVueComponent(const std::string& html) {
    std::ostringstream out;
    out << "<template>\n"
        << "  <iframe\n"
        << "    title=\"JTML app\"\n"
        << "    :srcdoc=\"html\"\n"
        << "    sandbox=\"allow-scripts allow-forms allow-same-origin\"\n"
        << "    style=\"width: 100%; min-height: 100vh; border: 0\"\n"
        << "  />\n"
        << "</template>\n\n"
        << "<script setup>\n"
        << "const html = " << jsTemplateLiteral(html) << ";\n"
        << "</script>\n";
    return out.str();
}

std::string exportCustomElement(const std::string& html) {
    std::ostringstream out;
    out << "const html = " << jsTemplateLiteral(html) << ";\n\n"
        << "class JtmlAppElement extends HTMLElement {\n"
        << "  static get observedAttributes() { return [\"height\", \"sandbox\"]; }\n\n"
        << "  constructor() {\n"
        << "    super();\n"
        << "    this.attachShadow({ mode: \"open\" });\n"
        << "  }\n\n"
        << "  connectedCallback() {\n"
        << "    this.render();\n"
        << "  }\n\n"
        << "  attributeChangedCallback() {\n"
        << "    this.render();\n"
        << "  }\n\n"
        << "  get html() {\n"
        << "    return html;\n"
        << "  }\n\n"
        << "  reload() {\n"
        << "    const frame = this.shadowRoot && this.shadowRoot.querySelector(\"iframe\");\n"
        << "    if (frame) frame.srcdoc = html;\n"
        << "  }\n\n"
        << "  render() {\n"
        << "    if (!this.shadowRoot) return;\n"
        << "    const height = this.getAttribute(\"height\") || \"100vh\";\n"
        << "    const sandbox = this.getAttribute(\"sandbox\") || \"allow-scripts allow-forms allow-same-origin\";\n"
        << "    this.shadowRoot.innerHTML = `\n"
        << "      <style>\n"
        << "        :host { display: block; width: 100%; }\n"
        << "        iframe { width: 100%; min-height: ${height}; border: 0; display: block; }\n"
        << "      </style>\n"
        << "      <iframe title=\"JTML app\" sandbox=\"${sandbox.replace(/\"/g, \"&quot;\")}\"></iframe>\n"
        << "    `;\n"
        << "    this.reload();\n"
        << "  }\n"
        << "}\n\n"
        << "if (!customElements.get(\"jtml-app\")) {\n"
        << "  customElements.define(\"jtml-app\", JtmlAppElement);\n"
        << "}\n\n"
        << "export { JtmlAppElement };\n";
    return out.str();
}

int cmdExport(const Options& o) {
    if (o.inputFile.empty()) usage();
    auto program = parseProgramFromFile(o.inputFile, o.syntax);
    JtmlTranspiler transpiler;
    const std::string html = transpiler.transpile(program);

    std::string artifact;
    if (o.target == "html") {
        artifact = html;
    } else if (o.target == "react") {
        artifact = exportReactComponent(html);
    } else if (o.target == "vue") {
        artifact = exportVueComponent(html);
    } else if (o.target == "custom-element" || o.target == "web-component") {
        artifact = exportCustomElement(html);
    } else {
        throw std::runtime_error("Unsupported export target: " + o.target +
                                 " (expected html, react, vue, or custom-element)");
    }

    if (!o.outputFile.empty()) {
        std::ofstream out(o.outputFile);
        if (!out.is_open()) {
            throw std::runtime_error("Cannot write export output: " + o.outputFile);
        }
        out << artifact;
        std::cout << "Exported " << o.inputFile << " as " << o.target
                  << " -> " << o.outputFile << "\n";
    } else {
        std::cout << artifact;
    }
    return 0;
}

int cmdTest(const Options&) {
    std::filesystem::path examplesDir = "examples";
    if (!std::filesystem::exists(examplesDir)) {
        throw std::runtime_error("Cannot find examples/ from this working directory");
    }

    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(examplesDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".jtml") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    int failures = 0;
    int passed = 0;
    std::filesystem::path outDir = std::filesystem::temp_directory_path() / "jtml-smoke";
    std::filesystem::create_directories(outDir);

    for (const auto& file : files) {
        try {
            std::vector<std::unique_ptr<ASTNode>> program;
            {
                SilenceStdout silence;
                program = parseProgramFromFile(file.string());
            }

            JtmlLinter linter;
            auto diagnostics = linter.lint(program);
            bool hasLintError = std::any_of(
                diagnostics.begin(), diagnostics.end(),
                [](const LintDiagnostic& d) {
                    return d.severity == LintDiagnostic::Severity::Error;
                });

            JtmlTranspiler transpiler;
            std::string html = transpiler.transpile(program);
            if (html.find("<!DOCTYPE html>") == std::string::npos) {
                throw std::runtime_error("transpiler did not produce an HTML document");
            }

            std::filesystem::path outFile = outDir / (file.stem().string() + ".html");
            std::ofstream out(outFile);
            if (!out.is_open()) {
                throw std::runtime_error("cannot write smoke output: " + outFile.string());
            }
            out << html;

            if (hasLintError) {
                std::cout << "WARN: " << file.string()
                          << " parsed/transpiled, linter reported known limitations\n";
            } else {
                std::cout << "OK: " << file.string() << "\n";
            }
            ++passed;
        } catch (const std::exception& e) {
            std::cerr << "FAIL: " << file.string() << ": " << e.what() << "\n";
            ++failures;
        }
    }

    std::cout << "\nSmoke tested " << passed << " example(s)";
    if (failures > 0) {
        std::cout << ", " << failures << " failed\n";
        return 1;
    }
    std::cout << ". HTML output: " << outDir.string() << "\n";
    return 0;
}

} // namespace jtml::cli
