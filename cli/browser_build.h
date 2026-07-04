// cli/browser_build.h
//
// Browser production build service. This is intentionally separate from
// cmd_basic.cpp so `jtml build --target browser` can be link-checked against
// the browser production compiler/runtime target without live interpreter or
// legacy Environment runtime objects.
#pragma once

#include "util.h"

#include "jtml/runtime_plan.h"

#include <memory>
#include <string>
#include <vector>

class ASTNode;

namespace jtml::cli {

jtml::RuntimeProjectPlan buildRuntimeProjectPlanForInput(
    const std::string& inputFile,
    SyntaxMode syntax,
    const std::vector<std::unique_ptr<ASTNode>>& linkedProgram,
    const jtml::SemanticProgram& linkedSemantic);

int cmdBuildBrowser(const Options& o);
int cmdTranspileBrowser(const Options& o);

} // namespace jtml::cli
