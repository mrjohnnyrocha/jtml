// linter.h
//
// Static analyser for JTML programs. Walks the parsed AST and reports
// simple, high-signal issues before the code runs:
//
//   - references to undefined variables (shadowed by `for` bindings,
//     `function` parameters, `define`/`const`/`derive`, subscribe etc.)
//   - duplicate `define` / `const` / `derive` for the same name in the
//     same scope
//   - derived values whose expression depends on a name that does not
//     exist (caught together with undefined references)
//   - unreachable code after `return`, `throw`, `break`, `continue`
//
// The linter is deliberately limited and non-type-aware; it is meant to
// catch obvious mistakes before `jtml serve` starts the interpreter.
#pragma once

#include "jtml/ast.h"
#include "jtml/diagnostic.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

struct LintDiagnostic {
    enum class Severity { Warning, Error };
    Severity severity = Severity::Warning;
    std::string message;
    std::string code;
    int line = 0;
    int column = 0;
    std::string hint;
    std::string example;
};

namespace jtml {

class JtmlLinter {
public:
    // Optional. If set, `use` / `import` statements are resolved relative to
    // this path so cross-file references stop reporting as undefined. The
    // path should be the file whose program is about to be linted.
    void setSourcePath(const std::string& path);

    // Lint an entire parsed program. Returns a list of diagnostics in the
    // order they were discovered. An empty vector means the program passes.
    std::vector<LintDiagnostic> lint(const std::vector<std::unique_ptr<ASTNode>>& program);

private:
    // One scope per block / function / element body. `back()` is the
    // innermost.
    std::vector<std::vector<std::string>> scopes;
    std::vector<std::map<std::string, std::string>> typeScopes;
    std::vector<std::map<std::string, int>> functionArityScopes;
    std::vector<LintDiagnostic> diagnostics;
    std::string sourcePath_;
    int elementDepth_ = 0;
    // Canonical absolute paths already pulled into the module graph, so we
    // never reparse the same file or recurse forever on a cycle.
    std::vector<std::string> importedFiles_;
    void declareTopLevel(const std::vector<std::unique_ptr<ASTNode>>& program);
    void resolveImport(const std::string& rawPath);

    void pushScope();
    void popScope();
    bool isDefined(const std::string& name) const;
    bool isDefinedInCurrentScope(const std::string& name) const;
    void define(const std::string& name, const std::string& declaredType = "");
    void defineFunction(const std::string& name, int arity);
    std::string lookupType(const std::string& name) const;
    int lookupFunctionArity(const std::string& name) const;
    std::string inferExprType(const ExpressionStatementNode& expr) const;
    void checkDeclaredType(const std::string& name,
                           const std::string& declaredType,
                           const ExpressionStatementNode* expr);

    void visitStmt(const ASTNode& node);
    void visitBlock(const std::vector<std::unique_ptr<ASTNode>>& stmts);
    void visitElement(const JtmlElementNode& elem);
    void visitExpr(const ExpressionStatementNode& expr);

    void reportError(const std::string& msg);
    void reportWarning(const std::string& msg);
};

} // namespace jtml
