#include "jtml/runtime_plan.h"

#include "jtml/expression_ir.h"
#include "jtml/friendly.h"
#include "jtml/expression_source.h"
#include "jtml/lexer.h"
#include "jtml/parser.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <set>
#include <sstream>
#include <tuple>

namespace jtml {
namespace {

std::vector<RuntimePlanStatement> collectRuntimeStatements(
        const std::vector<std::unique_ptr<ASTNode>>& nodes) {
    std::vector<RuntimePlanStatement> statements;
    for (const auto& node : nodes) {
        if (!node) continue;
        if (node->getType() == ASTNodeType::AssignmentStatement) {
            const auto& stmt = static_cast<const AssignmentStatementNode&>(*node);
            RuntimePlanStatement planned;
            planned.kind = "assign";
            planned.lhs = expressionSource(stmt.lhs.get());
            planned.expr = expressionSource(stmt.rhs.get());
            statements.push_back(std::move(planned));
        } else if (node->getType() == ASTNodeType::DefineStatement) {
            const auto& stmt = static_cast<const DefineStatementNode&>(*node);
            RuntimePlanStatement planned;
            planned.kind = "assign";
            planned.lhs = stmt.identifier;
            planned.expr = expressionSource(stmt.expression.get());
            statements.push_back(std::move(planned));
        } else if (node->getType() == ASTNodeType::ShowStatement) {
            RuntimePlanStatement planned;
            planned.kind = "show";
            planned.expr = expressionSource(static_cast<const ShowStatementNode&>(*node).expr.get());
            statements.push_back(std::move(planned));
        } else if (node->getType() == ASTNodeType::IfStatement) {
            const auto& stmt = static_cast<const IfStatementNode&>(*node);
            RuntimePlanStatement planned;
            planned.kind = "if";
            planned.condition = expressionSource(stmt.condition.get());
            planned.thenStatements = collectRuntimeStatements(stmt.thenStatements);
            planned.elseStatements = collectRuntimeStatements(stmt.elseStatements);
            statements.push_back(std::move(planned));
        }
    }
    return statements;
}

std::string effectLocation(const SourceSpan& span) {
    if (span.original) {
        const auto& origin = *span.original;
        const auto [line, column] = origin.source->coordinates(origin.begin);
        return origin.source->file + ":" + std::to_string(line) + ":" + std::to_string(column);
    }
    return "lowered line " + std::to_string(span.startLine);
}

bool scalarEffectType(ExpressionValueType type) {
    return type == ExpressionValueType::Number || type == ExpressionValueType::String ||
           type == ExpressionValueType::Boolean;
}

// Stable topological order permits one finite browser pass, never recursive
// action dispatch or render-to-convergence. Ties retain subscription source order.
void orderRootSubscriptions(RuntimePlan& plan) {
    const auto count = plan.subscriptions.size();
    if (count > 64) {
        plan.effectIssues.push_back("effect at " + effectLocation(plan.subscriptions[64].sourceSpan) +
            ": at most 64 root subscriptions are supported; split the application");
        return;
    }
    std::vector<size_t> incoming(count, 0);
    std::vector<std::vector<size_t>> edges(count);
    for (size_t i = 0; i < count; ++i) for (size_t j = 0; j < count; ++j) {
        const auto& writes = plan.subscriptions[i].writes;
        if (std::find(writes.begin(), writes.end(), plan.subscriptions[j].dependency) != writes.end()) {
            edges[i].push_back(j);
            ++incoming[j];
        }
    }
    std::vector<RuntimePlanSubscription> ordered;
    std::vector<bool> used(count, false);
    for (size_t step = 0; step < count; ++step) {
        size_t next = 0;
        while (next < count && (used[next] || incoming[next])) ++next;
        if (next == count) {
            size_t pending = 0;
            while (pending < count && used[pending]) ++pending;
            plan.effectIssues.push_back("effect at " + effectLocation(plan.subscriptions[pending].sourceSpan) +
                ": cyclic root effect writes involving `" + plan.subscriptions[pending].dependency +
                "` are unsupported; remove the dependency/write cycle");
            return;
        }
        used[next] = true;
        ordered.push_back(plan.subscriptions[next]);
        for (const auto target : edges[next]) --incoming[target];
    }
    plan.subscriptions = std::move(ordered);
}

void planRootSubscriptions(const std::vector<std::unique_ptr<ASTNode>>& program,
                           RuntimePlan& plan) {
    ExpressionTypeEnvironment types;
    std::set<std::string> mutableRoots;
    std::map<std::string, size_t> declarations;
    std::map<std::string, const FunctionDeclarationNode*> functions;
    std::set<std::string> duplicateFunctions;
    for (const auto& node : program) {
        if (!node) continue;
        if (node->getType() == ASTNodeType::DefineStatement) {
            const auto& state = static_cast<const DefineStatementNode&>(*node);
            const auto expression = buildExpressionIr(state.expression.get(), types);
            types.symbols[state.identifier] = expression ? expression->valueType : ExpressionValueType::Unknown;
            ++declarations[state.identifier];
            if (!state.isConst) mutableRoots.insert(state.identifier);
            else mutableRoots.erase(state.identifier);
        } else if (node->getType() == ASTNodeType::DeriveStatement) {
            const auto& derived = static_cast<const DeriveStatementNode&>(*node);
            ++declarations[derived.identifier];
            mutableRoots.erase(derived.identifier);
            const auto expression = buildExpressionIr(derived.expression.get(), types);
            types.symbols[derived.identifier] = expression ? expression->valueType : ExpressionValueType::Unknown;
        } else if (node->getType() == ASTNodeType::FunctionDeclaration) {
            const auto& action = static_cast<const FunctionDeclarationNode&>(*node);
            if (!functions.emplace(action.name, &action).second) duplicateFunctions.insert(action.name);
        }
    }
    for (const auto& component : plan.semantic.componentDefinitions) {
        if (component.localEffects.empty()) continue;
        const auto span = component.provenance ? component.provenance->inputConstruct(component.sourceLine)
                                               : std::optional<OriginalRange>{};
        std::string location = "lowered line " + std::to_string(component.sourceLine);
        if (span) {
            const auto [line, column] = span->source->coordinates(span->begin);
            location = span->source->file + ":" + std::to_string(line) + ":" + std::to_string(column);
        }
        plan.effectIssues.push_back("component " + component.name + " at " + location +
            ": component-local effects are unsupported in clean production; use a module-owned scalar let/effect");
    }
    for (const auto& span : plan.semantic.unsubscriptions)
        plan.effectIssues.push_back("effect at " + effectLocation(span) +
            ": dynamic unsubscribe is unsupported in clean production; guard effect writes with an owned scalar flag instead");
    for (const auto& subscription : plan.semantic.subscriptions) {
        auto issue = [&](const std::string& reason) {
            plan.effectIssues.push_back("effect at " + effectLocation(subscription.sourceSpan) + ": " + reason);
        };
        if (!subscription.rootOwned) {
            issue("nested subscriptions are unsupported; declare the effect at module root");
            continue;
        }
        if (declarations[subscription.dependency] != 1) {
            issue("dependency must have one module-root mutable scalar let declaration; put subsequent writes in a when action");
            continue;
        }
        const auto dependency = types.symbols.find(subscription.dependency);
        if (!mutableRoots.count(subscription.dependency) || dependency == types.symbols.end() ||
            !scalarEffectType(dependency->second)) {
            issue("dependency `" + subscription.dependency +
                "` must be a module-owned mutable scalar let; derived, imported, collection and parameter watches are unsupported");
            continue;
        }
        const auto action = functions.find(subscription.action);
        if (duplicateFunctions.count(subscription.action)) {
            issue("ambiguous callback declaration; use a distinct zero-argument root action");
            continue;
        }
        if (action == functions.end() || !action->second->parameters.empty()) {
            issue("callback must be a module-root zero-argument action");
            continue;
        }
        RuntimePlanSubscription record;
        record.action = subscription.action;
        record.dependency = subscription.dependency;
        record.sourceSpan = subscription.sourceSpan;
        size_t statements = 0;
        auto expressionSupported = [&](const ExpressionStatementNode* expression) {
            const auto ir = buildExpressionIr(expression, types);
            return ir && expressionIrSupportsDirectJs(*ir) && scalarEffectType(ir->valueType);
        };
        std::function<void(const std::vector<std::unique_ptr<ASTNode>>&)> validateBody;
        validateBody = [&](const auto& nodes) {
            for (const auto& node : nodes) {
                if (!node) continue;
                if (++statements > 1024) { issue("effect body exceeds 1024 statements; simplify the effect"); return; }
                std::string target;
                const ExpressionStatementNode* rhs = nullptr;
                if (node->getType() == ASTNodeType::AssignmentStatement) {
                    const auto& assignment = static_cast<const AssignmentStatementNode&>(*node);
                    target = expressionSource(assignment.lhs.get());
                    rhs = assignment.rhs.get();
                } else if (node->getType() == ASTNodeType::DefineStatement) {
                    const auto& assignment = static_cast<const DefineStatementNode&>(*node);
                    if (assignment.isConst) issue("constant declarations in effect bodies are unsupported; assign an existing mutable scalar let");
                    target = assignment.identifier;
                    rhs = assignment.expression.get();
                } else if (node->getType() == ASTNodeType::ShowStatement) {
                    if (!expressionSupported(static_cast<const ShowStatementNode&>(*node).expr.get()))
                        issue("unsupported show expression; use a statically analyzable scalar expression");
                    continue;
                } else if (node->getType() == ASTNodeType::IfStatement) {
                    const auto& branch = static_cast<const IfStatementNode&>(*node);
                    if (!expressionSupported(branch.condition.get())) issue("unsupported condition; use a statically analyzable scalar condition");
                    validateBody(branch.thenStatements);
                    validateBody(branch.elseStatements);
                    continue;
                } else {
                    issue("unsupported effect statement at " + effectLocation(node->sourceSpan) +
                        "; use only owned scalar assignments, scalar show and if/else; calls, loops and async work are unsupported");
                    continue;
                }
                if (!mutableRoots.count(target) || !scalarEffectType(types.symbols[target]) || !expressionSupported(rhs)) {
                    issue("unsupported effect write `" + target + "` at " + effectLocation(node->sourceSpan) +
                        "; assign an existing module-owned scalar let using a statically analyzable scalar expression");
                }
                if (std::find(record.writes.begin(), record.writes.end(), target) == record.writes.end()) record.writes.push_back(target);
            }
        };
        validateBody(action->second->body);
        const auto duplicate = std::find_if(plan.subscriptions.begin(), plan.subscriptions.end(), [&](const auto& item) {
            return item.action == record.action && item.dependency == record.dependency;
        });
        if (duplicate == plan.subscriptions.end()) plan.subscriptions.push_back(std::move(record));
    }
    // Scalar watch ownership is checked at every statically visible writer.
    // Unknown argument values retain a runtime scalar guard; known collection
    // writes and member writes cannot satisfy the scalar dependency contract.
    std::set<std::string> dependencies;
    for (const auto& subscription : plan.subscriptions) dependencies.insert(subscription.dependency);
    for (const auto& entry : functions) {
        const auto* action = entry.second;
        auto actionTypes = types;
        for (const auto& param : action->parameters) actionTypes.symbols[param.name] = ExpressionValueType::Unknown;
        std::function<void(const std::vector<std::unique_ptr<ASTNode>>&)> inspect;
        inspect = [&](const auto& nodes) {
            for (const auto& node : nodes) {
                if (!node) continue;
                if (node->getType() == ASTNodeType::IfStatement) {
                    const auto& branch = static_cast<const IfStatementNode&>(*node);
                    inspect(branch.thenStatements); inspect(branch.elseStatements); continue;
                }
                std::string target;
                const ExpressionStatementNode* rhs = nullptr;
                if (node->getType() == ASTNodeType::AssignmentStatement) {
                    const auto& assignment = static_cast<const AssignmentStatementNode&>(*node);
                    target = expressionSource(assignment.lhs.get()); rhs = assignment.rhs.get();
                } else if (node->getType() == ASTNodeType::DefineStatement) {
                    const auto& assignment = static_cast<const DefineStatementNode&>(*node);
                    target = assignment.identifier; rhs = assignment.expression.get();
                } else continue;
                const auto root = target.substr(0, target.find_first_of(".["));
                if (!dependencies.count(root) || std::any_of(action->parameters.begin(), action->parameters.end(),
                    [&](const auto& param) { return param.name == root; })) continue;
                const auto ir = buildExpressionIr(rhs, actionTypes);
                if (root != target || (ir && ir->valueType != ExpressionValueType::Unknown && !scalarEffectType(ir->valueType)))
                    plan.effectIssues.push_back("effect dependency writer at " + effectLocation(node->sourceSpan) +
                        ": `" + root + "` requires a scalar assignment to its root; collections and member writes are unsupported");
            }
        };
        inspect(action->body);
    }
    if (!dependencies.empty()) for (const auto& node : program) {
        if (node && (node->getType() == ASTNodeType::AssignmentStatement || node->getType() == ASTNodeType::ExpressionStatement))
            plan.effectIssues.push_back("effect program at " + effectLocation(node->sourceSpan) +
                ": imperative module statements are unsupported in clean production; move writes/calls into a when action");
    }
    orderRootSubscriptions(plan);
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

std::string hexDecode(const std::string& hex) {
    std::string out;
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        const int hi = hexValue(hex[i]);
        const int lo = hexValue(hex[i + 1]);
        if (hi < 0 || lo < 0) continue;
        out.push_back(static_cast<char>((hi << 4) | lo));
    }
    return out;
}

std::string trimCopy(const std::string& value) {
    size_t start = 0;
    while (start < value.size() &&
           std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }
    size_t end = value.size();
    while (end > start &&
           std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(start, end - start);
}

std::vector<std::string> words(const std::string& value) {
    std::vector<std::string> out;
    std::string current;
    char quote = '\0';
    int depth = 0;
    for (size_t i = 0; i < value.size(); ++i) {
        const char ch = value[i];
        if (quote != '\0') {
            current.push_back(ch);
            if (ch == '\\' && i + 1 < value.size()) {
                current.push_back(value[++i]);
            } else if (ch == quote) {
                quote = '\0';
            }
            continue;
        }
        if (ch == '"' || ch == '\'') {
            quote = ch;
            current.push_back(ch);
            continue;
        }
        if (ch == '(' || ch == '[' || ch == '{') ++depth;
        if ((ch == ')' || ch == ']' || ch == '}') && depth > 0) --depth;
        if (std::isspace(static_cast<unsigned char>(ch)) && depth == 0) {
            if (!current.empty()) {
                out.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty()) out.push_back(current);
    return out;
}

std::string cleanComponentNameToken(std::string token) {
    while (!token.empty() &&
           (token.back() == ':' || token.back() == ',' ||
            token.back() == ')' || token.back() == '(')) {
        token.pop_back();
    }
    const auto colon = token.find(':');
    if (colon != std::string::npos) token = token.substr(0, colon);
    return token;
}

std::string rootDependencyName(const std::string& name) {
    const auto dot = name.find('.');
    const auto subscript = name.find('[');
    size_t end = std::string::npos;
    if (dot != std::string::npos) end = dot;
    if (subscript != std::string::npos) {
        end = end == std::string::npos ? subscript : std::min(end, subscript);
    }
    const std::string root = end == std::string::npos ? name : name.substr(0, end);
    return cleanComponentNameToken(root);
}

int parseBodyIndent(const std::string& encodedLine) {
    const auto colon = encodedLine.find(':');
    if (colon == std::string::npos) return 0;
    const auto secondColon = encodedLine.find(':', colon + 1);
    const auto thirdColon = secondColon == std::string::npos
        ? std::string::npos
        : encodedLine.find(':', secondColon + 1);
    const auto start = thirdColon != std::string::npos
        ? secondColon + 1
        : (secondColon == std::string::npos ? 0 : colon + 1);
    const auto count = thirdColon != std::string::npos
        ? thirdColon - secondColon - 1
        : (secondColon == std::string::npos ? colon : secondColon - colon - 1);
    try {
        return std::stoi(encodedLine.substr(start, count));
    } catch (...) {
        return 0;
    }
}

int parseBodySourceLine(const std::string& encodedLine) {
    const auto colon = encodedLine.find(':');
    if (colon == std::string::npos) return 0;
    const auto secondColon = encodedLine.find(':', colon + 1);
    if (secondColon == std::string::npos) return 0;
    try {
        return std::stoi(encodedLine.substr(0, colon));
    } catch (...) {
        return 0;
    }
}

int parseBodySourceColumn(const std::string& encodedLine) {
    const auto colon = encodedLine.find(':');
    if (colon == std::string::npos) return 0;
    const auto secondColon = encodedLine.find(':', colon + 1);
    if (secondColon == std::string::npos) return 0;
    const auto thirdColon = encodedLine.find(':', secondColon + 1);
    if (thirdColon == std::string::npos) return 0;
    try {
        return std::stoi(encodedLine.substr(colon + 1, secondColon - colon - 1));
    } catch (...) {
        return 0;
    }
}

std::string parseBodyText(const std::string& encodedLine) {
    const auto colon = encodedLine.find(':');
    if (colon == std::string::npos) return trimCopy(encodedLine);
    const auto secondColon = encodedLine.find(':', colon + 1);
    if (secondColon != std::string::npos) {
        const auto thirdColon = encodedLine.find(':', secondColon + 1);
        if (thirdColon != std::string::npos) {
            return trimCopy(encodedLine.substr(thirdColon + 1));
        }
        return trimCopy(encodedLine.substr(secondColon + 1));
    }
    return trimCopy(encodedLine.substr(colon + 1));
}

std::string componentBodyNodeKind(const std::string& head) {
    if (head == "let" || head == "const") return "state";
    if (head == "get") return "derived";
    if (head == "when") return "action";
    if (head == "effect") return "effect";
    if (head == "slot") return "slot";
    return "template";
}

bool isCallBodyLine(const std::vector<std::string>& tokens) {
    if (tokens.size() != 1) return false;
    const auto& token = tokens[0];
    const auto open = token.find('(');
    return open != std::string::npos && open > 0 && token.back() == ')';
}

bool isAssignmentBodyLine(const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) return false;
    return tokens[1] == "=" || tokens[1] == "+=" || tokens[1] == "-=" ||
           tokens[1] == "*=" || tokens[1] == "/=" || tokens[1] == "%=";
}

bool parsePostfixAssignmentToken(const std::string& token,
                                 std::string& target,
                                 std::string& op) {
    if (token.size() <= 2) return false;
    if (token.substr(token.size() - 2) == "++") {
        target = token.substr(0, token.size() - 2);
        op = "+=";
    } else if (token.substr(token.size() - 2) == "--") {
        target = token.substr(0, token.size() - 2);
        op = "-=";
    } else {
        return false;
    }
    return !target.empty() && target.find_first_of("\"'") == std::string::npos;
}

bool isPostfixAssignmentBodyLine(const std::vector<std::string>& tokens) {
    if (tokens.size() != 1) return false;
    std::string target;
    std::string op;
    return parsePostfixAssignmentToken(tokens[0], target, op);
}

std::string joinTokens(const std::vector<std::string>& tokens, size_t start) {
    std::ostringstream out;
    for (size_t i = start; i < tokens.size(); ++i) {
        if (i > start) out << ' ';
        out << tokens[i];
    }
    return out.str();
}

std::string joinTokensUntil(const std::vector<std::string>& tokens, size_t start, size_t end) {
    std::ostringstream out;
    for (size_t i = start; i < end && i < tokens.size(); ++i) {
        if (i > start) out << ' ';
        out << tokens[i];
    }
    return out.str();
}

bool isIdentifierToken(const std::string& token) {
    if (token.empty()) return false;
    if (!(std::isalpha(static_cast<unsigned char>(token.front())) || token.front() == '_')) {
        return false;
    }
    for (const char ch : token) {
        if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '.')) {
            return false;
        }
    }
    return true;
}

bool isQuotedToken(const std::string& token) {
    return token.size() >= 2 &&
           ((token.front() == '"' && token.back() == '"') ||
            (token.front() == '\'' && token.back() == '\''));
}

bool isBodyPlanAttributeName(const std::string& token) {
    static const std::set<std::string> names = {
        "id", "class", "style", "role", "href", "src", "alt", "title",
        "type", "name", "value", "placeholder", "for", "rel", "target",
        "method", "action", "enctype", "autocomplete", "inputmode",
        "pattern", "accept", "poster", "preload", "kind", "srclang",
        "label", "width", "height", "min", "max", "step", "minlength",
        "maxlength", "rows", "cols", "cx", "cy", "r", "x", "y", "x1",
        "y1", "x2", "y2", "d", "points", "viewBox", "fill", "stroke",
        "stroke-width", "transform", "opacity", "to", "active-class",
    };
    return names.count(token) > 0 ||
           token.rfind("aria-", 0) == 0 ||
           token.rfind("data-", 0) == 0;
}

bool isBodyPlanBooleanAttributeName(const std::string& token) {
    static const std::set<std::string> names = {
        "disabled", "required", "checked", "selected", "multiple",
        "readonly", "autofocus", "playsinline", "open", "hidden",
        "controls", "autoplay", "loop", "muted",
    };
    return names.count(token) > 0;
}

bool bodyPlanAttributeTakesValue(const std::string& token) {
    return isBodyPlanAttributeName(token) &&
           !isBodyPlanBooleanAttributeName(token);
}

bool isBodyPlanSemanticModifierName(const std::string& token) {
    static const std::set<std::string> names = {
        "cols", "gap", "pad", "radius", "shadow", "tone",
        "align", "justify", "width", "surface",
    };
    return names.count(token) > 0;
}

bool isBodyPlanEventName(const std::string& token) {
    static const std::set<std::string> names = {
        "click", "input", "change", "submit", "hover", "scroll",
        "focus", "blur", "keydown", "keyup",
    };
    return names.count(token) > 0;
}

void appendUnique(std::vector<std::string>& target, const std::string& value);
void appendUnique(std::vector<std::string>& target, const std::vector<std::string>& values);

std::string expressionDependencyPath(const ExpressionStatementNode* expr) {
    if (!expr) return "";
    switch (expr->getExprType()) {
    case ExpressionStatementNodeType::Variable: {
        const auto& node = static_cast<const VariableExpressionStatementNode&>(*expr);
        return node.name;
    }
    case ExpressionStatementNodeType::ObjectPropertyAccess: {
        const auto& node = static_cast<const ObjectPropertyAccessExpressionNode&>(*expr);
        const std::string base = expressionDependencyPath(node.base.get());
        if (base.empty() || node.propertyName.empty()) return "";
        return base + "." + node.propertyName;
    }
    case ExpressionStatementNodeType::Subscript: {
        const auto& node = static_cast<const SubscriptExpressionStatementNode&>(*expr);
        const std::string base = expressionDependencyPath(node.base.get());
        if (base.empty()) return "";
        return base + "[" + expressionSource(node.index.get()) + "]";
    }
    default:
        return "";
    }
}

void collectExpressionDependencies(
        const ExpressionStatementNode* expr,
        std::vector<std::string>& out) {
    if (!expr) return;
    switch (expr->getExprType()) {
    case ExpressionStatementNodeType::Variable: {
        const auto& node = static_cast<const VariableExpressionStatementNode&>(*expr);
        appendUnique(out, node.name);
        return;
    }
    case ExpressionStatementNodeType::Binary: {
        const auto& node = static_cast<const BinaryExpressionStatementNode&>(*expr);
        collectExpressionDependencies(node.left.get(), out);
        collectExpressionDependencies(node.right.get(), out);
        return;
    }
    case ExpressionStatementNodeType::Unary: {
        const auto& node = static_cast<const UnaryExpressionStatementNode&>(*expr);
        collectExpressionDependencies(node.right.get(), out);
        return;
    }
    case ExpressionStatementNodeType::EmbeddedVariable: {
        const auto& node = static_cast<const EmbeddedVariableExpressionStatementNode&>(*expr);
        collectExpressionDependencies(node.embeddedExpression.get(), out);
        return;
    }
    case ExpressionStatementNodeType::CompositeString: {
        const auto& node = static_cast<const CompositeStringExpressionStatementNode&>(*expr);
        for (const auto& part : node.parts) collectExpressionDependencies(part.get(), out);
        return;
    }
    case ExpressionStatementNodeType::ArrayLiteral: {
        const auto& node = static_cast<const ArrayLiteralExpressionStatementNode&>(*expr);
        for (const auto& item : node.elements) collectExpressionDependencies(item.get(), out);
        return;
    }
    case ExpressionStatementNodeType::DictionaryLiteral: {
        const auto& node = static_cast<const DictionaryLiteralExpressionStatementNode&>(*expr);
        for (const auto& entry : node.entries) {
            collectExpressionDependencies(entry.value.get(), out);
        }
        return;
    }
    case ExpressionStatementNodeType::Subscript: {
        const auto& node = static_cast<const SubscriptExpressionStatementNode&>(*expr);
        const std::string path = expressionDependencyPath(expr);
        appendUnique(out, path);
        appendUnique(out, rootDependencyName(path));
        collectExpressionDependencies(node.base.get(), out);
        collectExpressionDependencies(node.index.get(), out);
        return;
    }
    case ExpressionStatementNodeType::FunctionCall: {
        const auto& node = static_cast<const FunctionCallExpressionStatementNode&>(*expr);
        for (const auto& arg : node.arguments) collectExpressionDependencies(arg.get(), out);
        return;
    }
    case ExpressionStatementNodeType::Conditional: {
        const auto& node = static_cast<const ConditionalExpressionStatementNode&>(*expr);
        collectExpressionDependencies(node.condition.get(), out);
        collectExpressionDependencies(node.whenTrue.get(), out);
        collectExpressionDependencies(node.whenFalse.get(), out);
        return;
    }
    case ExpressionStatementNodeType::Range: {
        const auto& node = static_cast<const RangeExpressionStatementNode&>(*expr);
        collectExpressionDependencies(node.start.get(), out);
        collectExpressionDependencies(node.end.get(), out);
        return;
    }
    case ExpressionStatementNodeType::ObjectPropertyAccess: {
        const auto& node = static_cast<const ObjectPropertyAccessExpressionNode&>(*expr);
        const std::string path = expressionDependencyPath(expr);
        appendUnique(out, path);
        appendUnique(out, rootDependencyName(path));
        collectExpressionDependencies(node.base.get(), out);
        return;
    }
    case ExpressionStatementNodeType::ObjectMethodCall: {
        const auto& node = static_cast<const ObjectMethodCallExpressionNode&>(*expr);
        collectExpressionDependencies(node.base.get(), out);
        for (const auto& arg : node.arguments) collectExpressionDependencies(arg.get(), out);
        return;
    }
    case ExpressionStatementNodeType::StringLiteral:
    case ExpressionStatementNodeType::NumberLiteral:
    case ExpressionStatementNodeType::BooleanLiteral:
        return;
    }
}

std::unique_ptr<ExpressionStatementNode> parseExpressionFragment(const std::string& expression) {
    const std::string trimmed = trimCopy(expression);
    if (trimmed.empty()) return nullptr;
    Lexer lexer(trimmed);
    auto tokens = lexer.tokenize();
    if (!lexer.getErrors().empty()) return nullptr;
    Parser parser(std::move(tokens));
    try {
        auto parsed = parser.parseStandaloneExpression();
        if (!parser.getErrors().empty()) return nullptr;
        return parsed;
    } catch (...) {
        return nullptr;
    }
}

std::vector<std::string> expressionDependencies(const std::string& expression) {
    if (auto ir = parseExpressionIr(expression)) {
        return expressionIrDependencies(*ir);
    }
    return {};
}

std::vector<std::string> templateBindingWrites(const std::vector<std::string>& tokens) {
    std::vector<std::string> writes;
    for (size_t i = 1; i + 1 < tokens.size(); ++i) {
        if (tokens[i] != "into") continue;
        if (isIdentifierToken(tokens[i + 1])) appendUnique(writes, tokens[i + 1]);
    }
    return writes;
}

std::vector<std::string> templateExpressionDependencies(
        const std::string& expression,
        const std::vector<std::string>& tokens) {
    std::vector<std::string> reads;
    std::vector<std::string> positional;
    const bool likelyComponentCall = !tokens.empty() &&
        !tokens[0].empty() &&
        std::isupper(static_cast<unsigned char>(tokens[0].front())) != 0;
    for (size_t i = 1; i < tokens.size(); ++i) {
        const std::string& token = tokens[i];
        if (token == "into") {
            // Text inputs read state for their displayed value as well as
            // writing it on input. Keep this dependency in the Runtime Plan so
            // programmatic writes invalidate retained/conditional controls.
            if (tokens.front() == "input" && i + 1 < tokens.size()) {
                appendUnique(reads, expressionDependencies(tokens[i + 1]));
            }
            ++i;
            continue;
        }
        if (token == "key") break;
        if (isBodyPlanEventName(token)) {
            ++i;
            continue;
        }
        if (likelyComponentCall && !isQuotedToken(token)) {
            positional.push_back(token);
            continue;
        }
        if (isQuotedToken(token)) {
            appendUnique(reads, expressionDependencies(token));
            continue;
        }
        if (isBodyPlanBooleanAttributeName(token)) continue;
        if (isBodyPlanSemanticModifierName(token)) {
            ++i;
            continue;
        }
        if (bodyPlanAttributeTakesValue(token) && i + 1 < tokens.size()) {
            ++i;
            continue;
        }
        if (isBodyPlanAttributeName(token) && i + 1 < tokens.size()) {
            ++i;
            continue;
        }
        if (!isQuotedToken(token)) positional.push_back(token);
    }
    for (const auto& value : positional) {
        appendUnique(reads, expressionDependencies(value));
    }
    for (size_t i = 1; i + 1 < tokens.size(); ++i) {
        const std::string& token = tokens[i];
        if (!isBodyPlanAttributeName(token)) continue;
        const std::string& value = tokens[i + 1];
        if (isQuotedToken(value)) {
            appendUnique(reads, expressionDependencies(value));
            continue;
        }
        if (!bodyPlanAttributeTakesValue(token) &&
            ((isBodyPlanAttributeName(value) && value != token) ||
             isBodyPlanBooleanAttributeName(value) ||
             isBodyPlanSemanticModifierName(value))) {
            continue;
        }
        appendUnique(reads, expressionDependencies(value));
    }
    return reads;
}

std::vector<std::string> forLoopExpressionDependencies(
        const std::string& expression,
        const std::string& keyExpression) {
    const auto inPos = expression.find(" in ");
    std::vector<std::string> reads = expressionDependencies(
        inPos == std::string::npos ? expression : expression.substr(inPos + 4));
    const auto iterator = inPos == std::string::npos ? std::string() : trimCopy(expression.substr(0, inPos));
    for (const auto& dependency : expressionDependencies(keyExpression)) {
        // Key expressions execute in row scope. The iterator is not an outer
        // observable; collection reads above remain even when names shadow.
        if (!iterator.empty() && rootDependencyName(dependency) == iterator) continue;
        appendUnique(reads, dependency);
    }
    return reads;
}

void appendUnique(std::vector<std::string>& target, const std::string& value) {
    if (value.empty()) return;
    if (std::find(target.begin(), target.end(), value) == target.end()) {
        target.push_back(value);
    }
}

void appendUnique(std::vector<std::string>& target, const std::vector<std::string>& values) {
    for (const auto& value : values) appendUnique(target, value);
}

std::vector<RuntimePlanComponentBodyNode> buildComponentBodyPlan(
        const std::string& bodySource) {
    std::vector<RuntimePlanComponentBodyNode> plan;
    std::vector<size_t> openAncestors;
    std::istringstream input(bodySource);
    std::string raw;
    int bodyLine = 0;
    while (std::getline(input, raw)) {
        ++bodyLine;
        const std::string text = parseBodyText(raw);
        if (text.empty()) continue;
        const auto tokens = words(text);
        if (tokens.empty()) continue;
        RuntimePlanComponentBodyNode node;
        node.indent = parseBodyIndent(raw);
        const int explicitSourceLine = parseBodySourceLine(raw);
        const int explicitSourceColumn = parseBodySourceColumn(raw);
        while (!openAncestors.empty() &&
               plan[openAncestors.back()].indent >= node.indent) {
            openAncestors.pop_back();
        }
        node.parentIndex = openAncestors.empty()
            ? -1
            : static_cast<int>(openAncestors.back());
        node.text = text;
        node.sourceLine = explicitSourceLine > 0 ? explicitSourceLine : bodyLine;
        node.sourceLineAbsolute = explicitSourceLine > 0;
        node.sourceColumn = explicitSourceColumn > 0 ? explicitSourceColumn : node.indent + 1;
        node.head = tokens[0];
        const bool postfixAssignment = isPostfixAssignmentBodyLine(tokens);
        node.kind = (isAssignmentBodyLine(tokens) || postfixAssignment) ? "assignment" :
            (isCallBodyLine(tokens) ? "call" : componentBodyNodeKind(node.head));
        if (node.kind == "assignment") {
            if (postfixAssignment) {
                parsePostfixAssignmentToken(tokens[0], node.name, node.operatorToken);
                node.name = cleanComponentNameToken(node.name);
                node.expression = "1";
            } else {
                node.name = cleanComponentNameToken(tokens[0]);
                node.operatorToken = tokens[1];
                node.expression = joinTokens(tokens, 2);
            }
            appendUnique(node.writes, node.name);
            appendUnique(node.writes, rootDependencyName(node.name));
            appendUnique(node.reads, expressionDependencies(node.expression));
            if (node.operatorToken != "=" || node.name.find('[') != std::string::npos) {
                // Resolving a computed write target also reads its index.
                appendUnique(node.reads, expressionDependencies(node.name));
                appendUnique(node.reads, rootDependencyName(node.name));
            }
        } else if (node.kind == "call") {
            const auto open = tokens[0].find('(');
            node.name = cleanComponentNameToken(tokens[0].substr(0, open));
            node.expression = tokens[0].substr(open + 1, tokens[0].size() - open - 2);
            appendUnique(node.reads, expressionDependencies(node.expression));
        } else if ((node.kind == "state" || node.kind == "derived" ||
             node.kind == "action" || node.kind == "effect") &&
            tokens.size() > 1) {
            node.name = cleanComponentNameToken(tokens[1]);
            const auto equals = std::find(tokens.begin(), tokens.end(), "=");
            if (equals != tokens.end()) {
                node.operatorToken = "=";
                node.expression = joinTokens(tokens, static_cast<size_t>((equals - tokens.begin()) + 1));
            }
            if (node.kind == "state" || node.kind == "derived") {
                appendUnique(node.writes, node.name);
                appendUnique(node.reads, expressionDependencies(node.expression));
            } else if (node.kind == "effect") {
                appendUnique(node.reads, expressionDependencies(node.expression.empty() ? node.name : node.expression));
            }
        } else if (node.kind == "template") {
            node.name = cleanComponentNameToken(tokens[0]);
            node.expression = joinTokens(tokens, 1);
            if (node.name == "for") {
                auto keyIt = std::find(tokens.begin() + 1, tokens.end(), "key");
                if (keyIt != tokens.end()) {
                    const size_t keyIndex = static_cast<size_t>(keyIt - tokens.begin());
                    node.expression = joinTokensUntil(tokens, 1, keyIndex);
                    node.keyExpression = joinTokens(tokens, keyIndex + 1);
                }
            }
            if (node.name == "if" || node.name == "show" || node.name == "text") {
                // These heads own a complete expression, not element attributes.
                // In particular, observable names such as title, selected and
                // enabled must not be consumed by the template attribute grammar.
                appendUnique(node.reads, expressionDependencies(node.expression));
            } else if (node.name == "for") {
                appendUnique(node.reads, forLoopExpressionDependencies(node.expression, node.keyExpression));
            } else {
                appendUnique(node.reads, templateExpressionDependencies(node.expression, tokens));
            }
            appendUnique(node.writes, templateBindingWrites(tokens));
        }
        if (node.kind == "template" && node.head == "scene3d") {
            // Retain the authored read/write set and provenance, but render the
            // canonical canvas/attributes from Friendly's existing parser.
            // Backend element parsers must never reinterpret scene modifiers
            // as a quoted content expression.
            const auto element = lowerFriendlySceneElement(tokens);
            node.head = node.name = element.front();
            node.text = joinTokens(element, 0);
            node.expression.clear(); // An attribute-only canvas has no content.
        }
        node.renderRoot = node.indent == 0 && node.kind == "template";
        const int parentIndex = node.parentIndex;
        plan.push_back(std::move(node));
        if (parentIndex >= 0 && static_cast<size_t>(parentIndex) < plan.size()) {
            plan[static_cast<size_t>(parentIndex)].childIndices.push_back(
                static_cast<int>(plan.size() - 1));
        }
        openAncestors.push_back(plan.size() - 1);
    }
    return plan;
}

void absolutizeBodyPlanSourceLines(std::vector<RuntimePlanComponentBodyNode>& bodyPlan,
                                   int ownerSourceLine) {
    if (ownerSourceLine <= 0) return;
    for (auto& node : bodyPlan) {
        if (node.sourceLine > 0) {
            if (!node.sourceLineAbsolute) node.sourceLine += ownerSourceLine;
        }
    }
}

} // namespace

void attachBodyOrigins(std::vector<RuntimePlanComponentBodyNode>& body,
                       const std::shared_ptr<const SourceProvenance>& provenance) {
    if (!provenance || provenance->kind != SourceProvenance::Kind::FriendlyLowering) return;
    for (auto& node : body) {
        // Relative compatibility metadata is not an original coordinate.
        if (!node.sourceLineAbsolute) continue;
        node.sourceSpan.provenance = provenance;
        node.sourceSpan.original = provenance->inputRange(node.sourceLine, node.sourceColumn, node.text.size());
        if (node.sourceSpan.original) {
            const auto& origin = *node.sourceSpan.original;
            node.sourceSpan.originalExact = origin.source->text.substr(origin.begin, origin.end - origin.begin) == node.text;
        }
        if (!node.sourceSpan.originalExact) node.sourceSpan.original = provenance->inputConstruct(node.sourceLine);
    }
}

RuntimePlanComponentDefinition planComponentDefinition(
        const SemanticComponentDefinition& semantic,
        SemanticModuleId moduleId = InvalidSemanticModuleId) {
    RuntimePlanComponentDefinition planned;
    planned.moduleId = moduleId;
    planned.name = semantic.name;
    planned.params = semantic.params;
    planned.bindingParams = semantic.bindingParams;
    planned.emits = semantic.emits;
    planned.emitArity = semantic.emitArity;
    planned.emitPayloads = semantic.emitPayloads;
    planned.emitPayloadTypes = semantic.emitPayloadTypes;
    planned.localState = semantic.localState;
    planned.localDerived = semantic.localDerived;
    planned.localActions = semantic.localActions;
    planned.localEffects = semantic.localEffects;
    planned.eventBindings = semantic.eventBindings;
    planned.bodyHex = semantic.bodyHex;
    planned.bodySource = hexDecode(semantic.bodyHex);
    planned.bodyPlan = buildComponentBodyPlan(planned.bodySource);
    absolutizeBodyPlanSourceLines(planned.bodyPlan, semantic.sourceLine);
    attachBodyOrigins(planned.bodyPlan, semantic.provenance);
    planned.hasSlot = semantic.hasSlot;
    planned.bodyNodeCount = semantic.bodyNodeCount;
    planned.rootTemplateNodeCount = semantic.rootTemplateNodeCount;
    planned.slotCount = semantic.slotCount;
    planned.sourceLine = semantic.sourceLine;
    return planned;
}

RuntimePlanComponentInstance planComponentInstance(
        const SemanticComponentInstance& semantic,
        SemanticModuleId moduleId = InvalidSemanticModuleId,
        SemanticModuleId definitionModule = InvalidSemanticModuleId) {
    RuntimePlanComponentInstance planned;
    planned.moduleId = moduleId;
    planned.definitionModule = definitionModule;
    planned.id = semantic.id;
    planned.slotInstance = semantic.slotInstance;
    planned.component = semantic.component;
    planned.instanceId = semantic.instanceId;
    planned.role = semantic.role.empty() ? "component" : semantic.role;
    planned.params = semantic.params;
    planned.argumentExpressions = semantic.argumentExpressions;
    planned.bindingArguments = semantic.bindingArguments;
    planned.sourceSpan.provenance = semantic.provenance;
    if (semantic.provenance) planned.sourceSpan.original = semantic.provenance->inputConstruct(semantic.sourceLine);
    planned.locals = semantic.locals;
    planned.slotHex = semantic.slotHex;
    planned.slotSource = hexDecode(semantic.slotHex);
    planned.slotPlan = buildComponentBodyPlan(planned.slotSource);
    absolutizeBodyPlanSourceLines(planned.slotPlan, semantic.sourceLine);
    attachBodyOrigins(planned.slotPlan, semantic.provenance);
    planned.sourceLine = semantic.sourceLine;
    return planned;
}

SemanticModuleId resolveLocalComponentDefinitionModule(
        const SemanticProgram& semantic,
        const std::string& component,
        SemanticModuleId moduleId) {
    for (const auto& definition : semantic.componentDefinitions) {
        if (definition.name == component) return moduleId;
    }
    return InvalidSemanticModuleId;
}

SemanticModuleId resolveImportedComponentDefinitionModule(
        const SemanticModule& module,
        const std::string& component) {
    for (const auto& import : module.imports) {
        for (const auto& symbol : import.resolvedSymbols) {
            if (symbol.name == component &&
                (symbol.kind.empty() || symbol.kind == "make" ||
                 symbol.kind == "component")) {
                return symbol.module;
            }
        }
    }
    return InvalidSemanticModuleId;
}

SemanticModuleId resolveComponentDefinitionModule(
        const SemanticModule& module,
        const std::string& component) {
    if (component.empty()) return InvalidSemanticModuleId;
    const auto local = resolveLocalComponentDefinitionModule(
        module.semantic, component, module.id);
    if (local != InvalidSemanticModuleId) return local;
    return resolveImportedComponentDefinitionModule(module, component);
}

void annotateBodyPlanComponentModules(
        std::vector<RuntimePlanComponentBodyNode>& bodyPlan,
        const SemanticModule& module) {
    for (auto& node : bodyPlan) {
        if (node.kind != "template" || node.name.empty()) continue;
        node.definitionModule = resolveComponentDefinitionModule(module, node.name);
    }
}

RuntimePlan buildRuntimePlan(const std::vector<std::unique_ptr<ASTNode>>& program) {
    return buildRuntimePlan(program, analyzeSemanticProgram(program));
}

RuntimePlan buildRuntimePlan(const std::vector<std::unique_ptr<ASTNode>>& program,
                             const SemanticProgram& semantic) {
    RuntimePlan plan;
    plan.semantic = semantic;
    plan.routes = semantic.routeRecords;
    plan.fetches = semantic.fetchRecords;
    for (const auto& definition : semantic.componentDefinitions) {
        plan.componentDefinitions.push_back(planComponentDefinition(definition));
    }
    for (const auto& instance : semantic.componentInstances) {
        plan.componentInstances.push_back(planComponentInstance(instance));
    }

    for (const auto& node : program) {
        if (!node) continue;
        if (node->getType() == ASTNodeType::DefineStatement) {
            const auto& stmt = static_cast<const DefineStatementNode&>(*node);
            plan.state.push_back({stmt.identifier, expressionSource(stmt.expression.get()), node->sourceSpan});
        } else if (node->getType() == ASTNodeType::DeriveStatement) {
            const auto& stmt = static_cast<const DeriveStatementNode&>(*node);
            plan.derived.push_back({stmt.identifier, expressionSource(stmt.expression.get()), node->sourceSpan});
        } else if (node->getType() == ASTNodeType::FunctionDeclaration) {
            const auto& stmt = static_cast<const FunctionDeclarationNode&>(*node);
            RuntimePlanAction action;
            action.name = stmt.name;
            action.sourceSpan = node->sourceSpan;
            for (const auto& param : stmt.parameters) {
                action.params.push_back(param.name);
            }
            action.body = collectRuntimeStatements(stmt.body);
            plan.actions.push_back(std::move(action));
        }
    }

    planRootSubscriptions(program, plan);
    return plan;
}

RuntimeProjectPlan buildRuntimePlan(const SemanticProject& project) {
    const std::vector<std::unique_ptr<ASTNode>> emptyProgram;
    return buildRuntimePlan(project, buildRuntimePlan(emptyProgram, project.linkedProgram));
}

namespace {

// Entry-module text-input authority is proven from retained module plans. No
// imported lookup, component-scope fallback or browser name resolution grants it.
void planExplicitInputs(RuntimeProjectPlan& project) {
    // Reserve the complete declared namespace before issuing any generated
    // identifier. Every result is fresh with respect to authored bindings and
    // prior allocations; no particular spelling or finite suffix is special.
    std::set<std::string> occupied;
    const auto reserve = [&](const auto& names) { occupied.insert(names.begin(), names.end()); };
    const auto reservePlan = [&](const RuntimePlan& plan) {
        reserve(plan.semantic.state); reserve(plan.semantic.constants);
        reserve(plan.semantic.derived); reserve(plan.semantic.actions);
        reserve(plan.semantic.fetches); reserve(plan.semantic.stores); reserve(plan.semantic.externs);
        for (const auto& edge : plan.semantic.dependencies) { occupied.insert(edge.from); occupied.insert(edge.to); }
        for (const auto& action : plan.actions) { occupied.insert(action.name); reserve(action.params); }
        for (const auto& definition : plan.componentDefinitions) {
            occupied.insert(definition.name); reserve(definition.params);
            reserve(definition.localState); reserve(definition.localDerived); reserve(definition.localActions); reserve(definition.eventBindings);
            for (const auto& node : definition.bodyPlan) {
                occupied.insert(node.name); reserve(node.reads); reserve(node.writes);
                if (node.kind == "action") {
                    const auto words = splitFriendlyWords(node.text);
                    if (words.size() > 2) occupied.insert(words.begin() + 2, words.end());
                }
            }
        }
    };
    reservePlan(project.linkedPlan);
    for (const auto& module : project.modules) reservePlan(module.plan);
    const auto fresh = [&](const std::string& stem) {
        std::string name = stem;
        for (size_t suffix = 1; !occupied.insert(name).second; ++suffix)
            name = stem + "_" + std::to_string(suffix);
        return name;
    };
    for (auto& owner : project.modules) for (auto& instance : owner.plan.componentInstances) {
        if (instance.bindingArguments.empty()) continue;
        const auto fail = [&](const std::string& reason) {
            throw std::runtime_error("component " + instance.component + " binding at " +
                effectLocation(instance.sourceSpan) + ": " + reason);
        };
        if (owner.id != project.entry) fail("only an entry-module caller may authorize an input binding; imported state and binding forwarding are unsupported");
        if (!instance.sourceSpan.original || instance.sourceSpan.original->source->file != owner.path)
            fail("caller provenance does not match the owning entry module");
        const RuntimePlanComponentDefinition* definition = nullptr;
        for (const auto& module : project.modules) for (const auto& candidate : module.plan.componentDefinitions)
            if (candidate.moduleId == instance.definitionModule && candidate.name == instance.component) definition = &candidate;
        if (!definition) fail("binding declaration has no resolved component owner");
        for (const auto& argument : instance.bindingArguments) {
            for (const auto& route : owner.plan.routes)
                if (std::find(route.params.begin(), route.params.end(), argument.value) != route.params.end())
                    fail("route parameters cannot grant input binding authority");
            if (std::find(definition->bindingParams.begin(), definition->bindingParams.end(), argument.name) == definition->bindingParams.end())
                fail("unauthorized binding parameter `" + argument.name + "`");
            const auto state = std::find_if(owner.plan.state.begin(), owner.plan.state.end(), [&](const auto& binding) { return binding.name == argument.value; });
            if (state == owner.plan.state.end() || std::find(owner.plan.semantic.constants.begin(), owner.plan.semantic.constants.end(), argument.value) != owner.plan.semantic.constants.end())
                fail("`" + argument.value + "` must be a mutable let owned by the entry module; constants, derived and imported bindings are read-only");
            const auto value = parseExpressionIr(state->expr);
            if (!value || value->kind != ExpressionIrKind::Literal || value->valueType != ExpressionValueType::String)
                fail("`" + argument.value + "` requires a literal string initializer; borrowed/collection targets are unsupported");
            size_t owners = 0;
            for (const auto& module : project.modules) {
                for (const auto& binding : module.plan.state) if (binding.name == argument.value) ++owners;
                for (const auto& binding : module.plan.derived) if (binding.name == argument.value) ++owners;
            }
            if (owners != 1) fail("ambiguous module storage for `" + argument.value + "`; input authority requires a unique owner");
            for (const auto& node : definition->bodyPlan) {
                if (std::find(node.writes.begin(), node.writes.end(), argument.name) != node.writes.end() && node.kind == "assignment")
                    fail("binding parameter `" + argument.name + "` is input-only; action assignments are unsupported at " + effectLocation(node.sourceSpan));
                if (node.kind == "template" && node.head != "input" &&
                    std::find(node.writes.begin(), node.writes.end(), argument.name) != node.writes.end())
                    fail("binding parameters support text input only at " + effectLocation(node.sourceSpan));
                const auto words = splitFriendlyWords(node.text);
                for (size_t n = 0; n + 1 < words.size(); ++n) if (words[n] == "into" && words[n + 1] == argument.name) {
                    if (node.head != "input") fail("binding parameters support text input only at " + effectLocation(node.sourceSpan));
                    for (size_t k = 0; k + 1 < words.size(); ++k) if (words[k] == "type" && words[k + 1] != "\"text\"" && words[k + 1] != "\"email\"" && words[k + 1] != "\"password\"" && words[k + 1] != "\"search\"" && words[k + 1] != "\"tel\"" && words[k + 1] != "\"url\"")
                        fail("binding parameters support text input types only at " + effectLocation(node.sourceSpan));
                }
            }
            const auto actionName = fresh("__jtml_input_" + std::to_string(owner.id) + "_" + instance.id + "_" + argument.name);
            RuntimePlanAction action;
            action.name = actionName;
            action.params = {fresh("__jtml_input_value")};
            RuntimePlanStatement write;
            write.kind = "assign"; write.lhs = argument.value; write.expr = action.params.front();
            action.body.push_back(write);
            owner.plan.actions.push_back(action);
            project.linkedPlan.actions.push_back(action);
            instance.inputActions[argument.name] = actionName;
            for (auto& linked : project.linkedPlan.componentInstances) {
                if (linked.id != instance.id || linked.sourceLine != instance.sourceLine || linked.component != instance.component) continue;
                if (!linked.sourceSpan.original || !instance.sourceSpan.original || linked.sourceSpan.original->source->file != instance.sourceSpan.original->source->file)
                    fail("linked input instance lacks authoritative owner provenance");
                linked.inputActions[argument.name] = actionName;
                linked.moduleId = owner.id; linked.definitionModule = instance.definitionModule;
            }
        }
    }
    for (const auto& instance : project.linkedPlan.componentInstances) {
        if (instance.bindingArguments.size() != instance.inputActions.size())
            throw std::runtime_error("component " + instance.component + " binding at " + effectLocation(instance.sourceSpan) + ": no authoritative retained caller plan; explicit input authority cannot be issued");
    }
}

} // namespace

RuntimeProjectPlan buildRuntimePlan(const SemanticProject& project,
                                    const RuntimePlan& linkedPlan) {
    RuntimeProjectPlan projectPlan;
    projectPlan.entry = project.entry;
    projectPlan.linkedPlan = linkedPlan;

    const std::vector<std::unique_ptr<ASTNode>> emptyProgram;
    for (const auto& module : project.modules) {
        RuntimeModulePlan modulePlan;
        modulePlan.id = module.id;
        modulePlan.path = module.path;
        modulePlan.astAvailable = module.ast && module.ast->available;
        modulePlan.syntax = module.ast ? module.ast->syntax : "";
        modulePlan.clientExecutable =
            modulePlan.astAvailable &&
            modulePlan.syntax.find("import-stubs") == std::string::npos;
        modulePlan.plan = modulePlan.clientExecutable
            ? buildRuntimePlan(module.ast->nodes, module.semantic)
            : buildRuntimePlan(emptyProgram, module.semantic);
        for (auto& subscription : modulePlan.plan.subscriptions) subscription.moduleId = module.id;
        for (auto& definition : modulePlan.plan.componentDefinitions) {
            definition.moduleId = module.id;
            annotateBodyPlanComponentModules(definition.bodyPlan, module);
        }
        for (auto& instance : modulePlan.plan.componentInstances) {
            instance.moduleId = module.id;
            instance.definitionModule = resolveLocalComponentDefinitionModule(
                module.semantic, instance.component, module.id);
            if (instance.definitionModule == InvalidSemanticModuleId) {
                instance.definitionModule =
                    resolveImportedComponentDefinitionModule(module, instance.component);
            }
            annotateBodyPlanComponentModules(instance.slotPlan, module);
        }
        projectPlan.modules.push_back(std::move(modulePlan));
    }

    // The browser has one canonical root subscription table. Per-module plans
    // prove ownership; linked source order determines stable ties. Never merge
    // these tables again at bootstrap or register during rendering.
    const bool hasSubscriptions = !projectPlan.linkedPlan.subscriptions.empty() ||
        std::any_of(projectPlan.modules.begin(), projectPlan.modules.end(),
            [](const auto& module) { return !module.plan.subscriptions.empty(); });
    if (!projectPlan.modules.empty() && hasSubscriptions) {
        std::vector<RuntimePlanSubscription> subscriptions;
        std::set<std::tuple<SemanticModuleId, std::string, std::string>> seen;
        auto append = [&](const RuntimePlanSubscription& subscription) {
            if (seen.emplace(subscription.moduleId, subscription.action, subscription.dependency).second)
                subscriptions.push_back(subscription);
        };
        for (const auto& original : project.linkedProgram.subscriptions) {
            for (const auto& module : projectPlan.modules) for (const auto& subscription : module.plan.subscriptions) {
                if (original.action != subscription.action || original.dependency != subscription.dependency) continue;
                if (original.sourceSpan.original && subscription.sourceSpan.original &&
                    (original.sourceSpan.original->source->file != subscription.sourceSpan.original->source->file ||
                     original.sourceSpan.original->begin != subscription.sourceSpan.original->begin)) continue;
                append(subscription);
            }
        }
        for (const auto& module : projectPlan.modules) {
            for (const auto& subscription : module.plan.subscriptions) append(subscription);
            for (const auto& state : module.plan.state) {
                if (std::none_of(projectPlan.linkedPlan.state.begin(), projectPlan.linkedPlan.state.end(),
                    [&](const auto& item) { return item.name == state.name; })) projectPlan.linkedPlan.state.push_back(state);
            }
            for (const auto& action : module.plan.actions) {
                if (std::none_of(projectPlan.linkedPlan.actions.begin(), projectPlan.linkedPlan.actions.end(),
                    [&](const auto& item) { return item.name == action.name; })) projectPlan.linkedPlan.actions.push_back(action);
            }
        }
        projectPlan.linkedPlan.subscriptions = std::move(subscriptions);
        if (!projectPlan.linkedPlan.subscriptions.empty()) {
            std::map<std::string, std::set<SemanticModuleId>> owners;
            for (const auto& module : projectPlan.modules) {
                for (const auto& state : module.plan.state) owners[state.name].insert(module.id);
                for (const auto& action : module.plan.actions) owners[action.name].insert(module.id);
            }
            for (const auto& [name, modules] : owners) if (modules.size() > 1) {
                projectPlan.linkedPlan.effectIssues.push_back("effect at " +
                    effectLocation(projectPlan.linkedPlan.subscriptions.front().sourceSpan) +
                    ": ambiguous module root binding `" + name +
                    "`; root effects require distinct module state/action names");
            }
        }
        orderRootSubscriptions(projectPlan.linkedPlan);
    }
    for (const auto& module : projectPlan.modules) {
        for (const auto& issue : module.plan.effectIssues)
            if (std::find(projectPlan.linkedPlan.effectIssues.begin(), projectPlan.linkedPlan.effectIssues.end(), issue) ==
                projectPlan.linkedPlan.effectIssues.end()) projectPlan.linkedPlan.effectIssues.push_back(issue);
    }
    planExplicitInputs(projectPlan);
    return projectPlan;
}

} // namespace jtml
