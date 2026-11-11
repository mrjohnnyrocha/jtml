// cli/browser_build_assets.h
//
// Browser bootstrap metadata and filesystem asset copying. Copy eligibility
// is owned by the browser build policy.
#pragma once

#include <filesystem>
#include <string>

namespace jtml { struct RuntimeProjectPlan; }

namespace jtml::cli {

std::string emitBrowserFaultSites(const RuntimeProjectPlan& plan,
                                  const std::filesystem::path& input);

int copyBrowserBuildAssets(const std::filesystem::path& inputDir,
                           const std::filesystem::path& outDir);

} // namespace jtml::cli
