#include "jtml/runtime/static_update_plan_emitter.h"

#include "jtml/runtime_plan_json.h"
#include "json.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <vector>

namespace jtml {
namespace {

nlohmann::json moduleIdToJson(SemanticModuleId id) {
    return id == InvalidSemanticModuleId ? nlohmann::json(nullptr) : nlohmann::json(id);
}

std::string joinStrings(const std::vector<std::string>& values,
                        const std::string& separator) {
    std::ostringstream out;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) out << separator;
        out << values[i];
    }
    return out.str();
}

std::string joinInts(const std::vector<int>& values,
                     const std::string& separator) {
    std::ostringstream out;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) out << separator;
        out << values[i];
    }
    return out.str();
}

std::string componentUpdatePlanKey(
        const RuntimePlanComponentDefinition& definition) {
    std::ostringstream signature;
    for (size_t i = 0; i < definition.bodyPlan.size(); ++i) {
        if (i > 0) signature << "@@";
        const auto& node = definition.bodyPlan[i];
        signature << node.kind << '#'
                  << node.name << '#'
                  << node.text << '#'
                  << node.expression << '#'
                  << node.keyExpression << '#'
                  << node.sourceLine << '#'
                  << joinStrings(node.reads, "|") << '#'
                  << joinStrings(node.writes, "|") << '#'
                  << joinInts(node.childIndices, ",");
    }

    std::ostringstream key;
    key << (definition.moduleId == InvalidSemanticModuleId
                ? std::string("global")
                : std::to_string(definition.moduleId))
        << ':' << definition.name
        << ':' << definition.bodyPlan.size()
        << ':' << signature.str();
    return key.str();
}

std::string componentNodeHead(const RuntimePlanComponentBodyNode& node) {
    if (!node.name.empty()) return node.name;
    if (!node.head.empty()) return node.head;
    std::istringstream words(node.text);
    std::string head;
    words >> head;
    return head;
}

std::vector<std::string> splitStaticComponentWords(const std::string& source) {
    std::vector<std::string> words;
    std::string current;
    char quote = '\0';
    for (size_t i = 0; i < source.size(); ++i) {
        const char ch = source[i];
        if (quote != '\0') {
            current += ch;
            if (ch == '\\' && i + 1 < source.size()) {
                current += source[++i];
            } else if (ch == quote) {
                quote = '\0';
            }
            continue;
        }
        if (ch == '"' || ch == '\'') {
            quote = ch;
            current += ch;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(ch))) {
            if (!current.empty()) {
                words.push_back(current);
                current.clear();
            }
            continue;
        }
        current += ch;
    }
    if (!current.empty()) words.push_back(current);
    return words;
}

std::string componentTagName(const std::string& name) {
    static const std::map<std::string, std::string> tags = {
        {"app", "div"},       {"page", "main"},       {"shell", "div"},
        {"topbar", "header"}, {"sidebar", "aside"},   {"content", "main"},
        {"panel", "section"}, {"card", "section"},    {"metric", "article"},
        {"toolbar", "div"},   {"tabs", "div"},        {"tab", "button"},
        {"alert", "div"},     {"badge", "span"},      {"modal", "section"},
        {"drawer", "aside"},  {"toast", "div"},       {"loading", "div"},
        {"error", "div"},     {"empty", "div"},       {"field", "label"},
        {"spacer", "div"},    {"box", "div"},         {"stack", "div"},
        {"cluster", "div"},   {"split", "div"},       {"grid", "div"},
        {"text", "p"},        {"link", "a"},          {"navlink", "a"},
        {"image", "img"},     {"list", "ul"},         {"listOrdered", "ol"},
        {"item", "li"},       {"checkbox", "input"},  {"file", "input"},
        {"dropzone", "input"},{"title", "h1"},        {"subtitle", "p"},
    };
    const auto it = tags.find(name);
    return it == tags.end() ? name : it->second;
}

std::string trimCopy(const std::string& value) {
    size_t begin = 0;
    while (begin < value.size() &&
           std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }
    size_t end = value.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(begin, end - begin);
}

bool isQuotedString(const std::string& value) {
    return value.size() >= 2 &&
           ((value.front() == '"' && value.back() == '"') ||
            (value.front() == '\'' && value.back() == '\''));
}

std::string unquoteStaticString(const std::string& value) {
    if (!isQuotedString(value)) return value;
    std::string out;
    for (size_t i = 1; i + 1 < value.size(); ++i) {
        if (value[i] == '\\' && i + 2 < value.size()) {
            out += value[++i];
        } else {
            out += value[i];
        }
    }
    return out;
}

bool isSimplePathExpression(const std::string& value) {
    if (value.empty()) return false;
    bool expectSegmentStart = true;
    for (char ch : value) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (expectSegmentStart) {
            if (!(std::isalpha(uch) || ch == '_')) return false;
            expectSegmentStart = false;
            continue;
        }
        if (ch == '.') {
            expectSegmentStart = true;
            continue;
        }
        if (!(std::isalnum(uch) || ch == '_')) return false;
    }
    return !expectSegmentStart;
}

nlohmann::json compileStaticExpressionPlan(const std::string& expression);

bool isBalancedStaticExpression(const std::string& value,
                                size_t begin,
                                size_t end) {
    int paren = 0;
    int bracket = 0;
    int brace = 0;
    char quote = '\0';
    for (size_t i = begin; i < end; ++i) {
        const char ch = value[i];
        if (quote != '\0') {
            if (ch == '\\' && i + 1 < end) {
                ++i;
            } else if (ch == quote) {
                quote = '\0';
            }
            continue;
        }
        if (ch == '"' || ch == '\'') {
            quote = ch;
            continue;
        }
        if (ch == '(') ++paren;
        else if (ch == ')') --paren;
        else if (ch == '[') ++bracket;
        else if (ch == ']') --bracket;
        else if (ch == '{') ++brace;
        else if (ch == '}') --brace;
        if (paren < 0 || bracket < 0 || brace < 0) return false;
    }
    return quote == '\0' && paren == 0 && bracket == 0 && brace == 0;
}

std::string stripOuterStaticParens(std::string expr) {
    expr = trimCopy(expr);
    while (expr.size() >= 2 && expr.front() == '(' && expr.back() == ')' &&
           isBalancedStaticExpression(expr, 1, expr.size() - 1)) {
        expr = trimCopy(expr.substr(1, expr.size() - 2));
    }
    return expr;
}

bool staticExpressionOperatorMayBeUnary(const std::string& expr, size_t pos) {
    if (pos == 0) return true;
    size_t cursor = pos;
    while (cursor > 0 &&
           std::isspace(static_cast<unsigned char>(expr[cursor - 1]))) {
        --cursor;
    }
    if (cursor == 0) return true;
    const char prev = expr[cursor - 1];
    return prev == '(' || prev == '[' || prev == '{' || prev == '?' ||
           prev == ':' || prev == ',' || prev == '+' || prev == '-' ||
           prev == '*' || prev == '/' || prev == '%' || prev == '<' ||
           prev == '>' || prev == '=' || prev == '!' || prev == '&' ||
           prev == '|';
}

