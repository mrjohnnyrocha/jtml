// diagnostic.h
//
// Shared, repair-oriented diagnostic shape for CLI, Studio, future LSP, and
// AI-native tooling. Parsers and linters may still originate plain strings;
// these helpers normalize them into structured fields.
#pragma once

#include <string>
#include <vector>

namespace jtml {

enum class DiagnosticSeverity {
    Warning,
    Error,
};

struct Diagnostic {
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    std::string code = "JTML_DIAGNOSTIC";
    std::string message;
    int line = 0;
    int column = 0;
    std::string hint;
    std::string example;
};

const char* diagnosticSeverityName(DiagnosticSeverity severity);

Diagnostic diagnosticFromMessage(
    const std::string& message,
    DiagnosticSeverity severity = DiagnosticSeverity::Error);

std::vector<Diagnostic> diagnosticsFromMessageBlock(
    const std::string& messages,
    DiagnosticSeverity severity = DiagnosticSeverity::Error);

void remapDiagnosticLines(
    std::vector<Diagnostic>& diagnostics,
    const std::vector<int>& oneBasedLineMap);

} // namespace jtml
