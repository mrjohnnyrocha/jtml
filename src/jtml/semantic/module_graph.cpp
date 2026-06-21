#include "jtml/semantic/module_graph.h"

#include "jtml/diagnostic.h"
#include "jtml/module_resolver.h"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <sstream>
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

std::string joinNames(const std::vector<std::string>& names) {
    std::ostringstream out;
    for (size_t i = 0; i < names.size(); ++i) {
        if (i > 0) out << ", ";
        out << names[i];
    }
    return out.str();
}

std::string astNodeKind(ASTNodeType type) {
    switch (type) {
        case ASTNodeType::JtmlElement: return "JtmlElement";
        case ASTNodeType::ShowStatement: return "ShowStatement";
        case ASTNodeType::DefineStatement: return "DefineStatement";
        case ASTNodeType::DeriveStatement: return "DeriveStatement";
        case ASTNodeType::UnbindStatement: return "UnbindStatement";
        case ASTNodeType::StoreStatement: return "StoreStatement";
        case ASTNodeType::AssignmentStatement: return "AssignmentStatement";
        case ASTNodeType::ExpressionStatement: return "ExpressionStatement";
        case ASTNodeType::ReturnStatement: return "ReturnStatement";
        case ASTNodeType::ThrowStatement: return "ThrowStatement";
        case ASTNodeType::ImportStatement: return "ImportStatement";
        case ASTNodeType::IfStatement: return "IfStatement";
        case ASTNodeType::WhileStatement: return "WhileStatement";
        case ASTNodeType::ForStatement: return "ForStatement";
        case ASTNodeType::TryExceptThen: return "TryExceptThen";
        case ASTNodeType::BreakStatement: return "BreakStatement";
        case ASTNodeType::ContinueStatement: return "ContinueStatement";
        case ASTNodeType::BlockStatement: return "BlockStatement";
        case ASTNodeType::FunctionDeclaration: return "FunctionDeclaration";
        case ASTNodeType::SubscribeStatement: return "SubscribeStatement";
        case ASTNodeType::UnsubscribeStatement: return "UnsubscribeStatement";
        case ASTNodeType::NoOp: return "NoOp";
        case ASTNodeType::ClassDeclaration: return "ClassDeclaration";
    }
    return "Unknown";
}

std::string astNodeLabel(const ASTNode& node) {
    switch (node.getType()) {
        case ASTNodeType::JtmlElement:
            return static_cast<const JtmlElementNode&>(node).tagName;
        case ASTNodeType::DefineStatement:
            return static_cast<const DefineStatementNode&>(node).identifier;
        case ASTNodeType::DeriveStatement:
            return static_cast<const DeriveStatementNode&>(node).identifier;
        case ASTNodeType::UnbindStatement:
            return static_cast<const UnbindStatementNode&>(node).identifier;
        case ASTNodeType::StoreStatement: {
            const auto& store = static_cast<const StoreStatementNode&>(node);
            return store.targetScope.empty() ? store.variableName : store.targetScope;
        }
        case ASTNodeType::ImportStatement:
            return static_cast<const ImportStatementNode&>(node).path;
        case ASTNodeType::FunctionDeclaration:
            return static_cast<const FunctionDeclarationNode&>(node).name;
        case ASTNodeType::ClassDeclaration:
            return static_cast<const ClassDeclarationNode&>(node).name;
        case ASTNodeType::SubscribeStatement:
            return static_cast<const SubscribeStatementNode&>(node).functionName;
        case ASTNodeType::UnsubscribeStatement:
            return static_cast<const UnsubscribeStatementNode&>(node).functionName;
        case ASTNodeType::ForStatement:
            return static_cast<const ForStatementNode&>(node).iteratorName;
        default:
            return "";
    }
}

std::vector<const ASTNode*> astChildren(const ASTNode& node) {
    std::vector<const ASTNode*> children;
    auto append = [&](const std::vector<std::unique_ptr<ASTNode>>& nodes) {
        for (const auto& child : nodes) {
            if (child) children.push_back(child.get());
        }
    };

    switch (node.getType()) {
        case ASTNodeType::JtmlElement:
            append(static_cast<const JtmlElementNode&>(node).content);
            break;
        case ASTNodeType::BlockStatement:
            append(static_cast<const BlockStatementNode&>(node).statements);
            break;
        case ASTNodeType::IfStatement: {
            const auto& stmt = static_cast<const IfStatementNode&>(node);
            append(stmt.thenStatements);
            append(stmt.elseStatements);
            break;
        }
        case ASTNodeType::WhileStatement:
            append(static_cast<const WhileStatementNode&>(node).body);
            break;
        case ASTNodeType::ForStatement:
            append(static_cast<const ForStatementNode&>(node).body);
            break;
        case ASTNodeType::TryExceptThen: {
            const auto& stmt = static_cast<const TryExceptThenNode&>(node);
            append(stmt.tryBlock);
            append(stmt.catchBlock);
            append(stmt.finallyBlock);
            break;
        }
        case ASTNodeType::FunctionDeclaration:
            append(static_cast<const FunctionDeclarationNode&>(node).body);
            break;
        case ASTNodeType::ClassDeclaration:
            append(static_cast<const ClassDeclarationNode&>(node).members);
            break;
        default:
            break;
    }
    return children;
}

