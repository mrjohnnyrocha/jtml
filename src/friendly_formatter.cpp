// friendly_formatter.cpp — Friendly JTML v2 pretty-printer.
//
// Walks the AST and emits indentation-based Friendly syntax.
// Reverse-maps HTML tags to element dictionary aliases and
// classic keywords to friendly equivalents.
#include "jtml/friendly_formatter.h"

#include <cmath>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>

namespace {
constexpr const char* kIndentUnit = "  "; // 2 spaces for friendly (more readable)

// Reverse map from HTML tag → friendly alias.
const std::map<std::string, std::string>& reverseTagMap() {
    static const std::map<std::string, std::string> m = {
        {"p",      "text"},
        {"div",    "box"},
        {"a",      "link"},
        {"img",    "image"},
        {"iframe", "embed"},
        {"ul",     "list"},
        {"ol",     "list"},
        {"li",     "item"},
    };
    return m;
}

// Reverse map from classic event attribute → friendly event name.
const std::map<std::string, std::string>& reverseEventMap() {
    static const std::map<std::string, std::string> m = {
        {"onClick",     "click"},
        {"onInput",     "input"},
        {"onChange",     "change"},
        {"onKeyUp",     "keyup"},
        {"onKeyDown",   "keydown"},
        {"onMouseOver", "hover"},
        {"onScroll",    "scroll"},
        {"onSubmit",    "submit"},
    };
    return m;
}

std::string unquoteFormatted(const std::string& value) {
    if (value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\''))) {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

// Tags that pass through (keep their HTML name in friendly syntax).
const std::set<std::string>& passThruTags() {
    static const std::set<std::string> s = {
        "main", "h1", "h2", "h3", "h4", "h5", "h6",
        "span", "strong", "em", "code", "pre", "blockquote",
        "section", "nav", "header", "footer", "article", "aside",
        "form", "button", "input", "textarea", "select", "option",
        "table", "thead", "tbody", "tr", "th", "td",
        "video", "audio", "canvas", "svg", "rect", "circle", "ellipse",
        "line", "path", "polyline", "polygon", "g", "defs", "title",
        "desc", "label",
        "br", "hr",
    };
    return s;
}

// Boolean attributes that can be emitted as bare keywords.
const std::set<std::string>& booleanAttrs() {
    static const std::set<std::string> s = {
        "required", "disabled", "checked", "selected", "autofocus",
        "multiple", "readonly", "hidden", "controls", "autoplay",
        "loop", "muted",
    };
    return s;
}

// Check if a string looks like a synthesized setter function.
// These have the pattern: function setFoo(value)
bool isSynthesizedSetter(const FunctionDeclarationNode& fn) {
    if (fn.name.size() < 4) return false;
    if (fn.name.substr(0, 3) != "set") return false;
    if (!std::isupper(fn.name[3])) return false;
    if (fn.parameters.size() != 1) return false;
    if (fn.parameters[0].name != "value") return false;
    // Body should be a single assignment: variable = value
    if (fn.body.size() != 1) return false;
    return fn.body[0] && fn.body[0]->getType() == ASTNodeType::AssignmentStatement;
}

// Extract the variable name from a setter function name.
// "setEmail" → "email", "setUserName" → "userName"
std::string setterToVariable(const std::string& setterName) {
    if (setterName.size() < 4) return setterName;
    std::string var;
    var += static_cast<char>(std::tolower(setterName[3]));
    var += setterName.substr(4);
    return var;
}

} // namespace

