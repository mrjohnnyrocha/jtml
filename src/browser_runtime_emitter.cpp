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

std::string parameterizeRuntime(std::string script,
                                int webSocketPort,
                                bool browserLocalRuntime) {
    replaceAll(script, "@JTML_WS_PORT@", std::to_string(webSocketPort));
    replaceAll(script, "@JTML_BROWSER_LOCAL_RUNTIME@", browserLocalRuntime ? "true" : "false");
    return script;
}

std::string assembleRuntimeChunks() {
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
    return script;
}

} // namespace

std::string emitBrowserRuntimeScript(int webSocketPort,
                                     bool browserLocalRuntime) {
    // The browser runtime is authored as owned asset chunks instead of one
    // giant emitter literal. Keep this function as the stable C++ API that
    // parameterizes the runtime for live WebSocket and browser-local modes.
    return "<script>\n" +
        parameterizeRuntime(assembleRuntimeChunks(), webSocketPort, browserLocalRuntime) +
        "\n</script>";
}

std::string emitBrowserRuntimeExternalAsset(int webSocketPort,
                                            bool browserLocalRuntime) {
    // Split browser builds load jtml-runtime.js as a normal external script.
    // Asset chunks are authored as JavaScript, while emitBrowserRuntimeScript()
    // adds wrappers only for inline transpile/live paths.
    return parameterizeRuntime(assembleRuntimeChunks(), webSocketPort, browserLocalRuntime);
}

} // namespace jtml
