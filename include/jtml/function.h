// Function.h

#pragma once

#include "jtml/ast.h"
#include <vector>
#include <string>
#include <memory>

class Environment;

namespace JTMLInterpreter {

class Function {
public:
    std::string name;
    std::vector<Parameter> parameters;
    std::string returnType;
    std::vector<std::unique_ptr<ASTNode>> body;
    std::shared_ptr<Environment> closure;
    bool isAsync = false;

    Function(
        std::string funcName,
        std::vector<Parameter> params,
        std::string retType,
        std::vector<std::unique_ptr<ASTNode>> funcBody,
        std::shared_ptr<Environment> closureEnv,
        bool asyncFlag = false)
        : name(std::move(funcName)),
          parameters(std::move(params)),
          returnType(std::move(retType)),
          body(std::move(funcBody)), // Move the body here
          closure(std::move(closureEnv)),
          isAsync(asyncFlag) {}
};

} // namespace JTMLInterpreter
