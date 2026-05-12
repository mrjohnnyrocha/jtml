// cli/cmd_fmt_lint.cpp — `jtml fmt`, `jtml lint`, and `jtml migrate`.
// Both fmt and lint silence the parser's verbose `[DEBUG]` logging while
// parsing, then restore stdout before emitting their own output.
#include "commands.h"
#include "diagnostic_json.h"

#include "jtml/formatter.h"
#include "jtml/friendly_formatter.h"
#include "jtml/linter.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <sstream>

namespace jtml::cli {

using jtml::JtmlFormatter;
using jtml::JtmlLinter;

namespace {

std::vector<int> friendlyLineMapForFile(const Options& o) {
    try {
        const std::string source = readFile(o.inputFile);
        const bool friendly =
            o.syntax == SyntaxMode::Friendly ||
            (o.syntax == SyntaxMode::Auto &&
             (jtml::isFriendlySyntax(source) || jtml::looksLikeFriendlySyntax(source)));
        if (!friendly) return {};
        return jtml::friendlyToClassicWithSourceMap(source).classicLineToFriendlyLine;
    } catch (...) {
        return {};
    }
}

} // namespace

int cmdFmt(const Options& o) {
    const std::string original = readFile(o.inputFile);
    const bool friendlyInput =
        o.syntax == SyntaxMode::Friendly ||
        (o.syntax == SyntaxMode::Auto &&
         (jtml::isFriendlySyntax(original) || jtml::looksLikeFriendlySyntax(original)));

    std::string formatted;
    if (friendlyInput) {
        {
            SilenceStdout silence;
            (void)parseProgramFromFile(o.inputFile, SyntaxMode::Friendly);
        }
        formatted = jtml::formatFriendlySource(original);
    } else {
        std::vector<std::unique_ptr<ASTNode>> program;
        {
            SilenceStdout silence;
            program = parseProgramFromFile(o.inputFile, o.syntax);
        }
        JtmlFormatter formatter;
        formatted = formatter.format(program);
    }

    if (o.force) {
        // `jtml fmt <file> -w` writes in place (same convention as `gofmt -w`).
        std::ofstream ofs(o.inputFile);
        if (!ofs.is_open())
            throw std::runtime_error("Cannot write file: " + o.inputFile);
        ofs << formatted;
        std::cout << "Formatted " << o.inputFile << "\n";
    } else {
        std::cout << formatted;
    }
    return 0;
}

int cmdLint(const Options& o) {
    std::vector<LintDiagnostic> diagnostics;
    try {
        std::vector<std::unique_ptr<ASTNode>> program;
        {
            SilenceStdout silence;
            program = parseProgramFromFile(o.inputFile, o.syntax);
        }

        JtmlLinter linter;
        diagnostics = linter.lint(program);
    } catch (const std::exception& e) {
        if (o.json) {
            auto diagnostics = diagnosticsFromMessageBlock(e.what(), DiagnosticSeverity::Error);
            const auto lineMap = friendlyLineMapForFile(o);
            if (!lineMap.empty()) remapDiagnosticLines(diagnostics, lineMap);
            std::ostringstream message;
            for (const auto& diagnostic : diagnostics) message << diagnostic.message << "\n";
            auto out = nlohmann::json{
                {"ok", false},
                {"error", message.str().empty() ? std::string(e.what()) : message.str()},
                {"diagnostics", diagnosticsToJson(diagnostics)},
            };
            out["file"] = o.inputFile;
            std::cout << out.dump(2) << "\n";
        } else {
            std::cerr << "error: " << e.what() << "\n";
        }
        return 1;
    }

    int errors = 0;
    for (const auto& d : diagnostics) {
        if (d.severity == LintDiagnostic::Severity::Error) ++errors;
    }

    if (o.json) {
        std::cout << nlohmann::json{
            {"ok", errors == 0},
            {"file", o.inputFile},
            {"diagnostics", lintDiagnosticsToJson(diagnostics)},
        }.dump(2) << "\n";
        return errors > 0 ? 1 : 0;
    }

    if (diagnostics.empty()) {
        std::cout << "OK: " << o.inputFile << "\n";
        return 0;
    }

    for (const auto& d : diagnostics) {
        const char* label =
            (d.severity == LintDiagnostic::Severity::Error) ? "error" : "warning";
        std::cerr << o.inputFile << ": " << label << "[" << d.code << "]";
        if (d.line > 0) {
            std::cerr << ":" << d.line;
            if (d.column > 0) std::cerr << ":" << d.column;
        }
        std::cerr << ": " << d.message << "\n";
        if (!d.hint.empty()) std::cerr << "  hint: " << d.hint << "\n";
    }
    std::cerr << "\n" << diagnostics.size() << " issue(s), "
              << errors << " error(s)\n";
    return errors > 0 ? 1 : 0;
}

int cmdMigrate(const Options& o) {
    std::vector<std::unique_ptr<ASTNode>> program;
    {
        SilenceStdout silence;
        // Force classic syntax mode so we parse the original classic source.
        program = parseProgramFromFile(o.inputFile, SyntaxMode::Classic);
    }

    JtmlFriendlyFormatter formatter;
    std::string friendly = formatter.format(program);

    if (!o.outputFile.empty()) {
        std::ofstream ofs(o.outputFile);
        if (!ofs.is_open())
            throw std::runtime_error("Cannot write file: " + o.outputFile);
        ofs << friendly;
        std::cout << "Migrated " << o.inputFile << " → " << o.outputFile << "\n";
    } else if (o.force) {
        // `jtml migrate <file> -w` overwrites the original file in place.
        std::ofstream ofs(o.inputFile);
        if (!ofs.is_open())
            throw std::runtime_error("Cannot write file: " + o.inputFile);
        ofs << friendly;
        std::cout << "Migrated " << o.inputFile << " (in place)\n";
    } else {
        std::cout << friendly;
    }
    return 0;
}

} // namespace jtml::cli
