// linter.cpp — see linter.h for the contract.
#include "jtml/linter.h"
#include "jtml/friendly.h"
#include "jtml/lexer.h"
#include "jtml/module_resolver.h"
#include "jtml/parser.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace jtml {

namespace {

std::string normalizeType(std::string type) {
    std::transform(type.begin(), type.end(), type.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (type == "boolean") return "bool";
    if (type == "str") return "string";
    if (type == "num" || type == "int" || type == "float" || type == "double") return "number";
    if (type == "array" || type == "list") return "array";
    if (type == "dict" || type == "dictionary" || type == "map") return "object";
    return type;
}

bool typeCompatible(const std::string& declared, const std::string& actual) {
    if (declared.empty() || actual.empty() || actual == "unknown") return true;
    if (declared == "any") return true;
    return normalizeType(declared) == normalizeType(actual);
}

std::string stringLiteralValue(const ExpressionStatementNode* expr) {
    if (!expr || expr->getExprType() != ExpressionStatementNodeType::StringLiteral) return "";
    return static_cast<const StringLiteralExpressionStatementNode*>(expr)->value;
}

std::string literalValue(const ExpressionStatementNode* expr) {
    if (!expr) return "";
    if (expr->getExprType() == ExpressionStatementNodeType::StringLiteral) {
        return static_cast<const StringLiteralExpressionStatementNode*>(expr)->value;
    }
    if (expr->getExprType() == ExpressionStatementNodeType::BooleanLiteral) {
        return static_cast<const BooleanLiteralExpressionStatementNode*>(expr)->value ? "true" : "false";
    }
    return "";
}

const JtmlAttribute* findAttr(const JtmlElementNode& elem, const std::string& key) {
    for (const auto& attr : elem.attributes) {
        if (attr.key == key) return &attr;
    }
    return nullptr;
}

bool hasAttr(const JtmlElementNode& elem, const std::string& key) {
    return findAttr(elem, key) != nullptr;
}

bool attrIsTruthy(const JtmlElementNode& elem, const std::string& key) {
    const auto* attr = findAttr(elem, key);
    if (!attr) return false;
    if (!attr->value) return true;
    const std::string value = literalValue(attr->value.get());
    return value.empty() || value == key || value == "true";
}

bool hasAccessibleName(const JtmlElementNode& elem) {
    if (hasAttr(elem, "aria-label") || hasAttr(elem, "aria-labelledby") ||
        hasAttr(elem, "title")) {
        return true;
    }
    if (attrIsTruthy(elem, "aria-hidden")) return true;
    const auto* role = findAttr(elem, "role");
    return role && literalValue(role->value.get()) == "presentation";
}

bool isFileInput(const JtmlElementNode& elem) {
    if (elem.tagName != "input") return false;
    const auto* type = findAttr(elem, "type");
    return type && literalValue(type->value.get()) == "file";
}

bool isScene3d(const JtmlElementNode& elem) {
    if (elem.tagName != "canvas") return false;
    const auto* marker = findAttr(elem, "data-jtml-scene3d");
    return marker && attrIsTruthy(elem, "data-jtml-scene3d");
}

std::string attrLiteral(const JtmlElementNode& elem, const std::string& key) {
    const auto* attr = findAttr(elem, key);
    return attr && attr->value ? literalValue(attr->value.get()) : "";
}

std::string externActionName(const JtmlElementNode& elem) {
    if (elem.tagName != "meta") return "";
    for (const auto& attr : elem.attributes) {
        if (attr.key == "data-jtml-extern-action") {
            return stringLiteralValue(attr.value.get());
        }
    }
    return "";
}

int implicitEventArgumentCount(const std::string& eventName) {
    if (eventName == "onInput" || eventName == "onChange" || eventName == "onScroll") {
        return 1;
    }
    return 0;
}

} // namespace

void JtmlLinter::setSourcePath(const std::string& path) {
    sourcePath_ = path;
}

void JtmlLinter::declareTopLevel(const std::vector<std::unique_ptr<ASTNode>>& program) {
    for (const auto& node : program) {
        if (!node) continue;
        switch (node->getType()) {
        case ASTNodeType::FunctionDeclaration: {
            const auto& fn = static_cast<const FunctionDeclarationNode&>(*node);
            defineFunction(fn.name, static_cast<int>(fn.parameters.size()));
            break;
        }
        case ASTNodeType::ClassDeclaration:
            define(static_cast<const ClassDeclarationNode&>(*node).name);
            break;
        case ASTNodeType::DefineStatement:
            define(static_cast<const DefineStatementNode&>(*node).identifier,
                   static_cast<const DefineStatementNode&>(*node).declaredType);
            break;
        case ASTNodeType::DeriveStatement:
            define(static_cast<const DeriveStatementNode&>(*node).identifier,
                   static_cast<const DeriveStatementNode&>(*node).declaredType);
            break;
        case ASTNodeType::ImportStatement:
            resolveImport(static_cast<const ImportStatementNode&>(*node).path);
            break;
        case ASTNodeType::JtmlElement: {
            const std::string action = externActionName(static_cast<const JtmlElementNode&>(*node));
            if (!action.empty()) defineFunction(action, -1);
            break;
        }
        default: break;
        }
    }
}

void JtmlLinter::resolveImport(const std::string& rawPath) {
    namespace fs = std::filesystem;
    if (rawPath.empty()) return;

    fs::path importPath = resolveJtmlModulePath(rawPath, sourcePath_);
    std::error_code ec;
    fs::path canonical = fs::weakly_canonical(importPath, ec);
    if (!ec && !canonical.empty()) importPath = canonical;

    if (!fs::exists(importPath)) return;  // missing imports are runtime errors
    std::string canonicalKey = importPath.string();
    if (std::find(importedFiles_.begin(), importedFiles_.end(), canonicalKey)
        != importedFiles_.end()) {
        return;  // already pulled in; cycle guard
    }
    importedFiles_.push_back(canonicalKey);

    std::ifstream ifs(importPath);
    if (!ifs) return;
    std::ostringstream buf; buf << ifs.rdbuf();
    std::string source = buf.str();

    std::string normalized;
    try {
        normalized = jtml::normalizeSourceSyntax(source);
    } catch (const std::exception&) {
        return;  // bad imported file — let `check` flag it
    }

    Lexer lex(normalized);
    auto tokens = lex.tokenize();
    if (!lex.getErrors().empty()) return;
    Parser parser(std::move(tokens));
    std::vector<std::unique_ptr<ASTNode>> imported;
    try {
        imported = parser.parseProgram();
    } catch (const std::exception&) {
        return;
    }
    if (!parser.getErrors().empty()) return;

    // Recurse so transitive imports flow into the importer's namespace too.
    std::string previous = sourcePath_;
    sourcePath_ = importPath.string();
    declareTopLevel(imported);
    sourcePath_ = previous;
}

std::vector<LintDiagnostic> JtmlLinter::lint(const std::vector<std::unique_ptr<ASTNode>>& program) {
    diagnostics.clear();
    scopes.clear();
    typeScopes.clear();
    functionArityScopes.clear();
    importedFiles_.clear();

    // Pre-pass: collect top-level declarations so that forward references
    // between top-level functions (a calls b, b calls a) don't trip the
    // undefined-reference check. Also pulls names from imported modules in
    // when a source path is configured, so cross-file references resolve.
    pushScope();
    declareTopLevel(program);

    visitBlock(program);
    popScope();
    return diagnostics;
}

// ---------------------------------------------------------------------------
// Scope bookkeeping
// ---------------------------------------------------------------------------
void JtmlLinter::pushScope() {
    scopes.emplace_back();
    typeScopes.emplace_back();
    functionArityScopes.emplace_back();
}
void JtmlLinter::popScope()  {
    if (!scopes.empty()) scopes.pop_back();
    if (!typeScopes.empty()) typeScopes.pop_back();
    if (!functionArityScopes.empty()) functionArityScopes.pop_back();
}

bool JtmlLinter::isDefined(const std::string& name) const {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        if (std::find(it->begin(), it->end(), name) != it->end()) return true;
    }
    return false;
}

