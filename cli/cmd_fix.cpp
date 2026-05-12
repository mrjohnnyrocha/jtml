// cli/cmd_fix.cpp — conservative source repair command.
#include "commands.h"
#include "diagnostic_json.h"

#include "jtml/fix.h"
#include "jtml/lexer.h"
#include "jtml/parser.h"

#include "json.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

namespace jtml::cli {

namespace {

void parseSourceOrThrow(const std::string& source, SyntaxMode syntax) {
    std::string normalized = jtml::normalizeSourceSyntax(source, syntax);
    Lexer lexer(normalized);
    auto tokens = lexer.tokenize();
    if (!lexer.getErrors().empty()) {
        std::ostringstream oss;
        for (const auto& error : lexer.getErrors()) {
            oss << "Lexer Error: " << error << "\n";
        }
        throw std::runtime_error(oss.str());
    }
    Parser parser(std::move(tokens));
    (void)parser.parseProgram();
    if (!parser.getErrors().empty()) {
        std::ostringstream oss;
        for (const auto& error : parser.getErrors()) {
            oss << "Parser Error: " << error << "\n";
        }
        throw std::runtime_error(oss.str());
    }
}

} // namespace

int cmdFix(const Options& o) {
    const std::string original = readFile(o.inputFile);
    const auto fixed = jtml::fixSource(original, o.syntax);

    bool parses = true;
    std::string parseError;
    try {
        SilenceStdout silence;
        parseSourceOrThrow(fixed.source, o.syntax);
    } catch (const std::exception& e) {
        parses = false;
        parseError = e.what();
    }

    if (o.force && fixed.changed) {
        std::ofstream out(o.inputFile);
        if (!out.is_open()) {
            throw std::runtime_error("Cannot write file: " + o.inputFile);
        }
        out << fixed.source;
    }

    if (o.json) {
        nlohmann::json out = {
            {"ok", parses},
            {"file", o.inputFile},
            {"changed", fixed.changed},
            {"written", o.force && fixed.changed},
            {"changes", fixChangesToJson(fixed.changes)},
            {"diagnostics", nlohmann::json::array()},
        };
        if (!parses) {
            out["error"] = parseError;
            out["diagnostics"] = diagnosticsToJson(
                diagnosticsFromMessageBlock(parseError, DiagnosticSeverity::Error));
        }
        std::cout << out.dump(2) << "\n";
        return parses ? 0 : 1;
    }

    if (!fixed.changed) {
        std::cout << "No safe fixes needed: " << o.inputFile << "\n";
    } else if (o.force) {
        std::cout << "Fixed " << o.inputFile << "\n";
    } else {
        std::cout << fixed.source;
    }

    if (!parses) {
        const auto diagnostics = diagnosticsFromMessageBlock(
            parseError, DiagnosticSeverity::Error);
        std::cerr << "\nRemaining issue(s):\n";
        for (const auto& diagnostic : diagnostics) {
            std::cerr << "- " << diagnosticSeverityName(diagnostic.severity)
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
        return 1;
    }
    return 0;
}

} // namespace jtml::cli
