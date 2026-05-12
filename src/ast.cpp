#include "jtml/ast.h"   // Adjust the path if needed
#include "jtml/lexer.h"   // Full definition of 'Token'

#include <utility> // for std::move

// ------------------- Expression Nodes Implementations -------------------

BinaryExpressionStatementNode::BinaryExpressionStatementNode(const Token& opToken,
                                                             std::unique_ptr<ExpressionStatementNode> l,
                                                             std::unique_ptr<ExpressionStatementNode> r)
    : op(opToken.text),
      left(std::move(l)),
      right(std::move(r)) {}

ExpressionStatementNodeType BinaryExpressionStatementNode::getExprType() const {
    return ExpressionStatementNodeType::Binary;
}

std::unique_ptr<ExpressionStatementNode> BinaryExpressionStatementNode::clone() const {
    return std::make_unique<BinaryExpressionStatementNode>(
        Token{getTokenTypeForOperator(op), op, 0, 0, 0},
        left ? left->clone() : nullptr,
        right ? right->clone() : nullptr
    );
}

std::string BinaryExpressionStatementNode::toString() const {
    return "(" + left->toString() + " " + op + " " + right->toString() + ")";
}

UnaryExpressionStatementNode::UnaryExpressionStatementNode(const Token& opToken,
                                                           std::unique_ptr<ExpressionStatementNode> r)
    : op(opToken.text),
      right(std::move(r)) {}


ExpressionStatementNodeType UnaryExpressionStatementNode::getExprType() const {
    return ExpressionStatementNodeType::Unary;
}
std::unique_ptr<ExpressionStatementNode> UnaryExpressionStatementNode::clone() const {
    return std::make_unique<UnaryExpressionStatementNode>(
        Token{getTokenTypeForOperator(op), op, 0, 0, 0},
        right ? right->clone() : nullptr
    );
}

std::string UnaryExpressionStatementNode::toString() const {
    return "(" + op + " " + right->toString() + ")";
}
VariableExpressionStatementNode::VariableExpressionStatementNode(const Token& varToken)
    : name(varToken.text) {}

ExpressionStatementNodeType VariableExpressionStatementNode::getExprType() const {
    return ExpressionStatementNodeType::Variable;
}

std::unique_ptr<ExpressionStatementNode> VariableExpressionStatementNode::clone() const {
    // Recreate a Token for the variable name
    Token token{TokenType::IDENTIFIER, name, 0, 0, 0}; // Dummy position, line, column
    return std::make_unique<VariableExpressionStatementNode>(token);
}

std::string VariableExpressionStatementNode::toString() const {
    return name;
}

StringLiteralExpressionStatementNode::StringLiteralExpressionStatementNode(const Token& strToken)
    : value(strToken.text) {}

ExpressionStatementNodeType StringLiteralExpressionStatementNode::getExprType() const {
    return ExpressionStatementNodeType::StringLiteral;
}

std::unique_ptr<ExpressionStatementNode> StringLiteralExpressionStatementNode::clone() const {
    // Recreate a Token for the string literal
    Token token{TokenType::STRING_LITERAL, value, 0, 0, 0}; // Dummy position, line, column
    return std::make_unique<StringLiteralExpressionStatementNode>(token);
}

std::string StringLiteralExpressionStatementNode::toString() const {
    return value;
}

// ------------------- EmbeddedVariableExpressionStatementNode -------------------

EmbeddedVariableExpressionStatementNode::EmbeddedVariableExpressionStatementNode(
    std::unique_ptr<ExpressionStatementNode> expr)
    : embeddedExpression(std::move(expr)) {}

ExpressionStatementNodeType EmbeddedVariableExpressionStatementNode::getExprType() const {
    return ExpressionStatementNodeType::EmbeddedVariable;
}

std::unique_ptr<ExpressionStatementNode> EmbeddedVariableExpressionStatementNode::clone() const {
    return std::make_unique<EmbeddedVariableExpressionStatementNode>(
        embeddedExpression ? embeddedExpression->clone() : nullptr);
}

std::string EmbeddedVariableExpressionStatementNode::toString() const {
    return "#(" + (embeddedExpression ? embeddedExpression->toString() : "") + ")";
}

// ------------------- CompositeStringExpressionStatementNode -------------------

