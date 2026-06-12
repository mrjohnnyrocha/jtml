#include "jtml/runtime_plan.h"

#include "jtml/expression_source.h"

#include <algorithm>
#include <cctype>
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
    std::istringstream input(value);
    std::string word;
    while (input >> word) out.push_back(word);
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

std::vector<RuntimePlanComponentBodyNode> buildComponentBodyPlan(
        const std::string& bodySource) {
    std::vector<RuntimePlanComponentBodyNode> plan;
    std::vector<size_t> openAncestors;
    std::istringstream input(bodySource);
    std::string raw;
    while (std::getline(input, raw)) {
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
        node.head = tokens[0];
        node.kind = isAssignmentBodyLine(tokens) ? "assignment" : componentBodyNodeKind(node.head);
        if (node.kind == "assignment") {
            node.name = cleanComponentNameToken(tokens[0]);
            node.operatorToken = tokens[1];
            node.expression = joinTokens(tokens, 2);
        } else if ((node.kind == "state" || node.kind == "derived" ||
             node.kind == "action" || node.kind == "effect") &&
            tokens.size() > 1) {
            node.name = cleanComponentNameToken(tokens[1]);
            const auto equals = std::find(tokens.begin(), tokens.end(), "=");
            if (equals != tokens.end()) {
                node.operatorToken = "=";
                node.expression = joinTokens(tokens, static_cast<size_t>((equals - tokens.begin()) + 1));
            }
        } else if (node.kind == "template") {
            node.name = cleanComponentNameToken(tokens[0]);
            node.expression = joinTokens(tokens, 1);
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
        const SemanticComponentDefinition& semantic) {
    RuntimePlanComponentDefinition planned;
    planned.name = semantic.name;
    planned.params = semantic.params;
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
        const SemanticComponentInstance& semantic) {
    return {
        semantic.id,
        semantic.component,
        semantic.instanceId,
        semantic.role.empty() ? "component" : semantic.role,
        semantic.params,
        semantic.locals,
        semantic.sourceLine,
    };
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
    RuntimeProjectPlan projectPlan;
    projectPlan.entry = project.entry;

    const std::vector<std::unique_ptr<ASTNode>> emptyProgram;
    projectPlan.linkedPlan = buildRuntimePlan(emptyProgram, project.linkedProgram);

    for (const auto& module : project.modules) {
        RuntimeModulePlan modulePlan;
        modulePlan.id = module.id;
        modulePlan.path = module.path;
        modulePlan.astAvailable = module.ast && module.ast->available;
        modulePlan.syntax = module.ast ? module.ast->syntax : "";
        modulePlan.plan = modulePlan.astAvailable
            ? buildRuntimePlan(module.ast->nodes, module.semantic)
            : buildRuntimePlan(emptyProgram, module.semantic);
        projectPlan.modules.push_back(std::move(modulePlan));
    }

    return projectPlan;
}

} // namespace jtml