bool JtmlLinter::isDefinedInCurrentScope(const std::string& name) const {
    if (scopes.empty()) return false;
    const auto& s = scopes.back();
    return std::find(s.begin(), s.end(), name) != s.end();
}

void JtmlLinter::define(const std::string& name, const std::string& declaredType) {
    if (scopes.empty()) pushScope();
    scopes.back().push_back(name);
    if (!declaredType.empty()) {
        typeScopes.back()[name] = normalizeType(declaredType);
    }
}

void JtmlLinter::defineFunction(const std::string& name, int arity) {
    define(name);
    if (functionArityScopes.empty()) pushScope();
    functionArityScopes.back()[name] = arity;
}

std::string JtmlLinter::lookupType(const std::string& name) const {
    for (auto it = typeScopes.rbegin(); it != typeScopes.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return found->second;
    }
    return "";
}

int JtmlLinter::lookupFunctionArity(const std::string& name) const {
    for (auto it = functionArityScopes.rbegin(); it != functionArityScopes.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return found->second;
    }
    return -1;
}

std::string JtmlLinter::inferExprType(const ExpressionStatementNode& expr) const {
    switch (expr.getExprType()) {
    case ExpressionStatementNodeType::StringLiteral:
    case ExpressionStatementNodeType::CompositeString:
    case ExpressionStatementNodeType::EmbeddedVariable:
        return "string";
    case ExpressionStatementNodeType::NumberLiteral:
        return "number";
    case ExpressionStatementNodeType::BooleanLiteral:
        return "bool";
    case ExpressionStatementNodeType::ArrayLiteral:
        return "array";
    case ExpressionStatementNodeType::DictionaryLiteral:
        return "object";
    case ExpressionStatementNodeType::Variable: {
        const auto& v = static_cast<const VariableExpressionStatementNode&>(expr);
        return lookupType(v.name);
    }
    case ExpressionStatementNodeType::Unary: {
        const auto& n = static_cast<const UnaryExpressionStatementNode&>(expr);
        if (n.op == "!") return "bool";
        if (n.op == "-") return "number";
        return "unknown";
    }
    case ExpressionStatementNodeType::Binary: {
        const auto& n = static_cast<const BinaryExpressionStatementNode&>(expr);
        if (n.op == "==" || n.op == "!=" || n.op == "<" || n.op == "<=" ||
            n.op == ">" || n.op == ">=" || n.op == "and" || n.op == "or") {
            return "bool";
        }
        const std::string left = n.left ? inferExprType(*n.left) : "unknown";
        const std::string right = n.right ? inferExprType(*n.right) : "unknown";
        if (n.op == "+" && (left == "string" || right == "string")) return "string";
        if (n.op == "+" || n.op == "-" || n.op == "*" || n.op == "/" || n.op == "%") {
            return "number";
        }
        return "unknown";
    }
    case ExpressionStatementNodeType::Conditional: {
        const auto& n = static_cast<const ConditionalExpressionStatementNode&>(expr);
        const std::string whenTrue = n.whenTrue ? inferExprType(*n.whenTrue) : "unknown";
        const std::string whenFalse = n.whenFalse ? inferExprType(*n.whenFalse) : "unknown";
        return whenTrue == whenFalse ? whenTrue : "unknown";
    }
    default:
        return "unknown";
    }
}