CompositeStringExpressionStatementNode::CompositeStringExpressionStatementNode(
    std::vector<std::unique_ptr<ExpressionStatementNode>> p)
    : parts(std::move(p)) {}

ExpressionStatementNodeType CompositeStringExpressionStatementNode::getExprType() const {
    return ExpressionStatementNodeType::CompositeString;
}

std::unique_ptr<ExpressionStatementNode> CompositeStringExpressionStatementNode::clone() const {
    std::vector<std::unique_ptr<ExpressionStatementNode>> clonedParts;
    for (const auto& part : parts) {
        clonedParts.push_back(part->clone());
    }
    return std::make_unique<CompositeStringExpressionStatementNode>(std::move(clonedParts));
}

std::string CompositeStringExpressionStatementNode::toString() const {
    std::ostringstream oss;
    for (const auto& part : parts) {
        oss << part->toString();
    }
    return oss.str();
}

std::unique_ptr<ExpressionStatementNode> CompositeStringExpressionStatementNode::optimize() const {
    std::vector<std::unique_ptr<ExpressionStatementNode>> optimizedParts;
    std::string accumulatedString;

    for (const auto& part : parts) {
        if (part->getExprType() == ExpressionStatementNodeType::StringLiteral) {
            accumulatedString +=
                static_cast<StringLiteralExpressionStatementNode*>(part.get())->value;
        } else {
            if (!accumulatedString.empty()) {
                optimizedParts.push_back(std::make_unique<StringLiteralExpressionStatementNode>(
                    Token{TokenType::STRING_LITERAL, accumulatedString}));
                accumulatedString.clear();
            }
            optimizedParts.push_back(part->clone());
        }
    }

    if (!accumulatedString.empty()) {
        optimizedParts.push_back(std::make_unique<StringLiteralExpressionStatementNode>(
            Token{TokenType::STRING_LITERAL, accumulatedString}));
    }

    if (optimizedParts.size() == 1) {
        return std::move(optimizedParts[0]);
    }
    return std::make_unique<CompositeStringExpressionStatementNode>(std::move(optimizedParts));
}
NumberLiteralExpressionStatementNode::NumberLiteralExpressionStatementNode(const Token& numToken) {
    try {
        value = std::stod(numToken.text);
    }
    catch (const std::invalid_argument& e) {
        throw std::runtime_error("Invalid number format: " + numToken.text);
    }
    catch (const std::out_of_range& e) {
        throw std::runtime_error("Number out of range: " + numToken.text);
    }
}
ExpressionStatementNodeType NumberLiteralExpressionStatementNode::getExprType() const {
    return ExpressionStatementNodeType::NumberLiteral;
}

std::unique_ptr<ExpressionStatementNode> NumberLiteralExpressionStatementNode::clone() const {
    // Convert the double value back to a string with proper formatting
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(15) << value; // Use high precision to preserve the exact value
    std::string valueStr = oss.str();

    // Recreate a Token for the number literal
    Token token{TokenType::NUMBER_LITERAL, valueStr, 0, 0, 0}; // Dummy position, line, column

    return std::make_unique<NumberLiteralExpressionStatementNode>(token);
}
std::string NumberLiteralExpressionStatementNode::toString() const {
    // Convert the double value back to a string with proper formatting
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(15) << value; // Use high precision to preserve the exact value
    std::string valueStr = oss.str();


    return valueStr;
}

BooleanLiteralExpressionStatementNode::BooleanLiteralExpressionStatementNode(bool val)
    : value(val) {}

ExpressionStatementNodeType BooleanLiteralExpressionStatementNode::getExprType() const {
    return ExpressionStatementNodeType::BooleanLiteral;
}

std::unique_ptr<ExpressionStatementNode> BooleanLiteralExpressionStatementNode::clone() const {
    return std::make_unique<BooleanLiteralExpressionStatementNode>(value);
}

std::string BooleanLiteralExpressionStatementNode::toString() const {
    return value ? "true" : "false";
}

ConditionalExpressionStatementNode::ConditionalExpressionStatementNode(
    std::unique_ptr<ExpressionStatementNode> conditionExpr,
    std::unique_ptr<ExpressionStatementNode> trueExpr,
    std::unique_ptr<ExpressionStatementNode> falseExpr)
    : condition(std::move(conditionExpr)),
      whenTrue(std::move(trueExpr)),
      whenFalse(std::move(falseExpr)) {}

