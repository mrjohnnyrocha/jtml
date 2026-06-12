#include "jtml/transpiler.h"
#include "jtml/attribute_classifier.h"
#include "jtml/browser_runtime_emitter.h"
#include "jtml/client_manifest_emitter.h"
#include "jtml/expression_source.h"
#include <sstream>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <unordered_set>
#include <vector>

namespace {
std::string lowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

}

using jtml::expressionSource;

JtmlTranspiler::JtmlTranspiler() {
    // Initialize any required state for the transpiler
}

void JtmlTranspiler::setWebSocketPort(int port) {
    webSocketPort = port;
}

void JtmlTranspiler::setBrowserLocalRuntime(bool enabled) {
    browserLocalRuntime = enabled;
}

void JtmlTranspiler::setRuntimeProjectPlan(const jtml::RuntimeProjectPlan& plan) {
    runtimeProjectPlan = plan;
}

std::string JtmlTranspiler::transpile(const std::vector<std::unique_ptr<ASTNode>>& program) {
    uniqueElemId = 0;
    uniqueVarId  = 0;
    nodeID = 0;
    nodeDerivedMap.clear();
    nodeBindings.clear();
    attributeBindings.clear();

    std::ostringstream out;
    out << "<!DOCTYPE html>\n<html>\n<head>\n"
        << "  <meta charset=\"utf-8\">\n"
        << "  <title>JTML Final Example</title>\n"
        << "</head>\n<body data-jtml-app>\n";

    // Top-level statements => insideElement=false
    for (auto& node : program) {
        out << transpileNode(*node, /*insideElement=*/false);
    }

    if (browserLocalRuntime) {
        out << generateClientManifest(program);
    }
    out << generateScriptBlock();
    out << "\n</body>\n</html>\n";
    return out.str();
}

const std::string* JtmlTranspiler::getNodeBinding(const ASTNode& node, const std::string& role) const {
    auto nodeIt = nodeBindings.find(&node);
    if (nodeIt == nodeBindings.end()) {
        return nullptr;
    }

    auto roleIt = nodeIt->second.find(role);
    if (roleIt == nodeIt->second.end()) {
        return nullptr;
    }

    return &roleIt->second;
}

const TranspiledBinding* JtmlTranspiler::getAttributeBinding(const JtmlAttribute& attr) const {
    auto attrIt = attributeBindings.find(&attr);
    if (attrIt == attributeBindings.end()) {
        return nullptr;
    }

    return &attrIt->second;
}

//--------------------------------------------------
// Distinguish node type and top-level vs. inside-element
//--------------------------------------------------
std::string JtmlTranspiler::transpileNode(const ASTNode& node, bool insideElement) {
    switch (node.getType()) {
    case ASTNodeType::JtmlElement:
        // For an element, we always do transpileElement
        return transpileElement(static_cast<const JtmlElementNode&>(node));

    case ASTNodeType::IfStatement: {
        const auto& ifNode = static_cast<const IfStatementNode&>(node);
        return insideElement ?
            transpileIfInsideElement(ifNode) :
            transpileIfTopLevel(ifNode);
    }
    case ASTNodeType::ForStatement: {
        const auto& forNode = static_cast<const ForStatementNode&>(node);
        return insideElement ?
            transpileForInsideElement(forNode) :
            transpileForTopLevel(forNode);
    }
    case ASTNodeType::WhileStatement: {
        const auto& whileNode = static_cast<const WhileStatementNode&>(node);
        return insideElement ?
            transpileWhileInsideElement(whileNode) :
            transpileWhileTopLevel(whileNode);
    }
    case ASTNodeType::ShowStatement:
        return transpileShow(static_cast<const ShowStatementNode&>(node));

    // For define, function, class, etc. we might produce minimal placeholders
    // or skip
    default:
        return "<!-- " + node.toString() + " not explicitly transpiled. -->\n";
    }
}

