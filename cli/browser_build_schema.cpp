#include "browser_build_schema.h"

#include "json.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <sstream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace jtml::cli {
namespace {

std::string readAsset(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("browser build schema missing asset: " + path.string());
    }
    return std::string((std::istreambuf_iterator<char>(input)),
                       std::istreambuf_iterator<char>());
}

void requireExists(const std::filesystem::path& path,
                   std::vector<std::string>& issues) {
    if (!std::filesystem::exists(path)) {
        issues.push_back("missing required asset: " + path.string());
    }
}

void rejectContains(const std::string& asset,
                    const std::string& text,
                    const std::string& label,
                    std::vector<std::string>& issues) {
    if (asset.find(text) != std::string::npos) {
        issues.push_back(label + " must not contain `" + text + "`");
    }
}

void requireContains(const std::string& asset,
                     const std::string& text,
                     const std::string& label,
                     std::vector<std::string>& issues) {
    if (asset.find(text) == std::string::npos) {
        issues.push_back(label + " must contain `" + text + "`");
    }
}

void rejectDynamicCompilation(const std::string& asset,
                              const std::string& label,
                              std::vector<std::string>& issues) {
    rejectContains(asset, "new Function", label, issues);
    rejectContains(asset, "eval(", label, issues);
}

nlohmann::json extractClientManifestJson(const std::string& html,
                                         std::vector<std::string>& issues) {
    const std::string open = "<script type=\"application/json\" id=\"__jtml_client_manifest\">";
    const std::string close = "</script>";
    const auto start = html.find(open);
    if (start == std::string::npos) {
        issues.push_back("index.html must contain `#__jtml_client_manifest`");
        return nlohmann::json::object();
    }
    const auto bodyStart = start + open.size();
    const auto end = html.find(close, bodyStart);
    if (end == std::string::npos) {
        issues.push_back("index.html client manifest script must be closed");
        return nlohmann::json::object();
    }
    try {
        return nlohmann::json::parse(html.substr(bodyStart, end - bodyStart));
    } catch (const std::exception& e) {
        issues.push_back(std::string("index.html client manifest must parse as JSON: ") + e.what());
        return nlohmann::json::object();
    }
}

void rejectJsonKeyRecursive(const nlohmann::json& value,
                            const std::string& key,
                            const std::string& path,
                            std::vector<std::string>& issues) {
    if (value.is_object()) {
        for (auto it = value.begin(); it != value.end(); ++it) {
            const auto childPath = path + "." + it.key();
            if (it.key() == key) {
                issues.push_back("client manifest must not contain `" + key +
                                 "` at " + childPath);
            }
            rejectJsonKeyRecursive(it.value(), key, childPath, issues);
        }
    } else if (value.is_array()) {
        for (size_t i = 0; i < value.size(); ++i) {
            rejectJsonKeyRecursive(value[i],
                                   key,
                                   path + "[" + std::to_string(i) + "]",
                                   issues);
        }
    }
}

bool isAllowedKey(const std::vector<std::string>& allowed, const std::string& key) {
    return std::find(allowed.begin(), allowed.end(), key) != allowed.end();
}

void rejectUnknownKeys(const nlohmann::json& value,
                       const std::vector<std::string>& allowed,
                       const std::string& path,
                       std::vector<std::string>& issues) {
    if (!value.is_object()) return;
    for (auto it = value.begin(); it != value.end(); ++it) {
        if (!isAllowedKey(allowed, it.key())) {
            issues.push_back(path + " contains unsupported key `" + it.key() + "`");
        }
    }
}

bool requireObjectAt(const nlohmann::json& value,
                     const std::string& path,
                     std::vector<std::string>& issues) {
    if (!value.is_object()) {
        issues.push_back(path + " must be an object");
        return false;
    }
    return true;
}

bool requireArrayAt(const nlohmann::json& value,
                    const std::string& path,
                    std::vector<std::string>& issues) {
    if (!value.is_array()) {
        issues.push_back(path + " must be an array");
        return false;
    }
    return true;
}

void requireOptionalArray(const nlohmann::json& value,
                          const std::string& key,
                          const std::string& path,
                          std::vector<std::string>& issues) {
    if (value.contains(key) && !value.at(key).is_array()) {
        issues.push_back(path + "." + key + " must be an array when present");
    }
}

void requireOptionalObject(const nlohmann::json& value,
                           const std::string& key,
                           const std::string& path,
                           std::vector<std::string>& issues) {
    if (value.contains(key) && !value.at(key).is_object()) {
        issues.push_back(path + "." + key + " must be an object when present");
    }
}