ExpressionStatementNodeType ConditionalExpressionStatementNode::getExprType() const {
    return ExpressionStatementNodeType::Conditional;
}

std::unique_ptr<ExpressionStatementNode> ConditionalExpressionStatementNode::clone() const {
    return std::make_unique<ConditionalExpressionStatementNode>(
        condition ? condition->clone() : nullptr,
        whenTrue ? whenTrue->clone() : nullptr,
        whenFalse ? whenFalse->clone() : nullptr);
}

std::string ConditionalExpressionStatementNode::toString() const {
    return "(" +
           (condition ? condition->toString() : std::string()) +
           " ? " +
           (whenTrue ? whenTrue->toString() : std::string()) +
           " : " +
           (whenFalse ? whenFalse->toString() : std::string()) +
           ")";
}

ArrayLiteralExpressionStatementNode::ArrayLiteralExpressionStatementNode(std::vector<std::unique_ptr<ExpressionStatementNode>> elms)
    : elements(std::move(elms)) {}

ExpressionStatementNodeType ArrayLiteralExpressionStatementNode::getExprType() const {
    return ExpressionStatementNodeType::ArrayLiteral;
}

std::unique_ptr<ExpressionStatementNode> ArrayLiteralExpressionStatementNode::clone() const {
    std::vector<std::unique_ptr<ExpressionStatementNode>> clonedElements;
    clonedElements.reserve(elements.size());
    for (const auto& element : elements) {
        clonedElements.push_back(element->clone());
    }
    return std::make_unique<ArrayLiteralExpressionStatementNode>(
        std::move(clonedElements));
}

std::string ArrayLiteralExpressionStatementNode::toString() const {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < elements.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << elements[i]->toString();
    }
    oss << "]";
    return oss.str();
}

DictionaryLiteralExpressionStatementNode::DictionaryLiteralExpressionStatementNode(std::vector<DictionaryEntry> etrs)
    : entries(std::move(etrs)) {}

ExpressionStatementNodeType DictionaryLiteralExpressionStatementNode::getExprType() const {
    return ExpressionStatementNodeType::DictionaryLiteral;
}

std::unique_ptr<ExpressionStatementNode> DictionaryLiteralExpressionStatementNode::clone() const {
    std::vector<DictionaryEntry> clonedEntries;
    clonedEntries.reserve(entries.size());
    for (const auto& entry : entries) {
        DictionaryEntry newEntry;
        newEntry.key = entry.key; // Token can be copied as is
        if (entry.value) {
            newEntry.value = entry.value->clone();
        }
        clonedEntries.push_back(std::move(newEntry));
    }
    return std::make_unique<DictionaryLiteralExpressionStatementNode>(
        std::move(clonedEntries));
}

std::string DictionaryLiteralExpressionStatementNode::toString() const {
    // Example: {"key1": expr1, "key2": expr2}
    std::ostringstream oss;
    oss << "{";
    for (size_t i = 0; i < entries.size(); ++i) {
        if (i > 0) oss << ", ";
        // If key is a string, you'd want quotes. If it's an identifier, maybe not.
        // For simplicity, let's always show key.text in quotes:
        oss << "\"" << entries[i].key.text << "\": "
            << (entries[i].value ? entries[i].value->toString() : "null");
    }
    oss << "}";
    return oss.str();
}

SubscriptExpressionStatementNode::SubscriptExpressionStatementNode(
    std::unique_ptr<ExpressionStatementNode> baseExpr,
    std::unique_ptr<ExpressionStatementNode> indexExpr,
    bool slice)
    : base(std::move(baseExpr)), index(std::move(indexExpr)), isSlice(slice) {}

ExpressionStatementNodeType
SubscriptExpressionStatementNode::getExprType() const {
    return ExpressionStatementNodeType::Subscript;
}

std::unique_ptr<ExpressionStatementNode>
SubscriptExpressionStatementNode::clone() const {
    auto newBase = base ? base->clone() : nullptr;
    auto newIndex = index ? index->clone() : nullptr;
    return std::make_unique<SubscriptExpressionStatementNode>(
        std::move(newBase), std::move(newIndex), isSlice
    );
}

std::string SubscriptExpressionStatementNode::toString() const {
    std::ostringstream oss;
    if (base) oss << base->toString();
    oss << "[";
    if (index) oss << index->toString();
    oss << "]";
    return oss.str();
}



