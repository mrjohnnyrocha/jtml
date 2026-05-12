// formatter.cpp — see formatter.h for the contract.
#include "jtml/formatter.h"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace jtml {

namespace {
constexpr const char* kIndentUnit = "    "; // 4 spaces

// Lookup the string name of a keyword used as a lead-in for statements.
// Kept local to this file so we don't leak formatter choices elsewhere.
} // namespace

// ---------------------------------------------------------------------------
// Public entry
// ---------------------------------------------------------------------------
std::string JtmlFormatter::format(const std::vector<std::unique_ptr<ASTNode>>& program) {
    out.str("");
    out.clear();
    indentLevel = 0;

    for (size_t i = 0; i < program.size(); ++i) {
        const auto& node = program[i];
        if (!node) continue;

        // Blank line before top-level function / object / element blocks for
        // readability, but never two consecutive blank lines.
        if (i > 0) {
            auto t = node->getType();
            if (t == ASTNodeType::FunctionDeclaration
                || t == ASTNodeType::ClassDeclaration
                || t == ASTNodeType::JtmlElement) {
                out << "\n";
            }
        }
        formatStmt(*node);
    }
    return out.str();
}

// ---------------------------------------------------------------------------
// Indentation
// ---------------------------------------------------------------------------
void JtmlFormatter::writeIndent() {
    for (int i = 0; i < indentLevel; ++i) out << kIndentUnit;
}

void JtmlFormatter::formatBlock(const std::vector<std::unique_ptr<ASTNode>>& stmts) {
    ++indentLevel;
    for (const auto& s : stmts) {
        if (s) formatStmt(*s);
    }
    --indentLevel;
}

