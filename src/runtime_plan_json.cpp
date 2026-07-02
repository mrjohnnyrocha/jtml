#include "jtml/runtime_plan_json.h"

#include "jtml/expression_source.h"
#include "jtml/lexer.h"
#include "jtml/parser.h"

#include <cctype>
#include <cstdlib>
#include <set>
#include <sstream>

namespace jtml {
namespace {

nlohmann::json semanticPropertiesToJson(const std::vector<SemanticProperty>& properties) {
    nlohmann::json out = nlohmann::json::object();
    for (const auto& property : properties) out[property.name] = property.value;
    return out;
}

nlohmann::json moduleIdToJson(SemanticModuleId id) {
    return id == InvalidSemanticModuleId ? nlohmann::json(nullptr) : nlohmann::json(id);
}

nlohmann::json runtimePlanStatementsToJson(const std::vector<RuntimePlanStatement>& statements);

nlohmann::json runtimePlanBindingsObjectToJson(const std::vector<RuntimePlanBinding>& bindings) {
    nlohmann::json out = nlohmann::json::object();
    for (const auto& binding : bindings) out[binding.name] = binding.expr;
    return out;
}

nlohmann::json runtimePlanActionsObjectToJson(const std::vector<RuntimePlanAction>& actions) {
    nlohmann::json out = nlohmann::json::object();
    for (const auto& action : actions) {
        out[action.name] = {
            {"params", action.params},
            {"body", runtimePlanStatementsToJson(action.body)},
        };
    }
    return out;
}

nlohmann::json runtimePlanBindingsToJson(const std::vector<RuntimePlanBinding>& bindings) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& binding : bindings) {
        out.push_back({
            {"name", binding.name},
            {"expr", binding.expr},
        });
    }
    return out;
}

nlohmann::json runtimePlanStatementsToJson(const std::vector<RuntimePlanStatement>& statements) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& statement : statements) {
        nlohmann::json item = {
            {"kind", statement.kind},
        };
        if (!statement.lhs.empty()) item["lhs"] = statement.lhs;
        if (!statement.expr.empty()) item["expr"] = statement.expr;
        if (!statement.condition.empty()) item["condition"] = statement.condition;
        if (!statement.thenStatements.empty()) {
            item["then"] = runtimePlanStatementsToJson(statement.thenStatements);
        }
        if (!statement.elseStatements.empty()) {
            item["else"] = runtimePlanStatementsToJson(statement.elseStatements);
        }
        out.push_back(std::move(item));
    }
    return out;
}

nlohmann::json runtimePlanActionsToJson(const std::vector<RuntimePlanAction>& actions) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& action : actions) {
        out.push_back({
            {"name", action.name},
            {"params", action.params},
            {"body", runtimePlanStatementsToJson(action.body)},
        });
    }
    return out;
}

nlohmann::json runtimePlanRoutesToJson(const std::vector<SemanticRoute>& routes) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& route : routes) {
        out.push_back({
            {"path", route.path},
            {"name", route.component},
            {"component", route.component},
            {"params", route.params},
            {"load", route.loads},
            {"loads", route.loads},
        });
    }
    return out;
}

nlohmann::json runtimePlanFetchesToJson(const std::vector<SemanticFetch>& fetches) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& fetch : fetches) {
        out.push_back({
            {"name", fetch.name},
            {"url", fetch.url},
            {"method", fetch.method},
            {"bodyExpr", fetch.bodyExpr},
            {"refreshAction", fetch.refreshAction},
            {"cache", fetch.cache},
            {"credentials", fetch.credentials},
            {"timeoutMs", fetch.timeoutMs},
            {"retryCount", fetch.retryCount},
            {"stalePolicy", fetch.stalePolicy},
            {"group", fetch.group},
            {"cacheKeyExpr", fetch.cacheKeyExpr},
            {"revalidateMs", fetch.revalidateMs},
            {"dedupe", fetch.dedupe},
            {"background", fetch.background},
            {"lazy", fetch.lazy},
        });
    }
    return out;
}

