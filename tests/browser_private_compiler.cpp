#include "cli/browser_build_internal.h"

#include <exception>
#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    const bool authorityOnly = argc == 4 && std::string_view(argv[3]) == "--authority-only";
    if (argc != 3 && !authorityOnly) {
        std::cerr << "Usage: jtml_browser_private_compiler <source> <output-directory> [--authority-only]\n";
        return 1;
    }

    jtml::cli::Options options;
    options.inputFile = argv[1];
    options.outputFile = argv[2];
    options.target = "browser";
    try {
        return jtml::cli::detail::buildBrowser(
            options, authorityOnly ? jtml::cli::BrowserCertification::PrivateImportAuthority
                                   : jtml::cli::BrowserCertification::PrivateStrictScalar);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