// ------------------- AST Node Implementations -------------------

ASTNodeType JtmlElementNode::getType() const {
    return ASTNodeType::JtmlElement;
}

 std::unique_ptr<ASTNode> JtmlElementNode::clone() const  {
        auto newNode = std::make_unique<JtmlElementNode>();
        newNode->tagName = tagName;
        for (const auto& attr : attributes) {
            auto clonedValue = attr.value ? attr.value->clone() : nullptr;
            newNode->attributes.push_back({attr.key, std::move(clonedValue)});
        }
        for (const auto& child : content) {
            newNode->content.push_back(child->clone());
        }
        return newNode;
}

std::string JtmlElementNode::toString() const {
        std::ostringstream oss;
        oss << "JtmlElementNode(tagName=" << tagName << ", attributes=[";
        for (const auto& attr : attributes) {
            oss << "{key: " << attr.key << ", value: " << attr.value->toString() << "}, ";
        }
        oss << "], content=[";
        for (const auto& child : content) {
            oss << child->toString() << ", ";
        }
        oss << "])";
        return oss.str();
}
ASTNodeType BlockStatementNode::getType() const {
    return ASTNodeType::BlockStatement;
}

std::unique_ptr<ASTNode> BlockStatementNode::clone() const {
        auto newNode = std::make_unique<BlockStatementNode>();
        for (const auto& stmt : statements) {
            newNode->statements.push_back(stmt->clone());
        }
        return newNode;
}

 std::string BlockStatementNode::toString() const  {
        std::ostringstream oss;
        oss << "BlockStatementNode(statements=[";
        for (const auto& stmt : statements) {
            oss << stmt->toString() << ", ";
        }
        oss << "])";
        return oss.str();
    }

ASTNodeType ReturnStatementNode::getType() const {
        return ASTNodeType::ReturnStatement;
}

std::unique_ptr<ASTNode> ReturnStatementNode::clone() const {
        auto newNode = std::make_unique<ReturnStatementNode>();
        newNode->expr = expr ? expr->clone() : nullptr;
        return newNode;
}
std::string ReturnStatementNode::toString() const {
    std::ostringstream oss;
    oss << "ReturnStatementNode(expr=";
    try {
        if (expr) {
            oss << expr->toString();
        } else {
            oss << "null";
        }
    } catch (const std::exception& e) {
        oss << "Error in expr->toString(): " << e.what();
    } catch (...) {
        oss << "Unknown error in expr->toString()";
    }
    oss << ")";
    return oss.str();
}

ASTNodeType ShowStatementNode::getType() const {
    return ASTNodeType::ShowStatement;
}

std::unique_ptr<ASTNode> ShowStatementNode::clone() const {
    auto newNode = std::make_unique<ShowStatementNode>();
    newNode->expr = expr ? expr->clone() : nullptr;
    return newNode;
}

std::string ShowStatementNode::toString() const {
    std::ostringstream oss;
    oss << "ShowStatementNode(expr=";
    if (expr) {
        oss << expr->toString();
    } else {
        oss << "null";
    }
    oss << ")";
    return oss.str();
}

ASTNodeType DefineStatementNode::getType() const {
    return ASTNodeType::DefineStatement;
}

std::unique_ptr<ASTNode> DefineStatementNode::clone() const {
    auto newNode = std::make_unique<DefineStatementNode>();
    newNode->identifier = identifier;
    newNode->declaredType = declaredType;
    newNode->expression = expression ? expression->clone() : nullptr;
    newNode->isConst = isConst;
    return newNode;
}

 std::string DefineStatementNode::toString() const {
    std::ostringstream oss;
    oss << "DefineStatementNode(identifier=" << identifier
        << ", declaredType=" << declaredType
        << ", isConst=" << (isConst ? "true" : "false")
        << ", expression=";
    if (expression) {
        oss << expression->toString();
    } else {
        oss << "null";
    }
    oss << ")";
    return oss.str();
}
ASTNodeType AssignmentStatementNode::getType() const {
    return ASTNodeType::AssignmentStatement;
}

std::unique_ptr<ASTNode> AssignmentStatementNode::clone() const {
    auto newNode = std::make_unique<AssignmentStatementNode>();
    // Clone the left-hand side (lhs) expression
    newNode->lhs = lhs ? lhs->clone() : nullptr;
    // Clone the right-hand side (rhs) expression
    newNode->rhs = rhs ? rhs->clone() : nullptr;
    return newNode;
}

