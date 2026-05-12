// cli/cmd_refactor.cpp — `jtml refactor` command group.
//
// First slice: `jtml refactor rename <file-or-dir> --from <name> --to <newname>`.
// The rewrite is string- and comment-aware, word-boundary only — identical
// to the LSP rename, since both call `jtml::renameSymbolInSource`. Future
// sub-refactors (extract component, add wildcard route, add loading/error
// states) hang off the same `subcommand` dispatcher.
//
// Scopes:
//   * Default: rewrite a single .jtml file passed as the positional argument.
//   * `--workspace`: when the positional argument is a directory, walk it
//     recursively and rewrite every .jtml file. The same skip rules as the
//     LSP workspace scan apply (build/dist/node_modules etc.).
#include "commands.h"

#include "jtml/refactor.h"

#include "json.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace jtml::cli {

namespace {

// Names of directories the workspace scan must never recurse into. Mirrors
// the skip list used by `cli/cmd_lsp.cpp::shouldSkipDirectory` so workspace
// rename behaviour is consistent across CLI and LSP.
bool shouldSkipWorkspaceDirectory(const std::filesystem::path& path) {
    const std::string name = path.filename().string();
    if (name.empty() || name[0] == '.') return true;
    return name == "build" || name == "dist" || name == "node_modules" ||
           name == "out"   || name == "target" || name == ".git";
}

std::vector<std::filesystem::path> collectWorkspaceJtmlFiles(
        const std::filesystem::path& root) {
    namespace fs = std::filesystem;
    std::vector<fs::path> files;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(root, ec);
         !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) break;
        if (it->is_directory()) {
            if (shouldSkipWorkspaceDirectory(it->path())) {
                it.disable_recursion_pending();
            }
            continue;
        }
        if (it->path().extension() == ".jtml") {
            files.push_back(it->path());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

struct FileRenameOutcome {
    std::string path;
    bool changed = false;
    bool written = false;
    std::vector<jtml::RenameEdit> edits;
};

FileRenameOutcome applyRenameToFile(const std::filesystem::path& path,
                                     const std::string& fromName,
                                     const std::string& toName,
                                     bool write) {
    FileRenameOutcome outcome;
    outcome.path = path.string();

    std::ifstream in(path);
    if (!in.is_open()) {
        throw std::runtime_error("Cannot read file: " + outcome.path);
    }
    std::stringstream buf;
    buf << in.rdbuf();
    const std::string original = buf.str();
    in.close();

    const auto result = jtml::renameSymbolInSource(original, fromName, toName);
    outcome.changed = result.changed;
    outcome.edits = result.edits;

    if (write && result.changed) {
        std::ofstream out(path);
        if (!out.is_open()) {
            throw std::runtime_error("Cannot write file: " + outcome.path);
        }
        out << result.source;
        outcome.written = true;
    }
    return outcome;
}

nlohmann::json editsToJson(const std::vector<jtml::RenameEdit>& edits) {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& edit : edits) {
        arr.push_back({
            {"line", edit.line},
            {"startColumn", edit.startColumn},
            {"endColumn", edit.endColumn},
        });
    }
    return arr;
}

int cmdRefactorRenameSingle(const Options& o) {
    const std::string original = readFile(o.inputFile);
    const auto result = jtml::renameSymbolInSource(original, o.fromName, o.toName);

    if (o.force && result.changed) {
        std::ofstream out(o.inputFile);
        if (!out.is_open()) {
            throw std::runtime_error("Cannot write file: " + o.inputFile);
        }
        out << result.source;
    }

    if (o.json) {
        nlohmann::json out = {
            {"ok", true},
            {"file", o.inputFile},
            {"refactor", "rename"},
            {"scope", "file"},
            {"from", o.fromName},
            {"to", o.toName},
            {"changed", result.changed},
            {"written", o.force && result.changed},
            {"edits", editsToJson(result.edits)},
        };
        std::cout << out.dump(2) << "\n";
        return 0;
    }

    if (!result.changed) {
        std::cerr << "No occurrences of `" << o.fromName << "` in "
                  << o.inputFile << "\n";
        return 0;
    }
    if (o.force) {
        std::cout << "Renamed " << result.edits.size() << " occurrence"
                  << (result.edits.size() == 1 ? "" : "s")
                  << " of `" << o.fromName << "` -> `" << o.toName
                  << "` in " << o.inputFile << "\n";
        return 0;
    }
    // Default: print the rewritten source to stdout for piping/preview, like
    // `jtml fmt`. Use `-w` to apply in place.
    std::cout << result.source;
    return 0;
}

int cmdRefactorRenameWorkspace(const Options& o) {
    namespace fs = std::filesystem;
    const fs::path root = o.inputFile;
    const auto paths = collectWorkspaceJtmlFiles(root);

    std::vector<FileRenameOutcome> outcomes;
    std::size_t totalEdits = 0;
    std::size_t filesChanged = 0;
    for (const auto& path : paths) {
        auto outcome = applyRenameToFile(path, o.fromName, o.toName, o.force);
        if (outcome.changed) {
            ++filesChanged;
            totalEdits += outcome.edits.size();
        }
        outcomes.push_back(std::move(outcome));
    }

    if (o.json) {
        nlohmann::json files = nlohmann::json::array();
        for (const auto& outcome : outcomes) {
            if (!outcome.changed) continue;  // suppress no-op files in JSON
            files.push_back({
                {"file", outcome.path},
                {"changed", true},
                {"written", outcome.written},
                {"edits", editsToJson(outcome.edits)},
            });
        }
        nlohmann::json out = {
            {"ok", true},
            {"refactor", "rename"},
            {"scope", "workspace"},
            {"root", root.string()},
            {"from", o.fromName},
            {"to", o.toName},
            {"changed", filesChanged > 0},
            {"filesChanged", filesChanged},
            {"totalEdits", totalEdits},
            {"files", files},
        };
        std::cout << out.dump(2) << "\n";
        return 0;
    }

    if (filesChanged == 0) {
        std::cerr << "No occurrences of `" << o.fromName << "` under "
                  << root.string() << "\n";
        return 0;
    }
    for (const auto& outcome : outcomes) {
        if (!outcome.changed) continue;
        if (o.force) {
            std::cout << "Renamed " << outcome.edits.size() << " occurrence"
                      << (outcome.edits.size() == 1 ? "" : "s")
                      << " of `" << o.fromName << "` -> `" << o.toName
                      << "` in " << outcome.path << "\n";
        } else {
            std::cout << outcome.path << ": " << outcome.edits.size()
                      << " occurrence"
                      << (outcome.edits.size() == 1 ? "" : "s")
                      << " (preview only — rerun with -w to apply)\n";
        }
    }
    if (!o.force) {
        std::cout << "\n" << totalEdits << " edits across "
                  << filesChanged << " file"
                  << (filesChanged == 1 ? "" : "s")
                  << ". Rerun with -w to apply.\n";
    }
    return 0;
}

int cmdRefactorRename(const Options& o) {
    if (o.fromName.empty() || o.toName.empty()) {
        throw std::runtime_error(
            "jtml refactor rename requires --from <name> and --to <newname>.");
    }
    if (o.fromName == o.toName) {
        throw std::runtime_error("--from and --to are identical; nothing to rename.");
    }
    namespace fs = std::filesystem;
    std::error_code ec;
    if (fs::is_directory(o.inputFile, ec)) {
        return cmdRefactorRenameWorkspace(o);
    }
    return cmdRefactorRenameSingle(o);
}

} // namespace

int cmdRefactor(const Options& o) {
    if (o.subcommand == "rename") return cmdRefactorRename(o);
    throw std::runtime_error(
        "Unknown refactor: '" + o.subcommand +
        "'. Supported: rename --from <name> --to <newname>.");
}

} // namespace jtml::cli
