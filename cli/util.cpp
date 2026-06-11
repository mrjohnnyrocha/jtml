// cli/util.cpp — see util.h for the contract.
#include "util.h"

#include <cstdlib>
#include <fstream>
#include <iostream>

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
        << "  jtml transpile <file.jtml> -o out.html [--target browser]\n"
        << "  jtml build    <file.jtml|dir/> --out <dist> [--target browser]\n"
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
        << "  jtml keywords [--json] print the canonical Friendly keyword catalog\n"
        << "  jtml ui [--json]       print the semantic UI primitive catalog\n"
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
