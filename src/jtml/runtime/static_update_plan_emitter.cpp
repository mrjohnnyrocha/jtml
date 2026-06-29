#include "jtml/runtime/static_update_plan_emitter.h"

#include "jtml/runtime_plan_json.h"
#include "json.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>

namespace jtml {
namespace {

nlohmann::json moduleIdToJson(SemanticModuleId id) {
    return id == InvalidSemanticModuleId ? nlohmann::json(nullptr) : nlohmann::json(id);
}

std::string joinStrings(const std::vector<std::string>& values,
                        const std::string& separator) {
    std::ostringstream out;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) out << separator;
        out << values[i];
    }
    return out.str();
}

std::string joinInts(const std::vector<int>& values,
                     const std::string& separator) {
    std::ostringstream out;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) out << separator;
        out << values[i];
    }
    return out.str();
}

std::string componentUpdatePlanKey(
        const RuntimePlanComponentDefinition& definition) {
    std::ostringstream signature;
    for (size_t i = 0; i < definition.bodyPlan.size(); ++i) {
        if (i > 0) signature << "@@";
        const auto& node = definition.bodyPlan[i];
        signature << node.kind << '#'
                  << node.name << '#'
                  << node.text << '#'
                  << node.expression << '#'
                  << node.keyExpression << '#'
                  << node.sourceLine << '#'
                  << joinStrings(node.reads, "|") << '#'
                  << joinStrings(node.writes, "|") << '#'
                  << joinInts(node.childIndices, ",");
    }

    std::ostringstream key;
    key << (definition.moduleId == InvalidSemanticModuleId
                ? std::string("global")
                : std::to_string(definition.moduleId))
        << ':' << definition.name
        << ':' << definition.bodyPlan.size()
        << ':' << signature.str();
    return key.str();
}

std::string componentNodeHead(const RuntimePlanComponentBodyNode& node) {
    if (!node.name.empty()) return node.name;
    if (!node.head.empty()) return node.head;
    std::istringstream words(node.text);
    std::string head;
    words >> head;
    return head;
}

void addNameSet(std::set<std::string>& out,
                const std::vector<std::string>& values) {
    for (const auto& value : values) {
        if (!value.empty()) out.insert(value);
    }
}

std::vector<std::string> expandedNodeReads(
        const RuntimePlanComponentDefinition& definition,
        const RuntimePlanComponentBodyNode& node) {
    std::map<std::string, std::vector<std::string>> derivedDeps;
    for (const auto& candidate : definition.bodyPlan) {
        if (candidate.kind == "derived" && !candidate.name.empty()) {
            derivedDeps[candidate.name] = candidate.reads;
        }
    }

    std::set<std::string> reads;
    addNameSet(reads, node.reads);
    bool changed = true;
    int guard = 0;
    while (changed && guard++ < 1000) {
        changed = false;
        for (const auto& [name, deps] : derivedDeps) {
            if (!reads.count(name)) continue;
            const auto before = reads.size();
            addNameSet(reads, deps);
            changed = changed || reads.size() != before;
        }
    }
    return {reads.begin(), reads.end()};
}

using DefinitionKey = std::pair<SemanticModuleId, std::string>;

std::set<DefinitionKey> componentDefinitionKeys(
        const std::vector<RuntimePlanComponentDefinition>& definitions) {
    std::set<DefinitionKey> out;
    for (const auto& definition : definitions) {
        out.insert({definition.moduleId, definition.name});
        out.insert({InvalidSemanticModuleId, definition.name});
    }
    return out;
}

bool isKnownNestedComponent(
        const std::set<DefinitionKey>& definitionKeys,
        const RuntimePlanComponentDefinition& owner,
        const RuntimePlanComponentBodyNode& node) {
    const auto head = componentNodeHead(node);
    if (head.empty()) return false;
    const SemanticModuleId module =
        node.definitionModule != InvalidSemanticModuleId
            ? node.definitionModule
            : owner.moduleId;
    return definitionKeys.count({module, head}) ||
           definitionKeys.count({InvalidSemanticModuleId, head});
}

std::string staticPatchOperationKind(
        const std::set<DefinitionKey>& definitionKeys,
        const RuntimePlanComponentDefinition& definition,
        const RuntimePlanComponentBodyNode& node) {
    if (node.kind != "template") return "";
    const auto head = componentNodeHead(node);
    if (head == "if" || head == "for" || head == "slot") return "region";
    if (head == "else" || head == "while") return "";
    if (isKnownNestedComponent(definitionKeys, definition, node)) {
        return "nested-component";
    }
    if (head == "show" || head == "text") return "text";
    return "";
}

nlohmann::json staticPatchOperation(
        const std::string& kind,
        const RuntimePlanComponentBodyNode& node,
        size_t index) {
    nlohmann::json out = {
        {"operation", kind},
        {"nodeIndex", index},
        {"head", componentNodeHead(node)},
        {"sourceLine", node.sourceLine},
        {"expression", node.expression},
    };
    return out;
}