void requireOptionalString(const nlohmann::json& value,
                           const std::string& key,
                           const std::string& path,
                           std::vector<std::string>& issues) {
    if (value.contains(key) && !value.at(key).is_string()) {
        issues.push_back(path + "." + key + " must be a string when present");
    }
}

void requireOptionalBool(const nlohmann::json& value,
                         const std::string& key,
                         const std::string& path,
                         std::vector<std::string>& issues) {
    if (value.contains(key) && !value.at(key).is_boolean()) {
        issues.push_back(path + "." + key + " must be a boolean when present");
    }
}

void requireOptionalNumber(const nlohmann::json& value,
                           const std::string& key,
                           const std::string& path,
                           std::vector<std::string>& issues) {
    if (value.contains(key) && !value.at(key).is_number_integer()) {
        issues.push_back(path + "." + key + " must be an integer when present");
    }
}

void requireOptionalModuleId(const nlohmann::json& value,
                             const std::string& key,
                             const std::string& path,
                             std::vector<std::string>& issues) {
    if (!value.contains(key)) return;
    const auto& candidate = value.at(key);
    if (!(candidate.is_null() || candidate.is_number_integer())) {
        issues.push_back(path + "." + key + " must be an integer module id or null");
    }
}

void validateClientRuntimePlanSchema(const nlohmann::json& manifest,
                                     const std::string& path,
                                     bool allowProject,
                                     std::vector<std::string>& issues);

void validateClientExpressionPlan(const nlohmann::json& value,
                                  const std::string& path,
                                  std::vector<std::string>& issues);

void validateClientExpressionPlanArray(const nlohmann::json& value,
                                       const std::string& path,
                                       std::vector<std::string>& issues) {
    if (!requireArrayAt(value, path, issues)) return;
    for (size_t i = 0; i < value.size(); ++i) {
        validateClientExpressionPlan(value[i], path + "[" + std::to_string(i) + "]", issues);
    }
}

void validateClientExpressionPlan(const nlohmann::json& value,
                                  const std::string& path,
                                  std::vector<std::string>& issues) {
    if (!requireObjectAt(value, path, issues)) return;
    static const std::vector<std::string> allowedExpressionKeys = {
        "kind", "source", "producer", "valueType", "astKind", "value",
        "root", "segments", "property", "base", "index", "operator",
        "argument", "left", "right", "test", "whenTrue", "whenFalse",
        "elements", "entries", "key", "callee", "arguments", "method",
        "start", "end"
    };
    rejectUnknownKeys(value, allowedExpressionKeys, path, issues);
    requireOptionalString(value, "kind", path, issues);
    requireOptionalString(value, "source", path, issues);
    requireOptionalString(value, "producer", path, issues);
    requireOptionalString(value, "valueType", path, issues);
    requireOptionalNumber(value, "astKind", path, issues);
    requireOptionalString(value, "root", path, issues);
    requireOptionalString(value, "property", path, issues);
    requireOptionalString(value, "operator", path, issues);
    requireOptionalString(value, "key", path, issues);
    requireOptionalString(value, "callee", path, issues);
    requireOptionalString(value, "method", path, issues);
    requireOptionalArray(value, "segments", path, issues);
    if (value.contains("base")) {
        validateClientExpressionPlan(value.at("base"), path + ".base", issues);
    }
    if (value.contains("index")) {
        validateClientExpressionPlan(value.at("index"), path + ".index", issues);
    }
    if (value.contains("argument")) {
        validateClientExpressionPlan(value.at("argument"), path + ".argument", issues);
    }
    if (value.contains("left")) {
        validateClientExpressionPlan(value.at("left"), path + ".left", issues);
    }
    if (value.contains("right")) {
        validateClientExpressionPlan(value.at("right"), path + ".right", issues);
    }
    if (value.contains("test")) {
        validateClientExpressionPlan(value.at("test"), path + ".test", issues);
    }
    if (value.contains("whenTrue")) {
        validateClientExpressionPlan(value.at("whenTrue"), path + ".whenTrue", issues);
    }
    if (value.contains("whenFalse")) {
        validateClientExpressionPlan(value.at("whenFalse"), path + ".whenFalse", issues);
    }
    if (value.contains("start")) {
        validateClientExpressionPlan(value.at("start"), path + ".start", issues);
    }
    if (value.contains("end")) {
        validateClientExpressionPlan(value.at("end"), path + ".end", issues);
    }
    if (value.contains("elements")) {
        validateClientExpressionPlanArray(value.at("elements"), path + ".elements", issues);
    }
    if (value.contains("arguments")) {
        validateClientExpressionPlanArray(value.at("arguments"), path + ".arguments", issues);
    }
    if (value.contains("entries")) {
        if (requireArrayAt(value.at("entries"), path + ".entries", issues)) {
            const auto& entries = value.at("entries");
            for (size_t i = 0; i < entries.size(); ++i) {
                const std::string entryPath = path + ".entries[" + std::to_string(i) + "]";
                if (!requireObjectAt(entries[i], entryPath, issues)) continue;
                rejectUnknownKeys(entries[i], {"key", "value"}, entryPath, issues);
                requireOptionalString(entries[i], "key", entryPath, issues);
                if (entries[i].contains("value")) {
                    validateClientExpressionPlan(entries[i].at("value"),
                                                 entryPath + ".value",
                                                 issues);
                } else {
                    issues.push_back(entryPath + " missing `value`");
                }
            }
        }
    }
}

