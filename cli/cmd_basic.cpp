// cli/cmd_basic.cpp — commands that don't own servers or multi-pass
// pipelines: examples, new, check, interpret, transpile. Each stays short
// and self-contained.
#include "commands.h"
#include "diagnostic_json.h"
#include "package_manager.h"

#include "jtml/interpreter.h"
#include "jtml/browser_runtime_emitter.h"
#include "jtml/language_catalog.h"
#include "jtml/linter.h"
#include "jtml/runtime_plan.h"
#include "jtml/runtime_plan_json.h"
#include "jtml/semantic.h"
#include "jtml/semantic/module_graph.h"
#include "jtml/semantic_project_json.h"
#include "jtml/runtime/static_update_plan_emitter.h"
#include "jtml/transpiler.h"
#include "json.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

namespace jtml::cli {
using jtml::JtmlLinter;

namespace {

bool pathIsInside(const std::filesystem::path& path, const std::filesystem::path& parent) {
    auto rel = std::filesystem::relative(path, parent);
    return rel.empty() || (rel.native().rfind("..", 0) != 0 && rel != ".");
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

nlohmann::json listJson(const std::vector<std::string>& values) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& value : values) out.push_back(value);
    return out;
}

nlohmann::json attributeKindCountsToJson(const jtml::AttributeKindCounts& counts) {
    return {
        {"literal",     counts.literal},
        {"boolean",     counts.boolean},
        {"reactive",    counts.reactive},
        {"event",       counts.event},
        {"special",     counts.special},
        {"passthrough", counts.passthrough},
    };
}

nlohmann::json semanticNodeCountsToJson(const jtml::SemanticProgram& semantic) {
    return {
        {"moduleFiles", semantic.moduleFiles.size()},
        {"state",      semantic.state.size()},
        {"constants",  semantic.constants.size()},
        {"derived",    semantic.derived.size()},
        {"actions",    semantic.actions.size()},
        {"fetches",    semantic.fetches.size()},
        {"routes",     semantic.routes.size()},
        {"effects",    semantic.effects.size()},
        {"imports",    semantic.imports.size()},
        {"externs",    semantic.externs.size()},
        {"components", semantic.components.size()},
        {"stores",     semantic.stores.size()},
        {"uiPrimitives", semantic.uiPrimitives.size()},
        {"authorThemeTokens", semantic.authorThemeTokenCount},
        {"themeTokens", semantic.themeTokenCount},
        {"rawStyleAttributes", semantic.rawStyleAttributeCount},
        {"semanticPrimitiveRawStyleAttributes", semantic.semanticPrimitiveRawStyleCount},
        {"timelines",  semantic.timelineCount},
        {"styleBlocks",semantic.styleBlocks},
        {"rawCssBlocks", semantic.rawCssBlocks},
        {"rawHtmlBlocks", semantic.rawHtmlBlocks},
    };
}

nlohmann::json semanticEdgesToJson(const jtml::SemanticProgram& semantic) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& edge : semantic.dependencies) {
        out.push_back({
            {"from", edge.from},
            {"to", edge.to},
            {"kind", edge.kind},
        });
    }
    return out;
}

nlohmann::json semanticUiModifiersToJson(const jtml::SemanticProgram& semantic) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& modifier : semantic.uiModifiers) {
        out.push_back({
            {"primitive", modifier.primitive},
            {"modifier", modifier.modifier},
            {"value", modifier.value},
        });
    }
    return out;
}

nlohmann::json semanticUiUsesToJson(const jtml::SemanticProgram& semantic) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& use : semantic.uiUses) {
        out.push_back({
            {"primitive", use.primitive},
            {"tagName", use.tagName},
            {"hasTitle", use.hasTitle},
            {"hasAriaLabel", use.hasAriaLabel},
            {"hasLabel", use.hasLabel},
            {"hasControl", use.hasControl},
            {"hasActionBinding", use.hasActionBinding},
            {"hasNavigationTarget", use.hasNavigationTarget},
            {"hasTabChild", use.hasTabChild},
            {"hasDismissAction", use.hasDismissAction},
        });
    }
    return out;
}

nlohmann::json semanticUiToJson(const jtml::SemanticProgram& semantic) {
    return {
        {"primitives", listJson(semantic.uiPrimitives)},
        {"uses", semanticUiUsesToJson(semantic)},
        {"modifiers", semanticUiModifiersToJson(semantic)},
        {"authorThemeTokens", semantic.authorThemeTokenCount},
        {"themeTokens", semantic.themeTokenCount},
        {"styleBlocks", semantic.styleBlocks},
        {"rawStyleAttributes", semantic.rawStyleAttributeCount},
        {"semanticPrimitiveRawStyleAttributes", semantic.semanticPrimitiveRawStyleCount},
        {"rawCssBlocks", semantic.rawCssBlocks},
        {"rawHtmlBlocks", semantic.rawHtmlBlocks},
    };
}

nlohmann::json semanticRouteRecordsToJson(const jtml::SemanticProgram& semantic) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& route : semantic.routeRecords) {
        out.push_back({
            {"path", route.path},
            {"component", route.component},
            {"params", route.params},
            {"loads", route.loads},
        });
    }
    return out;
}

nlohmann::json semanticFetchRecordsToJson(const jtml::SemanticProgram& semantic) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& fetch : semantic.fetchRecords) {
        out.push_back({
            {"name", fetch.name},
            {"url", fetch.url},
            {"method", fetch.method},
            {"bodyExpr", fetch.bodyExpr},
            {"refreshAction", fetch.refreshAction},
            {"cache", fetch.cache},
            {"credentials", fetch.credentials},
            {"timeoutMs", fetch.timeoutMs},
            {"retryCount", fetch.retryCount},
            {"stalePolicy", fetch.stalePolicy},
            {"group", fetch.group},
            {"cacheKeyExpr", fetch.cacheKeyExpr},
            {"revalidateMs", fetch.revalidateMs},
            {"dedupe", fetch.dedupe},
            {"background", fetch.background},
            {"lazy", fetch.lazy},
        });
    }
    return out;
}

std::string injectBrowserBuildAssetScript(std::string html,
                                          const std::string& assetName) {
    const std::string tag = "<script src=\"./" + assetName + "\" defer></script>\n";
    const auto body = html.rfind("</body>");
    if (body != std::string::npos) {
        html.insert(body, tag);
        return html;
    }
    html += "\n" + tag;
    return html;
}

void writeBuildAsset(const std::filesystem::path& path,
                     const std::string& content,
                     const std::string& label) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("Cannot write " + label + ": " + path.string());
    }
    out << content;
}

nlohmann::json semanticImportRecordsToJson(const jtml::SemanticProgram& semantic) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& import : semantic.importRecords) {
        out.push_back({
            {"specifier", import.specifier},
            {"kind", import.kind},
            {"names", import.names},
            {"reExport", import.reExport},
            {"sourceLine", import.sourceLine},
            {"sourceColumn", import.sourceColumn},
        });
    }
    return out;
}

nlohmann::json semanticPropertiesToJson(const std::vector<jtml::SemanticProperty>& properties) {
    nlohmann::json out = nlohmann::json::object();
    for (const auto& property : properties) out[property.name] = property.value;
    return out;
}

