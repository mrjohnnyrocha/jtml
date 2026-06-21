#include "jtml/runtime_plan_json.h"

namespace jtml {
namespace {

nlohmann::json semanticPropertiesToJson(const std::vector<SemanticProperty>& properties) {
    nlohmann::json out = nlohmann::json::object();
    for (const auto& property : properties) out[property.name] = property.value;
    return out;
}

nlohmann::json moduleIdToJson(SemanticModuleId id) {
    return id == InvalidSemanticModuleId ? nlohmann::json(nullptr) : nlohmann::json(id);
}

nlohmann::json runtimePlanStatementsToJson(const std::vector<RuntimePlanStatement>& statements);

nlohmann::json runtimePlanBindingsObjectToJson(const std::vector<RuntimePlanBinding>& bindings) {
    nlohmann::json out = nlohmann::json::object();
    for (const auto& binding : bindings) out[binding.name] = binding.expr;
    return out;
}

nlohmann::json runtimePlanActionsObjectToJson(const std::vector<RuntimePlanAction>& actions) {
    nlohmann::json out = nlohmann::json::object();
    for (const auto& action : actions) {
        out[action.name] = {
            {"params", action.params},
            {"body", runtimePlanStatementsToJson(action.body)},
        };
    }
    return out;
}

nlohmann::json runtimePlanBindingsToJson(const std::vector<RuntimePlanBinding>& bindings) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& binding : bindings) {
        out.push_back({
            {"name", binding.name},
            {"expr", binding.expr},
        });
    }
    return out;
}

nlohmann::json runtimePlanStatementsToJson(const std::vector<RuntimePlanStatement>& statements) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& statement : statements) {
        nlohmann::json item = {
            {"kind", statement.kind},
        };
        if (!statement.lhs.empty()) item["lhs"] = statement.lhs;
        if (!statement.expr.empty()) item["expr"] = statement.expr;
        if (!statement.condition.empty()) item["condition"] = statement.condition;
        if (!statement.thenStatements.empty()) {
            item["then"] = runtimePlanStatementsToJson(statement.thenStatements);
        }
        if (!statement.elseStatements.empty()) {
            item["else"] = runtimePlanStatementsToJson(statement.elseStatements);
        }
        out.push_back(std::move(item));
    }
    return out;
}

nlohmann::json runtimePlanActionsToJson(const std::vector<RuntimePlanAction>& actions) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& action : actions) {
        out.push_back({
            {"name", action.name},
            {"params", action.params},
            {"body", runtimePlanStatementsToJson(action.body)},
        });
    }
    return out;
}

nlohmann::json runtimePlanRoutesToJson(const std::vector<SemanticRoute>& routes) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& route : routes) {
        out.push_back({
            {"path", route.path},
            {"name", route.component},
            {"component", route.component},
            {"params", route.params},
            {"load", route.loads},
            {"loads", route.loads},
        });
    }
    return out;
}

nlohmann::json runtimePlanFetchesToJson(const std::vector<SemanticFetch>& fetches) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& fetch : fetches) {
        out.push_back({
            {"name", fetch.name},
            {"url", fetch.url},
            {"method", fetch.method},
            {"bodyExpr", fetch.bodyExpr},
            {"refreshAction", fetch.refreshAction},
            {"cache", fetch.cache},
            {"credentials", fetch.credentials},
            {"timeoutMs", fetch.timeoutMs},
            {"retryCount", fetch.retryCount},
            {"stalePolicy", fetch.stalePolicy},
            {"group", fetch.group},
            {"cacheKeyExpr", fetch.cacheKeyExpr},
            {"revalidateMs", fetch.revalidateMs},
            {"dedupe", fetch.dedupe},
            {"background", fetch.background},
            {"lazy", fetch.lazy},
        });
    }
    return out;
}

