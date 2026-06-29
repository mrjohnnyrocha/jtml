#include "jtml/browser_runtime_emitter.h"

#include "jtml/browser_runtime_assets.h"

#include <cstring>
#include <string>

namespace jtml {
namespace {

void replaceAll(std::string& value, const std::string& needle, const std::string& replacement) {
    if (needle.empty()) return;
    std::size_t pos = 0;
    while ((pos = value.find(needle, pos)) != std::string::npos) {
        value.replace(pos, needle.size(), replacement);
        pos += replacement.size();
    }
}

} // namespace

std::string emitBrowserRuntimeScript(int webSocketPort, bool browserLocalRuntime) {
    // The browser runtime is authored as owned asset chunks instead of one
    // giant emitter literal. Keep this function as the stable C++ API that
    // parameterizes the runtime for live WebSocket and browser-local modes.
    const auto chunks = browserRuntimeAssetChunks();
    std::size_t size = 0;
    for (const auto& chunk : chunks) {
        size += std::strlen(chunk.source);
    }
    std::string script;
    script.reserve(size);
    for (const auto& chunk : chunks) {
        script += chunk.source;
    }
    replaceAll(script, "@JTML_WS_PORT@", std::to_string(webSocketPort));
    replaceAll(script, "@JTML_BROWSER_LOCAL_RUNTIME@", browserLocalRuntime ? "true" : "false");
    return script;
}

} // namespace jtml
