#pragma once
#include "jtml/runtime_plan.h"

namespace jtml {
// Closed literal-only proof and emission. Throws before returning any output.
// Definitions are selected by the retained project's module-qualified identity.
std::string emitStaticHtml(const SemanticProject& project, const RuntimeProjectPlan& plan);
}