nlohmann::json runtimePlanComponentDefinitionsToJson(
        const std::vector<RuntimePlanComponentDefinition>& definitions) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& definition : definitions) {
        out.push_back({
            {"moduleId", moduleIdToJson(definition.moduleId)},
            {"name", definition.name},
            {"params", definition.params},
            {"emits", definition.emits},
            {"emitArity", definition.emitArity},
            {"emitPayloads", definition.emitPayloads},
            {"emitPayloadTypes", definition.emitPayloadTypes},
            {"localState", definition.localState},
            {"localDerived", definition.localDerived},
            {"localActions", definition.localActions},
            {"localEffects", definition.localEffects},
            {"eventBindings", definition.eventBindings},
            {"bodySource", definition.bodySource},
            {"bodyHex", definition.bodyHex},
            {"bodyPlan", runtimePlanBodyPlanToJson(definition.bodyPlan)},
            {"hasSlot", definition.hasSlot},
            {"bodyNodeCount", definition.bodyNodeCount},
            {"rootTemplateNodeCount", definition.rootTemplateNodeCount},
            {"slotCount", definition.slotCount},
            {"sourceLine", definition.sourceLine},
        });
    }
    return out;
}

nlohmann::json runtimePlanComponentInstancesToJson(
        const std::vector<RuntimePlanComponentInstance>& instances) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& instance : instances) {
        out.push_back({
            {"moduleId", moduleIdToJson(instance.moduleId)},
            {"definitionModule", moduleIdToJson(instance.definitionModule)},
            {"id", instance.id},
            {"component", instance.component},
            {"instanceId", instance.instanceId},
            {"role", instance.role},
            {"params", semanticPropertiesToJson(instance.params)},
            {"locals", semanticPropertiesToJson(instance.locals)},
            {"slotSource", instance.slotSource},
            {"slotHex", instance.slotHex},
            {"slotPlan", runtimePlanBodyPlanToJson(instance.slotPlan)},
            {"sourceLine", instance.sourceLine},
        });
    }
    return out;
}

nlohmann::json runtimePlanComponentDefinitionsToClientJson(
        const std::vector<RuntimePlanComponentDefinition>& definitions) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& definition : definitions) {
        out.push_back({
            {"moduleId", moduleIdToJson(definition.moduleId)},
            {"name", definition.name},
            {"params", definition.params},
            {"emits", definition.emits},
            {"emitArity", definition.emitArity},
            {"emitPayloads", definition.emitPayloads},
            {"emitPayloadTypes", definition.emitPayloadTypes},
            {"localState", definition.localState},
            {"localDerived", definition.localDerived},
            {"localActions", definition.localActions},
            {"localEffects", definition.localEffects},
            {"eventBindings", definition.eventBindings},
            {"bodyPlan", runtimePlanBodyPlanToJson(definition.bodyPlan)},
            {"hasSlot", definition.hasSlot},
            {"bodyNodeCount", definition.bodyNodeCount},
            {"rootTemplateNodeCount", definition.rootTemplateNodeCount},
            {"slotCount", definition.slotCount},
        });
    }
    return out;
}

nlohmann::json runtimePlanComponentInstancesToClientJson(
        const std::vector<RuntimePlanComponentInstance>& instances) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& instance : instances) {
        out.push_back({
            {"moduleId", moduleIdToJson(instance.moduleId)},
            {"definitionModule", moduleIdToJson(instance.definitionModule)},
            {"id", instance.id},
            {"component", instance.component},
            {"instanceId", instance.instanceId},
            {"role", instance.role},
            {"params", semanticPropertiesToJson(instance.params)},
            {"locals", semanticPropertiesToJson(instance.locals)},
            {"slotPlan", runtimePlanBodyPlanToJson(instance.slotPlan)},
        });
    }
    return out;
}

nlohmann::json runtimePlanSemanticToJson(const SemanticProgram& semantic) {
    return {
        {"state", semantic.state},
        {"constants", semantic.constants},
        {"derived", semantic.derived},
        {"actions", semantic.actions},
        {"fetches", semantic.fetches},
        {"routes", semantic.routes},
        {"components", semantic.components},
        {"stores", semantic.stores},
        {"effects", semantic.effects},
        {"imports", semantic.imports},
        {"externs", semantic.externs},
        {"componentDefinitionCount", semantic.componentDefinitions.size()},
        {"componentInstanceCount", semantic.componentInstances.size()},
    };
}

