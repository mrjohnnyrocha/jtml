#pragma once

#include "commands.h"

#include "json.hpp"

#include <filesystem>
#include <string>

namespace jtml::cli {

std::string packageNameFromPath(const std::filesystem::path& source);

void installLocalPackage(const std::filesystem::path& source,
                         const std::string& packageName,
                         bool force,
                         bool jsonOutput,
                         bool quiet = false);

nlohmann::json verifyPackageLock(bool restoreFromManifest);

} // namespace jtml::cli
