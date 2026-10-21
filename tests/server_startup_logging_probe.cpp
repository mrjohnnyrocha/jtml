#include "cli/commands.h"
#include "jtml/local_server_security.h"
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <streambuf>
#include <string>
#include <atomic>

// Audit the actual CLI banner boundary. Exit from the child at its final flush;
// HTTP/WS readiness and normal shutdown remain separate integration checks.
class BannerAudit final : public std::streambuf {
    std::atomic<bool> sawBanner{false};
    std::streamsize xsputn(const char* text, std::streamsize size) override {
        const std::string chunk(text, static_cast<size_t>(size));
        if (chunk.find("jtml serve") != std::string::npos || chunk.find("JTML Studio:") != std::string::npos) {
            sawBanner = true;
            if (!JTMLInterpreter::LocalServerExecutionMutex::heldByCurrentThread()) {
                std::fputs("FAIL startup banner bypasses runtime execution lock\n", stderr);
                std::_Exit(86);
            }
        }
        return size;
    }
    int overflow(int value) override { return traits_type::not_eof(value); }
    int sync() override {
        if (sawBanner && JTMLInterpreter::LocalServerExecutionMutex::heldByCurrentThread()) {
            std::fputs("PASS startup banner owns runtime execution lock through flush\n", stderr);
            std::_Exit(0);
        }
        return 0;
    }
};
int main(int argc, char** argv) {
    if (argc != 4) return 2;
    BannerAudit audit;
    std::cout.rdbuf(&audit);
    jtml::cli::Options options;
    options.port = std::stoi(argv[2]);
    options.inputFile = argv[3];
    if (std::string(argv[1]) == "studio") jtml::cli::cmdStudio(options);
    else {
        options.watch = std::string(argv[1]) == "watch";
        jtml::cli::cmdServe(options);
    }
    return 3; // reaching here is not the required audited boundary
}
