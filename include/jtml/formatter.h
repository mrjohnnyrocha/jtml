// formatter.h
//
// Canonical pretty-printer for JTML. Walks a parsed AST and emits source
// that:
//   - is idempotent: fmt(fmt(x)) == fmt(x)
//   - is re-parsable: the lexer + parser accept the output unchanged
//   - uses 4-space indentation
//   - terminates every statement with `\\` on its own line
//   - opens elements with `element tag attrs\\` and closes with `#` at the
//     parent's indentation level
//   - closes function / if / while / for / try bodies with `\\` at the
//     parent's indentation level
//
// The existing `toString()` methods on AST nodes are debug dumps, not
// canonical source. They are intentionally NOT reused here.
#pragma once

#include "jtml/ast.h"
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace jtml {

class JtmlFormatter {
public:
    // Format an entire parsed program.
    std::string format(const std::vector<std::unique_ptr<ASTNode>>& program);

private:
    std::ostringstream out;
    int indentLevel = 0;

    void writeIndent();
    void formatStmt(const ASTNode& node);
    void formatBlock(const std::vector<std::unique_ptr<ASTNode>>& stmts);
    void formatElement(const JtmlElementNode& elem);
    std::string formatAttr(const JtmlAttribute& attr);

    // Returns the canonical text of an expression, without a trailing
    // newline or statement terminator.
    std::string formatExpr(const ExpressionStatementNode& expr) const;
    std::string formatString(const std::string& raw) const;
    std::string formatNumber(double v) const;
};

} // namespace jtml
