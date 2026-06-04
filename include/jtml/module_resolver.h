#pragma once

#include <filesystem>
#include <string>

namespace jtml {

bool isBareModuleSpecifier(const std::string& specifier);

std::filesystem::path resolveJtmlModulePath(
    const std::string& specifier,
    const std::filesystem::path& importerPath = {});

} // namespace jtml
