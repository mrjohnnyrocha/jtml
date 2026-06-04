#pragma once

#include "jtml/lexer.h"  // Ensure this header defines 'Token' and 'TokenType'
#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <iomanip>


/**
 * Identify node types for statements/markup in the AST.
 * - JtmlElement is the markup-like node created by element declarations.
 * - ShowStatement, DefineStatement, DeriveStatement, etc., are statements.
 * - We also have control-flow (If, While, For) and error-handling (TryExceptThen).
 */
enum class ASTNodeType {
    // Markup / Structural
    JtmlElement,

    // Simple Statements
    ShowStatement,
    DefineStatement,
    DeriveStatement,
    UnbindStatement,
    StoreStatement,
    AssignmentStatement,
    ExpressionStatement,
    ReturnStatement,
    ThrowStatement,
    ImportStatement,

    // Control Flow
    IfStatement,
    WhileStatement,
    ForStatement,
    TryExceptThen,
    BreakStatement,
    ContinueStatement,

    BlockStatement,
    FunctionDeclaration,
    SubscribeStatement,
    UnsubscribeStatement,

    NoOp,
    ClassDeclaration,
    // Add more as needed
};

/**
 * For expression parsing, we define a separate ExpressionStatementNode hierarchy
 * (Binary, Unary, Variable, etc.). This is not part of ASTNodeType
 * because it's only used inside statements or expressions,
 * not top-level statements themselves.
 */
enum class ExpressionStatementNodeType {
    Binary,
    Unary,
    Variable,
    StringLiteral,
    NumberLiteral,
    BooleanLiteral,
    EmbeddedVariable,
    CompositeString,
    ArrayLiteral,
    DictionaryLiteral,
    Subscript,
    FunctionCall,
    Conditional,
    ObjectPropertyAccess,
    ObjectMethodCall,
    // Add more as needed (e.g., BooleanLiteral)
};

// ------------------- Expression Nodes -------------------
struct ExpressionStatementNode {
    virtual ~ExpressionStatementNode() = default;
    virtual ExpressionStatementNodeType getExprType() const = 0;

    virtual std::unique_ptr<ExpressionStatementNode> clone() const = 0;
    virtual std::string toString() const = 0;
};

/**
 * A binary operation (left op right),
 * e.g., (a + b), (x * 2), (x == y).
 */
struct BinaryExpressionStatementNode : public ExpressionStatementNode {
    std::string op;  // Operator, e.g., "+", "-", "*", "/", "==", "!="

    std::unique_ptr<ExpressionStatementNode> left;
    std::unique_ptr<ExpressionStatementNode> right;

    BinaryExpressionStatementNode(const Token& opToken,
                                  std::unique_ptr<ExpressionStatementNode> l,
                                  std::unique_ptr<ExpressionStatementNode> r);

    ExpressionStatementNodeType getExprType() const override;

    std::unique_ptr<ExpressionStatementNode> clone() const override;

    std::string toString() const override;  // e.g., "(a + b)"
};

/**
 * A unary operation, e.g., -x, !x
 */
struct UnaryExpressionStatementNode : public ExpressionStatementNode {
    std::string op;  // Operator, e.g., "-", "!"

    std::unique_ptr<ExpressionStatementNode> right;

    UnaryExpressionStatementNode(const Token& opToken, std::unique_ptr<ExpressionStatementNode> r);

    ExpressionStatementNodeType getExprType() const override;

    std::unique_ptr<ExpressionStatementNode> clone() const override;

    std::string toString() const override;  // e.g., "-x" or "!x"
};

/**
 * A variable reference, e.g., "myVar".
 */
struct VariableExpressionStatementNode : public ExpressionStatementNode {
    std::string name;

    VariableExpressionStatementNode(const Token& varToken);

    ExpressionStatementNodeType getExprType() const override;

    std::unique_ptr<ExpressionStatementNode> clone() const override;

    std::string toString() const override;  // e.g., "myVar"
};