std::string AssignmentStatementNode::toString() const {
    std::ostringstream oss;
    oss << "AssignmentStatementNode(lhs=";
    if (lhs) {
        oss << lhs->toString();
    } else {
        oss << "null";
    }
    oss << ", rhs=";
    if (rhs) {
        oss << rhs->toString();
    } else {
        oss << "null";
    }
    oss << ")";
    return oss.str();
}

ASTNodeType ExpressionNode::getType() const {
    return ASTNodeType::ExpressionStatement;
}

std::unique_ptr<ASTNode> ExpressionNode::clone() const {
    auto newNode = std::make_unique<ExpressionNode>();

    newNode->expression = expression ? expression->clone() : nullptr;

    return newNode;
}

std::string ExpressionNode::toString() const {
    std::ostringstream oss;
    oss << "ExpressionNode(expression=";
    if (expression) {
        oss << expression->toString();
    } else {
        oss << "null";
    }
    oss << ")";
    return oss.str();
}

ASTNodeType DeriveStatementNode::getType() const {
    return ASTNodeType::DeriveStatement;
}

std::unique_ptr<ASTNode> DeriveStatementNode::clone() const {
        auto newNode = std::make_unique<DeriveStatementNode>();
        newNode->identifier = identifier;
        newNode->declaredType = declaredType;
        newNode->expression = expression ? expression->clone() : nullptr;
        return newNode;
}

std::string DeriveStatementNode::toString() const {
    std::ostringstream oss;
    oss << "DeriveStatementNode(identifier=" << identifier
        << ", declaredType=" << declaredType << ", expression=";
    if (expression) {
        oss << expression->toString();
    } else {
        oss << "null";
    }
    oss << ")";
    return oss.str();
}
ASTNodeType UnbindStatementNode::getType() const {
    return ASTNodeType::UnbindStatement;
}

std::unique_ptr<ASTNode> UnbindStatementNode::clone() const {
    auto newNode = std::make_unique<UnbindStatementNode>();
    newNode->identifier = identifier;
    return newNode;
}

std::string UnbindStatementNode::toString() const {
    std::ostringstream oss;
    oss << "UnbindStatementNode(identifier=" << identifier << ")";
    return oss.str();
}
ASTNodeType StoreStatementNode::getType() const {
    return ASTNodeType::StoreStatement;
}

std::unique_ptr<ASTNode> StoreStatementNode::clone() const {
    auto newNode = std::make_unique<StoreStatementNode>();
    newNode->targetScope = targetScope;
    newNode->variableName = variableName;
    return newNode;
}

std::string StoreStatementNode::toString() const {
    std::ostringstream oss;
    oss << "StoreStatementNode(targetScope=" << targetScope
        << ", variableName=" << variableName << ")";
    return oss.str();
}

ASTNodeType ImportStatementNode::getType() const {
    return ASTNodeType::ImportStatement;
}

std::unique_ptr<ASTNode> ImportStatementNode::clone() const {
    auto newNode = std::make_unique<ImportStatementNode>();
    newNode->path = path;
    return newNode;
}

std::string ImportStatementNode::toString() const {
    std::ostringstream oss;
    oss << "ImportStatementNode(path=" << path << ")";
    return oss.str();
}

ASTNodeType ThrowStatementNode::getType() const {
    return ASTNodeType::ThrowStatement;
}

std::unique_ptr<ASTNode> ThrowStatementNode::clone() const  {
    auto newNode = std::make_unique<ThrowStatementNode>();
    newNode->expression = expression ? expression->clone() : nullptr;
    return newNode;
}

std::string ThrowStatementNode::toString() const {
    std::ostringstream oss;
    oss << "ThrowStatementNode(expression=";
    if (expression) {
        oss << expression->toString();
    } else {
        oss << "null";
    }
    oss << ")";
    return oss.str();
}
ASTNodeType IfStatementNode::getType() const {
    return ASTNodeType::IfStatement;
}

std::unique_ptr<ASTNode> IfStatementNode::clone() const {
    auto newNode = std::make_unique<IfStatementNode>();
    newNode->condition = condition ? condition->clone() : nullptr;
    for (const auto& stmt : thenStatements) {
        newNode->thenStatements.push_back(stmt->clone());
    }
    for (const auto& stmt : elseStatements) {
        newNode->elseStatements.push_back(stmt->clone());
    }
    return newNode;
}