void validateClientLoopPlan(const nlohmann::json& value,
                            const std::string& path,
                            std::vector<std::string>& issues) {
    if (!requireObjectAt(value, path, issues)) return;
    rejectUnknownKeys(value, {"item", "collectionExpression", "collectionPlan"},
                      path, issues);
    requireOptionalString(value, "item", path, issues);
    requireOptionalString(value, "collectionExpression", path, issues);
    if (value.contains("collectionPlan")) {
        validateClientExpressionPlan(value.at("collectionPlan"),
                                     path + ".collectionPlan",
                                     issues);
    } else {
        issues.push_back(path + " missing `collectionPlan`");
    }
}

void validateClientBodyPlanArray(const nlohmann::json& value,
                                 const std::string& path,
                                 std::vector<std::string>& issues) {
    if (!requireArrayAt(value, path, issues)) return;
    static const std::vector<std::string> allowedBodyNodeKeys = {
        "definitionModule", "sourceLine", "sourceColumn", "indent", "parentIndex",
        "childIndices", "kind", "head", "name", "text", "operator", "expression",
        "expressionPlan", "keyExpression", "keyExpressionPlan", "wordPlans",
        "reads", "writes", "renderRoot", "loopPlan", "argPlans"
    };
    for (size_t i = 0; i < value.size(); ++i) {
        const std::string itemPath = path + "[" + std::to_string(i) + "]";
        const auto& node = value[i];
        if (!requireObjectAt(node, itemPath, issues)) continue;
        rejectUnknownKeys(node, allowedBodyNodeKeys, itemPath, issues);
        requireOptionalModuleId(node, "definitionModule", itemPath, issues);
        requireOptionalNumber(node, "sourceLine", itemPath, issues);
        requireOptionalNumber(node, "sourceColumn", itemPath, issues);
        requireOptionalNumber(node, "indent", itemPath, issues);
        requireOptionalNumber(node, "parentIndex", itemPath, issues);
        requireOptionalArray(node, "childIndices", itemPath, issues);
        requireOptionalString(node, "kind", itemPath, issues);
        requireOptionalString(node, "head", itemPath, issues);
        requireOptionalString(node, "name", itemPath, issues);
        requireOptionalString(node, "text", itemPath, issues);
        requireOptionalString(node, "operator", itemPath, issues);
        requireOptionalString(node, "expression", itemPath, issues);
        requireOptionalString(node, "keyExpression", itemPath, issues);
        requireOptionalArray(node, "wordPlans", itemPath, issues);
        requireOptionalArray(node, "reads", itemPath, issues);
        requireOptionalArray(node, "writes", itemPath, issues);
        requireOptionalBool(node, "renderRoot", itemPath, issues);
        if (node.contains("expressionPlan")) {
            validateClientExpressionPlan(node.at("expressionPlan"),
                                         itemPath + ".expressionPlan",
                                         issues);
        }
        if (node.contains("keyExpressionPlan")) {
            validateClientExpressionPlan(node.at("keyExpressionPlan"),
                                         itemPath + ".keyExpressionPlan",
                                         issues);
        }
        if (node.contains("wordPlans")) {
            validateClientExpressionPlanArray(node.at("wordPlans"),
                                              itemPath + ".wordPlans",
                                              issues);
        }
        if (node.contains("loopPlan")) {
            validateClientLoopPlan(node.at("loopPlan"),
                                   itemPath + ".loopPlan",
                                   issues);
        }
        if (node.contains("argPlans")) {
            validateClientExpressionPlanArray(node.at("argPlans"),
                                              itemPath + ".argPlans",
                                              issues);
        }
    }
}

