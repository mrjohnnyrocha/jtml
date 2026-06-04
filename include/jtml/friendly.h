#pragma once

#include <string>
#include <vector>

namespace jtml {

enum class SyntaxMode {
    Auto,
    Classic,
    Friendly,
};

bool isFriendlySyntax(const std::string& source);
bool looksLikeFriendlySyntax(const std::string& source);
struct FriendlyClassicResult {
    std::string classicSource;
    // 1-based classic output line -> 1-based original Friendly source line.
    // Entry 0 is unused so callers can index with a diagnostic line directly.
    std::vector<int> classicLineToFriendlyLine;
};

std::string friendlyToClassic(const std::string& source);
FriendlyClassicResult friendlyToClassicWithSourceMap(const std::string& source);
std::string formatFriendlySource(const std::string& source);
std::string normalizeSourceSyntax(const std::string& source);
std::string normalizeSourceSyntax(const std::string& source, SyntaxMode mode);

} // namespace jtml
