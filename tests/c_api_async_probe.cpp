// Real C ABI owner replacement: pending render work dereferences the transpiler.
// Isolated by CTest with a hard timeout, also exercised under ASan/UBSan/TSan.
#include "jtml/c_api.h"
#include "json.hpp"
#include <memory>
#include <stdexcept>
#include <string>
#include <iostream>

static void load(jtml_context* context, const std::string& source) {
    char* html = nullptr;
    char* error = nullptr;
    const bool ok = jtml_load(context, source.c_str(), &html, &error) == 1;
    const std::string message = error ? error : "load failed";
    jtml_free(html);
    jtml_free(error);
    if (!ok) throw std::runtime_error(message);
}
int main() {
    try {
        std::unique_ptr<jtml_context, decltype(&jtml_destroy)> context(jtml_create(), jtml_destroy);
        std::string pending = R"JTML(define revision = 0\\
async function paint()\\
    element p\\
        show revision\\
    #
\\
)JTML";
        for (int i = 0; i < 64; ++i) pending += "paint()\\\\\n";
        // This exercises admitted work across repeated replacement, including
        // tasks not yet running. Safety cannot depend on how quickly they run.
        for (int round = 1; round <= 8; ++round) {
            load(context.get(), pending);
            load(context.get(), "define revision = " + std::to_string(round) + "\\\\\n");
            char* raw = jtml_state(context.get());
            const auto state = nlohmann::json::parse(raw);
            jtml_free(raw);
            if (state["variables"]["revision"]["value"] != round)
                throw std::runtime_error("C API replacement did not publish fresh state");
        }
        load(context.get(), "async function fail()\\\\\n    missing()\\\\\n\\\\\nfail()\\\\\n");
        char* html = nullptr;
        char* error = nullptr;
        const int ok = jtml_load(context.get(), "define revision = 99\\\\\n", &html, &error);
        const std::string diagnostic = error ? error : "";
        jtml_free(html);
        jtml_free(error);
        if (ok != 0 || diagnostic.find("Async interpreter failures") == std::string::npos)
            throw std::runtime_error("C API replacement swallowed a worker failure");
        load(context.get(), pending); // reported failures are consumed once
        context.reset(); // pending render work must also drain before destruction
        std::cout << "PASS C API async transpiler lifetime across load/destroy\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
