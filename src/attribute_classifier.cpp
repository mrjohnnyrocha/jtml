#include "jtml/attribute_classifier.h"

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace {
std::string lowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool startsWith(const std::string& value, const std::string& prefix) {
    return value.rfind(prefix, 0) == 0;
}

const std::unordered_set<std::string>& booleanAttributeNames() {
    static const std::unordered_set<std::string> names = {
        "allowfullscreen", "async", "autofocus", "autoplay", "checked",
        "controls", "default", "defer", "disabled", "formnovalidate",
        "hidden", "ismap", "loop", "multiple", "muted", "nomodule",
        "novalidate", "open", "playsinline", "readonly", "required",
        "reversed", "selected",
    };
    return names;
}
}

bool isJtmlEventAttribute(const std::string& attrName) {
    return attrName == "onClick" ||
           attrName == "onInput" ||
           attrName == "onChange" ||
           attrName == "onKeyUp" ||
           attrName == "onMouseOver" ||
           attrName == "onScroll" ||
           attrName == "onSubmit" ||
           attrName == "onDragOver" ||
           attrName == "onDrop";
}

bool jtmlEventCarriesElementValue(const std::string& attrName) {
    return attrName == "onInput" ||
           attrName == "onChange" ||
           attrName == "onKeyUp";
}

bool jtmlStaticAttributeValue(const ExpressionStatementNode* expr, std::string& value) {
    if (!expr) {
        value.clear();
        return true;
    }
    switch (expr->getExprType()) {
        case ExpressionStatementNodeType::StringLiteral: {
            const auto* literal = static_cast<const StringLiteralExpressionStatementNode*>(expr);
            value = literal->value;
            return true;
        }
        case ExpressionStatementNodeType::NumberLiteral: {
            const auto* literal = static_cast<const NumberLiteralExpressionStatementNode*>(expr);
            value = literal->toString();
            return true;
        }
        case ExpressionStatementNodeType::BooleanLiteral: {
            const auto* literal = static_cast<const BooleanLiteralExpressionStatementNode*>(expr);
            value = literal->value ? "true" : "false";
            return true;
        }
        default:
            return false;
    }
}

JtmlAttributeClassification classifyJtmlAttribute(const JtmlAttribute& attr) {
    JtmlAttributeClassification classification;
    const std::string lowerName = lowerCopy(attr.key);

    if (isJtmlEventAttribute(attr.key)) {
        classification.kind = JtmlAttributeKind::Event;
        return classification;
    }

    if (!attr.value) {
        classification.kind = JtmlAttributeKind::Boolean;
        return classification;
    }

    if (jtmlStaticAttributeValue(attr.value.get(), classification.literalValue)) {
        if (startsWith(lowerName, "data-jtml-")) {
            classification.kind = JtmlAttributeKind::Special;
        } else if (startsWith(lowerName, "data-") ||
                   startsWith(lowerName, "aria-")) {
            classification.kind = JtmlAttributeKind::Passthrough;
        } else if (booleanAttributeNames().count(lowerName) &&
                   classification.literalValue == "true") {
            classification.kind = JtmlAttributeKind::Boolean;
        } else {
            classification.kind = JtmlAttributeKind::Literal;
        }
        return classification;
    }

    classification.kind = JtmlAttributeKind::Reactive;
    return classification;
}

