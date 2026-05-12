#include "diagnostic_json.h"

namespace jtml::cli {

nlohmann::json diagnosticToJson(const Diagnostic& diagnostic) {
    return {
        {"severity", diagnosticSeverityName(diagnostic.severity)},
        {"code", diagnostic.code},
        {"message", diagnostic.message},
        {"line", diagnostic.line},
        {"column", diagnostic.column},
        {"hint", diagnostic.hint},
        {"example", diagnostic.example},
    };
}

nlohmann::json diagnosticsToJson(const std::vector<Diagnostic>& diagnostics) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& diagnostic : diagnostics) {
        out.push_back(diagnosticToJson(diagnostic));
    }
    return out;
}

nlohmann::json lintDiagnosticsToJson(const std::vector<LintDiagnostic>& diagnostics) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& d : diagnostics) {
        Diagnostic diagnostic;
        diagnostic.severity = d.severity == LintDiagnostic::Severity::Error
            ? DiagnosticSeverity::Error
            : DiagnosticSeverity::Warning;
        diagnostic.code = d.code;
        diagnostic.message = d.message;
        diagnostic.line = d.line;
        diagnostic.column = d.column;
        diagnostic.hint = d.hint;
        diagnostic.example = d.example;
        out.push_back(diagnosticToJson(diagnostic));
    }
    return out;
}

nlohmann::json fixChangesToJson(const std::vector<FixChange>& changes) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& change : changes) {
        out.push_back({
            {"line", change.line},
            {"code", change.code},
            {"message", change.message},
        });
    }
    return out;
}

nlohmann::json errorResponseJson(const std::string& message) {
    const auto diagnostics = diagnosticsFromMessageBlock(message, DiagnosticSeverity::Error);
    return {
        {"ok", false},
        {"error", message},
        {"diagnostics", diagnosticsToJson(diagnostics)},
    };
}

} // namespace jtml::cli
