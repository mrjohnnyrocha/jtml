#include "jtml/runtime_plan_json.h"

#include "jtml/expression_ir.h"

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

nlohmann::json runtimePlanStatementsToJson(
    const std::vector<RuntimePlanStatement>& statements,
    const ExpressionTypeEnvironment& environment,
    const RuntimePlanJsonOptions& options);

nlohmann::json compileRuntimeExpressionPlanForOptions(
    const std::string& expression,
    const ExpressionTypeEnvironment& environment,
    const RuntimePlanJsonOptions& options);

ExpressionTypeEnvironment runtimePlanComponentTypeEnvironment(
    const ExpressionTypeEnvironment& base,
    const RuntimePlanComponentDefinition& definition);

ExpressionTypeEnvironment runtimePlanInstanceSlotTypeEnvironment(
    const ExpressionTypeEnvironment& base,
    const RuntimePlanComponentInstance& instance);

nlohmann::json runtimePlanBindingsObjectToJson(const std::vector<RuntimePlanBinding>& bindings) {
    nlohmann::json out = nlohmann::json::object();
    for (const auto& binding : bindings) out[binding.name] = binding.expr;
    return out;
}

nlohmann::json runtimePlanBindingPlansObjectToJson(
        const std::vector<RuntimePlanBinding>& bindings,
        const ExpressionTypeEnvironment& environment,
        const RuntimePlanJsonOptions& options) {
    nlohmann::json out = nlohmann::json::object();
    for (const auto& binding : bindings) {
        out[binding.name] = compileRuntimeExpressionPlanForOptions(
            binding.expr, environment, options);
    }
    return out;
}

nlohmann::json runtimePlanActionsObjectToJson(
        const std::vector<RuntimePlanAction>& actions,
        const ExpressionTypeEnvironment& environment,
        const RuntimePlanJsonOptions& options) {
    nlohmann::json out = nlohmann::json::object();
    for (const auto& action : actions) {
        auto actionEnvironment = environment;
        for (const auto& param : action.params) {
            actionEnvironment.symbols.emplace(param, ExpressionValueType::Unknown);
        }
        out[action.name] = {
            {"params", action.params},
            {"body", runtimePlanStatementsToJson(action.body, actionEnvironment, options)},
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

nlohmann::json runtimePlanStatementsToJson(
        const std::vector<RuntimePlanStatement>& statements,
        const ExpressionTypeEnvironment& environment,
        const RuntimePlanJsonOptions& options) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& statement : statements) {
        nlohmann::json item = {
            {"kind", statement.kind},
        };
        if (!statement.lhs.empty()) item["lhs"] = statement.lhs;
        if (!statement.expr.empty()) {
            item["expr"] = statement.expr;
            item["exprPlan"] = compileRuntimeExpressionPlanForOptions(
                statement.expr, environment, options);
        }
        if (!statement.condition.empty()) {
            item["condition"] = statement.condition;
            item["conditionPlan"] = compileRuntimeExpressionPlanForOptions(
                statement.condition, environment, options);
        }
        if (!statement.thenStatements.empty()) {
            item["then"] = runtimePlanStatementsToJson(
                statement.thenStatements, environment, options);
        }
        if (!statement.elseStatements.empty()) {
            item["else"] = runtimePlanStatementsToJson(
                statement.elseStatements, environment, options);
        }
        out.push_back(std::move(item));
    }
    return out;
}

nlohmann::json runtimePlanActionsToJson(
        const std::vector<RuntimePlanAction>& actions,
        const ExpressionTypeEnvironment& environment,
        const RuntimePlanJsonOptions& options) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& action : actions) {
        auto actionEnvironment = environment;
        for (const auto& param : action.params) {
            actionEnvironment.symbols.emplace(param, ExpressionValueType::Unknown);
        }
        out.push_back({
            {"name", action.name},
            {"params", action.params},
            {"body", runtimePlanStatementsToJson(action.body, actionEnvironment, options)},
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
        const std::vector<RuntimePlanComponentDefinition>& definitions,
        const ExpressionTypeEnvironment& baseEnvironment) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& definition : definitions) {
        const auto environment = runtimePlanComponentTypeEnvironment(baseEnvironment, definition);
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
            {"bodyPlan", runtimePlanBodyPlanToJson(
                definition.bodyPlan,
                RuntimePlanJsonOptions{true, &environment})},
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
        const std::vector<RuntimePlanComponentInstance>& instances,
        const ExpressionTypeEnvironment& baseEnvironment) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& instance : instances) {
        const auto environment = runtimePlanInstanceSlotTypeEnvironment(baseEnvironment, instance);
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
            {"slotPlan", runtimePlanBodyPlanToJson(
                instance.slotPlan,
                RuntimePlanJsonOptions{true, &environment})},
            {"sourceLine", instance.sourceLine},
        });
    }
    return out;
}

