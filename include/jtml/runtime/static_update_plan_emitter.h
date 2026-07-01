#pragma once

#include "jtml/runtime_plan.h"

#include <string>

namespace jtml {

std::string emitStaticUpdatePlanAsset(const RuntimeProjectPlan& plan);
std::string emitStaticComponentModuleAsset(const RuntimeProjectPlan& plan);

} // namespace jtml