nlohmann::json semanticComponentDefinitionsToJson(const jtml::SemanticProgram& semantic) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& definition : semantic.componentDefinitions) {
        out.push_back({
            {"name", definition.name},
            {"params", definition.params},
            {"emits", definition.emits},
            {"emitArity", definition.emitArity},
            {"localState", definition.localState},
            {"localDerived", definition.localDerived},
            {"localActions", definition.localActions},
            {"localEffects", definition.localEffects},
            {"eventBindings", definition.eventBindings},
            {"bodyHex", definition.bodyHex},
            {"hasSlot", definition.hasSlot},
            {"bodyNodeCount", definition.bodyNodeCount},
            {"rootTemplateNodeCount", definition.rootTemplateNodeCount},
            {"slotCount", definition.slotCount},
            {"sourceLine", definition.sourceLine},
        });
    }
    return out;
}

nlohmann::json semanticComponentInstancesToJson(const jtml::SemanticProgram& semantic) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& instance : semantic.componentInstances) {
        out.push_back({
            {"id", instance.id},
            {"component", instance.component},
            {"instanceId", instance.instanceId},
            {"role", instance.role},
            {"params", semanticPropertiesToJson(instance.params)},
            {"locals", semanticPropertiesToJson(instance.locals)},
            {"slotHex", instance.slotHex},
            {"sourceLine", instance.sourceLine},
        });
    }
    return out;
}

std::string joinDisplay(const std::vector<std::string>& values,
                        const std::string& empty = "(none)") {
    if (values.empty()) return empty;
    std::ostringstream out;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) out << ", ";
        out << values[i];
    }
    return out.str();
}

std::string compactProjectPath(const std::string& path) {
    if (path.empty()) return "(unknown)";
    const std::filesystem::path fsPath(path);
    if (!fsPath.is_absolute()) return fsPath.generic_string();
    try {
        const auto rel = std::filesystem::relative(fsPath, std::filesystem::current_path());
        const auto relText = rel.generic_string();
        if (!relText.empty() && relText.rfind("..", 0) != 0 && relText != ".") {
            return relText;
        }
    } catch (...) {
    }
    return fsPath.generic_string();
}

std::string importLabel(const jtml::SemanticModuleImport& import) {
    std::ostringstream out;
    if (!import.names.empty()) {
        out << joinDisplay(import.names) << " ";
    } else if (!import.kind.empty()) {
        out << import.kind << " ";
    }
    out << "from \"" << import.specifier << "\"";
    if (!import.resolvedSymbols.empty()) {
        std::vector<std::string> symbols;
        symbols.reserve(import.resolvedSymbols.size());
        for (const auto& symbol : import.resolvedSymbols) {
            symbols.push_back((symbol.kind.empty() ? "symbol" : symbol.kind) + " " + symbol.name);
        }
        out << " [" << joinDisplay(symbols) << "]";
    }
    return out.str();
}

std::string exportLabel(const jtml::SemanticExport& exportRecord) {
    if (exportRecord.kind.empty()) return exportRecord.name;
    if (exportRecord.name.empty()) return exportRecord.kind;
    return exportRecord.kind + " " + exportRecord.name;
}

struct ParsedModuleForExplain {
    std::vector<std::unique_ptr<ASTNode>> program;
    std::string syntax;
};

