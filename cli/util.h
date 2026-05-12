// cli/util.h
//
// Shared helpers for every jtml CLI command. Kept deliberately tiny so each
// command translation unit only depends on what it actually needs (ast.h,
// lexer.h, parser.h for parsing; filesystem for I/O; streambuf for the
// stdout silencer).
#pragma once

#include "jtml/ast.h"
#include "jtml/friendly.h"
#include "jtml/lexer.h"
#include "jtml/parser.h"

#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace jtml::cli {

// Structured options produced by argument parsing in main.cpp and consumed
// by individual command handlers.
struct Options {
    std::string command;
    std::string subcommand;       // optional: e.g. `refactor rename`, the second positional.
    std::string inputFile;
    std::string outputFile;
    std::string target = "html"; // export target: html|react|vue|custom-element
    std::string fromName;        // --from (used by `refactor rename`)
    std::string toName;          // --to   (used by `refactor rename`)
    bool        force  = false;  // --force / -w (fmt writes in place)
    bool        watch  = false;  // --watch
    bool        json   = false;  // --json (machine-readable output where supported)
    int         port   = 8000;   // --port; `serve` uses port+80 for WebSocket.
    SyntaxMode  syntax = SyntaxMode::Auto;
};

// I/O helpers.
std::string readFile(const std::string& path);
void        writeFile(const std::string& path, const std::string& content, bool force);

// Parse a .jtml file into an AST. Throws std::runtime_error with a
// multi-line message if the lexer or parser report any errors.
std::vector<std::unique_ptr<ASTNode>>
parseProgramFromFile(const std::string& inputFile);

std::vector<std::unique_ptr<ASTNode>>
parseProgramFromFile(const std::string& inputFile, SyntaxMode syntax);

std::vector<std::filesystem::path>
collectSourceFiles(const std::string& inputFile, SyntaxMode syntax);

// `jtml X.Y.Z[-suffix]` as a single string. Used by --version and the
// usage banner.
std::string versionString();

// Print the usage banner to stdout and exit(1).
[[noreturn]] void usage();

// RAII guard that redirects std::cout / std::clog to a throw-away buffer
// for its lifetime. Restores the original stream buffers on destruct.
// The parser and interpreter emit verbose `[DEBUG]` logging to stdout which
// is useful for development but ruins machine-readable commands like
// `fmt`, `lint`, and `check`.
struct SilenceStdout {
    std::ostringstream sink;
    std::streambuf*    savedCout;
    std::streambuf*    savedClog;
    SilenceStdout();
    ~SilenceStdout();
    SilenceStdout(const SilenceStdout&)            = delete;
    SilenceStdout& operator=(const SilenceStdout&) = delete;
};

} // namespace jtml::cli