std::string IfStatementNode::toString() const {
    std::ostringstream oss;
    oss << "IfStatementNode(condition=";
    if (condition) {
        oss << condition->toString();
    } else {
        oss << "null";
    }
    oss << ", thenStatements=[";
    for (const auto& stmt : thenStatements) {
        oss << stmt->toString() << ", ";
    }
    oss << "], elseStatements=[";
    for (const auto& stmt : elseStatements) {
        oss << stmt->toString() << ", ";
    }
    oss << "])";
    return oss.str();
}

ASTNodeType WhileStatementNode::getType() const {
    return ASTNodeType::WhileStatement;
}

std::unique_ptr<ASTNode> WhileStatementNode::clone() const {
    auto newNode = std::make_unique<WhileStatementNode>();
    newNode->condition = condition ? condition->clone() : nullptr;
    for (const auto& stmt : body) {
        newNode->body.push_back(stmt->clone());
    }
    return newNode;
}

std::string WhileStatementNode::toString() const {
    std::ostringstream oss;
    oss << "WhileStatementNode(condition=";
    if (condition) {
        oss << condition->toString();
    } else {
        oss << "null";
    }
    oss << ", body=[";
    for (const auto& stmt : body) {
        oss << stmt->toString() << ", ";
    }
    oss << "])";
    return oss.str();
}

ASTNodeType BreakStatementNode::getType() const {
    return ASTNodeType::BreakStatement;
}

std::unique_ptr<ASTNode> BreakStatementNode::clone() const {
    auto newNode = std::make_unique<BreakStatementNode>();
    return newNode;
}

std::string BreakStatementNode::toString() const {
    std::ostringstream oss;
    oss << "BreakStatementNode()";
    return oss.str();
}

ASTNodeType ContinueStatementNode::getType() const {
    return ASTNodeType::ContinueStatement;
}

std::unique_ptr<ASTNode> ContinueStatementNode::clone() const {
    auto newNode = std::make_unique<ContinueStatementNode>();
    return newNode;
}

std::string ContinueStatementNode::toString() const {
    std::ostringstream oss;
    oss << "ContinueStatementNode()";
    return oss.str();
}


ASTNodeType ForStatementNode::getType() const {
    return ASTNodeType::ForStatement;
}

std::unique_ptr<ASTNode> ForStatementNode::clone() const {
    auto newNode = std::make_unique<ForStatementNode>();
    newNode->iteratorName = iteratorName;
    newNode->iterableExpression = iterableExpression ? iterableExpression->clone() : nullptr;
    newNode->rangeEndExpr = rangeEndExpr ? rangeEndExpr->clone() : nullptr;
    for (const auto& stmt : body) {
        newNode->body.push_back(stmt->clone());
    }
    return newNode;
}

std::string ForStatementNode::toString() const {
    std::ostringstream oss;
    oss << "ForStatementNode(iteratorName=" << iteratorName
        << ", iterableExpression=";
    if (iterableExpression) {
        oss << iterableExpression->toString();
    } else {
        oss << "null";
    }
    oss << ", rangeEndExpr=";
    if (rangeEndExpr) {
        oss << rangeEndExpr->toString();
    } else {
        oss << "null";
    }
    oss << ", body=[";
    for (const auto& stmt : body) {
        oss << stmt->toString() << ", ";
    }
    oss << "])";
    return oss.str();
}

ASTNodeType TryExceptThenNode::getType() const {
    return ASTNodeType::TryExceptThen;
}

std::unique_ptr<ASTNode> TryExceptThenNode::clone() const {
    auto newNode = std::make_unique<TryExceptThenNode>();
    for (const auto& stmt : tryBlock) {
        newNode->tryBlock.push_back(stmt->clone());
    }
    newNode->hasCatch = hasCatch;
    newNode->catchIdentifier = catchIdentifier;
    for (const auto& stmt : catchBlock) {
        newNode->catchBlock.push_back(stmt->clone());
    }
    newNode->hasFinally = hasFinally;
    for (const auto& stmt : finallyBlock) {
        newNode->finallyBlock.push_back(stmt->clone());
    }
    return newNode;
}

