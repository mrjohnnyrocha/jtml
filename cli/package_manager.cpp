#include "package_manager.h"

#include "util.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

namespace jtml::cli {
namespace {

bool isValidPackageName(const std::string& name) {
    if (name.empty() || name == "." || name == "..") return false;
    for (char ch : name) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (std::isalnum(c) || ch == '-' || ch == '_' || ch == '.') continue;
        return false;
    }
    return true;
}

std::string fnv1aHex(const std::string& input) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (unsigned char ch : input) {
        hash ^= ch;
        hash *= 1099511628211ULL;
    }
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

nlohmann::json packageLockEntry(const std::filesystem::path& packageDir,
                                const std::filesystem::path& source,
                                const std::string& packageName) {
    namespace fs = std::filesystem;
    std::vector<fs::path> files;
    for (const auto& entry : fs::recursive_directory_iterator(packageDir)) {
        if (entry.is_regular_file()) files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());

    nlohmann::json fileEntries = nlohmann::json::array();
    std::string combined;
    for (const auto& file : files) {
        const fs::path rel = fs::relative(file, packageDir);
        const std::string content = readFile(file.string());
        const std::string fingerprint = fnv1aHex(content);
        combined += rel.generic_string() + "\n" + fingerprint + "\n";
        fileEntries.push_back({
            {"path", rel.generic_string()},
            {"size", static_cast<std::uintmax_t>(fs::file_size(file))},
            {"fingerprint", fingerprint}
        });
    }

    fs::path entry = "index.jtml";
    if (!fs::exists(packageDir / entry) && fs::exists(packageDir / "package.jtml")) {
        entry = "package.jtml";
    }

    return {
        {"name", packageName},
        {"path", (fs::path("jtml_modules") / packageName).generic_string()},
        {"source", source.string()},
        {"entry", entry.generic_string()},
        {"fingerprint", fnv1aHex(combined)},
        {"files", fileEntries}
    };
}

nlohmann::json readJsonFile(const std::filesystem::path& path,
                            const std::string& label) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("Missing " + label + ": " + path.string());
    }
    try {
        std::ifstream in(path);
        nlohmann::json value;
        in >> value;
        return value;
    } catch (...) {
        throw std::runtime_error("Cannot parse " + label + ": " + path.string());
    }
}

} // namespace

std::string packageNameFromPath(const std::filesystem::path& source) {
    std::string name = source.filename().string();
    if (source.has_extension() && source.extension() == ".jtml") {
        name = source.stem().string();
    }
    return name;
}

