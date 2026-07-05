#include "browser_build_schema.h"

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