nlohmann::json runtimePlanComponentDefinitionsToClientJson(
        const std::vector<RuntimePlanComponentDefinition>& definitions,
        const ExpressionTypeEnvironment& baseEnvironment) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& definition : definitions) {
        const auto environment = runtimePlanComponentTypeEnvironment(baseEnvironment, definition);
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
            {"bodyPlan", runtimePlanBodyPlanToJson(
                definition.bodyPlan,
                RuntimePlanJsonOptions{false, &environment})},
            {"hasSlot", definition.hasSlot},
            {"bodyNodeCount", definition.bodyNodeCount},
            {"rootTemplateNodeCount", definition.rootTemplateNodeCount},
            {"slotCount", definition.slotCount},
        });
    }
    return out;
}

nlohmann::json runtimePlanComponentInstancesToClientJson(
        const std::vector<RuntimePlanComponentInstance>& instances,
        const ExpressionTypeEnvironment& baseEnvironment) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& instance : instances) {
        const auto environment = runtimePlanInstanceSlotTypeEnvironment(baseEnvironment, instance);
        out.push_back({
            {"moduleId", moduleIdToJson(instance.moduleId)},
            {"definitionModule", moduleIdToJson(instance.definitionModule)},
            {"id", instance.id},
            {"component", instance.component},
            {"instanceId", instance.instanceId},
            {"role", instance.role},
            {"params", semanticPropertiesToJson(instance.params)},
            {"locals", semanticPropertiesToJson(instance.locals)},
            {"slotPlan", runtimePlanBodyPlanToJson(
                instance.slotPlan,
                RuntimePlanJsonOptions{false, &environment})},
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

ExpressionValueType inferExpressionType(const std::string& expression,
                                        const ExpressionTypeEnvironment& environment) {
    if (auto ir = parseExpressionIr(expression, environment)) {
        return ir->valueType;
    }
    return ExpressionValueType::Unknown;
}

void addBindingsToTypeEnvironment(ExpressionTypeEnvironment& environment,
                                  const std::vector<RuntimePlanBinding>& bindings) {
    for (const auto& binding : bindings) {
        if (binding.name.empty()) continue;
        environment.symbols[binding.name] = inferExpressionType(binding.expr, environment);
    }
}

ExpressionTypeEnvironment runtimePlanTypeEnvironment(const RuntimePlan& plan) {
    ExpressionTypeEnvironment environment;
    for (const auto& fetch : plan.fetches) {
        if (!fetch.name.empty()) environment.symbols[fetch.name] = ExpressionValueType::Object;
    }
    addBindingsToTypeEnvironment(environment, plan.state);
    addBindingsToTypeEnvironment(environment, plan.derived);
    return environment;
}

ExpressionTypeEnvironment runtimePlanComponentTypeEnvironment(
        const ExpressionTypeEnvironment& base,
        const RuntimePlanComponentDefinition& definition) {
    auto environment = base;
    for (const auto& param : definition.params) {
        if (!param.empty()) environment.symbols.emplace(param, ExpressionValueType::Unknown);
    }
    for (const auto& node : definition.bodyPlan) {
        if ((node.kind == "state" || node.kind == "derived") && !node.name.empty()) {
            environment.symbols[node.name] = inferExpressionType(node.expression, environment);
        }
    }
    return environment;
}

ExpressionTypeEnvironment runtimePlanInstanceSlotTypeEnvironment(
        const ExpressionTypeEnvironment& base,
        const RuntimePlanComponentInstance& instance) {
    auto environment = base;
    for (const auto& property : instance.params) {
        if (!property.name.empty()) {
            environment.symbols[property.name] = inferExpressionType(property.value, environment);
        }
    }
    for (const auto& property : instance.locals) {
        if (!property.name.empty()) {
            environment.symbols[property.name] = inferExpressionType(property.value, environment);
        }
    }
    return environment;
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

} // namespace

nlohmann::json compileRuntimeExpressionPlan(const ExpressionStatementNode* expression) {
    return compileRuntimeExpressionPlan(expression, ExpressionTypeEnvironment{});
}

nlohmann::json compileRuntimeExpressionPlan(
        const ExpressionStatementNode* expression,
        const ExpressionTypeEnvironment& environment) {
    if (auto ir = buildExpressionIr(expression, environment)) {
        return expressionIrToRuntimePlanJson(*ir);
    }
    return {{"kind", "empty"}, {"source", ""}, {"producer", "typed-ir"}};
}

nlohmann::json compileRuntimeExpressionPlan(const std::string& expression) {
    return compileRuntimeExpressionPlan(expression, ExpressionTypeEnvironment{});
}

nlohmann::json compileRuntimeExpressionPlan(
        const std::string& expression,
        const ExpressionTypeEnvironment& environment) {
    const auto expr = stripOuterRuntimeParens(expression);
    if (auto ir = parseExpressionIr(expr, environment)) {
        return expressionIrToRuntimePlanJson(*ir);
    }
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
                    {"test", compileRuntimeExpressionPlan(test, environment)},
                    {"whenTrue", compileRuntimeExpressionPlan(truthy, environment)},
                    {"whenFalse", compileRuntimeExpressionPlan(falsey, environment)}
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
            {"left", compileRuntimeExpressionPlan(left, environment)},
            {"right", compileRuntimeExpressionPlan(right, environment)}
        };
    }
    if (expr.size() > 1 && expr.front() == '!') {
        const auto arg = trimRuntimePlanString(expr.substr(1));
        if (!arg.empty()) {
            return {
                {"kind", "unary"},
                {"source", expr},
                {"operator", "!"},
                {"argument", compileRuntimeExpressionPlan(arg, environment)}
            };
        }
    }
    return {{"kind", "source"}, {"source", expr}};
}

namespace {

nlohmann::json compileRuntimeWordPlans(
        const std::string& text,
        const ExpressionTypeEnvironment& environment,
        const RuntimePlanJsonOptions& options) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& word : splitRuntimePlanWords(text)) {
        out.push_back(compileRuntimeExpressionPlanForOptions(word, environment, options));
    }
    return out;
}