// ---------------------------------------------------------------------------
// Public entry
// ---------------------------------------------------------------------------
std::string JtmlFriendlyFormatter::format(
    const std::vector<std::unique_ptr<ASTNode>>& program) {
    out.str("");
    out.clear();
    indentLevel = 0;

    out << "jtml 2\n";

    // Collect setter function names to suppress them from output.
    // They'll be re-synthesized from `into` bindings.
    std::set<std::string> suppressedSetters;
    for (const auto& node : program) {
        if (!node) continue;
        if (node->getType() == ASTNodeType::FunctionDeclaration) {
            const auto& fn = static_cast<const FunctionDeclarationNode&>(*node);
            if (isSynthesizedSetter(fn)) {
                suppressedSetters.insert(fn.name);
            }
        }
    }

    bool lastWasBlank = true; // suppress leading blank line after header
    for (size_t i = 0; i < program.size(); ++i) {
        const auto& node = program[i];
        if (!node) continue;

        // Skip synthesized setter functions.
        if (node->getType() == ASTNodeType::FunctionDeclaration) {
            const auto& fn = static_cast<const FunctionDeclarationNode&>(*node);
            if (suppressedSetters.count(fn.name)) continue;
        }

        // Blank line before top-level function/element/class for readability.
        auto t = node->getType();
        if (!lastWasBlank && (t == ASTNodeType::FunctionDeclaration
            || t == ASTNodeType::ClassDeclaration
            || t == ASTNodeType::JtmlElement)) {
            out << "\n";
        }

        formatStmt(*node);
        lastWasBlank = false;
    }
    return out.str();
}

// ---------------------------------------------------------------------------
// Indentation
// ---------------------------------------------------------------------------
void JtmlFriendlyFormatter::writeIndent() {
    for (int i = 0; i < indentLevel; ++i) out << kIndentUnit;
}

void JtmlFriendlyFormatter::formatBlock(
    const std::vector<std::unique_ptr<ASTNode>>& stmts) {
    ++indentLevel;
    for (const auto& s : stmts) {
        if (s) formatStmt(*s);
    }
    --indentLevel;
}

// ---------------------------------------------------------------------------
// Tag reverse-mapping
// ---------------------------------------------------------------------------
std::string JtmlFriendlyFormatter::friendlyTagName(
    const std::string& htmlTag) const {
    auto it = reverseTagMap().find(htmlTag);
    if (it != reverseTagMap().end()) return it->second;
    return htmlTag;
}

std::string JtmlFriendlyFormatter::friendlyEventName(
    const std::string& attrKey) const {
    auto it = reverseEventMap().find(attrKey);
    if (it != reverseEventMap().end()) return it->second;
    return "";
}

