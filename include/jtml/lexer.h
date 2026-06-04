// jtml_lexer.h
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <cctype>

// ------------------- Token Types Enumeration -------------------
enum class TokenType {
    HASH,
    BACKSLASH_HASH,
    AT,
    ELEMENT,
    PLUS,
    PLUSEQ,
    MINUSEQ,
    MULTIPLYEQ,
    DIVIDEEQ,
    MODULUSEQ,
    POWEREQ,
    NOT,
    LT,
    GT,
    LTEQ,
    GTEQ,
    EQ,
    NEQ,
    ASSIGN,
    MINUS,
    MULTIPLY,
    DIVIDE,
    MODULUS,
    POWER,
    LPAREN,
    RPAREN,
    DOTS,
    LBRACKET,
    RBRACKET,
    LBRACE,
    RBRACE,
    COMMA,
    QUESTION,
    COLON,
    STMT_TERMINATOR,
    AND,
    OR,
    SHOW,
    DEFINE,
    DERIVE,
    UNBIND,
    STORE,
    FOR,
    IF,
    CONST,
    IN,
    BREAK,
    CONTINUE,
    THROW,
    ELSE,
    WHILE,
    TRY,
    EXCEPT,
    THEN,
    RETURN,
    FUNCTION,
    TO,
    SUBSCRIBE,
    FROM,
    UNSUBSCRIBE,
    OBJECT,
    DERIVES,
    DOT,
    ASYNC,
    IMPORT,
    MAIN,
    IDENTIFIER,
    STRING_LITERAL,
    NUMBER_LITERAL,
    BOOLEAN_LITERAL,
    END_OF_FILE,
    ERROR
};

TokenType getTokenTypeForOperator(const std::string& op);
std::string tokenTypeToString(TokenType type);

// ------------------- Token Structure -------------------
struct Token {
    TokenType type;
    std::string text;
    int position;
    int line;
    int column;
};

// ------------------- Lexer Class -------------------
class Lexer {
public:
    explicit Lexer(const std::string& input);

    // Tokenize the input string and return a vector of tokens
    std::vector<Token> tokenize();

    // Retrieve any errors encountered during tokenization
    const std::vector<std::string>& getErrors() const;

private:
    std::string m_input;
    size_t m_pos;
    int m_line;
    int m_column;

    std::vector<std::string> errors;

    // Helper functions
    bool isEOF() const;
    char peek() const;
    void recoverFromError();
    void advance();
    bool matchSequence(const std::string& seq);
    Token makeToken(TokenType type, const std::string& text);
    Token consumeStringLiteral(char quoteChar);
    Token consumeNumber();
    Token consumeIdentifier();
};
