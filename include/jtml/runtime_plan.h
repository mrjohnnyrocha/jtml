#pragma once

#include "jtml/ast.h"
#include "jtml/semantic.h"

#include <memory>
#include <string>
#include <vector>

namespace jtml {

struct RuntimePlanStatement {
    std::string kind;
    std::string lhs;
    std::string expr;
    std::string condition;
    std::vector<RuntimePlanStatement> thenStatements;
    std::vector<RuntimePlanStatement> elseStatements;
};

struct RuntimePlanBinding {
    std::string name;
    std::string expr;
};

struct RuntimePlanAction {
    std::string name;
    std::vector<std::string> params;
    std::vector<RuntimePlanStatement> body;
};

struct RuntimePlanComponentBodyNode {
    int indent = 0;
    int parentIndex = -1;
    std::string kind;
    std::string head;
    std::string name;
    std::string text;
    bool renderRoot = false;
};

struct RuntimePlanComponentDefinition {
    std::string name;
    std::vector<std::string> params;
    std::vector<std::string> localState;
    std::vector<std::string> localDerived;
    std::vector<std::string> localActions;
    std::vector<std::string> localEffects;
    std::vector<std::string> eventBindings;
    std::string bodySource;
    std::string bodyHex;
    std::vector<RuntimePlanComponentBodyNode> bodyPlan;
    bool hasSlot = false;
    int bodyNodeCount = 0;
    int rootTemplateNodeCount = 0;
    int slotCount = 0;
    int sourceLine = 0;
};

struct RuntimePlanComponentInstance {
    std::string id;
    std::string component;
    int instanceId = 0;
    std::string role;
    std::vector<SemanticProperty> params;
    std::vector<SemanticProperty> locals;
    int sourceLine = 0;
};

struct RuntimePlan {
    SemanticProgram semantic;
    std::vector<RuntimePlanBinding> state;
    std::vector<RuntimePlanBinding> derived;
    std::vector<RuntimePlanAction> actions;
    std::vector<SemanticRoute> routes;
    std::vector<SemanticFetch> fetches;
    std::vector<RuntimePlanComponentDefinition> componentDefinitions;
    std::vector<RuntimePlanComponentInstance> componentInstances;
};

RuntimePlan buildRuntimePlan(const std::vector<std::unique_ptr<ASTNode>>& program);

RuntimePlan buildRuntimePlan(const std::vector<std::unique_ptr<ASTNode>>& program,
                             const SemanticProgram& semantic);

} // namespace jtml
