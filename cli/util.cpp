// cli/util.cpp — see util.h for the contract.
#include "util.h"

#include "jtml/module_resolver.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>

#ifndef JTML_VERSION_STRING
#define JTML_VERSION_STRING "0.0.0"
#endif
#ifndef JTML_VERSION_SUFFIX_STRING
#define JTML_VERSION_SUFFIX_STRING ""
#endif

namespace jtml::cli {

std::string readFile(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

namespace {

std::string trimCopy(std::string s) {
    auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

bool parseImportLine(const std::string& line, std::string& path) {
    std::string text = trimCopy(line);
    const std::string keyword = "import";
    if (text.rfind(keyword, 0) != 0) return false;
    if (text.size() == keyword.size() ||
        !std::isspace(static_cast<unsigned char>(text[keyword.size()]))) {
        return false;
    }

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
    if (text.rfind(keyword, 0) != 0) return false;
    if (text.size() == keyword.size() ||
        !std::isspace(static_cast<unsigned char>(text[keyword.size()]))) {
        return false;
    }

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

bool parseFriendlyUseLine(const std::string& line, std::string& path) {
    FriendlyUseSpec spec;
    if (!parseFriendlyUseLine(line, spec)) return false;
    path = spec.path;
    return true;
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

bool startsWithWord(const std::string& text, const std::string& word) {
    if (text.rfind(word, 0) != 0) return false;
    return text.size() == word.size() ||
           std::isspace(static_cast<unsigned char>(text[word.size()]));
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

struct FriendlyLoad {
    std::string classicPrefix;
    std::string friendlySource;
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
                                         std::vector<std::filesystem::path>& stack,
                                         std::vector<std::filesystem::path>& files,
                                         const std::vector<std::string>& selectedExports = {});

FriendlyLoad loadFriendlyDependency(const std::filesystem::path& inputFile,
                                    std::set<std::string>& visited,
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
    FriendlyLoad loaded = loadFriendlyCompilationUnit(file, visited, stack, files, selectedExports);
    stack.pop_back();
    return loaded;
}

std::string loadCompilationUnit(const std::filesystem::path& inputFile,
                                SyntaxMode syntax,
                                std::set<std::string>& visited,
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
        FriendlyLoad loaded = loadFriendlyCompilationUnit(file, visited, stack, files);
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
            out << loadCompilationUnit(resolved, SyntaxMode::Auto, visited, stack, files);
            continue;
        }
        out << line << "\n";
    }

    stack.pop_back();
    return out.str();
}

FriendlyLoad loadFriendlyCompilationUnit(const std::filesystem::path& file,
                                         std::set<std::string>& visited,
                                         std::vector<std::filesystem::path>& stack,
                                         std::vector<std::filesystem::path>& files,
                                         const std::vector<std::string>& selectedExports) {
    const std::string source = readFile(file.string());
    const auto sourceLines = readLines(source);
    const bool explicitImport = !selectedExports.empty();
    bool moduleUsesExports = false;
    for (const auto& raw : sourceLines) {
        const std::string text = trimCopy(stripLineComment(raw));
        if (leadingSpaces(raw) == 0 && startsWithWord(text, "export")) {
            moduleUsesExports = true;
            break;
        }
    }
    const std::set<std::string> selected(selectedExports.begin(), selectedExports.end());

    FriendlyLoad result;
    std::ostringstream friendly;
    bool includingSelectedExportBlock = false;
    int selectedExportIndent = -1;
    for (size_t i = 0; i < sourceLines.size(); ++i) {
        std::string line = sourceLines[i];
        const std::string text = trimCopy(stripLineComment(line));
        if (text == "jtml 2") continue;

        FriendlyUseSpec use;
        if (parseFriendlyUseLine(line, use)) {
            auto resolved = resolveJtmlModulePath(use.path, file);
            FriendlyLoad dependency = loadFriendlyDependency(
                resolved, visited, stack, files,
                use.sideEffect ? std::vector<std::string>{} : use.names);
            result.classicPrefix += dependency.classicPrefix;
            friendly << dependency.friendlySource;
            continue;
        }

        if (explicitImport && moduleUsesExports) {
            const int indent = leadingSpaces(line);
            if (includingSelectedExportBlock && indent > selectedExportIndent) {
                friendly << line << "\n";
                continue;
            }
            includingSelectedExportBlock = false;
            selectedExportIndent = -1;

            if (indent != 0) continue;
            if (!startsWithWord(text, "export")) {
                while (i + 1 < sourceLines.size() &&
                       leadingSpaces(sourceLines[i + 1]) > indent) {
                    ++i;
                }
                continue;
            }

            const auto exportPos = line.find("export");
            const std::string stripped =
                trimCopy(line.substr(exportPos + std::string("export").size()));
            const std::string symbol = exportedSymbolName(stripped);
            const bool selectedSymbol = selected.count(symbol) > 0;
            if (!selectedSymbol) {
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
            line = std::string(static_cast<size_t>(indent), ' ') +
                   trimCopy(line.substr(exportPos + std::string("export").size()));
        }

        friendly << line << "\n";
    }
    result.friendlySource = friendly.str();
    return result;
}

std::string loadCompilationUnit(const std::string& inputFile,
                                SyntaxMode syntax,
                                std::vector<std::filesystem::path>* filesOut) {
    std::set<std::string> visited;
    std::vector<std::filesystem::path> stack;
    std::vector<std::filesystem::path> files;
    std::string source = loadCompilationUnit(inputFile, syntax, visited, stack, files);
    if (filesOut) *filesOut = std::move(files);
    return source;
}

} // namespace

void writeFile(const std::string& path, const std::string& content, bool force) {
    if (!force && std::filesystem::exists(path)) {
        throw std::runtime_error(
            "Refusing to overwrite existing file: " + path + " (use --force)");
    }
    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        throw std::runtime_error("Cannot write file: " + path);
    }
    ofs << content;
}

std::vector<std::unique_ptr<ASTNode>>
parseProgramFromFile(const std::string& inputFile) {
    return parseProgramFromFile(inputFile, SyntaxMode::Auto);
}

std::vector<std::unique_ptr<ASTNode>>
parseProgramFromFile(const std::string& inputFile, SyntaxMode syntax) {
    std::string inputText = loadCompilationUnit(inputFile, syntax, nullptr);

    Lexer lexer(inputText);
    auto tokens = lexer.tokenize();

    const auto& lexErrors = lexer.getErrors();
    if (!lexErrors.empty()) {
        std::ostringstream oss;
        for (const auto& e : lexErrors) oss << "Lexer Error: " << e << "\n";
        throw std::runtime_error(oss.str());
    }

    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    const auto& parseErrors = parser.getErrors();
    if (!parseErrors.empty()) {
        std::ostringstream oss;
        for (const auto& e : parseErrors) oss << "Parser Error: " << e << "\n";
        throw std::runtime_error(oss.str());
    }
    return program;
}

std::vector<std::filesystem::path>
collectSourceFiles(const std::string& inputFile, SyntaxMode syntax) {
    std::vector<std::filesystem::path> files;
    (void)loadCompilationUnit(inputFile, syntax, &files);
    return files;
}

std::string versionString() {
    std::string v = JTML_VERSION_STRING;
    std::string suffix = JTML_VERSION_SUFFIX_STRING;
    if (!suffix.empty()) v += "-" + suffix;
    return v;
}

[[noreturn]] void usage() {
    std::cout
        << "jtml " << versionString() << " — tiny reactive HTML language\n"
        << "\n"
        << "DEVELOP\n"
        << "  jtml new <file.jtml>                   scaffold a new page\n"
        << "  jtml new app <dir>                     scaffold a modular app\n"
        << "  jtml add <path|name> [--from path]     install a local JTML package\n"
        << "  jtml install [--json]                  restore/verify package lockfile\n"
        << "  jtml dev <file.jtml|dir/> [--port N]   zero-config hot-reload server\n"
        << "  jtml serve <file.jtml> [--port N]      live server\n"
        << "  jtml serve <file.jtml> --watch          hot-reload on save\n"
        << "  jtml studio [--port N]                 interactive IDE + tutorial\n"
        << "\n"
        << "COMPILE\n"
        << "  jtml check    <file.jtml>              parse and report errors\n"
        << "  jtml fix      <file.jtml> [-w]         apply safe mechanical repairs\n"
        << "  jtml fmt      <file.jtml> [-w]         format source\n"
        << "  jtml lint     <file.jtml>              lint for common mistakes\n"
        << "  jtml transpile <file.jtml> -o out.html compile to static HTML\n"
        << "  jtml build    <file.jtml|dir/> --out <dist>\n"
        << "  jtml export   <file.jtml> --target html|react|vue|custom-element -o out\n"
        << "  jtml migrate  <file.jtml> [-o out.jtml] classic → friendly syntax\n"
        << "  jtml refactor rename <file.jtml|dir/> --from <name> --to <newname> [-w] [--json]\n"
        << "                                          (directory = workspace-wide rename)\n"
        << "\n"
        << "  All compile commands accept --syntax auto|classic|friendly\n"
        << "  check, fix, lint, and doctor accept --json\n"
        << "\n"
        << "TOOLING\n"
        << "  jtml test              smoke-test bundled examples\n"
        << "  jtml examples          list bundled examples\n"
        << "  jtml doctor            verify local toolkit layout\n"
        << "  jtml generate \"desc\"   print an AI-ready component starter\n"
        << "  jtml explain <file>    explain a JTML source file\n"
        << "  jtml suggest <file>    suggest production refactors\n"
        << "  jtml lsp               run the Language Server Protocol over stdio\n"
        << "  jtml interpret <file>  run in the interpreter\n"
        << "  jtml --version\n"
        << "\n"
        << "ALIASES\n"
        << "  jtml demo     →  jtml studio\n"
        << "  jtml tutorial →  jtml studio\n";
    std::exit(1);
}

SilenceStdout::SilenceStdout()
    : savedCout(std::cout.rdbuf(sink.rdbuf())),
      savedClog(std::clog.rdbuf(sink.rdbuf())) {}

SilenceStdout::~SilenceStdout() {
    std::cout.rdbuf(savedCout);
    std::clog.rdbuf(savedClog);
}

} // namespace jtml::cli
