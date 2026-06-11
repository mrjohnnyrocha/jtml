#include "jtml/semantic/module_graph.h"

#include <filesystem>
#include <map>
#include <system_error>
#include <utility>

namespace jtml {
namespace {

std::string normalizeProjectPath(const std::string& path) {
    if (path.empty() || path[0] == '<') return path;
    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(path, ec);
    if (!ec) return canonical.generic_string();
    return std::filesystem::absolute(path).lexically_normal().generic_string();
}

std::string resolveImportPath(const std::string& importerPath,
                              const std::string& specifier) {
    if (specifier.empty() || specifier[0] == '<') return specifier;
    std::filesystem::path specifierPath(specifier);
    if (specifierPath.is_absolute()) return normalizeProjectPath(specifier);
    if (importerPath.empty() || importerPath[0] == '<') {
        return normalizeProjectPath(specifier);
    }
    const auto base = std::filesystem::path(importerPath).parent_path();
    return normalizeProjectPath((base / specifierPath).generic_string());
}

} // namespace

SemanticProject buildSemanticProject(const SemanticProgram& linkedProgram,
                                     const std::string& entryPath) {
    SemanticProject project;
    project.linkedProgram = linkedProgram;

    std::map<std::string, SemanticModuleId> idsByPath;
    auto ensureModule = [&](const std::string& path) -> SemanticModuleId {
        auto found = idsByPath.find(path);
        if (found != idsByPath.end()) return found->second;
        SemanticModule module;
        module.id = static_cast<SemanticModuleId>(project.modules.size());
        module.path = path;
        idsByPath.emplace(path, module.id);
        project.modules.push_back(std::move(module));
        return static_cast<SemanticModuleId>(project.modules.size() - 1);
    };

    if (!entryPath.empty()) {
        project.entry = ensureModule(normalizeProjectPath(entryPath));
    }

    for (const auto& file : linkedProgram.moduleFiles) {
        const auto normalized = normalizeProjectPath(file);
        const auto id = ensureModule(normalized);
        if (!entryPath.empty() && normalized == normalizeProjectPath(entryPath)) {
            project.entry = id;
        }
    }

    if (project.modules.empty()) {
        project.entry = ensureModule(entryPath.empty() ? "<memory>" : entryPath);
    }

    for (const auto& record : linkedProgram.importRecords) {
        SemanticModuleImport import;
        import.specifier = record.specifier;
        import.kind = record.kind;
        import.names = record.names;
        import.reExport = record.reExport;
        import.importer = project.entry;
        const auto resolvedPath = resolveImportPath(project.modules[project.entry].path,
                                                   record.specifier);
        const auto found = idsByPath.find(resolvedPath);
        if (found != idsByPath.end()) {
            import.resolved = found->second;
        }
        import.span.module = project.entry;
        project.modules[project.entry].imports.push_back(std::move(import));
    }

    return project;
}

} // namespace jtml