//--------------------------------------------------
// Transpile an element (with attributes + content)
//--------------------------------------------------
std::string JtmlTranspiler::transpileElement(const JtmlElementNode& elem) {
    if (lowerCopy(elem.tagName) == "raw") {
        std::ostringstream raw;
        for (const auto& child : elem.content) {
            if (child->getType() == ASTNodeType::ShowStatement) {
                const auto& show = static_cast<const ShowStatementNode&>(*child);
                if (show.expr && show.expr->getExprType() == ExpressionStatementNodeType::StringLiteral) {
                    const auto* literal = static_cast<const StringLiteralExpressionStatementNode*>(show.expr.get());
                    raw << literal->value;
                    if (literal->value.empty() || literal->value.back() != '\n') raw << "\n";
                    continue;
                }
                if (show.expr) raw << show.expr->toString() << "\n";
            } else {
                raw << "<!-- Unsupported statement inside JTML raw block: "
                    << escapeHTML(child->toString()) << " -->\n";
            }
        }
        return raw.str();
    }

    if (lowerCopy(elem.tagName) == "style") {
        std::ostringstream style;
        style << "<style>\n";
        for (const auto& child : elem.content) {
            if (child->getType() == ASTNodeType::ShowStatement) {
                const auto& show = static_cast<const ShowStatementNode&>(*child);
                if (show.expr && show.expr->getExprType() == ExpressionStatementNodeType::StringLiteral) {
                    const auto* literal = static_cast<const StringLiteralExpressionStatementNode*>(show.expr.get());
                    style << literal->value;
                    if (literal->value.empty() || literal->value.back() != '\n') style << "\n";
                    continue;
                }
                if (show.expr) style << show.expr->toString() << "\n";
            } else {
                style << "/* Unsupported statement inside JTML style block: "
                      << escapeHTML(child->toString()) << " */\n";
            }
        }
        style << "</style>\n";
        return style.str();
    }

    ++uniqueElemId;
    nodeID++;
    std::string domId = "elem_" + std::to_string(uniqueElemId);

    std::ostringstream out;
    out << "<" << elem.tagName << " id=\"" << domId << "\"";

    // Transpile attributes using the semantic classifier: most attributes are
    // static platform data, while only events and real expressions need runtime
    // bindings.
    for (auto& attr : elem.attributes) {
        const auto classification = classifyJtmlAttribute(attr);

       if (classification.kind == JtmlAttributeKind::Event) {
            ++uniqueVarId;
            std::string derivedVarName = "attr_" + std::to_string(uniqueVarId);

            // Store in nodeDerivedMap
            nodeDerivedMap[nodeID][attr.key] = derivedVarName;
            attributeBindings[&attr] = {derivedVarName, derivedVarName};
            // Ensure that the attribute value represents the JTML function or handler
            std::string functionCall = escapeHTML(escapeJS(expressionSource(attr.value.get())));

            // Handle browser events that provide a useful value to JTML.
            std::ostringstream args;
            if (jtmlEventCarriesElementValue(attr.key)) {
                args << ", window.jtmlEventValue(event)";
            } else if (attr.key == "onScroll") {
                args << ", String(window.scrollY)"; // Pass the scroll position as an argument
            }

            std::string htmlEventName = attr.key;
            std::transform(htmlEventName.begin(), htmlEventName.end(), htmlEventName.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });

            // Generate the event handler. JTML uses readable names such as
            // onInput, while HTML event attributes are most reliable lower-case.
            out << " " << htmlEventName << "=\"sendEvent('" << derivedVarName << "', '" << attr.key << "', ['" << functionCall << "'" << args.str() << "])\"";

        } else {
            if (classification.kind == JtmlAttributeKind::Boolean) {
                out << " " << attr.key;
                continue;
            }
            if (classification.kind == JtmlAttributeKind::Literal ||
                classification.kind == JtmlAttributeKind::Passthrough ||
                classification.kind == JtmlAttributeKind::Special) {
                out << " " << attr.key << "=\"" << escapeHTML(classification.literalValue) << "\"";
                continue;
            }

            ++uniqueVarId;
            std::string derivedVarName = "attr_" + std::to_string(uniqueVarId);

            // Store in nodeDerivedMap
            nodeDerivedMap[nodeID][attr.key] = derivedVarName;
            attributeBindings[&attr] = {derivedVarName, domId};

            // Add enough metadata for both live bindings and browser-local
            // builds. The live backend still owns the generated binding name;
            // the browser-local runtime evaluates the original source expr.
            out << " data-jtml-attr-" << attr.key << "=\"" << derivedVarName << "\"";
            if (attr.value) {
                out << " data-jtml-attr-" << attr.key << "-expr=\""
                    << escapeHTML(expressionSource(attr.value.get())) << "\"";
            }
        }
    }

    const bool voidTag = isVoidElement(elem.tagName);
    if (voidTag && elem.content.empty()) {
        out << ">\n";
    } else {
        out << ">";
        // Child statements => insideElement=true
        out << transpileChildren(elem.content, /*insideElement=*/true);
        out << "</" << elem.tagName << ">\n";
    }

    return out.str();
}

