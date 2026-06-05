#include "jtml/semantic.h"
#include "jtml/language_catalog.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <sstream>
#include <tuple>
#include <utility>

namespace jtml {
namespace {

std::string trimCopy(std::string value) {
    auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string lowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::vector<std::string> words(const std::string& line) {
    std::vector<std::string> out;
    std::istringstream input(line);
    std::string word;
    while (input >> word) out.push_back(word);
    return out;
}

std::string stripInlineComment(const std::string& line) {
    char quote = '\0';
    for (size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (quote != '\0') {
            if (ch == '\\' && i + 1 < line.size()) ++i;
            else if (ch == quote) quote = '\0';
            continue;
        }
        if (ch == '"' || ch == '\'') {
            quote = ch;
            continue;
        }
        if (ch == '/' && i + 1 < line.size() && line[i + 1] == '/') return line.substr(0, i);
        if (ch == '#' && (i == 0 || std::isspace(static_cast<unsigned char>(line[i - 1])))) {
            return line.substr(0, i);
        }
    }
    return line;
}

std::string cleanIdentifier(std::string token) {
    while (!token.empty() && (token.back() == '\\' || token.back() == '(' ||
                              token.back() == ')' || token.back() == ',' ||
                              token.back() == ':')) {
        token.pop_back();
    }
    const auto colon = token.find(':');
    if (colon != std::string::npos) token = token.substr(0, colon);
    return token;
}

std::string unquoteToken(std::string token) {
    token = trimCopy(std::move(token));
    while (!token.empty() && (token.back() == '\\' || token.back() == ',')) {
        token.pop_back();
    }
    if (token.size() >= 2) {
        const char first = token.front();
        const char last = token.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            return token.substr(1, token.size() - 2);
        }
    }
    return token;
}

void insertSorted(std::set<std::string>& values, const std::string& value) {
    if (!value.empty()) values.insert(value);
}

std::vector<std::string> toVector(const std::set<std::string>& values) {
    return {values.begin(), values.end()};
}

std::vector<std::string> splitCommaList(const std::string& value) {
    std::vector<std::string> out;
    std::stringstream input(value);
    std::string item;
    while (std::getline(input, item, ',')) {
        item = trimCopy(item);
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

std::string joinValues(const std::vector<std::string>& values) {
    std::ostringstream out;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i) out << ',';
        out << values[i];
    }
    return out.str();
}

std::string hexDecode(const std::string& hex) {
    std::string out;
    if (hex.size() % 2 != 0) return out;
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        unsigned int value = 0;
        std::istringstream input(hex.substr(i, 2));
        input >> std::hex >> value;
        if (input.fail()) return {};
        out.push_back(static_cast<char>(value));
    }
    return out;
}

std::vector<SemanticProperty> splitPropertyMap(const std::string& value) {
    std::vector<SemanticProperty> out;
    std::stringstream input(value);
    std::string item;
    while (std::getline(input, item, ';')) {
        item = trimCopy(item);
        if (item.empty()) continue;
        const auto equals = item.find('=');
        if (equals == std::string::npos) {
            out.push_back({item, ""});
            continue;
        }
        out.push_back({trimCopy(item.substr(0, equals)), trimCopy(item.substr(equals + 1))});
    }
    return out;
}

int parseIntLiteral(const std::string& value) {
    if (value.empty()) return 0;
    try {
        return std::stoi(value);
    } catch (...) {
        return 0;
    }
}

struct ComponentBodySummary {
    std::set<std::string> localState;
    std::set<std::string> localDerived;
    std::set<std::string> localActions;
    std::set<std::string> localEffects;
    std::set<std::string> eventBindings;
    bool hasSlot = false;
    int bodyNodeCount = 0;
    int rootTemplateNodeCount = 0;
    int slotCount = 0;
};

const std::set<std::string>& friendlyEventNames() {
    static const std::set<std::string> names = {
        "click", "input", "change", "submit", "hover", "scroll", "focus", "blur",
        "keyup", "keydown", "key-up", "key-down", "dragover", "drop", "dblclick",
        "double-click"
    };
    return names;
}

std::string componentBodyLineText(const std::string& encodedLine) {
    const auto colon = encodedLine.find(':');
    return trimCopy(colon == std::string::npos ? encodedLine : encodedLine.substr(colon + 1));
}

int componentBodyLineIndent(const std::string& encodedLine) {
    const auto colon = encodedLine.find(':');
    if (colon == std::string::npos) return 0;
    return parseIntLiteral(encodedLine.substr(0, colon));
}

ComponentBodySummary summarizeComponentBody(const std::string& bodyHex) {
    ComponentBodySummary summary;
    const std::string body = hexDecode(bodyHex);
    std::istringstream input(body);
    std::string raw;
    while (std::getline(input, raw)) {
        const std::string text = componentBodyLineText(raw);
        if (text.empty()) continue;
        ++summary.bodyNodeCount;
        const auto tokens = words(text);
        if (tokens.empty()) continue;
        if (componentBodyLineIndent(raw) == 0) {
            const std::string head = tokens[0];
            if (head != "let" && head != "const" && head != "get" &&
                head != "when" && head != "effect" && head != "slot") {
                ++summary.rootTemplateNodeCount;
            }
        }
        const std::string head = tokens[0];
        if ((head == "let" || head == "const") && tokens.size() > 1) {
            summary.localState.insert(cleanIdentifier(tokens[1]));
        } else if (head == "get" && tokens.size() > 1) {
            summary.localDerived.insert(cleanIdentifier(tokens[1]));
        } else if (head == "when" && tokens.size() > 1) {
            summary.localActions.insert(cleanIdentifier(tokens[1]));
        } else if (head == "effect" && tokens.size() > 1) {
            summary.localEffects.insert(cleanIdentifier(tokens[1]));
        } else if (head == "slot") {
            summary.hasSlot = true;
            ++summary.slotCount;
        }

        for (size_t i = 0; i + 1 < tokens.size(); ++i) {
            if (friendlyEventNames().count(tokens[i])) {
                summary.eventBindings.insert(cleanIdentifier(tokens[i + 1]));
            }
        }
    }
    return summary;
}

std::string rootIdentifier(std::string token) {
    token = cleanIdentifier(token);
    if (token.empty() || token[0] == '"' || token[0] == '\'') return {};
    if (token.front() == '{') token.erase(token.begin());
    if (!token.empty() && token.back() == '}') token.pop_back();
    auto paren = token.find('(');
    if (paren != std::string::npos) token = token.substr(0, paren);
    auto dot = token.find('.');
    if (dot != std::string::npos) token = token.substr(0, dot);
    return cleanIdentifier(token);
}

std::string expressionName(const ExpressionStatementNode* expr) {
    if (!expr) return {};
    if (expr->getExprType() == ExpressionStatementNodeType::Variable) {
        return static_cast<const VariableExpressionStatementNode*>(expr)->name;
    }
    if (expr->getExprType() == ExpressionStatementNodeType::ObjectPropertyAccess) {
        const auto* access = static_cast<const ObjectPropertyAccessExpressionNode*>(expr);
        const auto base = expressionName(access->base.get());
        return base.empty() ? access->propertyName : base + "." + access->propertyName;
    }
    if (expr->getExprType() == ExpressionStatementNodeType::FunctionCall) {
        return static_cast<const FunctionCallExpressionStatementNode*>(expr)->functionName;
    }
    if (expr->getExprType() == ExpressionStatementNodeType::ObjectMethodCall) {
        const auto* call = static_cast<const ObjectMethodCallExpressionNode*>(expr);
        const auto base = expressionName(call->base.get());
        return base.empty() ? call->methodName : base + "." + call->methodName;
    }
    if (expr->getExprType() == ExpressionStatementNodeType::Subscript) {
        return rootIdentifier(expr->toString());
    }
    return {};
}

void collectExpressionRefs(const ExpressionStatementNode* expr, std::set<std::string>& refs) {
    if (!expr) return;
    switch (expr->getExprType()) {
        case ExpressionStatementNodeType::Variable:
            insertSorted(refs, static_cast<const VariableExpressionStatementNode*>(expr)->name);
            break;
        case ExpressionStatementNodeType::Binary: {
            const auto* e = static_cast<const BinaryExpressionStatementNode*>(expr);
            collectExpressionRefs(e->left.get(), refs);
            collectExpressionRefs(e->right.get(), refs);
            break;
        }
        case ExpressionStatementNodeType::Unary:
            collectExpressionRefs(static_cast<const UnaryExpressionStatementNode*>(expr)->right.get(), refs);
            break;
        case ExpressionStatementNodeType::EmbeddedVariable:
            collectExpressionRefs(static_cast<const EmbeddedVariableExpressionStatementNode*>(expr)->embeddedExpression.get(), refs);
            break;
        case ExpressionStatementNodeType::CompositeString: {
            const auto* e = static_cast<const CompositeStringExpressionStatementNode*>(expr);
            for (const auto& part : e->parts) collectExpressionRefs(part.get(), refs);
            break;
        }
        case ExpressionStatementNodeType::ArrayLiteral: {
            const auto* e = static_cast<const ArrayLiteralExpressionStatementNode*>(expr);
            for (const auto& item : e->elements) collectExpressionRefs(item.get(), refs);
            break;
        }
        case ExpressionStatementNodeType::DictionaryLiteral: {
            const auto* e = static_cast<const DictionaryLiteralExpressionStatementNode*>(expr);
            for (const auto& entry : e->entries) collectExpressionRefs(entry.value.get(), refs);
            break;
        }
        case ExpressionStatementNodeType::Subscript: {
            const auto* e = static_cast<const SubscriptExpressionStatementNode*>(expr);
            collectExpressionRefs(e->base.get(), refs);
            collectExpressionRefs(e->index.get(), refs);
            break;
        }
        case ExpressionStatementNodeType::FunctionCall: {
            const auto* e = static_cast<const FunctionCallExpressionStatementNode*>(expr);
            for (const auto& arg : e->arguments) collectExpressionRefs(arg.get(), refs);
            break;
        }
        case ExpressionStatementNodeType::Conditional: {
            const auto* e = static_cast<const ConditionalExpressionStatementNode*>(expr);
            collectExpressionRefs(e->condition.get(), refs);
            collectExpressionRefs(e->whenTrue.get(), refs);
            collectExpressionRefs(e->whenFalse.get(), refs);
            break;
        }
        case ExpressionStatementNodeType::ObjectPropertyAccess: {
            const auto* e = static_cast<const ObjectPropertyAccessExpressionNode*>(expr);
            collectExpressionRefs(e->base.get(), refs);
            insertSorted(refs, expressionName(expr));
            break;
        }
        case ExpressionStatementNodeType::ObjectMethodCall: {
            const auto* e = static_cast<const ObjectMethodCallExpressionNode*>(expr);
            collectExpressionRefs(e->base.get(), refs);
            for (const auto& arg : e->arguments) collectExpressionRefs(arg.get(), refs);
            break;
        }
        case ExpressionStatementNodeType::StringLiteral:
        case ExpressionStatementNodeType::NumberLiteral:
        case ExpressionStatementNodeType::BooleanLiteral:
            break;
    }
}

void countAttributeKind(AttributeKindCounts& counts, JtmlAttributeKind kind) {
    switch (kind) {
        case JtmlAttributeKind::Literal:     ++counts.literal; break;
        case JtmlAttributeKind::Boolean:     ++counts.boolean; break;
        case JtmlAttributeKind::Reactive:    ++counts.reactive; break;
        case JtmlAttributeKind::Event:       ++counts.event; break;
        case JtmlAttributeKind::Special:     ++counts.special; break;
        case JtmlAttributeKind::Passthrough: ++counts.passthrough; break;
    }
}

bool attributeLiteral(const JtmlElementNode& elem, const std::string& key, std::string& value) {
    for (const auto& attr : elem.attributes) {
        if (attr.key != key) continue;
        auto classified = classifyJtmlAttribute(attr);
        if (classified.kind == JtmlAttributeKind::Literal ||
            classified.kind == JtmlAttributeKind::Special ||
            classified.kind == JtmlAttributeKind::Passthrough) {
            value = classified.literalValue;
            return true;
        }
    }
    return false;
}

std::string effectVariableFromFunction(const std::string& name) {
    const std::string prefix = "__effect_";
    if (name.rfind(prefix, 0) != 0) return "";
    const size_t afterPrefix = prefix.size();
    const size_t lastUnderscore = name.rfind('_');
    if (lastUnderscore == std::string::npos || lastUnderscore <= afterPrefix) return "";
    return name.substr(afterPrefix, lastUnderscore - afterPrefix);
}

struct SemanticSets {
    std::set<std::string> state;
    std::set<std::string> constants;
    std::set<std::string> derived;
    std::set<std::string> actions;
    std::set<std::string> components;
    std::set<std::tuple<std::string, std::string, std::string, std::string, std::string,
                        std::string, std::string, std::string, bool, int, int, int, int>> componentDefinitions;
    std::set<std::tuple<std::string, std::string, int, std::string, std::string, std::string, int>> componentInstances;
    std::set<std::string> routes;
    std::set<std::tuple<std::string, std::string, std::string, std::string>> routeRecords;
    std::set<std::string> fetches;
    std::set<std::tuple<std::string, std::string, std::string, std::string, std::string,
                        std::string, std::string, std::string, std::string, std::string, std::string,
                        std::string, std::string, bool, bool, bool>> fetchRecords;
    std::set<std::string> stores;
    std::set<std::string> effects;
    std::set<std::string> imports;
    std::set<std::string> externs;
    std::set<std::string> uiPrimitives;
    std::set<std::tuple<std::string, std::string, bool, bool, bool, bool, bool, bool, bool, bool>> uiUses;
    std::set<std::tuple<std::string, std::string, std::string>> uiModifiers;
    std::set<std::tuple<std::string, std::string, std::string>> edges;
    AttributeKindCounts attributes;
    int rawStyleAttributeCount = 0;
    int semanticPrimitiveRawStyleCount = 0;
    int styleBlocks = 0;
    int rawCssBlocks = 0;
    int rawHtmlBlocks = 0;
    int authorThemeTokenCount = 0;
    int themeTokenCount = 0;
    int timelineCount = 0;
};

void collectFromNodes(const std::vector<std::unique_ptr<ASTNode>>& nodes,
                      SemanticSets& out,
                      const std::string& context = "");

void addEdge(SemanticSets& out,
             const std::string& from,
             const std::string& to,
             const std::string& kind) {
    if (from.empty() || to.empty() || kind.empty()) return;
    out.edges.insert({from, to, kind});
}

void addExpressionEdges(SemanticSets& out,
                        const std::string& from,
                        const ExpressionStatementNode* expr,
                        const std::string& kind) {
    std::set<std::string> refs;
    collectExpressionRefs(expr, refs);
    for (const auto& ref : refs) addEdge(out, from, ref, kind);
}

void addDelimitedEdges(SemanticSets& out,
                       const std::string& from,
                       const std::string& csv,
                       const std::string& kind) {
    std::istringstream input(csv);
    std::string part;
    while (std::getline(input, part, ',')) addEdge(out, from, trimCopy(part), kind);
}

void addLiteralReferenceEdge(SemanticSets& out,
                             const JtmlElementNode& elem,
                             const std::string& key,
                             const std::string& kind = "binds") {
    std::string value;
    if (!attributeLiteral(elem, key, value)) return;
    addEdge(out, "ui:@" + elem.tagName + "." + key, value, kind);
}

void addTimelineReferenceEdges(SemanticSets& out, const JtmlElementNode& elem) {
    std::string animates;
    if (!attributeLiteral(elem, "data-jtml-timeline-animates", animates)) return;
    std::istringstream input(animates);
    std::string item;
    while (std::getline(input, item, ',')) {
        const auto colon = item.find(':');
        const auto name = trimCopy(colon == std::string::npos ? item : item.substr(0, colon));
        addEdge(out, "ui:@" + elem.tagName + ".data-jtml-timeline-animates", name, "binds");
    }
}

std::vector<std::string> classTokens(const std::string& classValue) {
    std::vector<std::string> out;
    std::istringstream input(classValue);
    std::string token;
    while (input >> token) out.push_back(token);
    return out;
}

std::pair<std::string, std::string> semanticModifierFromClass(const std::string& token) {
    static const std::vector<std::string> prefixes = {
        "jtml-cols-", "jtml-gap-", "jtml-pad-", "jtml-radius-", "jtml-shadow-",
        "jtml-tone-", "jtml-align-", "jtml-justify-", "jtml-width-", "jtml-surface-"
    };
    for (const auto& prefix : prefixes) {
        if (token.rfind(prefix, 0) != 0) continue;
        std::string name = prefix.substr(std::string("jtml-").size());
        if (!name.empty() && name.back() == '-') name.pop_back();
        return {name, token.substr(prefix.size())};
    }
    return {};
}

void addSemanticUiModifiers(SemanticSets& out,
                            const std::string& primitive,
                            const JtmlElementNode& elem) {
    std::string classValue;
    if (!attributeLiteral(elem, "class", classValue)) return;
    for (const auto& token : classTokens(classValue)) {
        const auto [modifier, value] = semanticModifierFromClass(token);
        if (modifier.empty()) continue;
        out.uiModifiers.insert({primitive, modifier, value});
        addEdge(out, "primitive:" + primitive, modifier + ":" + value, "modifies");
    }
}

bool elementHasSemanticTitle(const JtmlElementNode& elem) {
    std::string value;
    if (attributeLiteral(elem, "title", value)) return true;
    for (const auto& child : elem.content) {
        if (!child || child->getType() != ASTNodeType::JtmlElement) continue;
        const auto& childElem = static_cast<const JtmlElementNode&>(*child);
        std::string classValue;
        if (childElem.tagName == "h2" &&
            attributeLiteral(childElem, "class", classValue) &&
            classValue.find("jtml-panel-title") != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool expressionHasUserText(const ExpressionStatementNode* expr) {
    if (!expr) return false;
    const auto text = trimCopy(expr->toString());
    return !text.empty() && text != "\"\"" && text != "''";
}

bool elementHasLabelText(const JtmlElementNode& elem) {
    std::string value;
    if (attributeLiteral(elem, "title", value) ||
        attributeLiteral(elem, "aria-label", value) ||
        attributeLiteral(elem, "placeholder", value) ||
        attributeLiteral(elem, "alt", value)) {
        return !trimCopy(value).empty();
    }
    for (const auto& child : elem.content) {
        if (!child) continue;
        if (child->getType() == ASTNodeType::ShowStatement) {
            const auto& show = static_cast<const ShowStatementNode&>(*child);
            if (expressionHasUserText(show.expr.get())) return true;
        }
        if (child->getType() == ASTNodeType::JtmlElement &&
            elementHasLabelText(static_cast<const JtmlElementNode&>(*child))) {
            return true;
        }
    }
    return false;
}

bool elementHasControl(const JtmlElementNode& elem) {
    static const std::set<std::string> controlTags = {
        "input", "select", "textarea"
    };
    if (controlTags.count(elem.tagName) > 0) return true;
    for (const auto& child : elem.content) {
        if (!child || child->getType() != ASTNodeType::JtmlElement) continue;
        if (elementHasControl(static_cast<const JtmlElementNode&>(*child))) return true;
    }
    return false;
}

bool elementHasActionBinding(const JtmlElementNode& elem) {
    for (const auto& attr : elem.attributes) {
        const auto classified = classifyJtmlAttribute(attr);
        if (classified.kind == JtmlAttributeKind::Event && attr.value) return true;
    }
    for (const auto& child : elem.content) {
        if (!child || child->getType() != ASTNodeType::JtmlElement) continue;
        if (elementHasActionBinding(static_cast<const JtmlElementNode&>(*child))) return true;
    }
    return false;
}

bool looksLikeDismissAction(const std::string& name) {
    const auto lower = lowerCopy(name);
    return lower.find("close") != std::string::npos ||
           lower.find("dismiss") != std::string::npos ||
           lower.find("cancel") != std::string::npos ||
           lower.find("hide") != std::string::npos;
}

bool elementHasDismissAction(const JtmlElementNode& elem) {
    for (const auto& attr : elem.attributes) {
        const auto classified = classifyJtmlAttribute(attr);
        if (classified.kind == JtmlAttributeKind::Event && attr.value &&
            looksLikeDismissAction(expressionName(attr.value.get()))) {
            return true;
        }
        if ((attr.key == "data-jtml-dismiss" || attr.key == "data-dismiss" ||
             attr.key == "aria-controls") &&
            attr.value) {
            return true;
        }
    }
    for (const auto& child : elem.content) {
        if (!child || child->getType() != ASTNodeType::JtmlElement) continue;
        if (elementHasDismissAction(static_cast<const JtmlElementNode&>(*child))) return true;
    }
    return false;
}

bool elementHasNavigationTarget(const JtmlElementNode& elem) {
    std::string value;
    if (attributeLiteral(elem, "data-jtml-href", value) ||
        attributeLiteral(elem, "href", value)) {
        return true;
    }
    for (const auto& child : elem.content) {
        if (!child || child->getType() != ASTNodeType::JtmlElement) continue;
        if (elementHasNavigationTarget(static_cast<const JtmlElementNode&>(*child))) return true;
    }
    return false;
}

bool elementHasPrimitiveChild(const JtmlElementNode& elem, const std::string& primitive) {
    for (const auto& child : elem.content) {
        if (!child || child->getType() != ASTNodeType::JtmlElement) continue;
        const auto& childElem = static_cast<const JtmlElementNode&>(*child);
        std::string value;
        if (attributeLiteral(childElem, "data-jtml-ui", value) && value == primitive) return true;
        if (elementHasPrimitiveChild(childElem, primitive)) return true;
    }
    return false;
}

void collectFromElement(const JtmlElementNode& elem, SemanticSets& out, const std::string& context) {
    if (elem.tagName == "style") ++out.styleBlocks;
    if (elem.tagName == "raw") ++out.rawHtmlBlocks;

    std::string primitiveName;
    const bool semanticPrimitive = attributeLiteral(elem, "data-jtml-ui", primitiveName);

    for (const auto& attr : elem.attributes) {
        const auto classified = classifyJtmlAttribute(attr);
        countAttributeKind(out.attributes, classified.kind);
        if (attr.key == "style") {
            ++out.rawStyleAttributeCount;
            if (semanticPrimitive) ++out.semanticPrimitiveRawStyleCount;
        }
        if (classified.kind == JtmlAttributeKind::Reactive) {
            addExpressionEdges(out, "ui:@" + elem.tagName + "." + attr.key, attr.value.get(), "binds");
        } else if (classified.kind == JtmlAttributeKind::Event && attr.value) {
            addEdge(out, "ui:@" + elem.tagName + "." + attr.key, expressionName(attr.value.get()), "triggers");
        }
    }

    std::string value;
    if (attributeLiteral(elem, "data-jtml-fetch", value)) {
        insertSorted(out.fetches, value);
        std::string url;
        std::string method;
        std::string bodyExpr;
        std::string refreshAction;
        std::string cache;
        std::string credentials;
        std::string timeoutMs;
        std::string retryCount;
        std::string stalePolicy;
        std::string group;
        std::string cacheKeyExpr;
        std::string revalidateMs;
        std::string dedupeValue;
        std::string backgroundValue;
        std::string lazyValue;
        attributeLiteral(elem, "data-url", url);
        attributeLiteral(elem, "data-method", method);
        attributeLiteral(elem, "data-body-expr", bodyExpr);
        attributeLiteral(elem, "data-refresh-action", refreshAction);
        attributeLiteral(elem, "data-cache", cache);
        attributeLiteral(elem, "data-credentials", credentials);
        attributeLiteral(elem, "data-timeout-ms", timeoutMs);
        attributeLiteral(elem, "data-retry", retryCount);
        attributeLiteral(elem, "data-stale", stalePolicy);
        attributeLiteral(elem, "data-group", group);
        attributeLiteral(elem, "data-cache-key-expr", cacheKeyExpr);
        attributeLiteral(elem, "data-revalidate-ms", revalidateMs);
        attributeLiteral(elem, "data-dedupe", dedupeValue);
        attributeLiteral(elem, "data-background", backgroundValue);
        attributeLiteral(elem, "data-lazy", lazyValue);
        out.fetchRecords.insert({value, url, method, bodyExpr, refreshAction, cache, credentials,
                                 timeoutMs, retryCount, stalePolicy, group, cacheKeyExpr, revalidateMs,
                                 dedupeValue == "true", backgroundValue == "true", lazyValue == "true"});
        if (attributeLiteral(elem, "data-refresh-action", refreshAction)) {
            addEdge(out, "fetch:" + value, refreshAction, "refresh-action");
        }
    }
    if (attributeLiteral(elem, "data-jtml-extern-action", value)) {
        insertSorted(out.externs, value);
        std::string target;
        if (attributeLiteral(elem, "data-window", target)) {
            addEdge(out, "extern:" + value, target, "calls");
        }
    }
    if (attributeLiteral(elem, "data-jtml-route", value)) {
        std::string routeName;
        attributeLiteral(elem, "data-jtml-route-name", routeName);
        std::string routeParams;
        attributeLiteral(elem, "data-jtml-route-params", routeParams);
        std::string routeLoad;
        attributeLiteral(elem, "data-jtml-route-load", routeLoad);
        out.routeRecords.insert({value, routeName, joinValues(splitCommaList(routeParams)),
                                 joinValues(splitCommaList(routeLoad))});
        if (attributeLiteral(elem, "data-jtml-route-name", routeName)) {
            insertSorted(out.routes, value + " -> " + routeName);
            addEdge(out, "route:" + value, "component:" + routeName, "renders");
        } else {
            insertSorted(out.routes, value);
        }
        if (!routeLoad.empty()) {
            addDelimitedEdges(out, "route:" + value, routeLoad, "loads");
        }
    }
    if (attributeLiteral(elem, "data-jtml-component-def", value)) {
        insertSorted(out.components, value);
        std::string params;
        std::string bodyHex;
        std::string sourceLine;
        attributeLiteral(elem, "data-jtml-component-def-params", params);
        attributeLiteral(elem, "data-jtml-component-body-hex", bodyHex);
        attributeLiteral(elem, "data-jtml-source-line", sourceLine);
        const auto summary = summarizeComponentBody(bodyHex);
        out.componentDefinitions.insert({
            value,
            params,
            joinValues(toVector(summary.localState)),
            joinValues(toVector(summary.localDerived)),
            joinValues(toVector(summary.localActions)),
            joinValues(toVector(summary.localEffects)),
            joinValues(toVector(summary.eventBindings)),
            bodyHex,
            summary.hasSlot,
            summary.bodyNodeCount,
            summary.rootTemplateNodeCount,
            summary.slotCount,
            parseIntLiteral(sourceLine)
        });
    } else if (attributeLiteral(elem, "data-jtml-component", value)) {
        insertSorted(out.components, value);
        std::string id;
        std::string instanceId;
        std::string role;
        std::string params;
        std::string locals;
        std::string sourceLine;
        attributeLiteral(elem, "data-jtml-instance", id);
        attributeLiteral(elem, "data-jtml-instance-id", instanceId);
        attributeLiteral(elem, "data-jtml-component-role", role);
        attributeLiteral(elem, "data-jtml-component-params", params);
        attributeLiteral(elem, "data-jtml-component-locals", locals);
        attributeLiteral(elem, "data-jtml-source-line", sourceLine);
        out.componentInstances.insert({id, value, parseIntLiteral(instanceId), role.empty() ? "component" : role,
                                       params, locals, parseIntLiteral(sourceLine)});
    }
    if (attributeLiteral(elem, "data-jtml-timeline", value)) {
        ++out.timelineCount;
    }
    if (semanticPrimitive) {
        insertSorted(out.uiPrimitives, primitiveName);
        addEdge(out, "ui:@" + elem.tagName, "primitive:" + primitiveName, "uses");
        addSemanticUiModifiers(out, primitiveName, elem);
        std::string ariaLabel;
        out.uiUses.insert({
            primitiveName,
            elem.tagName,
            elementHasSemanticTitle(elem),
            attributeLiteral(elem, "aria-label", ariaLabel),
            elementHasLabelText(elem),
            elementHasControl(elem),
            elementHasActionBinding(elem),
            elementHasNavigationTarget(elem),
            elementHasPrimitiveChild(elem, "tab"),
            elementHasDismissAction(elem)
        });
    }
    if (elem.tagName == "style") {
        for (const auto& child : elem.content) {
            if (child && child->getType() == ASTNodeType::ShowStatement) {
                const auto& show = static_cast<const ShowStatementNode&>(*child);
                const auto text = show.expr ? show.expr->toString() : "";
                size_t pos = 0;
                while ((pos = text.find("--jtml-", pos)) != std::string::npos) {
                    ++out.themeTokenCount;
                    pos += 7;
                }
            }
        }
    }

    addLiteralReferenceEdge(out, elem, "data-jtml-chart-data");
    addLiteralReferenceEdge(out, elem, "data-jtml-media-controller");
    addLiteralReferenceEdge(out, elem, "data-jtml-image-src");
    addLiteralReferenceEdge(out, elem, "data-jtml-image-into");
    addTimelineReferenceEdges(out, elem);

    collectFromNodes(elem.content, out, context);
}

void collectFromNode(const ASTNode& node, SemanticSets& out, const std::string& context) {
    switch (node.getType()) {
        case ASTNodeType::JtmlElement:
            collectFromElement(static_cast<const JtmlElementNode&>(node), out, context);
            break;
        case ASTNodeType::DefineStatement: {
            const auto& stmt = static_cast<const DefineStatementNode&>(node);
            if (stmt.isConst) insertSorted(out.constants, stmt.identifier);
            else insertSorted(out.state, stmt.identifier);
            addExpressionEdges(out, stmt.identifier, stmt.expression.get(), stmt.isConst ? "const-initializer" : "state-initializer");
            break;
        }
        case ASTNodeType::DeriveStatement: {
            const auto& stmt = static_cast<const DeriveStatementNode&>(node);
            insertSorted(out.derived, stmt.identifier);
            addExpressionEdges(out, stmt.identifier, stmt.expression.get(), "derives");
            break;
        }
        case ASTNodeType::ShowStatement:
            addExpressionEdges(out, "ui:show", static_cast<const ShowStatementNode&>(node).expr.get(), "renders");
            break;
        case ASTNodeType::AssignmentStatement: {
            const auto& stmt = static_cast<const AssignmentStatementNode&>(node);
            const auto lhs = expressionName(stmt.lhs.get());
            if (!context.empty()) {
                addEdge(out, context, lhs, "writes");
                addExpressionEdges(out, context, stmt.rhs.get(), "reads");
            } else {
                addExpressionEdges(out, lhs, stmt.rhs.get(), "writes-from");
            }
            break;
        }
        case ASTNodeType::ExpressionStatement:
            addExpressionEdges(out, "expr", static_cast<const ExpressionNode&>(node).expression.get(), "reads");
            break;
        case ASTNodeType::FunctionDeclaration: {
            const auto& stmt = static_cast<const FunctionDeclarationNode&>(node);
            const auto effectVar = effectVariableFromFunction(stmt.name);
            if (!effectVar.empty()) {
                insertSorted(out.effects, effectVar);
                addEdge(out, "effect:" + effectVar, effectVar, "subscribes");
                collectFromNodes(stmt.body, out, "effect:" + effectVar);
            } else {
                insertSorted(out.actions, stmt.name);
                collectFromNodes(stmt.body, out, "action:" + stmt.name);
            }
            break;
        }
        case ASTNodeType::ImportStatement: {
            const auto& stmt = static_cast<const ImportStatementNode&>(node);
            insertSorted(out.imports, stmt.path);
            addEdge(out, "module", stmt.path, "imports");
            break;
        }
        case ASTNodeType::StoreStatement: {
            const auto& stmt = static_cast<const StoreStatementNode&>(node);
            insertSorted(out.stores, stmt.targetScope);
            break;
        }
        case ASTNodeType::IfStatement: {
            const auto& stmt = static_cast<const IfStatementNode&>(node);
            addExpressionEdges(out, "if", stmt.condition.get(), "condition");
            collectFromNodes(stmt.thenStatements, out, context);
            collectFromNodes(stmt.elseStatements, out, context);
            break;
        }
        case ASTNodeType::WhileStatement: {
            const auto& stmt = static_cast<const WhileStatementNode&>(node);
            addExpressionEdges(out, "while", stmt.condition.get(), "condition");
            collectFromNodes(stmt.body, out, context);
            break;
        }
        case ASTNodeType::ForStatement: {
            const auto& stmt = static_cast<const ForStatementNode&>(node);
            addExpressionEdges(out, "for:" + stmt.iteratorName, stmt.iterableExpression.get(), "iterates");
            addExpressionEdges(out, "for:" + stmt.iteratorName, stmt.rangeEndExpr.get(), "iterates");
            collectFromNodes(stmt.body, out, context);
            break;
        }
        case ASTNodeType::BlockStatement:
            collectFromNodes(static_cast<const BlockStatementNode&>(node).statements, out, context);
            break;
        case ASTNodeType::TryExceptThen: {
            const auto& stmt = static_cast<const TryExceptThenNode&>(node);
            collectFromNodes(stmt.tryBlock, out, context);
            collectFromNodes(stmt.catchBlock, out, context);
            collectFromNodes(stmt.finallyBlock, out, context);
            break;
        }
        default:
            break;
    }
}

void collectFromNodes(const std::vector<std::unique_ptr<ASTNode>>& nodes,
                      SemanticSets& out,
                      const std::string& context) {
    for (const auto& node : nodes) {
        if (node) collectFromNode(*node, out, context);
    }
}

int leadingSpaceCount(const std::string& raw) {
    int count = 0;
    for (char c : raw) {
        if (c == ' ') {
            ++count;
        } else if (c == '\t') {
            count += 2;
        } else {
            break;
        }
    }
    return count;
}

bool isThemeTokenLine(const std::vector<std::string>& tokens) {
    if (tokens.empty()) return false;
    static const std::set<std::string> tokenKinds = {
        "color", "space", "radius", "shadow", "font"
    };
    return tokenKinds.count(tokens[0]) > 0 && tokens.size() >= 3;
}

void collectFriendlySourceFallback(const std::string& source, SemanticSets& out) {
    std::istringstream input(source);
    std::string raw;
    bool insideTheme = false;
    int themeIndent = -1;
    while (std::getline(input, raw)) {
        std::string line = trimCopy(stripInlineComment(raw));
        if (line.empty() || line == "jtml 2" || line == "jtl 1") continue;
        auto tokens = words(line);
        if (tokens.empty()) continue;

        const std::string head = tokens[0];
        const int indent = leadingSpaceCount(raw);
        if (insideTheme && indent <= themeIndent) {
            insideTheme = false;
            themeIndent = -1;
        }
        if (head == "theme") {
            insideTheme = true;
            themeIndent = indent;
            continue;
        }
        if (insideTheme && isThemeTokenLine(tokens)) {
            ++out.authorThemeTokenCount;
            continue;
        }
        const auto second = [&]() {
            return tokens.size() > 1 ? cleanIdentifier(tokens[1]) : std::string{};
        };

        if (head == "store" && tokens.size() > 1) {
            insertSorted(out.stores, second());
        } else if (head == "css" && tokens.size() > 1 && tokens[1] == "raw") {
            ++out.rawCssBlocks;
        } else if (head == "extern" && tokens.size() > 1) {
            insertSorted(out.externs, second());
        } else if (head == "effect" && tokens.size() > 1) {
            insertSorted(out.effects, second());
        } else if (head == "route" && tokens.size() >= 4) {
            insertSorted(out.routes, unquoteToken(tokens[1]) + " -> " + cleanIdentifier(tokens[3]));
        } else if (head == "use" && tokens.size() > 1) {
            std::string path;
            if (tokens.size() == 2) {
                path = unquoteToken(tokens[1]);
            } else {
                const auto from = std::find(tokens.begin(), tokens.end(), "from");
                if (from != tokens.end() && from + 1 != tokens.end()) {
                    path = unquoteToken(*(from + 1));
                }
            }
            insertSorted(out.imports, path);
            addEdge(out, "module", path, "imports");
        } else if (head == "make" && tokens.size() > 1) {
            insertSorted(out.components, second());
        } else if ((head == "let" || head == "define") && tokens.size() > 1) {
            const auto name = second();
            if (line.find(" fetch ") != std::string::npos ||
                line.find("= fetch ") != std::string::npos) {
                insertSorted(out.fetches, name);
            }
        }
    }
}

std::vector<SemanticEdge> edgesToVector(
    const std::set<std::tuple<std::string, std::string, std::string>>& values) {
    std::vector<SemanticEdge> edges;
    for (const auto& edge : values) {
        edges.push_back({std::get<0>(edge), std::get<1>(edge), std::get<2>(edge)});
    }
    return edges;
}

std::vector<SemanticUiModifier> modifiersToVector(
    const std::set<std::tuple<std::string, std::string, std::string>>& values) {
    std::vector<SemanticUiModifier> modifiers;
    for (const auto& modifier : values) {
        modifiers.push_back({std::get<0>(modifier), std::get<1>(modifier), std::get<2>(modifier)});
    }
    return modifiers;
}

std::vector<SemanticUiUse> uiUsesToVector(
    const std::set<std::tuple<std::string, std::string, bool, bool, bool, bool, bool, bool, bool, bool>>& values) {
    std::vector<SemanticUiUse> uses;
    for (const auto& use : values) {
        uses.push_back({
            std::get<0>(use),
            std::get<1>(use),
            std::get<2>(use),
            std::get<3>(use),
            std::get<4>(use),
            std::get<5>(use),
            std::get<6>(use),
            std::get<7>(use),
            std::get<8>(use),
            std::get<9>(use),
        });
    }
    return uses;
}

std::vector<SemanticRoute> routesToVector(
    const std::set<std::tuple<std::string, std::string, std::string, std::string>>& values) {
    std::vector<SemanticRoute> routes;
    for (const auto& route : values) {
        routes.push_back({
            std::get<0>(route),
            std::get<1>(route),
            splitCommaList(std::get<2>(route)),
            splitCommaList(std::get<3>(route)),
        });
    }
    return routes;
}

std::vector<SemanticFetch> fetchesToVector(
    const std::set<std::tuple<std::string, std::string, std::string, std::string, std::string,
                              std::string, std::string, std::string, std::string, std::string, std::string,
                              std::string, std::string, bool, bool, bool>>& values) {
    std::vector<SemanticFetch> fetches;
    for (const auto& fetch : values) {
        fetches.push_back({
            std::get<0>(fetch),
            std::get<1>(fetch),
            std::get<2>(fetch),
            std::get<3>(fetch),
            std::get<4>(fetch),
            std::get<5>(fetch),
            std::get<6>(fetch),
            std::get<7>(fetch),
            std::get<8>(fetch),
            std::get<9>(fetch),
            std::get<10>(fetch),
            std::get<11>(fetch),
            std::get<12>(fetch),
            std::get<13>(fetch),
            std::get<14>(fetch),
            std::get<15>(fetch),
        });
    }
    return fetches;
}

std::vector<SemanticComponentDefinition> componentDefinitionsToVector(
    const std::set<std::tuple<std::string, std::string, std::string, std::string, std::string,
                              std::string, std::string, std::string, bool, int, int, int, int>>& values) {
    std::vector<SemanticComponentDefinition> definitions;
    for (const auto& definition : values) {
        definitions.push_back({
            std::get<0>(definition),
            splitCommaList(std::get<1>(definition)),
            splitCommaList(std::get<2>(definition)),
            splitCommaList(std::get<3>(definition)),
            splitCommaList(std::get<4>(definition)),
            splitCommaList(std::get<5>(definition)),
            splitCommaList(std::get<6>(definition)),
            std::get<7>(definition),
            std::get<8>(definition),
            std::get<9>(definition),
            std::get<10>(definition),
            std::get<11>(definition),
            std::get<12>(definition),
        });
    }
    return definitions;
}

std::vector<SemanticComponentInstance> componentInstancesToVector(
    const std::set<std::tuple<std::string, std::string, int, std::string, std::string, std::string, int>>& values) {
    std::vector<SemanticComponentInstance> instances;
    for (const auto& instance : values) {
        instances.push_back({
            std::get<0>(instance),
            std::get<1>(instance),
            std::get<2>(instance),
            std::get<3>(instance),
            splitPropertyMap(std::get<4>(instance)),
            splitPropertyMap(std::get<5>(instance)),
            std::get<6>(instance),
        });
    }
    return instances;
}

} // namespace

SemanticProgram analyzeSemanticProgram(
    const std::vector<std::unique_ptr<ASTNode>>& program,
    const std::string& originalSource) {
    SemanticSets sets;
    collectFromNodes(program, sets);
    collectFriendlySourceFallback(originalSource, sets);

    SemanticProgram semantic;
    semantic.state = toVector(sets.state);
    semantic.constants = toVector(sets.constants);
    semantic.derived = toVector(sets.derived);
    semantic.actions = toVector(sets.actions);
    semantic.components = toVector(sets.components);
    semantic.componentDefinitions = componentDefinitionsToVector(sets.componentDefinitions);
    semantic.componentInstances = componentInstancesToVector(sets.componentInstances);
    semantic.routes = toVector(sets.routes);
    semantic.routeRecords = routesToVector(sets.routeRecords);
    semantic.fetches = toVector(sets.fetches);
    semantic.fetchRecords = fetchesToVector(sets.fetchRecords);
    semantic.stores = toVector(sets.stores);
    semantic.effects = toVector(sets.effects);
    semantic.imports = toVector(sets.imports);
    semantic.externs = toVector(sets.externs);
    semantic.uiPrimitives = toVector(sets.uiPrimitives);
    semantic.uiUses = uiUsesToVector(sets.uiUses);
    semantic.uiModifiers = modifiersToVector(sets.uiModifiers);
    semantic.dependencies = edgesToVector(sets.edges);
    semantic.attributes = sets.attributes;
    semantic.rawStyleAttributeCount = sets.rawStyleAttributeCount;
    semantic.semanticPrimitiveRawStyleCount = sets.semanticPrimitiveRawStyleCount;
    semantic.styleBlocks = sets.styleBlocks;
    semantic.rawCssBlocks = sets.rawCssBlocks;
    semantic.rawHtmlBlocks = sets.rawHtmlBlocks;
    semantic.authorThemeTokenCount = sets.authorThemeTokenCount;
    semantic.themeTokenCount = sets.themeTokenCount;
    semantic.timelineCount = sets.timelineCount;
    return semantic;
}

namespace {

bool containsValue(const std::vector<std::string>& values, const std::string& value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

std::string rootName(std::string value) {
    value = trimCopy(std::move(value));
    if (value.empty()) return {};
    const auto colon = value.find(':');
    if (colon != std::string::npos && value.rfind("http", 0) != 0) {
        value = value.substr(0, colon);
    }
    const auto dot = value.find('.');
    if (dot != std::string::npos) value = value.substr(0, dot);
    return cleanIdentifier(value);
}

std::string memberName(std::string value) {
    value = trimCopy(std::move(value));
    const auto colon = value.find(':');
    if (colon != std::string::npos && value.rfind("http", 0) != 0) {
        value = value.substr(0, colon);
    }
    const auto dot = value.rfind('.');
    if (dot == std::string::npos || dot + 1 >= value.size()) return {};
    return cleanIdentifier(value.substr(dot + 1));
}

void addUnique(std::vector<std::string>& values, const std::string& value) {
    if (!value.empty() && std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

std::vector<std::string> sortedVector(std::set<std::string> values) {
    return {values.begin(), values.end()};
}

std::string actionDisplayName(const SemanticProgram& semantic, const std::string& name) {
    for (const auto& store : semantic.stores) {
        const std::string prefix = store + "_";
        if (name.rfind(prefix, 0) == 0 && name.size() > prefix.size()) {
            return name.substr(prefix.size());
        }
    }
    return name;
}

std::string stateDisplayName(const SemanticProgram& semantic, const std::string& name) {
    for (const auto& store : semantic.stores) {
        const std::string prefix = store + ".";
        if (name.rfind(prefix, 0) == 0 && name.size() > prefix.size()) {
            return name.substr(prefix.size());
        }
    }
    return name;
}

bool isActionEdge(const SemanticEdge& edge) {
    return edge.from.rfind("action:", 0) == 0;
}

std::string actionNameFromEdge(const SemanticEdge& edge) {
    if (!isActionEdge(edge)) return {};
    return edge.from.substr(std::string("action:").size());
}

bool isUiObservationEdge(const SemanticEdge& edge) {
    if (edge.kind == "renders" || edge.kind == "binds") {
        return edge.from.rfind("ui:", 0) == 0;
    }
    if (edge.kind == "condition" && (edge.from == "if" || edge.from == "while")) return true;
    if (edge.kind == "iterates" && edge.from.rfind("for:", 0) == 0) return true;
    return false;
}

void observeRef(const SemanticProgram& semantic,
                const std::string& ref,
                std::set<std::string>& observedState,
                std::set<std::string>& observedDerived) {
    const auto root = rootName(ref);
    if (containsValue(semantic.state, root)) observedState.insert(root);
    if (containsValue(semantic.derived, root)) observedDerived.insert(root);
    if (containsValue(semantic.state, ref)) observedState.insert(ref);
    if (containsValue(semantic.derived, ref)) observedDerived.insert(ref);
    const auto member = memberName(ref);
    if (containsValue(semantic.state, member)) observedState.insert(member);
    if (containsValue(semantic.derived, member)) observedDerived.insert(member);
}

bool writeIsObserved(const SemanticProgram& semantic,
                     const std::string& write,
                     const std::set<std::string>& observedState,
                     const std::set<std::string>& observedDerived) {
    if (observedState.count(write) || observedDerived.count(write)) return true;
    const auto root = rootName(write);
    if (observedState.count(root) || observedDerived.count(root)) return true;
    const auto member = memberName(write);
    return observedState.count(member) || observedDerived.count(member);
}

bool isLayoutPrimitive(const std::string& primitive) {
    static const std::set<std::string> names = {
        "app", "shell", "topbar", "sidebar", "content", "grid", "stack",
        "cluster", "split", "toolbar", "tabs", "spacer"
    };
    return names.count(primitive) > 0;
}

const SemanticUiModifierSpec* semanticUiModifierSpec(const std::string& name) {
    const auto& modifiers = semanticUiCatalog().modifiers;
    const auto it = std::find_if(modifiers.begin(), modifiers.end(), [&](const auto& modifier) {
        return modifier.name == name;
    });
    return it == modifiers.end() ? nullptr : &*it;
}

} // namespace

SemanticUsageReport analyzeSemanticUsage(const SemanticProgram& semantic) {
    SemanticUsageReport report;
    std::set<std::string> observedState;
    std::set<std::string> observedDerived;
    std::set<std::string> usedByActionsOrEffects;
    std::set<std::string> boundActions;
    std::set<std::string> generatedFetchActions;
    std::map<std::string, SemanticActionProfile> profiles;
    std::map<std::string, std::vector<std::string>> rawWrites;

    for (const auto& action : semantic.actions) {
        profiles[action].name = actionDisplayName(semantic, action);
    }

    for (const auto& edge : semantic.dependencies) {
        if (isUiObservationEdge(edge)) {
            observeRef(semantic, edge.to, observedState, observedDerived);
        }

        if (edge.kind == "reads" || edge.kind == "writes") {
            const auto action = actionNameFromEdge(edge);
            if (!action.empty()) {
                auto& profile = profiles[action];
                if (profile.name.empty()) profile.name = actionDisplayName(semantic, action);
                const auto visibleName = stateDisplayName(semantic, edge.to);
                if (edge.kind == "reads") addUnique(profile.reads, visibleName);
                if (edge.kind == "writes") {
                    addUnique(profile.writes, visibleName);
                    addUnique(rawWrites[action], edge.to);
                }
            }
            usedByActionsOrEffects.insert(rootName(edge.to));
            const auto member = memberName(edge.to);
            if (!member.empty()) usedByActionsOrEffects.insert(member);
        }

        if (edge.kind == "triggers") {
            const auto action = edge.to;
            if (!action.empty()) {
                auto& profile = profiles[action];
                if (profile.name.empty()) profile.name = actionDisplayName(semantic, action);
                addUnique(profile.triggers, "ui");
                boundActions.insert(action);
            }
        }
        if (edge.kind == "refresh-action") {
            generatedFetchActions.insert(edge.to);
        }
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& edge : semantic.dependencies) {
            if (edge.kind != "derives") continue;
            if (!observedDerived.count(edge.from)) continue;
            const auto beforeState = observedState.size();
            const auto beforeDerived = observedDerived.size();
            observeRef(semantic, edge.to, observedState, observedDerived);
            changed = changed ||
                      observedState.size() != beforeState ||
                      observedDerived.size() != beforeDerived;
        }
    }

    for (auto& [action, profile] : profiles) {
        for (const auto& write : rawWrites[action]) {
            if (writeIsObserved(semantic, write, observedState, observedDerived)) {
                profile.hasVisibleEffect = true;
                break;
            }
        }
    }

    for (const auto& state : semantic.state) {
        if (containsValue(semantic.fetches, state)) {
            if (observedState.count(state)) report.observedState.push_back(state);
            continue;
        }
        if (observedState.count(state)) {
            report.observedState.push_back(state);
        } else if (usedByActionsOrEffects.count(state)) {
            report.zombieState.push_back(state);
        } else {
            report.deadState.push_back(state);
        }
    }

    for (const auto& derived : semantic.derived) {
        if (observedDerived.count(derived)) report.observedDerived.push_back(derived);
        else report.unusedDerived.push_back(derived);
    }

    for (const auto& action : semantic.actions) {
        if (boundActions.count(action)) report.boundActions.push_back(actionDisplayName(semantic, action));
        else if (generatedFetchActions.count(action)) continue;
        else if (action.rfind("__", 0) != 0) report.unboundActions.push_back(actionDisplayName(semantic, action));
    }

    for (const auto& [action, profile] : profiles) {
        if (!profile.triggers.empty() && !profile.hasVisibleEffect && !profile.writes.empty()) {
            report.unproductiveActions.push_back(profile.name);
        }
        report.actionProfiles.push_back(profile);
    }

    for (auto& profile : report.actionProfiles) {
        std::sort(profile.reads.begin(), profile.reads.end());
        std::sort(profile.writes.begin(), profile.writes.end());
        std::sort(profile.triggers.begin(), profile.triggers.end());
    }
    std::sort(report.actionProfiles.begin(), report.actionProfiles.end(),
              [](const auto& a, const auto& b) { return a.name < b.name; });

    report.observedState = sortedVector(observedState);
    report.observedDerived = sortedVector(observedDerived);
    std::sort(report.deadState.begin(), report.deadState.end());
    std::sort(report.zombieState.begin(), report.zombieState.end());
    std::sort(report.unusedDerived.begin(), report.unusedDerived.end());
    std::sort(report.boundActions.begin(), report.boundActions.end());
    std::sort(report.unboundActions.begin(), report.unboundActions.end());
    std::sort(report.unproductiveActions.begin(), report.unproductiveActions.end());

    for (const auto& state : report.deadState) {
        report.warnings.push_back({
            "JTML_DEAD_STATE",
            "\"" + state + "\" is never observed in UI output; remove it or bind it to a text, attribute, or condition"
        });
    }
    for (const auto& derived : report.unusedDerived) {
        report.warnings.push_back({
            "JTML_UNUSED_DERIVED",
            "derived \"" + derived + "\" is computed but never shown; bind it to an output or remove it"
        });
    }
    for (const auto& action : report.unboundActions) {
        report.warnings.push_back({
            "JTML_UNBOUND_ACTION",
            "action \"" + action + "\" is defined but never triggered from the UI; add a click, input, or other event"
        });
    }
    if (semantic.semanticPrimitiveRawStyleCount > 0) {
        report.warnings.push_back({
            "JTML_RAW_STYLE_ON_UI_PRIMITIVE",
            "semantic UI primitives should prefer theme tokens and modifiers; keep inline style as an escape hatch"
        });
    } else if (semantic.rawStyleAttributeCount >= 3 && !semantic.uiPrimitives.empty()) {
        report.warnings.push_back({
            "JTML_STYLE_MODIFIER_HINT",
            "this file mixes semantic UI primitives with several inline style attributes; consider gap, pad, tone, cols, width, surface, shadow, or a theme block"
        });
    }
    if (semantic.rawCssBlocks > 0) {
        report.warnings.push_back({
            "JTML_RAW_CSS_ESCAPE_HATCH",
            "raw CSS is emitted unscoped; keep it for trusted host widgets or third-party surfaces that semantic JTML cannot express"
        });
    }
    if (semantic.rawHtmlBlocks > 0) {
        report.warnings.push_back({
            "JTML_RAW_HTML_ESCAPE_HATCH",
            "raw HTML bypasses JTML escaping; use it only for trusted host widgets, custom elements, or reviewed platform markup"
        });
    }
    if (!semantic.externs.empty()) {
        report.warnings.push_back({
            "JTML_EXTERN_ESCAPE_HATCH",
            "extern actions call host-provided browser functions; review the target window path and keep the boundary explicit"
        });
    }
    for (const auto& use : semantic.uiUses) {
        const bool labeled = use.hasTitle || use.hasAriaLabel;
        if ((use.primitive == "panel" || use.primitive == "card") && !labeled) {
            report.warnings.push_back({
                "JTML_UI_SURFACE_UNLABELED",
                "semantic " + use.primitive + " should usually have title \"...\" or aria-label \"...\" so sections are understandable in large apps"
            });
        }
        if ((use.primitive == "modal" || use.primitive == "drawer" || use.primitive == "toast") && !labeled) {
            report.warnings.push_back({
                "JTML_UI_OVERLAY_UNLABELED",
                "semantic " + use.primitive + " needs title \"...\" or aria-label \"...\" so the overlay has an accessible name"
            });
        }
        if ((use.primitive == "modal" || use.primitive == "drawer") && !use.hasDismissAction) {
            report.warnings.push_back({
                "JTML_UI_OVERLAY_WITHOUT_DISMISS",
                "semantic " + use.primitive + " should expose a close, dismiss, cancel, or hide action so users can leave the overlay"
            });
        }
        if (use.primitive == "field" && !use.hasControl) {
            report.warnings.push_back({
                "JTML_UI_FIELD_WITHOUT_CONTROL",
                "semantic field should wrap an input, checkbox, file, dropzone, select, or textarea control"
            });
        }
        if (use.primitive == "field" && use.hasControl && !use.hasLabel) {
            report.warnings.push_back({
                "JTML_UI_FIELD_UNLABELED",
                "semantic field should include visible text, aria-label, title, placeholder, or another clear label for its control"
            });
        }
        if (use.primitive == "tabs" && !use.hasTabChild) {
            report.warnings.push_back({
                "JTML_UI_TABS_EMPTY",
                "semantic tabs should contain at least one tab child so navigation structure is explicit"
            });
        }
        if (use.primitive == "tab" && !use.hasActionBinding && !use.hasNavigationTarget) {
            report.warnings.push_back({
                "JTML_UI_TAB_WITHOUT_ACTION",
                "semantic tab should trigger an action or route target; otherwise it behaves like a decorative button"
            });
        }
    }
    for (const auto& modifier : semantic.uiModifiers) {
        if (modifier.modifier == "cols" && modifier.primitive != "grid") {
            report.warnings.push_back({
                "JTML_UI_COLS_ON_NON_GRID",
                "the cols modifier only affects grid primitives; move cols " + modifier.value + " onto a grid or remove it"
            });
        }
        if (modifier.modifier == "tone" && isLayoutPrimitive(modifier.primitive)) {
            report.warnings.push_back({
                "JTML_UI_TONE_ON_LAYOUT",
                "tone is intended for content surfaces such as card, panel, metric, alert, badge, or toast; use theme colors or surface modifiers for layout primitive " + modifier.primitive
            });
        }
        if (const auto* spec = semanticUiModifierSpec(modifier.modifier)) {
            if (!spec->values.empty() &&
                std::find(spec->values.begin(), spec->values.end(), modifier.value) == spec->values.end()) {
                std::ostringstream allowed;
                for (size_t i = 0; i < spec->values.size(); ++i) {
                    if (i > 0) allowed << ", ";
                    allowed << spec->values[i];
                }
                report.warnings.push_back({
                    "JTML_UI_INVALID_MODIFIER_VALUE",
                    "modifier " + modifier.modifier + " on " + modifier.primitive +
                        " uses unsupported value \"" + modifier.value +
                        "\"; expected one of: " + allowed.str()
                });
            }
        }
    }

    return report;
}

} // namespace jtml
