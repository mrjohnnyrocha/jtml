#pragma once

#include <string>
#include <vector>

namespace jtml {

struct LanguageKeywordGroup {
    std::string area;
    std::vector<std::string> keywords;
};

struct LanguageCatalog {
    std::vector<LanguageKeywordGroup> friendlyGroups;
    std::vector<std::string> compatibilityBackendKeywords;
    std::vector<std::string> eventAttributes;
};

const LanguageCatalog& languageCatalog();
std::vector<std::string> friendlyKeywords();
std::vector<std::string> allLanguageKeywords();

} // namespace jtml
