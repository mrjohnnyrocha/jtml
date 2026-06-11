#ifndef JTML_CLIENT_MANIFEST_EMITTER_H
#define JTML_CLIENT_MANIFEST_EMITTER_H

#include "jtml/ast.h"
#include "jtml/runtime_plan.h"

#include <memory>
#include <string>
#include <vector>

namespace jtml {

std::string emitClientManifestScript(const std::vector<std::unique_ptr<ASTNode>>& program);
std::string emitClientManifestScript(const RuntimePlan& plan);

} // namespace jtml

#endif // JTML_CLIENT_MANIFEST_EMITTER_H
