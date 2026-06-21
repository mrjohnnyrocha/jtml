#pragma once

#include "jtml/semantic.h"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace jtml {

using SemanticModuleId = std::uint32_t;
inline constexpr SemanticModuleId InvalidSemanticModuleId =
    std::numeric_limits<SemanticModuleId>::max();

struct SemanticSourceSpan {
    SemanticModuleId module = 0;
    std::size_t line = 0;
    std::size_t column = 0;
};

struct SemanticAstNodeRecord {
    std::string kind;
    std::string label;
    std::size_t depth = 0;
    std::size_t childCount = 0;
};

struct SemanticAstNodeCount {
    std::string kind;
    std::size_t count = 0;
};

struct SemanticModuleIr {
    bool available = false;
    std::string syntax;
    std::string parseError;
    std::size_t parseErrorLine = 0;
    std::size_t parseErrorColumn = 0;
    std::size_t topLevelCount = 0;
    std::size_t totalNodeCount = 0;
    std::vector<SemanticAstNodeRecord> topLevelNodes;
    std::vector<SemanticAstNodeCount> nodeCounts;
};

struct SemanticImportedSymbol {
    std::string name;
    std::string kind;
    SemanticModuleId module = InvalidSemanticModuleId;
};

struct SemanticModuleAst {
    bool available = false;
    std::string syntax;
    std::vector<std::unique_ptr<ASTNode>> nodes;
};

struct SemanticModuleImport {
    std::string specifier;
    std::string kind;
    std::vector<std::string> names;
    std::vector<SemanticImportedSymbol> resolvedSymbols;
    bool reExport = false;
    SemanticModuleId importer = InvalidSemanticModuleId;
    SemanticModuleId resolved = InvalidSemanticModuleId;
    std::string resolvedPath;
    SemanticSourceSpan span;
};

struct SemanticModule {
    SemanticModuleId id = 0;
    std::string path;
    std::vector<SemanticModuleImport> imports;
    std::vector<SemanticExport> exports;
    SemanticProgram semantic;
    SemanticModuleIr ir;
    std::shared_ptr<const SemanticModuleAst> ast;
};

struct SemanticProject {
    SemanticModuleId entry = InvalidSemanticModuleId;
    std::vector<SemanticModule> modules;
    SemanticProgram linkedProgram;
};

struct SemanticProjectIssue {
    std::string code;
    std::string message;
    SemanticModuleId module = InvalidSemanticModuleId;
    std::size_t importIndex = 0;
    std::string specifier;
    std::string resolvedPath;
    std::string path;
    std::size_t line = 0;
    std::size_t column = 0;
    std::vector<std::string> requested;
    std::vector<std::string> available;
};

struct SemanticModuleSource {
    std::string path;
    SemanticProgram semantic;
    SemanticModuleIr ir;
    std::shared_ptr<const SemanticModuleAst> ast;
};

std::shared_ptr<const SemanticModuleAst> cloneSemanticAst(
    const std::vector<std::unique_ptr<ASTNode>>& program,
    const std::string& syntax = "");

SemanticModuleIr summarizeSemanticAst(const std::vector<std::unique_ptr<ASTNode>>& program,
                                      const std::string& syntax = "");

SemanticProject buildSemanticProject(const SemanticProgram& linkedProgram,
                                     const std::string& entryPath = "");

SemanticProject buildSemanticProject(const std::vector<SemanticModuleSource>& modules,
                                     const std::string& entryPath = "",
                                     const SemanticProgram& linkedProgram = {});

std::vector<SemanticProjectIssue> analyzeSemanticProject(const SemanticProject& project);

} // namespace jtml