/**
 * A string literal, e.g., "Hello world".
 */
struct StringLiteralExpressionStatementNode : public ExpressionStatementNode {
    std::string value;

    explicit StringLiteralExpressionStatementNode(const Token& strToken);

    ExpressionStatementNodeType getExprType() const override;

    std::unique_ptr<ExpressionStatementNode> clone() const override;

    std::string toString() const override;  // e.g., "Hello world"
};

struct EmbeddedVariableExpressionStatementNode : public ExpressionStatementNode {
    std::unique_ptr<ExpressionStatementNode> embeddedExpression;

    explicit EmbeddedVariableExpressionStatementNode(std::unique_ptr<ExpressionStatementNode> expr);

    ExpressionStatementNodeType getExprType() const override;

    std::unique_ptr<ExpressionStatementNode> clone() const override;

    std::string toString() const override;
};

struct CompositeStringExpressionStatementNode : public ExpressionStatementNode {
    std::vector<std::unique_ptr<ExpressionStatementNode>> parts;

    explicit CompositeStringExpressionStatementNode(
        std::vector<std::unique_ptr<ExpressionStatementNode>> p);

    ExpressionStatementNodeType getExprType() const override;

    std::unique_ptr<ExpressionStatementNode> clone() const override;

    std::string toString() const override;

    std::unique_ptr<ExpressionStatementNode> optimize() const;
};

/**
 * A numeric literal, e.g., 42, 3.14
 * If you prefer, store them as double or string.
 */


struct NumberLiteralExpressionStatementNode : public ExpressionStatementNode {
    double value; // Alternatively, use double numericValue

    explicit NumberLiteralExpressionStatementNode(const Token& numToken);

    ExpressionStatementNodeType getExprType() const override;

    std::unique_ptr<ExpressionStatementNode> clone() const override;

    std::string toString() const override;  // e.g., "42" or "3.14"
};

struct BooleanLiteralExpressionStatementNode : public ExpressionStatementNode {
    bool value;

    explicit BooleanLiteralExpressionStatementNode(bool val);

    ExpressionStatementNodeType getExprType() const override;

    std::unique_ptr<ExpressionStatementNode> clone() const override;

    std::string toString() const override;
};

struct ConditionalExpressionStatementNode : public ExpressionStatementNode {
    std::unique_ptr<ExpressionStatementNode> condition;
    std::unique_ptr<ExpressionStatementNode> whenTrue;
    std::unique_ptr<ExpressionStatementNode> whenFalse;

    ConditionalExpressionStatementNode(std::unique_ptr<ExpressionStatementNode> conditionExpr,
                                       std::unique_ptr<ExpressionStatementNode> trueExpr,
                                       std::unique_ptr<ExpressionStatementNode> falseExpr);

    ExpressionStatementNodeType getExprType() const override;

    std::unique_ptr<ExpressionStatementNode> clone() const override;

    std::string toString() const override;
};

// 1) ArrayLiteralExpressionStatementNode
struct ArrayLiteralExpressionStatementNode : public ExpressionStatementNode {
    std::vector<std::unique_ptr<ExpressionStatementNode>> elements;

    explicit ArrayLiteralExpressionStatementNode(
        std::vector<std::unique_ptr<ExpressionStatementNode>> elms);

    ExpressionStatementNodeType getExprType() const override;

    std::unique_ptr<ExpressionStatementNode> clone() const override;

    std::string toString() const override;
};

// 2) DictionaryLiteralExpressionStatementNode
struct DictionaryEntry {
    Token key;
    std::unique_ptr<ExpressionStatementNode> value;
};

struct DictionaryLiteralExpressionStatementNode : public ExpressionStatementNode {
    std::vector<DictionaryEntry> entries;

    explicit DictionaryLiteralExpressionStatementNode(std::vector<DictionaryEntry> etrs) ;

    ExpressionStatementNodeType getExprType() const override;

