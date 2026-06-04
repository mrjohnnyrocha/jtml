// friendly_formatter.h
//
// AST-driven pretty-printer that emits Friendly JTML v2 syntax.
// Walks the same AST as JtmlFormatter but produces indentation-based
// output using v2 keywords: `let`, `when`, `page`, `show`, etc.
//
// Reverse-maps HTML tags to element dictionary aliases where applicable
// (e.g. `div` → `box`, `p` → `text`, `ul`/`ol` → `list`, etc.).
//
// The output includes a `jtml 2` header and is idempotent:
//   friendlyFmt(friendlyFmt(x)) == friendlyFmt(x)
#pragma once

#include "jtml/ast.h"
#include <memory>
#include <sstream>
#include <string>
#include <vector>

class JtmlFriendlyFormatter {
public:
    // Format an entire parsed program as Friendly JTML v2.
    std::string format(const std::vector<std::unique_ptr<ASTNode>>& program);

private:
    std::ostringstream out;
    int indentLevel = 0;

    void writeIndent();
    void formatStmt(const ASTNode& node);
    void formatBlock(const std::vector<std::unique_ptr<ASTNode>>& stmts);
    void formatElement(const JtmlElementNode& elem);

    // Reverse-map an HTML tag name to a friendly alias (e.g. "div" → "box").
    std::string friendlyTagName(const std::string& htmlTag) const;

    // Detect if an element attribute is an event handler and return the
    // friendly event name (e.g. "onClick" → "click"). Returns empty if not.
    std::string friendlyEventName(const std::string& attrKey) const;

    // Format a single attribute for friendly output.
    // Returns empty string for attributes that are handled inline
    // (events, type="checkbox" on checkboxes, etc.).
    std::string formatFriendlyAttr(const JtmlAttribute& attr,
                                   const std::string& friendlyTag) const;

    std::string formatExpr(const ExpressionStatementNode& expr) const;
    std::string formatString(const std::string& raw) const;
    std::string formatNumber(double v) const;
};