void validateClientRoutes(const nlohmann::json& routes,
                          const std::string& path,
                          std::vector<std::string>& issues) {
    if (!requireArrayAt(routes, path, issues)) return;
    for (size_t i = 0; i < routes.size(); ++i) {
        const std::string itemPath = path + "[" + std::to_string(i) + "]";
        if (!requireObjectAt(routes[i], itemPath, issues)) continue;
        rejectUnknownKeys(routes[i], {"path", "name", "component", "params", "load", "loads"},
                          itemPath, issues);
        requireOptionalString(routes[i], "path", itemPath, issues);
        requireOptionalString(routes[i], "name", itemPath, issues);
        requireOptionalString(routes[i], "component", itemPath, issues);
        requireOptionalArray(routes[i], "params", itemPath, issues);
        requireOptionalArray(routes[i], "load", itemPath, issues);
        requireOptionalArray(routes[i], "loads", itemPath, issues);
    }
}

void validateClientFetches(const nlohmann::json& fetches,
                           const std::string& path,
                           std::vector<std::string>& issues) {
    if (!requireArrayAt(fetches, path, issues)) return;
    for (size_t i = 0; i < fetches.size(); ++i) {
        const std::string itemPath = path + "[" + std::to_string(i) + "]";
        if (!requireObjectAt(fetches[i], itemPath, issues)) continue;
        rejectUnknownKeys(fetches[i], {"name", "url", "method", "bodyExpr", "refreshAction",
                                       "cache", "credentials", "timeoutMs", "retryCount",
                                       "stalePolicy", "group", "cacheKeyExpr", "revalidateMs",
                                       "dedupe", "background", "lazy"},
                          itemPath, issues);
        for (const auto& key : {"name", "url", "method", "bodyExpr", "refreshAction", "cache",
                                "credentials", "timeoutMs", "retryCount", "stalePolicy",
                                "group", "cacheKeyExpr", "revalidateMs"}) {
            requireOptionalString(fetches[i], key, itemPath, issues);
        }
        requireOptionalBool(fetches[i], "dedupe", itemPath, issues);
        requireOptionalBool(fetches[i], "background", itemPath, issues);
        requireOptionalBool(fetches[i], "lazy", itemPath, issues);
    }
}

void validateClientActions(const nlohmann::json& actions,
                           const std::string& path,
                           std::vector<std::string>& issues) {
    if (!requireObjectAt(actions, path, issues)) return;
    for (auto it = actions.begin(); it != actions.end(); ++it) {
        const std::string itemPath = path + "." + it.key();
        if (!requireObjectAt(it.value(), itemPath, issues)) continue;
        rejectUnknownKeys(it.value(), {"params", "body"}, itemPath, issues);
        requireOptionalArray(it.value(), "params", itemPath, issues);
        requireOptionalArray(it.value(), "body", itemPath, issues);
    }
}

void validateClientComponentDefinitions(const nlohmann::json& definitions,
                                        const std::string& path,
                                        std::vector<std::string>& issues) {
    if (!requireArrayAt(definitions, path, issues)) return;
    for (size_t i = 0; i < definitions.size(); ++i) {
        const std::string itemPath = path + "[" + std::to_string(i) + "]";
        const auto& definition = definitions[i];
        if (!requireObjectAt(definition, itemPath, issues)) continue;
        rejectUnknownKeys(definition, {"moduleId", "name", "params", "emits", "emitArity",
                                       "emitPayloads", "emitPayloadTypes", "localState",
                                       "localDerived", "localActions", "localEffects",
                                       "eventBindings", "bodyPlan", "hasSlot", "bodyNodeCount",
                                       "rootTemplateNodeCount", "slotCount"},
                          itemPath, issues);
        requireOptionalModuleId(definition, "moduleId", itemPath, issues);
        if (!definition.contains("name") || !definition.at("name").is_string()) {
            issues.push_back(itemPath + ".name must be a string");
        }
        for (const auto& key : {"params", "emits", "localState", "localDerived", "localActions",
                                "localEffects", "eventBindings"}) {
            requireOptionalArray(definition, key, itemPath, issues);
        }
        requireOptionalObject(definition, "emitArity", itemPath, issues);
        requireOptionalObject(definition, "emitPayloads", itemPath, issues);
        requireOptionalObject(definition, "emitPayloadTypes", itemPath, issues);
        if (definition.contains("bodyPlan")) {
            validateClientBodyPlanArray(definition.at("bodyPlan"), itemPath + ".bodyPlan", issues);
        }
        requireOptionalBool(definition, "hasSlot", itemPath, issues);
        requireOptionalNumber(definition, "bodyNodeCount", itemPath, issues);
        requireOptionalNumber(definition, "rootTemplateNodeCount", itemPath, issues);
        requireOptionalNumber(definition, "slotCount", itemPath, issues);
    }
}