    std::unique_ptr<ExpressionStatementNode> clone() const override;

    std::string toString() const override;
};

struct SubscriptExpressionStatementNode : public ExpressionStatementNode {
    std::unique_ptr<ExpressionStatementNode> base;   // e.g. "arr"
    std::unique_ptr<ExpressionStatementNode> index;  // e.g. "2" or "myKey"
    bool isSlice = false; // optional if you support arr[2..5]

    explicit SubscriptExpressionStatementNode(
        std::unique_ptr<ExpressionStatementNode> baseExpr,
        std::unique_ptr<ExpressionStatementNode> indexExpr,
        bool slice = false);

    ExpressionStatementNodeType getExprType() const override;
    std::unique_ptr<ExpressionStatementNode> clone() const override;
    std::string toString() const override;
};



/**
 * Add more expression nodes as needed, such as BooleanLiteralExpressionStatementNode.
 */

// ------------------- Base AST Node (Statement-Level) -------------------
struct ASTNode {
    virtual ~ASTNode() = default;
    virtual ASTNodeType getType() const = 0;

    virtual std::unique_ptr<ASTNode> clone() const = 0;
    virtual std::string toString() const = 0;
};

// ------------------- Markup Node -------------------
/**
 * Holds a key:value attribute (e.g., style, class, onclick).
 */
struct JtmlAttribute {
    std::string key;
    std::unique_ptr<ExpressionStatementNode> value;

    // Default constructor
    JtmlAttribute() = default;

    // Parameterized constructor
    JtmlAttribute(const std::string& key, std::unique_ptr<ExpressionStatementNode> value)
        : key(key), value(std::move(value)) {}

    // Copy constructor (deep copy)
    JtmlAttribute(const JtmlAttribute& other)
        : key(other.key), value(other.value ? other.value->clone() : nullptr) {}

    // Move constructor
    JtmlAttribute(JtmlAttribute&& other) noexcept = default;

    // Copy assignment operator (deep copy)
    JtmlAttribute& operator=(const JtmlAttribute& other) {
        if (this != &other) {
            key = other.key;
            value = other.value ? other.value->clone() : nullptr;
        }
        return *this;
    }

    // Move assignment operator
    JtmlAttribute& operator=(JtmlAttribute&& other) noexcept = default;

    ~JtmlAttribute() = default;
};


/**
 * Represents a markup element, such as element div ... #.
 */
struct JtmlElementNode : public ASTNode {
    std::string tagName;
    std::vector<JtmlAttribute> attributes;
    std::vector<std::unique_ptr<ASTNode>> content;  // Child nodes: statements or nested elements

    ASTNodeType getType() const override;

    std::unique_ptr<ASTNode> clone() const override;
    std::string toString() const override;
};

// ------------------- Simple Statement Nodes -------------------


struct BlockStatementNode : public ASTNode {
public:
    std::vector<std::unique_ptr<ASTNode>> statements;

    ASTNodeType getType() const override;

    std::unique_ptr<ASTNode> clone() const override;
    std::string toString() const override;
};

struct ReturnStatementNode : public ASTNode {
    std::unique_ptr<ExpressionStatementNode> expr;

    ASTNodeType getType() const override;

    std::unique_ptr<ASTNode> clone() const override;
    std::string toString() const override;
};

// -- ShowStatementNode --
struct ShowStatementNode : public ASTNode {
    std::unique_ptr<ExpressionStatementNode> expr;


    ASTNodeType getType() const override;

    std::unique_ptr<ASTNode> clone() const override;
    std::string toString() const override;
};

// -- DefineStatementNode --
struct DefineStatementNode : public ASTNode {
    std::string identifier;
    std::string declaredType; // Optional, erased at runtime and used by tooling.
    std::unique_ptr<ExpressionStatementNode> expression;
    bool isConst = false;

    ASTNodeType getType() const override;

    std::unique_ptr<ASTNode> clone() const override;
    std::string toString() const override;
};

