#ifndef JTML_CLIENT_MANIFEST_EMITTER_H
#define JTML_CLIENT_MANIFEST_EMITTER_H

#include "jtml/ast.h"

#include <memory>
#include <string>
#include <vector>

namespace jtml {

std::string emitClientManifestScript(const std::vector<std::unique_ptr<ASTNode>>& program);

} // namespace jtml

#endif // JTML_CLIENT_MANIFEST_EMITTER_H