void JtmlLinter::checkDeclaredType(const std::string& name,
                                   const std::string& declaredType,
                                   const ExpressionStatementNode* expr) {
    if (declaredType.empty() || !expr) return;
    const std::string declared = normalizeType(declaredType);
    const std::string actual = normalizeType(inferExprType(*expr));
    if (!typeCompatible(declared, actual)) {
        reportError("type mismatch for '" + name + "': declared " + declared +
                    " but expression is " + actual);
    }
}

void JtmlLinter::reportError(const std::string& msg) {
    auto d = jtml::diagnosticFromMessage(msg, jtml::DiagnosticSeverity::Error);
    diagnostics.push_back({
        LintDiagnostic::Severity::Error,
        d.message,
        d.code,
        d.line,
        d.column,
        d.hint,
        d.example,
    });
}
void JtmlLinter::reportWarning(const std::string& msg) {
    auto d = jtml::diagnosticFromMessage(msg, jtml::DiagnosticSeverity::Warning);
    diagnostics.push_back({
        LintDiagnostic::Severity::Warning,
        d.message,
        d.code,
        d.line,
        d.column,
        d.hint,
        d.example,
    });
}

// ---------------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------------
void JtmlLinter::visitBlock(const std::vector<std::unique_ptr<ASTNode>>& stmts) {
    bool unreachableReported = false;
    bool flowEnded = false;
    for (const auto& s : stmts) {
        if (!s) continue;
        if (flowEnded && !unreachableReported) {
            reportWarning("unreachable code after return/throw/break/continue");
            unreachableReported = true;
        }
        visitStmt(*s);
        auto t = s->getType();
        if (t == ASTNodeType::ReturnStatement
         || t == ASTNodeType::ThrowStatement
         || t == ASTNodeType::BreakStatement
         || t == ASTNodeType::ContinueStatement) {
            flowEnded = true;
        }
    }
}

