// cli/browser_build_policy.h
//
// Browser build policy owns mode selection and public asset eligibility. Keep
// these decisions outside browser_build.cpp so the build service does not grow
// into another command monolith.
#pragma once

#include "util.h"
#include "jtml/runtime_plan.h"

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

void validateBrowserRuntimeBindingOwnership(const jtml::RuntimeProjectPlan& project);

bool shouldCopyBrowserBuildAsset(const std::filesystem::path& rel);

} // namespace jtml::cli
