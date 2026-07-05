// cli/browser_build_assets.h
//
// Filesystem asset copying for browser builds. Policy decides which relative
// paths are public; this module only executes that policy safely.
#pragma once

#include <filesystem>

namespace jtml::cli {

int copyBrowserBuildAssets(const std::filesystem::path& inputDir,
                           const std::filesystem::path& outDir);

} // namespace jtml::cli
