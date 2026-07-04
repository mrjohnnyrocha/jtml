#include "cli/browser_build.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream input(path);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

bool existsFile(const std::filesystem::path& path) {
    return std::filesystem::is_regular_file(path);
}

} // namespace

int main() {
    try {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto root = std::filesystem::temp_directory_path() /
            ("jtml-browser-build-linkcheck-" + std::to_string(stamp));
        std::filesystem::create_directories(root);
        const auto input = root / "input.jtml";
        const auto output = root / "out.html";
        const auto outDir = root / "dist";
        const auto prodDir = root / "dist-prod";

        {
            std::ofstream out(input);
            out << "jtml 2\n"
                << "let count = 0\n"
                << "when add\n"
                << "  count += 1\n"
                << "page\n"
                << "  text count\n"
                << "  button \"Add\" click add\n";
        }

        jtml::cli::Options options;
        options.inputFile = input.string();
        options.outputFile = output.string();
        options.target = "browser";
        options.syntax = jtml::SyntaxMode::Auto;

        const int result = jtml::cli::cmdTranspileBrowser(options);
        if (result != 0) {
            std::cerr << "cmdTranspileBrowser returned " << result << "\n";
            return result;
        }
        const auto html = readFile(output);
        if (!contains(html, "__jtml_client_manifest") ||
            !contains(html, "const browserLocalRuntime = true;") ||
            contains(html, "<script src=\"./jtml-runtime.js\" defer></script>")) {
            std::cerr << "browser build linkcheck did not produce expected inline browser transpile output\n";
            return 2;
        }

        jtml::cli::Options buildOptions;
        buildOptions.inputFile = input.string();
        buildOptions.outputFile = outDir.string();
        buildOptions.target = "browser";
        buildOptions.syntax = jtml::SyntaxMode::Auto;
        const int buildResult = jtml::cli::cmdBuildBrowser(buildOptions);
        if (buildResult != 0) {
            std::cerr << "cmdBuildBrowser returned " << buildResult << "\n";
            return buildResult;
        }

        const auto index = outDir / "index.html";
        const auto runtime = outDir / "jtml-runtime.js";
        const auto components = outDir / "components" / "index.js";
        const auto app = outDir / "app.js";
        const auto legacyPlans = outDir / "jtml-update-plans.js";
        if (!existsFile(index) || !existsFile(runtime) || !existsFile(components) ||
            !existsFile(app) || !existsFile(legacyPlans)) {
            std::cerr << "cmdBuildBrowser did not emit the expected split browser assets\n";
            return 3;
        }
        const auto indexHtml = readFile(index);
        const auto runtimeJs = readFile(runtime);
        const auto componentJs = readFile(components);
        const auto appJs = readFile(app);
        const auto legacyJs = readFile(legacyPlans);
        if (!contains(indexHtml, "<script src=\"./jtml-runtime.js\" defer></script>") ||
            !contains(indexHtml, "<script src=\"./components/index.js\" defer></script>") ||
            !contains(indexHtml, "<script src=\"./app.js\" defer></script>")) {
            std::cerr << "cmdBuildBrowser index.html missing split production asset tags\n";
            return 4;
        }
        if (!contains(runtimeJs, "const browserLocalRuntime = true;") ||
            contains(runtimeJs, "new Function") ||
            contains(runtimeJs, "eval(")) {
            std::cerr << "cmdBuildBrowser emitted unsafe or non-browser-local runtime asset\n";
            return 5;
        }
        if (!contains(componentJs, "__jtml_static_component_modules") ||
            !contains(componentJs, "\"assetRole\":\"component-module\"") ||
            contains(componentJs, "__jtml_static_update_plans = plans") ||
            contains(componentJs, "\"bodyPlan\"") ||
            contains(componentJs, "\"rootCreateOperations\"") ||
            contains(componentJs, "\"sourceLine\"") ||
            contains(componentJs, "\"sourceColumn\"")) {
            std::cerr << "cmdBuildBrowser component module asset leaked legacy/debug metadata\n";
            return 6;
        }
        if (!contains(legacyJs, "__jtml_static_update_plans = plans") ||
            !contains(legacyJs, "\"assetRole\":\"legacy-update-plan\"")) {
            std::cerr << "cmdBuildBrowser legacy update-plan asset lost explicit compatibility role\n";
            return 7;
        }
        if (!contains(appJs, "compatibilityUpdatePlans")) {
            std::cerr << "cmdBuildBrowser app bootstrap no longer reports compatibility update-plan asset explicitly\n";
            return 8;
        }

        jtml::cli::Options prodOptions = buildOptions;
        prodOptions.outputFile = prodDir.string();
        prodOptions.prod = true;
        const int prodResult = jtml::cli::cmdBuildBrowser(prodOptions);
        if (prodResult != 0) {
            std::cerr << "cmdBuildBrowser --prod returned " << prodResult << "\n";
            return prodResult;
        }
        const auto prodIndex = prodDir / "index.html";
        const auto prodRuntime = prodDir / "jtml-runtime.js";
        const auto prodComponents = prodDir / "components" / "index.js";
        const auto prodApp = prodDir / "app.js";
        const auto prodLegacyPlans = prodDir / "jtml-update-plans.js";
        if (!existsFile(prodIndex) || !existsFile(prodRuntime) ||
            !existsFile(prodComponents) || !existsFile(prodApp)) {
            std::cerr << "cmdBuildBrowser --prod did not emit required split browser assets\n";
            return 9;
        }
        if (existsFile(prodLegacyPlans)) {
            std::cerr << "cmdBuildBrowser --prod emitted legacy update plans by default\n";
            return 10;
        }
        const auto prodAppJs = readFile(prodApp);
        const auto prodHtml = readFile(prodIndex);
        if (contains(prodAppJs, "compatibilityUpdatePlans") ||
            contains(prodAppJs, "jtml-update-plans.js") ||
            contains(prodHtml, "jtml-update-plans.js")) {
            std::cerr << "cmdBuildBrowser --prod still references legacy compatibility update plans\n";
            return 11;
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "browser build linkcheck failed: " << e.what() << "\n";
        return 1;
    }
}
