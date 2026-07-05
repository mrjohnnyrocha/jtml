#include "browser_build_assets.h"

#include "browser_build_policy.h"

#include <filesystem>
#include <iostream>

namespace jtml::cli {
namespace {

bool pathIsInside(const std::filesystem::path& path, const std::filesystem::path& parent) {
    auto rel = std::filesystem::relative(path, parent);
    return rel.empty() || (rel.native().rfind("..", 0) != 0 && rel != ".");
}

} // namespace

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