// -- AssignmentStatementNode --
struct AssignmentStatementNode : public ASTNode {
    std::unique_ptr<ExpressionStatementNode> lhs;
    std::unique_ptr<ExpressionStatementNode> rhs;

    ASTNodeType getType() const override;

    std::unique_ptr<ASTNode> clone() const override;
    std::string toString() const override;
};

struct ExpressionNode : public ASTNode {
    std::unique_ptr<ExpressionStatementNode> expression;

    ASTNodeType getType() const override;

    std::unique_ptr<ASTNode> clone() const override;
    std::string toString() const override;
};

// -- DeriveStatementNode (Reactive Variables) --
struct DeriveStatementNode : public ASTNode {
    std::string identifier;
    std::string declaredType; // Optional, might be empty
    std::unique_ptr<ExpressionStatementNode> expression;

    ASTNodeType getType() const override;

    std::unique_ptr<ASTNode> clone() const override;
    std::string toString() const override;
};

// -- UnbindStatementNode --
struct UnbindStatementNode : public ASTNode {
    std::string identifier; // The variable to freeze/unbind from reactivity

    ASTNodeType getType() const override;

    std::unique_ptr<ASTNode> clone() const override;
    std::string toString() const override;
};

// -- StoreStatementNode (Move Variable to a Different Scope) --
struct StoreStatementNode : public ASTNode {
    std::string targetScope;  // e.g., "main" or a function/class name
    std::string variableName; // The variable being stored

    ASTNodeType getType() const override;

    std::unique_ptr<ASTNode> clone() const override;
    std::string toString() const override;
};

struct ImportStatementNode : public ASTNode {
    std::string path;

    ASTNodeType getType() const override;

    std::unique_ptr<ASTNode> clone() const override;
    std::string toString() const override;
};

// -- ThrowStatementNode --
struct ThrowStatementNode : public ASTNode {
    // an expression representing the error or exception data
    std::unique_ptr<ExpressionStatementNode> expression;

    ASTNodeType getType() const override;

    std::unique_ptr<ASTNode> clone() const override;
    std::string toString() const override;
};

// ------------------- Control Flow Statements -------------------

// -- IfStatementNode --
struct IfStatementNode : public ASTNode {
    std::unique_ptr<ExpressionStatementNode> condition;
    std::vector<std::unique_ptr<ASTNode>> thenStatements;
    std::vector<std::unique_ptr<ASTNode>> elseStatements; // empty if no else

    ASTNodeType getType() const override;

    std::unique_ptr<ASTNode> clone() const override;
    std::string toString() const override;
};

// -- WhileStatementNode --
struct WhileStatementNode : public ASTNode {
    std::unique_ptr<ExpressionStatementNode> condition;
    std::vector<std::unique_ptr<ASTNode>> body; // statements

    ASTNodeType getType() const override;

    std::unique_ptr<ASTNode> clone() const override;
    std::string toString() const override;
};

// -- BreakStatementNode --
struct BreakStatementNode : public ASTNode {
    ASTNodeType getType() const override;

    std::unique_ptr<ASTNode> clone() const override;
    std::string toString() const override;
};

// -- ContinueStatementNode --
struct ContinueStatementNode : public ASTNode {
    ASTNodeType getType() const override;

    std::unique_ptr<ASTNode> clone() const override;
    std::string toString() const override;
};


// -- ForStatementNode --
struct ForStatementNode : public ASTNode {
    std::string iteratorName;
    std::unique_ptr<ExpressionStatementNode> iterableExpression;
    std::unique_ptr<ExpressionStatementNode> rangeEndExpr;
    std::vector<std::unique_ptr<ASTNode>> body;

    ASTNodeType getType() const override;

    std::unique_ptr<ASTNode> clone() const override;
    std::string toString() const override;
};

// -- TryExceptThenNode --
struct TryExceptThenNode : public ASTNode {
    std::vector<std::unique_ptr<ASTNode>> tryBlock;

