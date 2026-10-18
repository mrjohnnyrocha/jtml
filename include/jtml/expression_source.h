#ifndef JTML_EXPRESSION_SOURCE_H
#define JTML_EXPRESSION_SOURCE_H

#include "jtml/ast.h"

#include <string>

namespace jtml {

std::string expressionSource(const ExpressionStatementNode* expr);
// Decode JTML quoted source using JSON escapes, plus single-quoted strings.
std::string decodeStringLiteral(const std::string& source);
std::string jsonString(const std::string& value);

} // namespace jtml

#endif // JTML_EXPRESSION_SOURCE_H