void JtmlLinter::visitStmt(const ASTNode& node) {
    switch (node.getType()) {
    case ASTNodeType::JtmlElement:
        visitElement(static_cast<const JtmlElementNode&>(node));
        return;
    case ASTNodeType::DefineStatement: {
        const auto& n = static_cast<const DefineStatementNode&>(node);
        if (n.expression) visitExpr(*n.expression);
        checkDeclaredType(n.identifier, n.declaredType, n.expression.get());
        // The pre-pass (for top-level and class bodies) may have already
        // declared this name so that forward references between siblings
        // resolve. That means we cannot cheaply tell the difference between
        // a legitimate user duplicate and a pre-pass entry, so we skip the
        // duplicate-define warning entirely rather than emit false
        // positives. Runtime duplicate detection still applies.
        if (!isDefinedInCurrentScope(n.identifier)) define(n.identifier, n.declaredType);
        return;
    }
    case ASTNodeType::DeriveStatement: {
        const auto& n = static_cast<const DeriveStatementNode&>(node);
        if (n.expression) visitExpr(*n.expression);
        checkDeclaredType(n.identifier, n.declaredType, n.expression.get());
        if (!isDefinedInCurrentScope(n.identifier)) define(n.identifier, n.declaredType);
        return;
    }
    case ASTNodeType::UnbindStatement: {
        const auto& n = static_cast<const UnbindStatementNode&>(node);
        if (!isDefined(n.identifier))
            reportError("unbind references undefined variable '" + n.identifier + "'");
        return;
    }
    case ASTNodeType::AssignmentStatement: {
        const auto& n = static_cast<const AssignmentStatementNode&>(node);
        // Tolerate assignments to subscripts / property access (like
        // `user["name"] = x`). Only flag pure undefined variable targets.
        if (n.lhs && n.lhs->getExprType() == ExpressionStatementNodeType::Variable) {
            const auto& v = static_cast<const VariableExpressionStatementNode&>(*n.lhs);
            if (!isDefined(v.name))
                reportError("assignment to undefined variable '" + v.name + "'");
        } else if (n.lhs) {
            visitExpr(*n.lhs);
        }
        if (n.rhs) visitExpr(*n.rhs);
        return;
    }
    case ASTNodeType::ShowStatement: {
        const auto& n = static_cast<const ShowStatementNode&>(node);
        if (n.expr) visitExpr(*n.expr);
        return;
    }
    case ASTNodeType::ReturnStatement: {
        const auto& n = static_cast<const ReturnStatementNode&>(node);
        if (n.expr) visitExpr(*n.expr);
        return;
    }
    case ASTNodeType::ThrowStatement: {
        const auto& n = static_cast<const ThrowStatementNode&>(node);
        if (n.expression) visitExpr(*n.expression);
        return;
    }
    case ASTNodeType::ExpressionStatement: {
        const auto& n = static_cast<const ExpressionNode&>(node);
        if (n.expression) visitExpr(*n.expression);
        return;
    }
    case ASTNodeType::IfStatement: {
        const auto& n = static_cast<const IfStatementNode&>(node);
        if (n.condition) visitExpr(*n.condition);
        pushScope(); visitBlock(n.thenStatements); popScope();
        if (!n.elseStatements.empty()) {
            pushScope(); visitBlock(n.elseStatements); popScope();
        }
        return;
    }
    case ASTNodeType::WhileStatement: {
        const auto& n = static_cast<const WhileStatementNode&>(node);
        if (n.condition) visitExpr(*n.condition);
        pushScope(); visitBlock(n.body); popScope();
        return;
    }
    case ASTNodeType::ForStatement: {
        const auto& n = static_cast<const ForStatementNode&>(node);
        if (n.iterableExpression) visitExpr(*n.iterableExpression);
        if (n.rangeEndExpr)       visitExpr(*n.rangeEndExpr);
        pushScope();
        define(n.iteratorName);
        visitBlock(n.body);
        popScope();
        return;
    }
    case ASTNodeType::TryExceptThen: {
        const auto& n = static_cast<const TryExceptThenNode&>(node);
        pushScope(); visitBlock(n.tryBlock); popScope();
        if (n.hasCatch) {
            pushScope();
            if (!n.catchIdentifier.empty()) define(n.catchIdentifier);
            visitBlock(n.catchBlock);
            popScope();
        }
        if (n.hasFinally) { pushScope(); visitBlock(n.finallyBlock); popScope(); }
        return;
    }
    case ASTNodeType::BlockStatement: {
        const auto& n = static_cast<const BlockStatementNode&>(node);
        pushScope(); visitBlock(n.statements); popScope();
        return;
    }
    case ASTNodeType::FunctionDeclaration: {
        const auto& n = static_cast<const FunctionDeclarationNode&>(node);
        if (!isDefinedInCurrentScope(n.name)) {
            defineFunction(n.name, static_cast<int>(n.parameters.size()));
        } else if (!functionArityScopes.empty()) {
            functionArityScopes.back()[n.name] = static_cast<int>(n.parameters.size());
        }
        pushScope();
        for (const auto& p : n.parameters) define(p.name);
        visitBlock(n.body);
        popScope();
        return;
    }
    case ASTNodeType::ClassDeclaration: {
        const auto& n = static_cast<const ClassDeclarationNode&>(node);
        if (!isDefinedInCurrentScope(n.name)) define(n.name);
        pushScope();
        // Class body can define members; collect first so methods can
        // reference each other.
        for (const auto& m : n.members) {
            if (!m) continue;
            if (m->getType() == ASTNodeType::DefineStatement) {
                define(static_cast<const DefineStatementNode&>(*m).identifier);
            } else if (m->getType() == ASTNodeType::DeriveStatement) {
                define(static_cast<const DeriveStatementNode&>(*m).identifier);
            } else if (m->getType() == ASTNodeType::FunctionDeclaration) {
                const auto& fn = static_cast<const FunctionDeclarationNode&>(*m);
                defineFunction(fn.name, static_cast<int>(fn.parameters.size()));
            }
        }
        visitBlock(n.members);
        popScope();
        return;
    }
    case ASTNodeType::SubscribeStatement: {
        const auto& n = static_cast<const SubscribeStatementNode&>(node);
        if (!isDefined(n.functionName))
            reportError("subscribe references undefined function '" + n.functionName + "'");
        if (!isDefined(n.variableName))
            reportError("subscribe references undefined variable '" + n.variableName + "'");
        return;
    }
    case ASTNodeType::UnsubscribeStatement: {
        const auto& n = static_cast<const UnsubscribeStatementNode&>(node);
        if (!isDefined(n.functionName))
            reportError("unsubscribe references undefined function '" + n.functionName + "'");
        if (!isDefined(n.variableName))
            reportError("unsubscribe references undefined variable '" + n.variableName + "'");
        return;
    }
    case ASTNodeType::StoreStatement: {
        const auto& n = static_cast<const StoreStatementNode&>(node);
        if (!isDefined(n.variableName))
            reportError("store references undefined variable '" + n.variableName + "'");
        return;
    }
    case ASTNodeType::ImportStatement:
    case ASTNodeType::BreakStatement:
    case ASTNodeType::ContinueStatement:
    case ASTNodeType::NoOp:
        return;
    }
}

