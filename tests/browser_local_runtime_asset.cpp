#include "src/browser_runtime_internal.h"

#include <iostream>

int main() {
    std::cout << jtml::detail::emitBrowserLocalRuntimeAsset();
    return std::cout ? 0 : 1;
}
