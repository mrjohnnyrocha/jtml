#include "jtml/runtime_plan.h"
#include "jtml/expression_ir.h"
#include "jtml/expression_source.h"
#include "jtml/attribute_classifier.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>

namespace jtml {
namespace {
using Names = std::map<std::string, std::string>;

std::string location(const SourceSpan& span, const std::string& file) {
    if (!span.original) return file;
    const auto& origin = *span.original;
    const auto position = origin.source->coordinates(origin.begin);
    return origin.source->file + ":" + std::to_string(position.first) + ":" + std::to_string(position.second);
}

bool scalar(ExpressionValueType type) {
    return type == ExpressionValueType::String || type == ExpressionValueType::Number ||
           type == ExpressionValueType::Boolean;
}

struct Proof {
    const RuntimeModulePlan& owner;
    [[noreturn]] void fail(const SourceSpan& span, const std::string& reason) const {
        throw std::runtime_error(location(span, owner.path) + ": " + reason);
    }
    void owned(const ASTNode& node) const {
        if (!node.sourceSpan.original || node.sourceSpan.original->source->file != owner.path)
            fail(node.sourceSpan, "declaration has no retained defining-module provenance");
    }
    // Only parser-owned variable nodes are renamed. This recursive AST walk also
    // closes the expression language: no member, call, subscript or fallback lookup.
    void expression(ExpressionStatementNode* node, const Names& bindings) const {
        if (!node) fail({}, "missing scalar expression");
        switch (node->getExprType()) {
            case ExpressionStatementNodeType::StringLiteral: {
                // Decoded escapes can hide interpolation from Friendly's AST
                // lowering. Runtime Plan serialization rebuilds Expression IR;
                // such a string would introduce reads never resolved above.
                const auto ir = buildExpressionIr(node);
                if (!ir || ir->kind != ExpressionIrKind::Literal)
                    fail(node->sourceSpan, "string interpolation requires resolved expression nodes; use explicit scalar concatenation");
                return;
            }
            case ExpressionStatementNodeType::BooleanLiteral: return;
            case ExpressionStatementNodeType::NumberLiteral:
                if (!std::isfinite(static_cast<NumberLiteralExpressionStatementNode&>(*node).value))
                    fail(node->sourceSpan, "numeric literals must be finite");
                return;
            case ExpressionStatementNodeType::Variable: {
                auto& variable = static_cast<VariableExpressionStatementNode&>(*node);
                const auto found = bindings.find(variable.name);
                if (found == bindings.end()) fail(node->sourceSpan,
                    "unresolved or imported binding `" + variable.name + "`; scalar actions may only read their defining module or parameters");
                variable.name = found->second;
                return;
            }
            case ExpressionStatementNodeType::Binary: {
                auto& binary = static_cast<BinaryExpressionStatementNode&>(*node);
                expression(binary.left.get(), bindings); expression(binary.right.get(), bindings); break;
            }
            case ExpressionStatementNodeType::Unary:
                expression(static_cast<UnaryExpressionStatementNode&>(*node).right.get(), bindings); break;
            default: fail(node->sourceSpan, "only scalar literals, identifiers and scalar unary/binary expressions are supported");
        }
        const auto ir = buildExpressionIr(node);
        if (!ir || !ir->supported || !expressionIrSupportsDirectJs(*ir))
            fail(node->sourceSpan, "expression requires unsupported fallback evaluation");
    }
    void scalarValue(const ExpressionStatementNode* node, const ExpressionTypeEnvironment& types) const {
        const auto ir = buildExpressionIr(node, types);
        if (!ir || !scalar(ir->valueType)) fail(node->sourceSpan, "assignment/initializer must prove a scalar value");
    }
    // Root markup must remain independent of renamed storage. Static metadata
    // and literal action arguments do not establish a writable/readable binding.
    void markup(const ASTNode& node, const std::set<std::string>& actions) const {
        if (node.getType() == ASTNodeType::JtmlElement) {
            const auto& element = static_cast<const JtmlElementNode&>(node);
            for (const auto& attribute : element.attributes) {
                if (attribute.key == "data-jtml-expr" || attribute.key == "data-jtml-cond-expr" ||
                    attribute.key == "data-jtml-for-expr" || attribute.key.rfind("data-jtml-attr-", 0) == 0 ||
                    attribute.key.rfind("data-jtml-event-", 0) == 0 || attribute.key == "data-jtml-media-controller" ||
                    attribute.key == "data-jtml-scene3d-controller")
                    fail(node.sourceSpan, "dynamic runtime metadata requires an owner-qualified contract");
                if (isJtmlEventAttribute(attribute.key)) {
                    if (!attribute.value) fail(node.sourceSpan, "missing event action");
                    const auto ir = buildExpressionIr(attribute.value.get());
                    if (ir && ir->kind == ExpressionIrKind::Path && ir->segments.size() == 1 && actions.count(ir->root)) continue;
                    if (ir && ir->kind == ExpressionIrKind::Call && actions.count(ir->callee) &&
                        std::all_of(ir->children.begin(), ir->children.end(), [](const auto& arg) {
                            return arg && arg->kind == ExpressionIrKind::Literal && scalar(arg->valueType);
                        })) continue;
                    fail(node.sourceSpan, "events require a distinct root action and literal scalar arguments");
                }
                std::string literal;
                if (!jtmlStaticAttributeValue(attribute.value.get(), literal))
                    fail(node.sourceSpan, "root/component state reads and writable UI bindings remain unsupported in scalar collision builds");
            }
            for (const auto& child : element.content) markup(*child, actions);
            return;
        }
        if (node.getType() == ASTNodeType::ShowStatement) {
            const auto& show = static_cast<const ShowStatementNode&>(node);
            std::string literal;
            if (jtmlStaticAttributeValue(show.expr.get(), literal)) return;
        }
        fail(node.sourceSpan, "only static markup and scalar root actions are supported in collision builds");
    }
};

void certify(RuntimeProjectPlan& project) {
    std::set<std::string> occupied, actionNames;
    for (const auto& module : project.modules) {
        const Proof proof{module};
        if (!module.clientExecutable || !module.ast || !module.ast->available)
            proof.fail({}, "scalar isolation requires retained executable module ASTs");
        for (const auto& imported : module.importedSymbols)
            if (imported.kind != "make" && imported.kind != "component" && imported.kind != "when" && imported.kind != "action")
                proof.fail({}, "imported value bindings are outside the scalar isolation contract");
        const auto& plan = module.plan;
        if (!plan.derived.empty() || !plan.fetches.empty() || !plan.subscriptions.empty() ||
            !plan.semantic.effects.empty() || !plan.semantic.stores.empty() ||
            !plan.semantic.externs.empty() || plan.semantic.rawHtmlBlocks || !plan.routes.empty())
            proof.fail({}, "derived, fetch, effect, store, raw HTML, extern and route consumers require a separate owner-qualified contract");
        for (const auto& definition : plan.componentDefinitions) {
            if (!definition.params.empty() || !definition.localState.empty() || !definition.localDerived.empty() ||
                !definition.localActions.empty() || !definition.emits.empty())
                proof.fail({}, "this scalar slice permits static control components only");
            for (const auto& node : definition.bodyPlan)
                if (!node.reads.empty() || !node.writes.empty())
                    proof.fail(node.sourceSpan, "component-to-module reads/writes remain unsupported");
        }
        for (const auto& node : module.ast->nodes) {
            if (node->getType() == ASTNodeType::DefineStatement) {
                const auto& binding = static_cast<const DefineStatementNode&>(*node);
                occupied.insert(binding.identifier);
            } else if (node->getType() == ASTNodeType::FunctionDeclaration) {
                const auto& action = static_cast<const FunctionDeclarationNode&>(*node);
                if (action.name == "__proto__") proof.fail(node->sourceSpan, "reserved JavaScript object-key action name");
                if (!actionNames.insert(action.name).second)
                    proof.fail(node->sourceSpan, "colliding action names remain unsupported");
                occupied.insert(action.name);
                for (const auto& param : action.parameters) occupied.insert(param.name);
            }
        }
    }
    // Fixed-width monotonically allocated names preserve initializer order in
    // name-sorted client JSON. Reserve authored declarations/parameters first;
    // unresolved reads are rejected, never captured by an allocated identifier.
    size_t next = 0;
    auto fresh = [&]() {
        for (;;) {
            std::ostringstream name;
            name << "__jtml_scalar_" << std::setw(20) << std::setfill('0') << next++;
            if (occupied.insert(name.str()).second) return name.str();
        }
    };
    std::vector<RuntimePlanBinding> states;
    std::vector<RuntimePlanAction> actions;
    for (auto& module : project.modules) {
        const Proof proof{module};
        Names bindings;
        ExpressionTypeEnvironment types;
        std::vector<std::unique_ptr<ASTNode>> lowered;
        for (const auto& original : module.ast->nodes) {
            if (original->getType() != ASTNodeType::DefineStatement) continue;
            proof.owned(*original);
            auto copy = original->clone();
            auto& state = static_cast<DefineStatementNode&>(*copy);
            if (state.isConst || bindings.count(state.identifier))
                proof.fail(state.sourceSpan, "only distinct mutable scalar let declarations are supported");
            const auto authored = state.identifier;
            // Earlier declarations in this same module are the only initializer scope.
            proof.expression(state.expression.get(), bindings);
            proof.scalarValue(state.expression.get(), types);
            state.identifier = fresh();
            types.symbols[state.identifier] = buildExpressionIr(state.expression.get(), types)->valueType;
            bindings.emplace(authored, state.identifier);
            lowered.push_back(std::move(copy));
        }
        for (const auto& original : module.ast->nodes) {
            if (original->getType() == ASTNodeType::DefineStatement) continue;
            if (original->getType() != ASTNodeType::FunctionDeclaration) {
                proof.markup(*original, actionNames); continue;
            }
            proof.owned(*original);
            auto copy = original->clone();
            auto& action = static_cast<FunctionDeclarationNode&>(*copy);
            if (action.isAsync) proof.fail(action.sourceSpan, "async root actions remain unsupported");
            Names scope = bindings;
            auto actionTypes = types;
            std::set<std::string> params;
            for (auto& param : action.parameters) {
                if (!params.insert(param.name).second) proof.fail(action.sourceSpan, "duplicate action parameter");
                const auto authored = param.name;
                param.name = fresh();
                scope[authored] = param.name; // Parameter binding shadows the module declaration.
                actionTypes.symbols[param.name] = ExpressionValueType::Unknown;
            }
            for (auto& statement : action.body) {
                if (statement->getType() == ASTNodeType::ShowStatement) {
                    proof.expression(static_cast<ShowStatementNode&>(*statement).expr.get(), scope);
                } else if (statement->getType() == ASTNodeType::AssignmentStatement) {
                    auto& write = static_cast<AssignmentStatementNode&>(*statement);
                    if (write.localDeclaration || !write.lhs || write.lhs->getExprType() != ExpressionStatementNodeType::Variable)
                        proof.fail(statement->sourceSpan, "only plain assignments to owned scalar bindings or parameters are supported");
                    proof.expression(write.lhs.get(), scope);
                    proof.expression(write.rhs.get(), scope);
                    proof.scalarValue(write.rhs.get(), actionTypes);
                } else proof.fail(statement->sourceSpan, "only straight-line assign/show actions are supported");
            }
            lowered.push_back(std::move(copy));
        }
        // Rebuild only the root execution records from transformed owned ASTs.
        // Component ownership and source ASTs are untouched.
        auto planned = buildRuntimePlan(lowered, module.plan.semantic);
        module.plan.state = std::move(planned.state);
        module.plan.actions = std::move(planned.actions);
        module.scalarStorageKeys = std::move(bindings);
        states.insert(states.end(), module.plan.state.begin(), module.plan.state.end());
        actions.insert(actions.end(), module.plan.actions.begin(), module.plan.actions.end());
    }
    // The linked projection is a consumer of certified records, never an owner.
    project.linkedPlan.state = std::move(states);
    project.linkedPlan.actions = std::move(actions);
}
} // namespace

void lowerBrowserScalarStorage(RuntimeProjectPlan& project) {
    std::map<std::string, SemanticModuleId> owners;
    bool collision = false;
    for (const auto& module : project.modules) if (module.clientExecutable)
        for (const auto& state : module.plan.state) {
            const auto inserted = owners.emplace(state.name, module.id);
            collision |= !inserted.second && inserted.first->second != module.id;
        }
    if (!collision) return; // Existing collision-free and single-module builds are byte-stable.
    auto candidate = project;
    try {
        certify(candidate);
        project = std::move(candidate);
    } catch (const std::runtime_error& error) {
        project.scalarStorageIssue = error.what(); // No partial renaming; PR #75 still rejects.
    }
}
} // namespace jtml