//--------------------------------------------------
// If top-level => minimal or comment
//--------------------------------------------------
std::string JtmlTranspiler::transpileIfTopLevel(const IfStatementNode& node) {
    return "<!-- IfStatement at top-level: server logic only -->\n";
}

//--------------------------------------------------
// If inside an element => data-jtml-if
//--------------------------------------------------
std::string JtmlTranspiler::transpileIfInsideElement(const IfStatementNode& node) {
    ++uniqueVarId;
    nodeID++;
    std::string condName = "cond_" + std::to_string(uniqueVarId);
    nodeDerivedMap[nodeID]["if"] = condName;
    nodeBindings[&node]["if"] = condName;

    // transpile "then" block
    std::ostringstream thenSS;
    for (auto& stmt : node.thenStatements) {
        thenSS << transpileNode(*stmt, /*insideElement=*/true);
    }
    std::string thenHTML = escapeHTML(thenSS.str());

    // transpile "else" block
    std::string elseHTML;
    if (!node.elseStatements.empty()) {
        std::ostringstream elseSS;
        for (auto& stmt : node.elseStatements) {
            elseSS << transpileNode(*stmt, /*insideElement=*/true);
        }
        elseHTML = escapeHTML(elseSS.str());
    }

    std::ostringstream out;
    out << "<div data-jtml-if=\"" << condName << "\" "
        << "data-jtml-cond-expr=\"" << escapeHTML(expressionSource(node.condition.get())) << "\" "
        << "data-then=\"" << thenHTML << "\" "
        << "data-else=\"" << elseHTML << "\">"
        << "</div>\n";

    return out.str();
}

//--------------------------------------------------
// For top-level => minimal
//--------------------------------------------------
std::string JtmlTranspiler::transpileForTopLevel(const ForStatementNode& node) {
    return "<!-- ForStatement at top-level: server logic only -->\n";
}

//--------------------------------------------------
// For inside element => data-jtml-for
//--------------------------------------------------
std::string JtmlTranspiler::transpileForInsideElement(const ForStatementNode& node) {
    // 1) Generate a unique name for the loop's iterable (to store in the environment or for the front-end)
    ++uniqueVarId;
    nodeID++;
    std::string rangeName = "range_" + std::to_string(uniqueVarId);

    // 2) Register in nodeDerivedMap
    //    "for" => the derived variable for the iterable
    //    "iteratorName" => node.iteratorName
    nodeDerivedMap[nodeID]["for"] = rangeName;
    nodeDerivedMap[nodeID]["iteratorName"] = node.iteratorName;
    nodeBindings[&node]["for"] = rangeName;
    nodeBindings[&node]["iteratorName"] = node.iteratorName;

    // 3) Transpile the loop body statements
    std::ostringstream bodyStream;
    for (auto& stmt : node.body) {
        // Recursively transpile each child statement inside the loop
        bodyStream << transpileNode(*stmt, /*insideElement=*/true);
    }
    std::string escapedBody = escapeHTML(bodyStream.str());

    // 4) Produce final HTML with data-jtml-for and data-jtml-iterator
    std::ostringstream out;
    out << "<div id=\"" << rangeName << "\" data-jtml-for=\"" << rangeName
        << "\" data-jtml-iterator=\"" << node.iteratorName
        << "\" data-jtml-for-expr=\"" << escapeHTML(expressionSource(node.iterableExpression.get()))
        << "\" data-body=\"" << escapedBody << "\"></div>\n";

    return out.str();
}

