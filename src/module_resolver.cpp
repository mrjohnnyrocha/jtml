#include "jtml/module_resolver.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace jtml {

namespace {

std::filesystem::path normalizePath(const std::filesystem::path& path) {
    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(path, ec);
    if (!ec && !canonical.empty()) return canonical;
    return std::filesystem::absolute(path).lexically_normal();
}

std::filesystem::path importerDirectory(const std::filesystem::path& importerPath) {
    if (importerPath.empty()) return std::filesystem::current_path();
    if (std::filesystem::is_directory(importerPath)) return importerPath;
    auto parent = importerPath.parent_path();
    if (!parent.empty()) return parent;
    return std::filesystem::current_path();
}

std::filesystem::path packageEntryFor(const std::filesystem::path& candidate) {
    if (std::filesystem::is_regular_file(candidate)) return candidate;
    if (!std::filesystem::is_directory(candidate)) return {};

    const std::filesystem::path index = candidate / "index.jtml";
    if (std::filesystem::is_regular_file(index)) return index;

    const std::filesystem::path package = candidate / "package.jtml";
    if (std::filesystem::is_regular_file(package)) return package;

    return {};
}

} // namespace

bool isBareModuleSpecifier(const std::string& specifier) {
    if (specifier.empty()) return false;
    if (specifier.rfind("./", 0) == 0 || specifier.rfind("../", 0) == 0) return false;
    if (specifier.front() == '/' || specifier.front() == '\\') return false;
    if (specifier.find(':') != std::string::npos) return false;
    return true;
}

std::filesystem::path resolveJtmlModulePath(
    const std::string& specifier,
    const std::filesystem::path& importerPath) {
    namespace fs = std::filesystem;
    const fs::path importerDir = importerDirectory(importerPath);

    if (!isBareModuleSpecifier(specifier)) {
        return normalizePath(importerDir / fs::path(specifier));
    }

    for (fs::path cursor = normalizePath(importerDir); !cursor.empty();) {
        fs::path candidate = cursor / "jtml_modules" / specifier;
        fs::path entry = packageEntryFor(candidate);
        if (!entry.empty()) return normalizePath(entry);

        fs::path parent = cursor.parent_path();
        if (parent == cursor) break;
        cursor = parent;
    }

    fs::path cwdCandidate = fs::current_path() / "jtml_modules" / specifier;
    fs::path cwdEntry = packageEntryFor(cwdCandidate);
    if (!cwdEntry.empty()) return normalizePath(cwdEntry);

    return normalizePath(importerDir / fs::path(specifier));
}

} // namespace jtml
