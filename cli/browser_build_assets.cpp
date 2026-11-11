#include "browser_build_assets.h"

#include "browser_build_policy.h"
#include "jtml/runtime_plan.h"
#include "json.hpp"

#include <filesystem>
#include <iostream>

namespace jtml::cli {
namespace {

bool pathIsInside(const std::filesystem::path& path, const std::filesystem::path& parent) {
    auto rel = std::filesystem::relative(path, parent);
    return rel.empty() || (rel.native().rfind("..", 0) != 0 && rel != ".");
}

} // namespace

std::string emitBrowserFaultSites(const RuntimeProjectPlan& plan,
                                  const std::filesystem::path& input) {
    auto sites = nlohmann::json::object();
    const auto sourceRoot = std::filesystem::absolute(input).lexically_normal().parent_path();
    auto addSite = [&](const std::string& key, const SourceSpan& span) {
        if (!span.original || !span.original->source || span.original->source->file.empty()) return;
        const auto& origin = *span.original;
        const auto file = std::filesystem::absolute(origin.source->file).lexically_normal()
            .lexically_relative(sourceRoot);
        if (file.empty()) return;
        const auto [line, column] = origin.source->coordinates(origin.begin);
        sites[key] = file.generic_string() + ":" + std::to_string(line) + ":" + std::to_string(column);
    };
    const auto& subscriptions = plan.linkedPlan.subscriptions;
    for (size_t i = 0; i < subscriptions.size(); ++i) {
        addSite("effect:" + std::to_string(i), subscriptions[i].sourceSpan);
    }
    auto addComponents = [&](const RuntimePlan& runtime) {
        for (const auto& definition : runtime.componentDefinitions) {
            const auto module = definition.moduleId == InvalidSemanticModuleId
                ? "global" : std::to_string(definition.moduleId);
            for (size_t i = 0; i < definition.bodyPlan.size(); ++i) {
                const auto& node = definition.bodyPlan[i];
                if (!node.keyExpression.empty()) {
                    addSite("component:" + module + ":" + definition.name + ":" + std::to_string(i), node.sourceSpan);
                }
            }
        }
    };
    addComponents(plan.linkedPlan);
    for (const auto& module : plan.modules) {
        if (module.clientExecutable) addComponents(module.plan);
    }
    if (sites.empty()) return {};

    // Keep arbitrary source filenames as data, including spellings reserved
    // by the generated-asset contract. Decode the UTF-8 JSON in the bootstrap.
    constexpr char hex[] = "0123456789abcdef";
    std::string encoded;
    const auto json = sites.dump();
    encoded.reserve(json.size() * 3);
    for (unsigned char byte : json) {
        encoded += '%';
        encoded += hex[byte >> 4];
        encoded += hex[byte & 15];
    }
    return "window.__jtml_fault_sites = JSON.parse(decodeURIComponent('" + encoded + "'));\n";
}

int copyBrowserBuildAssets(const std::filesystem::path& inputDir,
                           const std::filesystem::path& outDir) {
    if (!std::filesystem::is_directory(inputDir)) return 0;

    const auto absInput = std::filesystem::weakly_canonical(inputDir);
    const auto absOut = std::filesystem::absolute(outDir).lexically_normal();
    int copied = 0;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(absInput)) {
        const auto src = entry.path();
        if (pathIsInside(src, absOut)) continue;
        if (entry.is_directory()) continue;
        if (entry.is_symlink()) continue;
        if (!entry.is_regular_file()) continue;
        if (src.extension() == ".jtml") continue;

        auto rel = std::filesystem::relative(src, absInput);
        if (!shouldCopyBrowserBuildAsset(rel)) continue;
        auto dest = outDir / rel;
        std::filesystem::create_directories(dest.parent_path());
        std::filesystem::copy_file(src, dest, std::filesystem::copy_options::overwrite_existing);
        ++copied;
    }

    if (copied > 0) {
        std::cout << "Copied " << copied << " asset(s) from " << inputDir.string() << "\n";
    }
    return copied;
}

} // namespace jtml::cli
