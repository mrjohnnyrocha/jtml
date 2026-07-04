#include "cli/browser_build.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream input(path);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

} // namespace

int main() {
    try {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto root = std::filesystem::temp_directory_path() /
            ("jtml-browser-build-linkcheck-" + std::to_string(stamp));
        std::filesystem::create_directories(root);
        const auto input = root / "input.jtml";
        const auto output = root / "out.html";

        {
            std::ofstream out(input);
            out << "jtml 2\n"
                << "let count = 0\n"
                << "when add\n"
                << "  count += 1\n"
                << "page\n"
                << "  text count\n"
                << "  button \"Add\" click add\n";
        }

        jtml::cli::Options options;
        options.inputFile = input.string();
        options.outputFile = output.string();
        options.target = "browser";
        options.syntax = jtml::SyntaxMode::Auto;

        const int result = jtml::cli::cmdTranspileBrowser(options);
        if (result != 0) {
            std::cerr << "cmdTranspileBrowser returned " << result << "\n";
            return result;
        }
        const auto html = readFile(output);
        if (!contains(html, "__jtml_client_manifest") ||
            !contains(html, "const browserLocalRuntime = true;") ||
            contains(html, "<script src=\"./jtml-runtime.js\" defer></script>")) {
            std::cerr << "browser build linkcheck did not produce expected inline browser transpile output\n";
            return 2;
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "browser build linkcheck failed: " << e.what() << "\n";
        return 1;
    }
}
