// cli/browser_build_schema.h
//
// Browser build output schema validation. This is the mode-specific contract
// for emitted assets; tests can still inspect strings, but production builds
// should validate their own output before returning success.
#pragma once

#include "browser_build_policy.h"

#include <filesystem>

namespace jtml::cli {

void validateBrowserBuildOutputSchema(const std::filesystem::path& outDir,
                                      BrowserBuildMode mode);

} // namespace jtml::cli