nlohmann::json compileRuntimeArgumentPlans(
        const std::string& source,
        const ExpressionTypeEnvironment& environment,
        const RuntimePlanJsonOptions& options) {
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
            if (!expr.empty()) {
                out.push_back(compileRuntimeExpressionPlanForOptions(expr, environment, options));
            }
            current.clear();
            continue;
        }
        current += ch;
    }
    const auto expr = trimRuntimePlanString(current);
    if (!expr.empty()) {
        out.push_back(compileRuntimeExpressionPlanForOptions(expr, environment, options));
    }
    return out;
}

nlohmann::json compileRuntimeLoopPlan(
        const RuntimePlanComponentBodyNode& node,
        const ExpressionTypeEnvironment& environment,
        const RuntimePlanJsonOptions& options) {
    const auto raw = node.expression.empty()
        ? std::string()
        : trimRuntimePlanString(node.expression);
    const auto inPos = raw.find(" in ");
    if (inPos == std::string::npos) {
        return {
            {"item", ""},
            {"collectionExpression", raw},
            {"collectionPlan", compileRuntimeExpressionPlanForOptions(raw, environment, options)}
        };
    }
    const auto item = trimRuntimePlanString(raw.substr(0, inPos));
    const auto collection = trimRuntimePlanString(raw.substr(inPos + 4));
    return {
        {"item", item},
        {"collectionExpression", collection},
        {"collectionPlan", compileRuntimeExpressionPlanForOptions(
            collection, environment, options)}
    };
}

bool isRuntimeLoopNode(const RuntimePlanComponentBodyNode& node) {
    return node.head == "for" || node.name == "for";
}

std::pair<std::string, std::string> runtimeLoopItemAndCollection(
        const RuntimePlanComponentBodyNode& node) {
    if (!isRuntimeLoopNode(node)) return {"", ""};
    const auto raw = node.expression.empty()
        ? std::string()
        : trimRuntimePlanString(node.expression);
    const auto inPos = raw.find(" in ");
    if (inPos == std::string::npos) return {"", raw};
    return {
        trimRuntimePlanString(raw.substr(0, inPos)),
        trimRuntimePlanString(raw.substr(inPos + 4))
    };
}

ExpressionValueType loopItemTypeForCollection(const std::string& collection,
                                              const ExpressionTypeEnvironment& environment) {
    if (collection.empty()) return ExpressionValueType::Unknown;
    if (auto collectionPlan = parseExpressionIr(collection, environment)) {
        if (collectionPlan->kind == ExpressionIrKind::Range) {
            return ExpressionValueType::Number;
        }
    }
    return ExpressionValueType::Unknown;
}

void addLoopLocalToEnvironment(ExpressionTypeEnvironment& environment,
                               const RuntimePlanComponentBodyNode& node) {
    const auto [item, collection] = runtimeLoopItemAndCollection(node);
    if (item.empty()) return;
    environment.symbols[item] = loopItemTypeForCollection(collection, environment);
}

ExpressionTypeEnvironment environmentForBodyNode(
        const std::vector<RuntimePlanComponentBodyNode>& bodyPlan,
        int nodeIndex,
        const ExpressionTypeEnvironment& baseEnvironment) {
    auto environment = baseEnvironment;
    std::vector<int> chain;
    int cursor = nodeIndex;
    while (cursor >= 0 && static_cast<size_t>(cursor) < bodyPlan.size()) {
        chain.push_back(cursor);
        const int parent = bodyPlan[static_cast<size_t>(cursor)].parentIndex;
        if (parent == cursor) break;
        cursor = parent;
    }
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        addLoopLocalToEnvironment(environment, bodyPlan[static_cast<size_t>(*it)]);
    }
    return environment;
}

