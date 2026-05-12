// jtml_lexer.cpp
#include "jtml/lexer.h"

// --------------------- Utility Functions ---------------------------


TokenType getTokenTypeForOperator(const std::string& op) {
    static const std::unordered_map<std::string, TokenType> opTokenMap = {
        // Arithmetic Operators
        {"+", TokenType::PLUS},
        {"-", TokenType::MINUS},
        {"*", TokenType::MULTIPLY},
        {"/", TokenType::DIVIDE},
        {"%", TokenType::MODULUS},
        {"^", TokenType::POWER},

        // Assignment Operators
        {"+=", TokenType::PLUSEQ},
        {"-=", TokenType::MINUSEQ},
        {"*=", TokenType::MULTIPLYEQ},
        {"/=", TokenType::DIVIDEEQ},
        {"%=", TokenType::MODULUSEQ},
        {"^=", TokenType::POWEREQ},
        {"=", TokenType::ASSIGN},

        // Comparison Operators
        {"<", TokenType::LT},
        {">", TokenType::GT},
        {"<=", TokenType::LTEQ},
        {">=", TokenType::GTEQ},
        {"==", TokenType::EQ},
        {"!=", TokenType::NEQ},

        // Logical Operators
        {"&&", TokenType::AND},
        {"||", TokenType::OR},
        {"!", TokenType::NOT},

        // Delimiters
        {"@", TokenType::AT},
        {"(", TokenType::LPAREN},
        {")", TokenType::RPAREN},
        {"[", TokenType::LBRACKET},
        {"]", TokenType::RBRACKET},
        {"{", TokenType::LBRACE},
        {"}", TokenType::RBRACE},
        {"..", TokenType::DOTS},
        {".", TokenType::DOT},
        {",", TokenType::COMMA},
        {"?", TokenType::QUESTION},
        {":", TokenType::COLON},
        {"\\", TokenType::STMT_TERMINATOR},

        // Keywords
        {"show", TokenType::SHOW},
        {"define", TokenType::DEFINE},
        {"derive", TokenType::DERIVE},
        {"unbind", TokenType::UNBIND},
        {"store", TokenType::STORE},
        {"for", TokenType::FOR},
        {"if", TokenType::IF},
        {"true", TokenType::BOOLEAN_LITERAL},
        {"false", TokenType::BOOLEAN_LITERAL},
        {"const", TokenType::CONST},
        {"in", TokenType::IN},
        {"break", TokenType::BREAK},
        {"continue", TokenType::CONTINUE},
        {"throw", TokenType::THROW},
        {"else", TokenType::ELSE},
        {"while", TokenType::WHILE},
        {"element", TokenType::ELEMENT},
        {"try", TokenType::TRY},
        {"except", TokenType::EXCEPT},
        {"then", TokenType::THEN},
        {"return", TokenType::RETURN},
        {"function", TokenType::FUNCTION},
        {"subscribe", TokenType::SUBSCRIBE},
        {"unsubscribe", TokenType::UNSUBSCRIBE},
        {"to", TokenType::TO},
        {"from", TokenType::FROM},
        {"object", TokenType::OBJECT},
        {"derives", TokenType::DERIVES},
        {"async", TokenType::ASYNC},
        {"import", TokenType::IMPORT},
        {"main", TokenType::MAIN}
    };

    auto it = opTokenMap.find(op);
    if (it != opTokenMap.end()) {
        return it->second;
    }
    throw std::runtime_error("Unknown operator or keyword: " + op);
}

#include <unordered_map>
#include <string>