nlohmann::json runtimePlanComponentDefinitionsToJson(
        const std::vector<RuntimePlanComponentDefinition>& definitions) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& definition : definitions) {
        out.push_back({
            {"moduleId", moduleIdToJson(definition.moduleId)},
            {"name", definition.name},
            {"params", definition.params},
            {"emits", definition.emits},
            {"emitArity", definition.emitArity},
            {"emitPayloads", definition.emitPayloads},
            {"emitPayloadTypes", definition.emitPayloadTypes},
            {"localState", definition.localState},
            {"localDerived", definition.localDerived},
            {"localActions", definition.localActions},
            {"localEffects", definition.localEffects},
            {"eventBindings", definition.eventBindings},
            {"bodySource", definition.bodySource},
            {"bodyHex", definition.bodyHex},
            {"bodyPlan", runtimePlanBodyPlanToJson(definition.bodyPlan)},
            {"hasSlot", definition.hasSlot},
            {"bodyNodeCount", definition.bodyNodeCount},
            {"rootTemplateNodeCount", definition.rootTemplateNodeCount},
            {"slotCount", definition.slotCount},
            {"sourceLine", definition.sourceLine},
        });
    }
    return out;
}

nlohmann::json runtimePlanComponentInstancesToJson(
        const std::vector<RuntimePlanComponentInstance>& instances) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& instance : instances) {
        out.push_back({
            {"moduleId", moduleIdToJson(instance.moduleId)},
            {"definitionModule", moduleIdToJson(instance.definitionModule)},
            {"id", instance.id},
            {"component", instance.component},
            {"instanceId", instance.instanceId},
            {"role", instance.role},
            {"params", semanticPropertiesToJson(instance.params)},
            {"locals", semanticPropertiesToJson(instance.locals)},
            {"slotSource", instance.slotSource},
            {"slotHex", instance.slotHex},
            {"slotPlan", runtimePlanBodyPlanToJson(instance.slotPlan)},
            {"sourceLine", instance.sourceLine},
        });
    }
    return out;
}

nlohmann::json runtimePlanComponentDefinitionsToClientJson(
        const std::vector<RuntimePlanComponentDefinition>& definitions) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& definition : definitions) {
        out.push_back({
            {"moduleId", moduleIdToJson(definition.moduleId)},
            {"name", definition.name},
            {"params", definition.params},
            {"emits", definition.emits},
            {"emitArity", definition.emitArity},
            {"emitPayloads", definition.emitPayloads},
            {"emitPayloadTypes", definition.emitPayloadTypes},
            {"localState", definition.localState},
            {"localDerived", definition.localDerived},
            {"localActions", definition.localActions},
            {"localEffects", definition.localEffects},
            {"eventBindings", definition.eventBindings},
            {"bodyPlan", runtimePlanBodyPlanToJson(definition.bodyPlan)},
            {"hasSlot", definition.hasSlot},
            {"bodyNodeCount", definition.bodyNodeCount},
            {"rootTemplateNodeCount", definition.rootTemplateNodeCount},
            {"slotCount", definition.slotCount},
        });
    }
    return out;
}

nlohmann::json runtimePlanComponentInstancesToClientJson(
        const std::vector<RuntimePlanComponentInstance>& instances) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& instance : instances) {
        out.push_back({
            {"moduleId", moduleIdToJson(instance.moduleId)},
            {"definitionModule", moduleIdToJson(instance.definitionModule)},
            {"id", instance.id},
            {"component", instance.component},
            {"instanceId", instance.instanceId},
            {"role", instance.role},
            {"params", semanticPropertiesToJson(instance.params)},
            {"locals", semanticPropertiesToJson(instance.locals)},
            {"slotPlan", runtimePlanBodyPlanToJson(instance.slotPlan)},
        });
    }
    return out;
}

nlohmann::json runtimePlanSemanticToJson(const SemanticProgram& semantic) {
    return {
        {"state", semantic.state},
        {"constants", semantic.constants},
        {"derived", semantic.derived},
        {"actions", semantic.actions},
        {"fetches", semantic.fetches},
        {"routes", semantic.routes},
        {"components", semantic.components},
        {"stores", semantic.stores},
        {"effects", semantic.effects},
        {"imports", semantic.imports},
        {"externs", semantic.externs},
        {"componentDefinitionCount", semantic.componentDefinitions.size()},
        {"componentInstanceCount", semantic.componentInstances.size()},
    };
}

} // namespace

