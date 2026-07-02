#pragma once

#include "jtml/runtime_plan.h"
#include "json.hpp"

struct ExpressionStatementNode;

namespace jtml {

struct RuntimePlanJsonOptions {
    bool includeExpressionCompilerHints = false;
};

nlohmann::json compileRuntimeExpressionPlan(const ExpressionStatementNode* expression);
nlohmann::json compileRuntimeExpressionPlan(const std::string& expression);

nlohmann::json runtimePlanBodyPlanToJson(
    const std::vector<RuntimePlanComponentBodyNode>& bodyPlan,
    const RuntimePlanJsonOptions& options = {});

nlohmann::json runtimePlanToExplainJson(const RuntimePlan& plan);

nlohmann::json runtimeProjectPlanToExplainJson(const RuntimeProjectPlan& plan);

nlohmann::json runtimePlanToClientJson(const RuntimePlan& plan);

nlohmann::json runtimeProjectPlanToClientJson(const RuntimeProjectPlan& plan);

nlohmann::json runtimePlanToJson(const RuntimePlan& plan);

nlohmann::json runtimeProjectPlanToJson(const RuntimeProjectPlan& plan);

} // namespace jtml