void validateClientComponentInstances(const nlohmann::json& instances,
                                      const std::string& path,
                                      std::vector<std::string>& issues) {
    if (!requireArrayAt(instances, path, issues)) return;
    for (size_t i = 0; i < instances.size(); ++i) {
        const std::string itemPath = path + "[" + std::to_string(i) + "]";
        const auto& instance = instances[i];
        if (!requireObjectAt(instance, itemPath, issues)) continue;
        rejectUnknownKeys(instance, {"moduleId", "definitionModule", "id", "component",
                                     "instanceId", "role", "params", "argumentPlans", "inputActions", "locals", "slotPlan"},
                          itemPath, issues);
        requireOptionalModuleId(instance, "moduleId", itemPath, issues);
        requireOptionalModuleId(instance, "definitionModule", itemPath, issues);
        requireOptionalString(instance, "id", itemPath, issues);
        if (!instance.contains("component") || !instance.at("component").is_string()) {
            issues.push_back(itemPath + ".component must be a string");
        }
        requireOptionalNumber(instance, "instanceId", itemPath, issues);
        requireOptionalString(instance, "role", itemPath, issues);
        requireOptionalObject(instance, "params", itemPath, issues);
        requireOptionalObject(instance, "argumentPlans", itemPath, issues);
        if (instance.contains("argumentPlans") && instance["argumentPlans"].is_object()) {
            for (const auto& argument : instance["argumentPlans"].items())
                validateClientExpressionPlan(argument.value(), itemPath + ".argumentPlans." + argument.key(), issues);
        }
        requireOptionalObject(instance, "inputActions", itemPath, issues);
        if (instance.contains("inputActions") && instance["inputActions"].is_object()) {
            for (const auto& action : instance["inputActions"].items())
                if (!action.value().is_string() || action.value().get<std::string>().rfind("__jtml_input_", 0) != 0)
                    issues.push_back(itemPath + ".inputActions must contain compiler-generated action identities");
        }
        requireOptionalObject(instance, "locals", itemPath, issues);
        if (instance.contains("slotPlan")) {
            validateClientBodyPlanArray(instance.at("slotPlan"), itemPath + ".slotPlan", issues);
        }
    }
}

void validateClientProjectSchema(const nlohmann::json& project,
                                 const std::string& path,
                                 std::vector<std::string>& issues) {
    if (!requireObjectAt(project, path, issues)) return;
    rejectUnknownKeys(project, {"sourceOfTruth", "entry", "linkedPlan", "modules"},
                      path, issues);
    requireOptionalString(project, "sourceOfTruth", path, issues);
    requireOptionalModuleId(project, "entry", path, issues);
    if (project.contains("linkedPlan")) {
        validateClientRuntimePlanSchema(project.at("linkedPlan"), path + ".linkedPlan", false, issues);
    }
    if (project.contains("modules")) {
        if (requireArrayAt(project.at("modules"), path + ".modules", issues)) {
            const auto& modules = project.at("modules");
            for (size_t i = 0; i < modules.size(); ++i) {
                const std::string itemPath = path + ".modules[" + std::to_string(i) + "]";
                if (!requireObjectAt(modules[i], itemPath, issues)) continue;
                rejectUnknownKeys(modules[i], {"id", "executable", "plan"}, itemPath, issues);
                requireOptionalModuleId(modules[i], "id", itemPath, issues);
                requireOptionalBool(modules[i], "executable", itemPath, issues);
                if (modules[i].contains("plan")) {
                    validateClientRuntimePlanSchema(modules[i].at("plan"), itemPath + ".plan", false, issues);
                }
            }
        }
    }
}

