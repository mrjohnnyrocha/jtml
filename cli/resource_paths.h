#pragma once

#include <filesystem>

namespace jtml::cli {

struct ResourcePaths {
    std::filesystem::path examples;
    std::filesystem::path tutorial;
    std::filesystem::path studio;
    std::filesystem::path docs;
};

// Installed resources take precedence over caller-controlled working directories.
// No build-time source path is embedded in an installed executable.
ResourcePaths resourcePaths();
ResourcePaths resourcePathsForExecutable(const std::filesystem::path& executable);

} // namespace jtml::cli