void JtmlLinter::visitElement(const JtmlElementNode& elem) {
    if (elem.tagName == "img" && !hasAttr(elem, "alt") && !attrIsTruthy(elem, "aria-hidden")) {
        reportWarning("media accessibility: image elements need alt text or aria-hidden");
    }
    if (elem.tagName == "iframe" && !hasAttr(elem, "title") && !hasAttr(elem, "aria-label")) {
        reportWarning("media accessibility: embedded frames need a title or aria-label");
    }
    if ((elem.tagName == "video" || elem.tagName == "audio") &&
        !hasAttr(elem, "controls") && !hasAccessibleName(elem)) {
        reportWarning("media accessibility: " + elem.tagName +
                      " without controls should provide an accessible custom controller");
    }
    if ((elem.tagName == "canvas" || elem.tagName == "svg") && !hasAccessibleName(elem)) {
        reportWarning("media accessibility: " + elem.tagName +
                      " needs aria-label, title, or role presentation");
    }
    if (isFileInput(elem)) {
        if (!hasAttr(elem, "accept")) {
            reportWarning("media input: file inputs should declare an accept attribute");
        }
        if (!hasAccessibleName(elem)) {
            reportWarning("media accessibility: file inputs and dropzones need aria-label or title");
        }
    }
    if (isScene3d(elem)) {
        if (!hasAttr(elem, "width") || !hasAttr(elem, "height")) {
            reportWarning("media 3d: scene3d canvases should declare width and height for stable rendering");
        }
        const std::string renderer = attrLiteral(elem, "data-jtml-renderer");
        if (!renderer.empty()) {
            static const std::vector<std::string> known = {
                "auto", "three", "babylon", "webgpu", "webgl", "custom", "native"
            };
            if (std::find(known.begin(), known.end(), renderer) == known.end()) {
                reportWarning("media 3d: unknown scene3d renderer '" + renderer +
                              "'; expected auto, three, babylon, webgpu, webgl, custom, or native");
            }
        }
    }

    for (const auto& attr : elem.attributes) {
        if (!attr.value) continue;
        const int implicitArgs = implicitEventArgumentCount(attr.key);
        if (implicitArgs > 0 &&
            attr.value->getExprType() == ExpressionStatementNodeType::FunctionCall) {
            const auto& call = static_cast<const FunctionCallExpressionStatementNode&>(*attr.value);
            if (!isDefined(call.functionName)) {
                reportError("call to undefined function '" + call.functionName + "'");
            } else {
                const int expected = lookupFunctionArity(call.functionName);
                const int got = static_cast<int>(call.arguments.size()) + implicitArgs;
                if (expected >= 0 && expected != got) {
                    reportError("function '" + call.functionName + "' expects " +
                                std::to_string(expected) + " argument(s), got " +
                                std::to_string(got));
                }
            }
            for (const auto& a : call.arguments) if (a) visitExpr(*a);
        } else {
            visitExpr(*attr.value);
        }
    }
    pushScope();
    visitBlock(elem.content);
    popScope();
}

