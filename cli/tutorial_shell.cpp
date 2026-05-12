// cli/tutorial_shell.cpp — kept for link-compatibility.
//
// kTutorialShellHTML was replaced by the unified kStudioShellHTML.
// This file re-exports it under the old name so any code that still
// references kTutorialShellHTML compiles without changes.
#include "tutorial_shell.h"
#include "studio_shell.h"

namespace jtml::cli {

const char* kTutorialShellHTML = kStudioShellHTML;

} // namespace jtml::cli