std::string tokenTypeToString(TokenType type) {
    static const std::unordered_map<TokenType, std::string> tokenTypeMap = {
        // Arithmetic Operators
        {TokenType::PLUS, "PLUS"},
        {TokenType::MINUS, "MINUS"},
        {TokenType::MULTIPLY, "MULTIPLY"},
        {TokenType::DIVIDE, "DIVIDE"},
        {TokenType::MODULUS, "MODULUS"},
        {TokenType::POWER, "POWER"},

        // Assignment Operators
        {TokenType::PLUSEQ, "PLUSEQ"},
        {TokenType::MINUSEQ, "MINUSEQ"},
        {TokenType::MULTIPLYEQ, "MULTIPLYEQ"},
        {TokenType::DIVIDEEQ, "DIVIDEEQ"},
        {TokenType::MODULUSEQ, "MODULUSEQ"},
        {TokenType::POWEREQ, "POWEREQ"},
        {TokenType::ASSIGN, "ASSIGN"},

        // Comparison Operators
        {TokenType::LT, "LT"},
        {TokenType::GT, "GT"},
        {TokenType::LTEQ, "LTEQ"},
        {TokenType::GTEQ, "GTEQ"},
        {TokenType::EQ, "EQ"},
        {TokenType::NEQ, "NEQ"},

        // Logical Operators
        {TokenType::AND, "AND"},
        {TokenType::OR, "OR"},
        {TokenType::NOT, "NOT"},

        // Delimiters
        {TokenType::AT, "AT"},
        {TokenType::LPAREN, "LPAREN"},
        {TokenType::RPAREN, "RPAREN"},
        {TokenType::LBRACKET, "LBRACKET"},
        {TokenType::RBRACKET, "RBRACKET"},
        {TokenType::LBRACE, "LBRACE"},
        {TokenType::RBRACE, "RBRACE"},
        {TokenType::DOTS, "DOTS"},
        {TokenType::DOT, "DOT"},
        {TokenType::COMMA, "COMMA"},
        {TokenType::QUESTION, "QUESTION"},
        {TokenType::COLON, "COLON"},
        {TokenType::STMT_TERMINATOR, "STMT_TERMINATOR"},

        // Keywords
        {TokenType::SHOW, "SHOW"},
        {TokenType::DEFINE, "DEFINE"},
        {TokenType::DERIVE, "DERIVE"},
        {TokenType::UNBIND, "UNBIND"},
        {TokenType::STORE, "STORE"},
        {TokenType::FOR, "FOR"},
        {TokenType::IF, "IF"},
        {TokenType::ELSE, "ELSE"},
        {TokenType::WHILE, "WHILE"},
        {TokenType::TRY, "TRY"},
        {TokenType::EXCEPT, "EXCEPT"},
        {TokenType::THEN, "THEN"},
        {TokenType::RETURN, "RETURN"},
        {TokenType::ELEMENT, "ELEMENT"},
        {TokenType::THROW, "THROW"},
        {TokenType::CONST, "CONST"},
        {TokenType::IN, "IN"},
        {TokenType::BREAK, "BREAK"},
        {TokenType::CONTINUE, "CONTINUE"},
        {TokenType::FUNCTION, "FUNCTION"},
        {TokenType::SUBSCRIBE, "SUBSCRIBE"},
        {TokenType::UNSUBSCRIBE, "UNSUBSCRIBE"},
        {TokenType::TO, "TO"},
        {TokenType::FROM, "FROM"},
        {TokenType::OBJECT, "OBJECT"},
        {TokenType::DERIVES, "DERIVES"},
        {TokenType::ASYNC, "ASYNC"},
        {TokenType::IMPORT, "IMPORT"},
        {TokenType::MAIN, "MAIN"},

        // Literals
        {TokenType::BOOLEAN_LITERAL, "BOOLEAN_LITERAL"},
        {TokenType::STRING_LITERAL, "STRING_LITERAL"},
        {TokenType::NUMBER_LITERAL, "NUMBER_LITERAL"},

        // Identifiers
        {TokenType::IDENTIFIER, "IDENTIFIER"},

        // Special Tokens
        {TokenType::HASH, "HASH"},
        {TokenType::BACKSLASH_HASH, "BACKSLASH_HASH"},
        {TokenType::END_OF_FILE, "END_OF_FILE"},
        {TokenType::ERROR, "ERROR"},
    };

    // Default if type not found
    auto it = tokenTypeMap.find(type);
    if (it != tokenTypeMap.end()) {
        return it->second;
    }
    return "UNKNOWN";
}



