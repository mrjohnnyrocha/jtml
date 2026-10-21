#include "browser_build.h"

#include "browser_build_assets.h"
#include "browser_build_policy.h"
#include "browser_build_schema.h"

#include "jtml/browser_runtime_emitter.h"
#include "jtml/diagnostic.h"
#include "jtml/friendly.h"
#include "jtml/lexer.h"
#include "jtml/parser.h"
#include "jtml/runtime/static_update_plan_emitter.h"
#include "jtml/semantic.h"
#include "jtml/semantic/module_graph.h"
#include "jtml/transpiler.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace jtml::cli {
namespace {

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

struct ParsedModuleForBrowserBuild {
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

std::string importedComponentStubsForBrowserBuild(const std::string& source) {
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

std::vector<std::unique_ptr<ASTNode>> parseStandaloneNormalizedSource(const std::string& source,
        std::shared_ptr<const SourceProvenance> provenance = {}) {
    Lexer lexer(source, std::move(provenance));
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

ParsedModuleForBrowserBuild parseStandaloneModuleForBrowserBuild(const std::string& source,
                                                                 SyntaxMode syntax,
                                                                 bool allowImportStubs,
                                                                 const std::string& file) {
    const bool friendly =
        syntax == SyntaxMode::Friendly ||
        (syntax == SyntaxMode::Auto &&
         (isFriendlySyntax(source) || looksLikeFriendlySyntax(source)));
    auto parseMapped = [&](MappedSource bytes, SyntaxMode mode) {
        CompilationSource input;
        input.source = std::move(bytes);
        input.syntax = mode;
        auto normalized = normalizeCompilationSource(input);
        return parseStandaloneNormalizedSource(normalized.text, normalized.provenance);
    };
    if (!friendly) {
        return {parseMapped(MappedSource::original(file, source), SyntaxMode::Classic),
                "classic"};
    }

    try {
        return {parseMapped(MappedSource::original(file, source), SyntaxMode::Friendly),
                "friendly"};
    } catch (const std::exception& firstError) {
        if (!allowImportStubs) {
            throw;
        }
        const auto stubs = importedComponentStubsForBrowserBuild(source);
        if (stubs.empty()) throw;
        try {
            auto withStubs = MappedSource::original(file, source);
            withStubs.appendSynthetic(stubs);
            return {parseMapped(std::move(withStubs), SyntaxMode::Friendly),
                    "friendly+import-stubs"};
        } catch (...) {
            throw firstError;
        }
    }
}

} // namespace

jtml::RuntimeProjectPlan buildRuntimeProjectPlanForInput(
        const std::string& inputFile,
        SyntaxMode syntax,
        const std::vector<std::unique_ptr<ASTNode>>& linkedProgram,
        const jtml::SemanticProgram& linkedSemantic,
        bool allowImportStubs) {
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
            auto parsedModule =
                parseStandaloneModuleForBrowserBuild(moduleSource, syntax, allowImportStubs, path);
            ir = jtml::summarizeSemanticAst(parsedModule.program, parsedModule.syntax);
            moduleAst = jtml::cloneSemanticAst(parsedModule.program, parsedModule.syntax);
            moduleSemantic = parsedModule.syntax == "friendly+import-stubs"
                ? jtml::analyzeSemanticProgram({}, moduleSource)
                : jtml::analyzeSemanticProgram(parsedModule.program, moduleSource);
        } catch (const std::exception& e) {
            if (!allowImportStubs) {
                throw std::runtime_error(
                    "Cannot build clean browser production module plan for " +
                    path + ": " + e.what());
            }
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

int cmdBuildBrowser(const Options& o) {
    if (o.inputFile.empty() || o.outputFile.empty()) usage();
    const auto buildMode = browserBuildModeFromOptions(o);

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
    transpiler.setBrowserLocalRuntime(true);
    transpiler.setExternalRuntimeScript(true);
    const bool emitCompatibilityAsset =
        browserBuildModeEmitsCompatibilityAsset(buildMode);
    const auto semantic = jtml::analyzeSemanticProgram(program, readFile(input.string()));
    auto runtimeProjectPlan =
        buildRuntimeProjectPlanForInput(input.string(),
                                        o.syntax,
                                        program,
                                        semantic,
                                        browserBuildModeAllowsImportStubs(buildMode));
    transpiler.setRuntimeProjectPlan(runtimeProjectPlan);

    if (buildMode == BrowserBuildMode::Production) {
        const auto productionIssues =
            jtml::validateStaticComponentModuleForProduction(runtimeProjectPlan);
        if (!productionIssues.empty()) {
            std::ostringstream message;
            message << "This app cannot compile to clean browser production yet.\n"
                    << "The clean production target only ships static browser assets and "
                    << "does not include the legacy compatibility update-plan fallback.\n"
                    << "Unsupported shape(s):";
            for (const auto& issue : productionIssues) {
                message << "\n- " << issue;
            }
            message
                << "\n\nWhat to do:\n"
                << "- Temporary migration build: jtml build " << input.string()
                << " --target browser --compat --out " << outDir.string() << "\n"
                << "- Diagnostic build with source-rich compatibility metadata: jtml build "
                << input.string() << " --target browser --debug --out "
                << outDir.string() << "\n"
                << "- Or change the component shape above to clean-prod supported template/action semantics.";
            throw std::runtime_error(message.str());
        }
    }

    std::string html = transpiler.transpile(program);
    html = injectBrowserBuildAssetScript(html, "jtml-runtime.js");
    html = injectBrowserBuildAssetScript(html, "components/index.js");
    html = injectBrowserBuildAssetScript(html, "app.js");

    std::ofstream ofs(outFile);
    if (!ofs.is_open()) {
        throw std::runtime_error("Cannot write build output: " + outFile.string());
    }
    ofs << html;
    ofs.close();

    std::cout << "Built " << input.string() << " -> " << outFile.string() << "\n";

    const auto runtimeFile = outDir / "jtml-runtime.js";
    const auto componentModuleFile = outDir / "components" / "index.js";
    const auto appFile = outDir / "app.js";
    const auto legacyUpdatePlanFile = outDir / "jtml-update-plans.js";
    const auto componentModuleAsset =
        emitStaticComponentModuleAsset(runtimeProjectPlan);
    writeBuildAsset(runtimeFile,
                    jtml::emitBrowserRuntimeExternalAsset(8080, true),
                    "browser runtime output");
    writeBuildAsset(componentModuleFile,
                    componentModuleAsset,
                    "browser component module output");
    writeBuildAsset(appFile,
                    emitBrowserBuildAppBootstrap(buildMode),
                    "browser app bootstrap output");
    if (emitCompatibilityAsset) {
        writeBuildAsset(legacyUpdatePlanFile,
                        emitStaticUpdatePlanAsset(runtimeProjectPlan),
                        "legacy browser update plan output");
    } else if (std::filesystem::exists(legacyUpdatePlanFile)) {
        std::filesystem::remove(legacyUpdatePlanFile);
    }
    std::cout << "Built browser runtime asset -> "
              << runtimeFile.string() << "\n";
    std::cout << "Built browser component modules -> "
              << componentModuleFile.string() << "\n";
    std::cout << "Built browser app bootstrap -> "
              << appFile.string() << "\n";
    if (emitCompatibilityAsset) {
        std::cout << "Built legacy browser update plans -> "
                  << legacyUpdatePlanFile.string() << "\n";
    } else {
        std::cout << "Skipped legacy browser update plans for production browser output\n";
    }

    validateBrowserBuildOutputSchema(outDir, buildMode);

    if (inputWasDirectory) {
        copyBrowserBuildAssets(inputDir, outDir);
    }
    return 0;
}

int cmdTranspileBrowser(const Options& o) {
    if (o.prod || o.compat || o.debug) {
        throw std::runtime_error(
            "browser asset mode flags are only supported by `jtml build --target browser`; "
            "`jtml transpile --target browser` is an inline preview artifact");
    }
    auto program = parseProgramFromFile(o.inputFile, o.syntax);
    JtmlTranspiler transpiler;
    transpiler.setBrowserLocalRuntime(true);
    const auto semantic = jtml::analyzeSemanticProgram(program, readFile(o.inputFile));
    transpiler.setRuntimeProjectPlan(
        buildRuntimeProjectPlanForInput(o.inputFile, o.syntax, program, semantic, true));
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

} // namespace jtml::cli