//--------------------------------------------------
// While top-level => minimal
//--------------------------------------------------
std::string JtmlTranspiler::transpileWhileTopLevel(const WhileStatementNode& node) {
    return "<!-- WhileStatement at top-level: server logic only -->\n";
}

//--------------------------------------------------
// While inside element => data-jtml-while
//--------------------------------------------------
std::string JtmlTranspiler::transpileWhileInsideElement(const WhileStatementNode& node) {
    ++uniqueVarId;
    nodeID++;
    std::string condName = "cond_" + std::to_string(uniqueVarId);

    nodeDerivedMap[nodeID]["while"] = condName;
    nodeBindings[&node]["while"] = condName;

    std::ostringstream bodySS;
    for (auto& stmt : node.body) {
        bodySS << transpileNode(*stmt, /*insideElement=*/true);
    }
    std::string bodyHTML = escapeHTML(bodySS.str());

    std::ostringstream out;
    out << "<div data-jtml-while=\"" << condName << "\" "
        << "data-jtml-cond-expr=\"" << escapeHTML(expressionSource(node.condition.get())) << "\" "
        << "data-body=\"" << bodyHTML << "\">"
        << "</div>\n";
    return out.str();
}

//--------------------------------------------------
// Show => produce placeholders
//--------------------------------------------------
std::string JtmlTranspiler::transpileShow(const ShowStatementNode& node) {
    if(!node.expr) {
        return "<p><!-- show with no expr? --></p>\n";
    }
    ++uniqueVarId;
    nodeID++;
    std::string exprVarName = "expr_" + std::to_string(uniqueVarId);

    nodeDerivedMap[nodeID]["show"] = exprVarName;
    nodeBindings[&node]["show"] = exprVarName;

    // produce placeholder
    // e.g. <p>{{someExpr}}</p>
    const std::string clientExpr = expressionSource(node.expr.get());
    std::string placeholder = "{{" + clientExpr + "}}";

    std::ostringstream out;
    out << "<div id=\""<< exprVarName << "\" data-jtml-expr=\""
        << escapeHTML(clientExpr) << "\">" << placeholder << "</div>\n";
    return out.str();
}

//--------------------------------------------------
// Helper to transpile a list of child statements
//--------------------------------------------------
std::string JtmlTranspiler::transpileChildren(const std::vector<std::unique_ptr<ASTNode>>& children, bool insideElement) {
    std::ostringstream out;
    for (auto& c : children) {
        out << transpileNode(*c, insideElement);
    }
    return out.str();
}

std::string JtmlTranspiler::generateClientManifest(const std::vector<std::unique_ptr<ASTNode>>& program) {
    if (runtimeProjectPlan) {
        return jtml::emitClientManifestScript(*runtimeProjectPlan);
    }
    return jtml::emitClientManifestScript(program);
}

//--------------------------------------------------
// Insert a minimal script
//--------------------------------------------------
std::string JtmlTranspiler::generateScriptBlock() {
    return jtml::emitBrowserRuntimeScript(webSocketPort, browserLocalRuntime);
}

bool JtmlTranspiler::isVoidElement(const std::string& tagName) const {
    static const std::unordered_set<std::string> voidElements = {
        "area", "base", "br", "col", "embed", "hr", "img", "input",
        "link", "meta", "param", "source", "track", "wbr"
    };
    std::string lower = tagName;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return voidElements.find(lower) != voidElements.end();
}

// Utility to escape HTML for data attributes and inner content
std::string JtmlTranspiler::escapeHTML(const std::string& input) {
    std::string out;
    for (char c : input) {
        switch(c) {
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '&': out += "&amp;"; break;
            case '\"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default:
                out += c;
        }
    }
    return out;
}

// Utility to escape JavaScript strings
std::string JtmlTranspiler::escapeJS(const std::string& input) {
    std::string out;
    for (char c : input) {
        switch(c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '/': out += "\\/"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if ('\x00' <= c && c <= '\x1f') {
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}
