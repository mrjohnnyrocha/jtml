#include "jtml/attribute_classifier.h"
#include "jtml/friendly.h"
#include "jtml/lexer.h"
#include "jtml/parser.h"
#include "jtml/transpiler.h"

#include <gtest/gtest.h>
#include <memory>
#include <string>

namespace {
Token token(TokenType type, const std::string& text) {
    return Token{type, text, 0, 1, 1};
}

JtmlAttribute stringAttr(const std::string& key, const std::string& value) {
    return JtmlAttribute(
        key,
        std::make_unique<StringLiteralExpressionStatementNode>(
            token(TokenType::STRING_LITERAL, value)));
}

JtmlAttribute variableAttr(const std::string& key, const std::string& value) {
    return JtmlAttribute(
        key,
        std::make_unique<VariableExpressionStatementNode>(
            token(TokenType::IDENTIFIER, value)));
}

size_t countOccurrences(const std::string& text, const std::string& needle) {
    size_t count = 0;
    size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}
}

TEST(AttributeClassifier, ClassifiesCoreAttributeKinds) {
    EXPECT_EQ(classifyJtmlAttribute(stringAttr("class", "card")).kind,
              JtmlAttributeKind::Literal);
    EXPECT_EQ(classifyJtmlAttribute(JtmlAttribute("disabled", nullptr)).kind,
              JtmlAttributeKind::Boolean);
    EXPECT_EQ(classifyJtmlAttribute(variableAttr("class", "cardClass")).kind,
              JtmlAttributeKind::Reactive);
    EXPECT_EQ(classifyJtmlAttribute(variableAttr("onClick", "save()")).kind,
              JtmlAttributeKind::Event);
    EXPECT_EQ(classifyJtmlAttribute(stringAttr("aria-label", "Close")).kind,
              JtmlAttributeKind::Passthrough);
    EXPECT_EQ(classifyJtmlAttribute(stringAttr("data-jtml-route", "/")).kind,
              JtmlAttributeKind::Special);
}

TEST(AttributeClassifier, LiteralAttributesDoNotBecomeReactiveBindings) {
    const std::string classic = jtml::normalizeSourceSyntax(
        "jtml 2\n"
        "let cardClass = \"active\"\n"
        "page\n"
        "  box class \"card\" style \"padding: 12px\" aria-label \"Profile card\"\n"
        "    text \"Static\"\n"
        "  box class cardClass\n"
        "    text \"Reactive\"\n",
        jtml::SyntaxMode::Friendly);

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    const std::string html = transpiler.transpile(program);

    EXPECT_EQ(html.find("data-jtml-attr-style"), std::string::npos) << html;
    EXPECT_EQ(html.find("data-jtml-attr-aria-label"), std::string::npos) << html;
    EXPECT_EQ(countOccurrences(html, "data-jtml-attr-class=\""), 1u) << html;
    EXPECT_EQ(countOccurrences(html, "data-jtml-attr-class-expr=\""), 1u) << html;
    EXPECT_NE(html.find("class=\"card\""), std::string::npos) << html;
    EXPECT_NE(html.find("style=\"padding: 12px\""), std::string::npos) << html;
    EXPECT_NE(html.find("aria-label=\"Profile card\""), std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-attr-class"), std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-attr-class-expr=\"cardClass\""), std::string::npos) << html;
}
