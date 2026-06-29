#pragma once

#include <array>

namespace jtml {

struct BrowserRuntimeAssetChunk {
    const char* name;
    const char* source;
};

using BrowserRuntimeAssetChunks = std::array<BrowserRuntimeAssetChunk, 4>;

BrowserRuntimeAssetChunks browserRuntimeAssetChunks();

} // namespace jtml
