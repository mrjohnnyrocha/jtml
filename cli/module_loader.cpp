#include "module_loader.h"

#include "util.h"

#include "jtml/module_resolver.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace jtml::cli {
namespace {

std::string trimCopy(std::string s) {
    auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

bool startsWithWord(const std::string& text, const std::string& word) {
    if (text.rfind(word, 0) != 0) return false;
    return text.size() == word.size() ||
           std::isspace(static_cast<unsigned char>(text[word.size()]));
}

std::string stripLineComment(const std::string& line) {
    char quote = '\0';
    int braceDepth = 0;
    for (size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (quote != '\0') {
            if (ch == '\\' && i + 1 < line.size()) ++i;
            else if (ch == quote) quote = '\0';
            continue;
        }
        if (ch == '"' || ch == '\'') {
            quote = ch;
            continue;
        }
        if (ch == '{') ++braceDepth;
        if (ch == '}' && braceDepth > 0) --braceDepth;
        if (braceDepth == 0 && ch == '/' && i + 1 < line.size() && line[i + 1] == '/') {
            return line.substr(0, i);
        }
        if (braceDepth == 0 && ch == '#' &&
            (i == 0 || std::isspace(static_cast<unsigned char>(line[i - 1])))) {
            return line.substr(0, i);
        }
    }
    return line;
}

bool firstQuotedLiteralAfter(const std::string& text, size_t start, std::string& value) {
    for (size_t i = start; i < text.size(); ++i) {
        if (text[i] != '"' && text[i] != '\'') continue;
        const char quote = text[i++];
        std::string out;
        for (; i < text.size(); ++i) {
            const char ch = text[i];
            if (ch == '\\' && i + 1 < text.size()) {
                out.push_back(text[++i]);
                continue;
            }
            if (ch == quote) {
                value = out;
                return true;
            }
            out.push_back(ch);
        }
        return false;
    }
    return false;
}

bool parseImportLine(const std::string& line, std::string& path) {
    std::string text = trimCopy(line);
    const std::string keyword = "import";
    if (!startsWithWord(text, keyword)) return false;

    size_t i = keyword.size();
    while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
    if (i >= text.size() || (text[i] != '"' && text[i] != '\'')) return false;

    const char quote = text[i++];
    std::string value;
    for (; i < text.size(); ++i) {
        char ch = text[i];
        if (ch == '\\' && i + 1 < text.size() && text[i + 1] != '\\') {
            value.push_back(text[++i]);
            continue;
        }
        if (ch == quote) {
            path = value;
            return true;
        }
        value.push_back(ch);
    }
    return false;
}

struct FriendlyUseSpec {
    std::string path;
    std::vector<std::string> names;
    bool sideEffect = false;
};

std::string identifierFromToken(std::string token) {
    token.erase(std::remove_if(token.begin(), token.end(), [](unsigned char ch) {
        return !(std::isalnum(ch) || ch == '_');
    }), token.end());
    return token;
}

bool parseFriendlyUseLine(const std::string& line, FriendlyUseSpec& spec) {
    const std::string text = trimCopy(stripLineComment(line));
    const std::string keyword = "use";
    if (!startsWithWord(text, keyword)) return false;

    size_t i = keyword.size();
    while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
    if (i < text.size() && (text[i] == '"' || text[i] == '\'')) {
        spec.sideEffect = true;
        return firstQuotedLiteralAfter(text, i, spec.path);
    }

    const std::string from = " from ";
    const auto fromPos = text.find(from, i);
    if (fromPos == std::string::npos) return false;
    if (!firstQuotedLiteralAfter(text, fromPos + from.size(), spec.path)) return false;

    std::string imported = trimCopy(text.substr(i, fromPos - i));
    if (!imported.empty() && imported.front() == '{') {
        if (!imported.empty() && imported.back() == '}') {
            imported = imported.substr(1, imported.size() - 2);
        }
        std::stringstream ss(imported);
        std::string item;
        while (std::getline(ss, item, ',')) {
            item = identifierFromToken(trimCopy(item));
            if (!item.empty()) spec.names.push_back(item);
        }
    } else {
        imported = identifierFromToken(imported);
        if (!imported.empty()) spec.names.push_back(imported);
    }
    return !spec.names.empty();
}

int leadingSpaces(const std::string& line) {
    int count = 0;
    for (char ch : line) {
        if (ch == ' ') ++count;
        else if (ch == '\t') count += 2;
        else break;
    }
    return count;
}

std::string exportedSymbolName(const std::string& strippedExportLine) {
    std::istringstream in(strippedExportLine);
    std::string keyword;
    in >> keyword;
    if (keyword == "let" || keyword == "const" || keyword == "get" ||
        keyword == "when" || keyword == "make" || keyword == "store" ||
        keyword == "route" || keyword == "extern") {
        std::string name;
        in >> name;
        if (keyword == "route") {
            std::string token;
            while (in >> token) {
                if (token == "as" && (in >> name)) break;
            }
        }
        const auto colon = name.find(':');
        if (colon != std::string::npos) name = name.substr(0, colon);
        return identifierFromToken(name);
    }
    return {};
}

bool isPrivateImplementationDeclaration(const std::string& text) {
    std::istringstream in(text);
    std::string keyword;
    in >> keyword;
    return keyword == "let" || keyword == "const" || keyword == "get" ||
           keyword == "when" || keyword == "effect" || keyword == "extern";
}

std::string joinNames(const std::vector<std::string>& names) {
    std::ostringstream out;
    for (size_t i = 0; i < names.size(); ++i) {
        if (i > 0) out << ", ";
        out << names[i];
    }
    return out.str();
}

std::vector<std::string> readLines(const std::string& source) {
    std::vector<std::string> lines;
    std::istringstream in(source);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

std::filesystem::path normalizePath(const std::filesystem::path& path) {
    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(path, ec);
    if (!ec) return canonical;
    return std::filesystem::absolute(path).lexically_normal();
}

std::runtime_error importResolutionError(const std::string& specifier,
                                         const std::filesystem::path& importer,
                                         const std::filesystem::path& resolved,
                                         const std::exception& cause) {
    std::ostringstream oss;
    oss << "Cannot resolve import '" << specifier << "' from "
        << importer.string() << " (attempted " << resolved.string()
        << "): " << cause.what();
    return std::runtime_error(oss.str());
}

struct FriendlyLoad {
    std::string classicPrefix;
    std::string friendlySource;
};

struct ExportDecl {
    std::string symbol;
    std::string strippedLine;
    FriendlyUseSpec reexport;
    bool isReexport = false;
};

std::string moduleVisitKey(const std::filesystem::path& file,
                           const std::vector<std::string>& selectedExports = {}) {
    std::vector<std::string> names = selectedExports;
    std::sort(names.begin(), names.end());
    std::ostringstream key;
    key << file.string() << "|";
    if (names.empty()) {
        key << "*";
    } else {
        for (const auto& name : names) key << name << ",";
    }
    return key.str();
}

FriendlyLoad loadFriendlyCompilationUnit(const std::filesystem::path& inputFile,
                                         std::set<std::string>& visited,
                                         std::set<std::string>& privateImplEmitted,
                                         std::vector<std::filesystem::path>& stack,
                                         std::vector<std::filesystem::path>& files,
                                         const std::vector<std::string>& selectedExports = {});

std::map<std::string, ExportDecl> collectExports(const std::filesystem::path& file,
                                                 const std::vector<std::string>& lines) {
    std::map<std::string, ExportDecl> exports;
    for (const auto& raw : lines) {
        const std::string text = trimCopy(stripLineComment(raw));
        if (leadingSpaces(raw) != 0 || !startsWithWord(text, "export")) continue;

        const auto exportPos = raw.find("export");
        const std::string stripped =
            trimCopy(raw.substr(exportPos + std::string("export").size()));

        FriendlyUseSpec reexport;
        if (parseFriendlyUseLine(stripped, reexport)) {
            if (reexport.sideEffect) {
                throw std::runtime_error("Unsupported re-export in " + file.string() +
                                         ": " + text);
            }
            for (const auto& name : reexport.names) {
                ExportDecl decl;
                decl.symbol = name;
                decl.reexport = reexport;
                decl.isReexport = true;
                auto [it, inserted] = exports.emplace(name, decl);
                if (!inserted) {
                    throw std::runtime_error("Duplicate exported symbol '" + name +
                                             "' in " + file.string());
                }
            }
            continue;
        }

        const std::string symbol = exportedSymbolName(stripped);
        if (symbol.empty()) {
            throw std::runtime_error("Unsupported export declaration in " + file.string() +
                                     ": " + text);
        }
        ExportDecl decl;
        decl.symbol = symbol;
        decl.strippedLine = stripped;
        auto [it, inserted] = exports.emplace(symbol, decl);
        if (!inserted) {
            throw std::runtime_error("Duplicate exported symbol '" + symbol + "' in " +
                                     file.string());
        }
    }
    return exports;
}

FriendlyLoad loadFriendlyDependency(const std::filesystem::path& inputFile,
                                    std::set<std::string>& visited,
                                    std::set<std::string>& privateImplEmitted,
                                    std::vector<std::filesystem::path>& stack,
                                    std::vector<std::filesystem::path>& files,
                                    const std::vector<std::string>& selectedExports = {}) {
    const auto file = normalizePath(inputFile);
    if (std::find(stack.begin(), stack.end(), file) != stack.end()) {
        std::ostringstream cycle;
        cycle << "Import cycle detected:";
        for (const auto& item : stack) cycle << " " << item.string() << " ->";
        cycle << " " << file.string();
        throw std::runtime_error(cycle.str());
    }
    const std::string key = moduleVisitKey(file, selectedExports);
    if (visited.count(key)) return {};
    std::string source = readFile(file.string());
    if (!isFriendlySyntax(source) && !looksLikeFriendlySyntax(source)) {
        visited.insert(key);
        files.push_back(file);
        return {normalizeSourceSyntax(source, SyntaxMode::Auto), ""};
    }

    visited.insert(key);
    files.push_back(file);
    stack.push_back(file);
    FriendlyLoad loaded = loadFriendlyCompilationUnit(file, visited, privateImplEmitted,
                                                      stack, files, selectedExports);
    stack.pop_back();
    return loaded;
}

std::string loadCompilationUnit(const std::filesystem::path& inputFile,
                                SyntaxMode syntax,
                                std::set<std::string>& visited,
                                std::set<std::string>& privateImplEmitted,
                                std::vector<std::filesystem::path>& stack,
                                std::vector<std::filesystem::path>& files) {
    const auto file = normalizePath(inputFile);
    if (std::find(stack.begin(), stack.end(), file) != stack.end()) {
        std::ostringstream cycle;
        cycle << "Import cycle detected:";
        for (const auto& item : stack) cycle << " " << item.string() << " ->";
        cycle << " " << file.string();
        throw std::runtime_error(cycle.str());
    }
    const std::string key = moduleVisitKey(file);
    if (visited.count(key)) return "";
    visited.insert(key);
    files.push_back(file);
    stack.push_back(file);

    std::string rawSource = readFile(file.string());
    const bool friendly =
        syntax == SyntaxMode::Friendly ||
        (syntax == SyntaxMode::Auto &&
         (isFriendlySyntax(rawSource) || looksLikeFriendlySyntax(rawSource)));
    if (friendly) {
        FriendlyLoad loaded = loadFriendlyCompilationUnit(file, visited, privateImplEmitted,
                                                          stack, files);
        stack.pop_back();
        return loaded.classicPrefix +
               normalizeSourceSyntax(loaded.friendlySource, SyntaxMode::Friendly);
    }

    std::string source = normalizeSourceSyntax(rawSource, syntax);
    std::istringstream in(source);
    std::ostringstream out;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::string importPath;
        if (parseImportLine(line, importPath)) {
            auto resolved = resolveJtmlModulePath(importPath, file);
            try {
                out << loadCompilationUnit(resolved, SyntaxMode::Auto, visited,
                                           privateImplEmitted, stack, files);
            } catch (const std::exception& e) {
                throw importResolutionError(importPath, file, resolved, e);
            }
            continue;
        }
        out << line << "\n";
    }

    stack.pop_back();
    return out.str();
}

void validateNamedImport(const std::filesystem::path& file,
                         const std::map<std::string, ExportDecl>& exports,
                         const std::vector<std::string>& selectedExports) {
    if (selectedExports.empty()) return;
    if (exports.empty()) {
        throw std::runtime_error("Named import from " + file.string() +
                                 " requires exported declarations; requested: " +
                                 joinNames(selectedExports));
    }
    std::vector<std::string> missing;
    for (const auto& name : selectedExports) {
        if (!exports.count(name)) missing.push_back(name);
    }
    if (!missing.empty()) {
        std::vector<std::string> available;
        for (const auto& [name, decl] : exports) {
            (void)decl;
            available.push_back(name);
        }
        throw std::runtime_error("Missing export(s) from " + file.string() + ": " +
                                 joinNames(missing) + ". Available exports: " +
                                 joinNames(available));
    }
}

void collectSourceFilesRecoverable(const std::filesystem::path& inputFile,
                                   SyntaxMode syntax,
                                   std::set<std::string>& visited,
                                   std::vector<std::filesystem::path>& files) {
    const auto file = normalizePath(inputFile);
    const auto key = file.string();
    if (!visited.insert(key).second) return;
    files.push_back(file);

    std::string source;
    try {
        source = readFile(file.string());
    } catch (...) {
        return;
    }

    const bool friendly =
        syntax == SyntaxMode::Friendly ||
        (syntax == SyntaxMode::Auto &&
         (isFriendlySyntax(source) || looksLikeFriendlySyntax(source)));
    const auto lines = readLines(source);
    for (const auto& rawLine : lines) {
        std::string specifier;
        if (friendly) {
            FriendlyUseSpec use;
            if (parseFriendlyUseLine(rawLine, use)) {
                specifier = use.path;
            } else {
                const std::string text = trimCopy(stripLineComment(rawLine));
                if (startsWithWord(text, "export")) {
                    const auto exportPos = rawLine.find("export");
                    const std::string stripped = exportPos == std::string::npos
                        ? ""
                        : trimCopy(rawLine.substr(exportPos + std::string("export").size()));
                    if (parseFriendlyUseLine(stripped, use)) specifier = use.path;
                }
            }
        } else {
            parseImportLine(rawLine, specifier);
        }
        if (specifier.empty()) continue;

        const auto resolved = resolveJtmlModulePath(specifier, file);
        collectSourceFilesRecoverable(resolved, SyntaxMode::Auto, visited, files);
    }
}

FriendlyLoad loadReexport(const FriendlyUseSpec& use,
                          const std::vector<std::string>& selectedNames,
                          const std::filesystem::path& importer,
                          std::set<std::string>& visited,
                          std::set<std::string>& privateImplEmitted,
                          std::vector<std::filesystem::path>& stack,
                          std::vector<std::filesystem::path>& files) {
    auto resolved = resolveJtmlModulePath(use.path, importer);
    try {
        return loadFriendlyDependency(resolved, visited, privateImplEmitted,
                                      stack, files, selectedNames);
    } catch (const std::exception& e) {
        throw importResolutionError(use.path, importer, resolved, e);
    }
}

FriendlyLoad loadFriendlyCompilationUnit(const std::filesystem::path& file,
                                         std::set<std::string>& visited,
                                         std::set<std::string>& privateImplEmitted,
                                         std::vector<std::filesystem::path>& stack,
                                         std::vector<std::filesystem::path>& files,
                                         const std::vector<std::string>& selectedExports) {
    const std::string source = readFile(file.string());
    const auto sourceLines = readLines(source);
    const bool namedImport = !selectedExports.empty();
    const auto exports = collectExports(file, sourceLines);
    validateNamedImport(file, exports, selectedExports);

    const bool moduleUsesExports = !exports.empty();
    const std::set<std::string> selected(selectedExports.begin(), selectedExports.end());

    FriendlyLoad result;
    std::ostringstream friendly;
    bool includingSelectedExportBlock = false;
    int selectedExportIndent = -1;
    std::set<std::string> importedNamedSymbols;
    std::set<std::string> emittedReexports;
    const bool emitPrivateImpl = privateImplEmitted.insert(file.string()).second;

    for (size_t i = 0; i < sourceLines.size(); ++i) {
        std::string line = sourceLines[i];
        const std::string text = trimCopy(stripLineComment(line));
        if (text == "jtml 2" || text == "jtl 1") continue;

        FriendlyUseSpec use;
        if (parseFriendlyUseLine(line, use)) {
            if (!use.sideEffect) {
                for (const auto& name : use.names) {
                    if (!importedNamedSymbols.insert(name).second) {
                        throw std::runtime_error("Duplicate named import '" + name +
                                                 "' in " + file.string());
                    }
                }
            }
            auto resolved = resolveJtmlModulePath(use.path, file);
            FriendlyLoad dependency;
            try {
                dependency = loadFriendlyDependency(
                    resolved, visited, privateImplEmitted, stack, files,
                    use.sideEffect ? std::vector<std::string>{} : use.names);
            } catch (const std::exception& e) {
                throw importResolutionError(use.path, file, resolved, e);
            }
            result.classicPrefix += dependency.classicPrefix;
            friendly << dependency.friendlySource;
            continue;
        }

        if (namedImport && moduleUsesExports) {
            const int indent = leadingSpaces(line);
            if (includingSelectedExportBlock && indent > selectedExportIndent) {
                friendly << line << "\n";
                continue;
            }
            includingSelectedExportBlock = false;
            selectedExportIndent = -1;

            if (indent != 0) continue;
            if (!startsWithWord(text, "export")) {
                if (isPrivateImplementationDeclaration(text)) {
                    if (emitPrivateImpl) friendly << line << "\n";
                    while (i + 1 < sourceLines.size() &&
                           leadingSpaces(sourceLines[i + 1]) > indent) {
                        ++i;
                        if (emitPrivateImpl) friendly << sourceLines[i] << "\n";
                    }
                    continue;
                }
                while (i + 1 < sourceLines.size() &&
                       leadingSpaces(sourceLines[i + 1]) > indent) {
                    ++i;
                }
                continue;
            }

            const auto exportPos = line.find("export");
            const std::string stripped =
                trimCopy(line.substr(exportPos + std::string("export").size()));
            FriendlyUseSpec reexport;
            if (parseFriendlyUseLine(stripped, reexport)) {
                std::vector<std::string> wanted;
                for (const auto& name : reexport.names) {
                    if (selected.count(name)) wanted.push_back(name);
                }
                if (!wanted.empty()) {
                    const std::string key = reexport.path + "|" + joinNames(wanted);
                    if (emittedReexports.insert(key).second) {
                        FriendlyLoad dependency = loadReexport(
                            reexport, wanted, file, visited, privateImplEmitted,
                            stack, files);
                        result.classicPrefix += dependency.classicPrefix;
                        friendly << dependency.friendlySource;
                    }
                }
                continue;
            }

            const std::string symbol = exportedSymbolName(stripped);
            if (!selected.count(symbol)) {
                while (i + 1 < sourceLines.size() &&
                       leadingSpaces(sourceLines[i + 1]) > indent) {
                    ++i;
                }
                continue;
            }
            line = std::string(static_cast<size_t>(indent), ' ') + stripped;
            includingSelectedExportBlock = true;
            selectedExportIndent = indent;
        } else if (startsWithWord(text, "export")) {
            const int indent = leadingSpaces(line);
            const auto exportPos = line.find("export");
            const std::string stripped =
                trimCopy(line.substr(exportPos + std::string("export").size()));
            FriendlyUseSpec reexport;
            if (parseFriendlyUseLine(stripped, reexport)) {
                FriendlyLoad dependency = loadReexport(
                    reexport, reexport.names, file, visited, privateImplEmitted,
                    stack, files);
                result.classicPrefix += dependency.classicPrefix;
                friendly << dependency.friendlySource;
                continue;
            }
            line = std::string(static_cast<size_t>(indent), ' ') + stripped;
        }

        friendly << line << "\n";
    }
    result.friendlySource = friendly.str();
    return result;
}

} // namespace

std::string loadCompilationUnit(const std::string& inputFile,
                                SyntaxMode syntax,
                                std::vector<std::filesystem::path>* filesOut) {
    std::set<std::string> visited;
    std::set<std::string> privateImplEmitted;
    std::vector<std::filesystem::path> stack;
    std::vector<std::filesystem::path> files;
    std::string source = loadCompilationUnit(inputFile, syntax, visited,
                                             privateImplEmitted, stack, files);
    if (filesOut) *filesOut = std::move(files);
    return source;
}

std::vector<std::filesystem::path>
collectSourceFiles(const std::string& inputFile, SyntaxMode syntax) {
    std::vector<std::filesystem::path> files;
    (void)loadCompilationUnit(inputFile, syntax, &files);
    return files;
}

std::vector<std::filesystem::path>
collectSourceFilesRecoverable(const std::string& inputFile, SyntaxMode syntax) {
    std::set<std::string> visited;
    std::vector<std::filesystem::path> files;
    collectSourceFilesRecoverable(std::filesystem::path(inputFile), syntax, visited, files);
    return files;
}

} // namespace jtml::cli
