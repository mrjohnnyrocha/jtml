#include "browser_build.h"
#include "browser_build_assets.h"
#include "module_loader.h"
#include "jtml/lexer.h"
#include "jtml/parser.h"
#include "jtml/runtime/static_html_emitter.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace jtml::cli {
int cmdBuildStatic(const Options& o) {
    if (o.prod || o.compat || o.debug)
        throw std::runtime_error("--prod/--compat/--debug belong to --target browser, not --target static");
    namespace fs = std::filesystem;
    const auto inputRoot = fs::absolute(o.inputFile).lexically_normal();
    const bool directory = fs::is_directory(inputRoot);
    const auto input = directory ? inputRoot / "index.jtml" : inputRoot;
    std::vector<SemanticModuleSource> modules;
    for (const auto& file : collectSourceFilesRecoverable(input.string(), o.syntax)) {
        const auto source = readFile(file.string());
        if (o.syntax == SyntaxMode::Classic || !isFriendlySyntax(source))
            throw std::runtime_error(file.string() + ":1:1: static HTML requires canonical Friendly JTML 2");
        const auto lowered = friendlyToClassicWithSourceMap(MappedSource::original(file.string(), source), true);
        Lexer lexer(lowered.classicSource, lowered.provenance);
        auto tokens = lexer.tokenize();
        if (!lexer.getErrors().empty()) throw std::runtime_error(lexer.getErrors().front());
        Parser parser(std::move(tokens));
        auto program = parser.parseProgram();
        if (!parser.getErrors().empty()) throw std::runtime_error(parser.getErrors().front());
        modules.push_back({file.string(), analyzeSemanticProgram(program, source),
            summarizeSemanticAst(program, "friendly"), cloneSemanticAst(program, "friendly")});
    }
    const auto retained = buildSemanticProject(modules, input.string());
    const auto plan = buildRuntimePlan(retained);
    const auto html = emitStaticHtml(retained, plan); // Prove/render entirely before publication.
    const auto output = fs::weakly_canonical(fs::absolute(o.outputFile));
    if (fs::is_symlink(fs::symlink_status(fs::absolute(o.outputFile))))
        throw std::runtime_error("Static output must not be a symlink");
    for (const auto& m : retained.modules) {
        const auto rel = fs::weakly_canonical(m.path).lexically_relative(output);
        if (!rel.empty() && *rel.begin() != "..")
            throw std::runtime_error("Static output must not replace source files or their ancestors");
    }
    if (fs::exists(output) && !fs::is_directory(output))
        throw std::runtime_error("Static output must be a directory");
    fs::create_directories(output.parent_path());
    const auto suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path staged = output.string() + ".static-stage-" + suffix;
    const fs::path backup = output.string() + ".static-backup-" + suffix;
    if (!fs::create_directory(staged)) throw std::runtime_error("Cannot stage static output");
    bool backedUp = false;
    try {
        if (directory) copyBrowserBuildAssets(inputRoot, staged);
        std::ofstream file(staged / "index.html", std::ios::binary);
        file.exceptions(std::ios::badbit | std::ios::failbit);
        file << html;
        file.close();
        if (fs::exists(output)) { fs::rename(output, backup); backedUp = true; }
        try { fs::rename(staged, output); }
        catch (...) { if (backedUp) fs::rename(backup, output); throw; }
    } catch (...) {
        std::error_code ignored;
        fs::remove_all(staged, ignored);
        throw;
    }
    if (backedUp) { std::error_code ignored; fs::remove_all(backup, ignored); }
    std::cout << "Built proved static HTML (no JavaScript) -> " << (output / "index.html").string() << "\n";
    return 0;
}
}
