// jtml_parser.hpp
#pragma once

#include "jtml/ast.h"
#include "jtml/lexer.h"
#include <memory>
#include <iostream>
#include <stdexcept>
/**
 * Parser Class
 * Responsible for parsing a sequence of tokens into an Abstract Syntax Tree (AST).
 */
class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    // Parses the entire program and returns a vector of AST nodes
    std::vector<std::unique_ptr<ASTNode>> parseProgram();
    // Parses a single top-level JtmlElement (e.g., 'element div ... #')
    std::unique_ptr<JtmlElementNode> parseJtmlElement();
    const std::vector<std::string>& getErrors() const { return m_errors; }



private:
    std::vector<Token> m_tokens;
    size_t m_pos;
    int m_line;
    int m_column;

    std::vector<std::string> m_errors;


    // ------------------- Parsing Helper Functions -------------------

    // Parses a single statement and returns an AST node
    std::unique_ptr<ASTNode> parseStatement();

    // Parses an expression and returns an ExpressionStatementNode
    std::unique_ptr<ExpressionStatementNode> parseExpression();
    std::unique_ptr<ASTNode> parseExpressionStatement();

    std::unique_ptr<ASTNode> parseExpressionStatement(std::unique_ptr<ExpressionStatementNode> lhs);
    bool canBeReferenceExpression();
    // Parses an assignment statement (e.g., 'a = 10\\')
    std::unique_ptr<ASTNode> parseAssignmentStatement(
        std::unique_ptr<ExpressionStatementNode> lhs);
    std::unique_ptr<ExpressionStatementNode> parseReferenceExpression(bool &validLHS);
    std::vector<std::unique_ptr<ExpressionStatementNode>> parseArguments();

    // Parses a derive statement (e.g., 'derive sum = a + b\\')
    std::unique_ptr<ASTNode> parseDeriveStatement();

    // Parses an unbind statement (e.g., 'unbind sum\\')
    std::unique_ptr<ASTNode> parseUnbindStatement();

    // Parses a store statement (e.g., 'store(main) a\\')
    std::unique_ptr<ASTNode> parseStoreStatement();

    // Parses a show statement (e.g., 'show sum\\')
    std::unique_ptr<ASTNode> parseShowStatement();

    // Parses a define statement (e.g., 'define a = 2\\')
    std::unique_ptr<ASTNode> parseDefineStatement();
    std::unique_ptr<ASTNode> parseConstStatement();
    std::unique_ptr<ASTNode> parseImportStatement();


    // Parses an if-else statement
    std::unique_ptr<ASTNode> parseIfElseStatement();

    // Parses a while statement
    std::unique_ptr<ASTNode> parseWhileStatement();

    // Parses a break statement
    std::unique_ptr<ASTNode> parseBreakStatement();

    // Parses a continue statement
    std::unique_ptr<ASTNode> parseContinueStatement();

    // Parses a for statement
    std::unique_ptr<ASTNode> parseForStatement();

    // Parses a try-except-then statement
    std::unique_ptr<ASTNode> parseTryExceptThenStatement();

    std::unique_ptr<ASTNode> parseFunctionDeclaration(bool isAsync = false);

    std::unique_ptr<ASTNode> parseClassDeclaration();

    void parseClassBody(ClassDeclarationNode& classNode);

    std::unique_ptr<ASTNode> parseSubscribeStatement();

    std::unique_ptr<ASTNode> parseUnsubscribeStatement();

    // Parses a return statement
    std::unique_ptr<ASTNode> parseReturnStatement();

    // Parses a throw statement
    std::unique_ptr<ASTNode> parseThrowStatement();

    // ------------------- JtmlElement Parsing Helpers -------------------

    // Parses a block of statements, enclosed by statement terminators
    void parseBlockStatementList(std::vector<std::unique_ptr<ASTNode>>& stmts) ;
    std::string consumeElementTagName();
    std::string consumeAttributeName();
    bool canStartAttributeName() const;
    bool canContinueAttributeName() const;

    // ------------------- Utility Methods -------------------
    bool check(TokenType type) const;
    bool checkNext(TokenType type) const;
    bool checkNextNext(TokenType type) const;

    bool match(TokenType type);
    Token consume(TokenType type, const std::string& errMsg);
    Token advance();
    Token previous() const;
    bool isAtEnd() const;
    const Token& peek() const;

    bool matchNumberLiteral();

    void recordError(const std::string& message);
    void synchronize();

    // ------------------- Expression Parser Nested Class -------------------
    class ExpressionParser {
    public:
        ExpressionParser(const std::vector<Token>& tokens, size_t& posRef, Parser& parentParser)
            : m_tokens(tokens), m_posRef(posRef), m_parentParser(parentParser) {}

        // Parses an expression and returns an ExpressionStatementNode
        std::unique_ptr<ExpressionStatementNode> parseExpression();


    private:
        const std::vector<Token>& m_tokens;
        size_t& m_posRef;
        Parser& m_parentParser;

        std::unique_ptr<ExpressionStatementNode> parseConditional();
        std::unique_ptr<ExpressionStatementNode> parseLogicalOr();
        std::unique_ptr<ExpressionStatementNode> parseLogicalAnd();
        std::unique_ptr<ExpressionStatementNode> parseEquality();
        std::unique_ptr<ExpressionStatementNode> parseComparison();
        std::unique_ptr<ExpressionStatementNode> parseAddition();
        std::unique_ptr<ExpressionStatementNode> parseMultiplication();
        std::unique_ptr<ExpressionStatementNode> parseUnary();
        std::unique_ptr<ExpressionStatementNode> parsePrimary();
        std::unique_ptr<ExpressionStatementNode> parseFunctionCall(const Token& nameToken);
        std::unique_ptr<ExpressionStatementNode> parseArrayLiteral();
        std::unique_ptr<ExpressionStatementNode> parseDictionaryLiteral();

        // ------------------- Utility Methods for ExpressionParser -------------------
        bool match(TokenType type);
        bool check(TokenType type) const;
        Token advance();
        Token previous() const;
        bool isAtEnd() const;
        const Token& peek() const;

        bool matchNumberLiteral();

    };
};
