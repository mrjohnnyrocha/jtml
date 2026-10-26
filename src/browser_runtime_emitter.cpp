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

std::string emitBrowserRootEventBindings() {
    return R"JTML(
(function () {
  // Delegation covers future conditional/loop DOM without per-node listeners.
  const names = ['onClick', 'onInput', 'onChange', 'onKeyUp', 'onKeyDown',
    'onFocus', 'onBlur', 'onDblClick', 'onMouseOver', 'onScroll', 'onSubmit', 'onDragOver', 'onDrop'];
  names.forEach(function (name) {
    const type = name.slice(2).toLowerCase(), attribute = 'data-jtml-event-' + type;
    const capture = type === 'focus' || type === 'blur' || type === 'scroll';
    document.addEventListener(type, function (event) {
      // Snapshot the original path: an action may replace/remove its own DOM.
      const path = capture ? [event.target] : event.composedPath();
      path.forEach(function (element) {
        if (!element.getAttribute || !element.hasAttribute(attribute)) return;
        const binding = JSON.parse(element.getAttribute(attribute));
        if (type === 'submit' && element.tagName === 'FORM') event.preventDefault();
        const args = [binding[1]];
        if (type === 'input' || type === 'change' || type === 'keyup') args.push(window.jtmlEventValue(event));
        else if (type === 'scroll') args.push(String(window.scrollY));
        else if (type === 'keydown') args.push({key: event.key, altKey: event.altKey,
          ctrlKey: event.ctrlKey, shiftKey: event.shiftKey, metaKey: event.metaKey});
        window.sendEvent(binding[0], name, args);
      });
    }, capture);
  });
})();
)JTML";
}

std::string emitBrowserRuntimeScript(int webSocketPort,
                                     bool browserLocalRuntime) {
    // The browser runtime is authored as owned asset chunks instead of one
    // giant emitter literal. Keep this function as the stable C++ API that
    // parameterizes the runtime for live WebSocket and browser-local modes.
    return "<script>\n" +
        parameterizeRuntime(assembleRuntimeChunks(), webSocketPort, browserLocalRuntime) +
        (browserLocalRuntime ? emitBrowserRootEventBindings() : "") +
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