int findTopLevelStaticChar(const std::string& expr, char needle) {
    int paren = 0;
    int bracket = 0;
    int brace = 0;
    char quote = '\0';
    for (size_t i = 0; i < expr.size(); ++i) {
        const char ch = expr[i];
        if (quote != '\0') {
            if (ch == '\\' && i + 1 < expr.size()) {
                ++i;
            } else if (ch == quote) {
                quote = '\0';
            }
            continue;
        }
        if (ch == '"' || ch == '\'') {
            quote = ch;
            continue;
        }
        if (ch == '(') ++paren;
        else if (ch == ')') --paren;
        else if (ch == '[') ++bracket;
        else if (ch == ']') --bracket;
        else if (ch == '{') ++brace;
        else if (ch == '}') --brace;
        else if (ch == needle && paren == 0 && bracket == 0 && brace == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int findMatchingTopLevelStaticColon(const std::string& expr, size_t question) {
    int paren = 0;
    int bracket = 0;
    int brace = 0;
    int nestedConditional = 0;
    char quote = '\0';
    for (size_t i = question + 1; i < expr.size(); ++i) {
        const char ch = expr[i];
        if (quote != '\0') {
            if (ch == '\\' && i + 1 < expr.size()) {
                ++i;
            } else if (ch == quote) {
                quote = '\0';
            }
            continue;
        }
        if (ch == '"' || ch == '\'') {
            quote = ch;
            continue;
        }
        if (ch == '(') ++paren;
        else if (ch == ')') --paren;
        else if (ch == '[') ++bracket;
        else if (ch == ']') --bracket;
        else if (ch == '{') ++brace;
        else if (ch == '}') --brace;
        else if (paren == 0 && bracket == 0 && brace == 0) {
            if (ch == '?') ++nestedConditional;
            else if (ch == ':') {
                if (nestedConditional == 0) return static_cast<int>(i);
                --nestedConditional;
            }
        }
    }
    return -1;
}

int findTopLevelStaticOperator(const std::string& expr,
                               const std::vector<std::string>& operators) {
    int paren = 0;
    int bracket = 0;
    int brace = 0;
    char quote = '\0';
    for (int i = static_cast<int>(expr.size()) - 1; i >= 0; --i) {
        const char ch = expr[static_cast<size_t>(i)];
        if (quote != '\0') {
            if (ch == quote) quote = '\0';
            continue;
        }
        if (ch == '"' || ch == '\'') {
            quote = ch;
            continue;
        }
        if (ch == ')') ++paren;
        else if (ch == '(') --paren;
        else if (ch == ']') ++bracket;
        else if (ch == '[') --bracket;
        else if (ch == '}') ++brace;
        else if (ch == '{') --brace;
        if (paren != 0 || bracket != 0 || brace != 0) continue;
        for (const auto& op : operators) {
            if (op.empty() || i + static_cast<int>(op.size()) > static_cast<int>(expr.size())) {
                continue;
            }
            if (expr.compare(static_cast<size_t>(i), op.size(), op) != 0) continue;
            if ((op == "+" || op == "-") &&
                staticExpressionOperatorMayBeUnary(expr, static_cast<size_t>(i))) {
                continue;
            }
            return i;
        }
    }
    return -1;
}

nlohmann::json compileStaticExpressionPlan(const std::string& expression) {
    const auto expr = stripOuterStaticParens(expression);
    if (expr.empty()) return {{"kind", "empty"}, {"source", ""}};
    if (isQuotedString(expr)) {
        return {
            {"kind", "literal"},
            {"source", expr},
            {"value", unquoteStaticString(expr)}
        };
    }
    if (expr == "true" || expr == "false") {
        return {
            {"kind", "literal"},
            {"source", expr},
            {"value", expr == "true"}
        };
    }
    if (expr == "null") {
        return {{"kind", "literal"}, {"source", expr}, {"value", nullptr}};
    }
    char* end = nullptr;
    const double number = std::strtod(expr.c_str(), &end);
    if (end && *end == '\0' && end != expr.c_str()) {
        return {{"kind", "literal"}, {"source", expr}, {"value", number}};
    }
    if (isSimplePathExpression(expr)) {
        nlohmann::json segments = nlohmann::json::array();
        std::string current;
        for (char ch : expr) {
            if (ch == '.') {
                segments.push_back(current);
                current.clear();
            } else {
                current += ch;
            }
        }
        if (!current.empty()) segments.push_back(current);
        return {
            {"kind", "path"},
            {"source", expr},
            {"root", segments.empty() ? std::string() : segments.front().get<std::string>()},
            {"segments", segments}
        };
    }
    const int question = findTopLevelStaticChar(expr, '?');
    if (question > 0) {
        const int colon = findMatchingTopLevelStaticColon(expr, static_cast<size_t>(question));
        if (colon > question) {
            const auto test = trimCopy(expr.substr(0, static_cast<size_t>(question)));
            const auto truthy = trimCopy(expr.substr(
                static_cast<size_t>(question + 1),
                static_cast<size_t>(colon - question - 1)));
            const auto falsey = trimCopy(expr.substr(static_cast<size_t>(colon + 1)));
            if (!test.empty() && !truthy.empty() && !falsey.empty()) {
                return {
                    {"kind", "conditional"},
                    {"source", expr},
                    {"test", compileStaticExpressionPlan(test)},
                    {"whenTrue", compileStaticExpressionPlan(truthy)},
                    {"whenFalse", compileStaticExpressionPlan(falsey)}
                };
            }
        }
    }
    const std::vector<std::vector<std::string>> operatorGroups = {
        {"||"},
        {"&&"},
        {"==", "!="},
        {"<=", ">=", "<", ">"},
        {"+", "-"},
        {"*", "/", "%"}
    };
    for (const auto& group : operatorGroups) {
        const int pos = findTopLevelStaticOperator(expr, group);
        if (pos <= 0) continue;
        std::string op;
        for (const auto& candidate : group) {
            if (expr.compare(static_cast<size_t>(pos), candidate.size(), candidate) == 0) {
                op = candidate;
                break;
            }
        }
        if (op.empty()) continue;
        const auto left = trimCopy(expr.substr(0, static_cast<size_t>(pos)));
        const auto right = trimCopy(expr.substr(static_cast<size_t>(pos) + op.size()));
        if (left.empty() || right.empty()) continue;
        return {
            {"kind", "binary"},
            {"source", expr},
            {"operator", op},
            {"left", compileStaticExpressionPlan(left)},
            {"right", compileStaticExpressionPlan(right)}
        };
    }
    if (expr.size() > 1 && expr.front() == '!') {
        const auto arg = trimCopy(expr.substr(1));
        if (!arg.empty()) {
            return {
                {"kind", "unary"},
                {"source", expr},
                {"operator", "!"},
                {"argument", compileStaticExpressionPlan(arg)}
            };
        }
    }
    return {{"kind", "source"}, {"source", expr}};
}

bool isSemanticUiModifierName(const std::string& name) {
    static const std::set<std::string> names = {
        "cols", "gap", "pad", "radius", "shadow",
        "tone", "align", "justify", "width", "surface"
    };
    return names.count(name) > 0;
}

bool isComponentAttributeName(const std::string& name) {
    static const std::set<std::string> names = {
        "id", "class", "style", "role", "href", "src", "alt", "title",
        "type", "name", "value", "placeholder", "for", "rel", "target",
        "method", "action", "enctype", "autocomplete", "inputmode",
        "pattern", "accept", "poster", "preload", "kind", "srclang",
        "label", "width", "height", "min", "max", "step", "minlength",
        "maxlength", "rows", "cols", "cx", "cy", "r", "x", "y", "x1",
        "y1", "x2", "y2", "d", "points", "viewBox", "fill", "stroke",
        "stroke-width", "transform", "opacity", "to", "active-class"
    };
    return names.count(name) > 0 ||
           name.rfind("aria-", 0) == 0 ||
           name.rfind("data-", 0) == 0;
}

bool isComponentBooleanAttribute(const std::string& name) {
    static const std::set<std::string> names = {
        "disabled", "required", "checked", "selected", "multiple",
        "readonly", "autofocus", "playsinline", "open", "hidden",
        "controls", "autoplay", "loop", "muted"
    };
    return names.count(name) > 0;
}

bool componentAttributeTakesValue(const std::string& name) {
    return isComponentAttributeName(name) && !isComponentBooleanAttribute(name);
}

bool isSemanticUiPrimitiveName(const std::string& name) {
    static const std::set<std::string> names = {
        "app", "shell", "topbar", "sidebar", "content", "panel", "card",
        "metric", "stack", "cluster", "split", "grid", "toolbar", "tabs",
        "tab", "alert", "badge", "modal", "drawer", "toast", "loading",
        "error", "empty", "field", "spacer", "box"
    };
    return names.count(name) > 0;
}

nlohmann::json compileStaticElementParts(
        const std::vector<std::string>& words,
        size_t start,
        const std::set<std::string>& stopWords = {}) {
    nlohmann::json attrs = nlohmann::json::array();
    nlohmann::json modifiers = nlohmann::json::array();
    std::vector<std::string> content;
    for (size_t i = start; i < words.size(); ++i) {
        const auto& token = words[i];
        if (stopWords.count(token)) break;
        if (isSemanticUiModifierName(token)) {
            const auto raw = i + 1 < words.size() ? words[i + 1] : std::string();
            if (!raw.empty() && !stopWords.count(raw) &&
                !isComponentAttributeName(raw) &&
                !isComponentBooleanAttribute(raw) &&
                !isSemanticUiModifierName(raw)) {
                modifiers.push_back({
                    {"name", token},
                    {"raw", raw},
                    {"exprPlan", compileStaticExpressionPlan(raw)}
                });
                ++i;
            } else {
                modifiers.push_back({{"name", token}, {"raw", ""}});
            }
            continue;
        }
        if (isComponentAttributeName(token)) {
            const auto raw = i + 1 < words.size() ? words[i + 1] : std::string();
            if (!raw.empty() && !stopWords.count(raw) &&
                (componentAttributeTakesValue(token) ||
                 (!isComponentAttributeName(raw) &&
                  !isComponentBooleanAttribute(raw)))) {
                attrs.push_back({
                    {"name", token},
                    {"raw", raw},
                    {"exprPlan", compileStaticExpressionPlan(raw)}
                });
                ++i;
            } else {
                content.push_back(token);
            }
            continue;
        }
        if (isComponentBooleanAttribute(token)) {
            attrs.push_back({{"name", token}, {"boolean", true}});
            continue;
        }
        content.push_back(token);
    }
    const auto contentSource = joinStrings(content, " ");
    return {
        {"content", contentSource},
        {"contentPlan", compileStaticExpressionPlan(contentSource)},
        {"attrs", attrs},
        {"modifiers", modifiers},
    };
}

nlohmann::json compileStaticActionInvocation(const std::string& raw) {
    const auto open = raw.find('(');
    if (open == std::string::npos || raw.empty() || raw.back() != ')') {
        return {
            {"name", raw},
            {"argExpressions", nlohmann::json::array()},
            {"argPlans", nlohmann::json::array()}
        };
    }
    const auto name = raw.substr(0, open);
    const auto args = raw.substr(open + 1, raw.size() - open - 2);
    nlohmann::json argExpressions = nlohmann::json::array();
    nlohmann::json argPlans = nlohmann::json::array();
    std::string current;
    int depth = 0;
    char quote = '\0';
    for (size_t i = 0; i < args.size(); ++i) {
        const char ch = args[i];
        if (quote != '\0') {
            current += ch;
            if (ch == '\\' && i + 1 < args.size()) current += args[++i];
            else if (ch == quote) quote = '\0';
            continue;
        }
        if (ch == '"' || ch == '\'') {
            quote = ch;
            current += ch;
            continue;
        }
        if (ch == '(' || ch == '[' || ch == '{') ++depth;
        if (ch == ')' || ch == ']' || ch == '}') --depth;
        if (ch == ',' && depth == 0) {
            if (!current.empty()) {
                const auto expr = trimCopy(current);
                argExpressions.push_back(expr);
                argPlans.push_back(compileStaticExpressionPlan(expr));
            }
            current.clear();
            continue;
        }
        current += ch;
    }
    if (!current.empty()) {
        const auto expr = trimCopy(current);
        argExpressions.push_back(expr);
        argPlans.push_back(compileStaticExpressionPlan(expr));
    }
    return {
        {"name", name},
        {"argExpressions", argExpressions},
        {"argPlans", argPlans}
    };
}

void addNameSet(std::set<std::string>& out,
                const std::vector<std::string>& values) {
    for (const auto& value : values) {
        if (!value.empty()) out.insert(value);
    }
}

std::vector<std::string> expandedNodeReads(
        const RuntimePlanComponentDefinition& definition,
        const RuntimePlanComponentBodyNode& node) {
    std::map<std::string, std::vector<std::string>> derivedDeps;
    for (const auto& candidate : definition.bodyPlan) {
        if (candidate.kind == "derived" && !candidate.name.empty()) {
            derivedDeps[candidate.name] = candidate.reads;
        }
    }

    std::set<std::string> reads;
    addNameSet(reads, node.reads);
    bool changed = true;
    int guard = 0;
    while (changed && guard++ < 1000) {
        changed = false;
        for (const auto& [name, deps] : derivedDeps) {
            if (!reads.count(name)) continue;
            const auto before = reads.size();
            addNameSet(reads, deps);
            changed = changed || reads.size() != before;
        }
    }
    return {reads.begin(), reads.end()};
}

using DefinitionKey = std::pair<SemanticModuleId, std::string>;

std::set<DefinitionKey> componentDefinitionKeys(
        const std::vector<RuntimePlanComponentDefinition>& definitions) {
    std::set<DefinitionKey> out;
    for (const auto& definition : definitions) {
        out.insert({definition.moduleId, definition.name});
        out.insert({InvalidSemanticModuleId, definition.name});
    }
    return out;
}

bool isKnownNestedComponent(
        const std::set<DefinitionKey>& definitionKeys,
        const RuntimePlanComponentDefinition& owner,
        const RuntimePlanComponentBodyNode& node) {
    const auto head = componentNodeHead(node);
    if (head.empty()) return false;
    const SemanticModuleId module =
        node.definitionModule != InvalidSemanticModuleId
            ? node.definitionModule
            : owner.moduleId;
    return definitionKeys.count({module, head}) ||
           definitionKeys.count({InvalidSemanticModuleId, head});
}

std::string staticPatchOperationKind(
        const std::set<DefinitionKey>& definitionKeys,
        const RuntimePlanComponentDefinition& definition,
        const RuntimePlanComponentBodyNode& node) {
    if (node.kind != "template") return "";
    const auto head = componentNodeHead(node);
    if (head == "if" || head == "for" || head == "slot") return "region";
    if (head == "else" || head == "while") return "";
    if (isKnownNestedComponent(definitionKeys, definition, node)) {
        return "nested-component";
    }
    if (head == "show" || head == "text") return "text";
    if (head == "button") return "button";
    return node.childIndices.empty() ? "element" : "container-attrs";
}

nlohmann::json staticPatchOperation(
        const std::string& kind,
        const RuntimePlanComponentBodyNode& node,
        size_t index) {
    const auto words = splitStaticComponentWords(node.text);
    nlohmann::json out = {
        {"operation", kind},
        {"nodeIndex", index},
        {"head", componentNodeHead(node)},
        {"tag", componentTagName(componentNodeHead(node))},
        {"words", words},
        {"sourceLine", node.sourceLine},
        {"expression", node.expression},
        {"expressionPlan", compileStaticExpressionPlan(node.expression)},
        {"keyExpression", node.keyExpression},
        {"keyExpressionPlan", compileStaticExpressionPlan(node.keyExpression)},
    };
    if (kind == "button") {
        out["partsPlan"] = compileStaticElementParts(words, 1, {"click"});
        auto it = std::find(words.begin(), words.end(), "click");
        out["clickInvocation"] = it != words.end() && std::next(it) != words.end()
            ? compileStaticActionInvocation(*std::next(it))
            : nlohmann::json({{"name", ""}, {"argExpressions", nlohmann::json::array()}});
    } else if (kind == "element" || kind == "container-attrs") {
        out["partsPlan"] = compileStaticElementParts(words, 1);
    }
    return out;
}

nlohmann::json compileStaticComponentUpdatePlan(
        const RuntimePlanComponentDefinition& definition,
        const std::set<DefinitionKey>& definitionKeys,
        const std::string& origin) {
    nlohmann::json entries = nlohmann::json::array();
    nlohmann::json entriesByRead = nlohmann::json::object();
    nlohmann::json unsafeEntries = nlohmann::json::array();
    nlohmann::json rootCreateOperations = nlohmann::json::array();
    nlohmann::json unsafeRootCreateEntries = nlohmann::json::array();

    for (size_t i = 0; i < definition.bodyPlan.size(); ++i) {
        const auto& node = definition.bodyPlan[i];
        if (node.kind != "template" || componentNodeHead(node) == "else") continue;
        if (node.renderRoot) {
            const auto rootOperationKind = staticPatchOperationKind(
                definitionKeys, definition, node);
            if (rootOperationKind.empty()) {
                unsafeRootCreateEntries.push_back({
                    {"index", i},
                    {"kind", componentNodeHead(node)},
                    {"sourceLine", node.sourceLine},
                });
            } else {
                rootCreateOperations.push_back(
                    staticPatchOperation(rootOperationKind, node, i));
            }
        }
        const auto reads = expandedNodeReads(definition, node);
        if (reads.empty()) continue;

        const auto operationKind = staticPatchOperationKind(
            definitionKeys, definition, node);
        if (operationKind.empty()) {
            unsafeEntries.push_back({
                {"index", i},
                {"reads", reads},
                {"kind", componentNodeHead(node)},
                {"sourceLine", node.sourceLine},
            });
            continue;
        }

        nlohmann::json entry = {
            {"index", i},
            {"reads", reads},
            {"kind", componentNodeHead(node)},
            {"sourceLine", node.sourceLine},
            {"operation", staticPatchOperation(operationKind, node, i)},
        };
        entries.push_back(entry);
        for (const auto& read : reads) {
            entriesByRead[read].push_back(entry);
        }
    }

    return {
        {"origin", origin},
        {"moduleId", moduleIdToJson(definition.moduleId)},
        {"name", definition.name},
        {"key", componentUpdatePlanKey(definition)},
        {"params", definition.params},
        {"localState", definition.localState},
        {"localDerived", definition.localDerived},
        {"localActions", definition.localActions},
        {"bodyPlan", runtimePlanBodyPlanToJson(definition.bodyPlan)},
        {"entries", entries},
        {"entriesByRead", entriesByRead},
        {"unsafeEntries", unsafeEntries},
        {"rootCreateOperations", rootCreateOperations},
        {"unsafeRootCreateEntries", unsafeRootCreateEntries},
        {"staticPatchCoverage", "text-region-nested-element-first-slice"},
        {"staticCreateCoverage", "direct-text-button-element-container-create-first-slice"},
    };
}

void appendComponentPlans(
        nlohmann::json& components,
        const std::vector<RuntimePlanComponentDefinition>& definitions,
        const std::set<DefinitionKey>& definitionKeys,
        const std::string& origin) {
    for (const auto& definition : definitions) {
        components.push_back(
            compileStaticComponentUpdatePlan(definition, definitionKeys, origin));
    }
}

std::string escapeJsScriptText(std::string value) {
    auto replaceAll = [](std::string& text,
                         const std::string& needle,
                         const std::string& replacement) {
        size_t pos = 0;
        while ((pos = text.find(needle, pos)) != std::string::npos) {
            text.replace(pos, needle.size(), replacement);
            pos += replacement.size();
        }
    };
    replaceAll(value, "\\", "\\\\");
    replaceAll(value, "'", "\\'");
    replaceAll(value, "\n", "\\n");
    replaceAll(value, "\r", "\\r");
    replaceAll(value, "\xE2\x80\xA8", "\\u2028");
    replaceAll(value, "\xE2\x80\xA9", "\\u2029");
    return value;
}

std::string jsStringLiteral(const std::string& value) {
    return "'" + escapeJsScriptText(value) + "'";
}

std::string jsExpressionPlanValue(const nlohmann::json& value) {
    if (value.is_string()) return jsStringLiteral(value.get<std::string>());
    if (value.is_boolean()) return value.get<bool>() ? "true" : "false";
    if (value.is_number()) return value.dump();
    if (value.is_null()) return "null";
    return "undefined";
}

bool isDirectJsExpressionPlan(const nlohmann::json& plan);

std::string jsExpressionPlanArgument(const nlohmann::json& plan) {
    if (!plan.is_object()) return plan.dump();
    const auto kind = plan.value("kind", "");
    if (kind == "empty") {
        return "(function(scope){ return ''; })";
    }
    if (kind == "literal") {
        return "(function(scope){ return " +
               jsExpressionPlanValue(plan.contains("value") ? plan["value"] : nlohmann::json()) +
               "; })";
    }
    if (kind == "path" && plan.contains("segments") && plan["segments"].is_array()) {
        const auto& segments = plan["segments"];
        if (segments.empty() || !segments.front().is_string()) return plan.dump();
        std::ostringstream out;
        out << "(function(scope){";
        out << " if(!scope || !Object.prototype.hasOwnProperty.call(scope,"
            << jsStringLiteral(segments.front().get<std::string>())
            << ")) return undefined;";
        out << " let value = scope[" << jsStringLiteral(segments.front().get<std::string>()) << "];";
        for (size_t i = 1; i < segments.size(); ++i) {
            if (!segments[i].is_string()) return plan.dump();
            out << " if(value == null) return '';";
            out << " value = value[" << jsStringLiteral(segments[i].get<std::string>()) << "];";
        }
        out << " return value == null ? '' : value; })";
        return out.str();
    }
    if (kind == "unary" && plan.value("operator", "") == "!") {
        const auto argument = plan.value("argument", nlohmann::json::object());
        if (!isDirectJsExpressionPlan(argument)) return plan.dump();
        return "(function(scope){ return !" + jsExpressionPlanArgument(argument) + "(scope); })";
    }
    if (kind == "binary") {
        const auto left = plan.value("left", nlohmann::json::object());
        const auto right = plan.value("right", nlohmann::json::object());
        const auto op = plan.value("operator", "");
        static const std::set<std::string> supported = {
            "||", "&&", "==", "!=", "<=", ">=", "<", ">", "+", "-", "*", "/", "%"
        };
        if (!supported.count(op) ||
            !isDirectJsExpressionPlan(left) ||
            !isDirectJsExpressionPlan(right)) {
            return plan.dump();
        }
        return "(function(scope){ const left = " + jsExpressionPlanArgument(left) +
               "(scope); const right = " + jsExpressionPlanArgument(right) +
               "(scope); return left " + op + " right; })";
    }
    if (kind == "conditional") {
        const auto test = plan.value("test", nlohmann::json::object());
        const auto whenTrue = plan.value("whenTrue", nlohmann::json::object());
        const auto whenFalse = plan.value("whenFalse", nlohmann::json::object());
        if (!isDirectJsExpressionPlan(test) ||
            !isDirectJsExpressionPlan(whenTrue) ||
            !isDirectJsExpressionPlan(whenFalse)) {
            return plan.dump();
        }
        return "(function(scope){ return " + jsExpressionPlanArgument(test) +
               "(scope) ? " + jsExpressionPlanArgument(whenTrue) +
               "(scope) : " + jsExpressionPlanArgument(whenFalse) + "(scope); })";
    }
    return plan.dump();
}

bool isDirectJsExpressionPlan(const nlohmann::json& plan) {
    if (!plan.is_object()) return false;
    const auto kind = plan.value("kind", "");
    if (kind == "empty" || kind == "literal" || kind == "path") return true;
    if (kind == "unary") {
        return plan.value("operator", "") == "!" &&
               isDirectJsExpressionPlan(plan.value("argument", nlohmann::json::object()));
    }
    if (kind == "binary") {
        static const std::set<std::string> supported = {
            "||", "&&", "==", "!=", "<=", ">=", "<", ">", "+", "-", "*", "/", "%"
        };
        return supported.count(plan.value("operator", "")) &&
               isDirectJsExpressionPlan(plan.value("left", nlohmann::json::object())) &&
               isDirectJsExpressionPlan(plan.value("right", nlohmann::json::object()));
    }
    if (kind == "conditional") {
        return isDirectJsExpressionPlan(plan.value("test", nlohmann::json::object())) &&
               isDirectJsExpressionPlan(plan.value("whenTrue", nlohmann::json::object())) &&
               isDirectJsExpressionPlan(plan.value("whenFalse", nlohmann::json::object()));
    }
    return false;
}

std::string jsDirectExpressionCall(const nlohmann::json& plan) {
    return jsExpressionPlanArgument(plan) + "(scope)";
}

bool staticElementPartsAreDirect(const nlohmann::json& partsPlan,
                                 bool requireContentPlan) {
    if (!partsPlan.is_object()) return false;
    if (requireContentPlan &&
        !isDirectJsExpressionPlan(
            partsPlan.value("contentPlan", nlohmann::json::object()))) {
        return false;
    }
    for (const auto& attr : partsPlan.value("attrs", nlohmann::json::array())) {
        if (!attr.is_object() || attr.value("boolean", false)) continue;
        if (!isDirectJsExpressionPlan(
                attr.value("exprPlan", nlohmann::json::object()))) {
            return false;
        }
    }
    for (const auto& modifier : partsPlan.value("modifiers", nlohmann::json::array())) {
        if (!modifier.is_object() || modifier.value("raw", "").empty()) continue;
        if (!isDirectJsExpressionPlan(
                modifier.value("exprPlan", nlohmann::json::object()))) {
            return false;
        }
    }
    return true;
}

bool staticActionInvocationIsDirect(const nlohmann::json& invocation) {
    if (!invocation.is_object()) return true;
    for (const auto& plan : invocation.value("argPlans", nlohmann::json::array())) {
        if (!isDirectJsExpressionPlan(plan)) return false;
    }
    return true;
}

void appendDirectAttributeMapStatements(std::ostringstream& out,
                                        const nlohmann::json& partsPlan,
                                        const std::string& head) {
    out << "const next = {}; "
        << "let classValue = ''; "
        << "function addAttr(name, value) { "
        << "if (value == null || value === '') return; "
        << "if (name === 'class' && value !== true) { "
        << "classValue = (classValue ? classValue + ' ' : '') + String(value); "
        << "} else if (value === true) next[name] = true; "
        << "else next[name] = String(value); "
        << "} ";

    if (isSemanticUiPrimitiveName(head)) {
        out << "addAttr('class'," << jsStringLiteral("jtml-" + head) << "); "
            << "addAttr('data-jtml-ui'," << jsStringLiteral(head) << "); ";
    }

    for (const auto& modifier : partsPlan.value("modifiers", nlohmann::json::array())) {
        if (!modifier.is_object()) continue;
        const auto name = modifier.value("name", "");
        if (name.empty()) continue;
        if (modifier.value("raw", "").empty()) {
            out << "addAttr('class'," << jsStringLiteral("jtml-" + name) << "); "
                << "addAttr(" << jsStringLiteral("data-jtml-ui-" + name)
                << ",'true'); ";
            continue;
        }
        const auto plan = modifier.value("exprPlan", nlohmann::json::object());
        out << "{ const modifierValue = " << jsDirectExpressionCall(plan) << "; "
            << "const modifierText = String(modifierValue == null ? '' : modifierValue); "
            << "const suffix = modifierText ? '-' + modifierText.replace(/[^A-Za-z0-9_-]/g, '-') : ''; "
            << "addAttr('class'," << jsStringLiteral("jtml-" + name)
            << " + suffix); "
            << "addAttr(" << jsStringLiteral("data-jtml-ui-" + name)
            << ", modifierText || 'true'); } ";
    }

    for (const auto& attr : partsPlan.value("attrs", nlohmann::json::array())) {
        if (!attr.is_object()) continue;
        const auto name = attr.value("name", "");
        if (name.empty()) continue;
        if (attr.value("boolean", false)) {
            out << "addAttr(" << jsStringLiteral(name) << ", true); ";
            continue;
        }
        out << "addAttr(" << jsStringLiteral(name) << ", "
            << jsDirectExpressionCall(attr.value("exprPlan", nlohmann::json::object()))
            << "); ";
    }

    if (head == "checkbox") {
        out << "if (!Object.prototype.hasOwnProperty.call(next, 'type')) addAttr('type', 'checkbox'); ";
    } else if (head == "file" || head == "dropzone") {
        out << "if (!Object.prototype.hasOwnProperty.call(next, 'type')) addAttr('type', 'file'); ";
    }
    if (!head.empty()) {
        out << "addAttr('data-jtml-direct-node'," << jsStringLiteral(head) << "); ";
    }
    out << "if (classValue) next.class = classValue; ";
}

void appendDirectAttributePatchStatements(std::ostringstream& out,
                                          const nlohmann::json& partsPlan,
                                          const std::string& head) {
    appendDirectAttributeMapStatements(out, partsPlan, head);
    out
        << "const previous = String(el.getAttribute('data-jtml-direct-managed-attrs') || '')"
        << ".split(',').filter(Boolean); "
        << "previous.forEach(function(name){ if (!Object.prototype.hasOwnProperty.call(next, name)) el.removeAttribute(name); }); "
        << "Object.keys(next).forEach(function(name){ "
        << "if (next[name] === true) el.setAttribute(name, name); "
        << "else el.setAttribute(name, next[name]); "
        << "}); "
        << "el.setAttribute('data-jtml-direct-managed-attrs', Object.keys(next).join(',')); ";
}

std::string directStaticTextPatchExpression(size_t nodeIndex,
                                            const nlohmann::json& plan) {
    if (!isDirectJsExpressionPlan(plan)) return "";
    std::ostringstream out;
    out << "(function(){ "
        << "const el = instance && instance.element ? "
        << "instance.element.querySelector('[data-jtml-direct-body-node=\""
        << nodeIndex << "\"]') : null; "
        << "if (!el) return false; "
        << "const value = " << jsExpressionPlanArgument(plan) << "(scope); "
        << "el.textContent = String(value == null ? '' : value); "
        << "return true; "
        << "})()";
    return out.str();
}

std::string directStaticButtonPatchExpression(size_t nodeIndex,
                                              const std::string& head,
                                              const nlohmann::json& partsPlan,
                                              const nlohmann::json& clickInvocation) {
    if (!staticElementPartsAreDirect(partsPlan, true) ||
        !staticActionInvocationIsDirect(clickInvocation)) {
        return "";
    }
    std::ostringstream out;
    out << "(function(){ "
        << "const el = instance && instance.element ? "
        << "instance.element.querySelector('[data-jtml-direct-body-node=\""
        << nodeIndex << "\"]') : null; "
        << "if (!el || !el.tagName || el.tagName.toLowerCase() !== 'button') return false; "
        << "const labelValue = "
        << jsDirectExpressionCall(partsPlan.value("contentPlan", nlohmann::json::object()))
        << "; "
        << "const label = String(labelValue == null ? '' : labelValue); "
        << "if (el.textContent !== label) el.textContent = label; ";
    appendDirectAttributePatchStatements(out, partsPlan, head.empty() ? "button" : head);
    const auto actionName = clickInvocation.value("name", "");
    if (!actionName.empty()) {
        out << "const actionArgs = [";
        const auto argPlans = clickInvocation.value("argPlans", nlohmann::json::array());
        for (size_t i = 0; i < argPlans.size(); ++i) {
            if (i > 0) out << ",";
            out << jsDirectExpressionCall(argPlans[i]);
        }
        out << "]; "
            << "el.setAttribute('data-jtml-direct-component-id', instance && instance.id || ''); "
            << "el.setAttribute('data-jtml-direct-component-action', "
            << jsStringLiteral(actionName) << "); "
            << "el.setAttribute('data-jtml-direct-component-args', JSON.stringify(actionArgs)); ";
    } else {
        out << "el.removeAttribute('data-jtml-direct-component-id'); "
            << "el.removeAttribute('data-jtml-direct-component-action'); "
            << "el.removeAttribute('data-jtml-direct-component-args'); ";
    }
    out << "return true; })()";
    return out.str();
}

std::string directStaticElementPatchExpression(size_t nodeIndex,
                                               const std::string& head,
                                               const std::string& tag,
                                               const nlohmann::json& partsPlan,
                                               bool patchContent,
                                               const nlohmann::json& expressionPlan) {
    const auto contentPlan = partsPlan.value("contentPlan", nlohmann::json::object());
    const bool usesPartsContent = !partsPlan.value("content", "").empty();
    const auto& planForContent = usesPartsContent ? contentPlan : expressionPlan;
    if (!staticElementPartsAreDirect(partsPlan, patchContent && usesPartsContent) ||
        (patchContent && !isDirectJsExpressionPlan(planForContent))) {
        return "";
    }
    const auto resolvedHead = head.empty() ? std::string("box") : head;
    const auto resolvedTag = tag.empty() ? componentTagName(resolvedHead) : tag;
    std::ostringstream out;
    out << "(function(){ "
        << "const el = instance && instance.element ? "
        << "instance.element.querySelector('[data-jtml-direct-body-node=\""
        << nodeIndex << "\"]') : null; "
        << "if (!el || !el.tagName || el.tagName.toLowerCase() !== "
        << jsStringLiteral(resolvedTag) << ".toLowerCase()) return false; ";
    if (patchContent) {
        out << "const contentValue = " << jsDirectExpressionCall(planForContent) << "; "
            << "const contentText = String(contentValue == null ? '' : contentValue); "
            << "if (el.textContent !== contentText) el.textContent = contentText; ";
    }
    appendDirectAttributePatchStatements(out, partsPlan, resolvedHead);
    out << "return true; })()";
    return out.str();
}

std::string directStaticTextCreateExpression(size_t nodeIndex,
                                             const nlohmann::json& plan) {
    if (!isDirectJsExpressionPlan(plan)) return "";
    std::ostringstream out;
    out << "(function(){ "
        << "const value = " << jsExpressionPlanArgument(plan) << "(scope); "
        << "return '<p data-jtml-direct-body-node=\"" << nodeIndex
        << "\" data-jtml-direct-text-node=\"true\">' + "
        << "jtml_static_escape_html(value) + '</p>'; "
        << "})()";
    return out.str();
}

std::string directStaticButtonCreateExpression(size_t nodeIndex,
                                               const std::string& head,
                                               const nlohmann::json& partsPlan,
                                               const nlohmann::json& clickInvocation) {
    if (!staticElementPartsAreDirect(partsPlan, true) ||
        !staticActionInvocationIsDirect(clickInvocation)) {
        return "";
    }
    const auto actionName = clickInvocation.value("name", "");
    std::ostringstream out;
    out << "(function(){ "
        << "const labelValue = "
        << jsDirectExpressionCall(partsPlan.value("contentPlan", nlohmann::json::object()))
        << "; ";
    appendDirectAttributeMapStatements(out, partsPlan, head.empty() ? "button" : head);
    out << "next['data-jtml-direct-body-node'] = " << jsStringLiteral(std::to_string(nodeIndex)) << "; "
        << "next['data-jtml-direct-managed-attrs'] = Object.keys(next).join(','); ";
    if (!actionName.empty()) {
        out << "const actionArgs = [";
        const auto argPlans = clickInvocation.value("argPlans", nlohmann::json::array());
        for (size_t i = 0; i < argPlans.size(); ++i) {
            if (i > 0) out << ",";
            out << jsDirectExpressionCall(argPlans[i]);
        }
        out << "]; "
            << "next['data-jtml-direct-component-id'] = instance && instance.id || ''; "
            << "next['data-jtml-direct-component-action'] = "
            << jsStringLiteral(actionName) << "; "
            << "next['data-jtml-direct-component-args'] = JSON.stringify(actionArgs); ";
    }
    out << "return '<button' + jtml_static_attrs_to_string(next) + '>' + "
        << "jtml_static_escape_html(labelValue) + '</button>'; "
        << "})()";
    return out.str();
}

std::string directStaticElementCreateExpression(size_t nodeIndex,
                                                const std::string& head,
                                                const std::string& tag,
                                                const nlohmann::json& partsPlan,
                                                const nlohmann::json& expressionPlan) {
    const auto contentPlan = partsPlan.value("contentPlan", nlohmann::json::object());
    const bool usesPartsContent = !partsPlan.value("content", "").empty();
    const auto& planForContent = usesPartsContent ? contentPlan : expressionPlan;
    if (!staticElementPartsAreDirect(partsPlan, usesPartsContent) ||
        !isDirectJsExpressionPlan(planForContent)) {
        return "";
    }
    const auto resolvedHead = head.empty() ? std::string("box") : head;
    const auto resolvedTag = tag.empty() ? componentTagName(resolvedHead) : tag;
    std::ostringstream out;
    out << "(function(){ "
        << "const contentValue = " << jsDirectExpressionCall(planForContent) << "; ";
    appendDirectAttributeMapStatements(out, partsPlan, resolvedHead);
    out << "next['data-jtml-direct-body-node'] = " << jsStringLiteral(std::to_string(nodeIndex)) << "; "
        << "next['data-jtml-direct-managed-attrs'] = Object.keys(next).join(','); "
        << "return '<" << resolvedTag << "' + jtml_static_attrs_to_string(next) + '>' + "
        << "jtml_static_escape_html(contentValue) + '</" << resolvedTag << ">'; "
        << "})()";
    return out.str();
}

bool looksLikeComponentCallHead(const std::string& head) {
    return !head.empty() &&
           std::isupper(static_cast<unsigned char>(head.front()));
}

std::string directStaticNodeCreateExpression(const nlohmann::json& componentPlan,
                                             size_t nodeIndex,
                                             std::set<size_t>& visiting);

std::string directStaticContainerCreateExpression(const nlohmann::json& componentPlan,
                                                  size_t nodeIndex,
                                                  const nlohmann::json& node,
                                                  const std::string& head,
                                                  const std::string& tag,
                                                  const nlohmann::json& partsPlan) {
    if (!staticElementPartsAreDirect(partsPlan, false)) return "";
    const auto children = node.value("childIndices", nlohmann::json::array());
    if (!children.is_array()) return "";

    std::vector<std::string> childExpressions;
    std::set<size_t> visiting;
    visiting.insert(nodeIndex);
    for (const auto& childIndexValue : children) {
        if (!childIndexValue.is_number_integer()) return "";
        const auto childIndex = static_cast<size_t>(childIndexValue.get<int>());
        auto child = directStaticNodeCreateExpression(componentPlan, childIndex, visiting);
        if (child.empty()) return "";
        childExpressions.push_back(std::move(child));
    }

    const auto resolvedHead = head.empty() ? std::string("box") : head;
    const auto resolvedTag = tag.empty() ? componentTagName(resolvedHead) : tag;
    std::ostringstream out;
    out << "(function(){ ";
    appendDirectAttributeMapStatements(out, partsPlan, resolvedHead);
    out << "next['data-jtml-direct-body-node'] = "
        << jsStringLiteral(std::to_string(nodeIndex)) << "; "
        << "next['data-jtml-direct-managed-attrs'] = Object.keys(next).join(','); "
        << "const children = [];";
    for (size_t i = 0; i < childExpressions.size(); ++i) {
        out << " const child_" << i << " = " << childExpressions[i] << "; "
            << "if (typeof child_" << i << " !== 'string') return null; "
            << "children.push(child_" << i << ");";
    }
    out << " return '<" << resolvedTag << "' + jtml_static_attrs_to_string(next) + '>' + "
        << "children.join('') + '</" << resolvedTag << ">'; "
        << "})()";
    return out.str();
}

std::string directStaticNodeCreateExpression(const nlohmann::json& componentPlan,
                                             size_t nodeIndex,
                                             std::set<size_t>& visiting) {
    const auto bodyPlan = componentPlan.value("bodyPlan", nlohmann::json::array());
    if (!bodyPlan.is_array() || nodeIndex >= bodyPlan.size() ||
        !bodyPlan[nodeIndex].is_object()) {
        return "";
    }
    if (visiting.count(nodeIndex)) return "";
    visiting.insert(nodeIndex);

    const auto& node = bodyPlan[nodeIndex];
    if (node.value("kind", "") != "template") return "";
    const auto head = node.value("head", node.value("name", ""));
    const auto resolvedHead = head.empty()
        ? splitStaticComponentWords(node.value("text", "")).empty()
            ? std::string("")
            : splitStaticComponentWords(node.value("text", "")).front()
        : head;
    if (resolvedHead.empty() || resolvedHead == "else" || resolvedHead == "while" ||
        resolvedHead == "if" || resolvedHead == "for" || resolvedHead == "slot" ||
        looksLikeComponentCallHead(resolvedHead)) {
        return "";
    }

    const auto words = splitStaticComponentWords(node.value("text", ""));
    const auto expressionPlan =
        compileStaticExpressionPlan(node.value("expression", ""));
    if (resolvedHead == "show" || resolvedHead == "text") {
        return directStaticTextCreateExpression(nodeIndex, expressionPlan);
    }
    if (resolvedHead == "button") {
        auto partsPlan = compileStaticElementParts(words, 1, {"click"});
        auto it = std::find(words.begin(), words.end(), "click");
        auto clickInvocation = it != words.end() && std::next(it) != words.end()
            ? compileStaticActionInvocation(*std::next(it))
            : nlohmann::json({{"name", ""}, {"argExpressions", nlohmann::json::array()}});
        return directStaticButtonCreateExpression(
            nodeIndex, resolvedHead, partsPlan, clickInvocation);
    }

    auto partsPlan = compileStaticElementParts(words, 1);
    const auto tag = componentTagName(resolvedHead);
    const auto children = node.value("childIndices", nlohmann::json::array());
    if (children.is_array() && !children.empty()) {
        return directStaticContainerCreateExpression(
            componentPlan, nodeIndex, node, resolvedHead, tag, partsPlan);
    }
    return directStaticElementCreateExpression(
        nodeIndex, resolvedHead, tag, partsPlan, expressionPlan);
}

std::string directStaticCreateCall(
        const nlohmann::json& operation,
        size_t operationIndex,
        size_t componentIndex,
        const nlohmann::json& componentPlan) {
    if (!operation.is_object() || !operation.contains("operation") ||
        !operation.contains("nodeIndex")) {
        return "";
    }
    const auto kind = operation.value("operation", "");
    const auto nodeIndex = operation.value("nodeIndex", 0);
    std::ostringstream out;
    out << "    const html_" << operationIndex << " = ";
    if (kind == "text") {
        const auto directCreate = directStaticTextCreateExpression(
            static_cast<size_t>(nodeIndex),
            operation.value("expressionPlan", nlohmann::json::object()));
        if (!directCreate.empty()) {
            out << directCreate << ";\n";
        } else {
            out << "(h && h.renderStaticComponentTextNode ? h.renderStaticComponentTextNode(instance, definition, scope, "
                << nodeIndex << ", "
                << jsStringLiteral(operation.value("expression", "")) << ", "
                << jsExpressionPlanArgument(operation.value("expressionPlan", nlohmann::json::object()))
                << ") : null);\n";
        }
    } else if (kind == "button") {
        const auto directCreate = directStaticButtonCreateExpression(
            static_cast<size_t>(nodeIndex),
            operation.value("head", "button"),
            operation.value("partsPlan", nlohmann::json::object()),
            operation.value("clickInvocation", nlohmann::json::object()));
        if (!directCreate.empty()) {
            out << directCreate << ";\n";
        } else {
            out << "(h && h.renderStaticComponentButtonNode ? h.renderStaticComponentButtonNode(instance, definition, scope, "
                << nodeIndex << ", "
                << jsStringLiteral(operation.value("head", "button")) << ", "
                << operation.value("partsPlan", nlohmann::json::object()).dump()
                << ", "
                << operation.value("clickInvocation", nlohmann::json::object()).dump()
                << ") : null);\n";
        }
    } else if (kind == "element") {
        const auto directCreate = directStaticElementCreateExpression(
            static_cast<size_t>(nodeIndex),
            operation.value("head", "box"),
            operation.value("tag", "div"),
            operation.value("partsPlan", nlohmann::json::object()),
            operation.value("expressionPlan", nlohmann::json::object()));
        if (!directCreate.empty()) {
            out << directCreate << ";\n";
        } else {
            out << "(h && h.renderStaticComponentElementNode ? h.renderStaticComponentElementNode(instance, definition, scope, "
                << nodeIndex << ", "
                << jsStringLiteral(operation.value("head", "box")) << ", "
                << jsStringLiteral(operation.value("tag", "div")) << ", "
                << operation.value("partsPlan", nlohmann::json::object()).dump()
                << ", "
                << jsStringLiteral(operation.value("expression", "")) << ", "
                << jsExpressionPlanArgument(operation.value("expressionPlan", nlohmann::json::object()))
                << ") : null);\n";
        }
    } else if (kind == "container-attrs") {
        std::set<size_t> visiting;
        const auto directCreate = directStaticNodeCreateExpression(
            componentPlan, static_cast<size_t>(nodeIndex), visiting);
        if (!directCreate.empty()) {
            out << directCreate << ";\n";
        } else {
            out << "(h && h.renderStaticComponentElementNode ? h.renderStaticComponentElementNode(instance, definition, scope, "
                << nodeIndex << ", "
                << jsStringLiteral(operation.value("head", "box")) << ", "
                << jsStringLiteral(operation.value("tag", "div")) << ", "
                << operation.value("partsPlan", nlohmann::json::object()).dump()
                << ", "
                << jsStringLiteral(operation.value("expression", "")) << ", "
                << jsExpressionPlanArgument(operation.value("expressionPlan", nlohmann::json::object()))
                << ") : null);\n";
        }
    } else if (kind == "region" || kind == "nested-component") {
        out << "(h && h.renderStaticComponentRegionNode ? h.renderStaticComponentRegionNode(instance, definition, scope, "
            << nodeIndex << ") : null);\n";
    } else {
        return "";
    }
    out << "    if (typeof html_" << operationIndex
        << " !== 'string') return jtml_static_component_create_fallback_"
        << componentIndex << "();\n"
        << "    rendered.push(html_" << operationIndex << ");\n";
    return out.str();
}

std::string directStaticPatchExpression(const nlohmann::json& operation) {
    if (!operation.is_object() || !operation.contains("operation") ||
        !operation.contains("nodeIndex")) {
        return "";
    }
    const auto kind = operation.value("operation", "");
    const auto nodeIndex = operation.value("nodeIndex", 0);
    std::ostringstream out;
    if (kind == "text") {
        const auto directPatch = directStaticTextPatchExpression(
            static_cast<size_t>(nodeIndex),
            operation.value("expressionPlan", nlohmann::json::object()));
        if (!directPatch.empty()) {
            out << directPatch;
        } else {
            out << "(h && h.patchStaticComponentTextNode ? h.patchStaticComponentTextNode(instance, "
                << nodeIndex << ", "
                << jsStringLiteral(operation.value("expression", "")) << ", scope, "
                << jsExpressionPlanArgument(operation.value("expressionPlan", nlohmann::json::object()))
                << ") : false)";
        }
    } else if (kind == "button") {
        const auto directPatch = directStaticButtonPatchExpression(
            static_cast<size_t>(nodeIndex),
            operation.value("head", "button"),
            operation.value("partsPlan", nlohmann::json::object()),
            operation.value("clickInvocation", nlohmann::json::object()));
        if (!directPatch.empty()) {
            out << directPatch;
        } else {
            out << "(h && h.patchStaticComponentButtonNode ? h.patchStaticComponentButtonNode(instance, "
                << nodeIndex << ", "
                << jsStringLiteral(operation.value("head", "button")) << ", "
                << operation.value("partsPlan", nlohmann::json::object()).dump()
                << ", "
                << operation.value("clickInvocation", nlohmann::json::object()).dump()
                << ", scope) : false)";
        }
    } else if (kind == "element" || kind == "container-attrs") {
        const auto directPatch = directStaticElementPatchExpression(
            static_cast<size_t>(nodeIndex),
            operation.value("head", "box"),
            operation.value("tag", "div"),
            operation.value("partsPlan", nlohmann::json::object()),
            kind == "element",
            operation.value("expressionPlan", nlohmann::json::object()));
        if (!directPatch.empty()) {
            out << directPatch;
        } else {
            out << "(h && h.patchStaticComponentElementNode ? h.patchStaticComponentElementNode(instance, "
                << nodeIndex << ", "
                << jsStringLiteral(operation.value("head", "box")) << ", "
                << jsStringLiteral(operation.value("tag", "div")) << ", "
                << operation.value("partsPlan", nlohmann::json::object()).dump()
                << ", "
                << jsStringLiteral(operation.value("expression", "")) << ", "
                << "scope, " << (kind == "element" ? "true" : "false") << ", "
                << jsExpressionPlanArgument(operation.value("expressionPlan", nlohmann::json::object()))
                << ") : false)";
        }
    } else if (kind == "region" || kind == "nested-component") {
        out << "(h && h.patchStaticComponentRegionNode ? "
            << "h.patchStaticComponentRegionNode(instance, definition, "
            << nodeIndex << ", scope) : false)";
    } else {
        return "";
    }
    return out.str();
}

std::string directStaticPatchCase(const nlohmann::json& entry) {
    if (!entry.is_object() || !entry.contains("index") ||
        !entry.contains("operation")) {
        return "";
    }
    const auto operation = entry["operation"];
    const auto patchExpression = directStaticPatchExpression(operation);
    if (patchExpression.empty()) return "";
    const auto index = entry.value("index", 0);
    std::ostringstream out;
    out << "      case " << index << ": {\n"
        << "        return " << patchExpression << ";\n"
        << "      }\n";
    return out.str();
}

std::string staticCreateFallbackFunctionBody(size_t componentIndex) {
    std::ostringstream out;
    out << "    function jtml_static_component_create_fallback_"
        << componentIndex << "() {\n"
        << "      if (h && h.renderStaticComponentCreateOperations && plan && plan.rootCreateOperations) {\n"
        << "        const html = h.renderStaticComponentCreateOperations(instance, definition, scope, plan.rootCreateOperations);\n"
        << "        if (typeof html === 'string') return html;\n"
        << "      }\n"
        << "      if (!h || !h.renderStaticComponentRoots) return '';\n"
        << "      return h.renderStaticComponentRoots(instance, definition, scope);\n"
        << "    }\n";
    return out.str();
}

std::string staticUpdateFunctionBody(size_t componentIndex,
                                     const nlohmann::json& componentPlan) {
    std::ostringstream out;
    out << "  function jtml_static_component_create_" << componentIndex
        << "(instance, definition, scope, h) {\n"
        << "    const plan = plans.components[" << componentIndex << "];\n"
        << staticCreateFallbackFunctionBody(componentIndex)
        << "    function jtml_static_escape_html(value) {\n"
        << "      return String(value == null ? '' : value)\n"
        << "        .replace(/&/g, '&amp;')\n"
        << "        .replace(/</g, '&lt;')\n"
        << "        .replace(/>/g, '&gt;')\n"
        << "        .replace(/\"/g, '&quot;')\n"
        << "        .replace(/'/g, '&#39;');\n"
        << "    }\n"
        << "    function jtml_static_attrs_to_string(attrs) {\n"
        << "      return Object.keys(attrs || {}).map(function(name) {\n"
        << "        if (attrs[name] === true) return ' ' + name;\n"
        << "        return ' ' + name + '=\"' + jtml_static_escape_html(attrs[name]) + '\"';\n"
        << "      }).join('');\n"
        << "    }\n"
        << "    const rendered = [];\n";
    size_t emittedCalls = 0;
    const auto operations = componentPlan.value(
        "rootCreateOperations", nlohmann::json::array());
    for (const auto& operation : operations) {
        const auto call = directStaticCreateCall(
            operation, emittedCalls, componentIndex, componentPlan);
        if (call.empty()) continue;
        out << call;
        emittedCalls += 1;
    }
    if (emittedCalls == 0) {
        out << "    return jtml_static_component_create_fallback_"
            << componentIndex << "();\n";
    } else {
        out << "    if (plan) {\n"
            << "      plan.__lastCreateMode = 'static-production-direct-create-function';\n"
            << "      plan.__lastDirectCreateCount = rendered.length;\n"
            << "    }\n"
            << "    return rendered.join('');\n";
    }
    out
        << "  }\n";
    out << "  function jtml_static_component_patch_" << componentIndex
        << "(entry, node, instance, definition, scope, h) {\n"
        << "    if (!entry) return false;\n"
        << "    switch (entry.index) {\n";
    size_t emittedPatchCases = 0;
    const auto entries = componentPlan.value("entries", nlohmann::json::array());
    for (const auto& entry : entries) {
        const auto patchCase = directStaticPatchCase(entry);
        if (patchCase.empty()) continue;
        out << patchCase;
        emittedPatchCases += 1;
    }
    (void)emittedPatchCases;
    out << "      default: return false;\n"
        << "    }\n"
        << "  }\n";
    out << "  function jtml_static_component_update_" << componentIndex
        << "(instance, definition, changed, scope, h) {\n"
        << "    const plan = plans.components[" << componentIndex << "];\n"
        << "    if (!plan) return { handled: false, patched: 0 };\n"
        << "    function affectedEntries() {\n"
        << "      const out = [];\n"
        << "      const seen = {};\n"
        << "      const byRead = plan.entriesByRead || {};\n"
        << "      (changed || []).forEach(function(name) {\n"
        << "        (byRead[name] || []).forEach(function(entry) {\n"
        << "          const key = String(entry && entry.index);\n"
        << "          if (seen[key]) return;\n"
        << "          seen[key] = true;\n"
        << "          out.push(entry);\n"
        << "        });\n"
        << "      });\n"
        << "      return out;\n"
        << "    }\n"
        << "    function firstUnsafeEntry() {\n"
        << "      const writes = {};\n"
        << "      (changed || []).forEach(function(name) { writes[name] = true; });\n"
        << "      const unsafe = plan.unsafeEntries || [];\n"
        << "      for (let i = 0; i < unsafe.length; i += 1) {\n"
        << "        const reads = unsafe[i].reads || [];\n"
        << "        for (let r = 0; r < reads.length; r += 1) {\n"
        << "          if (writes[reads[r]]) return unsafe[i];\n"
        << "        }\n"
        << "      }\n"
        << "      return null;\n"
        << "    }\n"
        << "    const affected = h && h.componentPlanAffectedEntries ? h.componentPlanAffectedEntries(plan, changed) : affectedEntries();\n"
        << "    plan.__lastOperationCount = affected.length;\n"
        << "    if (!affected.length) return { handled: false, patched: 0 };\n"
        << "    const unsafeNode = h && h.firstUnsafeAffectedRenderNodeFromPlan ? h.firstUnsafeAffectedRenderNodeFromPlan(plan, definition, changed) : null;\n"
        << "    const unsafeEntry = unsafeNode ? null : firstUnsafeEntry();\n"
        << "    if (unsafeNode || unsafeEntry) {\n"
        << "      if (h && h.recordCompiledPatchFallback) h.recordCompiledPatchFallback(instance, definition, unsafeNode || ((definition.bodyPlan || [])[unsafeEntry.index]), \"affected body-plan node requires full rerender\", changed);\n"
        << "      return { handled: false, patched: 0 };\n"
        << "    }\n"
        << "    let patched = 0;\n"
        << "    for (let i = 0; i < affected.length; i += 1) {\n"
        << "      const node = (definition.bodyPlan || [])[affected[i].index];\n"
        << "      if (!node) return { handled: false, patched: patched };\n"
        << "      if (!jtml_static_component_patch_" << componentIndex
        << "(affected[i], node, instance, definition, scope, h) &&\n"
        << "          !(h && h.executeComponentPatchOperation && h.executeComponentPatchOperation(instance, definition, affected[i].operation, scope)) &&\n"
        << "          !(h && h.replaceDirectComponentNode && h.replaceDirectComponentNode(instance, definition, node, scope))) {\n"
        << "        if (h && h.recordCompiledPatchFallback) h.recordCompiledPatchFallback(instance, definition, node, \"static update function failed for affected body-plan node\", changed);\n"
        << "        return { handled: false, patched: patched };\n"
        << "      }\n"
        << "      patched += 1;\n"
        << "    }\n"
        << "    if (h && h.recordComponentUpdatePlanSuccess) h.recordComponentUpdatePlanSuccess(instance, definition, plan, changed, patched, \"static-production-update-function\");\n"
        << "    return { handled: true, patched: patched };\n"
        << "  }\n";
    return out.str();
}

} // namespace

std::string emitStaticUpdatePlanAsset(const RuntimeProjectPlan& plan) {
    nlohmann::json components = nlohmann::json::array();
    auto linkedKeys = componentDefinitionKeys(plan.linkedPlan.componentDefinitions);
    appendComponentPlans(components,
                         plan.linkedPlan.componentDefinitions,
                         linkedKeys,
                         "linked");

    for (const auto& module : plan.modules) {
        if (!module.clientExecutable) continue;
        auto moduleKeys = componentDefinitionKeys(module.plan.componentDefinitions);
        appendComponentPlans(components,
                             module.plan.componentDefinitions,
                             moduleKeys,
                             "module:" + std::to_string(module.id));
    }

    nlohmann::json asset = {
        {"version", 1},
        {"sourceOfTruth", "runtime client manifest"},
        {"mode", "csp-safe static update plans"},
        {"dynamicGeneratedUpdateFunctions", false},
        {"staticUpdateFunctions", true},
        {"componentCount", components.size()},
        {"components", components},
    };

    std::ostringstream out;
    out << "/* Generated by jtml build --target browser. CSP-safe static update plan/function seed. */\n"
        << "(function () {\n"
        << "  const plans = JSON.parse('" << escapeJsScriptText(asset.dump()) << "');\n"
        << "  const functions = {};\n";
    out << "  const modules = {};\n";
    for (size_t i = 0; i < components.size(); ++i) {
        out << staticUpdateFunctionBody(i, components[i])
            << "  if (plans.components[" << i << "] && plans.components[" << i << "].key) {\n"
            << "    functions[plans.components[" << i << "].key] = jtml_static_component_update_" << i << ";\n"
            << "    modules[plans.components[" << i << "].key] = {\n"
            << "      create: jtml_static_component_create_" << i << ",\n"
            << "      update: jtml_static_component_update_" << i << ",\n"
            << "      component: plans.components[" << i << "].name || '',\n"
            << "      moduleId: plans.components[" << i << "].moduleId == null ? null : plans.components[" << i << "].moduleId,\n"
            << "      key: plans.components[" << i << "].key\n"
            << "    };\n"
            << "  }\n";
    }
    out
        << "  plans.staticUpdateFunctionCount = Object.keys(functions).length;\n"
        << "  plans.staticComponentModuleCount = Object.keys(modules).length;\n"
        << "  window.__jtml_static_update_plans = plans;\n"
        << "  window.__jtml_static_update_functions = functions;\n"
        << "  window.__jtml_static_component_modules = modules;\n"
        << "  if (window.jtml) {\n"
        << "    window.jtml.staticUpdatePlans = plans;\n"
        << "    window.jtml.staticUpdateFunctions = functions;\n"
        << "    window.jtml.staticComponentModules = modules;\n"
        << "    window.jtml.runtimeSecurity = window.jtml.runtimeSecurity || {};\n"
        << "    window.jtml.runtimeSecurity.staticUpdatePlansAsset = true;\n"
        << "    window.jtml.runtimeSecurity.staticUpdateFunctionsAsset = true;\n"
        << "    window.jtml.runtimeSecurity.staticComponentModulesAsset = true;\n"
        << "  }\n"
        << "  if (window.dispatchEvent && window.CustomEvent) {\n"
        << "    window.dispatchEvent(new CustomEvent('jtml:static-update-plans-ready', { detail: plans }));\n"
        << "  }\n"
        << "}());\n";
    return out.str();
}

std::string emitStaticComponentModuleAsset(const RuntimeProjectPlan& plan) {
    std::string asset = emitStaticUpdatePlanAsset(plan);
    const std::string legacy = "CSP-safe static update plan/function seed";
    const std::string current = "CSP-safe static component module seed";
    const auto pos = asset.find(legacy);
    if (pos != std::string::npos) {
        asset.replace(pos, legacy.size(), current);
    }
    return asset;
}

} // namespace jtml
