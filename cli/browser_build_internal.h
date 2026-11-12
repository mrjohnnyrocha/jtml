// Internal build entry for production and test-only certification.
#pragma once

#include "browser_build_policy.h"

namespace jtml::cli::detail {

int buildBrowser(const Options& options, BrowserCertification certification);

} // namespace jtml::cli::detail