nlohmann::json runtimePlanBodyPlanToJson(
        const std::vector<RuntimePlanComponentBodyNode>& bodyPlan) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& node : bodyPlan) {
        out.push_back({
            {"definitionModule", moduleIdToJson(node.definitionModule)},
            {"indent", node.indent},
            {"parentIndex", node.parentIndex},
            {"childIndices", node.childIndices},
            {"kind", node.kind},
            {"head", node.head},
            {"name", node.name},
            {"text", node.text},
            {"operator", node.operatorToken},
            {"expression", node.expression},
            {"keyExpression", node.keyExpression},
            {"renderRoot", node.renderRoot},
        });
    }
    return out;
}

nlohmann::json runtimePlanToExplainJson(const RuntimePlan& plan) {
    return {
        {"sourceOfTruth", "typed AST + semantic IR"},
        {"semantic", runtimePlanSemanticToJson(plan.semantic)},
        {"state", runtimePlanBindingsToJson(plan.state)},
        {"derived", runtimePlanBindingsToJson(plan.derived)},
        {"actions", runtimePlanActionsToJson(plan.actions)},
        {"routes", runtimePlanRoutesToJson(plan.routes)},
        {"fetches", runtimePlanFetchesToJson(plan.fetches)},
        {"componentDefinitions", runtimePlanComponentDefinitionsToJson(plan.componentDefinitions)},
        {"componentInstances", runtimePlanComponentInstancesToJson(plan.componentInstances)},
    };
}

nlohmann::json runtimeProjectPlanToExplainJson(const RuntimeProjectPlan& plan) {
    nlohmann::json modules = nlohmann::json::array();
    for (const auto& module : plan.modules) {
        modules.push_back({
            {"id", moduleIdToJson(module.id)},
            {"path", module.path},
            {"astAvailable", module.astAvailable},
            {"clientExecutable", module.clientExecutable},
            {"syntax", module.syntax},
            {"plan", runtimePlanToExplainJson(module.plan)},
        });
    }

    return {
        {"sourceOfTruth", "SemanticProject retained per-file AST + semantic IR"},
        {"entry", moduleIdToJson(plan.entry)},
        {"linkedPlan", runtimePlanToExplainJson(plan.linkedPlan)},
        {"modules", modules},
    };
}

nlohmann::json runtimePlanToClientJson(const RuntimePlan& plan) {
    return {
        {"state", runtimePlanBindingsObjectToJson(plan.state)},
        {"derived", runtimePlanBindingsObjectToJson(plan.derived)},
        {"actions", runtimePlanActionsObjectToJson(plan.actions)},
        {"routes", runtimePlanRoutesToJson(plan.routes)},
        {"fetches", runtimePlanFetchesToJson(plan.fetches)},
        {"componentDefinitions",
            runtimePlanComponentDefinitionsToClientJson(plan.componentDefinitions)},
        {"componentInstances",
            runtimePlanComponentInstancesToClientJson(plan.componentInstances)},
    };
}

nlohmann::json runtimeProjectPlanToClientJson(const RuntimeProjectPlan& plan) {
    nlohmann::json client = runtimePlanToClientJson(plan.linkedPlan);
    nlohmann::json modules = nlohmann::json::array();
    for (const auto& module : plan.modules) {
        nlohmann::json item = {
            {"id", moduleIdToJson(module.id)},
            {"executable", module.clientExecutable},
        };
        if (module.clientExecutable) {
            item["plan"] = runtimePlanToClientJson(module.plan);
        }
        modules.push_back(std::move(item));
    }
    client["project"] = {
        {"sourceOfTruth", "runtime client manifest"},
        {"entry", moduleIdToJson(plan.entry)},
        {"linkedPlan", runtimePlanToClientJson(plan.linkedPlan)},
        {"modules", modules},
    };
    return client;
}

nlohmann::json runtimePlanToJson(const RuntimePlan& plan) {
    return runtimePlanToExplainJson(plan);
}

nlohmann::json runtimeProjectPlanToJson(const RuntimeProjectPlan& plan) {
    return runtimeProjectPlanToExplainJson(plan);
}

} // namespace jtml
