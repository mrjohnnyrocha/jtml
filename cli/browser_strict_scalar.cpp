#include "browser_build_policy.h"

#include "jtml/expression_ir.h"
#include "jtml/friendly.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>

namespace jtml::cli {
namespace {

using Type = ExpressionValueType;
using Kind = ExpressionIrKind;
using Types = std::map<std::string, Type>;
using Names = std::set<std::string>;
using Arities = std::map<std::string, size_t>;

struct Site {
    const SourceSpan& span;
    const std::string& file;

    [[noreturn]] void reject(const std::string& reason) const {
        if (span.original) {
            const auto& origin = *span.original;
            const auto [line, column] = origin.source->coordinates(origin.begin);
            throw std::runtime_error(origin.source->file + ":" + std::to_string(line) + ":" +
                std::to_string(column) + ": E_STRICT_SCALAR: " + reason);
        }
        throw std::runtime_error(file + ":" + std::to_string(std::max(1, span.startLine)) + ":" +
            std::to_string(std::max(1, span.startColumn)) + ": E_STRICT_SCALAR: " + reason);
    }

    void requireUnescapedSource() const {
        if (!span.original) return;
        const auto& origin = *span.original;
        const auto escape = origin.source->text.find('\\', origin.begin);
        if (escape < origin.end) reject("escaped source text is outside the private subset");
    }
};

void requireName(const std::string& name, Site site) {
    static const Names reserved{
        "__proto__", "constructor", "toString", "toLocaleString", "valueOf", "hasOwnProperty",
        "isPrototypeOf", "propertyIsEnumerable", "__defineGetter__", "__defineSetter__",
        "__lookupGetter__", "__lookupSetter__"
    };
    auto start = [](char ch) { return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_'; };
    auto rest = [&](char ch) { return start(ch) || (ch >= '0' && ch <= '9'); };
    if (name.empty() || !start(name.front()) || !std::all_of(name.begin() + 1, name.end(), rest) ||
        reserved.count(name)) site.reject("unsupported binding name `" + name + "`");
}

Type literal(const ExpressionIr& ir, Site site) {
    if (ir.literal.is_boolean()) return Type::Boolean;
    if (ir.literal.is_number()) {
        const auto value = ir.literal.get<double>();
        if (std::isfinite(value) && std::abs(value) <= 9007199254740991.0 && std::trunc(value) == value)
            return Type::Number;
        site.reject("numeric literals must be exactly representable safe integers");
    }
    if (ir.literal.is_string()) {
        const auto& text = ir.literal.get_ref<const std::string&>();
        if (std::all_of(text.begin(), text.end(), [](unsigned char c) {
                return c >= 32 && c <= 126 && c != '{' && c != '}' && c != '"' && c != '\\';
            })) return Type::String;
        site.reject("only plain ASCII string literals are admitted");
    }
    site.reject("expected a scalar literal");
}

void literalText(const std::string& source, Site site) {
    const auto ir = parseExpressionIr(source);
    if (!ir || ir->kind != Kind::Literal || literal(*ir, site) != Type::String)
        site.reject("expected literal text");
}

// Closure is proved independently of inferred Expression IR result hints.
// Slot kinds stay invariant under every admitted assignment and event entry.
Type expression(const ExpressionIr* ir, const Types& scope, Site site) {
    if (!ir || !ir->supported) site.reject("expected a supported scalar expression");
    if (ir->kind == Kind::Literal) return literal(*ir, site);
    if (ir->kind == Kind::Path && ir->segments.size() == 1) {
        const auto binding = scope.find(ir->root);
        if (binding != scope.end()) return binding->second;
        site.reject("scalar binding `" + ir->root + "` is unavailable here");
    }
    auto child = [&](size_t i) { return expression(ir->children.at(i).get(), scope, site); };
    if (ir->kind == Kind::Unary) {
        const Type argument = child(0);
        if (ir->op == "!" && argument == Type::Boolean) return Type::Boolean;
        if (ir->op == "-" && argument == Type::Number) return Type::Number;
    } else if (ir->kind == Kind::Conditional) {
        const Type test = child(0), yes = child(1), no = child(2);
        if (test == Type::Boolean && yes == no) return yes;
    } else if (ir->kind == Kind::Binary) {
        const Type left = child(0), right = child(1);
        if (left == right) {
            const auto& op = ir->op;
            if (op == "==" || op == "!=") return Type::Boolean;
            if (left == Type::Boolean && (op == "&&" || op == "||")) return Type::Boolean;
            if (left == Type::String && op == "+") return Type::String;
            if (left == Type::Number) {
                if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%") return Type::Number;
                if (op == "<" || op == ">" || op == "<=" || op == ">=") return Type::Boolean;
            }
        }
    }
    site.reject("unsupported scalar expression or operand kinds: `" + ir->source + "`");
}

void checkLiterals(const ExpressionIr* ir, Site site) {
    if (!ir) return;
    if (ir->kind == Kind::Literal) literal(*ir, site);
    for (const auto& child : ir->children) checkLiterals(child.get(), site);
}

Type projection(const ExpressionIr* ir, const Types& scope, Site site) {
    if (ir && (ir->kind == Kind::Literal || ir->kind == Kind::Path))
        return expression(ir, scope, site);
    // Friendly lowering represents text interpolation with concatenation.
    // At each join, a string operand guarantees a text result in both renderers.
    if (ir && ir->kind == Kind::Binary && ir->op == "+") {
        const auto left = projection(ir->children.at(0).get(), scope, site);
        const auto right = projection(ir->children.at(1).get(), scope, site);
        if (left == Type::String || right == Type::String) return Type::String;
    }
    site.reject("projection must be plain text or scalar reads; compute arithmetic in a getter");
}

void assign(const std::string& target, const ExpressionIr* rhs, const Types& scope,
            const Names& writable, Site site, bool compound = false) {
    const auto binding = scope.find(target);
    if (!writable.count(target) || binding == scope.end())
        site.reject("assignment requires a writable scalar cell `" + target + "`");
    if (expression(rhs, scope, site) != binding->second || (compound && binding->second != Type::Number))
        site.reject("assignment must preserve the scalar kind of `" + target + "`");
}

void actionStatements(const std::vector<RuntimePlanStatement>& body, const Types& scope,
                      const Names& writable, const std::string& file) {
    for (const auto& statement : body) {
        const Site site{statement.sourceSpan, file};
        if (statement.kind == "assign" && statement.localBinding.empty()) {
            assign(statement.lhs, parseExpressionIr(statement.expr).get(), scope, writable, site);
        } else if (statement.kind == "show") {
            expression(parseExpressionIr(statement.expr).get(), scope, site);
        } else if (statement.kind == "if") {
            if (expression(parseExpressionIr(statement.condition).get(), scope, site) != Type::Boolean)
                site.reject("action condition must be Boolean");
            actionStatements(statement.thenStatements, scope, writable, file);
            actionStatements(statement.elseStatements, scope, writable, file);
        } else {
            site.reject("only assignments, show and Boolean branches are admitted in root actions");
        }
    }
}

// Verify the actual lowered AST as well as the Runtime Plan. Unsupported action
// statements can otherwise disappear when the plan is collected.
void actionAst(const std::vector<std::unique_ptr<ASTNode>>& body, const std::string& file) {
    for (const auto& statement : body) {
        const Site site{statement->sourceSpan, file};
        site.requireUnescapedSource();
        switch (statement->getType()) {
        case ASTNodeType::AssignmentStatement:
            checkLiterals(buildExpressionIr(static_cast<const AssignmentStatementNode&>(*statement).rhs.get()).get(), site);
            break;
        case ASTNodeType::ShowStatement:
            checkLiterals(buildExpressionIr(static_cast<const ShowStatementNode&>(*statement).expr.get()).get(), site);
            break;
        case ASTNodeType::IfStatement: {
            const auto& branch = static_cast<const IfStatementNode&>(*statement);
            checkLiterals(buildExpressionIr(branch.condition.get()).get(), site);
            actionAst(branch.thenStatements, file);
            actionAst(branch.elseStatements, file);
            break;
        }
        default: site.reject("unsupported action statement");
        }
    }
}

void event(const ExpressionIr* ir, const Types& scope, const Arities& arities, Site site) {
    if (!ir || (ir->kind != Kind::Call && (ir->kind != Kind::Path || ir->segments.size() != 1)))
        site.reject("expected a named click action");
    const auto& action = ir->kind == Kind::Call ? ir->callee : ir->root;
    const auto found = arities.find(action);
    if (found == arities.end() || found->second != ir->children.size())
        site.reject("click must match the declared arity of `" + action + "`");
    for (const auto& arg : ir->children) {
        if ((arg->kind != Kind::Literal && arg->kind != Kind::Path) ||
            expression(arg.get(), scope, site) != Type::Number)
            site.reject("click arguments must be finite integer literals or resolved numeric reads");
    }
}

struct ComponentProof {
    const RuntimePlanComponentDefinition& definition;
    Types scope;
};

ComponentProof component(const RuntimePlanComponentDefinition& definition, const Types& roots,
                         const Arities& rootActions, const std::string& file) {
    ComponentProof result{definition, {}};
    auto& scope = result.scope;
    const SourceSpan empty;
    const Site declaration{definition.bodyPlan.empty() ? empty : definition.bodyPlan.front().sourceSpan, file};
    if (!definition.emits.empty() || !definition.bindingParams.empty() || definition.hasSlot)
        declaration.reject("component slots, emitted callbacks and binding parameters are outside the private subset");
    for (const auto& name : definition.params) {
        requireName(name, declaration);
        if (!scope.emplace(name, Type::Number).second) declaration.reject("duplicate component parameter");
    }
    Names writable;
    auto declare = [&](const RuntimePlanComponentBodyNode& node) {
        const Site site{node.sourceSpan, file};
        site.requireUnescapedSource();
        requireName(node.name, site);
        const auto type = expression(parseExpressionIr(node.expression).get(), scope, site);
        if (!scope.emplace(node.name, type).second) site.reject("duplicate component binding `" + node.name + "`");
        if (node.head == "let") writable.insert(node.name);
    };
    for (const auto& node : definition.bodyPlan)
        if (node.parentIndex == -1 && node.kind == "state") declare(node);
    for (const auto& [name, key] : definition.captures) {
        requireName(name, declaration);
        const auto capture = roots.find(key);
        if (capture == roots.end() || !scope.emplace(name, capture->second).second)
            declaration.reject("unresolved or ambiguous scalar capture `" + name + "`");
    }
    Arities actions = rootActions;
    Names localActions;
    for (const auto& node : definition.bodyPlan) {
        if (node.parentIndex != -1) continue;
        if (node.kind == "derived") declare(node);
        if (node.kind == "action") {
            const Site site{node.sourceSpan, file};
            if (!node.introducedBindings.empty()) site.reject("component actions take no arguments in this subset");
            requireName(node.name, site);
            if (!localActions.insert(node.name).second) site.reject("duplicate component action");
            actions[node.name] = 0;
        }
    }
    for (const auto& node : definition.bodyPlan) {
        const Site site{node.sourceSpan, file};
        site.requireUnescapedSource();
        if (node.kind == "state" || node.kind == "derived" || node.kind == "action") {
            if (node.parentIndex != -1) site.reject("component declarations must be at component scope");
            continue;
        }
        const bool inAction = node.parentIndex >= 0 && definition.bodyPlan[node.parentIndex].kind == "action";
        if (inAction && node.kind == "assignment") {
            if (node.operatorToken != "=" && node.operatorToken != "+=")
                site.reject("component assignments admit only = and numeric +=");
            assign(node.name, parseExpressionIr(node.expression).get(), scope, writable, site, node.operatorToken != "=");
            continue;
        }
        if (inAction || node.kind != "template") site.reject("unsupported component action or statement");
        if (node.head == "text") {
            projection(parseExpressionIr(node.expression).get(), scope, site);
            continue;
        }
        const auto words = splitFriendlyWords(node.text);
        if (node.head == "box" && (words.size() == 1 || (words.size() == 3 && words[1] == "class"))) {
            if (words.size() == 3) literalText(words[2], site);
            continue;
        }
        if (node.head == "button" && words.size() == 4 && words[2] == "click") {
            literalText(words[1], site);
            const auto call = parseExpressionIr(words[3]);
            if (call && !call->children.empty()) site.reject("component click actions take no arguments");
            event(call.get(), scope, actions, site);
            continue;
        }
        if ((node.head == "p" || node.head == "h1" || node.head == "h2") && words.size() == 2) {
            projection(parseExpressionIr(words[1]).get(), scope, site);
            continue;
        }
        site.reject("unsupported component template; use box, text, headings or named click buttons");
    }
    return result;
}

const LoweredComponentMarkup* boundary(const SourceSpan& span) {
    if (!span.provenance) return nullptr;
    const auto& boundaries = span.provenance->componentMarkup;
    const auto found = boundaries.find(span.startLine);
    return found == boundaries.end() ? nullptr : &found->second;
}

struct Markup {
    const RuntimePlan& linked;
    const std::map<std::string, ComponentProof>& components;
    const std::string& file;
    void inspect(const std::vector<std::unique_ptr<ASTNode>>& nodes, const Types& scope,
                 const Arities& actions, bool document = false, bool insideComponent = false) const {
        for (const auto& node : nodes) {
            const Site site{node->sourceSpan, file};
            site.requireUnescapedSource();
            switch (node->getType()) {
            case ASTNodeType::JtmlElement: {
                const auto& element = static_cast<const JtmlElementNode&>(*node);
                if (const auto* mark = boundary(node->sourceSpan)) {
                    if (mark->kind == LoweredComponentMarkup::Kind::Definition) break;
                    if (mark->kind != LoweredComponentMarkup::Kind::Instance || insideComponent)
                        site.reject("only root component instances are admitted in the private subset");
                    const auto instance = std::find_if(linked.componentInstances.begin(), linked.componentInstances.end(),
                        [&](const auto& item) { return item.instanceId == mark->instanceId; });
                    if (instance == linked.componentInstances.end()) site.reject("missing certified component instance");
                    const auto found = components.find(instance->component);
                    if (found == components.end() || !instance->bindingArguments.empty() || !instance->slotPlan.empty() ||
                        instance->argumentExpressions.size() != found->second.definition.params.size())
                        site.reject("unsupported component arguments");
                    for (const auto& argument : instance->argumentExpressions) {
                        const auto ir = parseExpressionIr(argument.value);
                        if (!ir || (ir->kind != Kind::Literal && ir->kind != Kind::Path) ||
                            expression(ir.get(), scope, site) != Type::Number)
                            site.reject("component arguments must be numeric literals or resolved reads");
                    }
                    auto instanceTypes = scope;
                    auto instanceActions = actions;
                    for (const auto& alias : instance->locals) {
                        const auto& proof = found->second;
                        if (const auto value = proof.scope.find(alias.name); value != proof.scope.end())
                            instanceTypes.emplace(alias.value, value->second);
                        const auto& localActions = proof.definition.localActions;
                        if (std::find(localActions.begin(), localActions.end(), alias.name) != localActions.end())
                            instanceActions.emplace(alias.value, 0);
                    }
                    inspect(element.content, instanceTypes, instanceActions, false, true);
                    break;
                }
                static const Names tags{"main", "div", "span", "p", "button", "h1", "h2"};
                if (!tags.count(element.tagName)) site.reject("unsupported private markup tag `" + element.tagName + "`");
                for (const auto& attribute : element.attributes) {
                    const auto ir = buildExpressionIr(attribute.value.get());
                    if (attribute.key == "onClick" && element.tagName == "button") {
                        event(ir.get(), scope, actions, site);
                    } else if ((attribute.key == "class" || attribute.key == "type") &&
                               ir && ir->kind == Kind::Literal && literal(*ir, site) == Type::String) {
                        continue;
                    } else {
                        site.reject("unsupported private markup attribute `" + attribute.key + "`");
                    }
                }
                inspect(element.content, scope, actions, false, insideComponent);
                break;
            }
            case ASTNodeType::ShowStatement:
                projection(buildExpressionIr(static_cast<const ShowStatementNode&>(*node).expr.get()).get(), scope, site);
                break;
            case ASTNodeType::IfStatement: {
                const auto& branch = static_cast<const IfStatementNode&>(*node);
                const auto ir = buildExpressionIr(branch.condition.get());
                if (!ir || (ir->kind != Kind::Literal && ir->kind != Kind::Path) ||
                    expression(ir.get(), scope, site) != Type::Boolean)
                    site.reject("rendered condition must be a Boolean literal or resolved read");
                inspect(branch.thenStatements, scope, actions, false, insideComponent);
                inspect(branch.elseStatements, scope, actions, false, insideComponent);
                break;
            }
            case ASTNodeType::DefineStatement:
                if (!document && !insideComponent) site.reject("declarations in markup are unsupported");
                if (!static_cast<const DefineStatementNode&>(*node).declaredType.empty())
                    site.reject("explicit type annotations are outside the private subset");
                checkLiterals(buildExpressionIr(static_cast<const DefineStatementNode&>(*node).expression.get()).get(), site);
                break;
            case ASTNodeType::DeriveStatement:
                if (!document && !insideComponent) site.reject("getters in markup are unsupported");
                if (!static_cast<const DeriveStatementNode&>(*node).declaredType.empty())
                    site.reject("explicit type annotations are outside the private subset");
                checkLiterals(buildExpressionIr(static_cast<const DeriveStatementNode&>(*node).expression.get()).get(), site);
                break;
            case ASTNodeType::FunctionDeclaration: {
                if (!document && !insideComponent) site.reject("actions in markup are unsupported");
                const auto& action = static_cast<const FunctionDeclarationNode&>(*node);
                if (action.isAsync) site.reject("async actions are unsupported");
                for (const auto& parameter : action.parameters)
                    if (!parameter.type.empty()) site.reject("explicit parameter type annotations are outside the private subset");
                actionAst(action.body, file);
                break;
            }
            case ASTNodeType::ImportStatement:
            case ASTNodeType::SubscribeStatement:
                if (!document) site.reject("only root imports and root effects are admitted");
                break;
            default: site.reject("statement is outside the private scalar subset");
            }
        }
    }
};

} // namespace

void validatePrivateScalarProfile(const RuntimeProjectPlan& project,
                                  const std::vector<std::unique_ptr<ASTNode>>& linkedAst) {
    using OwnedBinding = std::pair<const RuntimeModulePlan*, const RuntimePlanBinding*>;
    std::map<std::string, OwnedBinding> initializers, getters;
    Arities actions;
    Names writable;
    std::string file;
    for (const auto& module : project.modules) {
        if (module.id == project.entry) file = module.path;
        for (const auto& state : module.plan.state) {
            if (!initializers.emplace(state.name, OwnedBinding{&module, &state}).second)
                Site{state.sourceSpan, module.path}.reject("duplicate scalar initializer `" + state.name + "`");
            if (!state.isConst) writable.insert(state.name);
        }
        for (const auto& getter : module.plan.derived) {
            if (!getters.emplace(getter.name, OwnedBinding{&module, &getter}).second)
                Site{getter.sourceSpan, module.path}.reject("duplicate scalar getter `" + getter.name + "`");
        }
        for (const auto& action : module.plan.actions) {
            const Site site{action.sourceSpan, module.path};
            requireName(action.name, site);
            if (!actions.emplace(action.name, action.params.size()).second) site.reject("duplicate root action");
        }
    }
    Types roots;
    auto declare = [&](const auto& bindings, const auto& order) {
        for (const auto& name : order) {
            const auto& [owner, declaration] = bindings.at(name);
            const Site site{declaration->sourceSpan, owner->path};
            requireName(name, site);
            const auto type = expression(parseExpressionIr(declaration->expr).get(), roots, site);
            if (!roots.emplace(name, type).second) site.reject("ambiguous scalar binding `" + name + "`");
        }
    };
    declare(initializers, *project.stateOrder);
    declare(getters, project.derivedOrder);

    std::map<std::string, ComponentProof> components;
    for (const auto& module : project.modules) {
        for (const auto& action : module.plan.actions) {
            auto scope = roots;
            auto targets = writable;
            Names parameters;
            for (const auto& name : action.params) {
                const Site site{action.sourceSpan, module.path};
                requireName(name, site);
                if (!parameters.insert(name).second) site.reject("duplicate action parameter");
                scope[name] = Type::Number;
                targets.insert(name);
            }
            actionStatements(action.body, scope, targets, module.path);
        }
        for (const auto& definition : module.plan.componentDefinitions) {
            auto proof = component(definition, roots, actions, module.path);
            if (!components.emplace(definition.name, std::move(proof)).second) {
                const SourceSpan empty;
                Site{empty, module.path}.reject("ambiguous component definition `" + definition.name + "`");
            }
        }
    }
    Markup{project.linkedPlan, components, file}.inspect(linkedAst, roots, actions, true);
}

} // namespace jtml::cli