// ------------------- Lexer Class Implementations -------------------

Lexer::Lexer(const std::string& input)
    : m_input(input), m_pos(0), m_line(1), m_column(1) {}

// Tokenize the input string and return a vector of tokens
std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (!isEOF()) {

        char c = peek();
        if (std::isspace(static_cast<unsigned char>(c))) {
            advance();
            continue;
        }

        if (matchSequence("\\\\")) {
            tokens.push_back(makeToken(TokenType::STMT_TERMINATOR, "\\\\"));
            m_pos += 2;
            continue;
        }

        if (matchSequence("\\#")) {
            tokens.push_back(makeToken(TokenType::BACKSLASH_HASH, "\\#"));
            m_pos += 2;
            continue;
        }

        // Multi-character operators
        if (matchSequence("-=")) {
            tokens.push_back(makeToken(TokenType::MINUSEQ, "-="));
            m_pos += 2;
            continue;
        }
        if (matchSequence("+=")) {
            tokens.push_back(makeToken(TokenType::PLUSEQ, "+="));
            m_pos += 2;
            continue;
        }
        if (matchSequence("*=")) {
            tokens.push_back(makeToken(TokenType::MULTIPLYEQ, "*="));
            m_pos += 2;
            continue;
        }
        if (matchSequence("/=")) {
            tokens.push_back(makeToken(TokenType::DIVIDEEQ, "/="));
            m_pos += 2;
            continue;
        }
        if (matchSequence("%=")) {
            tokens.push_back(makeToken(TokenType::MODULUSEQ, "%="));
            m_pos += 2;
            continue;
        }
        if (matchSequence("^=")) {
            tokens.push_back(makeToken(TokenType::POWEREQ, "^="));
            m_pos += 2;
            continue;
        }
        if (matchSequence("==")) {
            tokens.push_back(makeToken(TokenType::EQ, "=="));
            m_pos += 2;
            continue;
        }
        if (matchSequence("<=")) {
            tokens.push_back(makeToken(TokenType::LTEQ, "<="));
            m_pos += 2;
            continue;
        }
        if (matchSequence(">=")) {
            tokens.push_back(makeToken(TokenType::GTEQ, ">="));
            m_pos += 2;
            continue;
        }
        if (matchSequence("!=")) {
            tokens.push_back(makeToken(TokenType::NEQ, "!="));
            m_pos += 2;
            continue;
        }

        if (matchSequence("&&")) {
            tokens.push_back(makeToken(TokenType::AND, "&&"));
            m_pos += 2;
            continue;
        }
        if (matchSequence("||")) {
            tokens.push_back(makeToken(TokenType::OR, "||"));
            m_pos += 2;
            continue;
        }
        if (matchSequence("..")) {
            tokens.push_back(makeToken(TokenType::DOTS, ".."));
            m_pos += 2;
            continue;
        }

        // Single-character tokens
        switch (c) {
            case '#':
                tokens.push_back(makeToken(TokenType::HASH, "#"));
                advance();
                break;
            case '@':
                tokens.push_back(makeToken(TokenType::AT, "@"));
                advance();
                break;
            case '(':
                tokens.push_back(makeToken(TokenType::LPAREN, "("));
                advance();
                break;
            case ')':
                tokens.push_back(makeToken(TokenType::RPAREN, ")"));
                advance();
                break;
            case '[':
                tokens.push_back(makeToken(TokenType::LBRACKET, "["));
                advance();
                break;
            case ']':
                tokens.push_back(makeToken(TokenType::RBRACKET, "]"));
                advance();
                break;
            case '{':
                tokens.push_back(makeToken(TokenType::LBRACE, "{"));
                advance();
                break;
            case '}':
                tokens.push_back(makeToken(TokenType::RBRACE, "}"));
                advance();
                break;
            case '+':
                tokens.push_back(makeToken(TokenType::PLUS, "+"));
                advance();
                break;
            case '*':
                tokens.push_back(makeToken(TokenType::MULTIPLY, "*"));
                advance();
                break;
            case '-':
                tokens.push_back(makeToken(TokenType::MINUS, "-"));
                advance();
                break;
            case '/':
                tokens.push_back(makeToken(TokenType::DIVIDE, "/"));
                advance();
                break;
            case '%':
                tokens.push_back(makeToken(TokenType::MODULUS, "%"));
                advance();
                break;
            case '^':
                tokens.push_back(makeToken(TokenType::POWER, "^"));
                advance();
                break;
            case '<':
                tokens.push_back(makeToken(TokenType::LT, "<"));
                advance();
                break;
            case '>':
                tokens.push_back(makeToken(TokenType::GT, ">"));
                advance();
                break;
            case '!':
                tokens.push_back(makeToken(TokenType::NOT, "!"));
                advance();
                break;
            case ',':
                tokens.push_back(makeToken(TokenType::COMMA, ","));
                advance();
                break;
            case '?':
                tokens.push_back(makeToken(TokenType::QUESTION, "?"));
                advance();
                break;
            case ':':
                tokens.push_back(makeToken(TokenType::COLON, ":"));
                advance();
                break;
            case '.':
                tokens.push_back(makeToken(TokenType::DOT, "."));
                advance();
                break;
            case '=':
                tokens.push_back(makeToken(TokenType::ASSIGN, "="));
                advance();
                break;
            case '"':
            case '\'':
                tokens.push_back(consumeStringLiteral(c));
                break;
            default:
                if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
                    Token tk = consumeIdentifier();
                    // Keyword recognition
                    if (tk.text == "show")   tk.type = TokenType::SHOW;
                    if (tk.text == "define") tk.type = TokenType::DEFINE;
                    if (tk.text == "derive") tk.type = TokenType::DERIVE;
                    if (tk.text == "unbind") tk.type = TokenType::UNBIND;
                    if (tk.text == "store") tk.type = TokenType::STORE;
                    if (tk.text == "and") tk.type = TokenType::AND;
                    if (tk.text == "or") tk.type = TokenType::OR;
                    if (tk.text == "for") tk.type = TokenType::FOR;
                    if (tk.text == "if") tk.type = TokenType::IF;
                    if (tk.text == "const") tk.type = TokenType::CONST;
                    if (tk.text == "in") tk.type = TokenType::IN;
                    if (tk.text == "break") tk.type = TokenType::BREAK;
                    if (tk.text == "continue") tk.type = TokenType::CONTINUE;
                    if (tk.text == "throw") tk.type = TokenType::THROW;
                    if (tk.text == "else") tk.type = TokenType::ELSE;
                    if (tk.text == "while") tk.type = TokenType::WHILE;
                    if (tk.text == "try") tk.type = TokenType::TRY;
                    if (tk.text == "except") tk.type = TokenType::EXCEPT;
                    if (tk.text == "element") tk.type = TokenType::ELEMENT;
                    if (tk.text == "then") tk.type = TokenType::THEN;
                    if (tk.text == "return") tk.type = TokenType::RETURN;
                    if (tk.text == "function") tk.type = TokenType::FUNCTION;
                    if (tk.text == "subscribe") tk.type = TokenType::SUBSCRIBE;
                    if (tk.text == "to") tk.type = TokenType::TO;
                    if (tk.text == "unsubscribe") tk.type = TokenType::UNSUBSCRIBE;
                    if (tk.text == "from") tk.type = TokenType::FROM;
                    if (tk.text == "object") tk.type = TokenType::OBJECT;
                    if (tk.text == "derives") tk.type = TokenType::DERIVES;
                    if (tk.text == "async") tk.type = TokenType::ASYNC;
                    if (tk.text == "import") tk.type = TokenType::IMPORT;
                    if (tk.text == "main") tk.type = TokenType::MAIN;
                    tokens.push_back(tk);
                } else if (std::isdigit(static_cast<unsigned char>(c))) {
                    Token tk = consumeNumber();
                    tokens.push_back(tk);
                    continue;
                }
                else {
                    errors.emplace_back("Error at line " +
                                std::to_string(m_line) + ", column " +
                                std::to_string(m_column) + ": " + ": Unexpected character '" + c + "'");
                    recoverFromError();
                }
        }
    }
    tokens.push_back(Token{TokenType::END_OF_FILE, "<EOF>", static_cast<int>(m_pos), m_line, m_column});
    return tokens;
}

