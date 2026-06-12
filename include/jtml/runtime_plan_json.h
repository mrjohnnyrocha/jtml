#pragma once

#include "jtml/runtime_plan.h"
#include "json.hpp"

namespace jtml {

nlohmann::json runtimePlanBodyPlanToJson(
    const std::vector<RuntimePlanComponentBodyNode>& bodyPlan);

nlohmann::json runtimePlanToJson(const RuntimePlan& plan);

nlohmann::json runtimeProjectPlanToJson(const RuntimeProjectPlan& plan);

} // namespace jtml