std::string TryExceptThenNode::toString() const {
    std::ostringstream oss;
    oss << "TryExceptThenNode(tryBlock=[";
    for (const auto& stmt : tryBlock) {
        oss << stmt->toString() << ", ";
    }
    oss << "], hasCatch=" << (hasCatch ? "true" : "false")
        << ", catchIdentifier=" << (hasCatch ? catchIdentifier : "null")
        << ", catchBlock=[";
    for (const auto& stmt : catchBlock) {
        oss << stmt->toString() << ", ";
    }
    oss << "], hasFinally=" << (hasFinally ? "true" : "false")
        << ", finallyBlock=[";
    for (const auto& stmt : finallyBlock) {
        oss << stmt->toString() << ", ";
    }
    oss << "])";
    return oss.str();
}

ASTNodeType FunctionDeclarationNode::getType() const {
    return ASTNodeType::FunctionDeclaration;
}

FunctionDeclarationNode::FunctionDeclarationNode(
        const std::string& funcName,
        const std::vector<Parameter>& params,
        const std::string& retType,
        std::vector<std::unique_ptr<ASTNode>>&& funcBody,
        bool asyncFlag)
: name(funcName), parameters(params), returnType(retType), body(std::move(funcBody)), isAsync(asyncFlag) {}
std::unique_ptr<ASTNode> FunctionDeclarationNode::clone() const {
    std::vector<std::unique_ptr<ASTNode>> clonedBody;
    clonedBody.reserve(body.size());
    for (const auto& stmt : body) {
        clonedBody.push_back(stmt->clone());
    }
    return std::make_unique<FunctionDeclarationNode>(name, parameters, returnType, std::move(clonedBody), isAsync);
}
std::string FunctionDeclarationNode::toString() const {
    std::ostringstream oss;
    oss << "FunctionDeclarationNode(name=" << name
        << ", isAsync=" << (isAsync ? "true" : "false")
        << ", parameters=[";
    for (const auto& param : parameters) {
        oss << "{name: " << param.name << ", type: " << param.type << "}, ";
    }
    oss << "], returnType=" << returnType << ", body=[";
    for (const auto& stmt : body) {
        oss << stmt->toString() << ", ";
    }
    oss << "])";
    return oss.str();
}

ASTNodeType SubscribeStatementNode::getType() const {
    return ASTNodeType::SubscribeStatement;
}
std::unique_ptr<ASTNode> SubscribeStatementNode::clone() const {
    auto newNode = std::make_unique<SubscribeStatementNode>();
    newNode->functionName = functionName;
    newNode->variableName = variableName;
    return newNode;
}
std::string SubscribeStatementNode::toString() const {
    std::ostringstream oss;
    oss << "SubscribeStatementNode(functionName=" << functionName  << "], variableName=" << variableName << ")";
    return oss.str();
}


ASTNodeType UnsubscribeStatementNode::getType() const {
    return ASTNodeType::UnsubscribeStatement;
}
std::unique_ptr<ASTNode> UnsubscribeStatementNode::clone() const {
    auto newNode = std::make_unique<UnsubscribeStatementNode>();
    newNode->functionName = functionName;
    newNode->variableName = variableName;
    return newNode;
}
std::string UnsubscribeStatementNode::toString() const {
    std::ostringstream oss;
    oss << "UnsubscribeStatementNode(functionName=" << functionName
        << ", variableName=" << variableName << ")";
    return oss.str();
}

ASTNodeType NoOpStatementNode::getType() const  {
     return ASTNodeType::NoOp;
}

std::string NoOpStatementNode::toString() const {
    return "NoOpStatementNode";
}


std::unique_ptr<ASTNode> NoOpStatementNode::clone() const {
    return std::make_unique<NoOpStatementNode>(*this);
}

ASTNodeType ClassDeclarationNode::getType() const {
       return ASTNodeType::ClassDeclaration;
}

ClassDeclarationNode::ClassDeclarationNode(const std::string& name, const std::string& parentName, std::vector<std::unique_ptr<ASTNode>> classMembers)
        : name(name), parentName(parentName), members(std::move(classMembers)) {}

std::string ClassDeclarationNode::toString() const {
    std::ostringstream oss;
    oss << "ClassDeclarationNode(name=" << name;
    if (!parentName.empty()) {
        oss << ", parent=" << parentName;
    }
    oss << ", members=[";
    for (const auto& mem : members) {
        oss << mem->toString() << ", ";
    }
    oss << "])";
    return oss.str();
}

