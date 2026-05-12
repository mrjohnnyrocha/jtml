#include "jtml/fix.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace jtml {

namespace {

std::string ltrim(std::string s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    return s;
}

std::string rtrim(std::string s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
    return s;
}

bool startsWith(const std::string& s, const std::string& prefix) {
    return s.rfind(prefix, 0) == 0;
}

bool hasFriendlyHeader(const std::vector<std::string>& lines) {
    for (const auto& line : lines) {
        std::string text = ltrim(line);
        if (text.empty()) continue;
        return startsWith(text, "jtml 2");
    }
    return false;
}

bool shouldTreatAsFriendly(const std::string& source, SyntaxMode syntax) {
    if (syntax == SyntaxMode::Friendly) return true;
    if (syntax == SyntaxMode::Classic) return false;
    return isFriendlySyntax(source) || looksLikeFriendlySyntax(source);
}

std::vector<std::string> splitPreservingLines(const std::string& source) {
    std::vector<std::string> lines;
    std::istringstream input(source);
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    if (source.empty()) lines.push_back("");
    return lines;
}

} // namespace

FixResult fixSource(const std::string& source, SyntaxMode syntax) {
    FixResult result;
    result.source = source;

    std::vector<std::string> lines = splitPreservingLines(source);
    const bool friendly = shouldTreatAsFriendly(source, syntax);

    if (friendly && !hasFriendlyHeader(lines)) {
        lines.insert(lines.begin(), "");
        lines.insert(lines.begin(), "jtml 2");
        result.changed = true;
        result.changes.push_back({
            1,
            "JTML_FIX_HEADER",
            "Added missing `jtml 2` Friendly syntax header.",
        });
    }

    for (size_t index = 0; index < lines.size(); ++index) {
        std::string fixed;
        fixed.reserve(lines[index].size());
        bool replacedTab = false;
        for (char ch : lines[index]) {
            if (ch == '\t') {
                fixed += "  ";
                replacedTab = true;
            } else {
                fixed.push_back(ch);
            }
        }
        if (replacedTab) {
            lines[index] = fixed;
            result.changed = true;
            result.changes.push_back({
                static_cast<int>(index + 1),
                "JTML_FIX_TABS",
                "Replaced tab indentation with two spaces.",
            });
        }

        std::string trimmed = rtrim(lines[index]);
        if (trimmed != lines[index]) {
            lines[index] = trimmed;
            result.changed = true;
            result.changes.push_back({
                static_cast<int>(index + 1),
                "JTML_FIX_TRAILING_SPACE",
                "Removed trailing whitespace.",
            });
        }
    }

    std::ostringstream out;
    for (const auto& line : lines) out << line << "\n";
    result.source = out.str();
    if (result.source != source) {
        result.changed = true;
    }
    if (source.empty() || source.back() != '\n') {
        result.changes.push_back({
            static_cast<int>(lines.size()),
            "JTML_FIX_FINAL_NEWLINE",
            "Added final newline.",
        });
    }
    return result;
}

} // namespace jtml
