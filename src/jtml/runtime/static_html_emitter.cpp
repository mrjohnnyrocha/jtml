#include "jtml/runtime/static_html_emitter.h"
#include "jtml/runtime/static_update_plan_emitter.h"
#include "jtml/expression_ir.h"
#include "jtml/friendly.h"
#include "jtml/attribute_classifier.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace jtml {
namespace {
using Scope = std::map<std::string, nlohmann::json>;
using Identity = std::pair<SemanticModuleId, std::string>;

std::string escape(const std::string& value) {
    std::string out;
    for (char ch : value) {
        switch (ch) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += ch;
        }
    }
    return out;
}
std::string text(const nlohmann::json& value) {
    if (value.is_string()) return value.get<std::string>();
    if (value.is_number()) return std::to_string(value.get<int64_t>());
    return value.dump();
}

class StaticHtml {
    const SemanticProject& project;
    const RuntimeProjectPlan& plan;
    std::map<Identity, const RuntimePlanComponentDefinition*> definitions;
    std::set<Identity> active;
    size_t expansionCount = 0;
    std::string context;
    SemanticModuleId current = InvalidSemanticModuleId;
    int line = 0, column = 0;

    [[noreturn]] void reject(const std::string& reason) const {
        std::string file = "<unknown>";
        for (const auto& m : project.modules) if (m.id == current) file = m.path;
        throw std::runtime_error(file + ":" + std::to_string(line) + ":" + std::to_string(column) +
            ": static HTML" + (context.empty() ? "" : " component " + context) + ": " + reason +
            "; use `build --target browser` for runtime behavior or replace this construct with literal markup");
    }
    void location(const ASTNode& node) {
        line = node.sourceSpan.startLine; column = node.sourceSpan.startColumn;
        if (node.sourceSpan.original) {
            const auto p = node.sourceSpan.original->source->coordinates(node.sourceSpan.original->begin);
            line = static_cast<int>(p.first); column = static_cast<int>(p.second);
        }
    }
    nlohmann::json value(const ExpressionIr& ir, const Scope& scope) {
        if (ir.kind == ExpressionIrKind::Literal && (ir.literal.is_string() || ir.literal.is_boolean())) return ir.literal;
        if (ir.kind == ExpressionIrKind::Literal && ir.literal.is_number()) {
            const double number = ir.literal.get<double>();
            if (std::isfinite(number) && std::trunc(number) == number && std::abs(number) <= 9007199254740991.0)
                return static_cast<int64_t>(number);
            reject("only exactly representable safe integer numeric literals are supported");
        }
        if (ir.kind == ExpressionIrKind::Unary && ir.op == "-" && ir.children.size() == 1 &&
            ir.children[0]->kind == ExpressionIrKind::Literal && ir.children[0]->literal.is_number())
            return -value(*ir.children[0], scope).get<int64_t>();
        if (ir.kind == ExpressionIrKind::Path && ir.segments.size() == 1) {
            const auto found = scope.find(ir.root);
            if (found != scope.end()) return found->second;
        }
        reject("expression is not an immutable literal or a bound parameter: " + ir.source);
    }
    nlohmann::json expression(const std::string& source, const Scope& scope) {
        const auto ir = parseExpressionIr(source);
        if (!ir) reject("unsupported literal expression syntax: " + source);
        return value(*ir, scope);
    }
    std::string attribute(const std::string& name, const nlohmann::json& v) {
        // Deliberately closed: no event attributes, compiler hooks, resource loaders,
        // raw style integration, or unknown host attributes in this first target.
        static const std::set<std::string> names = {
            "id", "class", "role", "href", "src", "alt", "title", "rel", "target",
            "tabindex", "lang", "dir", "width", "height", "colspan", "rowspan", "scope",
            "type", "name", "value", "disabled", "hidden", "download"
        };
        const bool aria = name.rfind("aria-", 0) == 0;
        if ((!names.count(name) && !aria) ||
            !std::all_of(name.begin(), name.end(), [](unsigned char ch) {
                return (ch >= 'a' && ch <= 'z') || ch == '-' || (ch >= '0' && ch <= '9');
            })) reject("unsupported attribute " + name);
        const auto s = text(v);
        if (name == "href" || name == "src") {
            const auto colon = s.find(':');
            const auto slash = s.find_first_of("/#?");
            if (colon != std::string::npos && (slash == std::string::npos || colon < slash)) {
                const auto scheme = s.substr(0, colon);
                if (scheme != "https" && scheme != "http" && scheme != "mailto" && scheme != "tel")
                    reject("unsupported URL scheme in " + name);
            }
            if (std::any_of(s.begin(), s.end(), [](unsigned char ch) { return ch <= 32; }))
                reject("URL contains whitespace/control characters");
        }
        if (name == "disabled" || name == "hidden" || name == "download") {
            if (v.is_boolean()) return v.get<bool>() ? " " + name : "";
        }
        return " " + name + "=\"" + escape(s) + "\"";
    }
    std::string tag(const std::string& name) {
        static const std::map<std::string, std::string> aliases = {
            {"box", "div"}, {"text", "p"}, {"image", "img"}, {"link", "a"}
        };
        static const std::set<std::string> tags = {
            "a", "article", "aside", "b", "blockquote", "br", "button", "code", "dd", "div", "dl", "dt",
            "em", "figcaption", "figure", "footer", "h1", "h2", "h3", "h4", "h5", "h6", "header", "hr",
            "img", "label", "li", "main", "nav", "ol", "p", "pre", "section", "small", "span", "strong",
            "table", "tbody", "td", "th", "thead", "tr", "ul"
        };
        auto a = aliases.find(name);
        const auto result = a == aliases.end() ? name : a->second;
        if (!tags.count(result)) reject("unsupported element " + name);
        return result;
    }
    std::string element(const std::string& name, const std::string& attrs,
                        const std::string& content) {
        if (name == "img" || name == "hr" || name == "br") {
            if (!content.empty()) reject("void element cannot contain content");
            return "<" + name + attrs + ">";
        }
        return "<" + name + attrs + ">" + content + "</" + name + ">";
    }
    std::string component(const Identity& id, const Scope& args) {
        const auto found = definitions.find(id);
        if (found == definitions.end()) reject("unresolved module-qualified component " + id.second);
        if (!active.insert(id).second) reject("cyclic component composition " + id.second);
        if (++expansionCount > 10000 || active.size() > 128) reject("static composition exceeds compiler expansion limit");
        const auto& d = *found->second;
        current = id.first; context = id.second; line = d.sourceLine; column = 1;
        for (const auto& node : d.bodyPlan) {
            if (node.kind != "template") {
                line = node.sourceLine; column = node.sourceColumn;
                reject("unsupported " + node.kind + " statement " + node.head);
            }
        }
        if (!d.localState.empty() || !d.localDerived.empty() || !d.localActions.empty() ||
            !d.localEffects.empty() || !d.emits.empty() ||
            !d.bindingParams.empty() || d.hasSlot) reject("component declares dynamic behavior or slots");
        if (std::set<std::string>(d.params.begin(), d.params.end()).size() != d.params.size())
            reject("duplicate component parameter");
        for (const auto& p : d.params) if (!args.count(p)) reject("missing literal parameter " + p);
        std::string out;
        for (size_t i = 0; i < d.bodyPlan.size(); ++i)
            if (d.bodyPlan[i].parentIndex == -1) out += body(d, i, args);
        active.erase(id);
        return out;
    }
    std::string body(const RuntimePlanComponentDefinition& d, size_t index, const Scope& scope) {
        current = d.moduleId; context = d.name;
        const auto& node = d.bodyPlan.at(index);
        line = node.sourceLine; column = node.sourceColumn;
        if (node.kind != "template") reject("unsupported " + node.kind + " statement " + node.head);
        const auto words = splitFriendlyWords(node.text);
        if (words.empty()) reject("empty body node");
        if (node.definitionModule != InvalidSemanticModuleId) {
            const Identity id{node.definitionModule, node.head};
            const auto found = definitions.find(id);
            if (found == definitions.end()) reject("unresolved component " + node.head);
            const auto& params = found->second->params;
            if (words.size() != params.size() + 1 || !node.childIndices.empty())
                reject("component arguments/slots require literal positional composition");
            Scope args;
            for (size_t i = 0; i < params.size(); ++i) args.emplace(params[i], expression(words[i + 1], scope));
            // Retain the browser's inert component boundary div; no runtime metadata.
            return "<div>" + component(id, args) + "</div>";
        }
        const auto name = tag(node.head);
        ExpressionTypeEnvironment env;
        for (const auto& p : scope) env.symbols[p.first] = ExpressionValueType::String;
        const auto parts = staticLiteralElementParts(words, env);
        if (!parts["modifiers"].empty()) reject("semantic presentation modifiers are outside the literal target");
        std::string attrs, content;
        std::string classes = parts.at("implicitClass").get<std::string>();
        std::set<std::string> seen;
        for (const auto& a : parts["attrs"]) {
            const auto key = a["name"].get<std::string>();
            if (!seen.insert(key).second) reject("duplicate attribute " + key);
            if (!a.contains("raw") && !a.value("boolean", false)) reject("unsupported dynamic attribute " + key);
            const auto literal = a.value("boolean", false) ? nlohmann::json(true)
                : expression(a.at("raw").get<std::string>(), scope);
            if (key == "class") classes += (classes.empty() ? "" : " ") + text(literal);
            else attrs += attribute(key, literal);
        }
        if (!classes.empty()) attrs += attribute("class", classes);
        if (node.head == "button" && !seen.count("type")) attrs += attribute("type", "button");
        const auto source = parts["content"].get<std::string>();
        if (!source.empty()) content = escape(text(expression(source, scope)));
        for (int child : node.childIndices) content += body(d, static_cast<size_t>(child), scope);
        return element(name, attrs, content);
    }
    std::string ast(const ASTNode& node, const RuntimeModulePlan& module) {
        current = module.id; context.clear(); location(node);
        if (node.getType() == ASTNodeType::NoOp || node.getType() == ASTNodeType::ImportStatement) return "";
        if (node.getType() == ASTNodeType::ShowStatement) {
            const auto& show = static_cast<const ShowStatementNode&>(node);
            return escape(text(value(*buildExpressionIr(show.expr.get()), {})));
        }
        if (node.getType() != ASTNodeType::JtmlElement) reject("unsupported top-level statement");
        const auto& e = static_cast<const JtmlElementNode&>(node);
        for (const auto& a : e.attributes) {
            if (a.key == "data-jtml-component-def" && e.tagName == "template") return "";
            if (a.key != "data-jtml-static-call") continue;
            std::string name, encodedArgs;
            if (!jtmlStaticAttributeValue(a.value.get(), name)) reject("invalid retained component call");
            context = name;
            if (!e.content.empty()) reject("component slots are unsupported");
            for (const auto& attr : e.attributes) if (attr.key == "data-jtml-static-args")
                jtmlStaticAttributeValue(attr.value.get(), encodedArgs);
            Identity id{module.id, name};
            if (!definitions.count(id)) {
                id.first = InvalidSemanticModuleId;
                for (const auto& owner : project.modules) if (owner.id == module.id)
                    for (const auto& import : owner.imports) for (const auto& symbol : import.resolvedSymbols)
                        if (symbol.name == name && (symbol.kind == "make" || symbol.kind == "component")) {
                            if (id.first != InvalidSemanticModuleId && id.first != symbol.module)
                                reject("ambiguous imported component " + name);
                            id.first = symbol.module;
                        }
            }
            const auto d = definitions.find(id);
            if (d == definitions.end()) reject("unresolved module-qualified component " + name);
            const auto values = nlohmann::json::parse(encodedArgs);
            if (!values.is_array() || values.size() != d->second->params.size()) reject("literal component argument arity mismatch");
            Scope args;
            for (size_t i = 0; i < values.size(); ++i)
                args.emplace(d->second->params[i], expression(values[i].get<std::string>(), {}));
            return "<div>" + component(id, args) + "</div>";
        }
        const auto name = tag(e.tagName);
        std::string attrs, content;
        std::set<std::string> seen;
        for (const auto& a : e.attributes) {
            if (!seen.insert(a.key).second) reject("duplicate attribute " + a.key);
            attrs += attribute(a.key, a.value ? value(*buildExpressionIr(a.value.get()), {}) : nlohmann::json(true));
        }
        for (const auto& child : e.content) content += ast(*child, module);
        return element(name, attrs, content);
    }
public:
    StaticHtml(const SemanticProject& p, const RuntimeProjectPlan& r) : project(p), plan(r) {}
    std::string run() {
        const auto issues = analyzeSemanticProject(project);
        if (!issues.empty()) throw std::runtime_error("Static HTML module resolution: " + issues.front().path + ":" +
            std::to_string(issues.front().line) + ":" + std::to_string(issues.front().column) + ": " + issues.front().message);
        const RuntimeModulePlan* entry = nullptr;
        for (const auto& m : plan.modules) {
            current = m.id;
            if (!m.clientExecutable || m.syntax != "friendly") reject("retained Friendly module AST required");
            if (m.id == plan.entry) entry = &m;
            for (const auto& d : m.plan.componentDefinitions) {
                if (!definitions.emplace(Identity{m.id, d.name}, &d).second) reject("duplicate component definition " + d.name);
            }
        }
        if (!entry) reject("entry module missing");
        for (const auto& m : project.modules) {
            current = m.id; context.clear(); line = 0; column = 0;
            std::map<std::string, SemanticModuleId> imported;
            for (const auto& import : m.imports) for (const auto& symbol : import.resolvedSymbols) {
                if (symbol.kind != "make" && symbol.kind != "component") continue;
                const auto inserted = imported.emplace(symbol.name, symbol.module);
                if (!inserted.second && inserted.first->second != symbol.module) {
                    line = static_cast<int>(import.span.line); column = static_cast<int>(import.span.column);
                    reject("ambiguous imported component " + symbol.name);
                }
            }
        }
        std::string output;
        for (const auto& m : project.modules) {
            current = m.id;
            if (!m.ast || !m.ast->available) reject("retained module AST unavailable");
            for (const auto& node : m.ast->nodes) {
                if (m.id == project.entry) output += ast(*node, *entry);
                else {
                    // Imported initializers/markup cannot disappear merely because
                    // the entry calls only one exported component.
                    context.clear(); location(*node);
                    if (node->getType() == ASTNodeType::NoOp || node->getType() == ASTNodeType::ImportStatement) continue;
                    if (node->getType() == ASTNodeType::JtmlElement) {
                        const auto& e = static_cast<const JtmlElementNode&>(*node);
                        if (e.tagName == "template" && std::any_of(e.attributes.begin(), e.attributes.end(),
                            [](const auto& a) { return a.key == "data-jtml-component-def"; })) continue;
                    }
                    reject("imported module has executable top-level content");
                }
            }
        }
        return "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\"><meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'self'; script-src 'none'; connect-src 'none'; object-src 'none'; base-uri 'none'\"><title></title></head><body>" + output + "</body></html>\n";
    }
};
}
std::string emitStaticHtml(const SemanticProject& project, const RuntimeProjectPlan& plan) {
    return StaticHtml(project, plan).run();
}
}
