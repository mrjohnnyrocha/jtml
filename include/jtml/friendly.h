#pragma once

#include <string>
#include <map>
#include <vector>
#include "jtml/source_origin.h"

namespace jtml {

enum class SyntaxMode {
    Auto,
    Classic,
    Friendly,
};

// Split authored words without breaking quoted or grouped expressions.
// Shared with Runtime Plan serialization so argument indices match the frontend.
std::vector<std::string> splitFriendlyWords(const std::string& source);

// Token roles are assigned by the Friendly element parser, before binding.
// Backends may consume them without maintaining another attribute grammar.
enum class FriendlyElementRole { Syntax, Expression, InlineContent, CompleteAttribute };
std::vector<FriendlyElementRole> classifyFriendlyElement(
    const std::vector<std::string>& tokens,
    const std::map<std::string, std::string>& bindings = {});

bool isFriendlySyntax(const std::string& source);
bool isJtlCoreSyntax(const std::string& source);
bool looksLikeFriendlySyntax(const std::string& source);
class FriendlySourceError : public std::runtime_error {
public:
    std::string description;
    int line;
    int column;
    std::optional<OriginalRange> original;
    FriendlySourceError(std::string message, int row, int col,
                        std::optional<OriginalRange> origin = {})
        : std::runtime_error(format(message, row, col, origin)), description(std::move(message)),
          line(row), column(col), original(std::move(origin)) {}
private:
    static std::string format(const std::string& message, int line, int column,
                              const std::optional<OriginalRange>& origin) {
        if (!origin) return message + " at line " + std::to_string(line) + ":" + std::to_string(column);
        const auto position = origin->source->coordinates(origin->begin);
        return message + " at " + origin->source->file + ":" + std::to_string(position.first) +
            ":" + std::to_string(position.second) + "; use spaces for indentation";
    }
};

struct FriendlyClassicResult {
    std::string classicSource;
    // 1-based classic output line -> 1-based original Friendly source line.
    // Entry 0 is unused so callers can index with a diagnostic line directly.
    std::vector<int> classicLineToFriendlyLine;
    std::shared_ptr<const SourceProvenance> provenance;
};

std::string friendlyToClassic(const std::string& source);
FriendlyClassicResult friendlyToClassicWithSourceMap(const std::string& source);
FriendlyClassicResult friendlyToClassicWithSourceMap(const MappedSource& source);
std::string formatFriendlySource(const std::string& source);
std::string normalizeSourceSyntax(const std::string& source);
std::string normalizeSourceSyntax(const std::string& source, SyntaxMode mode);

} // namespace jtml
