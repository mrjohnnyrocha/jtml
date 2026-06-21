#pragma once

#include "jtml/friendly.h"

#include <filesystem>
#include <string>
#include <vector>

namespace jtml::cli {

std::string loadCompilationUnit(const std::string& inputFile,
                                SyntaxMode syntax,
                                std::vector<std::filesystem::path>* filesOut = nullptr);

std::vector<std::filesystem::path>
collectSourceFiles(const std::string& inputFile, SyntaxMode syntax);

std::vector<std::filesystem::path>
collectSourceFilesRecoverable(const std::string& inputFile, SyntaxMode syntax);

} // namespace jtml::cli
