#include "jtml/runtime_plan.h"

#include "jtml/expression_source.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>

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

int parseBodyIndent(const std::string& encodedLine) {
    const auto colon = encodedLine.find(':');
    if (colon == std::string::npos) return 0;
    try {
        return std::stoi(encodedLine.substr(0, colon));
    } catch (...) {
        return 0;
    }
}

std::string parseBodyText(const std::string& encodedLine) {
    const auto colon = encodedLine.find(':');
    return trimCopy(colon == std::string::npos ? encodedLine : encodedLine.substr(colon + 1));
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

bool isDependencyStopWord(const std::string& word) {
    static const std::set<std::string> stops = {
        "true", "false", "null", "undefined",
        "if", "else", "for", "in", "while", "key",
        "let", "const", "get", "when", "effect", "slot",
        "and", "or", "not",
        "page", "box", "text", "show", "button", "input", "form",
        "panel", "card", "metric", "grid", "stack", "cluster", "split",
        "toolbar", "tabs", "tab", "alert", "badge", "modal", "drawer",
        "toast", "loading", "error", "empty", "field", "spacer",
        "class", "style", "id", "title", "href", "src", "alt", "to",
        "click", "input", "change", "submit", "hover", "scroll",
        "pad", "gap", "cols", "radius", "shadow", "tone", "align",
        "justify", "width", "surface",
    };
    return stops.count(word) > 0;
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

bool isBodyPlanSemanticModifierName(const std::string& token) {
    static const std::set<std::string> names = {
        "cols", "gap", "pad", "radius", "shadow", "tone",
        "align", "justify", "width", "surface",
    };
    return names.count(token) > 0;
}

std::vector<std::string> expressionDependencies(const std::string& expression) {
    std::set<std::string> names;
    std::string current;
    char quote = '\0';
    auto flush = [&] {
        if (current.empty()) return;
        const std::string token = current;
        current.clear();
        if (token.empty()) return;
        if (std::isdigit(static_cast<unsigned char>(token.front()))) return;
        if (isDependencyStopWord(token)) return;
        names.insert(token);
    };
    for (size_t i = 0; i < expression.size(); ++i) {
        const char ch = expression[i];
        if (quote != '\0') {
            if (ch == '\\' && i + 1 < expression.size()) {
                ++i;
            } else if (ch == quote) {
                quote = '\0';
            }
            continue;
        }
        if (ch == '"' || ch == '\'') {
            flush();
            quote = ch;
            continue;
        }
        if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '.') {
            current.push_back(ch);
        } else {
            flush();
        }
    }
    flush();
    return {names.begin(), names.end()};
}

void appendUnique(std::vector<std::string>& target, const std::string& value);
void appendUnique(std::vector<std::string>& target, const std::vector<std::string>& values);

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
    std::vector<std::string> reads = expressionDependencies(expression);
    for (size_t i = 1; i + 1 < tokens.size(); ++i) {
        const std::string& token = tokens[i];
        if (!isBodyPlanAttributeName(token)) continue;
        const std::string& value = tokens[i + 1];
        if (isQuotedToken(value) ||
            (isBodyPlanAttributeName(value) && value != token) ||
            isBodyPlanBooleanAttributeName(value) ||
            isBodyPlanSemanticModifierName(value)) {
            continue;
        }
        if (isIdentifierToken(value)) appendUnique(reads, value);
    }
    return reads;
}

std::vector<std::string> forLoopExpressionDependencies(
        const std::string& expression,
        const std::string& keyExpression) {
    const auto inPos = expression.find(" in ");
    std::vector<std::string> reads = expressionDependencies(
        inPos == std::string::npos ? expression : expression.substr(inPos + 4));
    appendUnique(reads, expressionDependencies(keyExpression));
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
        while (!openAncestors.empty() &&
               plan[openAncestors.back()].indent >= node.indent) {
            openAncestors.pop_back();
        }
        node.parentIndex = openAncestors.empty()
            ? -1
            : static_cast<int>(openAncestors.back());
        node.text = text;
        node.sourceLine = bodyLine;
        node.head = tokens[0];
        node.kind = isAssignmentBodyLine(tokens) ? "assignment" :
            (isCallBodyLine(tokens) ? "call" : componentBodyNodeKind(node.head));
        if (node.kind == "assignment") {
            node.name = cleanComponentNameToken(tokens[0]);
            node.operatorToken = tokens[1];
            node.expression = joinTokens(tokens, 2);
            appendUnique(node.writes, node.name);
            appendUnique(node.reads, expressionDependencies(node.expression));
            if (node.operatorToken != "=") appendUnique(node.reads, node.name);
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
            if (node.name == "for") {
                appendUnique(node.reads, forLoopExpressionDependencies(node.expression, node.keyExpression));
            } else {
                appendUnique(node.reads, templateExpressionDependencies(node.expression, tokens));
            }
            appendUnique(node.writes, templateBindingWrites(tokens));
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

} // namespace

RuntimePlanComponentDefinition planComponentDefinition(
        const SemanticComponentDefinition& semantic,
        SemanticModuleId moduleId = InvalidSemanticModuleId) {
    RuntimePlanComponentDefinition planned;
    planned.moduleId = moduleId;
    planned.name = semantic.name;
    planned.params = semantic.params;
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
    planned.component = semantic.component;
    planned.instanceId = semantic.instanceId;
    planned.role = semantic.role.empty() ? "component" : semantic.role;
    planned.params = semantic.params;
    planned.locals = semantic.locals;
    planned.slotHex = semantic.slotHex;
    planned.slotSource = hexDecode(semantic.slotHex);
    planned.slotPlan = buildComponentBodyPlan(planned.slotSource);
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
            plan.state.push_back({stmt.identifier, expressionSource(stmt.expression.get())});
        } else if (node->getType() == ASTNodeType::DeriveStatement) {
            const auto& stmt = static_cast<const DeriveStatementNode&>(*node);
            plan.derived.push_back({stmt.identifier, expressionSource(stmt.expression.get())});
        } else if (node->getType() == ASTNodeType::FunctionDeclaration) {
            const auto& stmt = static_cast<const FunctionDeclarationNode&>(*node);
            RuntimePlanAction action;
            action.name = stmt.name;
            for (const auto& param : stmt.parameters) {
                action.params.push_back(param.name);
            }
            action.body = collectRuntimeStatements(stmt.body);
            plan.actions.push_back(std::move(action));
        }
    }

    return plan;
}

RuntimeProjectPlan buildRuntimePlan(const SemanticProject& project) {
    const std::vector<std::unique_ptr<ASTNode>> emptyProgram;
    return buildRuntimePlan(project, buildRuntimePlan(emptyProgram, project.linkedProgram));
}

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

    return projectPlan;
}

} // namespace jtml
