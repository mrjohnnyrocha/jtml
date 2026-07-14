#include "browser_build_schema.h"

#include "json.hpp"

#include <fstream>
#include <iterator>
#include <sstream>
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

void validateClientManifestSchema(const nlohmann::json& manifest,
                                  std::vector<std::string>& issues) {
    static const std::vector<std::string> forbiddenKeys = {
        "bodySource",
        "bodyHex",
        "parseMetadata",
        "semanticSummary",
        "semantic",
        "sourcePath",
    };
    if (!manifest.is_object()) {
        issues.push_back("client manifest must be a JSON object");
        return;
    }
    for (const auto& required : {"state", "derived", "actions", "routes", "fetches",
                                "componentDefinitions", "componentInstances"}) {
        if (!manifest.contains(required)) {
            issues.push_back(std::string("client manifest missing `") + required + "`");
        }
    }
    auto requireObject = [&](const char* key) {
        if (manifest.contains(key) && !manifest.at(key).is_object()) {
            issues.push_back(std::string("client manifest `") + key + "` must be an object");
        }
    };
    auto requireArray = [&](const char* key) {
        if (manifest.contains(key) && !manifest.at(key).is_array()) {
            issues.push_back(std::string("client manifest `") + key + "` must be an array");
        }
    };
    requireObject("state");
    requireObject("derived");
    requireObject("actions");
    requireArray("routes");
    requireArray("fetches");
    requireArray("componentDefinitions");
    requireArray("componentInstances");

    if (manifest.contains("componentDefinitions") &&
        manifest.at("componentDefinitions").is_array()) {
        size_t index = 0;
        for (const auto& definition : manifest.at("componentDefinitions")) {
            const std::string path = "$.componentDefinitions[" + std::to_string(index) + "]";
            if (!definition.is_object()) {
                issues.push_back(path + " must be an object");
            } else {
                if (!definition.contains("name") || !definition.at("name").is_string()) {
                    issues.push_back(path + ".name must be a string");
                }
                if (definition.contains("bodyPlan") && !definition.at("bodyPlan").is_array()) {
                    issues.push_back(path + ".bodyPlan must be an array when present");
                }
                if (definition.contains("params") && !definition.at("params").is_array()) {
                    issues.push_back(path + ".params must be an array when present");
                }
            }
            ++index;
        }
    }
    if (manifest.contains("componentInstances") &&
        manifest.at("componentInstances").is_array()) {
        size_t index = 0;
        for (const auto& instance : manifest.at("componentInstances")) {
            const std::string path = "$.componentInstances[" + std::to_string(index) + "]";
            if (!instance.is_object()) {
                issues.push_back(path + " must be an object");
            } else {
                if (!instance.contains("component") || !instance.at("component").is_string()) {
                    issues.push_back(path + ".component must be a string");
                }
                if (instance.contains("params") && !instance.at("params").is_object()) {
                    issues.push_back(path + ".params must be an object when present");
                }
                if (instance.contains("slotPlan") && !instance.at("slotPlan").is_array()) {
                    issues.push_back(path + ".slotPlan must be an array when present");
                }
            }
            ++index;
        }
    }
    for (const auto& key : forbiddenKeys) {
        rejectJsonKeyRecursive(manifest, key, "$", issues);
    }
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
