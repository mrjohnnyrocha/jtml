#pragma once

#include "jtml/ast.h"
#include "json.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace jtml {

enum class ExpressionIrKind {
    Empty,
    Literal,
    Path,
    Member,
    Subscript,
    Unary,
    Binary,
    Conditional,
    Array,
    Object,
    Call,
    MethodCall,
    Source,
};

struct ExpressionIrObjectEntry {
    std::string key;
    std::unique_ptr<struct ExpressionIr> value;
};

struct ExpressionIr {
    ExpressionIrKind kind = ExpressionIrKind::Empty;
    std::string source;
    std::string op;
    std::string name;
    std::string root;
    std::string property;
    std::string callee;
    std::string method;
    nlohmann::json literal = nullptr;
    std::vector<std::string> segments;
    std::vector<std::unique_ptr<ExpressionIr>> children;
    std::vector<ExpressionIrObjectEntry> entries;
    int astKind = -1;
    bool supported = true;
};

std::unique_ptr<ExpressionIr> buildExpressionIr(const ExpressionStatementNode* expression);
std::unique_ptr<ExpressionIr> parseExpressionIr(const std::string& expression);

std::vector<std::string> expressionIrDependencies(const ExpressionIr& ir);
nlohmann::json expressionIrToRuntimePlanJson(const ExpressionIr& ir);

bool expressionIrSupportsDirectJs(const ExpressionIr& ir);
std::string expressionIrToJsExpression(const ExpressionIr& ir);

} // namespace jtml
