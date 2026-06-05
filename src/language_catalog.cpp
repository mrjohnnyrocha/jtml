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
            {"file/version", {"jtml", "jtl"}},
            {"state and values", {"let", "const", "get", "show"}},
            {"actions and effects", {"when", "effect", "store", "extern", "redirect", "refresh", "invalidate"}},
            {"control flow", {"if", "else", "for", "in", "while", "break", "continue", "try", "catch", "finally", "return", "throw"}},
            {"components and modules", {"make", "slot", "use", "export"}},
            {"async data", {"fetch", "method", "body", "cache", "credentials", "timeout", "retry", "stale", "keep", "group", "key", "cache-key", "cacheKey", "dedupe", "every", "revalidate", "background", "lazy", "load"}},
            {"routes", {"route", "layout", "guard", "require", "activeRoute", "activeRouteName"}},
            {"forms and events", {"into", "click", "input", "change", "submit", "hover", "scroll", "focus", "blur", "keyup", "keydown", "key-up", "key-down", "dragover", "drop", "dblclick", "double-click"}},
            {"common elements", {"page", "link", "navlink", "text", "box", "checkbox", "list", "ordered", "item"}},
            {"media", {"image", "video", "audio", "embed", "file", "dropzone"}},
            {"graphics", {"canvas", "svg", "graphic", "group", "bar", "dot", "line", "path", "polyline", "polygon", "svgtext", "chart", "scene3d"}},
            {"semantic UI", {"theme", "app", "shell", "topbar", "sidebar", "content", "panel", "card", "grid", "stack", "cluster", "split", "toolbar", "tabs", "tab", "alert", "badge", "modal", "drawer", "toast", "loading", "error", "empty", "field", "metric", "spacer"}},
            {"UI modifiers", {"cols", "gap", "pad", "radius", "shadow", "tone", "align", "justify", "width", "surface"}},
            {"style and interop escape hatches", {"style", "theme", "css", "html", "raw", "extern"}},
            {"media helpers", {"timeline", "animate", "resize", "crop", "filter", "axis", "legend", "grid", "stacked", "values", "series", "colors", "min", "max", "ticks", "annotate", "export", "duration", "easing", "autoplay", "repeat"}},
        },
        {"element", "define", "derive", "unbind", "function", "async", "subscribe", "unsubscribe", "object", "derives", "import", "main", "then", "except", "to", "from"},
        {"click", "input", "change", "submit", "hover", "scroll", "focus", "blur", "keyup", "keydown", "key-up", "key-down", "dragover", "drop", "dblclick", "double-click"},
        {
            {
                {"app", "application shell", "div", {"gap", "pad", "surface"}, "Top-level application wrapper for product-style pages."},
                {"shell", "application shell", "div", {"gap", "pad", "surface"}, "Main app layout wrapper, usually containing sidebar and content."},
                {"topbar", "application shell", "header", {"gap", "pad", "surface", "align", "justify"}, "Top navigation or command bar."},
                {"sidebar", "application shell", "aside", {"gap", "pad", "surface", "width"}, "Navigation or workspace rail."},
                {"content", "application shell", "section", {"gap", "pad", "width"}, "Primary page content region."},
                {"panel", "surface", "section", {"pad", "radius", "shadow", "surface", "tone", "width"}, "Named surface for major page sections; supports title text."},
                {"card", "surface", "article", {"pad", "radius", "shadow", "surface", "tone"}, "Compact repeated or nested content surface; supports title text."},
                {"grid", "layout", "div", {"cols", "gap", "align", "justify", "width"}, "Responsive grid intent visible to semantic analysis."},
                {"stack", "layout", "div", {"gap", "align", "width"}, "Vertical flow layout."},
                {"cluster", "layout", "div", {"gap", "align", "justify", "width"}, "Wrapping horizontal group for buttons, tags, and controls."},
                {"split", "layout", "div", {"gap", "align", "justify", "width"}, "Two-region layout with separated content."},
                {"toolbar", "layout", "div", {"gap", "align", "justify"}, "Action row for buttons and controls."},
                {"tabs", "navigation", "div", {"gap", "surface"}, "Container for tabbed navigation."},
                {"tab", "navigation", "button", {"tone"}, "Single tab control."},
                {"metric", "data display", "article", {"tone", "surface", "shadow"}, "Dashboard metric with label, value, and supporting copy."},
                {"badge", "feedback", "span", {"tone", "surface"}, "Small status label."},
                {"alert", "feedback", "div", {"tone", "surface"}, "Inline status, warning, or informational message."},
                {"loading", "feedback", "div", {"tone"}, "Consistent loading state."},
                {"error", "feedback", "div", {"tone"}, "Consistent error state with optional retry action."},
                {"empty", "feedback", "div", {"tone"}, "Empty-result state."},
                {"modal", "overlay", "dialog", {"pad", "radius", "shadow", "surface", "tone"}, "Modal overlay surface."},
                {"drawer", "overlay", "aside", {"pad", "surface", "shadow", "width"}, "Side overlay surface."},
                {"toast", "overlay", "div", {"tone", "surface", "shadow"}, "Temporary notification surface."},
                {"field", "form", "label", {"gap", "width"}, "Form field wrapper."},
                {"spacer", "layout", "span", {"width"}, "Flexible spacing element inside bars/toolbars."},
            },
            {
                {"cols", "grid", {"1", "2", "3", "4", "auto"}, "Number of grid columns or automatic fit."},
                {"gap", "layout", {"none", "xs", "sm", "md", "lg", "xl"}, "Space between child elements."},
                {"pad", "surface/layout", {"none", "xs", "sm", "md", "lg", "xl"}, "Internal spacing."},
                {"radius", "surface", {"none", "sm", "md", "lg", "pill"}, "Corner radius token."},
                {"shadow", "surface", {"none", "sm", "md", "lg"}, "Elevation token."},
                {"tone", "feedback/surface/data display", {"neutral", "primary", "good", "warn", "danger"}, "Semantic color intent."},
                {"align", "layout", {"start", "center", "end", "stretch"}, "Cross-axis alignment."},
                {"justify", "layout", {"start", "center", "between", "end"}, "Main-axis distribution."},
                {"width", "layout/surface", {"full", "content", "narrow", "wide"}, "Preferred width behavior."},
                {"surface", "surface/layout", {"flat", "raised", "inset", "dark"}, "Surface treatment."},
            },
            {"color", "space", "radius", "shadow"},
        },
    };
    return catalog;
}

const SemanticUiCatalog& semanticUiCatalog() {
    return languageCatalog().semanticUi;
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