// ---------------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------------
void JtmlLinter::visitExpr(const ExpressionStatementNode& expr) {
    switch (expr.getExprType()) {
    case ExpressionStatementNodeType::Variable: {
        const auto& e = static_cast<const VariableExpressionStatementNode&>(expr);
        if (!isDefined(e.name))
            reportError("undefined variable '" + e.name + "'");
        return;
    }
    case ExpressionStatementNodeType::Binary: {
        const auto& e = static_cast<const BinaryExpressionStatementNode&>(expr);
        if (e.left)  visitExpr(*e.left);
        if (e.right) visitExpr(*e.right);
        return;
    }
    case ExpressionStatementNodeType::Unary: {
        const auto& e = static_cast<const UnaryExpressionStatementNode&>(expr);
        if (e.right) visitExpr(*e.right);
        return;
    }
    case ExpressionStatementNodeType::ArrayLiteral: {
        const auto& e = static_cast<const ArrayLiteralExpressionStatementNode&>(expr);
        for (const auto& el : e.elements) if (el) visitExpr(*el);
        return;
    }
    case ExpressionStatementNodeType::DictionaryLiteral: {
        const auto& e = static_cast<const DictionaryLiteralExpressionStatementNode&>(expr);
        for (const auto& en : e.entries) if (en.value) visitExpr(*en.value);
        return;
    }
    case ExpressionStatementNodeType::Subscript: {
        const auto& e = static_cast<const SubscriptExpressionStatementNode&>(expr);
        if (e.base)  visitExpr(*e.base);
        if (e.index) visitExpr(*e.index);
        return;
    }
    case ExpressionStatementNodeType::FunctionCall: {
        const auto& e = static_cast<const FunctionCallExpressionStatementNode&>(expr);
        if (!isDefined(e.functionName)) {
            reportError("call to undefined function '" + e.functionName + "'");
        } else {
            const int expected = lookupFunctionArity(e.functionName);
            const int got = static_cast<int>(e.arguments.size());
            if (expected >= 0 && expected != got) {
                reportError("function '" + e.functionName + "' expects " +
                            std::to_string(expected) + " argument(s), got " +
                            std::to_string(got));
            }
        }
        for (const auto& a : e.arguments) if (a) visitExpr(*a);
        return;
    }
    case ExpressionStatementNodeType::Conditional: {
        const auto& e = static_cast<const ConditionalExpressionStatementNode&>(expr);
        if (e.condition) visitExpr(*e.condition);
        if (e.whenTrue)  visitExpr(*e.whenTrue);
        if (e.whenFalse) visitExpr(*e.whenFalse);
        return;
    }
    case ExpressionStatementNodeType::ObjectPropertyAccess: {
        const auto& e = static_cast<const ObjectPropertyAccessExpressionNode&>(expr);
        if (e.base) visitExpr(*e.base);
        return;
    }
    case ExpressionStatementNodeType::ObjectMethodCall: {
        const auto& e = static_cast<const ObjectMethodCallExpressionNode&>(expr);
        if (e.base) visitExpr(*e.base);
        for (const auto& a : e.arguments) if (a) visitExpr(*a);
        return;
    }
    case ExpressionStatementNodeType::StringLiteral:
    case ExpressionStatementNodeType::NumberLiteral:
    case ExpressionStatementNodeType::BooleanLiteral:
    case ExpressionStatementNodeType::EmbeddedVariable:
    case ExpressionStatementNodeType::CompositeString:
        return;
    }
}

} // namespace jtml