void summarizeAstNode(const ASTNode& node,
                      std::size_t depth,
                      std::map<std::string, std::size_t>& counts,
                      std::size_t& total) {
    ++total;
    ++counts[astNodeKind(node.getType())];
    for (const auto* child : astChildren(node)) {
        summarizeAstNode(*child, depth + 1, counts, total);
    }
}

} // namespace

std::shared_ptr<const SemanticModuleAst> cloneSemanticAst(
    const std::vector<std::unique_ptr<ASTNode>>& program,
    const std::string& syntax) {
    auto ast = std::make_shared<SemanticModuleAst>();
    ast->available = true;
    ast->syntax = syntax;
    ast->nodes.reserve(program.size());
    for (const auto& node : program) {
        if (node) {
            ast->nodes.push_back(node->clone());
        }
    }
    return ast;
}

SemanticModuleIr summarizeSemanticAst(const std::vector<std::unique_ptr<ASTNode>>& program,
                                      const std::string& syntax) {
    SemanticModuleIr ir;
    ir.available = true;
    ir.syntax = syntax;
    ir.topLevelCount = program.size();

    std::map<std::string, std::size_t> counts;
    for (const auto& node : program) {
        if (!node) continue;
        const auto children = astChildren(*node);
        ir.topLevelNodes.push_back({
            astNodeKind(node->getType()),
            astNodeLabel(*node),
            0,
            children.size(),
        });
        summarizeAstNode(*node, 0, counts, ir.totalNodeCount);
    }

    for (const auto& [kind, count] : counts) {
        ir.nodeCounts.push_back({kind, count});
    }
    return ir;
}

SemanticProject buildSemanticProject(const SemanticProgram& linkedProgram,
                                     const std::string& entryPath) {
    std::vector<SemanticModuleSource> modules;
    modules.reserve(linkedProgram.moduleFiles.size() + 1);
    for (const auto& file : linkedProgram.moduleFiles) {
        modules.push_back({file, {}});
    }
    if (!entryPath.empty()) {
        bool hasEntry = false;
        const auto normalizedEntry = normalizeProjectPath(entryPath);
        for (const auto& module : modules) {
            if (normalizeProjectPath(module.path) == normalizedEntry) {
                hasEntry = true;
                break;
            }
        }
        if (!hasEntry) modules.insert(modules.begin(), {entryPath, {}});
    }
    if (modules.empty()) modules.push_back({entryPath.empty() ? "<memory>" : entryPath, {}});

    modules.front().semantic = linkedProgram;
    return buildSemanticProject(modules, entryPath, linkedProgram);
}

