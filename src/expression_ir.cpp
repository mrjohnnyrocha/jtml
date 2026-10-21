#include "jtml/expression_ir.h"

#include "jtml/expression_source.h"
#include "jtml/lexer.h"
#include "jtml/parser.h"

#include <cctype>
#include <set>
#include <sstream>

namespace jtml {
namespace {

void appendUnique(std::vector<std::string>& out, const std::string& value) {
    if (value.empty()) return;
    for (const auto& existing : out) {
        if (existing == value) return;
    }
    out.push_back(value);
}

std::string rootDependencyName(const std::string& path) {
    const auto dot = path.find('.');
    const auto bracket = path.find('[');
    const auto end = std::min(dot == std::string::npos ? path.size() : dot,
                              bracket == std::string::npos ? path.size() : bracket);
    return path.substr(0, end);
}

std::string trimCopy(std::string value) {
    auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
    while (!value.empty() && isSpace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && isSpace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

std::unique_ptr<ExpressionIr> makeIr(ExpressionIrKind kind,
                                     const ExpressionStatementNode* expression) {
    auto ir = std::make_unique<ExpressionIr>();
    ir->kind = kind;
    ir->source = expression ? expressionSource(expression) : "";
    if (expression) ir->sourceSpan = expression->sourceSpan;
    ir->astKind = expression ? static_cast<int>(expression->getExprType()) : -1;
    return ir;
}

ExpressionValueType typeForRoot(const ExpressionTypeEnvironment& environment,
                                const std::string& root) {
    const auto it = environment.symbols.find(root);
    return it == environment.symbols.end() ? ExpressionValueType::Unknown : it->second;
}

std::unique_ptr<ExpressionIr> makeStringLiteralIr(std::string value) {
    auto ir = std::make_unique<ExpressionIr>();
    ir->kind = ExpressionIrKind::Literal;
    ir->valueType = ExpressionValueType::String;
    ir->source = nlohmann::json(value).dump();
    ir->literal = std::move(value);
    return ir;
}

nlohmann::json sourceSpanToJson(const SourceSpan& span) {
    if (!span.valid()) return nullptr;
    return {
        {"startLine", span.startLine},
        {"startColumn", span.startColumn},
        {"endLine", span.endLine},
        {"endColumn", span.endColumn},
        {"startOffset", span.startOffset},
        {"endOffset", span.endOffset},
    };
}

std::unique_ptr<ExpressionIr> makeStringConcatenationIr(std::unique_ptr<ExpressionIr> left,
                                                        std::unique_ptr<ExpressionIr> right,
                                                        const ExpressionStatementNode* expression) {
    auto ir = makeIr(ExpressionIrKind::Binary, expression);
    ir->sourceSpan.originalExact = false;
    ir->op = "+";
    ir->children.push_back(std::move(left));
    ir->children.push_back(std::move(right));
    ir->valueType = ExpressionValueType::String;
    return ir;
}

size_t findInterpolationClose(const std::string& value, size_t open) {
    int depth = 0;
    char quote = '\0';
    for (size_t i = open + 1; i < value.size(); ++i) {
        const char ch = value[i];
        if (quote != '\0') {
            if (ch == '\\' && i + 1 < value.size()) {
                ++i;
                continue;
            }
            if (ch == quote) quote = '\0';
            continue;
        }
        if (ch == '"' || ch == '\'') {
            quote = ch;
            continue;
        }
        if (ch == '{') {
            ++depth;
            continue;
        }
        if (ch == '}') {
            if (depth == 0) return i;
            --depth;
        }
    }
    return std::string::npos;
}

void attributeGeneratedExpression(ExpressionIr& ir, const SourceSpan& source) {
    ir.sourceSpan = source;
    ir.sourceSpan.originalExact = false;
    for (auto& child : ir.children) if (child) attributeGeneratedExpression(*child, source);
    for (auto& entry : ir.entries) if (entry.value) attributeGeneratedExpression(*entry.value, source);
}

std::unique_ptr<ExpressionIr> buildInterpolatedStringIr(const std::string& value,
                                                        const ExpressionStatementNode* expression,
                                                        const ExpressionTypeEnvironment& environment) {
    std::vector<std::unique_ptr<ExpressionIr>> parts;
    std::string literal;
    bool sawInterpolation = false;
    for (size_t i = 0; i < value.size();) {
        if (value[i] == '\\' && i + 1 < value.size() &&
            (value[i + 1] == '{' || value[i + 1] == '}' || value[i + 1] == '\\')) {
            literal.push_back(value[i + 1]);
            i += 2;
            continue;
        }
        if (value[i] != '{') {
            literal.push_back(value[i++]);
            continue;
        }
        const auto close = findInterpolationClose(value, i);
        if (close == std::string::npos) {
            literal.push_back(value[i++]);
            continue;
        }
        const auto inside = trimCopy(value.substr(i + 1, close - i - 1));
        if (inside.empty()) {
            literal.append(value.substr(i, close - i + 1));
            i = close + 1;
            continue;
        }
        if (!literal.empty()) {
            parts.push_back(makeStringLiteralIr(literal));
            literal.clear();
        }
        auto expressionPart = parseExpressionIr(inside, environment);
        if (!expressionPart) {
            literal.append(value.substr(i, close - i + 1));
            i = close + 1;
            continue;
        }
        if (expression) attributeGeneratedExpression(*expressionPart, expression->sourceSpan);
        parts.push_back(std::move(expressionPart));
        sawInterpolation = true;
        i = close + 1;
    }
    if (!literal.empty()) {
        parts.push_back(makeStringLiteralIr(literal));
    }
    if (!sawInterpolation || parts.empty()) return nullptr;
    if (parts.size() == 1) return std::move(parts.front());
    auto current = std::move(parts.front());
    for (size_t i = 1; i < parts.size(); ++i) {
        current = makeStringConcatenationIr(std::move(current), std::move(parts[i]), expression);
    }
    return current;
}

ExpressionValueType mergedConditionalType(ExpressionValueType whenTrue,
                                          ExpressionValueType whenFalse) {
    if (whenTrue == whenFalse) return whenTrue;
    return ExpressionValueType::Unknown;
}

ExpressionValueType binaryResultType(const std::string& op,
                                     ExpressionValueType left,
                                     ExpressionValueType right) {
    if (op == "==" || op == "!=" || op == "<=" || op == ">=" ||
        op == "<" || op == ">" || op == "&&" || op == "||") {
        return ExpressionValueType::Boolean;
    }
    if (op == "+") {
        if (left == ExpressionValueType::String || right == ExpressionValueType::String) {
            return ExpressionValueType::String;
        }
        if (left == ExpressionValueType::Number && right == ExpressionValueType::Number) {
            return ExpressionValueType::Number;
        }
        return ExpressionValueType::Unknown;
    }
    if (op == "-" || op == "*" || op == "/" || op == "%") {
        if (left == ExpressionValueType::Number && right == ExpressionValueType::Number) {
            return ExpressionValueType::Number;
        }
        return ExpressionValueType::Unknown;
    }
    return ExpressionValueType::Unknown;
}

std::unique_ptr<ExpressionIr> makeSourceIr(const ExpressionStatementNode* expression) {
    auto ir = makeIr(ExpressionIrKind::Source, expression);
    ir->supported = false;
    return ir;
}

bool collectPathSegments(const ExpressionStatementNode* expression,
                         std::vector<std::string>& segments) {
    if (!expression) return false;
    switch (expression->getExprType()) {
        case ExpressionStatementNodeType::Variable: {
            const auto* variable = static_cast<const VariableExpressionStatementNode*>(expression);
            if (variable->name.empty()) return false;
            segments.push_back(variable->name);
            return true;
        }
        case ExpressionStatementNodeType::ObjectPropertyAccess: {
            const auto* access = static_cast<const ObjectPropertyAccessExpressionNode*>(expression);
            if (!collectPathSegments(access->base.get(), segments) ||
                access->propertyName.empty()) {
                return false;
            }
            segments.push_back(access->propertyName);
            return true;
        }
        default:
            return false;
    }
}

std::string pathSource(const std::vector<std::string>& segments) {
    std::ostringstream out;
    for (size_t i = 0; i < segments.size(); ++i) {
        if (i) out << ".";
        out << segments[i];
    }
    return out.str();
}

std::unique_ptr<ExpressionIr> makePathIr(const ExpressionStatementNode* expression,
                                         const ExpressionTypeEnvironment& environment) {
    std::vector<std::string> segments;
    if (!collectPathSegments(expression, segments) || segments.empty()) {
        return makeSourceIr(expression);
    }
    auto ir = makeIr(ExpressionIrKind::Path, expression);
    ir->segments = std::move(segments);
    ir->root = ir->segments.front();
    ir->source = pathSource(ir->segments);
    ir->valueType = typeForRoot(environment, ir->root);
    return ir;
}

void collectDependencies(const ExpressionIr& ir, std::vector<std::string>& out) {
    switch (ir.kind) {
        case ExpressionIrKind::Path:
            appendUnique(out, ir.source);
            appendUnique(out, ir.root);
            return;
        case ExpressionIrKind::Member:
        case ExpressionIrKind::Subscript:
            appendUnique(out, ir.source);
            appendUnique(out, rootDependencyName(ir.source));
            break;
        case ExpressionIrKind::Source:
            appendUnique(out, rootDependencyName(ir.source));
            break;
        default:
            break;
    }
    for (const auto& child : ir.children) {
        if (child) collectDependencies(*child, out);
    }
    for (const auto& entry : ir.entries) {
        if (entry.value) collectDependencies(*entry.value, out);
    }
}

std::string jsQuotedString(const std::string& value) {
    return nlohmann::json(value).dump();
}

std::string jsPathLookup(const std::vector<std::string>& segments) {
    if (segments.empty()) return "undefined";
    std::ostringstream out;
    out << "(function(){ let value = scope == null ? undefined : scope["
        << jsQuotedString(segments.front()) << "];";
    for (size_t i = 1; i < segments.size(); ++i) {
        out << " if (value == null) return undefined;";
        out << " value = value[" << jsQuotedString(segments[i]) << "];";
    }
    out << " return value; })()";
    return out.str();
}

std::string jsChild(const ExpressionIr& ir, size_t index) {
    if (index >= ir.children.size() || !ir.children[index]) return "undefined";
    return expressionIrToJsExpression(*ir.children[index]);
}

bool allChildrenSupportJs(const ExpressionIr& ir) {
    for (const auto& child : ir.children) {
        if (!child || !expressionIrSupportsDirectJs(*child)) return false;
    }
    for (const auto& entry : ir.entries) {
        if (!entry.value || !expressionIrSupportsDirectJs(*entry.value)) return false;
    }
    return true;
}

} // namespace

std::unique_ptr<ExpressionIr> buildExpressionIr(
        const ExpressionStatementNode* expression,
        const ExpressionTypeEnvironment& environment) {
    if (!expression) return makeIr(ExpressionIrKind::Empty, nullptr);
    switch (expression->getExprType()) {
        case ExpressionStatementNodeType::StringLiteral: {
            const auto* literal = static_cast<const StringLiteralExpressionStatementNode*>(expression);
            if (auto interpolated = buildInterpolatedStringIr(literal->value, expression, environment)) {
                return interpolated;
            }
            auto ir = makeIr(ExpressionIrKind::Literal, expression);
            ir->literal = literal->value;
            ir->valueType = ExpressionValueType::String;
            return ir;
        }
        case ExpressionStatementNodeType::NumberLiteral: {
            const auto* literal = static_cast<const NumberLiteralExpressionStatementNode*>(expression);
            auto ir = makeIr(ExpressionIrKind::Literal, expression);
            ir->literal = literal->value;
            ir->valueType = ExpressionValueType::Number;
            return ir;
        }
        case ExpressionStatementNodeType::BooleanLiteral: {
            const auto* literal = static_cast<const BooleanLiteralExpressionStatementNode*>(expression);
            auto ir = makeIr(ExpressionIrKind::Literal, expression);
            ir->literal = literal->value;
            ir->valueType = ExpressionValueType::Boolean;
            return ir;
        }
        case ExpressionStatementNodeType::Variable:
            return makePathIr(expression, environment);
        case ExpressionStatementNodeType::ObjectPropertyAccess: {
            auto path = makePathIr(expression, environment);
            if (path && path->kind == ExpressionIrKind::Path) return path;
            const auto* access = static_cast<const ObjectPropertyAccessExpressionNode*>(expression);
            auto ir = makeIr(ExpressionIrKind::Member, expression);
            ir->property = access->propertyName;
            ir->children.push_back(buildExpressionIr(access->base.get(), environment));
            return ir;
        }
        case ExpressionStatementNodeType::Subscript: {
            const auto* subscript = static_cast<const SubscriptExpressionStatementNode*>(expression);
            auto ir = makeIr(ExpressionIrKind::Subscript, expression);
            ir->children.push_back(buildExpressionIr(subscript->base.get(), environment));
            ir->children.push_back(buildExpressionIr(subscript->index.get(), environment));
            return ir;
        }
        case ExpressionStatementNodeType::Unary: {
            const auto* unary = static_cast<const UnaryExpressionStatementNode*>(expression);
            if (unary->op != "!") return makeSourceIr(expression);
            auto ir = makeIr(ExpressionIrKind::Unary, expression);
            ir->op = unary->op;
            ir->children.push_back(buildExpressionIr(unary->right.get(), environment));
            ir->valueType = ExpressionValueType::Boolean;
            return ir;
        }
        case ExpressionStatementNodeType::Binary: {
            const auto* binary = static_cast<const BinaryExpressionStatementNode*>(expression);
            static const std::set<std::string> supported = {
                "||", "&&", "==", "!=", "<=", ">=", "<", ">", "+", "-", "*", "/", "%"
            };
            if (!supported.count(binary->op)) return makeSourceIr(expression);
            auto ir = makeIr(ExpressionIrKind::Binary, expression);
            ir->op = binary->op;
            ir->children.push_back(buildExpressionIr(binary->left.get(), environment));
            ir->children.push_back(buildExpressionIr(binary->right.get(), environment));
            ir->valueType = binaryResultType(
                ir->op,
                ir->children[0] ? ir->children[0]->valueType : ExpressionValueType::Unknown,
                ir->children[1] ? ir->children[1]->valueType : ExpressionValueType::Unknown);
            return ir;
        }
        case ExpressionStatementNodeType::Conditional: {
            const auto* conditional = static_cast<const ConditionalExpressionStatementNode*>(expression);
            auto ir = makeIr(ExpressionIrKind::Conditional, expression);
            ir->children.push_back(buildExpressionIr(conditional->condition.get(), environment));
            ir->children.push_back(buildExpressionIr(conditional->whenTrue.get(), environment));
            ir->children.push_back(buildExpressionIr(conditional->whenFalse.get(), environment));
            ir->valueType = mergedConditionalType(
                ir->children[1] ? ir->children[1]->valueType : ExpressionValueType::Unknown,
                ir->children[2] ? ir->children[2]->valueType : ExpressionValueType::Unknown);
            return ir;
        }
        case ExpressionStatementNodeType::Range: {
            const auto* range = static_cast<const RangeExpressionStatementNode*>(expression);
            auto ir = makeIr(ExpressionIrKind::Range, expression);
            ir->children.push_back(buildExpressionIr(range->start.get(), environment));
            ir->children.push_back(buildExpressionIr(range->end.get(), environment));
            ir->valueType = ExpressionValueType::Array;
            return ir;
        }
        case ExpressionStatementNodeType::EmbeddedVariable: {
            const auto* embedded = static_cast<const EmbeddedVariableExpressionStatementNode*>(expression);
            return buildExpressionIr(embedded->embeddedExpression.get(), environment);
        }
        case ExpressionStatementNodeType::CompositeString: {
            const auto* composite = static_cast<const CompositeStringExpressionStatementNode*>(expression);
            if (composite->parts.empty()) {
                auto ir = makeIr(ExpressionIrKind::Literal, expression);
                ir->source = "\"\"";
                ir->literal = "";
                ir->valueType = ExpressionValueType::String;
                return ir;
            }
            auto current = buildExpressionIr(composite->parts.front().get(), environment);
            for (size_t i = 1; i < composite->parts.size(); ++i) {
                auto next = makeIr(ExpressionIrKind::Binary, expression);
                next->op = "+";
                next->children.push_back(std::move(current));
                next->children.push_back(buildExpressionIr(composite->parts[i].get(), environment));
                next->valueType = ExpressionValueType::String;
                current = std::move(next);
            }
            return current;
        }
        case ExpressionStatementNodeType::FunctionCall: {
            const auto* call = static_cast<const FunctionCallExpressionStatementNode*>(expression);
            auto ir = makeIr(ExpressionIrKind::Call, expression);
            ir->callee = call->functionName;
            ir->valueType = ExpressionValueType::Unknown;
            for (const auto& arg : call->arguments) {
                ir->children.push_back(buildExpressionIr(arg.get(), environment));
            }
            return ir;
        }
        case ExpressionStatementNodeType::ObjectMethodCall: {
            const auto* call = static_cast<const ObjectMethodCallExpressionNode*>(expression);
            auto ir = makeIr(ExpressionIrKind::MethodCall, expression);
            ir->method = call->methodName;
            ir->valueType = ExpressionValueType::Unknown;
            ir->children.push_back(buildExpressionIr(call->base.get(), environment));
            for (const auto& arg : call->arguments) {
                ir->children.push_back(buildExpressionIr(arg.get(), environment));
            }
            return ir;
        }
        case ExpressionStatementNodeType::ArrayLiteral: {
            const auto* array = static_cast<const ArrayLiteralExpressionStatementNode*>(expression);
            auto ir = makeIr(ExpressionIrKind::Array, expression);
            ir->valueType = ExpressionValueType::Array;
            for (const auto& element : array->elements) {
                ir->children.push_back(buildExpressionIr(element.get(), environment));
            }
            return ir;
        }
        case ExpressionStatementNodeType::DictionaryLiteral: {
            const auto* dict = static_cast<const DictionaryLiteralExpressionStatementNode*>(expression);
            auto ir = makeIr(ExpressionIrKind::Object, expression);
            ir->valueType = ExpressionValueType::Object;
            for (const auto& entry : dict->entries) {
                ir->entries.push_back({entry.key.text, buildExpressionIr(entry.value.get(), environment)});
            }
            return ir;
        }
    }
    return makeSourceIr(expression);
}

std::unique_ptr<ExpressionIr> buildExpressionIr(const ExpressionStatementNode* expression) {
    return buildExpressionIr(expression, ExpressionTypeEnvironment{});
}

std::unique_ptr<ExpressionIr> parseExpressionIr(const std::string& expression) {
    return parseExpressionIr(expression, ExpressionTypeEnvironment{});
}

std::unique_ptr<ExpressionIr> parseExpressionIr(
        const std::string& expression,
        const ExpressionTypeEnvironment& environment) {
    const std::string trimmed = trimCopy(expression);
    if (trimmed.empty()) return nullptr;
    Lexer lexer(trimmed);
    auto tokens = lexer.tokenize();
    if (!lexer.getErrors().empty()) return nullptr;
    Parser parser(std::move(tokens));
    try {
        auto parsed = parser.parseStandaloneExpression();
        if (!parser.getErrors().empty()) return nullptr;
        return buildExpressionIr(parsed.get(), environment);
    } catch (...) {
        return nullptr;
    }
}

std::vector<std::string> expressionIrDependencies(const ExpressionIr& ir) {
    std::vector<std::string> out;
    collectDependencies(ir, out);
    return out;
}

std::string expressionValueTypeName(ExpressionValueType type) {
    switch (type) {
        case ExpressionValueType::Unknown:
            return "unknown";
        case ExpressionValueType::Null:
            return "null";
        case ExpressionValueType::Boolean:
            return "boolean";
        case ExpressionValueType::Number:
            return "number";
        case ExpressionValueType::String:
            return "string";
        case ExpressionValueType::Array:
            return "array";
        case ExpressionValueType::Object:
            return "object";
        case ExpressionValueType::Function:
            return "function";
    }
    return "unknown";
}

nlohmann::json expressionIrToRuntimePlanJson(const ExpressionIr& ir) {
    auto base = nlohmann::json{
        {"source", ir.source},
        {"producer", "typed-ir"},
        {"valueType", expressionValueTypeName(ir.valueType)}
    };
    if (ir.sourceSpan.valid()) {
        base["sourceSpan"] = sourceSpanToJson(ir.sourceSpan);
    }
    if (expressionIrSupportsDirectJs(ir)) {
        base["directJs"] = true;
        base["jsExpression"] = expressionIrToJsExpression(ir);
    }
    if (ir.astKind >= 0) base["astKind"] = ir.astKind;
    switch (ir.kind) {
        case ExpressionIrKind::Empty:
            base["kind"] = "empty";
            return base;
        case ExpressionIrKind::Literal:
            base["kind"] = "literal";
            base["value"] = ir.literal;
            return base;
        case ExpressionIrKind::Path:
            base["kind"] = "path";
            base["root"] = ir.root;
            base["segments"] = ir.segments;
            return base;
        case ExpressionIrKind::Member:
            base["kind"] = "member";
            base["property"] = ir.property;
            base["base"] = ir.children.empty() || !ir.children[0]
                ? nlohmann::json{{"kind", "empty"}, {"source", ""}}
                : expressionIrToRuntimePlanJson(*ir.children[0]);
            return base;
        case ExpressionIrKind::Subscript:
            base["kind"] = "subscript";
            base["base"] = ir.children.size() > 0 && ir.children[0]
                ? expressionIrToRuntimePlanJson(*ir.children[0])
                : nlohmann::json{{"kind", "empty"}, {"source", ""}};
            base["index"] = ir.children.size() > 1 && ir.children[1]
                ? expressionIrToRuntimePlanJson(*ir.children[1])
                : nlohmann::json{{"kind", "empty"}, {"source", ""}};
            return base;
        case ExpressionIrKind::Unary:
            base["kind"] = "unary";
            base["operator"] = ir.op;
            base["argument"] = ir.children.empty() || !ir.children[0]
                ? nlohmann::json{{"kind", "empty"}, {"source", ""}}
                : expressionIrToRuntimePlanJson(*ir.children[0]);
            return base;
        case ExpressionIrKind::Binary:
            base["kind"] = "binary";
            base["operator"] = ir.op;
            base["left"] = ir.children.size() > 0 && ir.children[0]
                ? expressionIrToRuntimePlanJson(*ir.children[0])
                : nlohmann::json{{"kind", "empty"}, {"source", ""}};
            base["right"] = ir.children.size() > 1 && ir.children[1]
                ? expressionIrToRuntimePlanJson(*ir.children[1])
                : nlohmann::json{{"kind", "empty"}, {"source", ""}};
            return base;
        case ExpressionIrKind::Conditional:
            base["kind"] = "conditional";
            base["test"] = ir.children.size() > 0 && ir.children[0]
                ? expressionIrToRuntimePlanJson(*ir.children[0])
                : nlohmann::json{{"kind", "empty"}, {"source", ""}};
            base["whenTrue"] = ir.children.size() > 1 && ir.children[1]
                ? expressionIrToRuntimePlanJson(*ir.children[1])
                : nlohmann::json{{"kind", "empty"}, {"source", ""}};
            base["whenFalse"] = ir.children.size() > 2 && ir.children[2]
                ? expressionIrToRuntimePlanJson(*ir.children[2])
                : nlohmann::json{{"kind", "empty"}, {"source", ""}};
            return base;
        case ExpressionIrKind::Range:
            base["kind"] = "range";
            base["start"] = ir.children.size() > 0 && ir.children[0]
                ? expressionIrToRuntimePlanJson(*ir.children[0])
                : nlohmann::json{{"kind", "empty"}, {"source", ""}};
            base["end"] = ir.children.size() > 1 && ir.children[1]
                ? expressionIrToRuntimePlanJson(*ir.children[1])
                : nlohmann::json{{"kind", "empty"}, {"source", ""}};
            return base;
        case ExpressionIrKind::Array:
            base["kind"] = "array";
            base["elements"] = nlohmann::json::array();
            for (const auto& child : ir.children) {
                base["elements"].push_back(child ? expressionIrToRuntimePlanJson(*child)
                                                 : nlohmann::json{{"kind", "empty"}, {"source", ""}});
            }
            return base;
        case ExpressionIrKind::Object:
            base["kind"] = "object";
            base["entries"] = nlohmann::json::array();
            for (const auto& entry : ir.entries) {
                base["entries"].push_back({
                    {"key", entry.key},
                    {"value", entry.value ? expressionIrToRuntimePlanJson(*entry.value)
                                          : nlohmann::json{{"kind", "empty"}, {"source", ""}}}
                });
            }
            return base;
        case ExpressionIrKind::Call:
            base["kind"] = "call";
            base["callee"] = ir.callee;
            base["arguments"] = nlohmann::json::array();
            for (const auto& child : ir.children) {
                base["arguments"].push_back(child ? expressionIrToRuntimePlanJson(*child)
                                                  : nlohmann::json{{"kind", "empty"}, {"source", ""}});
            }
            return base;
        case ExpressionIrKind::MethodCall:
            base["kind"] = "method-call";
            base["method"] = ir.method;
            base["base"] = ir.children.empty() || !ir.children[0]
                ? nlohmann::json{{"kind", "empty"}, {"source", ""}}
                : expressionIrToRuntimePlanJson(*ir.children[0]);
            base["arguments"] = nlohmann::json::array();
            for (size_t i = 1; i < ir.children.size(); ++i) {
                base["arguments"].push_back(ir.children[i] ? expressionIrToRuntimePlanJson(*ir.children[i])
                                                           : nlohmann::json{{"kind", "empty"}, {"source", ""}});
            }
            return base;
        case ExpressionIrKind::Source:
            base["kind"] = "source";
            return base;
    }
    base["kind"] = "source";
    return base;
}

bool expressionIrSupportsDirectJs(const ExpressionIr& ir) {
    switch (ir.kind) {
        case ExpressionIrKind::Empty:
        case ExpressionIrKind::Literal:
        case ExpressionIrKind::Path:
            return true;
        case ExpressionIrKind::Member:
        case ExpressionIrKind::Subscript:
        case ExpressionIrKind::Unary:
        case ExpressionIrKind::Binary:
        case ExpressionIrKind::Conditional:
        case ExpressionIrKind::Range:
        case ExpressionIrKind::Array:
        case ExpressionIrKind::Object:
            return allChildrenSupportJs(ir);
        case ExpressionIrKind::Call:
        case ExpressionIrKind::MethodCall:
        case ExpressionIrKind::Source:
            return false;
    }
    return false;
}

std::string expressionIrToJsExpression(const ExpressionIr& ir) {
    switch (ir.kind) {
        case ExpressionIrKind::Empty:
            return "undefined";
        case ExpressionIrKind::Literal:
            return ir.literal.dump();
        case ExpressionIrKind::Path:
            return jsPathLookup(ir.segments);
        case ExpressionIrKind::Member:
            return "(function(){ const base = " + jsChild(ir, 0) +
                   "; return base == null ? undefined : base[" + jsQuotedString(ir.property) + "]; })()";
        case ExpressionIrKind::Subscript:
            return "(function(){ const base = " + jsChild(ir, 0) +
                   "; const index = " + jsChild(ir, 1) +
                   "; return base == null ? undefined : base[index]; })()";
        case ExpressionIrKind::Unary:
            return "(!(" + jsChild(ir, 0) + "))";
        case ExpressionIrKind::Binary:
            return "((" + jsChild(ir, 0) + ") " + ir.op + " (" + jsChild(ir, 1) + "))";
        case ExpressionIrKind::Conditional:
            return "((" + jsChild(ir, 0) + ") ? (" + jsChild(ir, 1) + ") : (" + jsChild(ir, 2) + "))";
        case ExpressionIrKind::Range:
            return "(function(){ const __start = Number(" + jsChild(ir, 0) +
                   "); const __end = Number(" + jsChild(ir, 1) +
                   "); if (!Number.isFinite(__start) || !Number.isFinite(__end)) return []; "
                   "const __step = __start <= __end ? 1 : -1; const __values = []; "
                   "for (let __value = __start; __step > 0 ? __value <= __end : __value >= __end; __value += __step) "
                   "__values.push(__value); return __values; })()";
        case ExpressionIrKind::Array: {
            std::ostringstream out;
            out << "[";
            for (size_t i = 0; i < ir.children.size(); ++i) {
                if (i) out << ",";
                out << (ir.children[i] ? expressionIrToJsExpression(*ir.children[i]) : "undefined");
            }
            out << "]";
            return out.str();
        }
        case ExpressionIrKind::Object: {
            std::ostringstream out;
            out << "({";
            for (size_t i = 0; i < ir.entries.size(); ++i) {
                if (i) out << ",";
                out << "[" << jsQuotedString(ir.entries[i].key) << "]:"
                    << (ir.entries[i].value ? expressionIrToJsExpression(*ir.entries[i].value) : "undefined");
            }
            out << "})";
            return out.str();
        }
        case ExpressionIrKind::Call:
        case ExpressionIrKind::MethodCall:
        case ExpressionIrKind::Source:
            return "undefined";
    }
    return "undefined";
}

} // namespace jtml
