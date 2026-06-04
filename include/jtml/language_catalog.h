#pragma once

#include <string>
#include <vector>

namespace jtml {

struct LanguageKeywordGroup {
    std::string area;
    std::vector<std::string> keywords;
};

struct SemanticUiModifierSpec {
    std::string name;
    std::string appliesTo;
    std::vector<std::string> values;
    std::string description;
};

struct SemanticUiPrimitiveSpec {
    std::string name;
    std::string category;
    std::string lowersTo;
    std::vector<std::string> commonModifiers;
    std::string description;
};

struct SemanticUiCatalog {
    std::vector<SemanticUiPrimitiveSpec> primitives;
    std::vector<SemanticUiModifierSpec> modifiers;
    std::vector<std::string> themeTokenKinds;
};

struct LanguageCatalog {
    std::vector<LanguageKeywordGroup> friendlyGroups;
    std::vector<std::string> compatibilityBackendKeywords;
    std::vector<std::string> eventAttributes;
    SemanticUiCatalog semanticUi;
};

const LanguageCatalog& languageCatalog();
const SemanticUiCatalog& semanticUiCatalog();
std::vector<std::string> friendlyKeywords();
std::vector<std::string> allLanguageKeywords();

} // namespace jtml
