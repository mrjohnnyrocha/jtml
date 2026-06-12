#pragma once

#include "jtml/semantic/module_graph.h"

#include "nlohmann/json.hpp"

namespace jtml {

nlohmann::json semanticExportRecordsToJson(const std::vector<SemanticExport>& exports);
nlohmann::json semanticProjectIssuesToJson(const std::vector<SemanticProjectIssue>& issues);
nlohmann::json semanticProjectToJson(const SemanticProject& project);

} // namespace jtml
