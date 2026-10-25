#include "resource_paths.h"

#include <system_error>
#include <vector>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#ifndef JTML_DATA_FROM_BINDIR
#define JTML_DATA_FROM_BINDIR "../share/jtml"
#endif
#ifndef JTML_DOCS_FROM_BINDIR
#define JTML_DOCS_FROM_BINDIR "../share/doc/jtml/docs"
#endif

namespace jtml::cli {
namespace {
namespace fs = std::filesystem;

fs::path executablePath() {
#if defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size);
    if (_NSGetExecutablePath(buffer.data(), &size) == 0) return fs::canonical(buffer.data());
#elif defined(__linux__)
    std::error_code error;
    const auto path = fs::read_symlink("/proc/self/exe", error);
    if (!error) return path;
#endif
    return {};
}

ResourcePaths bundle(const fs::path& root) {
    return {root / "examples", root / "tutorial", root / "studio", root / "docs"};
}
} // namespace

ResourcePaths resourcePathsForExecutable(const std::filesystem::path& executable) {
    if (!executable.empty()) {
        const auto bin = fs::weakly_canonical(executable).parent_path();
        const auto data = (bin / JTML_DATA_FROM_BINDIR).lexically_normal();
        if (fs::is_directory(data)) {
            return {data / "examples", data / "tutorial", data / "studio",
                    (bin / JTML_DOCS_FROM_BINDIR).lexically_normal()};
        }
        // Archive bin/jtml and source build/jtml share this bundle layout.
        const auto root = bin.parent_path();
        if (fs::is_directory(root / "examples") || fs::is_directory(root / "studio")) {
            return bundle(root);
        }
    }
    // Preserve explicitly cwd-based source workflows on unsupported platforms
    // or when running a standalone development binary without bundled data.
    return bundle(fs::current_path());
}

ResourcePaths resourcePaths() {
    return resourcePathsForExecutable(executablePath());
}
} // namespace jtml::cli