// Retrieve any errors encountered during tokenization
const std::vector<std::string>& Lexer::getErrors() const {
    return errors;
}

// Helper Functions

bool Lexer::isEOF() const {
    return m_pos >= m_input.size();
}

char Lexer::peek() const {
    if (m_pos < m_input.size()) {
        return m_input[m_pos];
    }
    return '\0';
}

void Lexer::recoverFromError() {
    while (!isEOF()) {
        char c = peek();
        if (c == '\\' || c == '\n') {
            advance();
            break;
        }
        advance();
    }
}

void Lexer::advance() {
    if (m_pos < m_input.size()) {
        if (m_input[m_pos] == '\n') {
            ++m_line;
            m_column = 1;
        } else {
            ++m_column;
        }
        ++m_pos;
    }
}

bool Lexer::matchSequence(const std::string& seq) {
    if (m_pos + seq.size() <= m_input.size()) {
        return m_input.compare(m_pos, seq.size(), seq) == 0;
    }
    return false;
}

Token Lexer::makeToken(TokenType type, const std::string& text) {
    return Token{ type, text, static_cast<int>(m_pos), m_line, m_column };
}

Token Lexer::consumeStringLiteral(char quoteChar) {
    size_t startPos = m_pos;
    int startLine = m_line;
    int startColumn = m_column;

    advance(); // Consume opening quote

    std::string value;

    while (!isEOF() && peek() != quoteChar) {
        if (peek() == '\\') {
            advance();
            if (!isEOF()) {
                value.push_back(peek());
                advance();
            }
        } else {
            value.push_back(peek());
            advance();
        }
    }
    if (peek() != quoteChar) {
        errors.emplace_back("Unterminated string at line " + std::to_string(startLine)
            + ", column " + std::to_string(startColumn));
        return Token{TokenType::ERROR, value, static_cast<int>(startPos), startLine, startColumn};
    }
    advance(); // Consume closing quote

    return Token{TokenType::STRING_LITERAL, value, static_cast<int>(startPos), startLine, startColumn};
}