void installLocalPackage(const std::filesystem::path& source,
                         const std::string& packageName,
                         bool force,
                         bool jsonOutput,
                         bool quiet) {
    namespace fs = std::filesystem;
    if (!isValidPackageName(packageName)) {
        throw std::runtime_error(
            "Invalid package name `" + packageName +
            "`. Use letters, numbers, dots, hyphens, or underscores.");
    }
    if (!fs::exists(source)) {
        throw std::runtime_error(
            "Package source not found: " + source.string() +
            ". Registry installs are planned; today `jtml add` installs local files or directories.");
    }

    fs::path absoluteSource = fs::absolute(source).lexically_normal();
    if (fs::is_directory(absoluteSource) &&
        !fs::exists(absoluteSource / "index.jtml") &&
        !fs::exists(absoluteSource / "package.jtml")) {
        throw std::runtime_error(
            "Local package directory must contain index.jtml or package.jtml: " +
            absoluteSource.string());
    }
    if (fs::is_regular_file(absoluteSource) && absoluteSource.extension() != ".jtml") {
        throw std::runtime_error("Local package file must be a .jtml file: " +
                                 absoluteSource.string());
    }

    const fs::path modulesDir = fs::current_path() / "jtml_modules";
    const fs::path dest = modulesDir / packageName;
    if (fs::exists(dest) && !force) {
        throw std::runtime_error(
            "Package already installed: " + packageName + " (use --force)");
    }

    fs::create_directories(modulesDir);
    if (fs::exists(dest)) fs::remove_all(dest);
    fs::create_directories(dest);

    if (fs::is_directory(absoluteSource)) {
        fs::copy(absoluteSource, dest,
                 fs::copy_options::recursive | fs::copy_options::overwrite_existing);
    } else {
        fs::copy_file(absoluteSource, dest / "index.jtml",
                      fs::copy_options::overwrite_existing);
    }

    const fs::path manifestPath = fs::current_path() / "jtml.packages.json";
    nlohmann::json manifest = {
        {"version", 1},
        {"packages", nlohmann::json::object()}
    };
    if (fs::exists(manifestPath)) {
        try {
            std::ifstream in(manifestPath);
            in >> manifest;
            if (!manifest.contains("packages") || !manifest["packages"].is_object()) {
                manifest["packages"] = nlohmann::json::object();
            }
            manifest["version"] = manifest.value("version", 1);
        } catch (...) {
            throw std::runtime_error("Cannot parse existing package manifest: " +
                                     manifestPath.string());
        }
    }

    manifest["packages"][packageName] = {
        {"path", (fs::path("jtml_modules") / packageName).generic_string()},
        {"source", absoluteSource.string()}
    };
    writeFile(manifestPath.string(), manifest.dump(2) + "\n", true);

    const fs::path lockPath = fs::current_path() / "jtml.lock.json";
    nlohmann::json lock = {
        {"version", 1},
        {"packages", nlohmann::json::object()}
    };
    if (fs::exists(lockPath)) {
        try {
            std::ifstream in(lockPath);
            in >> lock;
            if (!lock.contains("packages") || !lock["packages"].is_object()) {
                lock["packages"] = nlohmann::json::object();
            }
            lock["version"] = lock.value("version", 1);
        } catch (...) {
            throw std::runtime_error("Cannot parse existing package lockfile: " +
                                     lockPath.string());
        }
    }
    lock["packages"][packageName] = packageLockEntry(dest, absoluteSource, packageName);
    writeFile(lockPath.string(), lock.dump(2) + "\n", true);

    if (jsonOutput && !quiet) {
        std::cout << nlohmann::json({
            {"ok", true},
            {"package", packageName},
            {"path", dest.string()},
            {"manifest", manifestPath.string()},
            {"lockfile", lockPath.string()}
        }).dump(2) << "\n";
    } else if (!quiet) {
        std::cout << "Installed JTML package `" << packageName << "` at "
                  << dest.string() << "\n"
                  << "Locked package fingerprint in " << lockPath.string() << "\n"
                  << "Import it with: use Name from \"" << packageName << "\"\n";
    }
}

nlohmann::json verifyPackageLock(bool restoreFromManifest) {
    namespace fs = std::filesystem;
    const fs::path root = fs::current_path();
    const fs::path manifestPath = root / "jtml.packages.json";
    const fs::path lockPath = root / "jtml.lock.json";
    nlohmann::json restored = nlohmann::json::array();

    if (restoreFromManifest && fs::exists(manifestPath)) {
        const auto manifest = readJsonFile(manifestPath, "package manifest");
        if (manifest.contains("packages") && manifest["packages"].is_object()) {
            for (const auto& item : manifest["packages"].items()) {
                const std::string name = item.key();
                const std::string source = item.value().value("source", "");
                if (source.empty() || !fs::exists(source)) continue;
                installLocalPackage(source, name, true, false, true);
                restored.push_back(name);
            }
        }
    }

    const auto lock = readJsonFile(lockPath, "package lockfile");
    if (!lock.contains("packages") || !lock["packages"].is_object()) {
        throw std::runtime_error("Package lockfile has no object `packages`: " +
                                 lockPath.string());
    }

    nlohmann::json packages = nlohmann::json::array();
    bool ok = true;
    for (const auto& item : lock["packages"].items()) {
        const std::string name = item.key();
        const auto& expected = item.value();
        const fs::path packageDir = root / expected.value(
            "path", (fs::path("jtml_modules") / name).generic_string());

        nlohmann::json result = {
            {"name", name},
            {"path", packageDir.string()},
            {"ok", false}
        };
        if (!fs::exists(packageDir)) {
            result["error"] = "package directory is missing";
            ok = false;
            packages.push_back(result);
            continue;
        }

        const auto actual = packageLockEntry(
            packageDir,
            expected.value("source", ""),
            name);
        result["expectedFingerprint"] = expected.value("fingerprint", "");
        result["actualFingerprint"] = actual.value("fingerprint", "");
        result["fileCount"] = actual["files"].size();

        if (actual.value("fingerprint", "") != expected.value("fingerprint", "")) {
            result["error"] = "package fingerprint mismatch";
            ok = false;
        } else {
            result["ok"] = true;
        }
        packages.push_back(result);
    }

    return {
        {"ok", ok},
        {"manifest", manifestPath.string()},
        {"lockfile", lockPath.string()},
        {"restored", restored},
        {"packages", packages}
    };
}

} // namespace jtml::cli
