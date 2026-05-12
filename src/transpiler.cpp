#include "jtml/transpiler.h"
#include <sstream>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace {
std::string lowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool isSupportedEventAttribute(const std::string& attrName) {
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

bool eventCarriesElementValue(const std::string& attrName) {
    return attrName == "onInput" ||
           attrName == "onChange" ||
           attrName == "onKeyUp";
}

bool staticAttributeValue(const ExpressionStatementNode* expr, std::string& value) {
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
}

JtmlTranspiler::JtmlTranspiler() {
    // Initialize any required state for the transpiler
}

void JtmlTranspiler::setWebSocketPort(int port) {
    webSocketPort = port;
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

    // Transpile attributes, adding reactivity for expressions
    for (auto& attr : elem.attributes) {
        std::string valStr = attr.value->toString();

       if (isSupportedEventAttribute(attr.key)) {
            ++uniqueVarId;
            std::string derivedVarName = "attr_" + std::to_string(uniqueVarId);

            // Store in nodeDerivedMap
            nodeDerivedMap[nodeID][attr.key] = derivedVarName;
            attributeBindings[&attr] = {derivedVarName, derivedVarName};
            // Ensure that the attribute value represents the JTML function or handler
            std::string functionCall = escapeJS(attr.value->toString());

            // Handle browser events that provide a useful value to JTML.
            std::ostringstream args;
            if (eventCarriesElementValue(attr.key)) {
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
            std::string staticValue;
            if (staticAttributeValue(attr.value.get(), staticValue)) {
                if (!attr.value) {
                    out << " " << attr.key;
                } else {
                    out << " " << attr.key << "=\"" << escapeHTML(staticValue) << "\"";
                }
                continue;
            }

            ++uniqueVarId;
            std::string derivedVarName = "attr_" + std::to_string(uniqueVarId);

            // Store in nodeDerivedMap
            nodeDerivedMap[nodeID][attr.key] = derivedVarName;
            attributeBindings[&attr] = {derivedVarName, domId};

            // Add a data attribute for the front-end to identify
            out << " data-jtml-attr-" << attr.key << "=\"" << derivedVarName << "\"";
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
        << "data-jtml-cond-expr=\"" << escapeHTML(node.condition ? node.condition->toString() : "") << "\" "
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
        << "\" data-jtml-for-expr=\"" << escapeHTML(node.iterableExpression ? node.iterableExpression->toString() : "")
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
        << "data-jtml-cond-expr=\"" << escapeHTML(node.condition ? node.condition->toString() : "") << "\" "
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
    std::string placeholder = "{{" + node.expr->toString() + "}}";

    std::ostringstream out;
    out << "<div id=\""<< exprVarName << "\" data-jtml-expr=\""
        << escapeHTML(node.expr->toString()) << "\">" << placeholder << "</div>\n";
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

//--------------------------------------------------
// Insert a minimal script
//--------------------------------------------------
std::string JtmlTranspiler::generateScriptBlock() {
    // The runtime script has three layers, in order of resilience:
    //
    //   1. `window.__jtml_bindings`  — optional JSON blob injected by the
    //      server before this script tag. Applied on DOMContentLoaded so the
    //      page renders with correct initial values even if no network is
    //      available.
    //   2. WebSocket — watch-mode reloads and future streaming updates.
    //   3. HTTP event dispatch — `fetch('/api/event', ...)`. This is the
    //      authoritative path for user events because it returns a complete
    //      bindings snapshot immediately.
    //
    // Events (`sendEvent`) use HTTP so local state, stores, component
    // instances, and conditionals are deterministic in every browser.
    std::ostringstream out;
    out << R"(
  <script>
    (function () {
      const wsPort = )" << webSocketPort << R"(;

      function reportStatus(state, message) {
        document.documentElement.dataset.jtmlStatus = state;
        if (message) document.documentElement.dataset.jtmlMessage = message;
        if (state === 'error') console.error('[jtml] ' + (message || 'runtime error'));
        try {
          if (window.parent && window.parent !== window) {
            window.parent.postMessage({
              type: 'jtml:runtime-status',
              state: state,
              message: message || ''
            }, '*');
          }
        } catch (_) {}
      }

      const clientState = {};
      const __jtml_extern_fns = {};
      const __jtml_media_actions = {};
      let componentInstances = [];
      let componentDefinitions = [];

      window.jtmlEventValue = function (event) {
        const target = event && event.target;
        if (!target) return '';
        const type = String(target.type || '').toLowerCase();
        if (type === 'checkbox') return !!target.checked;
        if (type === 'file') {
          const files = Array.prototype.slice.call(target.files || []).map(function (file) {
            const preview = (file && typeof URL !== 'undefined' && URL.createObjectURL)
              ? URL.createObjectURL(file)
              : '';
            return {
              name: file.name || '',
              type: file.type || '',
              size: file.size || 0,
              lastModified: file.lastModified || 0,
              preview: preview,
              url: preview
            };
          });
          if (target.multiple) return files;
          return files[0] || null;
        }
        return target.value;
      };

      function parseComponentMap(value) {
        const map = {};
        String(value || '').split(';').forEach(function (entry) {
          if (!entry) return;
          const pos = entry.indexOf('=');
          if (pos === -1) {
            map[entry] = '';
            return;
          }
          map[entry.slice(0, pos)] = entry.slice(pos + 1);
        });
        return map;
      }

      function scanComponentInstances() {
        componentInstances = Array.prototype.slice.call(
          document.querySelectorAll('[data-jtml-instance]')
        ).map(function (el) {
          return {
            id: el.getAttribute('data-jtml-instance') || '',
            component: el.getAttribute('data-jtml-component') || '',
            instanceId: Number(el.getAttribute('data-jtml-instance-id') || 0),
            role: el.getAttribute('data-jtml-component-role') || 'component',
            params: parseComponentMap(el.getAttribute('data-jtml-component-params') || ''),
            locals: parseComponentMap(el.getAttribute('data-jtml-component-locals') || ''),
            sourceLine: Number(el.getAttribute('data-jtml-source-line') || 0),
            element: el
          };
        });
        componentDefinitions = Array.prototype.slice.call(
          document.querySelectorAll('[data-jtml-component-def]')
        ).map(function (el) {
          return {
            name: el.getAttribute('data-jtml-component-def') || '',
            params: String(el.getAttribute('data-jtml-component-def-params') || '').split(';').filter(Boolean),
            sourceLine: Number(el.getAttribute('data-jtml-source-line') || 0),
            body: decodeHex(el.getAttribute('data-jtml-component-body-hex') || ''),
            element: el
          };
        });
        window.__jtml_components = componentInstances;
        window.__jtml_component_definitions = componentDefinitions;
        window.jtml = Object.assign(window.jtml || {}, {
          components: componentInstances,
          componentDefinitions: componentDefinitions,
          getComponentInstances: function () { return componentInstances.slice(); },
          getComponentDefinitions: function () { return componentDefinitions.slice(); },
          findComponentInstance: function (id) {
            return componentInstances.find(function (item) { return item.id === id; }) || null;
          },
          findComponentDefinition: function (name) {
            return componentDefinitions.find(function (item) { return item.name === name; }) || null;
          }
        });
        document.dispatchEvent(new CustomEvent('jtml:components-ready', {
          detail: { components: componentInstances, componentDefinitions: componentDefinitions }
        }));
      }

      function decodeHex(hex) {
        hex = String(hex || '');
        if (hex.length % 2 !== 0) return '';
        let out = '';
        for (let i = 0; i < hex.length; i += 2) {
          const code = parseInt(hex.slice(i, i + 2), 16);
          if (!Number.isFinite(code)) return '';
          out += String.fromCharCode(code);
        }
        return out;
      }

      function normalizeClientExpr(expr) {
        expr = String(expr || '').trim();
        const unwrapPaths = function (value) {
          let previous = '';
          while (previous !== value) {
            previous = value;
            value = value.replace(/\(([A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z_][A-Za-z0-9_]*)*)\)/g, '$1');
          }
          return value;
        };
        const hasWrappingParens = function (value) {
          if (value.length < 3 || value[0] !== '(' || value[value.length - 1] !== ')') return false;
          let depth = 0;
          for (let i = 0; i < value.length; i += 1) {
            const ch = value[i];
            if (ch === '(') depth += 1;
            else if (ch === ')') depth -= 1;
            if (depth === 0 && i < value.length - 1) return false;
          }
          return depth === 0 && value.slice(1, -1).trim().length > 0;
        };
        expr = unwrapPaths(expr);
        while (hasWrappingParens(expr)) {
          expr = expr.slice(1, -1).trim();
          expr = unwrapPaths(expr);
        }
        return expr;
      }

      function deepGet(scope, path) {
        const parts = normalizeClientExpr(path).split('.').filter(Boolean);
        if (!parts.length || !Object.prototype.hasOwnProperty.call(scope, parts[0])) {
          return { found: false, value: undefined };
        }
        let value = scope[parts[0]];
        for (let i = 1; i < parts.length; i += 1) {
          if (value == null || !Object.prototype.hasOwnProperty.call(Object(value), parts[i])) {
            return { found: false, value: undefined };
          }
          value = value[parts[i]];
        }
        return { found: true, value: value };
      }

      function evaluateClientExpression(expr) {
        expr = normalizeClientExpr(expr);
        if (!expr) return { found: false, value: undefined };
        const plusParts = splitTopLevelOperator(expr, '+');
        if (plusParts.length > 1) {
          let out = '';
          for (let i = 0; i < plusParts.length; i += 1) {
            const part = evaluateClientExpression(plusParts[i]);
            if (!part.found) {
              return { found: false, value: undefined };
            }
            out += renderTemplateValue(part.value);
          }
          return { found: true, value: out };
        }
        if (expr === 'true') return { found: true, value: true };
        if (expr === 'false') return { found: true, value: false };
        if ((expr[0] === '"' && expr[expr.length - 1] === '"') ||
            (expr[0] === "'" && expr[expr.length - 1] === "'")) {
          return { found: true, value: expr.slice(1, -1) };
        }
        if (/^-?\d+(?:\.\d+)?$/.test(expr)) {
          return { found: true, value: Number(expr) };
        }
        if (/^[A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z_][A-Za-z0-9_]*)*$/.test(expr)) {
          return deepGet(clientState, expr);
        }
        return { found: true, value: expr };
      }

      function splitTopLevelOperator(source, op) {
        const parts = [];
        let current = '';
        let quote = '';
        let depth = 0;
        for (let i = 0; i < source.length; i += 1) {
          const ch = source[i];
          if (quote) {
            current += ch;
            if (ch === '\\' && i + 1 < source.length) current += source[++i];
            else if (ch === quote) quote = '';
            continue;
          }
          if (ch === '"' || ch === "'") {
            quote = ch;
            current += ch;
            continue;
          }
          if (ch === '{' || ch === '[' || ch === '(') depth += 1;
          if (ch === '}' || ch === ']' || ch === ')') depth -= 1;
          if (ch === op && depth === 0) {
            parts.push(current.trim());
            current = '';
          } else {
            current += ch;
          }
        }
        if (parts.length) parts.push(current.trim());
        return parts.filter(function (part) { return part.length > 0; });
      }

      function splitTopLevelList(source) {
        const parts = [];
        let current = '';
        let quote = '';
        let depth = 0;
        for (let i = 0; i < source.length; i += 1) {
          const ch = source[i];
          if (quote) {
            current += ch;
            if (ch === '\\' && i + 1 < source.length) current += source[++i];
            else if (ch === quote) quote = '';
            continue;
          }
          if (ch === '"' || ch === "'") {
            quote = ch;
            current += ch;
            continue;
          }
          if (ch === '{' || ch === '[' || ch === '(') depth += 1;
          if (ch === '}' || ch === ']' || ch === ')') depth -= 1;
          if (ch === ',' && depth === 0) {
            if (current.trim()) parts.push(current.trim());
            current = '';
          } else {
            current += ch;
          }
        }
        if (current.trim()) parts.push(current.trim());
        return parts;
      }

      function evaluateClientBodyExpression(expr) {
        expr = normalizeClientExpr(expr);
        if (!expr) return { found: false, value: undefined };
        if (expr[0] === '{' && expr[expr.length - 1] === '}') {
          const body = {};
          const inner = expr.slice(1, -1).trim();
          if (!inner) return { found: true, value: body };
          splitTopLevelList(inner).forEach(function (entry) {
            const colon = entry.indexOf(':');
            const rawKey = colon === -1 ? entry : entry.slice(0, colon).trim();
            const key = rawKey.replace(/^['"]|['"]$/g, '');
            const valueExpr = colon === -1 ? rawKey : entry.slice(colon + 1).trim();
            const value = evaluateClientExpression(valueExpr);
            body[key] = value.found ? value.value : valueExpr;
          });
          return { found: true, value: body };
        }
        return evaluateClientExpression(expr);
      }

      function applyClientState() {
        applyClientExpressions();

        for (let pass = 0; pass < 5; pass += 1) {
          let changed = false;

          document.querySelectorAll('[data-jtml-cond-expr]').forEach(function (el) {
            const result = evaluateClientExpression(el.getAttribute('data-jtml-cond-expr'));
            if (!result.found) return;
            const source = result.value ? el.getAttribute('data-then') || el.getAttribute('data-body') || '' : el.getAttribute('data-else') || '';
            if (el.dataset.jtmlRendered !== source) {
              el.innerHTML = source;
              el.dataset.jtmlRendered = source;
              changed = true;
            }
          });

          document.querySelectorAll('[data-jtml-for-expr]').forEach(function (el) {
            const result = evaluateClientExpression(el.getAttribute('data-jtml-for-expr'));
            if (!result.found) return;
            const iterator = el.getAttribute('data-jtml-iterator') || 'item';
            const body = el.getAttribute('data-body') || '';
            let values = result.value;
            if (values == null) values = [];
            if (!Array.isArray(values)) {
              if (typeof values === 'string') values = values.split('');
              else if (typeof values === 'object') values = Object.values(values);
              else values = [values];
            }
            const html = values.map(function (item) { return renderLoopBody(body, iterator, item); }).join('');
            if (el.dataset.jtmlRendered !== html) {
              el.innerHTML = html;
              el.dataset.jtmlRendered = html;
              changed = true;
            }
          });

          applyClientExpressions();
          if (!changed) break;
        }
        renderCharts();
        processImageBindings();
      }

      function applyClientExpressions() {
        document.querySelectorAll('[data-jtml-expr]').forEach(function (el) {
          const result = evaluateClientExpression(el.getAttribute('data-jtml-expr'));
          if (result.found) el.textContent = renderTemplateValue(result.value);
        });
      }

      const __jtml_refresh_fns = {};
      const __jtml_fetch_fns = {};
      const __jtml_invalidate_fns = {};

      async function executeFetch(name, url, method, bodyExpr, cachePolicy, credentialsPolicy, timeoutMs, retryCount, stalePolicy) {
        const previous = clientState[name] || { data: [] };
        const keepStale = stalePolicy === 'keep';
        clientState[name] = { loading: true, data: keepStale ? previous.data : [], error: '', stale: keepStale };
        applyClientState();
        let lastError = null;
        const maxRetries = Math.max(0, Number(retryCount || 0) || 0);
        for (let attempt = 0; attempt <= maxRetries; attempt += 1) {
          const options = { method: method };
          let controller = null;
          let timeoutId = null;
          try {
            if (cachePolicy) options.cache = cachePolicy;
            if (credentialsPolicy) options.credentials = credentialsPolicy;
            if (timeoutMs) {
              controller = new AbortController();
              options.signal = controller.signal;
              timeoutId = setTimeout(function () { controller.abort(); }, Math.max(1, Number(timeoutMs) || 1));
            }
            if (bodyExpr) {
              const body = evaluateClientBodyExpression(bodyExpr);
              options.headers = Object.assign({ 'content-type': 'application/json' }, options.headers || {});
              options.body = JSON.stringify(body.found ? body.value : bodyExpr);
            }
            const response = await fetch(url, options);
            if (timeoutId) clearTimeout(timeoutId);
            const type = response.headers.get('content-type') || '';
            const payload = type.indexOf('application/json') !== -1 ? await response.json() : await response.text();
            if (!response.ok) throw new Error(response.status + ' ' + response.statusText);
            clientState[name] = { loading: false, data: payload, error: '', stale: false, attempts: attempt + 1 };
            lastError = null;
            break;
          } catch (err) {
            if (timeoutId) clearTimeout(timeoutId);
            lastError = err;
            if (attempt < maxRetries) continue;
          }
        }
        if (lastError) {
          clientState[name] = {
            loading: false,
            data: keepStale ? previous.data : [],
            error: lastError && lastError.name === 'AbortError'
              ? 'Fetch timed out'
              : (lastError && lastError.message ? lastError.message : String(lastError)),
            stale: keepStale,
            attempts: maxRetries + 1
          };
        }
        applyClientState();
      }

      function startFetchBindings() {
        document.querySelectorAll('[data-jtml-fetch]').forEach(function (marker) {
          const name = marker.getAttribute('data-jtml-fetch');
          const url = marker.getAttribute('data-url');
          const method = marker.getAttribute('data-method') || 'GET';
          const bodyExpr = marker.getAttribute('data-body-expr') || '';
          const refreshAction = marker.getAttribute('data-refresh-action') || '';
          const cachePolicy = marker.getAttribute('data-cache') || '';
          const credentialsPolicy = marker.getAttribute('data-credentials') || '';
          const timeoutMs = marker.getAttribute('data-timeout-ms') || '';
          const retryCount = marker.getAttribute('data-retry') || '';
          const stalePolicy = marker.getAttribute('data-stale') || 'clear';
          const lazy = marker.getAttribute('data-lazy') === 'true';
          if (!name || !url) return;
          __jtml_fetch_fns[name] = function () {
            return executeFetch(name, url, method, bodyExpr, cachePolicy, credentialsPolicy, timeoutMs, retryCount, stalePolicy);
          };
          if (refreshAction) {
            __jtml_refresh_fns[refreshAction] = function () {
              return __jtml_fetch_fns[name]();
            };
          }
          if (!lazy) __jtml_fetch_fns[name]();
        });
      }

      window.__jtml_redirect = function (path) {
        if (!path) return;
        const hash = path[0] === '#' ? path : '#' + (path[0] === '/' ? path : '/' + path);
        location.hash = hash;
      };

      function applyBindings(b) {
        if (!b) return;
        if (b.state) {
          for (const name in b.state) {
            clientState[name] = b.state[name];
          }
        }
        applyTemplates(b);
        if (b.content) {
          for (const id in b.content) {
            const el = document.getElementById(id);
            if (el) el.textContent = b.content[id];
          }
        }
        if (b.attributes) {
          for (const id in b.attributes) {
            const el = document.getElementById(id);
            if (!el) continue;
            for (const a in b.attributes[id]) {
              applyAttribute(el, a, b.attributes[id][a]);
            }
          }
        }
        applyClientExpressions();
        renderCharts();
        processImageBindings();
        applyRoutes();
      }

      function applyAttribute(el, attr, value) {
        el.setAttribute(attr, value);
        if (attr === 'value' && (el.tagName === 'INPUT' || el.tagName === 'TEXTAREA')) {
          el.value = value;
        }
      }

      function renderTemplateValue(value) {
        if (value == null) return '';
        if (typeof value === 'object') return JSON.stringify(value);
        return String(value);
      }

      function escapeSvgText(value) {
        return String(value == null ? '' : value)
          .replace(/&/g, '&amp;')
          .replace(/</g, '&lt;')
          .replace(/>/g, '&gt;')
          .replace(/"/g, '&quot;');
      }

      function normalizeChartRows(value) {
        if (value && typeof value === 'object' && Array.isArray(value.data)) {
          value = value.data;
        }
        if (Array.isArray(value)) return value;
        if (value && typeof value === 'object') return Object.values(value);
        return [];
      }

      function renderCharts() {
        document.querySelectorAll('svg[data-jtml-chart]').forEach(function (svg) {
          const type = svg.getAttribute('data-jtml-chart') || 'bar';
          const dataExpr = svg.getAttribute('data-jtml-chart-data') || '';
          const byField = svg.getAttribute('data-jtml-chart-by') || 'label';
          const valueField = svg.getAttribute('data-jtml-chart-value') || 'value';
          const data = evaluateClientExpression(dataExpr);
          if (!data.found) return;
          const rows = normalizeChartRows(data.value);
          const width = Math.max(160, Number(svg.getAttribute('width') || 640) || 640);
          const height = Math.max(120, Number(svg.getAttribute('height') || 320) || 320);
          const axisXLabel = svg.getAttribute('data-jtml-chart-axis-x') || '';
          const axisYLabel = svg.getAttribute('data-jtml-chart-axis-y') || '';
          const showLegend = svg.getAttribute('data-jtml-chart-legend') === 'true';
          const showGrid   = svg.getAttribute('data-jtml-chart-grid') === 'true';
          const isStacked  = svg.getAttribute('data-jtml-chart-stacked') === 'true';
          const padLeft  = axisYLabel ? 60 : 48;
          const padRight = showLegend ? 130 : 20;
          const padTop   = 20;
          const padBottom = axisXLabel ? 56 : 44;
          const pad = { left: padLeft, right: padRight, top: padTop, bottom: padBottom };
          const innerW = Math.max(1, width - pad.left - pad.right);
          const innerH = Math.max(1, height - pad.top - pad.bottom);
          const color = svg.getAttribute('data-jtml-chart-color') || '#0f766e';
          const values = rows.map(function (row) {
            return Math.max(0, Number(row && row[valueField]) || 0);
          });
          const maxValue = Math.max(1, Math.max.apply(null, values.length ? values : [1]));
          // Compute a clean tick interval
          const rawStep = maxValue / 4;
          const magnitude = Math.pow(10, Math.floor(Math.log10(rawStep || 1)));
          const niceStep = Math.ceil(rawStep / magnitude) * magnitude || 1;
          const niceMax = Math.ceil(maxValue / niceStep) * niceStep || niceStep;

          let html = '';
          // Grid lines and Y-axis ticks
          const tickCount = Math.min(6, Math.round(niceMax / niceStep));
          for (let t = 0; t <= tickCount; t++) {
            const tv = t * niceStep;
            const ty = (pad.top + innerH) - (tv / niceMax) * innerH;
            if (showGrid && t > 0) {
              html += '<line x1="' + pad.left + '" y1="' + ty.toFixed(1) + '" x2="' + (width - pad.right) + '" y2="' + ty.toFixed(1) + '" stroke="#e2e8f0" stroke-width="1" stroke-dasharray="3,3"/>';
            }
            const tickLabel = tv >= 1000 ? (tv / 1000).toFixed(1) + 'k' : String(tv);
            html += '<line x1="' + (pad.left - 4) + '" y1="' + ty.toFixed(1) + '" x2="' + pad.left + '" y2="' + ty.toFixed(1) + '" stroke="#94a3b8" stroke-width="1"/>';
            html += '<text x="' + (pad.left - 7) + '" y="' + (ty + 4).toFixed(1) + '" text-anchor="end" fill="#64748b" font-size="11">' + escapeSvgText(tickLabel) + '</text>';
          }
          // Axes
          html += '<line x1="' + pad.left + '" y1="' + (height - pad.bottom) + '" x2="' + (width - pad.right) + '" y2="' + (height - pad.bottom) + '" stroke="#94a3b8" stroke-width="1"/>';
          html += '<line x1="' + pad.left + '" y1="' + pad.top + '" x2="' + pad.left + '" y2="' + (height - pad.bottom) + '" stroke="#94a3b8" stroke-width="1"/>';
          // Y-axis label
          if (axisYLabel) {
            var rotTransform = 'rotate(-90)';
            html += '<text transform="' + rotTransform + '" x="' + (-(height / 2)).toFixed(1) + '" y="14" text-anchor="middle" fill="#475569" font-size="12">' + escapeSvgText(axisYLabel) + '</text>';
          }
          // X-axis label
          if (axisXLabel) {
            html += '<text x="' + ((pad.left + width - pad.right) / 2).toFixed(1) + '" y="' + (height - 6) + '" text-anchor="middle" fill="#475569" font-size="12">' + escapeSvgText(axisXLabel) + '</text>';
          }
          if (!rows.length) {
            html += '<text x="' + (width / 2) + '" y="' + (height / 2) + '" text-anchor="middle" fill="#64748b" font-size="14">No chart data</text>';
          }
          if (type === 'bar') {
            const gap = rows.length > 1 ? Math.max(2, Math.round(innerW / rows.length * 0.15)) : 0;
            const barW = rows.length ? Math.max(2, (innerW - gap * (rows.length - 1)) / rows.length) : innerW;
            rows.forEach(function (row, index) {
              const value = values[index];
              const barH = Math.round((value / niceMax) * innerH);
              const x = pad.left + index * (barW + gap);
              const y = height - pad.bottom - barH;
              const label = row && row[byField] != null ? row[byField] : String(index + 1);
              html += '<rect x="' + x.toFixed(2) + '" y="' + y.toFixed(2) + '" width="' + barW.toFixed(2) + '" height="' + barH.toFixed(2) + '" fill="' + escapeSvgText(color) + '" rx="2"/>';
              const xLabel = x + barW / 2;
              const truncLabel = String(label).length > 8 ? String(label).slice(0, 7) + '…' : String(label);
              html += '<text x="' + xLabel.toFixed(2) + '" y="' + (height - pad.bottom + 14) + '" text-anchor="middle" fill="#334155" font-size="11">' + escapeSvgText(truncLabel) + '</text>';
              if (barH > 14) {
                html += '<text x="' + xLabel.toFixed(2) + '" y="' + Math.max(pad.top + 12, y - 4).toFixed(2) + '" text-anchor="middle" fill="#334155" font-size="11">' + escapeSvgText(value) + '</text>';
              }
            });
          } else if (type === 'line') {
            const stepX = rows.length > 1 ? innerW / (rows.length - 1) : innerW;
            let points = '';
            rows.forEach(function (row, index) {
              const value = values[index];
              const x = (pad.left + (rows.length > 1 ? index * stepX : innerW / 2)).toFixed(2);
              const y = (height - pad.bottom - (value / niceMax) * innerH).toFixed(2);
              if (index === 0) points += 'M' + x + ' ' + y;
              else points += ' L' + x + ' ' + y;
            });
            // Fill area under line
            if (rows.length > 1) {
              const firstX = pad.left.toFixed(2);
              const lastX  = (pad.left + innerW).toFixed(2);
              const baseY  = (height - pad.bottom).toFixed(2);
              html += '<path d="' + points + ' L' + lastX + ' ' + baseY + ' L' + firstX + ' ' + baseY + ' Z" fill="' + escapeSvgText(color) + '" fill-opacity="0.12"/>';
            }
            html += '<path d="' + points + '" fill="none" stroke="' + escapeSvgText(color) + '" stroke-width="2.5" stroke-linejoin="round" stroke-linecap="round"/>';
            rows.forEach(function (row, index) {
              const value = values[index];
              const x = (pad.left + (rows.length > 1 ? index * stepX : innerW / 2)).toFixed(2);
              const y = (height - pad.bottom - (value / niceMax) * innerH).toFixed(2);
              const label = row && row[byField] != null ? row[byField] : String(index + 1);
              html += '<circle cx="' + x + '" cy="' + y + '" r="4" fill="' + escapeSvgText(color) + '" stroke="#fff" stroke-width="1.5"/>';
              if (rows.length <= 16) {
                const truncLabel = String(label).length > 8 ? String(label).slice(0, 7) + '…' : String(label);
                html += '<text x="' + x + '" y="' + (height - pad.bottom + 14) + '" text-anchor="middle" fill="#334155" font-size="11">' + escapeSvgText(truncLabel) + '</text>';
              }
            });
          }
          // Legend
          if (showLegend && rows.length > 0) {
            const lx = width - pad.right + 12;
            let ly = pad.top + 10;
            const field = svg.getAttribute('aria-label') || valueField;
            html += '<rect x="' + lx + '" y="' + ly + '" width="12" height="12" fill="' + escapeSvgText(color) + '" rx="2"/>';
            html += '<text x="' + (lx + 16) + '" y="' + (ly + 10) + '" fill="#334155" font-size="12">' + escapeSvgText(field) + '</text>';
          }
          if (svg.dataset.jtmlChartRendered !== html) {
            svg.innerHTML = html;
            svg.dataset.jtmlChartRendered = html;
          }
        });
      }

      // ── Timeline animations ────────────────────────────────────────
      const __jtml_timelines = {};
      function initTimelines() {
        document.querySelectorAll('template[data-jtml-timeline]').forEach(function (el) {
          const name = el.getAttribute('data-jtml-timeline');
          if (!name || __jtml_timelines[name]) return;
          const duration = Math.max(1, Number(el.getAttribute('data-jtml-timeline-duration')) || 400);
          const easingName = el.getAttribute('data-jtml-timeline-easing') || 'linear';
          const autoplay = el.getAttribute('data-jtml-timeline-autoplay') === 'true';
          const repeat = el.getAttribute('data-jtml-timeline-repeat') === 'true';
          const animateStr = el.getAttribute('data-jtml-timeline-animates') || '';
          const animates = animateStr.split(';').filter(Boolean).map(function (s) {
            const parts = s.split(':');
            return { varName: parts[0], from: Number(parts[1]) || 0, to: Number(parts[2]) || 0 };
          });
          function easeValue(t, easingFn) {
            if (easingFn === 'ease-in')     return t * t;
            if (easingFn === 'ease-out')    return 1 - (1 - t) * (1 - t);
            if (easingFn === 'ease-in-out') return t < 0.5 ? 2 * t * t : 1 - Math.pow(-2 * t + 2, 2) / 2;
            if (easingFn === 'cubic-bezier') return t; // fallback
            return t; // linear
          }
          const tl = {
            name: name, duration: duration, easing: easingName,
            playing: false, paused: false, elapsed: 0, startTime: null, rafId: null, repeat: repeat,
            play: function () {
              if (tl.playing && !tl.paused) return;
              if (tl.paused) { tl.startTime = performance.now() - tl.elapsed; tl.paused = false; }
              else { tl.startTime = performance.now(); tl.elapsed = 0; }
              tl.playing = true;
              __jtml_sendEvent('__timeline_' + name + '_play', []);
              function tick(now) {
                if (!tl.playing || tl.paused) return;
                tl.elapsed = now - tl.startTime;
                const rawT = Math.min(1, tl.elapsed / tl.duration);
                const t = easeValue(rawT, easingName);
                const progress = Math.round(t * 100);
                animates.forEach(function (a) {
                  const val = a.from + (a.to - a.from) * t;
                  __jtml_sendEvent('__timeline_animate_' + a.varName, [val]);
                });
                __jtml_sendEvent('__timeline_' + name + '_progress', [progress]);
                if (rawT < 1) { tl.rafId = requestAnimationFrame(tick); }
                else {
                  tl.playing = false;
                  if (repeat) { tl.elapsed = 0; tl.startTime = now; tl.playing = true; tl.rafId = requestAnimationFrame(tick); }
                  else { __jtml_sendEvent('__timeline_' + name + '_done', []); }
                }
              }
              tl.rafId = requestAnimationFrame(tick);
            },
            pause: function () {
              if (!tl.playing) return;
              tl.paused = true; tl.playing = false;
              if (tl.rafId) cancelAnimationFrame(tl.rafId);
              __jtml_sendEvent('__timeline_' + name + '_pause', []);
            },
            reset: function () {
              tl.playing = false; tl.paused = false; tl.elapsed = 0;
              if (tl.rafId) cancelAnimationFrame(tl.rafId);
              animates.forEach(function (a) {
                __jtml_sendEvent('__timeline_animate_' + a.varName, [a.from]);
              });
              __jtml_sendEvent('__timeline_' + name + '_progress', [0]);
            }
          };
          __jtml_timelines[name] = tl;
          if (autoplay) setTimeout(function () { tl.play(); }, 50);
        });
      }

      function __jtml_sendEvent(action, args) {
        try {
          sendEvent(action, args);
        } catch (e) {}
      }

      // ── Browser image processing ───────────────────────────────────
      function processImageBindings() {
        document.querySelectorAll('template[data-jtml-image-proc]').forEach(function (el) {
          const op     = el.getAttribute('data-jtml-image-proc') || 'resize';
          const srcVar = el.getAttribute('data-jtml-image-src') || '';
          const into   = el.getAttribute('data-jtml-image-into') || '';
          if (!srcVar || !into) return;
          const srcData = evaluateClientExpression(srcVar);
          if (!srcData.found) return;
          const srcValue = srcData.value;
          const srcUrl = typeof srcValue === 'object' && srcValue
            ? (srcValue.preview || srcValue.url || srcValue.src || '')
            : String(srcValue || '');
          if (!srcUrl) return;
          const cacheKey = op + ':' + srcVar + ':' + JSON.stringify({
            w: el.getAttribute('data-jtml-image-w'),
            h: el.getAttribute('data-jtml-image-h'),
            fit: el.getAttribute('data-jtml-image-fit'),
            x: el.getAttribute('data-jtml-image-x'),
            y: el.getAttribute('data-jtml-image-y'),
            filter: el.getAttribute('data-jtml-image-filter'),
            amount: el.getAttribute('data-jtml-image-amount'),
            src: srcUrl
          });
          if (el.dataset.jtmlImgCacheKey === cacheKey) return;
          el.dataset.jtmlImgCacheKey = cacheKey;
          const img = new Image();
          img.crossOrigin = 'anonymous';
          img.onload = function () {
            try {
              const canvas = document.createElement('canvas');
              let sw = img.naturalWidth, sh = img.naturalHeight;
              let dx = 0, dy = 0, dw, dh;
              if (op === 'resize') {
                const tw = Number(el.getAttribute('data-jtml-image-w')) || sw;
                const th = Number(el.getAttribute('data-jtml-image-h')) || sh;
                const fit = el.getAttribute('data-jtml-image-fit') || 'fill';
                const ratio = sw / sh;
                if (fit === 'cover') {
                  if (tw / th > ratio) { dw = tw; dh = Math.round(tw / ratio); }
                  else { dh = th; dw = Math.round(th * ratio); }
                  dx = Math.round((dw - tw) / 2); dy = Math.round((dh - th) / 2);
                  canvas.width = tw; canvas.height = th;
                  canvas.getContext('2d').drawImage(img, -dx, -dy, dw, dh);
                } else if (fit === 'contain') {
                  if (tw / th > ratio) { dh = th; dw = Math.round(th * ratio); }
                  else { dw = tw; dh = Math.round(tw / ratio); }
                  canvas.width = tw; canvas.height = th;
                  canvas.getContext('2d').drawImage(img, Math.round((tw - dw) / 2), Math.round((th - dh) / 2), dw, dh);
                } else {
                  canvas.width = tw; canvas.height = th;
                  canvas.getContext('2d').drawImage(img, 0, 0, tw, th);
                }
              } else if (op === 'crop') {
                const cx = Number(el.getAttribute('data-jtml-image-x')) || 0;
                const cy = Number(el.getAttribute('data-jtml-image-y')) || 0;
                const cw = Number(el.getAttribute('data-jtml-image-w')) || sw;
                const ch = Number(el.getAttribute('data-jtml-image-h')) || sh;
                canvas.width = cw; canvas.height = ch;
                canvas.getContext('2d').drawImage(img, cx, cy, cw, ch, 0, 0, cw, ch);
              } else if (op === 'filter') {
                const filterType = el.getAttribute('data-jtml-image-filter') || '';
                const amount = Number(el.getAttribute('data-jtml-image-amount') || '1');
                canvas.width = sw; canvas.height = sh;
                const ctx = canvas.getContext('2d');
                if (filterType === 'grayscale') {
                  ctx.filter = 'grayscale(' + (amount * 100).toFixed(0) + '%)';
                } else if (filterType === 'blur') {
                  ctx.filter = 'blur(' + amount.toFixed(1) + 'px)';
                } else if (filterType === 'brightness') {
                  ctx.filter = 'brightness(' + amount.toFixed(2) + ')';
                } else if (filterType === 'contrast') {
                  ctx.filter = 'contrast(' + amount.toFixed(2) + ')';
                } else if (filterType === 'sepia') {
                  ctx.filter = 'sepia(' + (amount * 100).toFixed(0) + '%)';
                } else if (filterType === 'invert') {
                  ctx.filter = 'invert(' + (amount * 100).toFixed(0) + '%)';
                } else if (filterType === 'saturate') {
                  ctx.filter = 'saturate(' + amount.toFixed(2) + ')';
                }
                ctx.drawImage(img, 0, 0, sw, sh);
              }
              const preview = canvas.toDataURL('image/png');
              const result = JSON.stringify({ preview: preview, loading: false, error: '', width: canvas.width, height: canvas.height });
              if (typeof applyBindings === 'function') {
                applyBindings({ bindings: { [into]: JSON.parse(result) } });
              }
            } catch (err) {
              const result = JSON.stringify({ preview: '', loading: false, error: String(err), width: 0, height: 0 });
              if (typeof applyBindings === 'function') {
                applyBindings({ bindings: { [into]: JSON.parse(result) } });
              }
            }
          };
          img.onerror = function () {
            if (typeof applyBindings === 'function') {
              applyBindings({ bindings: { [into]: { preview: '', loading: false, error: 'Failed to load image', width: 0, height: 0 } } });
            }
          };
          img.src = srcUrl;
        });
      }

      function lookupTemplatePath(scope, path) {
        const parts = path.split('.').filter(Boolean);
        let value = scope;
        for (const part of parts) {
          if (value == null) return '';
          value = value[part];
        }
        return renderTemplateValue(value);
      }

      function escapeSelectorValue(value) {
        if (window.CSS && typeof window.CSS.escape === 'function') return window.CSS.escape(value);
        return String(value).replace(/["\\]/g, '\\$&');
      }

      function renderLoopBody(template, iterator, item) {
        const scope = {};
        scope[iterator] = item;
        return template.replace(/\{\{\s*\(?\s*([A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z_][A-Za-z0-9_]*)*)\s*\)?\s*\}\}/g,
          function (_, expr) { return lookupTemplatePath(scope, expr); });
      }

      function applyTemplates(b) {
        if (b.conditions) {
          for (const key in b.conditions) {
            const escapedKey = escapeSelectorValue(key);
            document.querySelectorAll('[data-jtml-if="' + escapedKey + '"], [data-jtml-while="' + escapedKey + '"]').forEach(function (el) {
              const source = b.conditions[key] ? el.getAttribute('data-then') || el.getAttribute('data-body') || '' : el.getAttribute('data-else') || '';
              if (el.dataset.jtmlRendered !== source) {
                el.innerHTML = source;
                el.dataset.jtmlRendered = source;
              }
            });
          }
        }
        if (b.loops) {
          for (const id in b.loops) {
            const el = document.getElementById(id);
            if (!el) continue;
            const iterator = el.getAttribute('data-jtml-iterator') || 'item';
            const body = el.getAttribute('data-body') || '';
            let values = b.loops[id];
            if (values == null) values = [];
            if (!Array.isArray(values)) {
              if (typeof values === 'string') values = values.split('');
              else if (typeof values === 'object') values = Object.values(values);
              else values = [values];
            }
            const html = values.map(function (item) { return renderLoopBody(body, iterator, item); }).join('');
            if (el.dataset.jtmlRendered !== html) {
              el.innerHTML = html;
              el.dataset.jtmlRendered = html;
            }
          }
        }
      }

      function startRedirectBindings() {
        document.querySelectorAll('[data-jtml-redirect-action]').forEach(function (meta) {
          const actionName = meta.getAttribute('data-jtml-redirect-action');
          const path = meta.getAttribute('data-jtml-redirect-to') || '/';
          if (actionName) {
            __jtml_refresh_fns[actionName] = function () {
              window.__jtml_redirect(path);
            };
          }
        });
      }

      function startInvalidationBindings() {
        document.querySelectorAll('[data-jtml-invalidate-action]').forEach(function (meta) {
          const actionName = meta.getAttribute('data-jtml-invalidate-action');
          const fetches = String(meta.getAttribute('data-jtml-invalidate-fetches') || '')
            .split(',')
            .map(function (name) { return name.trim(); })
            .filter(Boolean);
          if (actionName && fetches.length) __jtml_invalidate_fns[actionName] = fetches;
        });
      }

      async function runInvalidations(actionName) {
        const fetches = __jtml_invalidate_fns[actionName] || [];
        for (const name of fetches) {
          if (__jtml_fetch_fns[name]) await __jtml_fetch_fns[name]();
        }
      }

      function runRouteLoads(route) {
        const fetches = String(route.getAttribute('data-jtml-route-load') || '')
          .split(',')
          .map(function (name) { return name.trim(); })
          .filter(Boolean);
        fetches.forEach(function (name) {
          if (__jtml_fetch_fns[name]) __jtml_fetch_fns[name]();
        });
      }

      function getWindowPath(path) {
        return String(path || '').split('.').filter(Boolean).reduce(function (value, part) {
          return value == null ? undefined : value[part];
        }, window);
      }

      function parseActionCall(callSource) {
        const raw = String(callSource || '').trim();
        const match = raw.match(/^([A-Za-z_][A-Za-z0-9_.]*)(?:\((.*)\))?$/);
        if (!match) return { name: raw.replace(/\(.*$/, ''), args: [] };
        const args = [];
        const inner = (match[2] || '').trim();
        if (inner) {
          splitTopLevelList(inner).forEach(function (part) {
            const value = evaluateClientBodyExpression(part);
            args.push(value.found ? value.value : part.replace(/^['"]|['"]$/g, ''));
          });
        }
        return { name: match[1], args: args };
      }

      function startExternBindings() {
        document.querySelectorAll('[data-jtml-extern-action]').forEach(function (meta) {
          const action = meta.getAttribute('data-jtml-extern-action');
          const target = meta.getAttribute('data-window') || action;
          if (!action) return;
          __jtml_extern_fns[action] = function (args) {
            const fn = getWindowPath(target);
            if (typeof fn !== 'function') {
              reportStatus('error', 'External host function not found: ' + target);
              return;
            }
            return fn.apply(window, args || []);
          };
        });
      }

      // Intercept clicks on Friendly route links (`link "Label" to "/path"` →
      // an inert anchor carrying `data-jtml-href="#/path"`.
      // Older output used plain hash links, which could resolve against the
      // parent Studio URL inside `srcdoc` previews if the default navigation
      // ran before the router. Setting `location.hash` here mutates the
      // iframe's own Location and keeps the preview on the rendered JTML app.
      function startLinkBindings() {
        document.addEventListener('click', function (event) {
          if (event.defaultPrevented) return;
          if (event.button !== 0) return;
          if (event.metaKey || event.ctrlKey || event.shiftKey || event.altKey) return;
          const anchor = event.target && event.target.closest && event.target.closest('a[data-jtml-link]');
          if (!anchor) return;
          if (anchor.target && anchor.target !== '' && anchor.target !== '_self') return;
          const href = anchor.getAttribute('data-jtml-href') || anchor.getAttribute('href') || '';
          if (!href || href[0] !== '#') return;
          event.preventDefault();
          const next = href.length > 1 ? href : '#/';
          if (location.hash === next) {
            // Same-hash click still has to re-run route bindings so users
            // can re-trigger active-link feedback or load handlers.
            applyRoutes();
          } else {
            location.hash = next;
          }
        });
      }

      function startDropzoneBindings() {
        document.querySelectorAll('input[type="file"][data-jtml-dropzone]').forEach(function (input) {
          if (input.dataset.jtmlDropzoneReady === 'true') return;
          input.dataset.jtmlDropzoneReady = 'true';
          input.addEventListener('dragover', function (event) {
            event.preventDefault();
            input.dataset.jtmlDrag = 'over';
          });
          input.addEventListener('dragleave', function () {
            delete input.dataset.jtmlDrag;
          });
          input.addEventListener('drop', function (event) {
            event.preventDefault();
            delete input.dataset.jtmlDrag;
            const files = event.dataTransfer && event.dataTransfer.files;
            if (!files || !files.length) return;
            try {
              input.files = files;
            } catch (_) {}
            input.dispatchEvent(new Event('change', { bubbles: true }));
          });
        });
      }

      function mediaStateFor(el) {
        return {
          currentTime: Number.isFinite(el.currentTime) ? el.currentTime : 0,
          duration: Number.isFinite(el.duration) ? el.duration : 0,
          paused: !!el.paused,
          ended: !!el.ended,
          muted: !!el.muted,
          volume: Number.isFinite(el.volume) ? el.volume : 1,
          playbackRate: Number.isFinite(el.playbackRate) ? el.playbackRate : 1,
          readyState: Number(el.readyState || 0),
          src: el.currentSrc || el.getAttribute('src') || ''
        };
      }

      function startMediaControllerBindings() {
        document.querySelectorAll('video[data-jtml-media-controller], audio[data-jtml-media-controller]').forEach(function (el) {
          const name = el.getAttribute('data-jtml-media-controller');
          if (!name) return;
          const update = function () {
            clientState[name] = mediaStateFor(el);
            applyClientState();
          };
          if (el.dataset.jtmlMediaReady !== 'true') {
            el.dataset.jtmlMediaReady = 'true';
            ['loadedmetadata', 'durationchange', 'timeupdate', 'play', 'pause', 'ended', 'volumechange', 'ratechange', 'emptied'].forEach(function (eventName) {
              el.addEventListener(eventName, update);
            });
          }
          __jtml_media_actions[name + '.play'] = function () {
            const result = el.play && el.play();
            if (result && typeof result.catch === 'function') {
              result.catch(function (err) { reportStatus('error', err && err.message ? err.message : 'media play failed'); });
            }
            update();
          };
          __jtml_media_actions[name + '.pause'] = function () {
            if (el.pause) el.pause();
            update();
          };
          __jtml_media_actions[name + '.toggle'] = function () {
            if (el.paused) __jtml_media_actions[name + '.play']();
            else __jtml_media_actions[name + '.pause']();
          };
          __jtml_media_actions[name + '.seek'] = function (args) {
            const value = Number(args && args.length ? args[0] : 0);
            if (Number.isFinite(value)) el.currentTime = value;
            update();
          };
          __jtml_media_actions[name + '.setVolume'] = function (args) {
            const value = Number(args && args.length ? args[0] : el.volume);
            if (Number.isFinite(value)) el.volume = Math.max(0, Math.min(1, value));
            update();
          };
          update();
        });
      }

      function drawScene3DFallback(canvas, spec) {
        const ctx = canvas.getContext && canvas.getContext('2d');
        if (!ctx) return;
        const width = Math.max(240, Number(canvas.getAttribute('width') || canvas.clientWidth || 640) || 640);
        const height = Math.max(160, Number(canvas.getAttribute('height') || canvas.clientHeight || 360) || 360);
        canvas.width = width;
        canvas.height = height;
        ctx.clearRect(0, 0, width, height);
        const gradient = ctx.createLinearGradient(0, 0, width, height);
        gradient.addColorStop(0, '#0f172a');
        gradient.addColorStop(1, '#134e4a');
        ctx.fillStyle = gradient;
        ctx.fillRect(0, 0, width, height);
        const size = Math.min(width, height) * 0.28;
        const cx = width / 2;
        const cy = height / 2;
        const dx = size * 0.42;
        const dy = -size * 0.32;
        const front = [
          [cx - size / 2, cy - size / 2],
          [cx + size / 2, cy - size / 2],
          [cx + size / 2, cy + size / 2],
          [cx - size / 2, cy + size / 2]
        ];
        const back = front.map(function (p) { return [p[0] + dx, p[1] + dy]; });
        ctx.strokeStyle = '#7dd3fc';
        ctx.lineWidth = 2;
        const line = function (a, b) {
          ctx.beginPath();
          ctx.moveTo(a[0], a[1]);
          ctx.lineTo(b[0], b[1]);
          ctx.stroke();
        };
        for (let i = 0; i < 4; i += 1) {
          line(front[i], front[(i + 1) % 4]);
          line(back[i], back[(i + 1) % 4]);
          line(front[i], back[i]);
        }
        ctx.fillStyle = 'rgba(255,255,255,.92)';
        ctx.font = '600 14px system-ui, sans-serif';
        ctx.textAlign = 'center';
        ctx.fillText(spec.scene ? '3D scene: ' + spec.scene : 'JTML 3D scene mount', cx, height - 42);
        ctx.font = '12px system-ui, sans-serif';
        ctx.fillStyle = 'rgba(226,232,240,.82)';
        ctx.fillText('Attach window.jtml3d.render(canvas, spec) for Three.js/WebGPU rendering', cx, height - 22);
      }

      function scene3DStateFor(canvas, spec, hostRendered, status, extra) {
        return Object.assign({
          scene: spec.scene || '',
          camera: spec.camera || 'orbit',
          controls: spec.controls || 'orbit',
          renderer: spec.renderer || 'auto',
          status: status || 'ready',
          hostRendered: !!hostRendered,
          width: Number(canvas.width || canvas.getAttribute('width') || canvas.clientWidth || 0),
          height: Number(canvas.height || canvas.getAttribute('height') || canvas.clientHeight || 0)
        }, extra && typeof extra === 'object' ? extra : {});
      }

      function startScene3DBindings() {
        document.querySelectorAll('canvas[data-jtml-scene3d]').forEach(function (canvas) {
          const controllerName = canvas.getAttribute('data-jtml-scene3d-controller') || '';
          const spec = {
            scene: canvas.getAttribute('data-jtml-scene') || '',
            camera: canvas.getAttribute('data-jtml-camera') || 'orbit',
            controls: canvas.getAttribute('data-jtml-controls') || 'orbit',
            renderer: canvas.getAttribute('data-jtml-renderer') || 'auto',
            controller: controllerName,
            state: clientState
          };
          const publish = function (extra, status, hostRendered) {
            if (!controllerName) return;
            clientState[controllerName] = scene3DStateFor(canvas, spec, hostRendered, status, extra);
            applyClientState();
          };
          spec.update = function (next) {
            publish(next, 'host-updated', true);
          };
          window.jtml = Object.assign(window.jtml || {}, {
            getScene3DSpec: function (el) {
              el = el || canvas;
              return {
                scene: el.getAttribute('data-jtml-scene') || '',
                camera: el.getAttribute('data-jtml-camera') || 'orbit',
                controls: el.getAttribute('data-jtml-controls') || 'orbit',
                renderer: el.getAttribute('data-jtml-renderer') || 'auto',
                controller: el.getAttribute('data-jtml-scene3d-controller') || '',
                state: clientState
              };
            }
          });
          const host = window.jtml3d && typeof window.jtml3d.render === 'function'
            ? window.jtml3d
            : null;
          if (host) {
            try {
              const hostResult = host.render(canvas, spec);
              canvas.dataset.jtmlScene3dStatus = 'host-rendered';
              publish(hostResult, 'host-rendered', true);
            } catch (err) {
              canvas.dataset.jtmlScene3dStatus = 'fallback';
              drawScene3DFallback(canvas, spec);
              publish({ error: err && err.message ? err.message : '3D renderer failed' }, 'fallback', false);
              reportStatus('error', err && err.message ? err.message : '3D renderer failed');
            }
          } else {
            canvas.dataset.jtmlScene3dStatus = 'fallback';
            drawScene3DFallback(canvas, spec);
            publish({}, 'fallback', false);
          }
          document.dispatchEvent(new CustomEvent('jtml:scene3d-ready', {
            detail: { canvas: canvas, spec: spec, controller: controllerName, hostRendered: !!host }
          }));
        });
      }

      function applyInitial() {
        if (window.__jtml_bindings) applyBindings(window.__jtml_bindings);
        scanComponentInstances();
        startFetchBindings();
        startInvalidationBindings();
        startExternBindings();
        startRedirectBindings();
        startGuardBindings();
        startLinkBindings();
        startDropzoneBindings();
        startMediaControllerBindings();
        startScene3DBindings();
        initTimelines();
        processImageBindings();
        applyRoutes();
      }
      if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', applyInitial);
      } else {
        applyInitial();
      }

      let ws = null;
      try {
        const host = location.hostname || 'localhost';
        ws = new WebSocket('ws://' + host + ':' + wsPort);
        ws.onmessage = function (event) {
          const m = JSON.parse(event.data);
          reportStatus('connected', 'Runtime connected');
          if (m.type === 'populateBindings' && m.bindings) applyBindings(m.bindings);
          else if (m.type === 'updateBinding') {
            const el = document.getElementById(m.elementId);
            if (el) el.textContent = m.value;
          } else if (m.type === 'updateAttribute') {
            const el = document.getElementById(m.elementId);
            if (el) applyAttribute(el, m.attribute, m.value);
          } else if (m.type === 'reload') {
            // Structural change (source file edited in watch mode).
            // A full reload is safer than patching bindings, since the
            // element tree itself may have changed.
            location.reload();
          }
        };
        ws.onopen = function () { reportStatus('connected', 'Runtime connected'); };
        ws.onclose = function () { reportStatus('offline', 'Runtime disconnected; HTTP fallback available'); };
        ws.onerror = function () { reportStatus('fallback', 'WebSocket unavailable; using HTTP fallback'); };
      } catch (_) {
        reportStatus('fallback', 'WebSocket unavailable; using HTTP fallback');
      }

      window.sendEvent = async function (elementId, eventType, args) {
        args = args || [];
        // Client-side intercept for refresh actions and redirect.
        const action = parseActionCall(args[0] || '');
        const fnName = action.name;
        if (fnName && __jtml_refresh_fns[fnName]) {
          await __jtml_refresh_fns[fnName]();
          return;
        }
        if (fnName && __jtml_extern_fns[fnName]) {
          await __jtml_extern_fns[fnName](action.args.concat(args.slice(1)));
          return;
        }
        if (fnName && __jtml_media_actions[fnName]) {
          await __jtml_media_actions[fnName](action.args.concat(args.slice(1)));
          return;
        }
        try {
          const res = await fetch('/api/event', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ elementId: elementId, eventType: eventType, args: args })
          });
          const data = await res.json();
          if (data && data.bindings) applyBindings(data.bindings);
          if (data && data.error) reportStatus('error', data.error);
          if (fnName) await runInvalidations(fnName);
        } catch (err) {
          reportStatus('error', err && err.message ? err.message : 'event dispatch failed');
          console.error('[jtml] event dispatch failed:', err);
        }
      };

      function normalizeRoute(path) {
        if (!path || path === '#') return '/';
        if (path[0] === '#') path = path.slice(1);
        if (!path) return '/';
        return path[0] === '/' ? path : '/' + path;
      }

      function matchRouteParams(pattern, path) {
        if (String(pattern || '').trim() === '*') return {};
        pattern = normalizeRoute(pattern);
        path = normalizeRoute(path);
        const pp = pattern.split('/').filter(Boolean);
        const sp = path.split('/').filter(Boolean);
        if (pp.length !== sp.length) return null;
        const params = {};
        for (let i = 0; i < pp.length; i += 1) {
          if (pp[i][0] === ':') {
            params[pp[i].slice(1)] = decodeURIComponent(sp[i]);
            continue;
          }
          if (pp[i] !== sp[i]) return null;
        }
        return params;
      }

      function applyActiveLinkClasses(path) {
        document.querySelectorAll('[data-jtml-active-class]').forEach(function (el) {
          const cls = el.getAttribute('data-jtml-active-class');
          const href = el.getAttribute('data-jtml-href') || el.getAttribute('href') || '';
          const linkPath = normalizeRoute(href[0] === '#' ? href.slice(1) : href);
          if (cls) {
            if (linkPath === path) el.classList.add(cls);
            else el.classList.remove(cls);
          }
        });
      }

      const __jtml_guards = [];

      function startGuardBindings() {
        document.querySelectorAll('[data-jtml-route-guard]').forEach(function (meta) {
          const routePath = normalizeRoute(meta.getAttribute('data-jtml-route-guard') || '');
          const guardVar = meta.getAttribute('data-jtml-guard-var') || '';
          const redirectTo = meta.getAttribute('data-jtml-guard-redirect') || '';
          if (routePath && guardVar) {
            __jtml_guards.push({ path: routePath, var: guardVar, redirect: redirectTo });
          }
        });
      }

      function checkGuards(path) {
        for (var i = 0; i < __jtml_guards.length; i++) {
          var g = __jtml_guards[i];
          if (matchRouteParams(g.path, path) !== null && !clientState[g.var]) {
            if (g.redirect) window.__jtml_redirect(g.redirect);
            return false;
          }
        }
        return true;
      }

      function applyRoutes() {
        const routes = document.querySelectorAll('[data-jtml-route]');
        if (!routes.length) return;
        const path = normalizeRoute(location.hash || '/');
        clientState['activeRoute'] = path;
        applyActiveLinkClasses(path);
        if (!checkGuards(path)) return;
        let matched = false;
        routes.forEach(function (route) {
          const params = !matched ? matchRouteParams(route.getAttribute('data-jtml-route'), path) : null;
          const isMatch = !!params;
          route.hidden = !isMatch;
          route.setAttribute('aria-hidden', isMatch ? 'false' : 'true');
          if (isMatch) {
            const declared = (route.getAttribute('data-jtml-route-params') || '').split(',').filter(Boolean);
            declared.forEach(function (name) {
              clientState[name] = Object.prototype.hasOwnProperty.call(params, name) ? params[name] : '';
            });
            matched = true;
            runRouteLoads(route);
            applyClientState();
          }
        });
        if (!matched) {
          routes.forEach(function (route, index) {
            route.hidden = index !== 0;
            route.setAttribute('aria-hidden', index === 0 ? 'false' : 'true');
          });
        }
      }

      window.addEventListener('hashchange', applyRoutes);
    })();
  </script>
)";
    return out.str();
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