// ---------------------------------------------------------------------------
// Statement dispatch
// ---------------------------------------------------------------------------
void JtmlFormatter::formatStmt(const ASTNode& node) {
    switch (node.getType()) {
    case ASTNodeType::JtmlElement: {
        formatElement(static_cast<const JtmlElementNode&>(node));
        return;
    }
    case ASTNodeType::ShowStatement: {
        const auto& n = static_cast<const ShowStatementNode&>(node);
        writeIndent();
        out << "show " << (n.expr ? formatExpr(*n.expr) : std::string()) << "\\\\\n";
        return;
    }
    case ASTNodeType::DefineStatement: {
        const auto& n = static_cast<const DefineStatementNode&>(node);
        writeIndent();
        out << (n.isConst ? "const " : "define ") << n.identifier;
        if (!n.declaredType.empty()) out << ": " << n.declaredType;
        out << " = "
            << (n.expression ? formatExpr(*n.expression) : std::string())
            << "\\\\\n";
        return;
    }
    case ASTNodeType::DeriveStatement: {
        const auto& n = static_cast<const DeriveStatementNode&>(node);
        writeIndent();
        out << "derive " << n.identifier;
        if (!n.declaredType.empty()) out << ": " << n.declaredType;
        out << " = "
            << (n.expression ? formatExpr(*n.expression) : std::string())
            << "\\\\\n";
        return;
    }
    case ASTNodeType::UnbindStatement: {
        const auto& n = static_cast<const UnbindStatementNode&>(node);
        writeIndent();
        out << "unbind " << n.identifier << "\\\\\n";
        return;
    }
    case ASTNodeType::StoreStatement: {
        const auto& n = static_cast<const StoreStatementNode&>(node);
        writeIndent();
        out << "store(" << n.targetScope << ") " << n.variableName << "\\\\\n";
        return;
    }
    case ASTNodeType::AssignmentStatement: {
        const auto& n = static_cast<const AssignmentStatementNode&>(node);
        writeIndent();
        out << (n.lhs ? formatExpr(*n.lhs) : std::string())
            << " = "
            << (n.rhs ? formatExpr(*n.rhs) : std::string())
            << "\\\\\n";
        return;
    }
    case ASTNodeType::ExpressionStatement: {
        const auto& n = static_cast<const ExpressionNode&>(node);
        writeIndent();
        out << (n.expression ? formatExpr(*n.expression) : std::string())
            << "\\\\\n";
        return;
    }
    case ASTNodeType::ReturnStatement: {
        const auto& n = static_cast<const ReturnStatementNode&>(node);
        writeIndent();
        out << "return";
        if (n.expr) out << " " << formatExpr(*n.expr);
        out << "\\\\\n";
        return;
    }
    case ASTNodeType::ThrowStatement: {
        const auto& n = static_cast<const ThrowStatementNode&>(node);
        writeIndent();
        out << "throw";
        if (n.expression) out << " " << formatExpr(*n.expression);
        out << "\\\\\n";
        return;
    }
    case ASTNodeType::ImportStatement: {
        const auto& n = static_cast<const ImportStatementNode&>(node);
        writeIndent();
        out << "import " << formatString(n.path) << "\\\\\n";
        return;
    }
    case ASTNodeType::BreakStatement: {
        writeIndent();
        out << "break\\\\\n";
        return;
    }
    case ASTNodeType::ContinueStatement: {
        writeIndent();
        out << "continue\\\\\n";
        return;
    }
    case ASTNodeType::IfStatement: {
        const auto& n = static_cast<const IfStatementNode&>(node);
        writeIndent();
        out << "if (" << (n.condition ? formatExpr(*n.condition) : std::string())
            << ")\\\\\n";
        formatBlock(n.thenStatements);
        writeIndent();
        out << "\\\\\n";
        if (!n.elseStatements.empty()) {
            writeIndent();
            out << "else \\\\\n";
            formatBlock(n.elseStatements);
            writeIndent();
            out << "\\\\\n";
        }
        return;
    }
    case ASTNodeType::WhileStatement: {
        const auto& n = static_cast<const WhileStatementNode&>(node);
        writeIndent();
        out << "while (" << (n.condition ? formatExpr(*n.condition) : std::string())
            << ")\\\\\n";
        formatBlock(n.body);
        writeIndent();
        out << "\\\\\n";
        return;
    }
    case ASTNodeType::ForStatement: {
        const auto& n = static_cast<const ForStatementNode&>(node);
        writeIndent();
        out << "for (" << n.iteratorName << " in "
            << (n.iterableExpression ? formatExpr(*n.iterableExpression) : std::string());
        if (n.rangeEndExpr) {
            out << ".." << formatExpr(*n.rangeEndExpr);
        }
        out << ")\\\\\n";
        formatBlock(n.body);
        writeIndent();
        out << "\\\\\n";
        return;
    }
    case ASTNodeType::TryExceptThen: {
        const auto& n = static_cast<const TryExceptThenNode&>(node);
        writeIndent();
        out << "try \\\\\n";
        formatBlock(n.tryBlock);
        writeIndent();
        out << "\\\\\n";
        if (n.hasCatch) {
            writeIndent();
            out << "except " << n.catchIdentifier << " \\\\\n";
            formatBlock(n.catchBlock);
            writeIndent();
            out << "\\\\\n";
        }
        if (n.hasFinally) {
            writeIndent();
            out << "then \\\\\n";
            formatBlock(n.finallyBlock);
            writeIndent();
            out << "\\\\\n";
        }
        return;
    }
    case ASTNodeType::BlockStatement: {
        const auto& n = static_cast<const BlockStatementNode&>(node);
        for (const auto& s : n.statements) {
            if (s) formatStmt(*s);
        }
        return;
    }
    case ASTNodeType::FunctionDeclaration: {
        const auto& n = static_cast<const FunctionDeclarationNode&>(node);
        writeIndent();
        if (n.isAsync) out << "async ";
        out << "function " << n.name << "(";
        for (size_t i = 0; i < n.parameters.size(); ++i) {
            if (i) out << ", ";
            out << n.parameters[i].name;
        }
        out << ")\\\\\n";
        formatBlock(n.body);
        writeIndent();
        out << "\\\\\n";
        return;
    }
    case ASTNodeType::ClassDeclaration: {
        const auto& n = static_cast<const ClassDeclarationNode&>(node);
        writeIndent();
        out << "object " << n.name;
        if (!n.parentName.empty()) {
            out << " derives from " << n.parentName;
        }
        out << "\\\\\n";
        formatBlock(n.members);
        writeIndent();
        out << "\\\\\n";
        return;
    }
    case ASTNodeType::SubscribeStatement: {
        const auto& n = static_cast<const SubscribeStatementNode&>(node);
        writeIndent();
        out << "subscribe " << n.functionName << " to " << n.variableName << "\\\\\n";
        return;
    }
    case ASTNodeType::UnsubscribeStatement: {
        const auto& n = static_cast<const UnsubscribeStatementNode&>(node);
        writeIndent();
        out << "unsubscribe " << n.functionName << " from " << n.variableName << "\\\\\n";
        return;
    }
    case ASTNodeType::NoOp:
        return;
    }
}