std::string trimDisplay(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

std::pair<std::size_t, std::size_t> locateModuleParseError(
        const std::string& source,
        const std::string& message) {
    const auto diagnostic = jtml::diagnosticFromMessage(message);
    if (diagnostic.line > 0) {
        return {
            static_cast<std::size_t>(diagnostic.line),
            diagnostic.column > 0 ? static_cast<std::size_t>(diagnostic.column) : 0,
        };
    }

    const std::string marker = "friendly JTML line:";
    const auto markerPos = message.find(marker);
    if (markerPos == std::string::npos) return {0, 0};
    const std::string fragment = trimDisplay(message.substr(markerPos + marker.size()));
    if (fragment.empty()) return {0, 0};

    std::istringstream input(source);
    std::string line;
    std::size_t lineNo = 0;
    while (std::getline(input, line)) {
        ++lineNo;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto trimmed = trimDisplay(line);
        if (trimmed == fragment || trimmed.find(fragment) != std::string::npos ||
            fragment.find(trimmed) != std::string::npos) {
            const auto column = line.find_first_not_of(" \t");
            return {lineNo, column == std::string::npos ? 1 : column + 1};
        }
    }
    return {0, 0};
}

std::string importedComponentStubsForExplain(const std::string& source) {
    const auto semantic = jtml::analyzeSemanticProgram({}, source);
    std::map<std::string, size_t> names;
    for (const auto& import : semantic.importRecords) {
        for (const auto& name : import.names) {
            if (!name.empty() && std::isupper(static_cast<unsigned char>(name.front()))) {
                names.emplace(name, 0);
            }
        }
    }
    if (names.empty()) return "";

    std::istringstream input(source);
    std::string line;
    while (std::getline(input, line)) {
        const auto text = trimDisplay(line);
        if (text.empty() || !std::isupper(static_cast<unsigned char>(text.front()))) continue;
        std::istringstream words(text);
        std::string name;
        words >> name;
        auto found = names.find(name);
        if (found == names.end()) continue;
        std::string rest;
        std::getline(words, rest);
        rest = trimDisplay(rest);
        if (rest.empty()) continue;

        size_t args = 0;
        bool inString = false;
        bool sawQuotedArg = false;
        for (size_t i = 0; i < rest.size(); ++i) {
            if (rest[i] == '"' && (i == 0 || rest[i - 1] != '\\')) {
                inString = !inString;
                if (inString) {
                    ++args;
                    sawQuotedArg = true;
                }
            }
        }
        if (!sawQuotedArg) {
            std::istringstream restWords(rest);
            std::string arg;
            while (restWords >> arg) ++args;
        }
        found->second = std::max(found->second, args);
    }

    std::ostringstream out;
    out << "\n";
    for (const auto& [name, argCount] : names) {
        out << "make " << name;
        for (size_t i = 0; i < argCount; ++i) {
            out << " arg" << (i + 1);
        }
        out << "\n"
            << "  page\n"
            << "    text \"\"\n\n";
    }
    return out.str();
}

std::vector<std::unique_ptr<ASTNode>> parseStandaloneNormalizedSource(const std::string& source) {
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    const auto& lexErrors = lexer.getErrors();
    if (!lexErrors.empty()) {
        std::ostringstream oss;
        for (const auto& error : lexErrors) oss << error << "\n";
        throw std::runtime_error(oss.str());
    }

    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    const auto& parseErrors = parser.getErrors();
    if (!parseErrors.empty()) {
        std::ostringstream oss;
        for (const auto& error : parseErrors) oss << error << "\n";
        throw std::runtime_error(oss.str());
    }
    return program;
}

ParsedModuleForExplain parseStandaloneModuleForExplain(const std::string& source,
                                                       SyntaxMode syntax) {
    const bool friendly =
        syntax == SyntaxMode::Friendly ||
        (syntax == SyntaxMode::Auto &&
         (isFriendlySyntax(source) || looksLikeFriendlySyntax(source)));
    if (!friendly) {
        return {parseStandaloneNormalizedSource(normalizeSourceSyntax(source, SyntaxMode::Classic)),
                "classic"};
    }

    try {
        return {parseStandaloneNormalizedSource(normalizeSourceSyntax(source, SyntaxMode::Friendly)),
                "friendly"};
    } catch (const std::exception& firstError) {
        const auto stubs = importedComponentStubsForExplain(source);
        if (stubs.empty()) throw;
        try {
            return {parseStandaloneNormalizedSource(
                        normalizeSourceSyntax(source + stubs, SyntaxMode::Friendly)),
                    "friendly+import-stubs"};
        } catch (...) {
            throw firstError;
        }
    }
}

jtml::RuntimeProjectPlan buildRuntimeProjectPlanForInput(
        const std::string& inputFile,
        SyntaxMode syntax,
        const std::vector<std::unique_ptr<ASTNode>>& linkedProgram,
        const jtml::SemanticProgram& linkedSemantic) {
    std::set<std::string> seenModuleFiles;
    std::vector<jtml::SemanticModuleSource> moduleSources;
    for (const auto& file : collectSourceFiles(inputFile, syntax)) {
        const auto path = file.generic_string();
        if (!seenModuleFiles.insert(path).second) continue;

        const auto moduleSource = readFile(path);
        jtml::SemanticModuleIr ir;
        jtml::SemanticProgram moduleSemantic;
        std::shared_ptr<const jtml::SemanticModuleAst> moduleAst;
        try {
            SilenceStdout silence;
            auto parsedModule = parseStandaloneModuleForExplain(moduleSource, syntax);
            ir = jtml::summarizeSemanticAst(parsedModule.program, parsedModule.syntax);
            moduleAst = jtml::cloneSemanticAst(parsedModule.program, parsedModule.syntax);
            moduleSemantic = parsedModule.syntax == "friendly+import-stubs"
                ? jtml::analyzeSemanticProgram({}, moduleSource)
                : jtml::analyzeSemanticProgram(parsedModule.program, moduleSource);
        } catch (const std::exception& e) {
            moduleSemantic = jtml::analyzeSemanticProgram({}, moduleSource);
            ir.available = false;
            ir.syntax = "unknown";
            ir.parseError = e.what();
            const auto location = locateModuleParseError(moduleSource, ir.parseError);
            ir.parseErrorLine = location.first;
            ir.parseErrorColumn = location.second;
        }

        moduleSources.push_back({
            path,
            std::move(moduleSemantic),
            std::move(ir),
            std::move(moduleAst),
        });
    }

    const auto project = moduleSources.empty()
        ? jtml::buildSemanticProject(linkedSemantic, inputFile)
        : jtml::buildSemanticProject(moduleSources, inputFile, linkedSemantic);
    return jtml::buildRuntimePlan(
        project,
        jtml::buildRuntimePlan(linkedProgram, linkedSemantic));
}

struct DepthSummary {
    int level = 0;
    std::string label = "static";
};

DepthSummary classifyDepth(const jtml::SemanticProgram& semantic) {
    int depth = 0;
    if (!semantic.state.empty() || !semantic.constants.empty()) depth = std::max(depth, 1);
    if (!semantic.derived.empty()) depth = std::max(depth, 2);
    if (!semantic.actions.empty()) depth = std::max(depth, 3);
    if (!semantic.fetches.empty()) depth = std::max(depth, 4);
    if (!semantic.routes.empty()) depth = std::max(depth, 5);
    if (!semantic.components.empty()) depth = std::max(depth, 6);
    if (!semantic.effects.empty() || semantic.timelineCount > 0 ||
        !semantic.stores.empty() || !semantic.uiPrimitives.empty()) {
        depth = std::max(depth, 7);
    }

    static const std::array<std::string, 9> labels = {
        "static",
        "stateful",
        "reactive",
        "interactive",
        "data-driven",
        "routed",
        "composed",
        "full-featured",
        "full-stack",
    };
    return {depth, labels[std::min(static_cast<size_t>(depth), labels.size() - 1)]};
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
    struct Gate {
        std::string id;
        std::string label;
        std::string command;
        std::string status;
    };
    struct Tier {
        std::string name;
        std::vector<std::string> items;
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
        {"runtime benchmark script", "scripts/benchmark_runtime.sh", true},
        {"browser runtime benchmark script", "scripts/benchmark_browser_runtime.sh", true},
        {"verification script", "scripts/verify_all.sh", true},
    };
    const std::vector<Gate> gates = {
        {"build", "Build CLI and tests", "cmake --build build --target jtml_tests jtml_cli -j4", "required"},
        {"unit", "Unit test suite", "ctest --test-dir build --output-on-failure", "required"},
        {"examples", "Examples and tutorial smoke", "./build/jtml test", "required"},
        {"predeploy", "Full local predeploy verification", "scripts/verify_all.sh", "required"},
        {"docs", "Docs and catalog consistency", "ctest --test-dir build --output-on-failure -R LanguageSurface", "required"},
        {"release", "Release archive build", "scripts/verify_all.sh", "required"},
        {"asan-ubsan", "Sanitizer build", "cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DJTML_ENABLE_SANITIZERS=ON", "planned"},
        {"coverage", "Coverage report", "planned", "planned"},
        {"benchmarks", "Runtime and compiler benchmark smoke", "scripts/benchmark_runtime.sh", "required"},
        {"browser-benchmarks", "Headless browser click/runtime benchmark", "scripts/benchmark_browser_runtime.sh", "required"},
    };
    const std::vector<Tier> tiers = {
        {"stable", {
            "Friendly JTML 2 authoring surface",
            "Classic compatibility backend",
            "CLI check/lint/fmt/test/build/serve",
            "semantic keyword and UI catalogs",
            "local package install/lockfile first slice",
        }},
        {"first_slice", {
            "fetch lifecycle, cache identity, groups, invalidation, and timed revalidation",
            "hash routing, guards, layouts, route-load fetches, and active route state",
            "semantic UI primitives, theme tokens, accessibility lints, and generated CSS",
            "media, charts, graphics, and scene3d mount contracts",
            "Studio as the local language hub",
        }},
        {"experimental", {
            "jtl 1 core-language experiments",
            "first browser-local direct component body-plan execution",
            "component body-plan parity before expanded compatibility removal",
            "advanced browser-local runtime parity",
            "compiler-first browser production target with fine-grained updates (planned)",
            "extern/custom-element/framework export boundaries",
            "contract-first JTL API modules for governed backend operations (planned)",
            "remote package registry and semantic version solving",
        }},
    };
    const std::vector<std::string> nextTargets = {
        "complete direct ComponentInstance body-plan parity",
        "browser-local production runtime parity with live runtime semantics",
        "contract-first JTL API design: types, errors, operation signatures, OpenAPI generation, policies, adapters, and JTML fetch/call integration",
        "Studio content externalization out of embedded C++ literals",
        "internal module boundaries for friendly, semantic, runtime, emit, lsp, and studio code",
        "security, release, deprecation, benchmark, and compatibility policies",
    };
    const nlohmann::json runtimeCapabilities = {
        {"runtimeAssembly", {
            {"browserRuntimeEmitterWrapper", true},
            {"browserRuntimeAssetChunks", true},
        }},
        {"moduleSystem", {
            {"relativeImportsFromImporter", true},
            {"recoverableModuleParseIssues", true},
            {"sourceFirstImportIssueSpans", true},
            {"moduleScopedComponentIdentity", true},
        }},
        {"directComponentExecution", {
            {"browserLocalFirstSlice", true},
            {"bodyPlanTemplates", true},
            {"ifElseFor", true},
            {"actionWhile", true},
            {"templateWhileActionOnly", true},
            {"slots", true},
            {"nestedComponentCalls", true},
            {"commonAttributes", true},
            {"simpleActionArguments", true},
            {"actionLocalDeclarations", true},
            {"bodyPlanActionExpressionPlans", true},
            {"canonicalRuntimeExpressionPlans", true},
            {"astOwnedRuntimeExpressionPlans", true},
            {"richerAstExpressionPlanShapes", true},
            {"memberWriteDependencyRoots", true},
            {"memberAssignmentDirectMutation", true},
            {"memberAssignmentDeepDictCreation", true},
            {"memberAssignmentDeepArrayCreation", true},
            {"liveLoopValueParity", true},
            {"metadataDrivenLeafPatches", true},
            {"metadataDrivenTextAndAttributePatches", true},
            {"cachedCompiledUpdatePlans", true},
            {"compiledPatchOperations", true},
            {"precompiledPatchOperationShapes", true},
            {"precompiledCompositeExpressions", true},
            {"compiledUpdateFunctions", true},
            {"indexedCompiledUpdateFunctions", true},
            {"generatedProductionUpdateFunctions", true},
            {"staticComponentCreateUpdateModules", true},
            {"componentModulePlanIndexAsset", true},
            {"legacyUpdatePlanCompatibilityAsset", true},
            {"directStaticComponentCreateHtml", true},
            {"directStaticContainerCreateHtml", true},
            {"directStaticControlFlowCreateHtml", true},
            {"directStaticSlotCreateHtml", true},
            {"directStaticNestedComponentCreateHtml", true},
            {"directStaticSlotHtmlFastPath", true},
            {"helperIndependentSlotNestedPatchFunctions", true},
            {"helperIndependentStaticUpdateFunctions", true},
            {"containerAttributePatchOperations", true},
            {"controlFlowRegionPatchOperations", true},
            {"directStaticControlFlowRegionPatches", true},
            {"slotRegionPatchOperations", true},
            {"nestedComponentPatchOperations", true},
            {"directStaticSlotRegionPatches", true},
            {"directStaticNestedComponentPatches", true},
            {"directStaticNestedParamPatches", true},
            {"legacyHeuristicPatchFallback", false},
            {"keyedListRegionMarkers", true},
            {"keyedListLifecycleTelemetry", true},
            {"keyedForRegionPatch", true},
            {"keyedListBelowWrapperReconciliation", true},
            {"keyedListChildMarkerReconciliation", true},
            {"keyedListPrunesRemovedDynamicChildren", true},
            {"liveBodyPlanPatchTelemetry", true},
            {"liveBodyPlanExpressionPlans", true},
            {"liveBodyPlanPrimaryTransportContract", true},
            {"sourceFirstBodyPlanColumns", true},
            {"browserSourceFirstFallbackContext", true},
            {"browserSourceFirstCreateFallbackContext", true},
            {"liveSourceFirstFallbackContext", true},
            {"liveInterpreterParity", false},
            {"fullParity", false},
        }},
        {"contractFirstJtlApis", {
            {"planned", true},
            {"implemented", false},
            {"syntax", "planned"},
        }},
        {"performanceTarget", {
            {"benchmarkPath", "compiler-first browser production target"},
            {"liveHtmlPatchPath", "dev/internal runtime backend"},
            {"optimizedJsCompiler", "planned"},
            {"fineGrainedUpdates", "cached browser-local indexed compiled update functions with generated production update-function source, parsed expression dependency metadata, canonical runtime expression plans for literal/path/unary/binary/conditional expressions, precompiled text, attribute, container patch operation, safe control-flow create/patch operation shapes, direct slot/nested component node create/patch contracts, in-place nested component parameter/body patches, and direct region replacement for safe if/keyed-for regions; legacy heuristic patch fallback removed; optimized compiler planned"},
            {"bodyPlanReadWriteMetadata", true},
            {"typedExpressionDependencyAnalysis", true},
            {"memberSubscriptReadPaths", true},
            {"cspSafeDefaultUpdatePlans", true},
            {"staticUpdatePlanBuildArtifact", "jtml-update-plans.js"},
            {"browserRuntimeBuildArtifact", "jtml-runtime.js"},
            {"componentModuleBuildArtifact", "components/index.js"},
            {"appBootstrapBuildArtifact", "app.js"},
            {"staticUpdatePlanPrecomputedIndexes", true},
            {"staticComponentModules", true},
            {"componentModulePlanIndex", true},
            {"legacyUpdatePlanCompatibilityAsset", true},
            {"benchmarkSmoke", "scripts/benchmark_runtime.sh"},
            {"browserRuntimeBenchmark", "scripts/benchmark_browser_runtime.sh"},
            {"browserAssetBudgets", "first-slice index/runtime/component/app/update-plan byte budgets"},
            {"dynamicGeneratedUpdateFunctions", "explicit opt-in bridge only"},
            {"keyedListDiffing", "first-slice direct list item key/index markers, lifecycle telemetry, conservative keyed for-region patching, below-wrapper element/text reconciliation for retained keyed items, child body-node marker reconciliation below retained wrappers, retained/inserted/removed/moved key reporting, and removed nested dynamic child pruning; optimized DOM diff planned"},
            {"prodDevRuntimeSplit", "external jtml-runtime.js is primary for browser build; embedded runtime retained for transpile/live compatibility"},
            {"liveBodyPlanTransport", "body-plan primary for supported live components with compatibility DOM only as explicit unsupported fallback"},
        }},
    };
    auto findProjectRoot = [] {
        fs::path cursor = fs::current_path();
        while (true) {
            if (fs::exists(cursor / "CMakeLists.txt") &&
                fs::exists(cursor / "include" / "jtml") &&
                fs::exists(cursor / "src")) {
                return cursor;
            }
            if (!cursor.has_parent_path() || cursor.parent_path() == cursor) break;
            cursor = cursor.parent_path();
        }
        return fs::current_path();
    };
    const fs::path projectRoot = findProjectRoot();

    bool ok = true;
    nlohmann::json results = nlohmann::json::array();
    if (!o.json) std::cout << "JTML doctor\nProject root: " << projectRoot.string() << "\n";
    for (const auto& check : checks) {
        const fs::path actualPath = projectRoot / check.path;
        bool exists = fs::exists(actualPath);
        bool pass = exists;
        if (check.executable && exists) {
            auto perms = fs::status(actualPath).permissions();
            pass = (perms & fs::perms::owner_exec) != fs::perms::none;
        }
        ok = ok && pass;
        results.push_back({
            {"label", check.label},
            {"path", check.path.string()},
            {"actualPath", actualPath.string()},
            {"requiredExecutable", check.executable},
            {"ok", pass},
        });
        if (!o.json) {
            std::cout << (pass ? "OK   " : "MISS ") << check.label
                      << " (" << check.path.string() << ")\n";
        }
    }

    nlohmann::json gateJson = nlohmann::json::array();
    for (const auto& gate : gates) {
        gateJson.push_back({
            {"id", gate.id},
            {"label", gate.label},
            {"command", gate.command},
            {"status", gate.status},
        });
    }

    nlohmann::json tierJson = nlohmann::json::array();
    for (const auto& tier : tiers) {
        tierJson.push_back({
            {"name", tier.name},
            {"items", tier.items},
        });
    }

    if (o.json) {
        nlohmann::json out = {
            {"ok", ok},
            {"version", versionString()},
            {"projectRoot", projectRoot.string()},
            {"enterpriseReady", false},
            {"readiness", "promising observable-first language platform; not enterprise-ready yet"},
            {"architectureSourceOfTruth", "Friendly JTML -> typed AST -> semantic IR -> observable graph -> backends"},
            {"runtimeCapabilities", runtimeCapabilities},
            {"stabilityTiers", tierJson},
            {"verificationGates", gateJson},
            {"nextArchitectureTargets", nextTargets},
            {"checks", results},
        };
        std::cout << out.dump(2) << "\n";
        return ok ? 0 : 1;
    }

    if (!ok) {
        std::cout << "Doctor found missing local toolkit pieces.\n";
        return 1;
    }
    std::cout << "Local toolkit shape is complete.\n"
              << "Readiness: promising observable-first language platform; not enterprise-ready yet.\n"
              << "Required gates: build, unit, examples/tutorial smoke, docs/catalog consistency, full predeploy verification.\n"
              << "Next architecture target: complete direct ComponentInstance body-plan parity.\n"
              << "For full verification run scripts/verify_all.sh\n";
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

int cmdKeywords(const Options& o) {
    const auto& catalog = jtml::languageCatalog();

    if (o.json) {
        nlohmann::json groups = nlohmann::json::array();
        for (const auto& group : catalog.friendlyGroups) {
            groups.push_back({
                {"area", group.area},
                {"keywords", group.keywords},
            });
        }
        std::cout << nlohmann::json{
            {"dialect", "Friendly JTML 2"},
            {"sourceOfTruth", "Friendly is canonical; Classic is backend-only compatibility"},
            {"friendlyGroups", groups},
            {"friendlyKeywords", jtml::friendlyKeywords()},
            {"compatibilityBackendKeywords", catalog.compatibilityBackendKeywords},
            {"eventAttributes", catalog.eventAttributes},
        }.dump(2) << "\n";
        return 0;
    }

    std::cout << "Friendly JTML 2 keyword catalog\n"
              << "Friendly is canonical. Classic words are backend-only compatibility.\n\n";
    for (const auto& group : catalog.friendlyGroups) {
        std::cout << group.area << ":\n  ";
        for (size_t i = 0; i < group.keywords.size(); ++i) {
            if (i > 0) std::cout << ' ';
            std::cout << group.keywords[i];
        }
        std::cout << "\n";
    }
    std::cout << "\ncompatibility backend:\n  ";
    for (size_t i = 0; i < catalog.compatibilityBackendKeywords.size(); ++i) {
        if (i > 0) std::cout << ' ';
        std::cout << catalog.compatibilityBackendKeywords[i];
    }
    std::cout << "\n";
    return 0;
}

int cmdUi(const Options& o) {
    const auto& ui = jtml::semanticUiCatalog();

    if (o.json) {
        nlohmann::json primitives = nlohmann::json::array();
        for (const auto& primitive : ui.primitives) {
            primitives.push_back({
                {"name", primitive.name},
                {"category", primitive.category},
                {"lowersTo", primitive.lowersTo},
                {"commonModifiers", primitive.commonModifiers},
                {"description", primitive.description},
            });
        }

        nlohmann::json modifiers = nlohmann::json::array();
        for (const auto& modifier : ui.modifiers) {
            modifiers.push_back({
                {"name", modifier.name},
                {"appliesTo", modifier.appliesTo},
                {"values", modifier.values},
                {"description", modifier.description},
            });
        }

        std::cout << nlohmann::json{
            {"dialect", "Friendly JTML 2"},
            {"sourceOfTruth", "Semantic UI is the canonical visual-intent layer; raw CSS remains an escape hatch"},
            {"primitives", primitives},
            {"modifiers", modifiers},
            {"themeTokenKinds", ui.themeTokenKinds},
        }.dump(2) << "\n";
        return 0;
    }

    std::cout << "Friendly JTML 2 semantic UI catalog\n"
              << "Semantic UI expresses visual intent. Use raw CSS for host/platform escape hatches.\n\n";

    std::map<std::string, std::vector<const jtml::SemanticUiPrimitiveSpec*>> primitivesByCategory;
    for (const auto& primitive : ui.primitives) {
        primitivesByCategory[primitive.category].push_back(&primitive);
    }
    for (const auto& [category, primitives] : primitivesByCategory) {
        std::cout << category << ":\n";
        for (const auto* primitive : primitives) {
            std::cout << "  " << primitive->name << " -> " << primitive->lowersTo
                      << "  " << primitive->description << "\n";
        }
    }

    std::cout << "\nmodifiers:\n";
    for (const auto& modifier : ui.modifiers) {
        std::cout << "  " << modifier.name << " (" << modifier.appliesTo << "): ";
        for (size_t i = 0; i < modifier.values.size(); ++i) {
            if (i > 0) std::cout << ' ';
            std::cout << modifier.values[i];
        }
        std::cout << "\n";
    }

    std::cout << "\ntheme token kinds:\n  ";
    for (size_t i = 0; i < ui.themeTokenKinds.size(); ++i) {
        if (i > 0) std::cout << ' ';
        std::cout << ui.themeTokenKinds[i];
    }
    std::cout << "\n";
    return 0;
}

int cmdExplain(const Options& o) {
    const std::string source = readFile(o.inputFile);

    std::vector<std::unique_ptr<ASTNode>> program;
    {
        SilenceStdout silence;
        try {
            program = parseProgramFromFile(o.inputFile, o.syntax);
        } catch (const std::exception&) {
            auto parsedEntry = parseStandaloneModuleForExplain(source, o.syntax);
            program = std::move(parsedEntry.program);
        }
    }
    JtmlLinter linter;
    auto diagnostics = linter.lint(program);
    auto semantic = jtml::analyzeSemanticProgram(program, source);
    std::set<std::string> seenModuleFiles;
    std::vector<jtml::SemanticModuleSource> moduleSources;
    for (const auto& file : collectSourceFilesRecoverable(o.inputFile, o.syntax)) {
        const auto path = file.generic_string();
        if (seenModuleFiles.insert(path).second) {
            semantic.moduleFiles.push_back(path);
            const auto moduleSource = readFile(path);
            jtml::SemanticModuleIr ir;
            jtml::SemanticProgram moduleSemantic;
            std::shared_ptr<const jtml::SemanticModuleAst> moduleAst;
            try {
                SilenceStdout silence;
                auto parsedModule = parseStandaloneModuleForExplain(moduleSource, o.syntax);
                ir = jtml::summarizeSemanticAst(parsedModule.program, parsedModule.syntax);
                moduleAst = jtml::cloneSemanticAst(parsedModule.program, parsedModule.syntax);
                moduleSemantic = parsedModule.syntax == "friendly+import-stubs"
                    ? jtml::analyzeSemanticProgram({}, moduleSource)
                    : jtml::analyzeSemanticProgram(parsedModule.program, moduleSource);
            } catch (const std::exception& e) {
                moduleSemantic = jtml::analyzeSemanticProgram({}, moduleSource);
                ir.available = false;
                ir.syntax = "unknown";
                ir.parseError = e.what();
                const auto location = locateModuleParseError(moduleSource, ir.parseError);
                ir.parseErrorLine = location.first;
                ir.parseErrorColumn = location.second;
            }
            moduleSources.push_back({
                path,
                std::move(moduleSemantic),
                std::move(ir),
                std::move(moduleAst),
            });
        }
    }
    const auto usage = jtml::analyzeSemanticUsage(semantic);
    const auto project = moduleSources.empty()
        ? jtml::buildSemanticProject(semantic, o.inputFile)
        : jtml::buildSemanticProject(moduleSources, o.inputFile, semantic);
    const auto projectIssues = jtml::analyzeSemanticProject(project);
    const auto runtimePlan = jtml::buildRuntimePlan(program, semantic);
    const auto runtimeProjectPlan = jtml::buildRuntimePlan(project, runtimePlan);
    const auto depth = classifyDepth(semantic);

    // Build action profiles map for easy lookup
    std::map<std::string, const jtml::SemanticActionProfile*> profMap;
    for (const auto& p : usage.actionProfiles) profMap[p.name] = &p;

    int lintErrors = 0;
    for (const auto& d : diagnostics)
        if (d.severity == LintDiagnostic::Severity::Error) ++lintErrors;

    // ── JSON output ────────────────────────────────────────────────────
    if (o.json) {
        // Action profile array
        nlohmann::json actionArr = nlohmann::json::array();
        for (const auto& p : usage.actionProfiles) {
            actionArr.push_back({
                {"name", p.name},
                {"writes", listJson(p.writes)},
                {"reads",  listJson(p.reads)},
                {"triggers", listJson(p.triggers)},
                {"hasVisibleEffect", p.hasVisibleEffect}
            });
        }
        std::cout << nlohmann::json{
            {"ok", lintErrors == 0},
            {"file", o.inputFile},
            {"depth", {{"level", depth.level}, {"label", depth.label}}},
            {"complexity", {
                {"moduleFiles", semantic.moduleFiles.size()},
                {"state",      semantic.state.size()},
                {"constants",  semantic.constants.size()},
                {"derived",    semantic.derived.size()},
                {"actions",    semantic.actions.size()},
                {"fetches",    semantic.fetches.size()},
                {"routes",     semantic.routes.size()},
                {"effects",    semantic.effects.size()},
                {"imports",    semantic.imports.size()},
                {"externs",    semantic.externs.size()},
                {"components", semantic.components.size()},
                {"stores",     semantic.stores.size()},
                {"uiPrimitives", semantic.uiPrimitives.size()},
                {"authorThemeTokens", semantic.authorThemeTokenCount},
                {"themeTokens", semantic.themeTokenCount},
                {"rawStyleAttributes", semantic.rawStyleAttributeCount},
                {"semanticPrimitiveRawStyleAttributes", semantic.semanticPrimitiveRawStyleCount},
                {"timelines",  semantic.timelineCount},
                {"styleBlocks",semantic.styleBlocks},
                {"rawCssBlocks", semantic.rawCssBlocks},
                {"rawHtmlBlocks", semantic.rawHtmlBlocks}
            }},
            {"observable", {
                {"state",   listJson(usage.observedState)},
                {"derived", listJson(usage.observedDerived)},
                {"actions", listJson(usage.boundActions)}
            }},
            {"semantic", {
                {"attributes", attributeKindCountsToJson(semantic.attributes)},
                {"nodes", semanticNodeCountsToJson(semantic)},
                {"moduleFiles", semantic.moduleFiles},
                {"project", jtml::semanticProjectToJson(project)},
                {"routeRecords", semanticRouteRecordsToJson(semantic)},
                {"fetchRecords", semanticFetchRecordsToJson(semantic)},
                {"importRecords", semanticImportRecordsToJson(semantic)},
                {"exportRecords", jtml::semanticExportRecordsToJson(semantic.exportRecords)},
                {"componentDefinitions", semanticComponentDefinitionsToJson(semantic)},
                {"componentInstances", semanticComponentInstancesToJson(semantic)},
                {"ui", semanticUiToJson(semantic)},
                {"uiModifiers", semanticUiModifiersToJson(semantic)},
                {"dependencies", semanticEdgesToJson(semantic)},
                {"sourceOfTruth", "typed AST -> semantic analysis -> observable graph"}
            }},
            {"runtimePlan", jtml::runtimePlanToExplainJson(runtimePlan)},
            {"runtimeProjectPlan", jtml::runtimeProjectPlanToExplainJson(runtimeProjectPlan)},
            {"issues", {
                {"deadState",          listJson(usage.deadState)},
                {"zombieState",        listJson(usage.zombieState)},
                {"unusedDerived",      listJson(usage.unusedDerived)},
                {"unboundActions",     listJson(usage.unboundActions)},
                {"unproductiveActions",listJson(usage.unproductiveActions)}
            }},
            {"actionProfiles", actionArr},
            {"state",      listJson(semantic.state)},
            {"constants",  listJson(semantic.constants)},
            {"derived",    listJson(semantic.derived)},
            {"actions",    listJson(semantic.actions)},
            {"components", listJson(semantic.components)},
            {"routes",     listJson(semantic.routes)},
            {"fetches",    listJson(semantic.fetches)},
            {"stores",     listJson(semantic.stores)},
            {"effects",    listJson(semantic.effects)},
            {"imports",    listJson(semantic.imports)},
            {"externs",    listJson(semantic.externs)},
            {"diagnostics",lintDiagnosticsToJson(diagnostics)},
        }.dump(2) << "\n";
        return lintErrors == 0 ? 0 : 1;
    }

    // ── Human-readable output ──────────────────────────────────────────
    const std::string sep(60, '-');
    std::cout << "Observable-first analysis: " << o.inputFile << "\n";
    std::cout << sep << "\n";

    // Depth
    std::cout << "Depth:       " << depth.level << " (" << depth.label << ")\n";

    // Complexity table
    std::cout << "\nComplexity:\n";
    auto metric = [](const std::string& label, int n, const std::string& note = "") {
        if (n == 0 && note.empty()) return;
        std::cout << "  " << std::left << std::setw(14) << label << n;
        if (!note.empty()) std::cout << "   " << note;
        std::cout << "\n";
    };
    metric("state",      static_cast<int>(semantic.state.size()));
    metric("constants",  static_cast<int>(semantic.constants.size()));
    metric("derived",    static_cast<int>(semantic.derived.size()));
    metric("actions",    static_cast<int>(semantic.actions.size()));
    metric("fetches",    static_cast<int>(semantic.fetches.size()));
    metric("routes",     static_cast<int>(semantic.routes.size()));
    metric("effects",    static_cast<int>(semantic.effects.size()));
    metric("imports",    static_cast<int>(semantic.imports.size()));
    metric("externs",    static_cast<int>(semantic.externs.size()));
    metric("timelines",  semantic.timelineCount);
    metric("ui primitives", static_cast<int>(semantic.uiPrimitives.size()));
    metric("author theme", semantic.authorThemeTokenCount);
    metric("theme tokens", semantic.themeTokenCount);
    metric("components", static_cast<int>(semantic.components.size()));
    metric("stores",     static_cast<int>(semantic.stores.size()));
    metric("styles",     semantic.styleBlocks);
    metric("raw css",    semantic.rawCssBlocks);
    metric("raw html",   semantic.rawHtmlBlocks);

    if (!project.modules.empty()) {
        std::cout << "\nProject modules (" << project.modules.size() << "):\n";
        for (const auto& module : project.modules) {
            std::cout << "  + ";
            if (module.id == project.entry) std::cout << "[entry] ";
            std::cout << compactProjectPath(module.path) << "\n";

            if (!module.imports.empty()) {
                std::cout << "      imports:\n";
                for (const auto& import : module.imports) {
                    std::cout << "        - " << importLabel(import);
                    if (import.resolved != jtml::InvalidSemanticModuleId) {
                        std::cout << " -> " << compactProjectPath(import.resolvedPath);
                    } else if (!import.resolvedPath.empty()) {
                        std::cout << " -> unresolved (" << compactProjectPath(import.resolvedPath) << ")";
                    } else {
                        std::cout << " -> unresolved";
                    }
                    if (import.reExport) std::cout << " re-export";
                    std::cout << "\n";
                }
            }

            if (!module.exports.empty()) {
                std::vector<std::string> labels;
                labels.reserve(module.exports.size());
                for (const auto& exportRecord : module.exports) {
                    labels.push_back(exportLabel(exportRecord));
                }
                std::cout << "      exports: " << joinDisplay(labels) << "\n";
            }

            std::vector<std::string> summary;
            const auto& sem = module.semantic;
            if (!sem.components.empty()) summary.push_back(std::to_string(sem.components.size()) + " component(s)");
            if (!sem.stores.empty()) summary.push_back(std::to_string(sem.stores.size()) + " store(s)");
            if (!sem.state.empty()) summary.push_back(std::to_string(sem.state.size()) + " state");
            if (!sem.actions.empty()) summary.push_back(std::to_string(sem.actions.size()) + " action(s)");
            if (!sem.fetches.empty()) summary.push_back(std::to_string(sem.fetches.size()) + " fetch(es)");
            if (!sem.routes.empty()) summary.push_back(std::to_string(sem.routes.size()) + " route(s)");
            if (!summary.empty()) {
                std::cout << "      semantic: " << joinDisplay(summary) << "\n";
            }
            if (module.ir.available) {
                std::cout << "      ir: " << module.ir.totalNodeCount << " typed node"
                          << (module.ir.totalNodeCount == 1 ? "" : "s");
                if (!module.ir.syntax.empty()) std::cout << " (" << module.ir.syntax << ")";
                if (!module.ir.topLevelNodes.empty()) {
                    std::vector<std::string> topLevel;
                    const size_t limit = std::min<size_t>(module.ir.topLevelNodes.size(), 4);
                    topLevel.reserve(limit);
                    for (size_t i = 0; i < limit; ++i) {
                        const auto& node = module.ir.topLevelNodes[i];
                        topLevel.push_back(node.label.empty()
                            ? node.kind
                            : node.kind + ":" + node.label);
                    }
                    std::cout << " top-level: " << joinDisplay(topLevel);
                    if (module.ir.topLevelNodes.size() > limit) {
                        std::cout << ", ...";
                    }
                }
                std::cout << "\n";
            } else if (!module.ir.parseError.empty()) {
                std::cout << "      ir: unavailable (" << module.ir.parseError << ")\n";
            }
        }

        if (!projectIssues.empty()) {
            std::cout << "\nProject issues (" << projectIssues.size() << "):\n";
            for (const auto& issue : projectIssues) {
                std::cout << "  ! " << issue.code << ": " << issue.message;
                if (!issue.path.empty()) {
                    std::cout << " [" << compactProjectPath(issue.path);
                    if (issue.line > 0) {
                        std::cout << ":" << issue.line;
                        if (issue.column > 0) std::cout << ":" << issue.column;
                    }
                    std::cout << "]";
                }
                std::cout << "\n";
            }
        }
    }

    if (!semantic.dependencies.empty()) {
        std::cout << "\nSemantic graph (" << semantic.dependencies.size() << " edge"
                  << (semantic.dependencies.size() == 1 ? "" : "s") << "):\n";
        const size_t limit = std::min<size_t>(semantic.dependencies.size(), 16);
        for (size_t i = 0; i < limit; ++i) {
            const auto& edge = semantic.dependencies[i];
            std::cout << "  " << edge.from << " --" << edge.kind << "--> " << edge.to << "\n";
        }
        if (semantic.dependencies.size() > limit) {
            std::cout << "  ... " << (semantic.dependencies.size() - limit)
                      << " more edge" << (semantic.dependencies.size() - limit == 1 ? "" : "s")
                      << " in `--json`\n";
        }
    }

    if (!semantic.uiPrimitives.empty() || !semantic.uiModifiers.empty() ||
        semantic.themeTokenCount > 0 || semantic.styleBlocks > 0) {
        std::cout << "\nSemantic UI:\n";
        if (!semantic.uiPrimitives.empty()) {
            std::cout << "  primitives: " << joinDisplay(semantic.uiPrimitives) << "\n";
        }
        if (!semantic.uiModifiers.empty()) {
            std::cout << "  modifiers:\n";
            for (const auto& modifier : semantic.uiModifiers) {
                std::cout << "    - " << modifier.primitive << " "
                          << modifier.modifier << " " << modifier.value << "\n";
            }
        }
        if (semantic.themeTokenCount > 0) {
            std::cout << "  author theme tokens: " << semantic.authorThemeTokenCount << "\n";
            std::cout << "  generated theme token references: " << semantic.themeTokenCount << "\n";
        }
        if (semantic.styleBlocks > 0) {
            std::cout << "  generated/style blocks: " << semantic.styleBlocks << "\n";
        }
    }

    if (!semantic.componentDefinitions.empty()) {
        std::cout << "\nComponent definitions (" << semantic.componentDefinitions.size() << "):\n";
        for (const auto& definition : semantic.componentDefinitions) {
            std::cout << "  + " << definition.name;
            if (!definition.params.empty()) {
                std::cout << "(" << joinDisplay(definition.params, "") << ")";
            }
            if (definition.sourceLine > 0) std::cout << "  line " << definition.sourceLine;
            if (definition.hasSlot) std::cout << "  slot";
            std::cout << "\n";
            if (!definition.emits.empty()) {
                std::cout << "      emits: " << joinDisplay(definition.emits) << "\n";
            }
            if (!definition.localState.empty()) {
                std::cout << "      state: " << joinDisplay(definition.localState) << "\n";
            }
            if (!definition.localDerived.empty()) {
                std::cout << "      derived: " << joinDisplay(definition.localDerived) << "\n";
            }
            if (!definition.localActions.empty()) {
                std::cout << "      actions: " << joinDisplay(definition.localActions) << "\n";
            }
            if (!definition.localEffects.empty()) {
                std::cout << "      effects: " << joinDisplay(definition.localEffects) << "\n";
            }
            if (!definition.eventBindings.empty()) {
                std::cout << "      event bindings: " << joinDisplay(definition.eventBindings) << "\n";
            }
        }
    }

    if (!semantic.componentInstances.empty()) {
        std::cout << "\nComponent instances (" << semantic.componentInstances.size() << "):\n";
        for (const auto& instance : semantic.componentInstances) {
            std::cout << "  + " << instance.id << " : " << instance.component
                      << " [" << instance.role << "]";
            if (instance.sourceLine > 0) std::cout << "  line " << instance.sourceLine;
            std::cout << "\n";
            if (!instance.params.empty()) {
                std::cout << "      params: ";
                for (size_t i = 0; i < instance.params.size(); ++i) {
                    if (i > 0) std::cout << ", ";
                    std::cout << instance.params[i].name << "=" << instance.params[i].value;
                }
                std::cout << "\n";
            }
            if (!instance.locals.empty()) {
                std::cout << "      locals: ";
                for (size_t i = 0; i < instance.locals.size(); ++i) {
                    if (i > 0) std::cout << ", ";
                    std::cout << instance.locals[i].name << "->" << instance.locals[i].value;
                }
                std::cout << "\n";
            }
        }
    }

    // Observable state
    std::cout << "\nObservable state (" << usage.observedState.size() << "):\n";
    if (usage.observedState.empty()) {
        std::cout << "  (none)\n";
    } else {
        for (const auto& s : usage.observedState) std::cout << "  + " << s << "\n";
    }

    if (!usage.observedDerived.empty()) {
        std::cout << "\nObservable derived (" << usage.observedDerived.size() << "):\n";
        for (const auto& d : usage.observedDerived) std::cout << "  + " << d << "\n";
    }

    // Bound actions with effect summary
    if (!usage.boundActions.empty()) {
        std::cout << "\nUI-bound actions (" << usage.boundActions.size() << "):\n";
        for (const auto& a : usage.boundActions) {
            std::cout << "  + " << a;
            auto it = profMap.find(a);
            if (it != profMap.end()) {
                const auto& p = *it->second;
                if (!p.writes.empty()) {
                    std::cout << "  →  writes: ";
                    for (size_t i = 0; i < p.writes.size(); ++i) {
                        if (i > 0) std::cout << ", ";
                        std::cout << p.writes[i];
                    }
                }
                if (!p.hasVisibleEffect && !p.writes.empty()) {
                    std::cout << "  ⚠ writes nothing observable";
                }
            }
            std::cout << "\n";
        }
    }

    // Issues
    bool hasIssues = !usage.deadState.empty() || !usage.zombieState.empty() ||
                     !usage.unusedDerived.empty() || !usage.unboundActions.empty() ||
                     !usage.unproductiveActions.empty();
    if (hasIssues) {
        std::cout << "\nObservability issues:\n";
        for (const auto& s : usage.deadState)
            std::cout << "  ! dead state: \"" << s << "\" is never read by UI, actions, or effects — remove it or bind it to an output.\n";
        for (const auto& s : usage.zombieState)
            std::cout << "  ~ zombie state: \"" << s << "\" is used by actions but never observed in the UI.\n";
        for (const auto& d : usage.unusedDerived)
            std::cout << "  ~ unused derived: \"" << d << "\" is computed but never shown or used.\n";
        for (const auto& a : usage.unboundActions)
            std::cout << "  ~ unbound action: \"" << a << "\" is defined but never triggered from the UI.\n";
        for (const auto& a : usage.unproductiveActions)
            std::cout << "  ~ unproductive action: \"" << a << "\" is triggered but its writes are not observed in the UI.\n";
    } else {
        std::cout << "\nNo observability issues found. Every state variable, derived value, and action is wired to the UI.\n";
    }

    // Linter
    if (!diagnostics.empty()) {
        std::cout << "\nLint (" << diagnostics.size() << " issue" << (diagnostics.size() == 1 ? "" : "s") << "):\n";
        for (const auto& d : diagnostics) {
            std::cout << "  " << (d.severity == LintDiagnostic::Severity::Error ? "error" : "warn")
                      << ": " << d.message << "\n";
        }
    }

    std::cout << sep << "\n";
    return lintErrors == 0 ? 0 : 1;
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
    if (o.target == "browser") {
        transpiler.setBrowserLocalRuntime(true);
        const auto semantic = jtml::analyzeSemanticProgram(program, readFile(o.inputFile));
        transpiler.setRuntimeProjectPlan(
            buildRuntimeProjectPlanForInput(o.inputFile, o.syntax, program, semantic));
    }
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
    jtml::RuntimeProjectPlan runtimeProjectPlan;
    bool hasRuntimeProjectPlan = false;
    if (o.target == "browser") {
        transpiler.setBrowserLocalRuntime(true);
        transpiler.setExternalRuntimeScript(true);
        const auto semantic = jtml::analyzeSemanticProgram(program, readFile(input.string()));
        runtimeProjectPlan =
            buildRuntimeProjectPlanForInput(input.string(), o.syntax, program, semantic);
        hasRuntimeProjectPlan = true;
        transpiler.setRuntimeProjectPlan(runtimeProjectPlan);
    }
    std::string html = transpiler.transpile(program);
    if (hasRuntimeProjectPlan) {
        html = injectBrowserBuildAssetScript(html, "jtml-runtime.js");
        html = injectBrowserBuildAssetScript(html, "components/index.js");
        html = injectBrowserBuildAssetScript(html, "app.js");
    }

    std::ofstream ofs(outFile);
    if (!ofs.is_open()) {
        throw std::runtime_error("Cannot write build output: " + outFile.string());
    }
    ofs << html;

    std::cout << "Built " << input.string() << " -> " << outFile.string() << "\n";
    if (hasRuntimeProjectPlan) {
        const auto runtimeFile = outDir / "jtml-runtime.js";
        const auto componentModuleFile = outDir / "components" / "index.js";
        const auto appFile = outDir / "app.js";
        const auto legacyUpdatePlanFile = outDir / "jtml-update-plans.js";
        const auto componentModuleAsset =
            emitStaticComponentModuleAsset(runtimeProjectPlan);
        writeBuildAsset(runtimeFile,
                        jtml::emitBrowserRuntimeScript(8080, true, false),
                        "browser runtime output");
        writeBuildAsset(componentModuleFile,
                        componentModuleAsset,
                        "browser component module output");
        writeBuildAsset(appFile,
                        "/* Generated by jtml build --target browser. */\n"
                        "(function(){\n"
                        "  window.jtml = window.jtml || {};\n"
                        "  window.jtml.productionAssets = Object.assign({}, window.jtml.productionAssets || {}, {\n"
                        "    runtime: './jtml-runtime.js',\n"
                        "    components: './components/index.js',\n"
                        "    app: './app.js',\n"
                        "    compatibilityUpdatePlans: './jtml-update-plans.js'\n"
                        "  });\n"
                        "}());\n",
                        "browser app bootstrap output");
        writeBuildAsset(legacyUpdatePlanFile,
                        emitStaticUpdatePlanAsset(runtimeProjectPlan),
                        "legacy browser update plan output");
        std::cout << "Built browser runtime asset -> "
                  << runtimeFile.string() << "\n";
        std::cout << "Built browser component modules -> "
                  << componentModuleFile.string() << "\n";
        std::cout << "Built browser app bootstrap -> "
                  << appFile.string() << "\n";
        std::cout << "Built legacy browser update plans -> "
                  << legacyUpdatePlanFile.string() << "\n";
    }
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