SemanticProject buildSemanticProject(const std::vector<SemanticModuleSource>& moduleSources,
                                     const std::string& entryPath,
                                     const SemanticProgram& linkedProgram) {
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

    const std::string normalizedEntry = entryPath.empty() ? "" : normalizeProjectPath(entryPath);
    for (const auto& source : moduleSources) {
        const auto normalized = normalizeProjectPath(source.path);
        const auto id = ensureModule(normalized);
        if (!normalizedEntry.empty() && normalized == normalizedEntry) {
            project.entry = id;
        }
    }

    if (project.modules.empty()) {
        project.entry = ensureModule(entryPath.empty() ? "<memory>" : entryPath);
    } else if (project.entry == InvalidSemanticModuleId) {
        project.entry = 0;
    }

    for (const auto& source : moduleSources) {
        const auto normalized = normalizeProjectPath(source.path);
        const auto importerIt = idsByPath.find(normalized);
        if (importerIt == idsByPath.end()) continue;
        const auto importerId = importerIt->second;
        project.modules[importerId].semantic = source.semantic;
        project.modules[importerId].ir = source.ir;
        project.modules[importerId].ast = source.ast;
        project.modules[importerId].exports = source.semantic.exportRecords;
    }

    std::function<std::optional<SemanticImportedSymbol>(
        SemanticModuleId,
        const std::string&,
        std::set<std::pair<SemanticModuleId, std::string>>&)> resolveExportedSymbol;

    resolveExportedSymbol =
        [&](SemanticModuleId moduleId,
            const std::string& name,
            std::set<std::pair<SemanticModuleId, std::string>>& seen)
                -> std::optional<SemanticImportedSymbol> {
            if (moduleId == InvalidSemanticModuleId || moduleId >= project.modules.size()) {
                return std::nullopt;
            }
            const auto key = std::make_pair(moduleId, name);
            if (!seen.insert(key).second) {
                return std::nullopt;
            }

            const auto& module = project.modules[moduleId];
            for (const auto& exported : module.exports) {
                if (exported.name != name) continue;
                if (exported.reExport && !exported.specifier.empty()) {
                    const auto resolvedPath = normalizeProjectPath(
                        resolveJtmlModulePath(exported.specifier, module.path).generic_string());
                    const auto targetIt = idsByPath.find(resolvedPath);
                    if (targetIt != idsByPath.end()) {
                        if (auto resolved = resolveExportedSymbol(targetIt->second, name, seen)) {
                            return resolved;
                        }
                    }
                    return std::nullopt;
                }
                if (exported.reExport) {
                    return std::nullopt;
                }
                return SemanticImportedSymbol{
                    exported.name,
                    exported.kind,
                    moduleId,
                };
            }
            return std::nullopt;
        };

    for (const auto& source : moduleSources) {
        const auto normalized = normalizeProjectPath(source.path);
        const auto importerIt = idsByPath.find(normalized);
        if (importerIt == idsByPath.end()) continue;
        const auto importerId = importerIt->second;
        for (const auto& record : source.semantic.importRecords) {
            SemanticModuleImport import;
            import.specifier = record.specifier;
            import.kind = record.kind;
            import.names = record.names;
            import.reExport = record.reExport;
            import.importer = importerId;
            const auto resolvedPath = normalizeProjectPath(
                resolveJtmlModulePath(record.specifier, project.modules[importerId].path)
                    .generic_string());
            import.resolvedPath = resolvedPath;
            const auto found = idsByPath.find(resolvedPath);
            if (found != idsByPath.end()) {
                import.resolved = found->second;
                for (const auto& requestedName : import.names) {
                    std::set<std::pair<SemanticModuleId, std::string>> seen;
                    if (auto symbol = resolveExportedSymbol(import.resolved, requestedName, seen)) {
                        import.resolvedSymbols.push_back(*symbol);
                    }
                }
            }
            import.span.module = importerId;
            import.span.line = record.sourceLine > 0 ? static_cast<std::size_t>(record.sourceLine) : 0;
            import.span.column = record.sourceColumn > 0 ? static_cast<std::size_t>(record.sourceColumn) : 0;
            project.modules[importerId].imports.push_back(std::move(import));
        }
    }

    return project;
}