std::unique_ptr<ASTNode> ClassDeclarationNode::clone() const {
   std::vector<std::unique_ptr<ASTNode>> clonedMembers;
    for (const auto& stmt : members) {
        clonedMembers.push_back(stmt->clone());
    }
    return std::make_unique<ClassDeclarationNode>(name, parentName, std::move(clonedMembers));
}

FunctionCallExpressionStatementNode::FunctionCallExpressionStatementNode(
    const std::string& funcName,
    std::vector<std::unique_ptr<ExpressionStatementNode>> args
)
    : functionName(funcName), arguments(std::move(args)) {}

    // Override methods
ExpressionStatementNodeType FunctionCallExpressionStatementNode::getExprType() const  {
    return ExpressionStatementNodeType::FunctionCall;
}

std::unique_ptr<ExpressionStatementNode> FunctionCallExpressionStatementNode::clone() const {
    std::vector<std::unique_ptr<ExpressionStatementNode>> clonedArgs;
    for (const auto& arg : arguments) {
        if (arg) {
            clonedArgs.push_back(arg->clone());
        } else {
            clonedArgs.push_back(nullptr); // Handle unexpected null arguments gracefully
        }
    }
    return std::make_unique<FunctionCallExpressionStatementNode>(functionName, std::move(clonedArgs));
}


std::string FunctionCallExpressionStatementNode::toString() const {
    if (functionName.empty()) {
        throw std::runtime_error("[ERROR] functionName is empty in FunctionCallExpressionStatementNode");
    }
    std::string result = functionName + "(";

    for (size_t i = 0; i < arguments.size(); ++i) {
        if (!arguments[i]) {
            break;
        }
        result += arguments[i]->toString();
        if (i < arguments.size() - 1) {
            result += ", ";
        }
    }
    result += ")";
    return result;
}

ObjectPropertyAccessExpressionNode::ObjectPropertyAccessExpressionNode(std::unique_ptr<ExpressionStatementNode> baseExpr,
                                    const std::string& propName)
    : base(std::move(baseExpr)), propertyName(propName) {}

ExpressionStatementNodeType ObjectPropertyAccessExpressionNode ::getExprType() const {
    // add a corresponding enum, e.g. ObjectPropertyAccess
    return ExpressionStatementNodeType::ObjectPropertyAccess;
}

std::string ObjectPropertyAccessExpressionNode::toString() const {
    return "(" + base->toString() + "." + propertyName + ")";
}

std::unique_ptr<ExpressionStatementNode> ObjectPropertyAccessExpressionNode ::clone() const {
    auto clonedBase = base->clone();
    auto copy = std::make_unique<ObjectPropertyAccessExpressionNode>(
        std::move(clonedBase), propertyName
    );
    return copy;
}

ObjectMethodCallExpressionNode::ObjectMethodCallExpressionNode(std::unique_ptr<ExpressionStatementNode> baseExpr,
                                const std::string& mName,
                                std::vector<std::unique_ptr<ExpressionStatementNode>> args)
    : base(std::move(baseExpr)), methodName(mName), arguments(std::move(args)) {}

ExpressionStatementNodeType ObjectMethodCallExpressionNode::getExprType() const {
    // add a corresponding enum, e.g. ObjectMethodCall
    return ExpressionStatementNodeType::ObjectMethodCall;
}

std::string ObjectMethodCallExpressionNode::toString() const {
    std::ostringstream oss;
    oss << "(" << base->toString() << "." << methodName << "(";
    for (size_t i = 0; i < arguments.size(); i++) {
        oss << arguments[i]->toString();
        if (i + 1 < arguments.size()) oss << ", ";
    }
    oss << "))";
    return oss.str();
}

std::unique_ptr<ExpressionStatementNode> ObjectMethodCallExpressionNode::clone() const {
    auto clonedBase = base->clone();
    std::vector<std::unique_ptr<ExpressionStatementNode>> clonedArgs;
    clonedArgs.reserve(arguments.size());
    for (const auto& arg : arguments) {
        clonedArgs.push_back(arg->clone());
    }
    auto copy = std::make_unique<ObjectMethodCallExpressionNode>(
        std::move(clonedBase), methodName, std::move(clonedArgs)
    );
    return copy;
}


// Add implementations for other AST nodes as needed
