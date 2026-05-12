#pragma once

#include "jtml/diagnostic.h"
#include "jtml/fix.h"
#include "jtml/linter.h"
#include "json.hpp"

#include <string>
#include <vector>

namespace jtml::cli {

nlohmann::json diagnosticToJson(const Diagnostic& diagnostic);
nlohmann::json diagnosticsToJson(const std::vector<Diagnostic>& diagnostics);
nlohmann::json lintDiagnosticsToJson(const std::vector<LintDiagnostic>& diagnostics);
nlohmann::json fixChangesToJson(const std::vector<FixChange>& changes);
nlohmann::json errorResponseJson(const std::string& message);

} // namespace jtml::cli