// ---------------------------------------------------------------------------
// Elements
// ---------------------------------------------------------------------------
void JtmlFormatter::formatElement(const JtmlElementNode& elem) {
    writeIndent();
    out << "element " << elem.tagName;
    for (const auto& attr : elem.attributes) {
        out << " " << formatAttr(attr);
    }
    out << "\\\\\n";
    formatBlock(elem.content);
    writeIndent();
    out << "#\n";
}

std::string JtmlFormatter::formatAttr(const JtmlAttribute& attr) {
    // Event attribute handlers are written as bare function calls without
    // quotes, e.g. `onClick=increment()`. Non-event attribute values are
    // usually string literals (`class="foo"`, `style="..."`), which we
    // preserve as quoted strings.
    std::string v = attr.value ? formatExpr(*attr.value) : std::string();
    return attr.key + "=" + v;
}

// ---------------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------------
std::string JtmlFormatter::formatExpr(const ExpressionStatementNode& expr) const {
    switch (expr.getExprType()) {
    case ExpressionStatementNodeType::Variable: {
        const auto& e = static_cast<const VariableExpressionStatementNode&>(expr);
        return e.name;
    }
    case ExpressionStatementNodeType::StringLiteral: {
        const auto& e = static_cast<const StringLiteralExpressionStatementNode&>(expr);
        return formatString(e.value);
    }
    case ExpressionStatementNodeType::NumberLiteral: {
        const auto& e = static_cast<const NumberLiteralExpressionStatementNode&>(expr);
        return formatNumber(e.value);
    }
    case ExpressionStatementNodeType::BooleanLiteral: {
        const auto& e = static_cast<const BooleanLiteralExpressionStatementNode&>(expr);
        return e.value ? "true" : "false";
    }
    case ExpressionStatementNodeType::Binary: {
        const auto& e = static_cast<const BinaryExpressionStatementNode&>(expr);
        // Parenthesise nested binaries conservatively to preserve parse order.
        // The lexer accepts this; it matches the existing debug toString
        // convention and guarantees round-trip safety without a precedence
        // table.
        std::string l = e.left  ? formatExpr(*e.left)  : "";
        std::string r = e.right ? formatExpr(*e.right) : "";
        bool needParen =
            (e.left  && e.left->getExprType()  == ExpressionStatementNodeType::Binary)
         || (e.right && e.right->getExprType() == ExpressionStatementNodeType::Binary);
        if (needParen) return "(" + l + " " + e.op + " " + r + ")";
        return l + " " + e.op + " " + r;
    }
    case ExpressionStatementNodeType::Unary: {
        const auto& e = static_cast<const UnaryExpressionStatementNode&>(expr);
        std::string r = e.right ? formatExpr(*e.right) : "";
        // No space between unary op and operand (`-x`, `!x`).
        return e.op + r;
    }
    case ExpressionStatementNodeType::ArrayLiteral: {
        const auto& e = static_cast<const ArrayLiteralExpressionStatementNode&>(expr);
        std::ostringstream o;
        o << "[";
        for (size_t i = 0; i < e.elements.size(); ++i) {
            if (i) o << ", ";
            o << (e.elements[i] ? formatExpr(*e.elements[i]) : std::string());
        }
        o << "]";
        return o.str();
    }
    case ExpressionStatementNodeType::DictionaryLiteral: {
        const auto& e = static_cast<const DictionaryLiteralExpressionStatementNode&>(expr);
        std::ostringstream o;
        o << "{ ";
        for (size_t i = 0; i < e.entries.size(); ++i) {
            if (i) o << ", ";
            const auto& entry = e.entries[i];
            // Keys are always string-literal tokens in the current grammar.
            o << formatString(entry.key.text) << ": "
              << (entry.value ? formatExpr(*entry.value) : std::string());
        }
        o << " }";
        return o.str();
    }
    case ExpressionStatementNodeType::Subscript: {
        const auto& e = static_cast<const SubscriptExpressionStatementNode&>(expr);
        std::string b = e.base  ? formatExpr(*e.base)  : "";
        std::string i = e.index ? formatExpr(*e.index) : "";
        return b + "[" + i + "]";
    }
    case ExpressionStatementNodeType::FunctionCall: {
        const auto& e = static_cast<const FunctionCallExpressionStatementNode&>(expr);
        std::ostringstream o;
        o << e.functionName << "(";
        for (size_t i = 0; i < e.arguments.size(); ++i) {
            if (i) o << ", ";
            o << (e.arguments[i] ? formatExpr(*e.arguments[i]) : std::string());
        }
        o << ")";
        return o.str();
    }
    case ExpressionStatementNodeType::Conditional: {
        const auto& e = static_cast<const ConditionalExpressionStatementNode&>(expr);
        std::string c = e.condition ? formatExpr(*e.condition) : "";
        std::string t = e.whenTrue  ? formatExpr(*e.whenTrue)  : "";
        std::string f = e.whenFalse ? formatExpr(*e.whenFalse) : "";
        return c + " ? " + t + " : " + f;
    }
    case ExpressionStatementNodeType::ObjectPropertyAccess: {
        const auto& e = static_cast<const ObjectPropertyAccessExpressionNode&>(expr);
        return (e.base ? formatExpr(*e.base) : std::string()) + "." + e.propertyName;
    }
    case ExpressionStatementNodeType::ObjectMethodCall: {
        const auto& e = static_cast<const ObjectMethodCallExpressionNode&>(expr);
        std::ostringstream o;
        o << (e.base ? formatExpr(*e.base) : std::string()) << "." << e.methodName << "(";
        for (size_t i = 0; i < e.arguments.size(); ++i) {
            if (i) o << ", ";
            o << (e.arguments[i] ? formatExpr(*e.arguments[i]) : std::string());
        }
        o << ")";
        return o.str();
    }
    case ExpressionStatementNodeType::EmbeddedVariable:
    case ExpressionStatementNodeType::CompositeString:
        // Legacy expression types not emitted by the current parser; fall
        // back to the node's toString so no information is lost.
        return expr.toString();
    }
    return expr.toString();
}

