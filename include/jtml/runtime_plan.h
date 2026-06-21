#pragma once

#include "jtml/ast.h"
#include "jtml/semantic.h"
#include "jtml/semantic/module_graph.h"

#include <map>
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
    SemanticModuleId definitionModule = InvalidSemanticModuleId;
    int sourceLine = 0;
    int indent = 0;
    int parentIndex = -1;
    std::vector<int> childIndices;
    std::string kind;
    std::string head;
    std::string name;
    std::string text;
    std::string operatorToken;
    std::string expression;
    std::string keyExpression;
    std::vector<std::string> reads;
    std::vector<std::string> writes;
    bool renderRoot = false;
};

struct RuntimePlanComponentDefinition {
    SemanticModuleId moduleId = InvalidSemanticModuleId;
    std::string name;
    std::vector<std::string> params;
    std::vector<std::string> emits;
    std::map<std::string, int> emitArity;
    std::map<std::string, std::vector<std::string>> emitPayloads;
    std::map<std::string, std::vector<std::string>> emitPayloadTypes;
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
    SemanticModuleId moduleId = InvalidSemanticModuleId;
    SemanticModuleId definitionModule = InvalidSemanticModuleId;
    std::string id;
    std::string component;
    int instanceId = 0;
    std::string role;
    std::vector<SemanticProperty> params;
    std::vector<SemanticProperty> locals;
    std::string slotSource;
    std::string slotHex;
    std::vector<RuntimePlanComponentBodyNode> slotPlan;
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

struct RuntimeModulePlan {
    SemanticModuleId id = InvalidSemanticModuleId;
    std::string path;
    bool astAvailable = false;
    bool clientExecutable = false;
    std::string syntax;
    RuntimePlan plan;
};

struct RuntimeProjectPlan {
    SemanticModuleId entry = InvalidSemanticModuleId;
    RuntimePlan linkedPlan;
    std::vector<RuntimeModulePlan> modules;
};

RuntimePlan buildRuntimePlan(const std::vector<std::unique_ptr<ASTNode>>& program);

RuntimePlan buildRuntimePlan(const std::vector<std::unique_ptr<ASTNode>>& program,
                             const SemanticProgram& semantic);

RuntimeProjectPlan buildRuntimePlan(const SemanticProject& project);

RuntimeProjectPlan buildRuntimePlan(const SemanticProject& project,
                                    const RuntimePlan& linkedPlan);

} // namespace jtml