// ---------------------------------------------------------------------------
// Statement dispatch
// ---------------------------------------------------------------------------
void JtmlFriendlyFormatter::formatStmt(const ASTNode& node) {
    switch (node.getType()) {
    case ASTNodeType::JtmlElement: {
        formatElement(static_cast<const JtmlElementNode&>(node));
        return;
    }
    case ASTNodeType::ShowStatement: {
        const auto& n = static_cast<const ShowStatementNode&>(node);
        writeIndent();
        out << "show " << (n.expr ? formatExpr(*n.expr) : std::string()) << "\n";
        return;
    }
    case ASTNodeType::DefineStatement: {
        const auto& n = static_cast<const DefineStatementNode&>(node);
        writeIndent();
        if (n.isConst) {
            out << "const " << n.identifier;
            if (!n.declaredType.empty()) out << ": " << n.declaredType;
            out << " = "
                << (n.expression ? formatExpr(*n.expression) : std::string())
                << "\n";
        } else {
            out << "let " << n.identifier;
            if (!n.declaredType.empty()) out << ": " << n.declaredType;
            out << " = "
                << (n.expression ? formatExpr(*n.expression) : std::string())
                << "\n";
        }
        return;
    }
    case ASTNodeType::DeriveStatement: {
        const auto& n = static_cast<const DeriveStatementNode&>(node);
        writeIndent();
        out << "get " << n.identifier;
        if (!n.declaredType.empty()) out << ": " << n.declaredType;
        out << " = "
            << (n.expression ? formatExpr(*n.expression) : std::string())
            << "\n";
        return;
    }
    case ASTNodeType::AssignmentStatement: {
        const auto& n = static_cast<const AssignmentStatementNode&>(node);
        writeIndent();
        out << "let "
            << (n.lhs ? formatExpr(*n.lhs) : std::string())
            << " = "
            << (n.rhs ? formatExpr(*n.rhs) : std::string())
            << "\n";
        return;
    }
    case ASTNodeType::ExpressionStatement: {
        const auto& n = static_cast<const ExpressionNode&>(node);
        writeIndent();
        out << (n.expression ? formatExpr(*n.expression) : std::string())
            << "\n";
        return;
    }
    case ASTNodeType::ReturnStatement: {
        const auto& n = static_cast<const ReturnStatementNode&>(node);
        writeIndent();
        out << "return";
        if (n.expr) out << " " << formatExpr(*n.expr);
        out << "\n";
        return;
    }
    case ASTNodeType::ThrowStatement: {
        const auto& n = static_cast<const ThrowStatementNode&>(node);
        writeIndent();
        out << "throw";
        if (n.expression) out << " " << formatExpr(*n.expression);
        out << "\n";
        return;
    }
    case ASTNodeType::ImportStatement: {
        const auto& n = static_cast<const ImportStatementNode&>(node);
        writeIndent();
        out << "use " << formatString(n.path) << "\n";
        return;
    }
    case ASTNodeType::BreakStatement: {
        writeIndent();
        out << "break\n";
        return;
    }
    case ASTNodeType::ContinueStatement: {
        writeIndent();
        out << "continue\n";
        return;
    }
    case ASTNodeType::IfStatement: {
        const auto& n = static_cast<const IfStatementNode&>(node);
        writeIndent();
        out << "if " << (n.condition ? formatExpr(*n.condition) : std::string())
            << "\n";
        formatBlock(n.thenStatements);
        if (!n.elseStatements.empty()) {
            writeIndent();
            out << "else\n";
            formatBlock(n.elseStatements);
        }
        return;
    }
    case ASTNodeType::WhileStatement: {
        const auto& n = static_cast<const WhileStatementNode&>(node);
        writeIndent();
        out << "while " << (n.condition ? formatExpr(*n.condition) : std::string())
            << "\n";
        formatBlock(n.body);
        return;
    }
    case ASTNodeType::ForStatement: {
        const auto& n = static_cast<const ForStatementNode&>(node);
        writeIndent();
        out << "for " << n.iteratorName << " in "
            << (n.iterableExpression ? formatExpr(*n.iterableExpression) : std::string());
        if (n.rangeEndExpr) {
            out << ".." << formatExpr(*n.rangeEndExpr);
        }
        out << "\n";
        formatBlock(n.body);
        return;
    }
    case ASTNodeType::TryExceptThen: {
        const auto& n = static_cast<const TryExceptThenNode&>(node);
        writeIndent();
        out << "try\n";
        formatBlock(n.tryBlock);
        if (n.hasCatch) {
            writeIndent();
            out << "catch " << n.catchIdentifier << "\n";
            formatBlock(n.catchBlock);
        }
        if (n.hasFinally) {
            writeIndent();
            out << "finally\n";
            formatBlock(n.finallyBlock);
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
        out << "when " << n.name;
        for (size_t i = 0; i < n.parameters.size(); ++i) {
            out << " " << n.parameters[i].name;
        }
        out << "\n";
        formatBlock(n.body);
        return;
    }
    case ASTNodeType::ClassDeclaration: {
        // Classes don't have a friendly equivalent yet; emit as comments.
        const auto& n = static_cast<const ClassDeclarationNode&>(node);
        writeIndent();
        out << "// object " << n.name;
        if (!n.parentName.empty()) {
            out << " derives from " << n.parentName;
        }
        out << "\n";
        writeIndent();
        out << "// (class declarations are not yet supported in friendly syntax)\n";
        return;
    }
    case ASTNodeType::UnbindStatement: {
        const auto& n = static_cast<const UnbindStatementNode&>(node);
        writeIndent();
        out << "// unbind " << n.identifier << "\n";
        return;
    }
    case ASTNodeType::StoreStatement: {
        const auto& n = static_cast<const StoreStatementNode&>(node);
        writeIndent();
        out << "// store(" << n.targetScope << ") " << n.variableName << "\n";
        return;
    }
    case ASTNodeType::SubscribeStatement: {
        const auto& n = static_cast<const SubscribeStatementNode&>(node);
        writeIndent();
        out << "// subscribe " << n.functionName << " to " << n.variableName << "\n";
        return;
    }
    case ASTNodeType::UnsubscribeStatement: {
        const auto& n = static_cast<const UnsubscribeStatementNode&>(node);
        writeIndent();
        out << "// unsubscribe " << n.functionName << " from " << n.variableName << "\n";
        return;
    }
    case ASTNodeType::NoOp:
        return;
    }
}

// ---------------------------------------------------------------------------
// Elements
// ---------------------------------------------------------------------------
void JtmlFriendlyFormatter::formatElement(const JtmlElementNode& elem) {
    writeIndent();

    if (elem.tagName == "canvas") {
        bool isScene3d = false;
        std::string sceneLabel;
        std::vector<std::pair<std::string, std::string>> sceneAttrs;
        std::vector<std::pair<std::string, std::string>> passAttrs;
        for (const auto& attr : elem.attributes) {
            const std::string value = attr.value ? formatExpr(*attr.value) : "";
            if (attr.key == "data-jtml-scene3d") {
                isScene3d = true;
                continue;
            }
            if (attr.key == "role" && value == "\"img\"") continue;
            if (attr.key == "aria-label" && attr.value) {
                sceneLabel = value;
                continue;
            }
            if (attr.key == "data-jtml-scene") {
                sceneAttrs.emplace_back("scene", unquoteFormatted(value));
                continue;
            }
            if (attr.key == "data-jtml-camera") {
                sceneAttrs.emplace_back("camera", unquoteFormatted(value));
                continue;
            }
            if (attr.key == "data-jtml-controls") {
                sceneAttrs.emplace_back("controls", unquoteFormatted(value));
                continue;
            }
            if (attr.key == "data-jtml-renderer") {
                sceneAttrs.emplace_back("renderer", unquoteFormatted(value));
                continue;
            }
            if (attr.key == "data-jtml-scene3d-controller") {
                sceneAttrs.emplace_back("into", unquoteFormatted(value));
                continue;
            }
            passAttrs.emplace_back(attr.key, value);
        }
        if (isScene3d) {
            out << "scene3d";
            if (!sceneLabel.empty()) out << " " << sceneLabel;
            for (const auto& [key, val] : sceneAttrs) out << " " << key << " " << val;
            for (const auto& [key, val] : passAttrs) {
                out << " " << key;
                if (!val.empty()) out << " " << val;
            }
            out << "\n";
            return;
        }
    }

    if (elem.tagName == "svg") {
        std::string chartType;
        std::string chartData;
        std::string chartBy;
        std::string chartValue;
        std::string chartLabel;
        std::string chartColor;
        for (const auto& attr : elem.attributes) {
            if (!attr.value) continue;
            const std::string value = formatExpr(*attr.value);
            if (attr.key == "data-jtml-chart") chartType = unquoteFormatted(value);
            else if (attr.key == "data-jtml-chart-data") chartData = unquoteFormatted(value);
            else if (attr.key == "data-jtml-chart-by") chartBy = unquoteFormatted(value);
            else if (attr.key == "data-jtml-chart-value") chartValue = unquoteFormatted(value);
            else if (attr.key == "aria-label") chartLabel = value;
            else if (attr.key == "data-jtml-chart-color") chartColor = value;
        }
        if (!chartType.empty()) {
            out << "chart " << chartType;
            if (!chartData.empty()) out << " data " << chartData;
            if (!chartBy.empty()) out << " by " << chartBy;
            if (!chartValue.empty()) out << " value " << chartValue;
            if (!chartLabel.empty()) out << " label " << chartLabel;
            if (!chartColor.empty()) out << " color " << chartColor;
            out << "\n";
            return;
        }
    }

    // Root element "main" becomes "page"
    const bool isPage = (elem.tagName == "main" && indentLevel == 0);
    const std::string tag = isPage ? "page" : friendlyTagName(elem.tagName);

    out << tag;

    // Check if this is a checkbox (input with type="checkbox")
    bool isCheckbox = false;
    bool isFile = false;
    bool isDropzone = false;
    if (elem.tagName == "input") {
        for (const auto& attr : elem.attributes) {
            if (attr.key == "type" && attr.value) {
                std::string typeVal = formatExpr(*attr.value);
                if (typeVal == "\"checkbox\"") {
                    isCheckbox = true;
                } else if (typeVal == "\"file\"") {
                    isFile = true;
                }
            } else if (attr.key == "data-jtml-dropzone") {
                isDropzone = true;
            }
        }
        if (isCheckbox || isFile || isDropzone) {
            const std::string replacement = isDropzone ? "dropzone" : (isFile ? "file" : "checkbox");
            // Reset and rewrite
            std::string current = out.str();
            current = current.substr(0, current.size() - tag.size());
            out.str(current);
            out.seekp(0, std::ios_base::end);
            out << replacement;
        }
    }

    // Check for ordered list
    if (elem.tagName == "ol") {
        out << " ordered";
    }

    // Collect event bindings and into bindings separately.
    std::string intoVariable;
    std::string mediaController;
    std::vector<std::pair<std::string, std::string>> events;  // friendly name → action
    std::vector<std::pair<std::string, std::string>> attrs;   // key → value

    for (const auto& attr : elem.attributes) {
        // Skip implicit attrs for element aliases.
        if ((isCheckbox || isFile || isDropzone) && attr.key == "type") continue;
        if (isDropzone && attr.key == "data-jtml-dropzone") continue;
        if (isDropzone && attr.key == "multiple") continue;
        if ((elem.tagName == "video" || elem.tagName == "audio") &&
            attr.key == "data-jtml-media-controller" && attr.value) {
            mediaController = formatExpr(*attr.value);
            if (mediaController.size() >= 2 &&
                ((mediaController.front() == '"' && mediaController.back() == '"') ||
                 (mediaController.front() == '\'' && mediaController.back() == '\''))) {
                mediaController = mediaController.substr(1, mediaController.size() - 2);
            }
            continue;
        }

        // File/dropzone `into` bindings do not carry a value attribute, but
        // their generated onChange setter still follows the setName() shape.
        if ((isFile || isDropzone) && attr.key == "onChange" && attr.value) {
            std::string action = formatExpr(*attr.value);
            if (action.size() > 2 && action.substr(action.size() - 2) == "()") {
                action = action.substr(0, action.size() - 2);
            }
            if (action.size() >= 4 && action.rfind("set", 0) == 0 &&
                std::isupper(static_cast<unsigned char>(action[3]))) {
                intoVariable = setterToVariable(action);
                continue;
            }
        }

        // Check for event handler.
        std::string friendlyEvt = friendlyEventName(attr.key);
        if (!friendlyEvt.empty()) {
            std::string action = attr.value ? formatExpr(*attr.value) : "";
            // Strip trailing "()" from action for friendly syntax.
            if (action.size() >= 2 && action.substr(action.size() - 2) == "()") {
                action = action.substr(0, action.size() - 2);
            }
            events.emplace_back(friendlyEvt, action);
            continue;
        }

        // Check for `value=variable` + `onInput=setVariable()` pattern → `into`.
        if (attr.key == "value" && attr.value) {
            // Look ahead for a matching setter.
            std::string varName = formatExpr(*attr.value);
            std::string expectedSetter = "set" + std::string(1, static_cast<char>(
                std::toupper(varName[0]))) + varName.substr(1);
            bool foundSetter = false;
            for (const auto& other : elem.attributes) {
                if ((other.key == "onInput" || other.key == "onChange") && other.value) {
                    std::string setter = formatExpr(*other.value);
                    if (setter == expectedSetter + "()") {
                        intoVariable = varName;
                        foundSetter = true;
                        break;
                    }
                }
            }
            if (foundSetter) continue; // Skip the value attr; into will handle it
        }

        // Skip the onInput/onChange setter that was consumed by into.
        if (!intoVariable.empty() &&
            (attr.key == "onInput" || attr.key == "onChange") && attr.value) {
            std::string setter = formatExpr(*attr.value);
            std::string expectedSetter = "set" + std::string(1, static_cast<char>(
                std::toupper(intoVariable[0]))) + intoVariable.substr(1) + "()";
            if (setter == expectedSetter) continue;
        }

        // Check for placeholder / aria-label attrs that become inline text.
        if ((isFile || isDropzone) && attr.key == "aria-label" && attr.value) {
            out << " " << formatExpr(*attr.value);
            continue;
        }
        if (attr.key == "placeholder" && attr.value) {
            out << " " << formatExpr(*attr.value);
            continue;
        }

        // Boolean attribute. HTML often serializes boolean attrs as
        // disabled="disabled"; Friendly keeps those as bare flags.
        if (booleanAttrs().count(attr.key) &&
            (!attr.value || formatExpr(*attr.value) == formatString(attr.key))) {
            attrs.emplace_back(attr.key, "");
            continue;
        }

        // Regular attribute.
        std::string val = attr.value ? formatExpr(*attr.value) : "";
        attrs.emplace_back(attr.key, val);
    }

    // Emit regular attributes.
    for (const auto& [key, val] : attrs) {
        if (val.empty()) {
            out << " " << key;
        } else {
            out << " " << key << " " << val;
        }
    }

    // Emit event bindings.
    for (const auto& [evt, action] : events) {
        out << " " << evt << " " << action;
    }

    // Emit into binding.
    if (!intoVariable.empty()) {
        out << " into " << intoVariable;
    }
    if (!mediaController.empty()) {
        out << " into " << mediaController;
    }

    out << "\n";

    // Emit body content.
    if (!elem.content.empty()) {
        ++indentLevel;
        for (const auto& child : elem.content) {
            if (child) formatStmt(*child);
        }
        --indentLevel;
    }
}

// ---------------------------------------------------------------------------
// Expressions (shared with classic formatter — same format)
// ---------------------------------------------------------------------------
std::string JtmlFriendlyFormatter::formatExpr(
    const ExpressionStatementNode& expr) const {
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
    case ExpressionStatementNodeType::Conditional: {
        const auto& e = static_cast<const ConditionalExpressionStatementNode&>(expr);
        return (e.condition ? formatExpr(*e.condition) : std::string()) +
               " ? " +
               (e.whenTrue ? formatExpr(*e.whenTrue) : std::string()) +
               " : " +
               (e.whenFalse ? formatExpr(*e.whenFalse) : std::string());
    }
    case ExpressionStatementNodeType::EmbeddedVariable:
    case ExpressionStatementNodeType::CompositeString:
        return expr.toString();
    }
    return expr.toString();
}

std::string JtmlFriendlyFormatter::formatString(const std::string& raw) const {
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

std::string JtmlFriendlyFormatter::formatNumber(double v) const {
    if (std::isnan(v))      return "0";
    if (std::isinf(v))      return v < 0 ? "-1e999" : "1e999";
    double integerPart = 0.0;
    if (std::modf(v, &integerPart) == 0.0
        && v >= -1e15 && v <= 1e15) {
        std::ostringstream o;
        o << std::fixed << std::setprecision(0) << v;
        return o.str();
    }
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
