#pragma once

#include "jtml/friendly.h"
#include "jtml/source_origin.h"

#include <filesystem>
#include <string>
#include <vector>

namespace jtml::cli {

// Source selected/linked by the existing loader, before Friendly lowering.
// Classic imports from Friendly modules are held separately, as before.
struct CompilationSource {
    MappedSource classicPrefix;
    MappedSource source;
    SyntaxMode syntax = SyntaxMode::Classic;
};
CompilationSource loadCompilationSource(const std::string& inputFile,
                                       SyntaxMode syntax,
                                       std::vector<std::filesystem::path>* filesOut = nullptr);

std::string loadCompilationUnit(const std::string& inputFile,
                                SyntaxMode syntax,
                                std::vector<std::filesystem::path>* filesOut = nullptr);

std::vector<std::filesystem::path>
collectSourceFiles(const std::string& inputFile, SyntaxMode syntax);

std::vector<std::filesystem::path>
collectSourceFilesRecoverable(const std::string& inputFile, SyntaxMode syntax);

} // namespace jtml::cli
