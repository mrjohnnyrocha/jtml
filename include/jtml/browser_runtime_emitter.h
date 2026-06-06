#ifndef JTML_BROWSER_RUNTIME_EMITTER_H
#define JTML_BROWSER_RUNTIME_EMITTER_H

#include <string>

namespace jtml {

std::string emitBrowserRuntimeScript(int webSocketPort, bool browserLocalRuntime);

} // namespace jtml

#endif // JTML_BROWSER_RUNTIME_EMITTER_H
