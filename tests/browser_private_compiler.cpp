#include "cli/browser_build_internal.h"

#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: jtml_browser_private_compiler <source> <output-directory>\n";
        return 1;
    }

    jtml::cli::Options options;
    options.inputFile = argv[1];
    options.outputFile = argv[2];
    options.target = "browser";
    try {
        return jtml::cli::detail::buildBrowser(
            options, jtml::cli::BrowserCertification::PrivateImportAuthority);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
