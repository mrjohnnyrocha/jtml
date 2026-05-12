// cli/cmd_tutorial.cpp — `jtml tutorial` is an alias for `jtml studio`.
//
// The full IDE + tutorial implementation lives in cmd_serve.cpp (cmdStudio).
// This thin wrapper keeps the old command name working so existing docs and
// shell scripts are not broken.
#include "commands.h"

namespace jtml::cli {

int cmdTutorial(const Options& o) {
    return cmdStudio(o);
}

} // namespace jtml::cli
