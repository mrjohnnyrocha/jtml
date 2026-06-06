#include "jtml/expression_source.h"

#include <sstream>

namespace jtml {

std::string jsonString(const std::string& value) {
    std::ostringstream out;
    out << '"';
    for (char ch : value) {
        switch (ch) {
            case '\\': out << "\\\\"; break;
            case '"':  out << "\\\""; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            case '<':  out << "\\u003c"; break;
            case '>':  out << "\\u003e"; break;
            case '&':  out << "\\u0026"; break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    out << "\\u00";
                    const char* hex = "0123456789abcdef";
                    out << hex[(ch >> 4) & 0xf] << hex[ch & 0xf];
                } else {
                    out << ch;
                }
        }
    }
    out << '"';
    return out.str();
}

std::string expressionSource(const ExpressionStatementNode* expr) {
    if (!expr) return "";
    switch (expr->getExprType()) {
        case ExpressionStatementNodeType::StringLiteral: {
            const auto* literal = static_cast<const StringLiteralExpressionStatementNode*>(expr);
            return jsonString(literal->value);
        }
        case ExpressionStatementNodeType::NumberLiteral:
        case ExpressionStatementNodeType::BooleanLiteral:
        case ExpressionStatementNodeType::Variable:
            return expr->toString();
        case ExpressionStatementNodeType::Binary: {
            const auto* binary = static_cast<const BinaryExpressionStatementNode*>(expr);
            return "(" + expressionSource(binary->left.get()) + " " + binary->op + " " +
                   expressionSource(binary->right.get()) + ")";
        }
        case ExpressionStatementNodeType::Unary: {
            const auto* unary = static_cast<const UnaryExpressionStatementNode*>(expr);
            return unary->op + expressionSource(unary->right.get());
        }
        case ExpressionStatementNodeType::EmbeddedVariable: {
            const auto* embedded = static_cast<const EmbeddedVariableExpressionStatementNode*>(expr);
            return expressionSource(embedded->embeddedExpression.get());
        }
        case ExpressionStatementNodeType::CompositeString: {
            const auto* composite = static_cast<const CompositeStringExpressionStatementNode*>(expr);
            std::ostringstream out;
            for (size_t i = 0; i < composite->parts.size(); ++i) {
                if (i) out << " + ";
                out << expressionSource(composite->parts[i].get());
            }
            return out.str();
        }
        case ExpressionStatementNodeType::ArrayLiteral: {
            const auto* array = static_cast<const ArrayLiteralExpressionStatementNode*>(expr);
            std::ostringstream out;
            out << '[';
            for (size_t i = 0; i < array->elements.size(); ++i) {
                if (i) out << ", ";
                out << expressionSource(array->elements[i].get());
            }
            out << ']';
            return out.str();
        }
        case ExpressionStatementNodeType::DictionaryLiteral: {
            const auto* dict = static_cast<const DictionaryLiteralExpressionStatementNode*>(expr);
            std::ostringstream out;
            out << '{';
            for (size_t i = 0; i < dict->entries.size(); ++i) {
                if (i) out << ", ";
                out << jsonString(dict->entries[i].key.text) << ": "
                    << expressionSource(dict->entries[i].value.get());
            }
            out << '}';
            return out.str();
        }
        case ExpressionStatementNodeType::Subscript: {
            const auto* subscript = static_cast<const SubscriptExpressionStatementNode*>(expr);
            return expressionSource(subscript->base.get()) + "[" +
                   expressionSource(subscript->index.get()) + "]";
        }
        case ExpressionStatementNodeType::FunctionCall: {
            const auto* call = static_cast<const FunctionCallExpressionStatementNode*>(expr);
            std::ostringstream out;
            out << call->functionName << "(";
            for (size_t i = 0; i < call->arguments.size(); ++i) {
                if (i) out << ", ";
                out << expressionSource(call->arguments[i].get());
            }
            out << ")";
            return out.str();
        }
        case ExpressionStatementNodeType::Conditional: {
            const auto* conditional = static_cast<const ConditionalExpressionStatementNode*>(expr);
            return expressionSource(conditional->condition.get()) + " ? " +
                   expressionSource(conditional->whenTrue.get()) + " : " +
                   expressionSource(conditional->whenFalse.get());
        }
        case ExpressionStatementNodeType::ObjectPropertyAccess: {
            const auto* access = static_cast<const ObjectPropertyAccessExpressionNode*>(expr);
            return expressionSource(access->base.get()) + "." + access->propertyName;
        }
        case ExpressionStatementNodeType::ObjectMethodCall: {
            const auto* call = static_cast<const ObjectMethodCallExpressionNode*>(expr);
            std::ostringstream out;
            out << expressionSource(call->base.get()) << "." << call->methodName << "(";
            for (size_t i = 0; i < call->arguments.size(); ++i) {
                if (i) out << ", ";
                out << expressionSource(call->arguments[i].get());
            }
            out << ")";
            return out.str();
        }
    }
    return expr->toString();
}

} // namespace jtml