std::string JtmlFormatter::formatString(const std::string& raw) const {
    std::string out;
    out.reserve(raw.size() + 2);
    out.push_back('"');
    for (char c : raw) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n";  break;
        case '\t': out += "\\t";  break;
        case '\r': out += "\\r";  break;
        default:   out.push_back(c);
        }
    }
    out.push_back('"');
    return out;
}

std::string JtmlFormatter::formatNumber(double v) const {
    if (std::isnan(v))      return "0"; // JTML has no NaN literal; degrade gracefully.
    if (std::isinf(v))      return v < 0 ? "-1e999" : "1e999";
    // Integers round-trip without a decimal point.
    double integerPart = 0.0;
    if (std::modf(v, &integerPart) == 0.0
        && v >= -1e15 && v <= 1e15) {
        std::ostringstream o;
        o << std::fixed << std::setprecision(0) << v;
        return o.str();
    }
    // Otherwise emit with enough precision to round-trip, trimming trailing
    // zeros that `std::fixed` would add.
    std::ostringstream o;
    o << std::setprecision(15) << v;
    std::string s = o.str();
    if (s.find('.') != std::string::npos) {
        auto lastNonZero = s.find_last_not_of('0');
        if (s[lastNonZero] == '.') --lastNonZero;
        s.erase(lastNonZero + 1);
    }
    return s;
}

} // namespace jtml