    bool hasCatch = false;
    std::string catchIdentifier; // e.g., "err"
    std::vector<std::unique_ptr<ASTNode>> catchBlock;

    bool hasFinally = false;     // or hasThen if you prefer
    std::vector<std::unique_ptr<ASTNode>> finallyBlock;

    ASTNodeType getType() const override;

    std::unique_ptr<ASTNode> clone() const override;
    std::string toString() const override;
};

struct Parameter {
    std::string name;
    std::string type;
};

struct FunctionDeclarationNode : public ASTNode {
    std::string name;
    std::vector<Parameter> parameters;
    std::string returnType; // Optional
    std::vector<std::unique_ptr<ASTNode>> body;
    bool isAsync = false;

    ASTNodeType getType() const override;

    FunctionDeclarationNode(
        const std::string& funcName,
        const std::vector<Parameter>& params,
        const std::string& retType,
        std::vector<std::unique_ptr<ASTNode>>&& funcBody,
        bool asyncFlag = false);

    std::unique_ptr<ASTNode> clone() const override;

    std::string toString() const override;

};

struct SubscribeStatementNode : public ASTNode {

    std::string functionName;
    std::string variableName;

    ASTNodeType getType() const override;

    std::unique_ptr<ASTNode> clone() const override;

    std::string toString() const override;
};

struct UnsubscribeStatementNode : public ASTNode {

    std::string functionName;
    std::string variableName;

    ASTNodeType getType() const override;

    std::unique_ptr<ASTNode> clone() const override;

    std::string toString() const override;
};

struct NoOpStatementNode : public ASTNode {
    ASTNodeType getType() const override;

    std::unique_ptr<ASTNode> clone() const override;

    std::string toString() const override;

};

struct ClassDeclarationNode : public ASTNode {
    std::string name;
    std::string parentName; // if "derives Parent"
    std::vector<std::unique_ptr<ASTNode>> members; // define/derive/function etc.

    ASTNodeType getType() const override;

    ClassDeclarationNode(const std::string& name, const std::string& parentName, std::vector<std::unique_ptr<ASTNode>> classMembers);

    std::unique_ptr<ASTNode> clone() const override;

    std::string toString() const override;
};

struct FunctionCallExpressionStatementNode : public ExpressionStatementNode {
    std::string functionName;
    std::vector<std::unique_ptr<ExpressionStatementNode>> arguments;

    FunctionCallExpressionStatementNode(const std::string& functionName,
                        std::vector<std::unique_ptr<ExpressionStatementNode>> arguments);

    ExpressionStatementNodeType getExprType() const override;

    std::string toString() const override;

    std::unique_ptr<ExpressionStatementNode> clone() const override;
};

struct ObjectPropertyAccessExpressionNode : public ExpressionStatementNode {
    std::unique_ptr<ExpressionStatementNode> base;  // e.g. 'obj'
    std::string propertyName;                       // e.g. 'field'

    ObjectPropertyAccessExpressionNode(std::unique_ptr<ExpressionStatementNode> baseExpr,
                                       const std::string& propName);

    ExpressionStatementNodeType getExprType() const override;

    std::string toString() const override;

    std::unique_ptr<ExpressionStatementNode> clone() const override;
};

struct ObjectMethodCallExpressionNode : public ExpressionStatementNode {
    std::unique_ptr<ExpressionStatementNode> base;  // e.g. 'obj'
    std::string methodName;                          // e.g. 'someMethod'
    std::vector<std::unique_ptr<ExpressionStatementNode>> arguments;

    ObjectMethodCallExpressionNode(std::unique_ptr<ExpressionStatementNode> baseExpr,
                                   const std::string& mName,
                                   std::vector<std::unique_ptr<ExpressionStatementNode>> args);

    ExpressionStatementNodeType getExprType() const override;

    std::string toString() const override;

    std::unique_ptr<ExpressionStatementNode> clone() const override;
};
