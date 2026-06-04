// fix.h
//
// Conservative source repairs for AI/human editing loops. The fixer only
// applies mechanical, low-risk changes and leaves semantic repairs to
// diagnostics and future refactor tools.
#pragma once

#include "jtml/friendly.h"

#include <string>
#include <vector>

namespace jtml {

struct FixChange {
    int line = 0;
    std::string code;
    std::string message;
};

struct FixResult {
    std::string source;
    bool changed = false;
    std::vector<FixChange> changes;
};

FixResult fixSource(const std::string& source, SyntaxMode syntax = SyntaxMode::Auto);

} // namespace jtml
