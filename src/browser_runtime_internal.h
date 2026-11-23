#pragma once

#include "jtml/browser_runtime_assets.h"

#include <string>

namespace jtml::detail {

// The split browser-only variants are internal test assets.
BrowserRuntimeAssetChunks browserLocalRuntimeAssetChunks();
std::string emitBrowserLocalRuntimeAsset(bool strictScalar = false);

} // namespace jtml::detail
