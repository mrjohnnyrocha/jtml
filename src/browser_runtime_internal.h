#pragma once

#include "jtml/browser_runtime_assets.h"

#include <string>

namespace jtml::detail {

// The split browser-only variant is currently exercised by the test driver.
BrowserRuntimeAssetChunks browserLocalRuntimeAssetChunks();
std::string emitBrowserLocalRuntimeAsset();

} // namespace jtml::detail