std::vector<SemanticProjectIssue> analyzeSemanticProject(const SemanticProject& project) {
    std::vector<SemanticProjectIssue> issues;
    std::map<std::string, SemanticModuleId> idsByPath;
    for (const auto& module : project.modules) {
        idsByPath.emplace(module.path, module.id);

        if (!module.ir.available && !module.ir.parseError.empty()) {
            const auto diagnostic = diagnosticFromMessage(module.ir.parseError);
            SemanticProjectIssue issue;
            issue.code = "JTML_MODULE_PARSE";
            issue.module = module.id;
            issue.path = module.path;
            issue.resolvedPath = module.path;
            issue.line = module.ir.parseErrorLine > 0
                ? module.ir.parseErrorLine
                : (diagnostic.line > 0 ? static_cast<std::size_t>(diagnostic.line) : 0);
            issue.column = module.ir.parseErrorColumn > 0
                ? module.ir.parseErrorColumn
                : (diagnostic.column > 0 ? static_cast<std::size_t>(diagnostic.column) : 0);
            issue.message = "Cannot parse module " + module.path + ": " + module.ir.parseError;
            issues.push_back(std::move(issue));
        }
    }

    enum class ReExportResolution {
        Resolved,
        MissingExport,
        MissingModule,
        Cycle,
    };

    auto resolveReExportChain = [&](SemanticModuleId start,
                                    const std::string& name,
                                    std::string& failedPath) {
        SemanticModuleId current = start;
        std::set<std::pair<SemanticModuleId, std::string>> seen;
        while (current != InvalidSemanticModuleId && current < project.modules.size()) {
            const auto key = std::make_pair(current, name);
            if (!seen.insert(key).second) return ReExportResolution::Cycle;

            const auto& module = project.modules[current];
            auto exportIt = std::find_if(
                module.exports.begin(),
                module.exports.end(),
                [&](const auto& exported) {
                    return exported.name == name;
                });
            if (exportIt == module.exports.end()) return ReExportResolution::MissingExport;
            if (!exportIt->reExport) return ReExportResolution::Resolved;

            const auto nextPath = normalizeProjectPath(
                resolveJtmlModulePath(exportIt->specifier, module.path).generic_string());
            failedPath = nextPath;
            const auto targetIt = idsByPath.find(nextPath);
            if (targetIt == idsByPath.end()) return ReExportResolution::MissingModule;
            current = targetIt->second;
        }
        return ReExportResolution::MissingModule;
    };

    for (const auto& module : project.modules) {
        for (size_t i = 0; i < module.imports.size(); ++i) {
            const auto& import = module.imports[i];
            if (import.resolved == InvalidSemanticModuleId ||
                import.resolved >= project.modules.size()) {
                SemanticProjectIssue issue;
                issue.code = "JTML_UNRESOLVED_IMPORT";
                issue.module = module.id;
                issue.importIndex = i;
                issue.specifier = import.specifier;
                issue.resolvedPath = import.resolvedPath;
                issue.requested = import.names;
                issue.message = "Cannot resolve import '" + import.specifier + "' from " +
                                module.path + " (attempted " + import.resolvedPath + ")";
                issues.push_back(std::move(issue));
                continue;
            }

            if (import.names.empty()) continue;
            const auto& target = project.modules[import.resolved];

            std::set<std::string> availableSet;
            std::vector<std::string> available;
            for (const auto& exported : target.exports) {
                if (availableSet.insert(exported.name).second) available.push_back(exported.name);
            }

            std::vector<std::string> missing;
            for (const auto& name : import.names) {
                if (!availableSet.count(name)) missing.push_back(name);
            }
            if (missing.empty()) continue;

            SemanticProjectIssue issue;
            issue.code = "JTML_MISSING_EXPORT";
            issue.module = module.id;
            issue.importIndex = i;
            issue.specifier = import.specifier;
            issue.resolvedPath = import.resolvedPath;
            issue.requested = missing;
            issue.available = available;
            issue.message = "Import '" + import.specifier + "' is missing exported " +
                            (missing.size() == 1 ? "declaration " : "declarations ") +
                            joinNames(missing);
            if (!available.empty()) issue.message += ". Available exports: " + joinNames(available);
            issues.push_back(std::move(issue));

            continue;
        }

        for (size_t i = 0; i < module.imports.size(); ++i) {
            const auto& import = module.imports[i];
            if (import.names.empty() ||
                import.resolved == InvalidSemanticModuleId ||
                import.resolved >= project.modules.size()) {
                continue;
            }

            std::set<std::string> resolvedNames;
            for (const auto& symbol : import.resolvedSymbols) {
                resolvedNames.insert(symbol.name);
            }

            for (const auto& name : import.names) {
                if (resolvedNames.count(name)) continue;

                const auto& target = project.modules[import.resolved];
                const bool isReExportedName = std::any_of(
                    target.exports.begin(),
                    target.exports.end(),
                    [&](const auto& exported) {
                        return exported.name == name && exported.reExport;
                    });
                if (!isReExportedName) continue;

                std::string failedPath;
                const auto status = resolveReExportChain(import.resolved, name, failedPath);
                if (status == ReExportResolution::Resolved) {
                    continue;
                }

                SemanticProjectIssue issue;
                issue.module = module.id;
                issue.importIndex = i;
                issue.specifier = import.specifier;
                issue.resolvedPath = failedPath.empty() ? import.resolvedPath : failedPath;
                issue.requested = {name};
                if (status == ReExportResolution::Cycle) {
                    issue.code = "JTML_REEXPORT_CYCLE";
                    issue.message = "Re-export chain for '" + name +
                                    "' cycles while resolving import '" + import.specifier + "'";
                } else if (status == ReExportResolution::MissingExport) {
                    issue.code = "JTML_UNRESOLVED_REEXPORT";
                    issue.message = "Re-export chain for '" + name + "' from import '" +
                                    import.specifier + "' does not reach an exported declaration";
                } else {
                    issue.code = "JTML_UNRESOLVED_REEXPORT";
                    issue.message = "Re-export chain for '" + name + "' from import '" +
                                    import.specifier + "' points to unresolved module " +
                                    issue.resolvedPath;
                }
                issues.push_back(std::move(issue));
            }
        }
    }
    return issues;
}

} // namespace jtml