std::vector<std::string> splitRuntimePlanWords(const std::string& source) {
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

std::string trimRuntimePlanString(const std::string& value) {
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

bool isQuotedRuntimePlanString(const std::string& value) {
    return value.size() >= 2 &&
           ((value.front() == '"' && value.back() == '"') ||
            (value.front() == '\'' && value.back() == '\''));
}

std::string unquoteRuntimePlanString(const std::string& value) {
    if (!isQuotedRuntimePlanString(value)) return value;
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

bool isSimpleRuntimePlanPath(const std::string& value) {
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

bool isBalancedRuntimeExpression(const std::string& value,
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

std::string stripOuterRuntimeParens(std::string expr) {
    expr = trimRuntimePlanString(expr);
    while (expr.size() >= 2 && expr.front() == '(' && expr.back() == ')' &&
           isBalancedRuntimeExpression(expr, 1, expr.size() - 1)) {
        expr = trimRuntimePlanString(expr.substr(1, expr.size() - 2));
    }
    return expr;
}

bool runtimeExpressionOperatorMayBeUnary(const std::string& expr, size_t pos) {
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

int findTopLevelRuntimeChar(const std::string& expr, char needle) {
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

int findMatchingTopLevelRuntimeColon(const std::string& expr, size_t question) {
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

int findTopLevelRuntimeOperator(const std::string& expr,
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
                runtimeExpressionOperatorMayBeUnary(expr, static_cast<size_t>(i))) {
                continue;
            }
            return i;
        }
    }
    return -1;
}

nlohmann::json astSourcePlan(const ExpressionStatementNode* expression) {
    nlohmann::json plan = {
        {"kind", "source"},
        {"source", expressionSource(expression)},
        {"producer", "ast"},
    };
    if (expression) {
        plan["astKind"] = static_cast<int>(expression->getExprType());
    }
    return plan;
}

bool collectPathSegmentsFromAst(const ExpressionStatementNode* expression,
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
            if (!collectPathSegmentsFromAst(access->base.get(), segments) ||
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

nlohmann::json compileAstPathPlan(const ExpressionStatementNode* expression) {
    std::vector<std::string> segmentVector;
    if (!collectPathSegmentsFromAst(expression, segmentVector) || segmentVector.empty()) {
        return astSourcePlan(expression);
    }
    nlohmann::json segments = nlohmann::json::array();
    std::ostringstream source;
    for (size_t i = 0; i < segmentVector.size(); ++i) {
        if (i) source << ".";
        source << segmentVector[i];
        segments.push_back(segmentVector[i]);
    }
    return {
        {"kind", "path"},
        {"source", source.str()},
        {"producer", "ast"},
        {"root", segmentVector.front()},
        {"segments", segments}
    };
}

std::unique_ptr<ExpressionStatementNode> parseRuntimeExpressionAst(const std::string& expression) {
    const auto expr = stripOuterRuntimeParens(expression);
    if (expr.empty()) return nullptr;
    Lexer lexer("show " + expr + "\\\\\n");
    auto tokens = lexer.tokenize();
    if (!lexer.getErrors().empty()) return nullptr;
    Parser parser(std::move(tokens));
    std::vector<std::unique_ptr<ASTNode>> program;
    try {
        program = parser.parseProgram();
    } catch (...) {
        return nullptr;
    }
    if (!parser.getErrors().empty() || program.size() != 1 ||
        program.front()->getType() != ASTNodeType::ShowStatement) {
        return nullptr;
    }
    auto* show = static_cast<ShowStatementNode*>(program.front().get());
    return show->expr ? show->expr->clone() : nullptr;
}

} // namespace

nlohmann::json compileRuntimeExpressionPlan(const ExpressionStatementNode* expression) {
    if (!expression) return {{"kind", "empty"}, {"source", ""}, {"producer", "ast"}};
    switch (expression->getExprType()) {
        case ExpressionStatementNodeType::StringLiteral: {
            const auto* literal = static_cast<const StringLiteralExpressionStatementNode*>(expression);
            return {
                {"kind", "literal"},
                {"source", expressionSource(expression)},
                {"producer", "ast"},
                {"value", literal->value}
            };
        }
        case ExpressionStatementNodeType::NumberLiteral: {
            const auto* literal = static_cast<const NumberLiteralExpressionStatementNode*>(expression);
            return {
                {"kind", "literal"},
                {"source", expressionSource(expression)},
                {"producer", "ast"},
                {"value", literal->value}
            };
        }
        case ExpressionStatementNodeType::BooleanLiteral: {
            const auto* literal = static_cast<const BooleanLiteralExpressionStatementNode*>(expression);
            return {
                {"kind", "literal"},
                {"source", expressionSource(expression)},
                {"producer", "ast"},
                {"value", literal->value}
            };
        }
        case ExpressionStatementNodeType::Variable:
            return compileAstPathPlan(expression);
        case ExpressionStatementNodeType::ObjectPropertyAccess: {
            auto path = compileAstPathPlan(expression);
            if (path.value("kind", std::string{}) == "path") return path;
            const auto* access = static_cast<const ObjectPropertyAccessExpressionNode*>(expression);
            auto plan = astSourcePlan(expression);
            plan["kind"] = "member";
            plan["base"] = compileRuntimeExpressionPlan(access->base.get());
            plan["property"] = access->propertyName;
            return plan;
        }
        case ExpressionStatementNodeType::Unary: {
            const auto* unary = static_cast<const UnaryExpressionStatementNode*>(expression);
            if (unary->op != "!") return astSourcePlan(expression);
            return {
                {"kind", "unary"},
                {"source", expressionSource(expression)},
                {"producer", "ast"},
                {"operator", unary->op},
                {"argument", compileRuntimeExpressionPlan(unary->right.get())}
            };
        }
        case ExpressionStatementNodeType::Binary: {
            const auto* binary = static_cast<const BinaryExpressionStatementNode*>(expression);
            static const std::set<std::string> supported = {
                "||", "&&", "==", "!=", "<=", ">=", "<", ">", "+", "-", "*", "/", "%"
            };
            if (!supported.count(binary->op)) return astSourcePlan(expression);
            return {
                {"kind", "binary"},
                {"source", expressionSource(expression)},
                {"producer", "ast"},
                {"operator", binary->op},
                {"left", compileRuntimeExpressionPlan(binary->left.get())},
                {"right", compileRuntimeExpressionPlan(binary->right.get())}
            };
        }
        case ExpressionStatementNodeType::Conditional: {
            const auto* conditional = static_cast<const ConditionalExpressionStatementNode*>(expression);
            return {
                {"kind", "conditional"},
                {"source", expressionSource(expression)},
                {"producer", "ast"},
                {"test", compileRuntimeExpressionPlan(conditional->condition.get())},
                {"whenTrue", compileRuntimeExpressionPlan(conditional->whenTrue.get())},
                {"whenFalse", compileRuntimeExpressionPlan(conditional->whenFalse.get())}
            };
        }
        case ExpressionStatementNodeType::EmbeddedVariable: {
            const auto* embedded = static_cast<const EmbeddedVariableExpressionStatementNode*>(expression);
            return compileRuntimeExpressionPlan(embedded->embeddedExpression.get());
        }
        case ExpressionStatementNodeType::CompositeString: {
            const auto* composite = static_cast<const CompositeStringExpressionStatementNode*>(expression);
            if (composite->parts.empty()) {
                return {{"kind", "literal"}, {"source", "\"\""}, {"producer", "ast"}, {"value", ""}};
            }
            nlohmann::json current = compileRuntimeExpressionPlan(composite->parts.front().get());
            for (size_t i = 1; i < composite->parts.size(); ++i) {
                current = {
                    {"kind", "binary"},
                    {"source", expressionSource(expression)},
                    {"producer", "ast"},
                    {"operator", "+"},
                    {"left", current},
                    {"right", compileRuntimeExpressionPlan(composite->parts[i].get())}
                };
            }
            return current;
        }
        case ExpressionStatementNodeType::Subscript: {
            const auto* subscript = static_cast<const SubscriptExpressionStatementNode*>(expression);
            auto plan = astSourcePlan(expression);
            plan["kind"] = "subscript";
            plan["base"] = compileRuntimeExpressionPlan(subscript->base.get());
            plan["index"] = compileRuntimeExpressionPlan(subscript->index.get());
            return plan;
        }
        case ExpressionStatementNodeType::FunctionCall: {
            const auto* call = static_cast<const FunctionCallExpressionStatementNode*>(expression);
            auto plan = astSourcePlan(expression);
            plan["kind"] = "call";
            plan["callee"] = call->functionName;
            plan["arguments"] = nlohmann::json::array();
            for (const auto& arg : call->arguments) {
                plan["arguments"].push_back(compileRuntimeExpressionPlan(arg.get()));
            }
            return plan;
        }
        case ExpressionStatementNodeType::ObjectMethodCall: {
            const auto* call = static_cast<const ObjectMethodCallExpressionNode*>(expression);
            auto plan = astSourcePlan(expression);
            plan["kind"] = "method-call";
            plan["base"] = compileRuntimeExpressionPlan(call->base.get());
            plan["method"] = call->methodName;
            plan["arguments"] = nlohmann::json::array();
            for (const auto& arg : call->arguments) {
                plan["arguments"].push_back(compileRuntimeExpressionPlan(arg.get()));
            }
            return plan;
        }
        case ExpressionStatementNodeType::ArrayLiteral: {
            const auto* array = static_cast<const ArrayLiteralExpressionStatementNode*>(expression);
            auto plan = astSourcePlan(expression);
            plan["kind"] = "array";
            plan["elements"] = nlohmann::json::array();
            for (const auto& element : array->elements) {
                plan["elements"].push_back(compileRuntimeExpressionPlan(element.get()));
            }
            return plan;
        }
        case ExpressionStatementNodeType::DictionaryLiteral: {
            const auto* dict = static_cast<const DictionaryLiteralExpressionStatementNode*>(expression);
            auto plan = astSourcePlan(expression);
            plan["kind"] = "object";
            plan["entries"] = nlohmann::json::array();
            for (const auto& entry : dict->entries) {
                plan["entries"].push_back({
                    {"key", entry.key.text},
                    {"value", compileRuntimeExpressionPlan(entry.value.get())}
                });
            }
            return plan;
        }
    }
    return astSourcePlan(expression);
}

nlohmann::json compileRuntimeExpressionPlan(const std::string& expression) {
    if (auto ast = parseRuntimeExpressionAst(expression)) {
        return compileRuntimeExpressionPlan(ast.get());
    }
    const auto expr = stripOuterRuntimeParens(expression);
    if (expr.empty()) return {{"kind", "empty"}, {"source", ""}};
    if (isQuotedRuntimePlanString(expr)) {
        return {
            {"kind", "literal"},
            {"source", expr},
            {"value", unquoteRuntimePlanString(expr)}
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
    if (isSimpleRuntimePlanPath(expr)) {
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
    const int question = findTopLevelRuntimeChar(expr, '?');
    if (question > 0) {
        const int colon = findMatchingTopLevelRuntimeColon(expr, static_cast<size_t>(question));
        if (colon > question) {
            const auto test = trimRuntimePlanString(expr.substr(0, static_cast<size_t>(question)));
            const auto truthy = trimRuntimePlanString(expr.substr(
                static_cast<size_t>(question + 1),
                static_cast<size_t>(colon - question - 1)));
            const auto falsey = trimRuntimePlanString(expr.substr(static_cast<size_t>(colon + 1)));
            if (!test.empty() && !truthy.empty() && !falsey.empty()) {
                return {
                    {"kind", "conditional"},
                    {"source", expr},
                    {"test", compileRuntimeExpressionPlan(test)},
                    {"whenTrue", compileRuntimeExpressionPlan(truthy)},
                    {"whenFalse", compileRuntimeExpressionPlan(falsey)}
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
        const int pos = findTopLevelRuntimeOperator(expr, group);
        if (pos <= 0) continue;
        std::string op;
        for (const auto& candidate : group) {
            if (expr.compare(static_cast<size_t>(pos), candidate.size(), candidate) == 0) {
                op = candidate;
                break;
            }
        }
        if (op.empty()) continue;
        const auto left = trimRuntimePlanString(expr.substr(0, static_cast<size_t>(pos)));
        const auto right = trimRuntimePlanString(expr.substr(static_cast<size_t>(pos) + op.size()));
        if (left.empty() || right.empty()) continue;
        return {
            {"kind", "binary"},
            {"source", expr},
            {"operator", op},
            {"left", compileRuntimeExpressionPlan(left)},
            {"right", compileRuntimeExpressionPlan(right)}
        };
    }
    if (expr.size() > 1 && expr.front() == '!') {
        const auto arg = trimRuntimePlanString(expr.substr(1));
        if (!arg.empty()) {
            return {
                {"kind", "unary"},
                {"source", expr},
                {"operator", "!"},
                {"argument", compileRuntimeExpressionPlan(arg)}
            };
        }
    }
    return {{"kind", "source"}, {"source", expr}};
}

namespace {

nlohmann::json compileRuntimeWordPlans(const std::string& text) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& word : splitRuntimePlanWords(text)) {
        out.push_back(compileRuntimeExpressionPlan(word));
    }
    return out;
}

nlohmann::json compileRuntimeArgumentPlans(const std::string& source) {
    nlohmann::json out = nlohmann::json::array();
    std::string current;
    int paren = 0;
    int bracket = 0;
    int brace = 0;
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
        if (ch == '(') ++paren;
        else if (ch == ')') --paren;
        else if (ch == '[') ++bracket;
        else if (ch == ']') --bracket;
        else if (ch == '{') ++brace;
        else if (ch == '}') --brace;
        if (ch == ',' && paren == 0 && bracket == 0 && brace == 0) {
            const auto expr = trimRuntimePlanString(current);
            if (!expr.empty()) out.push_back(compileRuntimeExpressionPlan(expr));
            current.clear();
            continue;
        }
        current += ch;
    }
    const auto expr = trimRuntimePlanString(current);
    if (!expr.empty()) out.push_back(compileRuntimeExpressionPlan(expr));
    return out;
}

nlohmann::json compileRuntimeLoopPlan(const RuntimePlanComponentBodyNode& node) {
    const auto raw = node.expression.empty()
        ? std::string()
        : trimRuntimePlanString(node.expression);
    const auto inPos = raw.find(" in ");
    if (inPos == std::string::npos) {
        return {
            {"item", ""},
            {"collectionExpression", raw},
            {"collectionPlan", compileRuntimeExpressionPlan(raw)}
        };
    }
    const auto item = trimRuntimePlanString(raw.substr(0, inPos));
    const auto collection = trimRuntimePlanString(raw.substr(inPos + 4));
    return {
        {"item", item},
        {"collectionExpression", collection},
        {"collectionPlan", compileRuntimeExpressionPlan(collection)}
    };
}

} // namespace

nlohmann::json runtimePlanBodyPlanToJson(
        const std::vector<RuntimePlanComponentBodyNode>& bodyPlan) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& node : bodyPlan) {
        auto item = nlohmann::json({
            {"definitionModule", moduleIdToJson(node.definitionModule)},
            {"sourceLine", node.sourceLine},
            {"sourceColumn", node.sourceColumn},
            {"indent", node.indent},
            {"parentIndex", node.parentIndex},
            {"childIndices", node.childIndices},
            {"kind", node.kind},
            {"head", node.head},
            {"name", node.name},
            {"text", node.text},
            {"operator", node.operatorToken},
            {"expression", node.expression},
            {"expressionPlan", compileRuntimeExpressionPlan(node.expression)},
            {"keyExpression", node.keyExpression},
            {"keyExpressionPlan", compileRuntimeExpressionPlan(node.keyExpression)},
            {"wordPlans", compileRuntimeWordPlans(node.text)},
            {"reads", node.reads},
            {"writes", node.writes},
            {"renderRoot", node.renderRoot},
        });
        if (node.head == "for" || node.name == "for") {
            item["loopPlan"] = compileRuntimeLoopPlan(node);
        }
        if (node.kind == "call") {
            item["argPlans"] = compileRuntimeArgumentPlans(node.expression);
        }
        out.push_back(std::move(item));
    }
    return out;
}

nlohmann::json runtimePlanToExplainJson(const RuntimePlan& plan) {
    return {
        {"sourceOfTruth", "typed AST + semantic IR"},
        {"semantic", runtimePlanSemanticToJson(plan.semantic)},
        {"state", runtimePlanBindingsToJson(plan.state)},
        {"derived", runtimePlanBindingsToJson(plan.derived)},
        {"actions", runtimePlanActionsToJson(plan.actions)},
        {"routes", runtimePlanRoutesToJson(plan.routes)},
        {"fetches", runtimePlanFetchesToJson(plan.fetches)},
        {"componentDefinitions", runtimePlanComponentDefinitionsToJson(plan.componentDefinitions)},
        {"componentInstances", runtimePlanComponentInstancesToJson(plan.componentInstances)},
    };
}

nlohmann::json runtimeProjectPlanToExplainJson(const RuntimeProjectPlan& plan) {
    nlohmann::json modules = nlohmann::json::array();
    for (const auto& module : plan.modules) {
        modules.push_back({
            {"id", moduleIdToJson(module.id)},
            {"path", module.path},
            {"astAvailable", module.astAvailable},
            {"clientExecutable", module.clientExecutable},
            {"syntax", module.syntax},
            {"plan", runtimePlanToExplainJson(module.plan)},
        });
    }

    return {
        {"sourceOfTruth", "SemanticProject retained per-file AST + semantic IR"},
        {"entry", moduleIdToJson(plan.entry)},
        {"linkedPlan", runtimePlanToExplainJson(plan.linkedPlan)},
        {"modules", modules},
    };
}

nlohmann::json runtimePlanToClientJson(const RuntimePlan& plan) {
    return {
        {"state", runtimePlanBindingsObjectToJson(plan.state)},
        {"derived", runtimePlanBindingsObjectToJson(plan.derived)},
        {"actions", runtimePlanActionsObjectToJson(plan.actions)},
        {"routes", runtimePlanRoutesToJson(plan.routes)},
        {"fetches", runtimePlanFetchesToJson(plan.fetches)},
        {"componentDefinitions",
            runtimePlanComponentDefinitionsToClientJson(plan.componentDefinitions)},
        {"componentInstances",
            runtimePlanComponentInstancesToClientJson(plan.componentInstances)},
    };
}

nlohmann::json runtimeProjectPlanToClientJson(const RuntimeProjectPlan& plan) {
    nlohmann::json client = runtimePlanToClientJson(plan.linkedPlan);
    nlohmann::json modules = nlohmann::json::array();
    for (const auto& module : plan.modules) {
        nlohmann::json item = {
            {"id", moduleIdToJson(module.id)},
            {"executable", module.clientExecutable},
        };
        if (module.clientExecutable) {
            item["plan"] = runtimePlanToClientJson(module.plan);
        }
        modules.push_back(std::move(item));
    }
    client["project"] = {
        {"sourceOfTruth", "runtime client manifest"},
        {"entry", moduleIdToJson(plan.entry)},
        {"linkedPlan", runtimePlanToClientJson(plan.linkedPlan)},
        {"modules", modules},
    };
    return client;
}

nlohmann::json runtimePlanToJson(const RuntimePlan& plan) {
    return runtimePlanToExplainJson(plan);
}

nlohmann::json runtimeProjectPlanToJson(const RuntimeProjectPlan& plan) {
    return runtimeProjectPlanToExplainJson(plan);
}

} // namespace jtml
