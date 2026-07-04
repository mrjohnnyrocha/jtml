#include "jtml/browser_runtime_emitter.h"
#include "jtml/client_manifest_emitter.h"
#include "jtml/friendly.h"
#include "jtml/lexer.h"
#include "jtml/parser.h"
#include "jtml/runtime_plan.h"
#include "jtml/semantic.h"
#include "jtml/transpiler.h"

#include <iostream>
#include <sstream>
#include <string>

namespace {

std::vector<std::unique_ptr<ASTNode>> parseFriendly(const std::string& source) {
    Lexer lexer(normalizeSourceSyntax(source, jtml::SyntaxMode::Friendly));
    auto tokens = lexer.tokenize();
    if (!lexer.getErrors().empty()) {
        std::ostringstream errors;
        for (const auto& error : lexer.getErrors()) {
            errors << error << "\n";
        }
        throw std::runtime_error(errors.str());
    }

    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    if (!parser.getErrors().empty()) {
        std::ostringstream errors;
        for (const auto& error : parser.getErrors()) {
            errors << error << "\n";
        }
        throw std::runtime_error(errors.str());
    }
    return program;
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

} // namespace

int main() {
    try {
        const std::string source =
            "jtml 2\n"
            "let count = 0\n"
            "when add\n"
            "  count += 1\n"
            "page\n"
            "  text count\n"
            "  button \"Add\" click add\n";
        auto program = parseFriendly(source);
        const auto semantic = jtml::analyzeSemanticProgram(program, source);
        const auto plan = jtml::buildRuntimePlan(program, semantic);
        const auto manifest = jtml::emitClientManifestScript(plan);
        const auto runtime = jtml::emitBrowserRuntimeScript(8080, true);

        if (!contains(manifest, "__jtml_client_manifest") ||
            !contains(manifest, "\"actions\"") ||
            !contains(manifest, "\"state\"")) {
            std::cerr << "browser production linkcheck did not emit a usable client manifest\n";
            return 2;
        }
        if (!contains(runtime, "const browserLocalRuntime = true;") ||
            contains(runtime, "new Function") ||
            contains(runtime, "eval(")) {
            std::cerr << "browser production linkcheck emitted an unsafe or non-browser-local runtime\n";
            return 3;
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "browser production linkcheck failed: " << e.what() << "\n";
        return 1;
    }
}
