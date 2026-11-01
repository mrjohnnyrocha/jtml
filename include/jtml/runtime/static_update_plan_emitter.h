#pragma once

#include "jtml/runtime_plan.h"
#include "jtml/expression_ir.h"

#include <string>
#include <vector>

namespace jtml {

// Shared compiler element classification; no evaluation or runtime execution.
nlohmann::json staticLiteralElementParts(const std::vector<std::string>& words,
                                       const ExpressionTypeEnvironment& environment);

std::vector<std::string> validateStaticComponentModuleForProduction(
    const RuntimeProjectPlan& plan);
std::string emitStaticUpdatePlanAsset(const RuntimeProjectPlan& plan);
std::string emitStaticComponentModuleAsset(const RuntimeProjectPlan& plan);

} // namespace jtml
