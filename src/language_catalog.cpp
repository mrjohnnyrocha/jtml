#include "jtml/language_catalog.h"

#include <set>

namespace jtml {
namespace {

std::vector<std::string> uniqueSorted(const std::vector<std::string>& input) {
    std::set<std::string> values(input.begin(), input.end());
    return {values.begin(), values.end()};
}

} // namespace

const LanguageCatalog& languageCatalog() {
    static const LanguageCatalog catalog = {
        {
            {"file/version", {"jtml"}},
            {"state and values", {"let", "const", "get", "show"}},
            {"actions and effects", {"when", "effect", "store", "extern", "redirect", "refresh", "invalidate"}},
            {"control flow", {"if", "else", "for", "in", "while", "break", "continue", "try", "catch", "finally", "return", "throw"}},
            {"components and modules", {"make", "slot", "use", "export"}},
            {"async data", {"fetch", "method", "body", "cache", "credentials", "timeout", "retry", "stale", "keep", "lazy", "load"}},
            {"routes", {"route", "layout", "guard", "require", "activeRoute", "activeRouteName"}},
            {"forms and events", {"into", "click", "input", "change", "submit", "hover", "scroll", "focus", "blur", "keyup", "keydown", "key-up", "key-down", "dragover", "drop", "dblclick", "double-click"}},
            {"common elements", {"page", "link", "navlink", "text", "box", "checkbox", "list", "ordered", "item"}},
            {"media", {"image", "video", "audio", "embed", "file", "dropzone"}},
            {"graphics", {"canvas", "svg", "graphic", "group", "bar", "dot", "line", "path", "polyline", "polygon", "chart", "scene3d"}},
            {"semantic UI", {"theme", "app", "shell", "topbar", "sidebar", "content", "panel", "card", "grid", "stack", "cluster", "split", "toolbar", "tabs", "tab", "alert", "badge", "modal", "drawer", "toast", "loading", "error", "empty", "field", "metric", "spacer"}},
            {"UI modifiers", {"cols", "gap", "pad", "radius", "shadow", "tone", "align", "justify", "width", "surface"}},
            {"style and interop escape hatches", {"style", "theme", "css", "html", "raw", "extern"}},
            {"media helpers", {"timeline", "animate", "resize", "crop", "filter", "axis", "legend", "stacked", "duration", "easing", "autoplay", "repeat"}},
        },
        {"element", "define", "derive", "unbind", "function", "async", "subscribe", "unsubscribe", "object", "derives", "import", "main", "then", "except", "to", "from"},
        {"click", "input", "change", "submit", "hover", "scroll", "focus", "blur", "keyup", "keydown", "key-up", "key-down", "dragover", "drop", "dblclick", "double-click"},
    };
    return catalog;
}

std::vector<std::string> friendlyKeywords() {
    std::vector<std::string> values;
    for (const auto& group : languageCatalog().friendlyGroups) {
        values.insert(values.end(), group.keywords.begin(), group.keywords.end());
    }
    return uniqueSorted(values);
}

std::vector<std::string> allLanguageKeywords() {
    std::vector<std::string> values = friendlyKeywords();
    const auto& compatibility = languageCatalog().compatibilityBackendKeywords;
    values.insert(values.end(), compatibility.begin(), compatibility.end());
    return uniqueSorted(values);
}

} // namespace jtml
