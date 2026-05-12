// cli/main.cpp — entry point for the `jtml` binary.
//
// Parses arguments and dispatches to the appropriate command handler.
// Every subcommand lives in its own translation unit under cli/.
#include "commands.h"

#include <cstring>
#include <iostream>

using namespace jtml::cli;

namespace {

bool commandTakesNoInputFile(const std::string& cmd) {
    return cmd == "examples" || cmd == "doctor"  || cmd == "demo"     ||
           cmd == "tutorial" || cmd == "studio"  || cmd == "test"     ||
           cmd == "install"  || cmd == "lsp";
}

jtml::SyntaxMode parseSyntaxMode(const char* value) {
    std::string mode = value;
    if (mode == "auto")     return jtml::SyntaxMode::Auto;
    if (mode == "classic")  return jtml::SyntaxMode::Classic;
    if (mode == "friendly") return jtml::SyntaxMode::Friendly;
    usage();
}

Options parseArgs(int argc, char* argv[]) {
    if (argc < 2) usage();

    Options o;
    o.command = argv[1];

    // `new` treats the second positional as the OUTPUT file.
    // All other file-taking commands treat it as INPUT.
    int firstOption = 2;
    if (o.command == "new") {
        if (argc < 3) usage();
        if (std::strcmp(argv[2], "app") == 0) {
            if (argc < 4) usage();
            o.inputFile = "app";
            o.outputFile = argv[3];
            firstOption = 4;
        } else {
            o.outputFile = argv[2];
            firstOption  = 3;
        }
    } else if (o.command == "migrate") {
        // migrate treats first positional as input, optional -o for output
        if (argc < 3) usage();
        o.inputFile = argv[2];
        o.syntax = jtml::SyntaxMode::Classic; // force classic parse for migration
        firstOption = 3;
    } else if (o.command == "refactor") {
        // refactor takes a sub-subcommand: `jtml refactor <kind> <file> ...`
        if (argc < 4) usage();
        o.subcommand = argv[2];
        o.inputFile  = argv[3];
        firstOption  = 4;
    } else if (!commandTakesNoInputFile(o.command)) {
        if (argc < 3) usage();
        o.inputFile = argv[2];
        firstOption = 3;
    }

    for (int i = firstOption; i < argc; ++i) {
        if ((std::strcmp(argv[i], "-o") == 0 || std::strcmp(argv[i], "--out") == 0)
            && i + 1 < argc) {
            o.outputFile = argv[++i];
        } else if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            o.port = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--target") == 0 && i + 1 < argc) {
            o.target = argv[++i];
        } else if (std::strcmp(argv[i], "--force") == 0) {
            o.force = true;
        } else if (std::strcmp(argv[i], "--watch") == 0) {
            o.watch = true;
        } else if (std::strcmp(argv[i], "--json") == 0) {
            o.json = true;
        } else if (std::strcmp(argv[i], "--syntax") == 0 && i + 1 < argc) {
            o.syntax = parseSyntaxMode(argv[++i]);
        } else if (std::strcmp(argv[i], "--from") == 0 && i + 1 < argc) {
            o.fromName = argv[++i];
        } else if (std::strcmp(argv[i], "--to") == 0 && i + 1 < argc) {
            o.toName = argv[++i];
        } else if (std::strcmp(argv[i], "-w") == 0) {
            // `fmt -w` writes in place (same convention as gofmt).
            o.force = true;
        } else {
            usage();
        }
    }
    return o;
}

int dispatch(const Options& o) {
    // Developer workflow
    if (o.command == "new")       return cmdNew      (o);
    if (o.command == "dev")       return cmdDev      (o);
    if (o.command == "add")       return cmdAdd      (o);
    if (o.command == "install")   return cmdInstall  (o);
    if (o.command == "serve")     return cmdServe    (o);
    if (o.command == "studio")    return cmdStudio   (o);
    // Compiler pipeline
    if (o.command == "check")     return cmdCheck    (o);
    if (o.command == "fix")       return cmdFix      (o);
    if (o.command == "fmt")       return cmdFmt      (o);
    if (o.command == "lint")      return cmdLint     (o);
    if (o.command == "transpile") return cmdTranspile(o);
    if (o.command == "build")     return cmdBuild    (o);
    if (o.command == "export")    return cmdExport   (o);
    if (o.command == "migrate")   return cmdMigrate  (o);
    if (o.command == "refactor")  return cmdRefactor (o);
    // Tooling
    if (o.command == "interpret") return cmdInterpret(o);
    if (o.command == "test")      return cmdTest     (o);
    if (o.command == "examples")  return cmdExamples (o);
    if (o.command == "doctor")    return cmdDoctor   (o);
    if (o.command == "generate")  return cmdGenerate (o);
    if (o.command == "explain")   return cmdExplain  (o);
    if (o.command == "suggest")   return cmdSuggest  (o);
    if (o.command == "lsp")       return cmdLsp      (o);
    // Backward-compatible aliases
    if (o.command == "demo")      return cmdDemo     (o);
    if (o.command == "tutorial")  return cmdTutorial (o);
    usage();
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) usage();

    // Short-circuit version flags before the regular parser.
    const std::string first = argv[1];
    if (first == "--version" || first == "-v" || first == "version") {
        std::cout << "jtml " << versionString() << "\n";
        return 0;
    }

    try {
        const Options o = parseArgs(argc, argv);
        return dispatch(o);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