Token Lexer::consumeNumber() {
    int startPos = m_pos;
    int startLine = m_line;
    int startCol = m_column;

    bool seenDecimal = false;

    while (!isEOF()) {
        char c = peek();

        if (std::isdigit(static_cast<unsigned char>(c))) {
            // Continue reading digits
            advance();
        } else if (c == '.') {
            // Look ahead to check for '..'
            if (m_pos + 1 < static_cast<int>(m_input.size()) && m_input[m_pos + 1] == '.') {
                // Stop reading the number and leave '..' for the main lexer loop
                break;
            } else if (!seenDecimal) {
                // If we haven't seen a decimal point, this is part of the number
                seenDecimal = true;
                advance();
            } else {
                // Second decimal point is invalid in a number
                throw std::runtime_error("Invalid number format: multiple decimal points");
            }
        } else {
            // Any other character ends the number
            break;
        }
    }

    size_t length = m_pos - startPos;
    std::string value = m_input.substr(startPos, length);

    // Return the token for the number literal
    return Token{
        TokenType::NUMBER_LITERAL,
        value,
        static_cast<int>(startPos),
        startLine,
        startCol
    };
}


Token Lexer::consumeIdentifier() {
    size_t start = m_pos;
    int startLine = m_line;
    int startColumn = m_column;

    while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_') {
        advance();
    }
    std::string value = m_input.substr(start, m_pos - start);

    if (value == "true" || value == "false") {
        return Token{TokenType::BOOLEAN_LITERAL, value, static_cast<int>(start), startLine, startColumn};
    }

    return Token{TokenType::IDENTIFIER, value, static_cast<int>(start), startLine, startColumn};
}