nlohmann::json compileStaticComponentUpdatePlan(
        const RuntimePlanComponentDefinition& definition,
        const std::set<DefinitionKey>& definitionKeys,
        const std::string& origin) {
    nlohmann::json entries = nlohmann::json::array();
    nlohmann::json entriesByRead = nlohmann::json::object();
    nlohmann::json unsafeEntries = nlohmann::json::array();

    for (size_t i = 0; i < definition.bodyPlan.size(); ++i) {
        const auto& node = definition.bodyPlan[i];
        if (node.kind != "template" || componentNodeHead(node) == "else") continue;
        const auto reads = expandedNodeReads(definition, node);
        if (reads.empty()) continue;

        const auto operationKind = staticPatchOperationKind(
            definitionKeys, definition, node);
        if (operationKind.empty()) {
            unsafeEntries.push_back({
                {"index", i},
                {"reads", reads},
                {"kind", componentNodeHead(node)},
                {"sourceLine", node.sourceLine},
            });
            continue;
        }

        nlohmann::json entry = {
            {"index", i},
            {"reads", reads},
            {"kind", componentNodeHead(node)},
            {"sourceLine", node.sourceLine},
            {"operation", staticPatchOperation(operationKind, node, i)},
        };
        const size_t entryIndex = entries.size();
        entries.push_back(entry);
        for (const auto& read : reads) {
            entriesByRead[read].push_back(entry);
        }
        (void)entryIndex;
    }

    return {
        {"origin", origin},
        {"moduleId", moduleIdToJson(definition.moduleId)},
        {"name", definition.name},
        {"key", componentUpdatePlanKey(definition)},
        {"params", definition.params},
        {"localState", definition.localState},
        {"localDerived", definition.localDerived},
        {"localActions", definition.localActions},
        {"bodyPlan", runtimePlanBodyPlanToJson(definition.bodyPlan)},
        {"entries", entries},
        {"entriesByRead", entriesByRead},
        {"unsafeEntries", unsafeEntries},
        {"staticPatchCoverage", "text-region-nested-first-slice"},
    };
}

void appendComponentPlans(
        nlohmann::json& components,
        const std::vector<RuntimePlanComponentDefinition>& definitions,
        const std::set<DefinitionKey>& definitionKeys,
        const std::string& origin) {
    for (const auto& definition : definitions) {
        components.push_back(
            compileStaticComponentUpdatePlan(definition, definitionKeys, origin));
    }
}

std::string escapeJsScriptText(std::string value) {
    auto replaceAll = [](std::string& text,
                         const std::string& needle,
                         const std::string& replacement) {
        size_t pos = 0;
        while ((pos = text.find(needle, pos)) != std::string::npos) {
            text.replace(pos, needle.size(), replacement);
            pos += replacement.size();
        }
    };
    replaceAll(value, "\\", "\\\\");
    replaceAll(value, "'", "\\'");
    replaceAll(value, "\n", "\\n");
    replaceAll(value, "\r", "\\r");
    replaceAll(value, "\xE2\x80\xA8", "\\u2028");
    replaceAll(value, "\xE2\x80\xA9", "\\u2029");
    return value;
}

} // namespace

std::string emitStaticUpdatePlanAsset(const RuntimeProjectPlan& plan) {
    nlohmann::json components = nlohmann::json::array();
    auto linkedKeys = componentDefinitionKeys(plan.linkedPlan.componentDefinitions);
    appendComponentPlans(components,
                         plan.linkedPlan.componentDefinitions,
                         linkedKeys,
                         "linked");

    for (const auto& module : plan.modules) {
        if (!module.clientExecutable) continue;
        auto moduleKeys = componentDefinitionKeys(module.plan.componentDefinitions);
        appendComponentPlans(components,
                             module.plan.componentDefinitions,
                             moduleKeys,
                             "module:" + std::to_string(module.id));
    }

    nlohmann::json asset = {
        {"version", 1},
        {"sourceOfTruth", "runtime client manifest"},
        {"mode", "csp-safe static update plans"},
        {"dynamicGeneratedUpdateFunctions", false},
        {"componentCount", components.size()},
        {"components", components},
    };

    std::ostringstream out;
    out << "/* Generated by jtml build --target browser. CSP-safe static update plan seed. */\n"
        << "(function () {\n"
        << "  const plans = JSON.parse('" << escapeJsScriptText(asset.dump()) << "');\n"
        << "  window.__jtml_static_update_plans = plans;\n"
        << "  if (window.jtml) {\n"
        << "    window.jtml.staticUpdatePlans = plans;\n"
        << "    window.jtml.runtimeSecurity = window.jtml.runtimeSecurity || {};\n"
        << "    window.jtml.runtimeSecurity.staticUpdatePlansAsset = true;\n"
        << "  }\n"
        << "  if (window.dispatchEvent && window.CustomEvent) {\n"
        << "    window.dispatchEvent(new CustomEvent('jtml:static-update-plans-ready', { detail: plans }));\n"
        << "  }\n"
        << "}());\n";
    return out.str();
}

} // namespace jtml
