#pragma once

#include "jtml/runtime_plan.h"
#include "json.hpp"

namespace jtml {

nlohmann::json compileRuntimeExpressionPlan(const std::string& expression);

nlohmann::json runtimePlanBodyPlanToJson(
    const std::vector<RuntimePlanComponentBodyNode>& bodyPlan);

nlohmann::json runtimePlanToExplainJson(const RuntimePlan& plan);

nlohmann::json runtimeProjectPlanToExplainJson(const RuntimeProjectPlan& plan);

nlohmann::json runtimePlanToClientJson(const RuntimePlan& plan);

nlohmann::json runtimeProjectPlanToClientJson(const RuntimeProjectPlan& plan);

nlohmann::json runtimePlanToJson(const RuntimePlan& plan);

nlohmann::json runtimeProjectPlanToJson(const RuntimeProjectPlan& plan);

} // namespace jtml