void validateClientRuntimePlanSchema(const nlohmann::json& manifest,
                                     const std::string& path,
                                     bool allowProject,
                                     std::vector<std::string>& issues) {
    static const std::vector<std::string> forbiddenKeys = {
        "bodySource",
        "bodyHex",
        "parseMetadata",
        "semanticSummary",
        "semantic",
        "sourcePath",
        "sourceSpan",
    };
    if (!manifest.is_object()) {
        issues.push_back(path + " must be a JSON object");
        return;
    }
    auto allowedTopLevel = std::vector<std::string>{
        "state", "statePlans", "derived", "derivedPlans", "actions", "subscriptions", "routes", "fetches",
        "componentDefinitions", "componentInstances"
    };
    if (allowProject) allowedTopLevel.push_back("project");
    rejectUnknownKeys(manifest, allowedTopLevel, path, issues);
    for (const auto& required : {"state", "derived", "actions", "routes", "fetches",
                                "componentDefinitions", "componentInstances"}) {
        if (!manifest.contains(required)) {
            issues.push_back(path + " missing `" + required + "`");
        }
    }
    auto requireObject = [&](const char* key) {
        if (manifest.contains(key) && !manifest.at(key).is_object()) {
            issues.push_back(path + "." + key + " must be an object");
        }
    };
    auto requireArray = [&](const char* key) {
        if (manifest.contains(key) && !manifest.at(key).is_array()) {
            issues.push_back(path + "." + key + " must be an array");
        }
    };
    requireObject("state");
    requireObject("statePlans");
    requireObject("derived");
    requireObject("derivedPlans");
    requireObject("actions");

    if (manifest.contains("subscriptions")) {
        const auto& subscriptions = manifest.at("subscriptions");
        if (requireArrayAt(subscriptions, path + ".subscriptions", issues)) {
            if (subscriptions.size() > 64) issues.push_back(path + ".subscriptions exceeds 64 entries");
            std::set<std::pair<std::string, std::string>> identities;
            for (const auto& subscription : subscriptions) {
                if (!requireObjectAt(subscription, path + ".subscriptions[]", issues)) continue;
                rejectUnknownKeys(subscription, {"action", "dependency", "moduleId"}, path + ".subscriptions[]", issues);
                if (!subscription.contains("moduleId")) issues.push_back(path + ".subscriptions requires moduleId");
                requireOptionalModuleId(subscription, "moduleId", path + ".subscriptions[]", issues);
                if (!subscription.contains("action") || !subscription["action"].is_string() ||
                    !subscription.contains("dependency") || !subscription["dependency"].is_string()) {
                    issues.push_back(path + ".subscriptions requires string action/dependency"); continue;
                }
                const auto action = subscription["action"].get<std::string>();
                const auto dependency = subscription["dependency"].get<std::string>();
                if (!manifest.value("actions", nlohmann::json::object()).contains(action) ||
                    !manifest.value("state", nlohmann::json::object()).contains(dependency))
                    issues.push_back(path + ".subscriptions references missing action/state");
                else if (!manifest["actions"][action].value("params", nlohmann::json::array()).empty())
                    issues.push_back(path + ".subscriptions requires zero-argument callbacks");
                if (!identities.emplace(action, dependency).second)
                    issues.push_back(path + ".subscriptions contains duplicate binding");
                if (manifest.contains("project") && manifest["project"].is_object() &&
                    manifest["project"].contains("modules") && manifest["project"]["modules"].is_array()) {
                    const auto& modules = manifest["project"]["modules"];
                    const auto owner = std::find_if(modules.begin(), modules.end(), [&](const auto& module) {
                        return module.is_object() && module.contains("id") && subscription.contains("moduleId") &&
                               module["id"] == subscription["moduleId"];
                    });
                    if (owner == modules.end() || !owner->contains("plan") || !(*owner)["plan"].is_object() ||
                        !(*owner)["plan"].value("state", nlohmann::json::object()).contains(dependency) ||
                        !(*owner)["plan"].value("actions", nlohmann::json::object()).contains(action))
                        issues.push_back(path + ".subscriptions must reference state/action in the owning module");
                }
            }
        }
    }
    if (manifest.contains("routes")) validateClientRoutes(manifest.at("routes"), path + ".routes", issues);
    if (manifest.contains("fetches")) validateClientFetches(manifest.at("fetches"), path + ".fetches", issues);
    if (manifest.contains("actions")) validateClientActions(manifest.at("actions"), path + ".actions", issues);
    if (manifest.contains("componentDefinitions")) {
        validateClientComponentDefinitions(manifest.at("componentDefinitions"),
                                           path + ".componentDefinitions",
                                           issues);
    }
    if (manifest.contains("componentInstances")) {
        validateClientComponentInstances(manifest.at("componentInstances"),
                                         path + ".componentInstances",
                                         issues);
    }
    // A capability is a compiler-issued action for this instance, with an
    // existing one-argument assignment and the matching compiled read path.
    if (manifest.contains("componentInstances") && manifest["componentInstances"].is_array()) {
        for (const auto& instance : manifest["componentInstances"]) {
            if (!instance.is_object() || !instance.contains("inputActions") || !instance["inputActions"].is_object()) continue;
            for (const auto& input : instance["inputActions"].items()) {
                const auto invalid = [&] { issues.push_back(path + ".inputActions has inconsistent compiler authority"); };
                if (!input.value().is_string() || !instance.contains("moduleId") || !instance["moduleId"].is_number_unsigned() ||
                    !instance.contains("id") || !instance["id"].is_string()) { invalid(); continue; }
                const auto actionName = input.value().get<std::string>();
                const auto stem = "__jtml_input_" + std::to_string(instance["moduleId"].get<unsigned>()) + "_" + instance["id"].get<std::string>() + "_" + input.key();
                const auto suffix = actionName.size() > stem.size() ? actionName.substr(stem.size()) : std::string();
                if (actionName != stem && (actionName.rfind(stem + "_", 0) != 0 || suffix.size() < 2 ||
                    !std::all_of(suffix.begin() + 1, suffix.end(), [](unsigned char c) { return std::isdigit(c); }))) { invalid(); continue; }
                if (!manifest.contains("actions") || !manifest["actions"].is_object() || !manifest["actions"].contains(actionName) ||
                    !instance.contains("params") || !instance["params"].is_object() || !instance["params"].contains(input.key()) ||
                    !instance.contains("argumentPlans") || !instance["argumentPlans"].is_object() || !instance["argumentPlans"].contains(input.key())) { invalid(); continue; }
                const auto& action = manifest["actions"][actionName];
                const auto& read = instance["argumentPlans"][input.key()];
                if (!action.is_object() || !action.contains("params") || !action["params"].is_array() || action["params"].size() != 1 || !action["params"][0].is_string() ||
                    !action.contains("body") || !action["body"].is_array() || action["body"].size() != 1 || !action["body"][0].is_object() ||
                    !read.is_object() || read.value("kind", "") != "path" || !read.contains("root") || !read["root"].is_string()) { invalid(); continue; }
                const auto& write = action["body"][0];
                if (write.value("kind", "") != "assign" || !write.contains("lhs") || write["lhs"] != read["root"] ||
                    !write.contains("expr") || write["expr"] != action["params"][0] || write["lhs"] == action["params"][0]) { invalid(); continue; }
                if (manifest.contains("project") && manifest["project"].is_object()) {
                    const auto& project = manifest["project"];
                    bool owned = false;
                    if (project.contains("entry") && project["entry"] == instance["moduleId"] && project.contains("modules") && project["modules"].is_array())
                        for (const auto& module : project["modules"]) {
                            if (!module.is_object() || !module.contains("id") || module["id"] != instance["moduleId"] || !module.contains("plan") || !module["plan"].is_object()) continue;
                            const auto& plan = module["plan"];
                            owned = plan.contains("state") && plan["state"].is_object() && plan["state"].contains(read["root"].get<std::string>()) &&
                                plan.contains("actions") && plan["actions"].is_object() && plan["actions"].contains(actionName) && plan["actions"][actionName] == action;
                        }
                    if (!owned) invalid();
                }
            }
        }
    }
    if (manifest.contains("project")) {
        if (allowProject) {
            validateClientProjectSchema(manifest.at("project"), path + ".project", issues);
        } else {
            issues.push_back(path + " must not contain nested `project`");
        }
    }
    for (const auto& key : forbiddenKeys) {
        rejectJsonKeyRecursive(manifest, key, path, issues);
    }
}