void stripExpressionCompilerHints(nlohmann::json& value) {
    if (value.is_object()) {
        value.erase("directJs");
        value.erase("jsExpression");
        value.erase("sourceSpan");
        for (auto& item : value.items()) {
            stripExpressionCompilerHints(item.value());
        }
    } else if (value.is_array()) {
        for (auto& item : value) {
            stripExpressionCompilerHints(item);
        }
    }
}

nlohmann::json compileRuntimeExpressionPlanForOptions(
        const std::string& expression,
        const ExpressionTypeEnvironment& environment,
        const RuntimePlanJsonOptions& options) {
    auto plan = compileRuntimeExpressionPlan(expression, environment);
    if (!options.includeExpressionCompilerHints) {
        stripExpressionCompilerHints(plan);
    }
    return plan;
}

} // namespace

nlohmann::json runtimePlanBodyPlanToJson(
        const std::vector<RuntimePlanComponentBodyNode>& bodyPlan,
        const RuntimePlanJsonOptions& options) {
    const ExpressionTypeEnvironment emptyEnvironment;
    const auto& environment = options.typeEnvironment
        ? *options.typeEnvironment
        : emptyEnvironment;
    nlohmann::json out = nlohmann::json::array();
    for (size_t nodeIndex = 0; nodeIndex < bodyPlan.size(); ++nodeIndex) {
        const auto& node = bodyPlan[nodeIndex];
        const auto nodeEnvironment = environmentForBodyNode(
            bodyPlan,
            static_cast<int>(nodeIndex),
            environment);
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
            {"expressionPlan", compileRuntimeExpressionPlanForOptions(
                node.expression, nodeEnvironment, options)},
            {"keyExpression", node.keyExpression},
            {"keyExpressionPlan", compileRuntimeExpressionPlanForOptions(
                node.keyExpression, nodeEnvironment, options)},
            {"wordPlans", compileRuntimeWordPlans(node.text, nodeEnvironment, options)},
            {"reads", node.reads},
            {"writes", node.writes},
            {"renderRoot", node.renderRoot},
        });
        if (node.head == "for" || node.name == "for") {
            item["loopPlan"] = compileRuntimeLoopPlan(node, environment, options);
        }
        if (node.kind == "call") {
            item["argPlans"] = compileRuntimeArgumentPlans(
                node.expression, nodeEnvironment, options);
        }
        out.push_back(std::move(item));
    }
    return out;
}

nlohmann::json runtimePlanToExplainJson(const RuntimePlan& plan) {
    const auto environment = runtimePlanTypeEnvironment(plan);
    return {
        {"sourceOfTruth", "typed AST + semantic IR"},
        {"semantic", runtimePlanSemanticToJson(plan.semantic)},
        {"state", runtimePlanBindingsToJson(plan.state)},
        {"derived", runtimePlanBindingsToJson(plan.derived)},
        {"actions", runtimePlanActionsToJson(
            plan.actions, environment, RuntimePlanJsonOptions{true, &environment})},
        {"routes", runtimePlanRoutesToJson(plan.routes)},
        {"fetches", runtimePlanFetchesToJson(plan.fetches)},
        {"componentDefinitions",
            runtimePlanComponentDefinitionsToJson(plan.componentDefinitions, environment)},
        {"componentInstances",
            runtimePlanComponentInstancesToJson(plan.componentInstances, environment)},
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
    const auto environment = runtimePlanTypeEnvironment(plan);
    auto client = nlohmann::json({
        {"state", runtimePlanBindingsObjectToJson(plan.state)},
        {"statePlans", runtimePlanBindingPlansObjectToJson(
            plan.state, environment, RuntimePlanJsonOptions{false, &environment})},
        {"derived", runtimePlanBindingsObjectToJson(plan.derived)},
        {"derivedPlans", runtimePlanBindingPlansObjectToJson(
            plan.derived, environment, RuntimePlanJsonOptions{false, &environment})},
        {"actions", runtimePlanActionsObjectToJson(
            plan.actions, environment, RuntimePlanJsonOptions{false, &environment})},
        {"routes", runtimePlanRoutesToJson(plan.routes)},
        {"fetches", runtimePlanFetchesToJson(plan.fetches)},
        {"componentDefinitions",
            runtimePlanComponentDefinitionsToClientJson(plan.componentDefinitions, environment)},
        {"componentInstances",
            runtimePlanComponentInstancesToClientJson(plan.componentInstances, environment)},
    });
    stripExpressionCompilerHints(client);
    return client;
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
