#include "jtml/runtime/static_update_plan_emitter.h"

#include "jtml/expression_ir.h"
#include "jtml/runtime_plan_json.h"
#include "json.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iterator>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <vector>

namespace jtml {
namespace {

nlohmann::json moduleIdToJson(SemanticModuleId id) {
    return id == InvalidSemanticModuleId ? nlohmann::json(nullptr) : nlohmann::json(id);
}

void eraseJsonKeyRecursive(nlohmann::json& value, const std::string& key) {
    if (value.is_object()) {
        value.erase(key);
        for (auto& item : value.items()) {
            eraseJsonKeyRecursive(item.value(), key);
        }
    } else if (value.is_array()) {
        for (auto& item : value) {
            eraseJsonKeyRecursive(item, key);
        }
    }
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

std::string fnv1a64Hex(const std::string& value) {
    uint64_t hash = 14695981039346656037ull;
    for (unsigned char ch : value) {
        hash ^= static_cast<uint64_t>(ch);
        hash *= 1099511628211ull;
    }
    static constexpr char digits[] = "0123456789abcdef";
    std::string out(16, '0');
    for (int i = 15; i >= 0; --i) {
        out[static_cast<size_t>(i)] = digits[hash & 0x0f];
        hash >>= 4;
    }
    return out;
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
                  << node.sourceColumn << '#'
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
        << ':' << fnv1a64Hex(signature.str());
    return key.str();
}

ExpressionValueType inferStaticComponentExpressionType(
        const std::string& expression,
        const ExpressionTypeEnvironment& environment) {
    auto ir = parseExpressionIr(expression, environment);
    return ir ? ir->valueType : ExpressionValueType::Unknown;
}

ExpressionTypeEnvironment staticComponentTypeEnvironment(
        const RuntimePlanComponentDefinition& definition) {
    ExpressionTypeEnvironment environment;
    for (const auto& param : definition.params) {
        if (!param.empty()) environment.symbols[param] = ExpressionValueType::Unknown;
    }
    for (const auto& node : definition.bodyPlan) {
        if (node.name.empty()) continue;
        if (node.kind == "state" || node.kind == "derived") {
            environment.symbols[node.name] =
                inferStaticComponentExpressionType(node.expression, environment);
        }
    }
    return environment;
}

std::string trimStaticPlanString(const std::string& value) {
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

bool isStaticLoopNode(const RuntimePlanComponentBodyNode& node) {
    return node.head == "for" || node.name == "for";
}

std::pair<std::string, std::string> staticLoopItemAndCollection(
        const RuntimePlanComponentBodyNode& node) {
    if (!isStaticLoopNode(node)) return {"", ""};
    const auto raw = trimStaticPlanString(node.expression);
    const auto inPos = raw.find(" in ");
    if (inPos == std::string::npos) return {"", raw};
    return {
        trimStaticPlanString(raw.substr(0, inPos)),
        trimStaticPlanString(raw.substr(inPos + 4))
    };
}

ExpressionValueType staticLoopItemTypeForCollection(
        const std::string& collection,
        const ExpressionTypeEnvironment& environment) {
    if (collection.empty()) return ExpressionValueType::Unknown;
    if (auto ir = parseExpressionIr(collection, environment)) {
        if (ir->kind == ExpressionIrKind::Range) return ExpressionValueType::Number;
    }
    return ExpressionValueType::Unknown;
}

void addStaticLoopLocalToEnvironment(ExpressionTypeEnvironment& environment,
                                     const RuntimePlanComponentBodyNode& node) {
    const auto [item, collection] = staticLoopItemAndCollection(node);
    if (item.empty()) return;
    environment.symbols[item] = staticLoopItemTypeForCollection(collection, environment);
}

ExpressionTypeEnvironment staticEnvironmentForBodyNode(
        const RuntimePlanComponentDefinition& definition,
        size_t nodeIndex,
        const ExpressionTypeEnvironment& baseEnvironment) {
    auto environment = baseEnvironment;
    std::vector<size_t> chain;
    size_t cursor = nodeIndex;
    while (cursor < definition.bodyPlan.size()) {
        chain.push_back(cursor);
        const int parent = definition.bodyPlan[cursor].parentIndex;
        if (parent < 0 || static_cast<size_t>(parent) == cursor ||
            static_cast<size_t>(parent) >= definition.bodyPlan.size()) {
            break;
        }
        cursor = static_cast<size_t>(parent);
    }
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        addStaticLoopLocalToEnvironment(environment, definition.bodyPlan[*it]);
    }
    return environment;
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
    int depth = 0;
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
        if (ch == '{' || ch == '[' || ch == '(') ++depth;
        if (ch == '}' || ch == ']' || ch == ')') --depth;
        if (depth == 0 && std::isspace(static_cast<unsigned char>(ch))) {
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
        const ExpressionTypeEnvironment& environment = ExpressionTypeEnvironment{},
        const std::set<std::string>& stopWords = {}) {
    nlohmann::json attrs = nlohmann::json::array();
    nlohmann::json modifiers = nlohmann::json::array();
    std::vector<std::string> content;
    for (size_t i = start; i < words.size(); ++i) {
        const auto& token = words[i];
        if (stopWords.count(token)) break;
        if ((token == "keydown" || token == "key-down") && i + 1 < words.size()) {
            const auto action = words[++i];
            const bool valid = !action.empty() &&
                (std::isalpha(static_cast<unsigned char>(action[0])) || action[0] == '_') &&
                std::all_of(action.begin(), action.end(), [](unsigned char ch) { return std::isalnum(ch) || ch == '_'; });
            attrs.push_back({{"name", "data-jtml-direct-component-keydown"},
                {"exprPlan", valid ? nlohmann::json({{"kind", "literal"}, {"value", action}})
                                   : nlohmann::json({{"kind", "source"}})}});
            continue;
        }
        if (token == "into" && i + 1 < words.size()) {
            attrs.push_back({
                {"name", "data-jtml-direct-component-bind"},
                {"raw", words[i + 1]},
                {"exprPlan", nlohmann::json({{"kind", "literal"}, {"value", words[i + 1]}})}
            });
            ++i;
            continue;
        }
        if (isSemanticUiModifierName(token)) {
            const auto raw = i + 1 < words.size() ? words[i + 1] : std::string();
            if (!raw.empty() && !stopWords.count(raw) &&
                !isComponentAttributeName(raw) &&
                !isComponentBooleanAttribute(raw) &&
                !isSemanticUiModifierName(raw)) {
                modifiers.push_back({
                    {"name", token},
                    {"raw", raw},
                    {"exprPlan", compileRuntimeExpressionPlan(raw, environment)}
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
                    {"exprPlan", compileRuntimeExpressionPlan(raw, environment)}
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
        {"contentPlan", compileRuntimeExpressionPlan(contentSource, environment)},
        {"attrs", attrs},
        {"modifiers", modifiers},
    };
}

nlohmann::json compileStaticActionInvocation(
        const std::string& raw,
        const ExpressionTypeEnvironment& environment = ExpressionTypeEnvironment{}) {
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
                argPlans.push_back(compileRuntimeExpressionPlan(expr, environment));
            }
            current.clear();
            continue;
        }
        current += ch;
    }
    if (!current.empty()) {
        const auto expr = trimCopy(current);
        argExpressions.push_back(expr);
        argPlans.push_back(compileRuntimeExpressionPlan(expr, environment));
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
    const auto head = componentNodeHead(node);
    std::set<std::string> loopLocals;
    if (head == "for") {
        const auto words = splitStaticComponentWords(node.text);
        if (words.size() > 1) loopLocals.insert(words[1]);
    }
    if (head == "for" || head == "if") {
        std::vector<int> stack = node.childIndices;
        if (head == "if") {
            const auto nodeIndex = static_cast<size_t>(&node - definition.bodyPlan.data());
            for (size_t i = nodeIndex + 1; i < definition.bodyPlan.size(); ++i) {
                const auto& sibling = definition.bodyPlan[i];
                if (sibling.parentIndex != node.parentIndex) continue;
                if (componentNodeHead(sibling) == "else") {
                    stack.insert(stack.end(), sibling.childIndices.begin(), sibling.childIndices.end());
                }
                break;
            }
        }
        std::set<int> visited;
        while (!stack.empty()) {
            const int childIndex = stack.back();
            stack.pop_back();
            if (childIndex < 0 || !visited.insert(childIndex).second ||
                static_cast<size_t>(childIndex) >= definition.bodyPlan.size()) {
                continue;
            }
            const auto& child = definition.bodyPlan[static_cast<size_t>(childIndex)];
            for (const auto& read : child.reads) {
                if (!loopLocals.count(read)) reads.insert(read);
            }
            for (const auto grandchild : child.childIndices) stack.push_back(grandchild);
        }
    }
    // Only descendant row-local reads are filtered above. Component-derived
    // dependencies resolve in the outer scope even if the iterator shadows one.
    // A worklist reaches the finite transitive closure, including cycles,
    // without silently truncating valid dependency chains at a fixed depth.
    std::vector<std::string> pending(reads.begin(), reads.end());
    for (size_t i = 0; i < pending.size(); ++i) {
        const auto derived = derivedDeps.find(pending[i]);
        if (derived == derivedDeps.end()) continue;
        for (const auto& dep : derived->second) {
            if (reads.insert(dep).second) pending.push_back(dep);
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

const RuntimePlanComponentDefinition* findNestedComponentDefinition(
        const std::vector<RuntimePlanComponentDefinition>& definitions,
        const RuntimePlanComponentDefinition& owner,
        const RuntimePlanComponentBodyNode& node) {
    const auto head = componentNodeHead(node);
    if (head.empty()) return nullptr;
    const SemanticModuleId module =
        node.definitionModule != InvalidSemanticModuleId
            ? node.definitionModule
            : owner.moduleId;
    for (const auto& candidate : definitions) {
        if (candidate.name == head && candidate.moduleId == module) {
            return &candidate;
        }
    }
    for (const auto& candidate : definitions) {
        if (candidate.name == head) {
            return &candidate;
        }
    }
    return nullptr;
}

std::string nestedComponentPlanKey(
        const std::vector<RuntimePlanComponentDefinition>& definitions,
        const RuntimePlanComponentDefinition& owner,
        const RuntimePlanComponentBodyNode& node) {
    const auto* nested = findNestedComponentDefinition(definitions, owner, node);
    return nested ? componentUpdatePlanKey(*nested) : std::string();
}

bool hasNestedComponentAncestor(
        const std::set<DefinitionKey>& definitionKeys,
        const RuntimePlanComponentDefinition& definition,
        size_t nodeIndex) {
    if (nodeIndex >= definition.bodyPlan.size()) return false;
    int parent = definition.bodyPlan[nodeIndex].parentIndex;
    int guard = 0;
    while (parent >= 0 &&
           static_cast<size_t>(parent) < definition.bodyPlan.size() &&
           guard++ < 1000) {
        const auto& ancestor = definition.bodyPlan[static_cast<size_t>(parent)];
        if (isKnownNestedComponent(definitionKeys, definition, ancestor)) {
            return true;
        }
        parent = ancestor.parentIndex;
    }
    return false;
}

bool isRenderTreeNode(
        const RuntimePlanComponentDefinition& definition,
        size_t nodeIndex) {
    if (nodeIndex >= definition.bodyPlan.size()) return false;
    const auto& node = definition.bodyPlan[nodeIndex];
    if (node.renderRoot) return true;
    int parent = node.parentIndex;
    int guard = 0;
    while (parent >= 0 &&
           static_cast<size_t>(parent) < definition.bodyPlan.size() &&
           guard++ < 1000) {
        const auto& ancestor = definition.bodyPlan[static_cast<size_t>(parent)];
        if (ancestor.kind == "action" || ancestor.kind == "state" ||
            ancestor.kind == "derived" || ancestor.kind == "assignment" ||
            ancestor.kind == "call") {
            return false;
        }
        if (ancestor.renderRoot) return true;
        parent = ancestor.parentIndex;
    }
    return false;
}

std::string staticPatchOperationKind(
        const std::set<DefinitionKey>& definitionKeys,
        const RuntimePlanComponentDefinition& definition,
        const RuntimePlanComponentBodyNode& node) {
    if (node.kind != "template" && node.kind != "slot") return "";
    const auto head = componentNodeHead(node);
    if (node.kind == "slot" || head == "slot") return "region";
    if (head == "if" || head == "for" || head == "slot") return "region";
    if (head == "else" || head == "while") return "";
    if (isKnownNestedComponent(definitionKeys, definition, node)) {
        return "nested-component";
    }
    if (head == "show" || head == "text") return "text";
    if (head == "button") return "button";
    return node.childIndices.empty() ? "element" : "container-attrs";
}

nlohmann::json originalLocation(const RuntimePlanComponentBodyNode& node) {
    if (!node.sourceSpan.original) return nullptr;
    const auto& range = *node.sourceSpan.original;
    const auto start = range.source->coordinates(range.begin);
    const auto end = range.source->coordinates(range.end);
    return {{"file", range.source->file}, {"line", start.first}, {"column", start.second},
            {"endLine", end.first}, {"endColumn", end.second}, {"exact", node.sourceSpan.originalExact}};
}

nlohmann::json staticPatchOperation(
        const std::string& kind,
        const RuntimePlanComponentBodyNode& node,
        size_t index,
        const ExpressionTypeEnvironment& environment,
        const std::string& childComponentKey = "") {
    const auto words = splitStaticComponentWords(node.text);
    nlohmann::json out = {
        {"operation", kind},
        {"nodeIndex", index},
        {"head", componentNodeHead(node)},
        {"tag", componentTagName(componentNodeHead(node))},
        {"words", words},
        {"sourceLine", node.sourceLine},
        {"sourceColumn", node.sourceColumn},
        {"originalLocation", originalLocation(node)},
        {"expression", node.expression},
        {"expressionPlan", compileRuntimeExpressionPlan(node.expression, environment)},
        {"keyExpression", node.keyExpression},
        {"keyExpressionPlan", compileRuntimeExpressionPlan(node.keyExpression, environment)},
    };
    if (kind == "button") {
        out["partsPlan"] = compileStaticElementParts(words, 1, environment, {"click"});
        auto it = std::find(words.begin(), words.end(), "click");
        out["clickInvocation"] = it != words.end() && std::next(it) != words.end()
            ? compileStaticActionInvocation(*std::next(it), environment)
            : nlohmann::json({{"name", ""}, {"argExpressions", nlohmann::json::array()}});
    } else if (kind == "element" || kind == "container-attrs") {
        const auto head = componentNodeHead(node);
        out["partsPlan"] = compileStaticElementParts(
            words, 1, environment, head == "form" ? std::set<std::string>{"submit"}
                                                   : std::set<std::string>{});
        if (head == "form") {
            const auto submit = std::find(words.begin(), words.end(), "submit");
            out["submitInvocation"] = submit != words.end() && std::next(submit) != words.end()
                ? compileStaticActionInvocation(*std::next(submit), environment)
                : nlohmann::json({{"name", ""}, {"argExpressions", nlohmann::json::array()}});
        }
    } else if (kind == "nested-component" && !childComponentKey.empty()) {
        out["childComponentKey"] = childComponentKey;
    }
    return out;
}

nlohmann::json compileStaticComponentUpdatePlan(
        const RuntimePlanComponentDefinition& definition,
        const std::vector<RuntimePlanComponentDefinition>& definitions,
        const std::set<DefinitionKey>& definitionKeys,
        const std::string& origin) {
    nlohmann::json entries = nlohmann::json::array();
    nlohmann::json entriesByRead = nlohmann::json::object();
    nlohmann::json unsafeEntries = nlohmann::json::array();
    nlohmann::json rootCreateOperations = nlohmann::json::array();
    nlohmann::json unsafeRootCreateEntries = nlohmann::json::array();
    const auto environment = staticComponentTypeEnvironment(definition);

    for (size_t i = 0; i < definition.bodyPlan.size(); ++i) {
        const auto& node = definition.bodyPlan[i];
        const auto nodeEnvironment = staticEnvironmentForBodyNode(definition, i, environment);
        if (node.kind != "template" || componentNodeHead(node) == "else") continue;
        if (!isRenderTreeNode(definition, i)) continue;
        if (hasNestedComponentAncestor(definitionKeys, definition, i)) continue;
        if (node.renderRoot) {
            const auto rootOperationKind = staticPatchOperationKind(
                definitionKeys, definition, node);
            if (rootOperationKind.empty()) {
                unsafeRootCreateEntries.push_back({
                    {"index", i},
                    {"kind", componentNodeHead(node)},
                    {"sourceLine", node.sourceLine},
                    {"sourceColumn", node.sourceColumn},
        {"originalLocation", originalLocation(node)},
                });
            } else {
                const auto childKey = rootOperationKind == "nested-component"
                    ? nestedComponentPlanKey(definitions, definition, node)
                    : std::string();
                rootCreateOperations.push_back(
                    staticPatchOperation(
                        rootOperationKind, node, i, nodeEnvironment, childKey));
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
                {"sourceColumn", node.sourceColumn},
        {"originalLocation", originalLocation(node)},
            });
            continue;
        }

        const auto childKey = operationKind == "nested-component"
            ? nestedComponentPlanKey(definitions, definition, node)
            : std::string();
        nlohmann::json entry = {
            {"index", i},
            {"reads", reads},
            {"kind", componentNodeHead(node)},
            {"sourceLine", node.sourceLine},
            {"sourceColumn", node.sourceColumn},
        {"originalLocation", originalLocation(node)},
            {"operation", staticPatchOperation(
                operationKind, node, i, nodeEnvironment, childKey)},
        };
        entries.push_back(entry);
        for (const auto& read : reads) {
            entriesByRead[read].push_back(entry);
        }
    }

    auto bodyPlanJson = runtimePlanBodyPlanToJson(
        definition.bodyPlan,
        RuntimePlanJsonOptions{true, &environment});
    eraseJsonKeyRecursive(bodyPlanJson, "sourceSpan");
    eraseJsonKeyRecursive(bodyPlanJson, "astKind");
    if (bodyPlanJson.is_array()) {
        for (size_t i = 0; i < definition.bodyPlan.size() && i < bodyPlanJson.size(); ++i) {
            bodyPlanJson[i]["originalLocation"] = originalLocation(definition.bodyPlan[i]);
            const auto childKey = nestedComponentPlanKey(definitions, definition, definition.bodyPlan[i]);
            if (!childKey.empty()) bodyPlanJson[i]["childComponentKey"] = childKey;
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
        {"sourceLine", definition.sourceLine},
        {"bodyPlan", bodyPlanJson},
        {"entries", entries},
        {"entriesByRead", entriesByRead},
        {"unsafeEntries", unsafeEntries},
        {"rootCreateOperations", rootCreateOperations},
        {"unsafeRootCreateEntries", unsafeRootCreateEntries},
        {"staticPatchCoverage", "text-region-slot-nested-element-first-slice"},
        {"staticCreateCoverage", "direct-text-button-element-container-control-flow-slot-nested-create-first-slice"},
    };
}

void appendComponentPlans(
        nlohmann::json& components,
        const std::vector<RuntimePlanComponentDefinition>& definitions,
        const std::set<DefinitionKey>& definitionKeys,
        const std::string& origin) {
    for (const auto& definition : definitions) {
        components.push_back(
            compileStaticComponentUpdatePlan(definition, definitions, definitionKeys, origin));
    }
}

nlohmann::json collectComponentPlans(const RuntimeProjectPlan& plan) {
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
    return components;
}

nlohmann::json assignComponentIdsAndChildIds(nlohmann::json components) {
    if (!components.is_array()) return components;

    std::map<std::string, size_t> keyToId;
    for (size_t i = 0; i < components.size(); ++i) {
        if (!components[i].is_object()) continue;
        components[i]["componentId"] = i;
        if (components[i].contains("key") && components[i]["key"].is_string()) {
            keyToId[components[i]["key"].get<std::string>()] = i;
        }
    }

    std::function<void(nlohmann::json&)> annotate = [&](nlohmann::json& value) {
        if (value.is_object()) {
            if (value.contains("childComponentKey") && value["childComponentKey"].is_string()) {
                const auto key = value["childComponentKey"].get<std::string>();
                const auto it = keyToId.find(key);
                if (it != keyToId.end()) value["childComponentId"] = it->second;
            }
            for (auto it = value.begin(); it != value.end(); ++it) {
                annotate(it.value());
            }
            return;
        }
        if (value.is_array()) {
            for (auto& item : value) annotate(item);
        }
    };

    for (auto& component : components) annotate(component);
    return components;
}

nlohmann::json componentModulePlanPayload(nlohmann::json component) {
    std::function<void(nlohmann::json&)> stripDebugFields =
        [&](nlohmann::json& value) {
            if (value.is_object()) {
                value.erase("sourceLine");
                value.erase("sourceColumn");
                value.erase("rootCreateOperations");
                value.erase("unsafeRootCreateEntries");
                if (value.contains("childComponentId")) {
                    value.erase("childComponentKey");
                }
                for (auto it = value.begin(); it != value.end(); ++it) {
                    stripDebugFields(it.value());
                }
                return;
            }
            if (value.is_array()) {
                for (auto& item : value) stripDebugFields(item);
            }
        };

    // The executable component module keeps only the metadata its generated
    // create/update functions and runtime lookup need. The source-rich body
    // plan stays in the legacy update-plan/debug asset.
    component.erase("bodyPlan");
    component.erase("localActions");
    component.erase("localDerived");
    component.erase("localState");
    component.erase("params");
    component.erase("rootCreateOperations");
    component.erase("unsafeRootCreateEntries");
    component.erase("entries");
    component.erase("entriesByRead");
    component.erase("unsafeEntries");
    stripDebugFields(component);
    return component;
}

nlohmann::json componentModulePlansPayload(const nlohmann::json& components) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& component : components) {
        out.push_back(componentModulePlanPayload(component));
    }
    return out;
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
    if (plan.value("directJs", false) && plan.contains("jsExpression") &&
        plan["jsExpression"].is_string()) {
        return "(function(scope){ const value = (" +
               plan["jsExpression"].get<std::string>() +
               "); return value == null ? '' : value; })";
    }
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
    if (kind == "member") {
        const auto base = plan.value("base", nlohmann::json::object());
        if (!isDirectJsExpressionPlan(base)) return plan.dump();
        return "(function(scope){ const base = " + jsExpressionPlanArgument(base) +
               "(scope); if (base == null) return ''; const value = base[" +
               jsStringLiteral(plan.value("property", "")) +
               "]; return value == null ? '' : value; })";
    }
    if (kind == "subscript") {
        const auto base = plan.value("base", nlohmann::json::object());
        const auto index = plan.value("index", nlohmann::json::object());
        if (!isDirectJsExpressionPlan(base) || !isDirectJsExpressionPlan(index)) {
            return plan.dump();
        }
        return "(function(scope){ const base = " + jsExpressionPlanArgument(base) +
               "(scope); const index = " + jsExpressionPlanArgument(index) +
               "(scope); if (base == null) return ''; const value = base[index]; "
               "return value == null ? '' : value; })";
    }
    if (kind == "array") {
        const auto elements = plan.value("elements", nlohmann::json::array());
        for (const auto& element : elements) {
            if (!isDirectJsExpressionPlan(element)) return plan.dump();
        }
        std::ostringstream out;
        out << "(function(scope){ return [";
        for (size_t i = 0; i < elements.size(); ++i) {
            if (i > 0) out << ",";
            out << jsExpressionPlanArgument(elements[i]) << "(scope)";
        }
        out << "]; })";
        return out.str();
    }
    if (kind == "object") {
        const auto entries = plan.value("entries", nlohmann::json::array());
        for (const auto& entry : entries) {
            if (!entry.is_object() ||
                !isDirectJsExpressionPlan(entry.value("value", nlohmann::json::object()))) {
                return plan.dump();
            }
        }
        std::ostringstream out;
        out << "(function(scope){ return {";
        for (size_t i = 0; i < entries.size(); ++i) {
            if (i > 0) out << ",";
            out << jsStringLiteral(entries[i].value("key", "")) << ":"
                << jsExpressionPlanArgument(entries[i].value("value", nlohmann::json::object()))
                << "(scope)";
        }
        out << "}; })";
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
    if (plan.value("directJs", false) && plan.contains("jsExpression") &&
        plan["jsExpression"].is_string()) {
        return true;
    }
    const auto kind = plan.value("kind", "");
    if (kind == "empty" || kind == "literal" || kind == "path") return true;
    if (kind == "member") {
        return !plan.value("property", "").empty() &&
               isDirectJsExpressionPlan(plan.value("base", nlohmann::json::object()));
    }
    if (kind == "subscript") {
        return isDirectJsExpressionPlan(plan.value("base", nlohmann::json::object())) &&
               isDirectJsExpressionPlan(plan.value("index", nlohmann::json::object()));
    }
    if (kind == "array") {
        for (const auto& element : plan.value("elements", nlohmann::json::array())) {
            if (!isDirectJsExpressionPlan(element)) return false;
        }
        return true;
    }
    if (kind == "object") {
        for (const auto& entry : plan.value("entries", nlohmann::json::array())) {
            if (!entry.is_object() ||
                !isDirectJsExpressionPlan(entry.value("value", nlohmann::json::object()))) {
                return false;
            }
        }
        return true;
    }
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
        << "if (value == null || (value === '' && name !== 'value')) return; "
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
        out << "addAttr(" << jsStringLiteral(name) << ", String("
            << jsDirectExpressionCall(attr.value("exprPlan", nlohmann::json::object()))
            << ")); ";
    }

    if (head == "link" || head == "navlink") {
        out << "if (Object.prototype.hasOwnProperty.call(next, 'to')) { "
            << "const target = String(next.to || '/'); delete next.to; "
            << "addAttr('href', 'javascript:void(0)'); "
            << "addAttr('data-jtml-href', target[0] === '#' ? target : '#' + (target[0] === '/' ? target : '/' + target)); "
            << "addAttr('data-jtml-link', 'true'); } "
            << "if (Object.prototype.hasOwnProperty.call(next, 'active-class')) { "
            << "const activeClass = next['active-class']; delete next['active-class']; "
            << "addAttr('data-jtml-active-class', activeClass); } ";
    }

    if (head == "button") {
        out << "if (!Object.prototype.hasOwnProperty.call(next, 'type')) addAttr('type', 'button'); ";
    } else if (head == "checkbox") {
        out << "if (!Object.prototype.hasOwnProperty.call(next, 'type')) addAttr('type', 'checkbox'); ";
    } else if (head == "file" || head == "dropzone") {
        out << "if (!Object.prototype.hasOwnProperty.call(next, 'type')) addAttr('type', 'file'); ";
    }
    if (!head.empty()) {
        out << "addAttr('data-jtml-direct-node'," << jsStringLiteral(head) << "); ";
    }
    out << "if (classValue) next.class = classValue; ";
    // Binding ownership is managed on both creation and retained patches.
    for (const auto& attr : partsPlan.value("attrs", nlohmann::json::array())) {
        if (attr.value("name", "") == "data-jtml-direct-component-bind" ||
            attr.value("name", "") == "data-jtml-direct-component-keydown") {
            out << "addAttr('data-jtml-direct-component-id', instance && instance.id || ''); ";
            break;
        }
    }
}

void appendDirectAttributePatchStatements(std::ostringstream& out,
                                          const nlohmann::json& partsPlan,
                                          const std::string& head) {
    appendDirectAttributeMapStatements(out, partsPlan, head);
    out
        << "const previous = String(el.getAttribute('data-jtml-direct-managed-attrs') || '')"
        << ".split(',').filter(Boolean); "
        << "previous.forEach(function(name){ if (name !== 'data-jtml-direct-body-node' && !Object.prototype.hasOwnProperty.call(next, name)) el.removeAttribute(name); }); "
        << "Object.keys(next).forEach(function(name){ "
        << "if (next[name] === true) el.setAttribute(name, name); "
        << "else el.setAttribute(name, next[name]); ";
    for (const auto& attr : partsPlan.value("attrs", nlohmann::json::array())) {
        if (attr.value("name", "") == "value") {
            // Dirty controls need property updates; unchanged values retain selection.
            out << "if (name === 'value' && (el.tagName === 'INPUT' || el.tagName === 'TEXTAREA' || el.tagName === 'SELECT') "
                << "&& el.value !== String(next[name])) el.value = next[name]; ";
            break;
        }
    }
    out << "}); "
        << "el.setAttribute('data-jtml-direct-managed-attrs', Object.keys(next).join(',')); ";
}

void appendDirectPatchElementLookup(std::ostringstream& out,
                                    size_t nodeIndex,
                                    const std::string& rootExpression) {
    out << "const el = ";
    if (!rootExpression.empty()) {
        out << rootExpression << " && " << rootExpression << ".querySelector ? "
            << rootExpression << ".querySelector('[data-jtml-direct-body-node=\""
            << nodeIndex << "\"]') : null; ";
        return;
    }
    out << "h && h.directComponentElementForNode ? "
        << "h.directComponentElementForNode(instance, " << nodeIndex << ") : "
        << "(instance && instance.element ? instance.element.querySelector('[data-jtml-direct-body-node=\""
        << nodeIndex << "\"]') : null); ";
}

std::string directStaticTextPatchExpression(size_t nodeIndex,
                                            const nlohmann::json& plan,
                                            const std::string& rootExpression = "") {
    if (!isDirectJsExpressionPlan(plan)) return "";
    std::ostringstream out;
    out << "(function(){ ";
    appendDirectPatchElementLookup(out, nodeIndex, rootExpression);
    out << "if (!el) return false; "
        << "const value = " << jsExpressionPlanArgument(plan) << "(scope); "
        << "el.textContent = String(value == null ? '' : value); "
        << "return true; "
        << "})()";
    return out.str();
}

std::string directStaticButtonPatchExpression(size_t nodeIndex,
                                              const std::string& head,
                                              const nlohmann::json& partsPlan,
                                              const nlohmann::json& clickInvocation,
                                              const std::string& rootExpression = "") {
    if (!staticElementPartsAreDirect(partsPlan, true) ||
        !staticActionInvocationIsDirect(clickInvocation)) {
        return "";
    }
    std::ostringstream out;
    out << "(function(){ ";
    appendDirectPatchElementLookup(out, nodeIndex, rootExpression);
    out << "if (!el || !el.tagName || el.tagName.toLowerCase() !== 'button') return false; "
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
                                               const nlohmann::json& expressionPlan,
                                               const std::string& rootExpression = "") {
    const auto contentPlan = partsPlan.value("contentPlan", nlohmann::json::object());
    const bool usesPartsContent = !partsPlan.value("content", "").empty();
    // An explicitly empty content plan means an attribute-only element. Its
    // raw expression contains attributes, not an alternate content expression.
    const auto& planForContent = partsPlan.contains("contentPlan") ? contentPlan : expressionPlan;
    if (!staticElementPartsAreDirect(partsPlan, patchContent && usesPartsContent) ||
        (patchContent && !isDirectJsExpressionPlan(planForContent))) {
        return "";
    }
    const auto resolvedHead = head.empty() ? std::string("box") : head;
    const auto resolvedTag = tag.empty() ? componentTagName(resolvedHead) : tag;
    std::ostringstream out;
    out << "(function(){ ";
    appendDirectPatchElementLookup(out, nodeIndex, rootExpression);
    out << "if (!el || !el.tagName || el.tagName.toLowerCase() !== "
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

void appendStaticEscapingHelpers(std::ostringstream& out) {
    out << "function jtml_static_escape_html(value) { "
        << "return String(value == null ? '' : value)"
        << ".replace(/&/g, '&amp;')"
        << ".replace(/</g, '&lt;')"
        << ".replace(/>/g, '&gt;')"
        << ".replace(/\\\"/g, '&quot;')"
        << ".replace(/'/g, '&#39;'); "
        << "} "
        << "function jtml_static_attrs_to_string(attrs) { "
        << "return Object.keys(attrs || {}).map(function(name) { "
        << "if (attrs[name] === true) return ' ' + name; "
        << "return ' ' + name + '=\\\"' + jtml_static_escape_html(attrs[name]) + '\\\"'; "
        << "}).join(''); "
        << "} ";
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
    // An explicitly empty content plan means an attribute-only element. Its
    // raw expression contains attributes, not an alternate content expression.
    const auto& planForContent = partsPlan.contains("contentPlan") ? contentPlan : expressionPlan;
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
        << "const attrs = jtml_static_attrs_to_string(next); ";
    if (resolvedTag == "input" || resolvedTag == "img") {
        out << "return '<" << resolvedTag << "' + attrs + '>'; ";
    } else {
        out << "return '<" << resolvedTag << "' + attrs + '>' + "
            << "jtml_static_escape_html(contentValue) + '</" << resolvedTag << ">'; ";
    }
    out
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

std::vector<std::string> directStaticChildCreateExpressions(
        const nlohmann::json& componentPlan,
        const nlohmann::json& node,
        std::set<size_t>& visiting) {
    const auto children = node.value("childIndices", nlohmann::json::array());
    if (!children.is_array()) return {};

    std::vector<std::string> childExpressions;
    for (const auto& childIndexValue : children) {
        if (!childIndexValue.is_number_integer()) return {};
        const auto childIndex = static_cast<size_t>(childIndexValue.get<int>());
        auto child = directStaticNodeCreateExpression(componentPlan, childIndex, visiting);
        if (child.empty()) return {};
        childExpressions.push_back(std::move(child));
    }
    return childExpressions;
}

void appendDirectRenderedChildrenStatements(
        std::ostringstream& out,
        const std::vector<std::string>& childExpressions,
        const std::string& outputArray) {
    for (size_t i = 0; i < childExpressions.size(); ++i) {
        out << " const child_" << i << " = " << childExpressions[i] << "; "
            << "if (typeof child_" << i << " !== 'string') return null; "
            << outputArray << ".push(child_" << i << ");";
    }
}

bool findStaticElseSibling(const nlohmann::json& componentPlan,
                           size_t nodeIndex,
                           const nlohmann::json& node,
                           nlohmann::json& elseNode) {
    const auto bodyPlan = componentPlan.value("bodyPlan", nlohmann::json::array());
    if (!bodyPlan.is_array()) return false;
    const int parentIndex = node.value("parentIndex", -1);
    for (size_t i = nodeIndex + 1; i < bodyPlan.size(); ++i) {
        if (!bodyPlan[i].is_object()) continue;
        if (bodyPlan[i].value("parentIndex", -2) != parentIndex) continue;
        const auto head = bodyPlan[i].value("head", bodyPlan[i].value("name", ""));
        if (head != "else") return false;
        elseNode = bodyPlan[i];
        return true;
    }
    return false;
}

std::string directStaticIfCreateExpression(const nlohmann::json& componentPlan,
                                           size_t nodeIndex,
                                           const nlohmann::json& node,
                                           std::set<size_t>& visiting) {
    const auto conditionPlan = node.value("expressionPlan", nlohmann::json::object());
    if (!isDirectJsExpressionPlan(conditionPlan)) return "";

    auto thenChildren = directStaticChildCreateExpressions(componentPlan, node, visiting);
    const auto thenChildCount = node.value("childIndices", nlohmann::json::array()).size();
    if (thenChildren.size() != thenChildCount) return "";

    std::vector<std::string> elseChildren;
    nlohmann::json elseNode;
    if (findStaticElseSibling(componentPlan, nodeIndex, node, elseNode)) {
        auto elseVisiting = visiting;
        elseVisiting.insert(nodeIndex);
        elseChildren = directStaticChildCreateExpressions(componentPlan, elseNode, elseVisiting);
        const auto elseChildCount = elseNode.value("childIndices", nlohmann::json::array()).size();
        if (elseChildren.size() != elseChildCount) return "";
    }

    std::ostringstream out;
    out << "(function(){ "
        << "const rendered = []; const branch = !!(" << jsDirectExpressionCall(conditionPlan) << "); "
        << "if (branch) {";
    appendDirectRenderedChildrenStatements(out, thenChildren, "rendered");
    out << "} else {";
    appendDirectRenderedChildrenStatements(out, elseChildren, "rendered");
    out << "} "
        << "return '<span data-jtml-direct-body-node=\""
        << nodeIndex
        << "\" data-jtml-direct-region=\"if\" data-jtml-direct-branch=\"' + (branch ? '1' : '0') + '\" style=\"display:contents\">' + "
        << "rendered.join('') + '</span>'; "
        << "})()";
    return out.str();
}

std::string directStaticForCreateExpression(const nlohmann::json& componentPlan,
                                            size_t nodeIndex,
                                            const nlohmann::json& node,
                                            std::set<size_t>& visiting) {
    const auto loopPlan = node.value("loopPlan", nlohmann::json::object());
    if (!loopPlan.is_object()) return "";
    const auto itemName = loopPlan.value("item", "");
    if (itemName.empty()) return "";

    const auto collectionPlan = loopPlan.value("collectionPlan", nlohmann::json::object());
    if (!isDirectJsExpressionPlan(collectionPlan)) return "";
    const auto keyExpression = node.value("keyExpression", "");
    const auto keyPlan = node.value("keyExpressionPlan", nlohmann::json::object());
    if (!keyExpression.empty() && !isDirectJsExpressionPlan(keyPlan)) return "";

    auto childExpressions = directStaticChildCreateExpressions(componentPlan, node, visiting);
    const auto childCount = node.value("childIndices", nlohmann::json::array()).size();
    if (childExpressions.size() != childCount) return "";

    std::ostringstream out;
    out << "(function(){ "
        << "let values = " << jsDirectExpressionCall(collectionPlan) << "; "
        << "if (values == null) values = []; "
        << "else if (!Array.isArray(values)) { "
        << "if (typeof values === 'string') values = values.split(''); "
        << "else if (typeof values === 'object') values = Object.values(values); "
        << "else values = [values]; "
        << "} "
        << "const rendered = []; "
        << "for (let itemIndex = 0; itemIndex < values.length; itemIndex += 1) { "
        << "const childScope = Object.assign({}, scope); "
        << "childScope[" << jsStringLiteral(itemName) << "] = values[itemIndex]; "
        << "const keySegment = h.componentPathSegment(";
    if (keyExpression.empty()) {
        out << "itemIndex";
    } else {
        out << "(function(scope){ return " << jsDirectExpressionCall(keyPlan)
            << "; })(childScope)";
    }
    // Match reconciliation's scoped loop identity while creating nested children.
    out << "); "
        << "const innerHtml = h.withComponentRenderPath('for" << nodeIndex
        << "_' + keySegment, function(){ return (function(scope){ const children = [];";
    appendDirectRenderedChildrenStatements(out, childExpressions, "children");
    out << " return children.join(''); })(childScope); }); "
        << "if (typeof innerHtml !== 'string') return null; "
        << "rendered.push('<span data-jtml-direct-list-item=\""
        << nodeIndex
        << "\" data-jtml-direct-list-key=\"' + jtml_static_escape_html(keySegment) + "
        << "'\" data-jtml-direct-list-index=\"' + String(itemIndex) + "
        << "'\" style=\"display:contents\">' + innerHtml + '</span>'); "
        << "} "
        << "return '<span data-jtml-direct-body-node=\""
        << nodeIndex
        << "\" data-jtml-direct-region=\"for\" style=\"display:contents\">' + "
        << "rendered.join('') + '</span>'; "
        << "})()";
    return out.str();
}

std::string directStaticSlotCreateExpression(size_t nodeIndex,
                                             const nlohmann::json& node) {
    const auto words = splitStaticComponentWords(node.value("text", ""));
    const auto slotName = words.size() > 1 ? words[1] : std::string();
    std::ostringstream out;
    out << "(function(){ "
        << "const slotName = " << jsStringLiteral(slotName) << "; "
        << "let html = ''; "
        << "if (!slotName && instance && instance.slotHtml) html = String(instance.slotHtml); "
        << "else if (slotName && instance && instance.slotHtmlByName && "
        << "Object.prototype.hasOwnProperty.call(instance.slotHtmlByName, slotName)) "
        << "html = String(instance.slotHtmlByName[slotName]); "
        << "return '<span data-jtml-direct-body-node=\""
        << nodeIndex
        << "\" data-jtml-direct-region=\"slot\" style=\"display:contents\">' + html + '</span>'; "
        << "})()";
    return out.str();
}

std::string directStaticNestedCreateExpression(size_t nodeIndex,
                                               const std::string& name,
                                               const std::string& childComponentKey = "",
                                               int childComponentId = -1) {
    if (name.empty()) return "";
    const auto emittedChildKey =
        childComponentId >= 0 ? std::string() : childComponentKey;
    std::ostringstream out;
    out << "(function(){ "
        << "const node = (definition && definition.bodyPlan || [])["
        << nodeIndex << "]; "
        << "const staticModule = ((typeof modulesById !== 'undefined' && "
        << childComponentId << " >= 0 && modulesById[" << childComponentId << "]) || ";
    if (emittedChildKey.empty()) {
        out << "null";
    } else {
        out << "(typeof modules !== 'undefined' ? modules["
            << jsStringLiteral(emittedChildKey) << "] : null)";
    }
    out
        << "); "
        << "if (!node || !staticModule || typeof staticModule.create !== 'function') return null; "
        << "if (!h || typeof h.createNestedComponentInstance !== 'function' || "
        << "typeof h.componentScopeFor !== 'function') return null; "
        << "const nested = h.createNestedComponentInstance(definition, instance, node, scope, "
        << jsStringLiteral(name) << "); "
        << "if (!nested || !nested.definition || !nested.instance) return null; "
        << "const nestedDefinition = nested.definition; "
        << "const nestedInstance = nested.instance; "
        << "const nestedScope = h.componentScopeFor(nestedInstance, nestedDefinition); "
        << "if (!nestedScope) return null; "
        << "if (typeof h.applyComponentDerived === 'function') "
        << "h.applyComponentDerived(nestedScope, nestedDefinition); "
        << "const helper = Object.assign({}, h || {}); "
        << "const html = staticModule.create(nestedInstance, nestedDefinition, nestedScope, helper); "
        << "if (typeof html !== 'string') return null; "
        << "const esc = jtml_static_escape_html; "
        << "return '<div data-jtml-direct-body-node=\""
        << nodeIndex
        << "\" data-jtml-direct-instance=\"' + esc(nestedInstance.id || '') + "
        << "'\" data-jtml-component=\"' + esc("
        << jsStringLiteral(name)
        << ") + '\" data-jtml-component-parent=\"' + esc(nestedInstance.parentId || '') + "
        << "'\" data-jtml-nested-component=\"true\">' + html + '</div>'; "
        << "})()";
    return out.str();
}

std::string directStaticContainerCreateExpression(const nlohmann::json& componentPlan,
                                                  size_t nodeIndex,
                                                  const nlohmann::json& node,
                                                  const std::string& head,
                                                  const std::string& tag,
                                                  const nlohmann::json& partsPlan) {
    if (!staticElementPartsAreDirect(partsPlan, false)) return "";
    const auto words = splitStaticComponentWords(node.value("text", ""));
    auto submitInvocation = nlohmann::json({
        {"name", ""}, {"argExpressions", nlohmann::json::array()}});
    if (head == "form") {
        const auto submit = std::find(words.begin(), words.end(), "submit");
        if (submit != words.end() && std::next(submit) != words.end()) {
            submitInvocation = compileStaticActionInvocation(*std::next(submit));
        }
        if (!staticActionInvocationIsDirect(submitInvocation)) return "";
    }
    std::set<size_t> visiting;
    visiting.insert(nodeIndex);
    auto childExpressions =
        directStaticChildCreateExpressions(componentPlan, node, visiting);
    const auto children = node.value("childIndices", nlohmann::json::array());
    if (!children.is_array() || childExpressions.size() != children.size()) return "";

    const auto resolvedHead = head.empty() ? std::string("box") : head;
    const auto resolvedTag = tag.empty() ? componentTagName(resolvedHead) : tag;
    std::ostringstream out;
    out << "(function(){ ";
    appendDirectAttributeMapStatements(out, partsPlan, resolvedHead);
    out << "next['data-jtml-direct-body-node'] = "
        << jsStringLiteral(std::to_string(nodeIndex)) << "; "
        << "next['data-jtml-direct-managed-attrs'] = Object.keys(next).join(','); ";
    const auto actionName = submitInvocation.value("name", "");
    if (!actionName.empty()) {
        out << "const submitArgs = [";
        const auto argPlans = submitInvocation.value("argPlans", nlohmann::json::array());
        for (size_t i = 0; i < argPlans.size(); ++i) {
            if (i > 0) out << ",";
            out << jsDirectExpressionCall(argPlans[i]);
        }
        out << "]; "
            << "next['data-jtml-direct-component-id'] = instance && instance.id || ''; "
            << "next['data-jtml-direct-component-submit'] = "
            << jsStringLiteral(actionName) << "; "
            << "next['data-jtml-direct-component-args'] = JSON.stringify(submitArgs); ";
    }
    out << "const children = [];";
    appendDirectRenderedChildrenStatements(out, childExpressions, "children");
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
    const auto nodeKind = node.value("kind", "");
    if (nodeKind != "template" && nodeKind != "slot") return "";
    const auto head = node.value("head", node.value("name", ""));
    const auto resolvedHead = head.empty()
        ? splitStaticComponentWords(node.value("text", "")).empty()
            ? std::string("")
            : splitStaticComponentWords(node.value("text", "")).front()
        : head;
    if (resolvedHead.empty() || resolvedHead == "else" || resolvedHead == "while") {
        return "";
    }

    const auto words = splitStaticComponentWords(node.value("text", ""));
    const auto expressionPlan =
        compileRuntimeExpressionPlan(node.value("expression", ""));
    if (resolvedHead == "slot") {
        return directStaticSlotCreateExpression(nodeIndex, node);
    }
    if (looksLikeComponentCallHead(resolvedHead)) {
        return directStaticNestedCreateExpression(
            nodeIndex,
            resolvedHead,
            node.value("childComponentKey", ""),
            node.value("childComponentId", -1));
    }
    if (resolvedHead == "if") {
        return directStaticIfCreateExpression(componentPlan, nodeIndex, node, visiting);
    }
    if (resolvedHead == "for") {
        return directStaticForCreateExpression(componentPlan, nodeIndex, node, visiting);
    }
    if (resolvedHead == "show" || resolvedHead == "text") {
        return directStaticTextCreateExpression(nodeIndex, expressionPlan);
    }
    if (resolvedHead == "button") {
        auto partsPlan = compileStaticElementParts(
            words, 1, ExpressionTypeEnvironment{}, {"click"});
        auto it = std::find(words.begin(), words.end(), "click");
        auto clickInvocation = it != words.end() && std::next(it) != words.end()
            ? compileStaticActionInvocation(*std::next(it))
            : nlohmann::json({{"name", ""}, {"argExpressions", nlohmann::json::array()}});
        return directStaticButtonCreateExpression(
            nodeIndex, resolvedHead, partsPlan, clickInvocation);
    }

    auto partsPlan = compileStaticElementParts(
        words, 1, ExpressionTypeEnvironment{},
        resolvedHead == "form" ? std::set<std::string>{"submit"}
                               : std::set<std::string>{});
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
                << ", " << operation.value("submitInvocation", nlohmann::json::object()).dump()
                << ") : null);\n";
        }
    } else if (kind == "region") {
        std::set<size_t> visiting;
        const auto directCreate = directStaticNodeCreateExpression(
            componentPlan, static_cast<size_t>(nodeIndex), visiting);
        if (!directCreate.empty()) {
            out << directCreate << ";\n";
        } else {
            out << "(h && h.renderStaticComponentRegionNode ? h.renderStaticComponentRegionNode(instance, definition, scope, "
                << nodeIndex << ") : null);\n";
        }
    } else if (kind == "nested-component") {
        std::set<size_t> visiting;
        auto directCreate = directStaticNodeCreateExpression(
            componentPlan, static_cast<size_t>(nodeIndex), visiting);
        if (directCreate.empty()) {
            directCreate = directStaticNestedCreateExpression(
                static_cast<size_t>(nodeIndex),
                operation.value("head", ""),
                operation.value("childComponentKey", ""),
                operation.value("childComponentId", -1));
        }
        if (!directCreate.empty()) {
            out << directCreate << ";\n";
        } else {
            out << "(h && h.renderStaticComponentNestedNode ? h.renderStaticComponentNestedNode(instance, definition, scope, "
                << nodeIndex << ", "
                << jsStringLiteral(operation.value("head", ""))
                << ") : null);\n";
        }
    } else {
        return "";
    }
    out << "    if (typeof html_" << operationIndex
        << " !== 'string') return jtml_static_component_create_fallback_"
        << componentIndex << "();\n"
        << "    rendered.push(html_" << operationIndex << ");\n";
    return out.str();
}

std::string directStaticRegionPatchExpression(
        const nlohmann::json& componentPlan,
    size_t nodeIndex,
    bool enableGeneratedKeyedPatch,
    size_t componentIndex) {
    const auto bodyPlan = componentPlan.value("bodyPlan", nlohmann::json::array());
    const nlohmann::json* node = nullptr;
    if (bodyPlan.is_array() && nodeIndex < bodyPlan.size() &&
        bodyPlan[nodeIndex].is_object()) {
        node = &bodyPlan[nodeIndex];
    }
    const auto head = node ? node->value("head", node->value("name", "")) : std::string();
    std::set<size_t> visiting;
    const auto directCreate =
        directStaticNodeCreateExpression(componentPlan, nodeIndex, visiting);
    if (directCreate.empty()) return "";

    std::string retainedItemPatch;
    if (enableGeneratedKeyedPatch && head == "for" && bodyPlan.is_array()) {
        std::ostringstream patch;
        size_t emitted = 0;
        const auto entries = componentPlan.value("entries", nlohmann::json::array());
        for (const auto& entry : entries) {
            if (!entry.is_object() || !entry.contains("index") ||
                !entry.contains("operation")) {
                continue;
            }
            const int entryIndex = entry.value("index", -1);
            if (entryIndex < 0 || static_cast<size_t>(entryIndex) >= bodyPlan.size()) continue;
            int parentIndex = bodyPlan[entryIndex].value("parentIndex", -1);
            bool descendant = false;
            size_t guard = 0;
            while (parentIndex >= 0 && guard++ < bodyPlan.size()) {
                if (static_cast<size_t>(parentIndex) == nodeIndex) {
                    descendant = true;
                    break;
                }
                if (static_cast<size_t>(parentIndex) >= bodyPlan.size()) break;
                parentIndex = bodyPlan[parentIndex].value("parentIndex", -1);
            }
            if (!descendant) continue;

            const auto operation = entry["operation"];
            const auto kind = operation.value("operation", "");
            const auto childIndex = static_cast<size_t>(entryIndex);
            std::string expression;
            if (kind == "text") {
                expression = directStaticTextPatchExpression(
                    childIndex,
                    operation.value("expressionPlan", nlohmann::json::object()),
                    "wrapper");
            } else if (kind == "button") {
                expression = directStaticButtonPatchExpression(
                    childIndex,
                    operation.value("head", "button"),
                    operation.value("partsPlan", nlohmann::json::object()),
                    operation.value("clickInvocation", nlohmann::json::object()),
                    "wrapper");
            } else if (kind == "element" || kind == "container-attrs") {
                expression = directStaticElementPatchExpression(
                    childIndex,
                    operation.value("head", "box"),
                    operation.value("tag", "div"),
                    operation.value("partsPlan", nlohmann::json::object()),
                    kind == "element",
                    operation.value("expressionPlan", nlohmann::json::object()),
                    "wrapper");
            } else {
                retainedItemPatch.clear();
                emitted = 0;
                break;
            }
            if (expression.empty()) {
                retainedItemPatch.clear();
                emitted = 0;
                break;
            }
            patch << "if (!(" << expression << ")) return false; ";
            emitted += 1;
        }
        if (emitted > 0) {
            retainedItemPatch = "function(wrapper, scope){ " + patch.str() + "return true; }";
        }
    }

    std::ostringstream out;
    out << "(function(){ ";
    appendStaticEscapingHelpers(out);
    const auto replaceReason = head == "if"
        ? std::string("subtree-prune")
        : std::string("body-node-html-replace");
    out << "const el = h && h.directComponentElementForNode ? "
        << "h.directComponentElementForNode(instance, " << nodeIndex << ") : "
        << "(instance && instance.element ? instance.element.querySelector('[data-jtml-direct-body-node=\""
        << nodeIndex << "\"]') : null); "
        << "if (!el) return false; ";
    if (head == "if") {
        out << "const branch = !!(" << jsDirectExpressionCall(node->value("expressionPlan", nlohmann::json::object())) << "); "
            << "if (el.getAttribute('data-jtml-direct-branch') === (branch ? '1' : '0')) { ";
        std::map<size_t, std::string> operations;
        for (const auto& entry : componentPlan.value("entries", nlohmann::json::array())) {
            operations[entry.value("index", 0)] = entry.value("operation", nlohmann::json::object()).value("operation", "");
        }
        // Reuse generated patch cases for the active branch. Containers patch
        // attributes only; regions and nested modules own their descendants.
        std::function<void(const nlohmann::json&)> patchChildren = [&](const nlohmann::json& parent) {
            for (const auto& index : parent.value("childIndices", nlohmann::json::array())) {
                const auto child = index.get<size_t>();
                if (child >= bodyPlan.size()) continue;
                const auto& childNode = bodyPlan[child];
                if (childNode.value("head", "") == "else") continue;
                const auto operation = operations.find(child);
                if (operation != operations.end()) {
                    out << "if (!jtml_static_component_patch_" << componentIndex << "(" << child
                        << ", definition.bodyPlan[" << child << "], instance, definition, scope, h)) return false; ";
                    if (operation->second != "container-attrs") continue;
                }
                patchChildren(childNode);
            }
        };
        out << "if (branch) { ";
        patchChildren(*node);
        out << "} else { ";
        nlohmann::json elseNode;
        if (findStaticElseSibling(componentPlan, nodeIndex, *node, elseNode)) patchChildren(elseNode);
        out << "} return true; } "
            // Destroy before creating: creation registers the new generation.
            << "if (h && h.destroyDirectComponentInstancesUnderElement) h.destroyDirectComponentInstancesUnderElement(el, 'subtree-prune'); ";
    }
    if (head == "for") {
        out << "const node = (definition && definition.bodyPlan || [])["
            << nodeIndex << "]; ";
        const auto loopPlan = node->value("loopPlan", nlohmann::json::object());
        const auto itemName = loopPlan.value("item", "");
        const auto collectionPlan = loopPlan.value("collectionPlan", nlohmann::json::object());
        const auto keyExpression = node->value("keyExpression", "");
        const auto keyPlan = node->value("keyExpressionPlan", nlohmann::json::object());
        if (!retainedItemPatch.empty() && !itemName.empty() &&
            isDirectJsExpressionPlan(collectionPlan) &&
            (keyExpression.empty() || isDirectJsExpressionPlan(keyPlan))) {
            out << "if ((function(){ "
                << "const expressionStarted = performance.now(); "
                << "let values = " << jsDirectExpressionCall(collectionPlan) << "; "
                << "if (values == null) values = []; "
                << "else if (!Array.isArray(values)) { "
                << "if (typeof values === 'string') values = values.split(''); "
                << "else if (typeof values === 'object') values = Object.values(values); "
                << "else values = [values]; } "
                << "const items = []; const keys = []; const seen = {}; "
                << "for (let itemIndex = 0; itemIndex < values.length; itemIndex += 1) { "
                << "const childScope = Object.assign({}, scope); childScope["
                << jsStringLiteral(itemName) << "] = values[itemIndex]; "
                << "let keyValue = ";
            if (keyExpression.empty()) {
                out << "itemIndex";
            } else {
                out << "(function(scope){ return " << jsDirectExpressionCall(keyPlan)
                    << "; })(childScope)";
            }
            out << "; if (keyValue == null || keyValue === '') keyValue = itemIndex; "
                << "const key = String(keyValue).replace(/[^A-Za-z0-9_.:-]/g, '_'); "
                << "if (seen[key]) return false; seen[key] = true; "
                << "keys.push(key); items.push({ key: key, index: itemIndex, scope: childScope }); } "
                << "const expressionEvaluationMs = performance.now() - expressionStarted; "
                << "const existing = Array.prototype.slice.call(el.children || []).filter(function(child){ "
                << "return child && child.getAttribute && child.getAttribute('data-jtml-direct-list-item') === "
                << jsStringLiteral(std::to_string(nodeIndex)) << "; }); "
                << "if (existing.length !== items.length) return false; "
                << "const existingByKey = {}; const previousKeys = []; "
                << "for (let index = 0; index < existing.length; index += 1) { "
                << "const key = String(existing[index].getAttribute('data-jtml-direct-list-key') || ''); "
                << "if (Object.prototype.hasOwnProperty.call(existingByKey, key)) return false; "
                << "existingByKey[key] = existing[index]; previousKeys.push(key); } "
                << "for (let index = 0; index < keys.length; index += 1) { "
                << "if (!Object.prototype.hasOwnProperty.call(existingByKey, keys[index])) return false; } "
                << "const previousIndex = {}; previousKeys.forEach(function(key, index){ previousIndex[key] = index; }); "
                << "const movedKeys = keys.filter(function(key, index){ return previousIndex[key] !== index; }); "
                << "const patchItem = " << retainedItemPatch << "; const wrappers = []; "
                << "const patchStarted = performance.now(); "
                << "for (let index = 0; index < items.length; index += 1) { "
                << "const item = items[index]; const wrapper = existingByKey[item.key]; "
                << "wrapper.setAttribute('data-jtml-direct-list-index', String(index)); "
                << "if (!patchItem(wrapper, item.scope)) return false; wrappers.push(wrapper); } "
                << "const domPatchingMs = performance.now() - patchStarted; "
                << "const movementStarted = performance.now(); let physicalMoves = 0; let cursor = el.firstElementChild; "
                << "for (let index = 0; index < wrappers.length; index += 1) { "
                << "const wrapper = wrappers[index]; if (wrapper === cursor) cursor = cursor.nextElementSibling; "
                << "else { el.insertBefore(wrapper, cursor); physicalMoves += 1; } } "
                << "const movementMs = performance.now() - movementStarted; "
                << "const lifecycleStarted = performance.now(); "
                << "const stageTimingsMs = { expressionEvaluation: expressionEvaluationMs, htmlRendering: 0, "
                << "htmlParsing: 0, domPatching: domPatchingMs, movement: movementMs, replacement: 0, lifecycleBookkeeping: 0 }; "
                << "window.jtml = Object.assign(window.jtml || {}, { "
                << "directComponentListLifecycle: { componentId: instance && instance.id || '', "
                << "component: instance && instance.component || definition && definition.name || '', "
                << "moduleId: instance && instance.moduleId == null ? null : instance && instance.moduleId, "
                << "bodyNodeIndex: " << nodeIndex << ", bodySourceLine: node && node.sourceLine || 0, "
                << "bodySourceColumn: node && node.sourceColumn || 0, previousKeys: previousKeys, currentKeys: keys.slice(), "
                << "insertedKeys: [], removedKeys: [], movedKeys: movedKeys, inserted: 0, removed: 0, moved: movedKeys.length, "
                << "mode: 'keyed-list-markers' }, "
                << "directComponentKeyedListPatch: { componentId: instance && instance.id || '', "
                << "component: instance && instance.component || definition && definition.name || '', bodyNodeIndex: "
                << nodeIndex << ", previousKeys: previousKeys, keys: keys, retainedKeys: keys.slice(), insertedKeys: [], "
                << "removedKeys: [], movedKeys: movedKeys, reused: wrappers.length, created: 0, removed: 0, "
                << "prunedDynamicInstances: 0, generatedItemPatches: wrappers.length, itemElementPatches: 0, "
                << "itemTextPatches: 0, itemHtmlReplacements: 0, physicalMoves: physicalMoves, "
                << "reordered: movedKeys.length > 0, mode: 'keyed-for-region-patch', "
                << "reconciliation: 'generated-retained-row-patch', stageTimingsMs: stageTimingsMs } }); "
                << "stageTimingsMs.lifecycleBookkeeping = performance.now() - lifecycleStarted; return true; })()) return true; ";
        }
        out << "if (h && h.patchComponentForRegion && "
            << "h.patchComponentForRegion(instance, definition, node, scope, el)) return true; ";
    }
    out << "const html = " << directCreate << "; "
        << "if (typeof html !== 'string') return false; ";
    if (head != "if") {
        out << "if (h && h.destroyDirectComponentInstancesUnderElement) "
            << "h.destroyDirectComponentInstancesUnderElement(el, "
            << jsStringLiteral(replaceReason) << "); ";
    }
    out << "el.outerHTML = html; "
        << "return true; "
        << "})()";
    return out.str();
}

std::string directStaticSlotPatchExpression(size_t nodeIndex,
                                            const nlohmann::json& operation) {
    const auto words = operation.value("words", nlohmann::json::array());
    std::string slotName;
    if (words.is_array() && words.size() > 1 && words[1].is_string()) {
        slotName = words[1].get<std::string>();
    }
    nlohmann::json node = {
        {"text", slotName.empty() ? std::string("slot") : std::string("slot ") + slotName}
    };
    const auto directCreate = directStaticSlotCreateExpression(nodeIndex, node);
    std::ostringstream out;
    out << "(function(){ "
        << "const el = h && h.directComponentElementForNode ? "
        << "h.directComponentElementForNode(instance, " << nodeIndex << ") : "
        << "(instance && instance.element ? instance.element.querySelector('[data-jtml-direct-body-node=\""
        << nodeIndex << "\"]') : null); "
        << "if (!el) return false; "
        << "const html = " << directCreate << "; "
        << "if (typeof html !== 'string' || !html.length) return false; "
        << "el.outerHTML = html; "
        << "return true; "
        << "})()";
    return out.str();
}

std::string directStaticNestedPatchExpression(size_t nodeIndex,
                                              const nlohmann::json& operation) {
    const auto name = operation.value("head", "");
    if (name.empty()) return "";
    const auto directCreate = directStaticNestedCreateExpression(
        nodeIndex,
        name,
        operation.value("childComponentKey", ""),
        operation.value("childComponentId", -1));
    std::ostringstream out;
    out << "(function(){ "
        << "if (h && h.patchStaticComponentNestedParams && "
        << "h.patchStaticComponentNestedParams(instance, definition, "
        << nodeIndex << ", scope, " << jsStringLiteral(name) << ")) return true; "
        << "const el = h && h.directComponentElementForNode ? "
        << "h.directComponentElementForNode(instance, " << nodeIndex << ") : "
        << "(instance && instance.element ? instance.element.querySelector('[data-jtml-direct-body-node=\""
        << nodeIndex << "\"]') : null); "
        << "if (!el) return false; "
        << "const html = " << directCreate << "; "
        << "if (typeof html !== 'string' || !html.length) return false; "
        << "el.outerHTML = html; "
        << "return true; "
        << "})()";
    return out.str();
}

std::string directStaticPatchExpression(const nlohmann::json& componentPlan,
                                        const nlohmann::json& operation,
                                        bool enableGeneratedKeyedPatch,
                                        size_t componentIndex) {
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
    } else if (kind == "region") {
        std::string directPatch;
        if (operation.value("head", "") == "slot") {
            directPatch = directStaticSlotPatchExpression(
                static_cast<size_t>(nodeIndex), operation);
        } else {
            directPatch = directStaticRegionPatchExpression(
                componentPlan,
                static_cast<size_t>(nodeIndex),
                enableGeneratedKeyedPatch, componentIndex);
        }
        if (!directPatch.empty()) {
            out << directPatch;
        } else {
            out << "(h && h.patchStaticComponentRegionNode ? "
                << "h.patchStaticComponentRegionNode(instance, definition, "
                << nodeIndex << ", scope) : false)";
        }
    } else if (kind == "nested-component") {
        const auto directPatch = directStaticNestedPatchExpression(
            static_cast<size_t>(nodeIndex), operation);
        if (!directPatch.empty()) {
            out << directPatch;
        } else {
            out << "(h && h.patchStaticComponentRegionNode ? "
                << "h.patchStaticComponentRegionNode(instance, definition, "
                << nodeIndex << ", scope) : false)";
        }
    } else {
        return "";
    }
    return out.str();
}

std::string directStaticPatchCase(const nlohmann::json& componentPlan,
                                  const nlohmann::json& entry,
                                  bool enableGeneratedKeyedPatch,
                                  size_t componentIndex) {
    if (!entry.is_object() || !entry.contains("index") ||
        !entry.contains("operation")) {
        return "";
    }
    const auto operation = entry["operation"];
    const auto patchExpression = directStaticPatchExpression(
        componentPlan, operation, enableGeneratedKeyedPatch, componentIndex);
    if (patchExpression.empty()) return "";
    const auto index = entry.value("index", 0);
    std::ostringstream out;
    out << "      case " << index << ": {\n"
        << "        return " << patchExpression << ";\n"
        << "      }\n";
    return out.str();
}

std::string staticUpdateAffectedIndexesFunctionBody(const nlohmann::json& componentPlan) {
    std::ostringstream out;
    out << "    function affectedIndexes() {\n"
        << "      const out = [];\n"
        << "      const seen = {};\n"
        << "      const subtreeSeen = {};\n"
        << "      const parentOf = {";
    bool firstParent = true;
    const auto bodyPlan = componentPlan.value("bodyPlan", nlohmann::json::array());
    if (bodyPlan.is_array()) {
        for (size_t i = 0; i < bodyPlan.size(); ++i) {
            if (!bodyPlan[i].is_object()) continue;
            int parentIndex = bodyPlan[i].value("parentIndex", -1);
            // Else is rendered by its preceding if region, not by a separate
            // DOM region. Route descendant suppression through that owner.
            if (bodyPlan[i].value("head", "") == "else") {
                for (size_t sibling = i; sibling-- > 0;) {
                    if (bodyPlan[sibling].value("parentIndex", -2) != parentIndex) continue;
                    if (bodyPlan[sibling].value("head", "") == "if") {
                        parentIndex = static_cast<int>(sibling);
                    }
                    break;
                }
            }
            if (parentIndex < 0) continue;
            if (!firstParent) out << ",";
            firstParent = false;
            out << i << ":" << parentIndex;
        }
    }
    out << "};\n"
        << "      function ancestorSeen(index) {\n"
        << "        let parent = parentOf[index];\n"
        << "        let guard = 0;\n"
        << "        while (parent != null && guard++ < " << bodyPlan.size() << ") {\n"
        << "          if (subtreeSeen[parent]) return true;\n"
        << "          parent = parentOf[parent];\n"
        << "        }\n"
        << "        return false;\n"
        << "      }\n"
        << "      const writes = {};\n"
        << "      (changed || []).forEach(function(name) { writes[name] = true; });\n";
    const auto entries = componentPlan.value("entries", nlohmann::json::array());
    for (const auto& entry : entries) {
        if (!entry.is_object() || !entry.contains("index")) continue;
        const auto index = entry.value("index", -1);
        if (index < 0) continue;
        out << "      if (ancestorSeen(" << index << ")) {}\n"
            << "      else ";
        out << "if (!seen[" << index << "]";
        const auto reads = entry.value("reads", nlohmann::json::array());
        for (const auto& read : reads) {
            if (!read.is_string()) continue;
            out << " && !writes[" << jsStringLiteral(read.get<std::string>()) << "]";
        }
        out << ") {}\n";
        // Attribute-only container patches leave their descendants untouched.
        // Only patches covering the subtree may suppress descendant work.
        out << "      else if (!seen[" << index << "]) { seen[" << index
            << "] = true; ";
        if (entry.value("operation", nlohmann::json::object())
                .value("operation", "") != "container-attrs") {
            out << "subtreeSeen[" << index << "] = true; ";
        }
        out << "out.push(" << index << "); }\n";
    }
    out << "      return out;\n"
        << "    }\n";
    return out.str();
}

std::string staticUpdateUnsafeAffectedFunctionBody(const nlohmann::json& componentPlan) {
    std::ostringstream out;
    out << "    function hasUnsafeAffectedNode() {\n"
        << "      const writes = {};\n"
        << "      (changed || []).forEach(function(name) { writes[name] = true; });\n";
    const auto entries = componentPlan.value("unsafeEntries", nlohmann::json::array());
    for (const auto& entry : entries) {
        if (!entry.is_object()) continue;
        out << "      if (";
        bool emitted = false;
        for (const auto& read : entry.value("reads", nlohmann::json::array())) {
            if (!read.is_string()) continue;
            if (emitted) out << " || ";
            out << "writes[" << jsStringLiteral(read.get<std::string>()) << "]";
            emitted = true;
        }
        out << (emitted ? ") return true;\n" : "false) return true;\n");
    }
    out << "      return false;\n"
        << "    }\n";
    return out.str();
}

std::string staticCreateFallbackFunctionBody(size_t componentIndex) {
    std::ostringstream out;
    out << "    function jtml_static_component_create_fallback_"
        << componentIndex << "() {\n"
        << "      if (h && h.recordStaticCreateFallback) h.recordStaticCreateFallback(instance, definition, plan, \"static component create function fell back to runtime renderer\");\n"
        << "      if (!h || !h.renderStaticComponentRoots) return '';\n"
        << "      return h.renderStaticComponentRoots(instance, definition, scope);\n"
        << "    }\n";
    return out.str();
}

std::string staticUpdateFunctionBody(size_t componentIndex,
                                     const nlohmann::json& componentPlan,
                                     bool enableGeneratedKeyedPatch) {
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
        << "(nodeIndex, node, instance, definition, scope, h) {\n"
        << "    switch (nodeIndex) {\n";
    size_t emittedPatchCases = 0;
    const auto entries = componentPlan.value("entries", nlohmann::json::array());
    for (const auto& entry : entries) {
        const auto patchCase = directStaticPatchCase(
            componentPlan, entry, enableGeneratedKeyedPatch, componentIndex);
        if (patchCase.empty()) continue;
        out << patchCase;
        emittedPatchCases += 1;
    }
    (void)emittedPatchCases;
    out << "      default: return false;\n"
        << "    }\n"
        << "  }\n";
    out << "  function jtml_static_component_destroy_" << componentIndex
        << "(instance, definition, h) {\n"
        << "    const plan = plans.components[" << componentIndex << "];\n"
        << "    const childrenDestroyed = h && h.destroyDirectComponentChildren ? h.destroyDirectComponentChildren(instance, definition) : 0;\n"
        << "    if (h && h.unregisterComponentInstance) h.unregisterComponentInstance(instance, definition);\n"
        << "    if (plan) plan.__lastDestroyMode = 'static-production-destroy-function';\n"
        << "    return { handled: true, destroyed: 1, childrenDestroyed: childrenDestroyed };\n"
        << "  }\n";
    out << "  function jtml_static_component_update_" << componentIndex
        << "(instance, definition, changed, scope, h) {\n"
        << "    const plan = plans.components[" << componentIndex << "];\n"
        << "    if (!plan) return { handled: false, patched: 0 };\n"
        << staticUpdateAffectedIndexesFunctionBody(componentPlan)
        << staticUpdateUnsafeAffectedFunctionBody(componentPlan)
        << "    const affected = affectedIndexes();\n"
        << "    plan.__lastAffectedNodeCount = affected.length;\n"
        << "    if (!affected.length) return { handled: false, patched: 0 };\n"
        << "    const unsafeNode = h && h.firstUnsafeAffectedRenderNodeFromPlan ? h.firstUnsafeAffectedRenderNodeFromPlan(plan, definition, changed) : null;\n"
        << "    const unsafeGeneratedNode = !unsafeNode && hasUnsafeAffectedNode();\n"
        << "    if (unsafeNode || unsafeGeneratedNode) {\n"
        << "      if (h && h.recordCompiledPatchFallback) h.recordCompiledPatchFallback(instance, definition, unsafeNode || null, \"affected body-plan node requires full rerender\", changed);\n"
        << "      return { handled: false, patched: 0 };\n"
        << "    }\n"
        << "    let patched = 0;\n"
        << "    for (let i = 0; i < affected.length; i += 1) {\n"
        << "      const nodeIndex = affected[i];\n"
        << "      const node = (definition.bodyPlan || [])[nodeIndex];\n"
        << "      if (!node) return { handled: false, patched: patched };\n"
        << "      if (!jtml_static_component_patch_" << componentIndex
        << "(nodeIndex, node, instance, definition, scope, h) &&\n"
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

std::string emitStaticComponentAsset(const RuntimeProjectPlan& plan,
                                     bool componentModule) {
    auto fullComponents = assignComponentIdsAndChildIds(collectComponentPlans(plan));
    // Source provenance belongs to compiler diagnostics, not shipped execution
    // records (including explicit compatibility assets).
    eraseJsonKeyRecursive(fullComponents, "sourceSpan");
    eraseJsonKeyRecursive(fullComponents, "originalLocation");
    const auto components = componentModule
        ? componentModulePlansPayload(fullComponents)
        : fullComponents;
    nlohmann::json asset = {
        {"version", 1},
        {"sourceOfTruth", "runtime client manifest"},
        {"mode", componentModule
            ? "csp-safe static component modules"
            : "csp-safe static update plans"},
        {"dynamicGeneratedUpdateFunctions", false},
        {"staticUpdateFunctions", true},
        {"assetRole", componentModule ? "component-module" : "legacy-update-plan"},
        {"componentCount", components.size()},
        {"components", components},
    };

    std::ostringstream out;
    out << "/* Generated by jtml build --target browser. "
        << (componentModule
            ? "CSP-safe static component module seed."
            : "CSP-safe static update plan/function seed.")
        << " */\n"
        << "(function () {\n"
        << "  const plans = JSON.parse('" << escapeJsScriptText(asset.dump()) << "');\n"
        << "  const functions = {};\n";
    out << "  const modules = {};\n";
    out << "  const modulesById = {};\n";
    for (size_t i = 0; i < components.size(); ++i) {
        out << staticUpdateFunctionBody(i, fullComponents[i], componentModule)
            << "  if (plans.components[" << i << "] && plans.components[" << i << "].key) {\n"
            << "    functions[plans.components[" << i << "].key] = jtml_static_component_update_" << i << ";\n"
            << "    modules[plans.components[" << i << "].key] = {\n"
            << "      create: jtml_static_component_create_" << i << ",\n"
            << "      update: jtml_static_component_update_" << i << ",\n"
            << "      destroy: jtml_static_component_destroy_" << i << ",\n"
            << "      component: plans.components[" << i << "].name || '',\n"
            << "      moduleId: plans.components[" << i << "].moduleId == null ? null : plans.components[" << i << "].moduleId,\n"
            << "      key: plans.components[" << i << "].key\n"
            << "    };\n"
            << "    if (plans.components[" << i << "].componentId != null) modulesById[plans.components[" << i << "].componentId] = modules[plans.components[" << i << "].key];\n"
            << "  }\n";
    }
    out
        << "  plans.staticUpdateFunctionCount = Object.keys(functions).length;\n"
        << "  plans.staticComponentModuleCount = Object.keys(modules).length;\n"
        << (componentModule
            ? "  window.__jtml_static_component_plan_index = plans;\n"
            : "  window.__jtml_static_update_plans = plans;\n")
        << "  window.__jtml_static_update_functions = functions;\n"
        << "  window.__jtml_static_component_modules = modules;\n"
        << "  window.__jtml_static_component_modules_by_id = modulesById;\n"
        << "  if (window.jtml) {\n"
        << (componentModule
            ? "    window.jtml.staticComponentPlanIndex = plans;\n"
            : "    window.jtml.staticUpdatePlans = plans;\n")
        << "    window.jtml.staticUpdateFunctions = functions;\n"
        << "    window.jtml.staticComponentModules = modules;\n"
        << "    window.jtml.staticComponentModulesById = modulesById;\n"
        << "    window.jtml.runtimeSecurity = window.jtml.runtimeSecurity || {};\n"
        << (componentModule
            ? "    window.jtml.runtimeSecurity.staticComponentPlanIndexAsset = true;\n"
            : "    window.jtml.runtimeSecurity.staticUpdatePlansAsset = true;\n")
        << "    window.jtml.runtimeSecurity.staticUpdateFunctionsAsset = true;\n"
        << "    window.jtml.runtimeSecurity.staticComponentModulesAsset = true;\n"
        << "  }\n"
        << "  if (window.dispatchEvent && window.CustomEvent) {\n"
        << "    window.dispatchEvent(new CustomEvent('"
        << (componentModule
            ? "jtml:static-component-modules-ready"
            : "jtml:static-update-plans-ready")
        << "', { detail: plans }));\n"
        << "  }\n"
        << "}());\n";
    return out.str();
}

std::string emitStaticUpdatePlanAsset(const RuntimeProjectPlan& plan) {
    return emitStaticComponentAsset(plan, false);
}

std::vector<std::string> validateStaticComponentModuleForProduction(
        const RuntimeProjectPlan& plan) {
    std::vector<std::string> issues;
    std::map<SemanticModuleId, std::string> modulePaths;
    for (const auto& module : plan.modules) {
        if (module.id != InvalidSemanticModuleId && !module.path.empty()) {
            modulePaths[module.id] = module.path;
        }
    }

    auto sourceLabelFor = [&](const nlohmann::json& component,
                              const nlohmann::json& entry) {
        const auto original = entry.value("originalLocation", nlohmann::json());
        if (original.is_object()) {
            std::ostringstream out;
            if (!original.value("exact", false)) out << "generated code attributed to ";
            out << original.value("file", "") << ":" << original.value("line", 0)
                << ":" << original.value("column", 0);
            return out.str();
        }
        // Compatibility/manual plans have no authoritative original range.
        // Preserve useful generated coordinates, explicitly labelled as such.
        std::ostringstream out;
        out << "lowered line " << entry.value("sourceLine", 0);
        if (entry.value("sourceColumn", 0) > 0) out << ":" << entry.value("sourceColumn", 0);
        if (component.contains("moduleId") && component["moduleId"].is_number_integer()) {
            const auto id = static_cast<SemanticModuleId>(component["moduleId"].get<int>());
            const auto found = modulePaths.find(id);
            if (found != modulePaths.end()) out << " (module " << found->second << ")";
        }
        return out.str();
    };

    const auto components = assignComponentIdsAndChildIds(collectComponentPlans(plan));
    if (!components.is_array()) return issues;

    for (const auto& component : components) {
        if (!component.is_object()) continue;
        const auto name = component.value("name", "<anonymous>");
        const auto key = component.value("key", "");
        const auto origin = component.value("origin", "");
        const auto context =
            name + (origin.empty() ? "" : " (" + origin + ")") +
            (key.empty() ? "" : " [" + key + "]");

        const auto body = component.value("bodyPlan", nlohmann::json::array());
        // Bindings, not bare names, carry ownership. A loop iterator receives
        // the provenance of its collection in the enclosing lexical scope.
        struct Binding {
            std::string name;
            int declaration;
            bool writable;
            bool iterator;
            bool borrowed;
            std::set<int> sources;
        };
        std::vector<Binding> bindings;
        std::map<std::pair<int, std::string>, std::vector<int>> scopes;
        auto declare = [&](int scope, const std::string& name, int declaration,
                           bool writable, bool iterator, bool borrowed) {
            const int id = static_cast<int>(bindings.size());
            bindings.push_back({name, declaration, writable, iterator, borrowed, {}});
            scopes[{scope, name}].push_back(id);
            return id;
        };
        const auto componentParams = component.value("params", std::vector<std::string>{});
        for (const auto& param : componentParams) declare(-1, param, -1, false, false, true);
        std::map<size_t, int> declarations;
        for (size_t i = 0; i < body.size(); ++i) {
            const auto& node = body[i];
            const auto kind = node.value("kind", "");
            if (kind == "state" || kind == "derived") {
                const auto scope = node.value("parentIndex", -1);
                const auto local = node.value("name", "");
                // Component scopes refresh parameters before every action; a
                // same-scope declaration cannot turn that parameter into owned state.
                const bool parameter = scope == -1 && std::find(componentParams.begin(), componentParams.end(), local) != componentParams.end();
                declarations[i] = declare(scope, local, static_cast<int>(i),
                    kind == "state" && node.value("head", "") == "let" && !parameter, false, parameter);
            } else if (kind == "action") {
                const auto words = splitStaticComponentWords(node.value("text", ""));
                for (size_t j = 2; j < words.size(); ++j) declare(static_cast<int>(i), words[j], static_cast<int>(i), false, false, true);
            } else if (node.value("head", "") == "for") {
                const auto loop = node.value("loopPlan", nlohmann::json::object());
                declarations[i] = declare(static_cast<int>(i), loop.value("item", ""), static_cast<int>(i), false, true, false);
            }
        }
        // Runtime declarations write to one persistent component scope. Unlike
        // action parameters and loop iterators, they are not restored on exit.
        // Lexical ownership is sound only when a nested declaration cannot
        // overwrite a different binding, including in sibling branches/actions.
        std::map<std::string, size_t> bindingCounts;
        for (const auto& binding : bindings) ++bindingCounts[binding.name];
        for (const auto& declaration : declarations) {
            const auto& node = body[declaration.first];
            const auto kind = node.value("kind", "");
            if ((kind != "state" && kind != "derived") || node.value("parentIndex", -1) == -1) continue;
            const auto local = node.value("name", "");
            if (bindingCounts[local] <= 1) continue;
            issues.push_back("component " + context + " has unsupported production nested declaration `" +
                local + "` at " + sourceLabelFor(component, node) +
                "; declarations share runtime storage and may overwrite owned or borrowed bindings; use a unique local name");
        }
        auto resolve = [&](const std::string& name, size_t at) {
            int scope = body[at].value("parentIndex", -1);
            for (size_t guard = 0; guard <= body.size(); ++guard) {
                const auto found = scopes.find({scope, name});
                if (found != scopes.end()) {
                    for (auto it = found->second.rbegin(); it != found->second.rend(); ++it) {
                        const auto declaration = bindings[*it].declaration;
                        if (declaration != static_cast<int>(at) && (scope == -1 || declaration < static_cast<int>(at))) return *it;
                    }
                }
                if (scope < 0 || static_cast<size_t>(scope) >= body.size()) break;
                scope = body[scope].value("parentIndex", -1);
            }
            return -1;
        };
        auto expressionSources = [&](int binding, const std::string& expression, size_t at) {
            const auto rhs = parseExpressionIr(expression);
            if (!rhs || !expressionIrSupportsDirectJs(*rhs)) {
                bindings[binding].borrowed = true;
                return;
            }
            // Unary/arithmetic/comparison results are scalar copies, never aliases.
            // Logical &&/|| can return an object operand and retain provenance.
            if (rhs->kind == ExpressionIrKind::Unary ||
                (rhs->kind == ExpressionIrKind::Binary && rhs->op != "&&" && rhs->op != "||")) return;
            for (const auto& read : expressionIrDependencies(*rhs)) {
                const auto source = resolve(read.substr(0, read.find_first_of(".[")), at);
                if (source < 0) bindings[binding].borrowed = true;
                else bindings[binding].sources.insert(source);
            }
        };
        for (size_t i = 0; i < body.size(); ++i) {
            const auto& node = body[i];
            const auto declaration = declarations.find(i);
            if (declaration != declarations.end()) {
                const auto expression = node.value("head", "") == "for"
                    ? node.value("loopPlan", nlohmann::json::object()).value("collectionExpression", "")
                    : node.value("expression", "");
                expressionSources(declaration->second, expression, i);
            } else if (node.value("kind", "") == "assignment") {
                const auto target = node.value("name", "");
                const auto binding = resolve(target.substr(0, target.find_first_of(".[")), i);
                if (binding >= 0) expressionSources(binding, node.value("expression", ""), i);
            }
        }
        bool borrowingChanged = true;
        while (borrowingChanged) {
            borrowingChanged = false;
            for (auto& binding : bindings) {
                for (const auto source : binding.sources) {
                    if (!binding.borrowed && bindings[source].borrowed) {
                        binding.borrowed = true;
                        borrowingChanged = true;
                    }
                }
            }
        }
        std::function<bool(const ExpressionIr&)> writablePath = [&](const ExpressionIr& target) {
            if (target.kind == ExpressionIrKind::Path) return !target.root.empty();
            if (target.kind != ExpressionIrKind::Member && target.kind != ExpressionIrKind::Subscript) return false;
            if (target.children.empty() || !target.children[0] || !writablePath(*target.children[0])) return false;
            return expressionIrSupportsDirectJs(target);
        };
        for (size_t i = 0; i < body.size(); ++i) {
            const auto& node = body[i];
            if (node.value("kind", "") != "assignment") continue;
            const auto target = node.value("name", "");
            const auto root = target.substr(0, target.find_first_of(".["));
            const auto lhs = parseExpressionIr(target);
            const auto rhs = parseExpressionIr(node.value("expression", ""));
            const std::set<std::string> operators = {"=", "+=", "-=", "*=", "/=", "%="};
            if (!lhs || !writablePath(*lhs) || !rhs || !expressionIrSupportsDirectJs(*rhs) ||
                !operators.count(node.value("operator", ""))) {
                issues.push_back("component " + context + " has unsupported production assignment `" + target +
                    "` at " + sourceLabelFor(component, node) +
                    "; use a writable path with statically analyzable indices and RHS; unknown or side-effectful calls are unsupported");
                continue;
            }
            const auto id = resolve(root, i);
            const bool path = root != target;
            if (id >= 0 && ((!bindings[id].writable && !bindings[id].iterator) || (path && bindings[id].borrowed))) {
                issues.push_back("component " + context + " has unsupported production write to borrowed value `" +
                    target + "` at " + sourceLabelFor(component, node) +
                    "; use component-owned state or a parent action; parameters are read-only, as are borrowed iterators");
            } else if (id < 0 || (!bindings[id].writable && !(path && bindings[id].iterator))) {
                issues.push_back("component " + context + " has unsupported production write target `" +
                    target + "` at " + sourceLabelFor(component, node) +
                    "; write a component-local let value or an owned collection member");
            }
        }

        for (const auto& node : component.value("bodyPlan", nlohmann::json::array())) {
            if (node.value("kind", "") == "template") {
                const auto words = splitStaticComponentWords(node.value("text", ""));
                const auto parts = compileStaticElementParts(words, 1);
                for (const auto& attr : parts.value("attrs", nlohmann::json::array())) {
                    if (attr.value("name", "") == "data-jtml-direct-component-keydown" &&
                        attr.value("exprPlan", nlohmann::json::object()).value("kind", "") == "source") {
                        issues.push_back("component " + context + " has unsupported production keydown invocation at " +
                            sourceLabelFor(component, node) + "; use a bare component action name");
                    }
                }
            }
            if (node.value("head", "") != "if") continue;
            if (isDirectJsExpressionPlan(node.value("expressionPlan", nlohmann::json::object()))) continue;
            const auto source = sourceLabelFor(component, node);
            issues.push_back("component " + context +
                " has unsupported production conditional expression at " + source +
                "; use a statically analyzable expression over observable values");
        }

        const auto unsafeRoots =
            component.value("unsafeRootCreateEntries", nlohmann::json::array());
        for (const auto& entry : unsafeRoots) {
            if (!entry.is_object()) continue;
            std::ostringstream issue;
            issue << "component " << context
                  << " has unsupported production create node `"
                  << entry.value("kind", "<unknown>") << "`";
            const auto source = sourceLabelFor(component, entry);
            if (!source.empty()) {
                issue << " at " << source;
            }
            issues.push_back(issue.str());
        }

        const auto unsafeUpdates =
            component.value("unsafeEntries", nlohmann::json::array());
        for (const auto& entry : unsafeUpdates) {
            if (!entry.is_object()) continue;
            std::ostringstream issue;
            issue << "component " << context
                  << " has unsupported production update node `"
                  << entry.value("kind", "<unknown>") << "`";
            const auto source = sourceLabelFor(component, entry);
            if (!source.empty()) {
                issue << " at " << source;
            }
            issues.push_back(issue.str());
        }
    }

    return issues;
}

std::string emitStaticComponentModuleAsset(const RuntimeProjectPlan& plan) {
    return emitStaticComponentAsset(plan, true);
}

} // namespace jtml