void validateClientManifestSchema(const nlohmann::json& manifest,
                                  std::vector<std::string>& issues) {
    validateClientRuntimePlanSchema(manifest, "$", true, issues);
}

} // namespace

void validateBrowserBuildOutputSchema(const std::filesystem::path& outDir,
                                      BrowserBuildMode mode) {
    std::vector<std::string> issues;
    const auto indexPath = outDir / "index.html";
    const auto runtimePath = outDir / "jtml-runtime.js";
    const auto componentPath = outDir / "components" / "index.js";
    const auto appPath = outDir / "app.js";
    const auto legacyPath = outDir / "jtml-update-plans.js";

    requireExists(indexPath, issues);
    requireExists(runtimePath, issues);
    requireExists(componentPath, issues);
    requireExists(appPath, issues);
    if (browserBuildModeEmitsCompatibilityAsset(mode)) {
        requireExists(legacyPath, issues);
    } else if (std::filesystem::exists(legacyPath)) {
        issues.push_back("clean production must not emit " + legacyPath.string());
    }
    if (!issues.empty()) {
        std::ostringstream out;
        out << "browser build output schema failed";
        for (const auto& issue : issues) out << "\n- " << issue;
        throw std::runtime_error(out.str());
    }

    const auto index = readAsset(indexPath);
    const auto runtime = readAsset(runtimePath);
    const auto component = readAsset(componentPath);
    const auto app = readAsset(appPath);

    requireContains(index, "<script src=\"./jtml-runtime.js\" defer></script>",
                    "index.html", issues);
    requireContains(index, "<script src=\"./components/index.js\" defer></script>",
                    "index.html", issues);
    requireContains(index, "<script src=\"./app.js\" defer></script>",
                    "index.html", issues);
    validateClientManifestSchema(extractClientManifestJson(index, issues), issues);

    requireContains(app, "window.jtml.browserBuild = {", "app.js", issues);
    requireContains(app, std::string("mode: '") + browserBuildModeName(mode) + "'",
                    "app.js", issues);
    requireContains(app, "productionMode = window.jtml.browserBuild.mode",
                    "app.js", issues);
    requireContains(component, "\"assetRole\":\"component-module\"",
                    "components/index.js", issues);
    requireContains(component, "window.__jtml_static_component_plan_index = plans",
                    "components/index.js", issues);

    rejectDynamicCompilation(runtime, "jtml-runtime.js", issues);
    rejectDynamicCompilation(component, "components/index.js", issues);
    rejectDynamicCompilation(app, "app.js", issues);
    rejectContains(runtime, "<script", "jtml-runtime.js", issues);
    rejectContains(runtime, "</script>", "jtml-runtime.js", issues);

    rejectContains(component, "window.__jtml_static_update_plans = plans",
                   "components/index.js", issues);
    rejectContains(component, "\"bodyPlan\"", "components/index.js", issues);
    rejectContains(component, "\"bodySource\"", "components/index.js", issues);
    rejectContains(component, "\"bodyHex\"", "components/index.js", issues);
    rejectContains(component, "\"sourceLine\"", "components/index.js", issues);
    rejectContains(component, "\"sourceColumn\"", "components/index.js", issues);

    if (mode == BrowserBuildMode::Production) {
        requireContains(app, "cleanProduction: true", "app.js", issues);
        requireContains(app, "compatibility: false", "app.js", issues);
        requireContains(app, "debug: false", "app.js", issues);
        rejectContains(index, "jtml-update-plans.js", "index.html", issues);
        rejectContains(app, "compatibilityUpdatePlans", "app.js", issues);
        rejectContains(app, "jtml-update-plans.js", "app.js", issues);
    } else {
        requireContains(app, "cleanProduction: false", "app.js", issues);
        requireContains(app, "compatibility: true", "app.js", issues);
        requireContains(app, "compatibilityUpdatePlans: './jtml-update-plans.js'",
                        "app.js", issues);
        const auto legacy = readAsset(legacyPath);
        requireContains(legacy, "\"assetRole\":\"legacy-update-plan\"",
                        "jtml-update-plans.js", issues);
        requireContains(legacy, "window.__jtml_static_update_plans",
                        "jtml-update-plans.js", issues);
        rejectDynamicCompilation(legacy, "jtml-update-plans.js", issues);
        if (mode == BrowserBuildMode::Debug) {
            requireContains(app, "debug: true", "app.js", issues);
            requireContains(app, "sourceSpans: true", "app.js", issues);
            requireContains(app, "fallbackTelemetry: true", "app.js", issues);
        } else {
            requireContains(app, "debug: false", "app.js", issues);
        }
    }

    if (!issues.empty()) {
        std::ostringstream out;
        out << "browser build output schema failed";
        for (const auto& issue : issues) out << "\n- " << issue;
        throw std::runtime_error(out.str());
    }
}

} // namespace jtml::cli
