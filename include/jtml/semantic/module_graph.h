#pragma once

#include "jtml/semantic.h"

#include <cstdint>
#include <string>
#include <vector>

namespace jtml {

using SemanticModuleId = std::uint32_t;

struct SemanticSourceSpan {
    SemanticModuleId module = 0;
    std::size_t line = 0;
    std::size_t column = 0;
};

struct SemanticModuleImport {
    std::string specifier;
    std::string kind;
    std::vector<std::string> names;
    bool reExport = false;
    SemanticModuleId importer = 0;
    SemanticModuleId resolved = 0;
    SemanticSourceSpan span;
};

struct SemanticModule {
    SemanticModuleId id = 0;
    std::string path;
    std::vector<SemanticModuleImport> imports;
};

struct SemanticProject {
    SemanticModuleId entry = 0;
    std::vector<SemanticModule> modules;
    SemanticProgram linkedProgram;
};

SemanticProject buildSemanticProject(const SemanticProgram& linkedProgram,
                                     const std::string& entryPath = "");

} // namespace jtml
