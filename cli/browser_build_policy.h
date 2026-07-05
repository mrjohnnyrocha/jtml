// cli/browser_build_policy.h
//
// Browser build policy owns mode selection and public asset-copy rules. Keep
// these decisions outside browser_build.cpp so the build service does not grow
// into another command monolith.
#pragma once

#include "util.h"

#include <filesystem>
#include <string>

namespace jtml::cli {

enum class BrowserBuildMode {
    Production,
    Compatibility,
    Debug,
};

BrowserBuildMode browserBuildModeFromOptions(const Options& o);
const char* browserBuildModeName(BrowserBuildMode mode);
bool browserBuildModeEmitsCompatibilityAsset(BrowserBuildMode mode);
bool browserBuildModeAllowsImportStubs(BrowserBuildMode mode);
std::string emitBrowserBuildAppBootstrap(BrowserBuildMode mode);

bool shouldCopyBrowserBuildAsset(const std::filesystem::path& rel);
int copyBrowserBuildAssets(const std::filesystem::path& inputDir,
                           const std::filesystem::path& outDir);

} // namespace jtml::cli
