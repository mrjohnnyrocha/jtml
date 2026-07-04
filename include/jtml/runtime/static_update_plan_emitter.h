#pragma once

#include "jtml/runtime_plan.h"

#include <string>
#include <vector>

namespace jtml {

std::vector<std::string> validateStaticComponentModuleForProduction(
    const RuntimeProjectPlan& plan);
std::string emitStaticUpdatePlanAsset(const RuntimeProjectPlan& plan);
std::string emitStaticComponentModuleAsset(const RuntimeProjectPlan& plan);

} // namespace jtml
