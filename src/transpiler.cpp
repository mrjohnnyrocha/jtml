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
           attrName == "onSubmit";
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
                args << ", event.target.value"; // Pass the input value as an argument
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
    //   2. WebSocket — full reactivity. Used when the page is
    //      loaded directly from the running interpreter (e.g. `jtml serve`).
    //   3. HTTP fallback — `fetch('/api/event', ...)`. Used when the page is
    //      embedded in an iframe served by `jtml tutorial` and the WebSocket
    //      can't be reached (for example through an IDE's browser-preview
    //      proxy which forwards HTTP but not ws://.
    //
    // Events (`sendEvent`) try the WebSocket first and transparently fall
    // back to HTTP. Both paths end by applying a fresh bindings snapshot.
    std::ostringstream out;
    out << R"(
  <script>
    (function () {
      const wsPort = )" << webSocketPort << R"(;

      function reportStatus(state, message) {
        document.documentElement.dataset.jtmlStatus = state;
        if (message) document.documentElement.dataset.jtmlMessage = message;
        if (state === 'error') console.error('[jtml] ' + (message || 'runtime error'));
      }

      const clientState = {};

      function normalizeClientExpr(expr) {
        expr = String(expr || '').trim();
        while (expr.length > 1 && expr[0] === '(' && expr[expr.length - 1] === ')') {
          expr = expr.slice(1, -1).trim();
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
        if (expr === 'true') return { found: true, value: true };
        if (expr === 'false') return { found: true, value: false };
        if ((expr[0] === '"' && expr[expr.length - 1] === '"') ||
            (expr[0] === "'" && expr[expr.length - 1] === "'")) {
          return { found: true, value: expr.slice(1, -1) };
        }
        if (/^-?\d+(?:\.\d+)?$/.test(expr)) {
          return { found: true, value: Number(expr) };
        }
        return deepGet(clientState, expr);
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
        document.querySelectorAll('[data-jtml-expr]').forEach(function (el) {
          const result = evaluateClientExpression(el.getAttribute('data-jtml-expr'));
          if (result.found) el.textContent = renderTemplateValue(result.value);
        });

        document.querySelectorAll('[data-jtml-cond-expr]').forEach(function (el) {
          const result = evaluateClientExpression(el.getAttribute('data-jtml-cond-expr'));
          if (!result.found) return;
          const source = result.value ? el.getAttribute('data-then') || el.getAttribute('data-body') || '' : el.getAttribute('data-else') || '';
          if (el.dataset.jtmlRendered !== source) {
            el.innerHTML = source;
            el.dataset.jtmlRendered = source;
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
          }
        });
      }

      const __jtml_refresh_fns = {};

      async function executeFetch(name, url, method, bodyExpr) {
        clientState[name] = { loading: true, data: [], error: '' };
        applyClientState();
        try {
          const options = { method: method };
          if (bodyExpr) {
            const body = evaluateClientBodyExpression(bodyExpr);
            options.headers = Object.assign({ 'content-type': 'application/json' }, options.headers || {});
            options.body = JSON.stringify(body.found ? body.value : bodyExpr);
          }
          const response = await fetch(url, options);
          const type = response.headers.get('content-type') || '';
          const payload = type.indexOf('application/json') !== -1 ? await response.json() : await response.text();
          if (!response.ok) throw new Error(response.status + ' ' + response.statusText);
          clientState[name] = { loading: false, data: payload, error: '' };
        } catch (err) {
          clientState[name] = {
            loading: false,
            data: [],
            error: err && err.message ? err.message : String(err)
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
          if (!name || !url) return;
          if (refreshAction) {
            __jtml_refresh_fns[refreshAction] = function () {
              return executeFetch(name, url, method, bodyExpr);
            };
          }
          executeFetch(name, url, method, bodyExpr);
        });
      }

      window.__jtml_redirect = function (path) {
        if (!path) return;
        const hash = path[0] === '#' ? path : '#' + (path[0] === '/' ? path : '/' + path);
        location.hash = hash;
      };

      function applyBindings(b) {
        if (!b) return;
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

      function applyInitial() {
        if (window.__jtml_bindings) applyBindings(window.__jtml_bindings);
        applyRoutes();
        startFetchBindings();
        startRedirectBindings();
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
        const fnName = args[0] ? String(args[0]).replace(/\(.*$/, '') : '';
        if (fnName && __jtml_refresh_fns[fnName]) {
          await __jtml_refresh_fns[fnName]();
          return;
        }
        if (ws && ws.readyState === WebSocket.OPEN) {
          try {
            ws.send(JSON.stringify({ type: 'event', elementId: elementId, eventType: eventType, args: args }));
            return;
          } catch (_) { /* fall through */ }
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

      function applyRoutes() {
        const routes = document.querySelectorAll('[data-jtml-route]');
        if (!routes.length) return;
        const path = normalizeRoute(location.hash || '/');
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
