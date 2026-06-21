// jtml_interpreter.cpp
#include "jtml/interpreter.h"
#include "jtml/attribute_classifier.h"
#include "jtml/friendly.h"
#include "jtml/lexer.h"
#include "jtml/parser.h"
#include "jtml/runtime_plan.h"
#include "jtml/semantic.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <fstream>
#include <functional>
#include <set>
#include <stdexcept>
#include <sstream>
#include <thread>

namespace {
bool eventCarriesBrowserValue(const std::string& eventType) {
    return eventType == "onInput" ||
           eventType == "onChange" ||
           eventType == "onKeyUp" ||
           eventType == "onScroll";
}

void assignLoopIterator(const std::shared_ptr<JTML::Environment>& env,
                        const std::string& name,
                        const std::shared_ptr<JTML::VarValue>& value) {
    JTML::CompositeKey key{env->instanceID, name};
    auto existing = env->variables.find(key);
    if (existing != env->variables.end()) {
        existing->second->currentValue = value;
    } else {
        env->defineVariable(key, value);
    }
}

nlohmann::json varValueToJson(const std::shared_ptr<JTML::VarValue>& value) {
    if (!value) return nullptr;
    if (value->isNumber()) return value->getNumber();
    if (value->isBool()) return value->getBool();
    if (value->isString()) return value->getString();
    if (value->isArray()) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& item : value->getArray()->getArrayData()) {
            arr.push_back(varValueToJson(item));
        }
        return arr;
    }
    if (value->isDict()) {
        nlohmann::json obj = nlohmann::json::object();
        for (const auto& [key, item] : value->getDict()->getDictData()) {
            obj[key] = varValueToJson(item);
        }
        return obj;
    }
    return value->toString();
}

nlohmann::json stringVectorToJson(const std::vector<std::string>& values) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& value : values) out.push_back(value);
    return out;
}

std::string lowerString(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool emittedPayloadTypeMatches(const nlohmann::json& value, std::string declaredType) {
    declaredType = lowerString(declaredType);
    if (declaredType.empty() || declaredType == "any" || declaredType == "unknown") return true;
    if (declaredType == "string") return value.is_string();
    if (declaredType == "number" || declaredType == "float" ||
        declaredType == "double" || declaredType == "int") {
        return value.is_number();
    }
    if (declaredType == "boolean" || declaredType == "bool") return value.is_boolean();
    if (declaredType == "array" ||
        (declaredType.size() >= 2 && declaredType.substr(declaredType.size() - 2) == "[]")) {
        return value.is_array();
    }
    if (declaredType == "object" || declaredType == "dict" || declaredType == "record") {
        return value.is_object();
    }
    if (declaredType == "null") return value.is_null();
    return true;
}

std::shared_ptr<JTML::VarValue> jsonToVarValue(const nlohmann::json& value,
                                               const std::shared_ptr<JTML::Environment>& env) {
    if (value.is_boolean()) return std::make_shared<JTML::VarValue>(value.get<bool>());
    if (value.is_number()) return std::make_shared<JTML::VarValue>(value.get<double>());
    if (value.is_string()) return std::make_shared<JTML::VarValue>(value.get<std::string>());
    if (value.is_null()) return std::make_shared<JTML::VarValue>(std::string{});
    if (value.is_array()) {
        static size_t jsonArrayId = 0;
        JTML::CompositeKey key{env ? env->instanceID : 0, "__json_array_" + std::to_string(jsonArrayId++)};
        auto array = std::make_shared<JTML::ReactiveArray>(env, key);
        for (const auto& item : value) {
            array->push(jsonToVarValue(item, env));
        }
        return std::make_shared<JTML::VarValue>(array);
    }
    if (value.is_object()) {
        static size_t jsonDictId = 0;
        JTML::CompositeKey key{env ? env->instanceID : 0, "__json_dict_" + std::to_string(jsonDictId++)};
        auto dict = std::make_shared<JTML::ReactiveDict>(env, key);
        for (auto it = value.begin(); it != value.end(); ++it) {
            dict->set(it.key(), jsonToVarValue(it.value(), env));
        }
        return std::make_shared<JTML::VarValue>(dict);
    }
    return std::make_shared<JTML::VarValue>(value.dump());
}

std::string attrValueToString(const ExpressionStatementNode* expr) {
    if (!expr) return "";
    if (expr->getExprType() == ExpressionStatementNodeType::StringLiteral) {
        return static_cast<const StringLiteralExpressionStatementNode*>(expr)->value;
    }
    if (expr->getExprType() == ExpressionStatementNodeType::NumberLiteral) {
        return expr->toString();
    }
    if (expr->getExprType() == ExpressionStatementNodeType::BooleanLiteral) {
        return static_cast<const BooleanLiteralExpressionStatementNode*>(expr)->value ? "true" : "false";
    }
    return expr->toString();
}

std::string attrValueFor(const JtmlElementNode& elem, const std::string& key) {
    for (const auto& attr : elem.attributes) {
        if (attr.key == key) return attrValueToString(attr.value.get());
    }
    return "";
}

std::map<std::string, std::string> parseRuntimeMap(const std::string& encoded) {
    std::map<std::string, std::string> out;
    std::istringstream entries(encoded);
    std::string entry;
    while (std::getline(entries, entry, ';')) {
        if (entry.empty()) continue;
        const auto pos = entry.find('=');
        if (pos == std::string::npos) {
            out[entry] = "";
        } else {
            out[entry.substr(0, pos)] = entry.substr(pos + 1);
        }
    }
    return out;
}

std::vector<std::string> parseRuntimeList(const std::string& encoded) {
    std::vector<std::string> out;
    std::istringstream entries(encoded);
    std::string entry;
    while (std::getline(entries, entry, ';')) {
        if (!entry.empty()) out.push_back(entry);
    }
    return out;
}

std::map<std::string, int> parseRuntimeIntMap(const std::string& encoded) {
    std::map<std::string, int> out;
    for (const auto& [key, value] : parseRuntimeMap(encoded)) {
        try {
            out[key] = value.empty() ? 0 : std::stoi(value);
        } catch (...) {
            out[key] = 0;
        }
    }
    return out;
}

std::map<std::string, std::vector<std::string>> parseRuntimeStringListMap(const std::string& encoded) {
    std::map<std::string, std::vector<std::string>> out;
    for (const auto& [key, value] : parseRuntimeMap(encoded)) {
        std::vector<std::string> items;
        std::istringstream input(value);
        std::string item;
        while (std::getline(input, item, ',')) {
            if (!item.empty()) items.push_back(item);
        }
        out[key] = std::move(items);
    }
    return out;
}

int hexValue(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

std::string hexDecode(const std::string& encoded) {
    std::string out;
    if (encoded.size() % 2 != 0) return out;
    out.reserve(encoded.size() / 2);
    for (size_t i = 0; i < encoded.size(); i += 2) {
        const int hi = hexValue(encoded[i]);
        const int lo = hexValue(encoded[i + 1]);
        if (hi < 0 || lo < 0) return "";
        out.push_back(static_cast<char>((hi << 4) | lo));
    }
    return out;
}

JTML::InstanceID parseInstanceId(const std::string& explicitId,
                                 const std::string& instanceName) {
    auto parseDigits = [](const std::string& value) -> JTML::InstanceID {
        if (value.empty()) return 0;
        for (char ch : value) {
            if (!std::isdigit(static_cast<unsigned char>(ch))) return 0;
        }
        try {
            return static_cast<JTML::InstanceID>(std::stoull(value));
        } catch (...) {
            return 0;
        }
    };
    if (auto id = parseDigits(explicitId)) return id;
    const auto pos = instanceName.rfind('_');
    if (pos != std::string::npos) return parseDigits(instanceName.substr(pos + 1));
    return 0;
}

jtml::SemanticModuleId parseSemanticModuleId(const std::string& value) {
    if (value.empty()) return jtml::InvalidSemanticModuleId;
    for (char ch : value) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            return jtml::InvalidSemanticModuleId;
        }
    }
    try {
        return static_cast<jtml::SemanticModuleId>(std::stoul(value));
    } catch (...) {
        return jtml::InvalidSemanticModuleId;
    }
}
}

Interpreter::~Interpreter() {
    if (wsServer) {
        wsServer->stop();
    }
    if (wsThread.joinable()) {
        wsThread.join();
         // Ensure the thread is joined before destruction
    }
}
Interpreter::Interpreter(JtmlTranspiler& transpilerRef, InterpreterConfig config)
    : transpiler(transpilerRef) // Initialize transpiler reference
{
    // Create the Renderer
    renderer = std::make_unique<JTML::Renderer>();

    // Initialize the global and current environments
    globalEnv = std::make_shared<JTML::Environment>(nullptr, 0, renderer.get());
    currentEnv = globalEnv;

    globalEnv->setRenderer(renderer.get());
    currentEnv->setRenderer(renderer.get());

    // Initialize counters
    nodeID = 0;
    uniqueVarID  = 0;
    uniqueArrayVarID = 1;
    uniqueDictVarID = 1;

    if (config.startWebSocket) {
        wsServer = std::make_shared<JTML::WebSocketServer>();

        wsServer->setOpenCallback(
            [this](websocketpp::connection_hdl hdl) {
                std::cout << "[DEBUG] New WebSocket connection established.\n";
                populateBindings(hdl);
        });

        // Set Renderer callback to send messages via WebSocket
        renderer->setFrontendCallback([this](const std::string& msg) {
            wsServer->broadcastMessage(msg);
        });

        // Set WebSocket message handler
        wsServer->setMessageCallback(
            [this](const std::string& msg, websocketpp::connection_hdl hdl) {
                handleFrontendMessage(msg, hdl);
            });

        uint16_t port = config.wsPort;
        wsThread = std::thread([this, port]() {
            wsServer->run(port);
        });
    } else {
        // Headless mode: renderer updates are swallowed instead of broadcast.
        renderer->setFrontendCallback([](const std::string&) {});
    }
}

void Interpreter::reset() {
    // Drop the current program state. We keep `wsServer`, `wsThread`, and
    // `renderer` alive so the frontend connection is preserved across
    // lesson / playground switches.
    classDeclarations.clear();
    componentInstances.clear();
    componentDefinitions.clear();
    dynamicComponentInstances.clear();
    componentRenderGeneration = 0;
    globalEnv = std::make_shared<JTML::Environment>(nullptr, 0, renderer.get());
    currentEnv = globalEnv;
    globalEnv->setRenderer(renderer.get());

    nodeID = 0;
    uniqueVarID = 0;
    uniqueArrayVarID = 1;
    uniqueDictVarID = 1;
    inFunctionContext = false;
}

void Interpreter::reload(const std::vector<std::unique_ptr<ASTNode>>& program) {
    reset();
    interpret(program);
    if (wsServer) {
        // Hint to connected clients that a new program is live. Clients that
        // have navigated to a fresh HTML document will reconnect anyway; this
        // covers the edge case where the HTML has been swapped via srcdoc but
        // the old socket is still open.
        wsServer->broadcastMessage(R"({"type":"reload"})");
    }
}

std::string Interpreter::getBindingsJSON() {
    nlohmann::json out;
    out["content"]    = nlohmann::json::object();
    out["attributes"] = nlohmann::json::object();
    out["conditions"] = nlohmann::json::object();
    out["loops"]      = nlohmann::json::object();
    out["state"]      = nlohmann::json::object();
    if (!globalEnv) return out.dump();

    auto isGeneratedBindingName = [](const std::string& name) {
        return name.rfind("expr_", 0) == 0 ||
               name.rfind("attr_", 0) == 0 ||
               name.rfind("cond_", 0) == 0 ||
               name.rfind("loop_", 0) == 0 ||
               name.rfind("__", 0) == 0;
    };

    for (const auto& [key, info] : globalEnv->variables) {
        if (!info || isGeneratedBindingName(key.varName)) continue;
        out["state"][key.varName] = varValueToJson(info->currentValue);
    }

    for (const auto& [_, bindingInfos] : globalEnv->getBindings()) {
        for (const auto& binding : bindingInfos) {
            if (binding.bindingType == "attribute_event") continue;
            auto bindingEnv = environmentForInstance(binding.varName.instanceID);
            if (!bindingEnv) bindingEnv = globalEnv;
            auto varVal = bindingEnv->getVariable(binding.varName);
            std::string valueStr = varVal ? varVal->toString() : "";
            if (binding.bindingType == "content") {
                out["content"][binding.elementId] = valueStr;
            } else if (binding.bindingType == "attribute") {
                out["attributes"][binding.elementId][binding.attribute] = valueStr;
            } else if (binding.bindingType == "if") {
                out["conditions"][binding.elementId] = isTruthy(valueStr);
            } else if (binding.bindingType == "for") {
                out["loops"][binding.elementId] = varValueToJson(varVal);
            } else if (binding.bindingType == "while") {
                out["conditions"][binding.elementId] = isTruthy(valueStr);
            }
        }
    }
    return out.dump();
}

std::string Interpreter::getStateJSON() {
    auto moduleIdToJson = [](jtml::SemanticModuleId id) {
        return id == jtml::InvalidSemanticModuleId ? nlohmann::json(nullptr) : nlohmann::json(id);
    };
    auto bodyPlanToJson = [&](const auto& bodyPlan) {
        nlohmann::json out = nlohmann::json::array();
        for (const auto& node : bodyPlan) {
            out.push_back({
                {"definitionModule", moduleIdToJson(node.definitionModule)},
                {"indent", node.indent},
                {"parentIndex", node.parentIndex},
                {"childIndices", node.childIndices},
                {"kind", node.kind},
                {"head", node.head},
                {"name", node.name},
                {"text", node.text},
                {"operator", node.operatorToken},
                {"expression", node.expression},
                {"keyExpression", node.keyExpression},
                {"reads", node.reads},
                {"writes", node.writes},
                {"renderRoot", node.renderRoot},
            });
        }
        return out;
    };
    auto trimString = [](const std::string& value) {
        size_t start = 0;
        while (start < value.size() &&
               std::isspace(static_cast<unsigned char>(value[start]))) {
            ++start;
        }
        size_t end = value.size();
        while (end > start &&
               std::isspace(static_cast<unsigned char>(value[end - 1]))) {
            --end;
        }
        return value.substr(start, end - start);
    };
    auto splitWords = [](const std::string& value) {
        std::vector<std::string> out;
        std::string current;
        char quote = '\0';
        int depth = 0;
        for (size_t i = 0; i < value.size(); ++i) {
            const char ch = value[i];
            if (quote != '\0') {
                current.push_back(ch);
                if (ch == '\\' && i + 1 < value.size()) {
                    current.push_back(value[++i]);
                } else if (ch == quote) {
                    quote = '\0';
                }
                continue;
            }
            if (ch == '"' || ch == '\'') {
                quote = ch;
                current.push_back(ch);
                continue;
            }
            if (ch == '(' || ch == '[' || ch == '{') ++depth;
            if (ch == ')' || ch == ']' || ch == '}') depth = std::max(0, depth - 1);
            if (std::isspace(static_cast<unsigned char>(ch)) && depth == 0) {
                if (!current.empty()) {
                    out.push_back(current);
                    current.clear();
                }
                continue;
            }
            current.push_back(ch);
        }
        if (!current.empty()) out.push_back(current);
        return out;
    };
    auto splitTopLevelList = [&](const std::string& source) {
        std::vector<std::string> out;
        std::string current;
        char quote = '\0';
        int depth = 0;
        for (size_t i = 0; i < source.size(); ++i) {
            const char ch = source[i];
            if (quote != '\0') {
                current.push_back(ch);
                if (ch == '\\' && i + 1 < source.size()) {
                    current.push_back(source[++i]);
                } else if (ch == quote) {
                    quote = '\0';
                }
                continue;
            }
            if (ch == '"' || ch == '\'') {
                quote = ch;
                current.push_back(ch);
                continue;
            }
            if (ch == '(' || ch == '[' || ch == '{') ++depth;
            if (ch == ')' || ch == ']' || ch == '}') depth = std::max(0, depth - 1);
            if (ch == ',' && depth == 0) {
                const auto trimmed = trimString(current);
                if (!trimmed.empty()) out.push_back(trimmed);
                current.clear();
                continue;
            }
            current.push_back(ch);
        }
        const auto trimmed = trimString(current);
        if (!trimmed.empty()) out.push_back(trimmed);
        return out;
    };
    auto unquote = [&](const std::string& value) {
        const std::string trimmed = trimString(value);
        if (trimmed.size() >= 2 &&
            ((trimmed.front() == '"' && trimmed.back() == '"') ||
             (trimmed.front() == '\'' && trimmed.back() == '\''))) {
            return trimmed.substr(1, trimmed.size() - 2);
        }
        return trimmed;
    };
    auto escapeHtml = [](const std::string& value) {
        std::string out;
        out.reserve(value.size());
        for (char ch : value) {
            if (ch == '&') out += "&amp;";
            else if (ch == '<') out += "&lt;";
            else if (ch == '>') out += "&gt;";
            else if (ch == '"') out += "&quot;";
            else out.push_back(ch);
        }
        return out;
    };
    auto escapeAttr = [&](const std::string& value) {
        std::string out = escapeHtml(value);
        std::string replaced;
        replaced.reserve(out.size());
        for (char ch : out) {
            if (ch == '\'') replaced += "&#39;";
            else replaced.push_back(ch);
        }
        return replaced;
    };
    auto tagName = [](const std::string& head) {
        if (head == "app" || head == "shell") return std::string("div");
        if (head == "topbar") return std::string("header");
        if (head == "sidebar" || head == "drawer") return std::string("aside");
        if (head == "content") return std::string("main");
        if (head == "metric") return std::string("article");
        if (head == "toolbar" || head == "tabs" || head == "alert" ||
            head == "toast" || head == "loading" || head == "error" ||
            head == "empty" || head == "spacer") {
            return std::string("div");
        }
        if (head == "tab") return std::string("button");
        if (head == "badge") return std::string("span");
        if (head == "modal") return std::string("section");
        if (head == "field") return std::string("label");
        if (head == "page") return std::string("main");
        if (head == "box" || head == "stack" || head == "cluster" ||
            head == "split" || head == "grid") {
            return std::string("div");
        }
        if (head == "card" || head == "panel") return std::string("section");
        if (head == "text") return std::string("p");
        if (head == "link" || head == "navlink") return std::string("a");
        if (head == "image") return std::string("img");
        if (head == "list") return std::string("ul");
        if (head == "listOrdered") return std::string("ol");
        if (head == "item") return std::string("li");
        if (head == "checkbox" || head == "file" || head == "dropzone") return std::string("input");
        if (head == "title") return std::string("h1");
        if (head == "subtitle") return std::string("p");
        return head;
    };
    auto isUiPrimitive = [](const std::string& name) {
        static const std::vector<std::string> primitives = {
            "app", "shell", "topbar", "sidebar", "content", "panel", "card",
            "metric", "stack", "cluster", "split", "grid", "toolbar", "tabs",
            "tab", "alert", "badge", "modal", "drawer", "toast", "loading",
            "error", "empty", "field", "spacer"
        };
        return std::find(primitives.begin(), primitives.end(), name) != primitives.end();
    };
    auto isUiModifier = [](const std::string& name) {
        static const std::vector<std::string> modifiers = {
            "cols", "gap", "pad", "radius", "shadow", "tone",
            "align", "justify", "width", "surface"
        };
        return std::find(modifiers.begin(), modifiers.end(), name) != modifiers.end();
    };
    auto isAttrName = [](const std::string& name) {
        static const std::vector<std::string> attrs = {
            "id", "class", "style", "role", "href", "src", "alt", "title",
            "type", "name", "value", "placeholder", "for", "rel", "target",
            "method", "action", "enctype", "autocomplete", "inputmode",
            "pattern", "accept", "poster", "preload", "kind", "srclang",
            "label", "width", "height", "min", "max", "step", "minlength",
            "maxlength", "rows", "cols", "cx", "cy", "r", "x", "y", "x1",
            "y1", "x2", "y2", "d", "points", "viewBox", "fill", "stroke",
            "stroke-width", "transform", "opacity"
        };
        return std::find(attrs.begin(), attrs.end(), name) != attrs.end() ||
               name.rfind("aria-", 0) == 0 ||
               name.rfind("data-", 0) == 0;
    };
    auto isBooleanAttr = [](const std::string& name) {
        static const std::vector<std::string> attrs = {
            "disabled", "required", "checked", "selected", "multiple",
            "readonly", "autofocus", "playsinline", "open", "hidden",
            "controls", "autoplay", "loop", "muted"
        };
        return std::find(attrs.begin(), attrs.end(), name) != attrs.end();
    };
    auto evalComponentExpression = [&](const std::string& expr,
                                       const std::shared_ptr<JTML::Environment>& env)
            -> std::shared_ptr<JTML::VarValue> {
        const std::string source = "jtml 2\nlet __jtml_render_expr = " + expr + "\n";
        const std::string lowered = jtml::normalizeSourceSyntax(source, jtml::SyntaxMode::Friendly);
        Lexer lexer(lowered);
        auto tokens = lexer.tokenize();
        if (!lexer.getErrors().empty()) return nullptr;
        Parser parser(std::move(tokens));
        auto parsed = parser.parseProgram();
        if (!parser.getErrors().empty() || parsed.empty() ||
            parsed.front()->getType() != ASTNodeType::DefineStatement) {
            return nullptr;
        }
        const auto& define = static_cast<const DefineStatementNode&>(*parsed.front());
        try {
            return evaluateExpression(define.expression.get(), env);
        } catch (...) {
            return nullptr;
        }
    };
    auto expressionText = [&](const std::string& expr,
                              const std::shared_ptr<JTML::Environment>& env) {
        const std::string trimmed = trimString(expr);
        if (trimmed.empty()) return std::string{};
        if (trimmed != unquote(trimmed)) return unquote(trimmed);
        const bool simpleIdentifier = !trimmed.empty() &&
            (std::isalpha(static_cast<unsigned char>(trimmed.front())) ||
             trimmed.front() == '_') &&
            std::all_of(trimmed.begin() + 1, trimmed.end(), [](char ch) {
                return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
            });
        if (simpleIdentifier && env) {
            try {
                auto value = env->getVariable(JTML::CompositeKey{env->instanceID, trimmed});
                if (value) return value->toString();
            } catch (...) {
            }
        }
        auto value = evalComponentExpression(trimmed, env);
        return value ? value->toString() : trimmed;
    };
    auto expressionValue = [&](const std::string& expr,
                               const std::shared_ptr<JTML::Environment>& env) {
        const std::string trimmed = trimString(expr);
        if (trimmed.empty()) return std::make_shared<JTML::VarValue>(std::string{});
        if (trimmed != unquote(trimmed)) {
            return std::make_shared<JTML::VarValue>(unquote(trimmed));
        }
        if (auto value = evalComponentExpression(trimmed, env)) return value;
        return std::make_shared<JTML::VarValue>(trimmed);
    };
    auto parseComponentActionInvocation = [&](const std::string& raw,
                                              const std::shared_ptr<JTML::Environment>& env) {
        const std::string source = trimString(raw);
        nlohmann::json out = {
            {"name", source},
            {"args", nlohmann::json::array()},
        };
        if (source.empty()) return out;
        const auto open = source.find('(');
        if (open == std::string::npos || source.back() != ')') {
            return out;
        }
        out["name"] = trimString(source.substr(0, open));
        const std::string inner = source.substr(open + 1, source.size() - open - 2);
        for (const auto& argExpr : splitTopLevelList(inner)) {
            if (auto value = evalComponentExpression(argExpr, env)) {
                out["args"].push_back(varValueToJson(value));
            } else {
                out["args"].push_back(unquote(argExpr));
            }
        }
        return out;
    };
    auto childNodes = [](const RuntimeComponentDefinition& def,
                         const BodyPlanNode& node) {
        std::vector<const BodyPlanNode*> out;
        for (int index : node.childIndices) {
            if (index < 0 || static_cast<size_t>(index) >= def.bodyPlan.size()) continue;
            out.push_back(&def.bodyPlan[static_cast<size_t>(index)]);
        }
        return out;
    };
    std::function<std::string(const RuntimeComponentDefinition&,
                              const RuntimeComponentInstance&,
                              const BodyPlanNode&,
                              const std::shared_ptr<JTML::Environment>&,
                              const std::string&)> renderNode;
    std::function<std::string(const RuntimeComponentDefinition&,
                              const RuntimeComponentInstance&,
                              const BodyPlanNode&,
                              const std::shared_ptr<JTML::Environment>&,
                              const std::string&)> renderChildren =
        [&](const RuntimeComponentDefinition& def,
            const RuntimeComponentInstance& instance,
            const BodyPlanNode& node,
            const std::shared_ptr<JTML::Environment>& env,
            const std::string& slotHtml) {
            std::string out;
            for (const auto* child : childNodes(def, node)) {
                if (!child || child->name == "else") continue;
                out += renderNode(def, instance, *child, env, slotHtml);
            }
            return out;
        };
    auto slotName = [&](const BodyPlanNode& node) {
        const auto slotWords = splitWords(node.text);
        return !slotWords.empty() && slotWords[0] == "slot" && slotWords.size() > 1
            ? slotWords[1]
            : std::string{};
    };
    auto renderSlotPlan = [&](const RuntimeComponentInstance& instance,
                              const std::shared_ptr<JTML::Environment>& env,
                              const std::string& requestedName) {
        RuntimeComponentDefinition slotDef;
        slotDef.name = "__slot";
        slotDef.bodyPlan = instance.slotPlan;
        std::string out;
        for (const auto& node : slotDef.bodyPlan) {
            if (node.parentIndex != -1 ||
                (node.kind != "template" && node.kind != "slot")) {
                continue;
            }
            if (node.kind == "slot" || node.name == "slot") {
                if (slotName(node) == requestedName) {
                    out += renderChildren(slotDef, instance, node, env, "");
                }
                continue;
            }
            if (requestedName.empty()) {
                out += renderNode(slotDef, instance, node, env, "");
            }
        }
        return out;
    };
    auto elseSibling = [&](const RuntimeComponentDefinition& def,
                           const BodyPlanNode& node) -> const BodyPlanNode* {
        auto it = std::find_if(def.bodyPlan.begin(), def.bodyPlan.end(),
            [&](const BodyPlanNode& candidate) { return &candidate == &node; });
        if (it == def.bodyPlan.end()) return nullptr;
        for (++it; it != def.bodyPlan.end(); ++it) {
            if (it->parentIndex == node.parentIndex) {
                return it->name == "else" ? &(*it) : nullptr;
            }
        }
        return nullptr;
    };
    auto nodeIndexString = [](const RuntimeComponentDefinition& def,
                              const BodyPlanNode& node) {
        const auto indexIt = std::find_if(def.bodyPlan.begin(), def.bodyPlan.end(),
            [&](const BodyPlanNode& candidate) { return &candidate == &node; });
        return indexIt == def.bodyPlan.end()
            ? std::string("x")
            : std::to_string(std::distance(def.bodyPlan.begin(), indexIt));
    };
    auto pathSegment = [](const std::string& value) {
        std::string out;
        for (char ch : value) {
            if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-') {
                out.push_back(ch);
            } else {
                out.push_back('_');
            }
        }
        while (!out.empty() && out.front() == '_') out.erase(out.begin());
        while (!out.empty() && out.back() == '_') out.pop_back();
        return out.empty() ? std::string("empty") : out;
    };
    auto slotPlanForNode = [&](const RuntimeComponentDefinition& def,
                               const BodyPlanNode& node) {
        std::vector<BodyPlanNode> out;
        std::function<int(const BodyPlanNode&, int)> cloneSubtree =
            [&](const BodyPlanNode& source, int parentIndex) {
                BodyPlanNode copy = source;
                const int newIndex = static_cast<int>(out.size());
                copy.parentIndex = parentIndex;
                copy.childIndices.clear();
                out.push_back(copy);
                for (int childIndex : source.childIndices) {
                    if (childIndex < 0 || static_cast<size_t>(childIndex) >= def.bodyPlan.size()) {
                        continue;
                    }
                    const int clonedChildIndex = cloneSubtree(
                        def.bodyPlan[static_cast<size_t>(childIndex)],
                        newIndex);
                    if (clonedChildIndex >= 0) {
                        out[static_cast<size_t>(newIndex)].childIndices.push_back(clonedChildIndex);
                    }
                }
                return newIndex;
            };
        for (int childIndex : node.childIndices) {
            if (childIndex < 0 || static_cast<size_t>(childIndex) >= def.bodyPlan.size()) {
                continue;
            }
            cloneSubtree(def.bodyPlan[static_cast<size_t>(childIndex)], -1);
        }
        return out;
    };
    std::vector<std::string> renderPath;
    auto renderNested = [&](const RuntimeComponentDefinition& parentDef,
                            const RuntimeComponentInstance& parentInstance,
                            const BodyPlanNode& node,
                            const std::shared_ptr<JTML::Environment>& env,
                            const std::string& name) {
        const auto* nestedDef = findComponentDefinition(
            name,
            node.definitionModule == jtml::InvalidSemanticModuleId
                ? parentDef.moduleId
                : node.definitionModule,
            parentInstance.moduleId);
        if (!nestedDef) return std::string{};
        const auto nodeIndex = nodeIndexString(parentDef, node);
        std::string pathSuffix;
        for (const auto& part : renderPath) {
            pathSuffix += "_" + part;
        }
        const std::string nestedId = parentInstance.id + "__" + name + "_" + nodeIndex + pathSuffix;
        auto dynamicIt = dynamicComponentInstances.find(nestedId);
        if (dynamicIt == dynamicComponentInstances.end()) {
            RuntimeComponentInstance nestedInstance;
            nestedInstance.moduleId = parentDef.moduleId;
            nestedInstance.definitionModule = nestedDef->moduleId;
            nestedInstance.id = nestedId;
            nestedInstance.parentId = parentInstance.id;
            nestedInstance.component = name;
            nestedInstance.role = "nested";
            nestedInstance.instanceID = static_cast<JTML::InstanceID>(
                100000 + (std::hash<std::string>{}(nestedId) % 800000));
            nestedInstance.environment = std::make_shared<JTML::Environment>(
                env, nestedInstance.instanceID, renderer.get());
            nestedInstance.environment->setRenderer(renderer.get());
            nestedInstance.stateNames = nestedDef->localState;
            nestedInstance.derivedNames = nestedDef->localDerived;
            nestedInstance.actionNames = nestedDef->localActions;
            nestedInstance.effectNames = nestedDef->localEffects;
            dynamicIt = dynamicComponentInstances.emplace(nestedId, std::move(nestedInstance)).first;
        }
        RuntimeComponentInstance& nestedInstance = dynamicIt->second;
        nestedInstance.lastSeenRenderGeneration = componentRenderGeneration;
        nestedInstance.moduleId = parentDef.moduleId;
        nestedInstance.definitionModule = nestedDef->moduleId;
        nestedInstance.parentId = parentInstance.id;
        nestedInstance.component = name;
        nestedInstance.role = "nested";
        nestedInstance.stateNames = nestedDef->localState;
        nestedInstance.derivedNames = nestedDef->localDerived;
        nestedInstance.actionNames = nestedDef->localActions;
        nestedInstance.effectNames = nestedDef->localEffects;
        if (!nestedInstance.environment) {
            nestedInstance.environment = std::make_shared<JTML::Environment>(
                env, nestedInstance.instanceID, renderer.get());
        }
        nestedInstance.environment->setRenderer(renderer.get());
        const auto words = splitWords(node.text);
        for (size_t i = 0; i < nestedDef->params.size(); ++i) {
            const std::string raw = i + 1 < words.size() ? words[i + 1] : "";
            JTML::CompositeKey key{nestedInstance.environment->instanceID, nestedDef->params[i]};
            auto value = expressionValue(raw, env);
            if (nestedInstance.environment->hasVariable(key)) {
                nestedInstance.environment->setVariable(key, value);
            } else {
                nestedInstance.environment->defineVariable(key, value);
            }
        }
        nestedInstance.eventHandlers.clear();
        for (size_t i = 1 + nestedDef->params.size(); i + 2 < words.size();) {
            if (words[i] != "on") {
                ++i;
                continue;
            }
            const std::string eventName = words[i + 1];
            const auto handler = parseComponentActionInvocation(words[i + 2], env);
            const std::string handlerName = handler.value("name", std::string{});
            if (!eventName.empty() && !handlerName.empty()) {
                nestedInstance.eventHandlers[eventName] = {
                    handlerName,
                    handler.value("args", nlohmann::json::array()),
                };
            }
            i += 3;
        }
        nestedInstance.slotPlan = slotPlanForNode(parentDef, node);
        for (const auto& planned : nestedDef->bodyPlan) {
            if (planned.parentIndex != -1 || planned.name.empty()) continue;
            if (planned.kind == "state") {
                JTML::CompositeKey key{nestedInstance.environment->instanceID, planned.name};
                if (!nestedInstance.environment->hasVariable(key)) {
                    nestedInstance.environment->defineVariable(
                        key,
                        expressionValue(planned.expression, nestedInstance.environment));
                }
            } else if (planned.kind == "derived") {
                JTML::CompositeKey key{nestedInstance.environment->instanceID, planned.name};
                nestedInstance.environment->defineVariable(
                    key,
                    expressionValue(planned.expression, nestedInstance.environment));
            }
        }
        const std::string slotHtml = renderChildren(parentDef, parentInstance, node, env, "");
        std::string out;
        for (const auto& root : nestedDef->bodyPlan) {
            if (root.renderRoot && root.kind == "template") {
                out += renderNode(*nestedDef, nestedInstance, root, nestedInstance.environment, slotHtml);
            }
        }
        return "<div data-jtml-direct-instance=\"" + escapeAttr(nestedInstance.id) + "\"" +
               " data-jtml-component=\"" + escapeAttr(name) + "\"" +
               " data-jtml-component-parent=\"" + escapeAttr(parentInstance.id) + "\"" +
               " data-jtml-nested-component=\"true\">" + out + "</div>";
    };
    renderNode = [&](const RuntimeComponentDefinition& def,
                     const RuntimeComponentInstance& instance,
                     const BodyPlanNode& node,
                     const std::shared_ptr<JTML::Environment>& env,
                     const std::string& slotHtml) {
        if (node.kind != "template" && node.kind != "slot") return std::string{};
        const auto words = splitWords(node.text);
        const std::string head = node.name.empty()
            ? (words.empty() ? std::string("box") : words[0])
            : node.name;
        if (head == "slot") {
            const std::string requestedSlot = words.size() > 1 ? words[1] : "";
            return requestedSlot.empty() && !slotHtml.empty()
                ? slotHtml
                : renderSlotPlan(instance, env, requestedSlot);
        }
        if (head == "else") return std::string{};
        if (head == "if") {
            const std::string condition = node.expression.empty()
                ? (words.size() > 1 ? node.text.substr(node.text.find(words[1])) : "")
                : node.expression;
            auto value = evalComponentExpression(condition, env);
            if (value && isTruthy(value->toString())) {
                return renderChildren(def, instance, node, env, slotHtml);
            }
            if (const auto* elseNode = elseSibling(def, node)) {
                return renderChildren(def, instance, *elseNode, env, slotHtml);
            }
            return std::string{};
        }
        if (head == "for") {
            const std::string raw = node.expression.empty()
                ? (words.size() > 1 ? node.text.substr(node.text.find(words[1])) : "")
                : node.expression;
            const auto inPos = raw.find(" in ");
            if (inPos == std::string::npos) return std::string{};
            const std::string itemName = trimString(raw.substr(0, inPos));
            const std::string listExpr = trimString(raw.substr(inPos + 4));
            auto value = evalComponentExpression(listExpr, env);
            if (!value || !value->isArray()) return std::string{};
            std::string out;
            size_t itemIndex = 0;
            for (const auto& item : value->getArray()->getArrayData()) {
                assignLoopIterator(env, itemName, item);
                std::string keySegment = std::to_string(itemIndex++);
                if (!node.keyExpression.empty()) {
                    keySegment = pathSegment(expressionText(node.keyExpression, env));
                }
                renderPath.push_back("for" + nodeIndexString(def, node) + "_" + keySegment);
                out += renderChildren(def, instance, node, env, slotHtml);
                renderPath.pop_back();
            }
            return out;
        }
        const auto preferredModule = node.definitionModule == jtml::InvalidSemanticModuleId
            ? def.moduleId
            : node.definitionModule;
        if (const auto* nested = findComponentDefinition(head, preferredModule, instance.moduleId)) {
            (void)nested;
            return renderNested(def, instance, node, env, head);
        }
        const std::string children = renderChildren(def, instance, node, env, slotHtml);
        if (head == "show" || head == "text") {
            const std::string expr = node.expression.empty()
                ? (words.size() > 1 ? node.text.substr(node.text.find(words[1])) : "")
                : node.expression;
            return "<p>" + escapeHtml(expressionText(expr, env)) + "</p>" + children;
        }
        std::vector<std::pair<std::string, std::string>> attrs;
        std::vector<std::pair<std::string, std::string>> modifiers;
        std::vector<std::string> boolAttrs;
        std::vector<std::string> content;
        for (size_t i = 1; i < words.size(); ++i) {
            const std::string& token = words[i];
            if (token == "click") break;
            if ((head == "link" || head == "navlink") && token == "to" && i + 1 < words.size()) {
                const std::string routeTarget = expressionText(words[++i], env);
                const std::string routeHref = routeTarget.empty() || routeTarget.front() == '#'
                    ? routeTarget
                    : "#" + routeTarget;
                attrs.push_back({"href", "javascript:void(0)"});
                attrs.push_back({"data-jtml-href", routeHref.empty() ? "#/" : routeHref});
                attrs.push_back({"data-jtml-link", "true"});
                if (i + 2 < words.size() && words[i + 1] == "active-class") {
                    attrs.push_back({"data-jtml-active-class", expressionText(words[i + 2], env)});
                    i += 2;
                }
                continue;
            }
            if (isUiModifier(token)) {
                if (i + 1 < words.size() &&
                    !isAttrName(words[i + 1]) &&
                    !isBooleanAttr(words[i + 1]) &&
                    !isUiModifier(words[i + 1]) &&
                    words[i + 1] != "click") {
                    modifiers.push_back({token, expressionText(words[i + 1], env)});
                    ++i;
                } else {
                    modifiers.push_back({token, "true"});
                }
            } else if (isAttrName(token)) {
                if (i + 1 < words.size() &&
                    !isAttrName(words[i + 1]) &&
                    !isBooleanAttr(words[i + 1]) &&
                    words[i + 1] != "click") {
                    attrs.push_back({token, expressionText(words[i + 1], env)});
                    ++i;
                } else {
                    content.push_back(token);
                }
            } else if (isBooleanAttr(token)) {
                boolAttrs.push_back(token);
            } else {
                content.push_back(token);
            }
        }
        std::string attrHtml;
        std::string classValue;
        auto hasAttr = [&](const std::string& needle) {
            return std::any_of(attrs.begin(), attrs.end(),
                [&](const auto& attr) { return attr.first == needle; });
        };
        if (head == "checkbox" && !hasAttr("type")) attrs.push_back({"type", "checkbox"});
        if ((head == "file" || head == "dropzone") && !hasAttr("type")) attrs.push_back({"type", "file"});
        for (const auto& [name, value] : attrs) {
            if (value.empty()) continue;
            if (name == "class") {
                if (!classValue.empty()) classValue += " ";
                classValue += value;
            } else {
                attrHtml += " " + name + "=\"" + escapeAttr(value) + "\"";
            }
        }
        for (const auto& name : boolAttrs) attrHtml += " " + name;
        if (isUiPrimitive(head)) {
            if (!classValue.empty()) classValue += " ";
            classValue += "jtml-" + head;
            attrHtml += " data-jtml-ui=\"" + escapeAttr(head) + "\"";
        }
        for (const auto& [name, value] : modifiers) {
            std::string safeValue;
            for (char ch : value) {
                if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-') {
                    safeValue.push_back(ch);
                } else {
                    safeValue.push_back('-');
                }
            }
            if (!classValue.empty()) classValue += " ";
            classValue += "jtml-" + name +
                (safeValue.empty() || safeValue == "true" ? std::string{} : "-" + safeValue);
            attrHtml += " data-jtml-ui-" + name + "=\"" + escapeAttr(value) + "\"";
        }
        if (!classValue.empty()) {
            attrHtml = " class=\"" + escapeAttr(classValue) + "\"" + attrHtml;
        }
        if (head != "button") attrHtml += " data-jtml-live-body-plan=\"" + escapeAttr(head) + "\"";
        const std::string tag = head == "button" ? std::string("button") : tagName(head);
        std::string inlineText;
        if (children.empty() && (!content.empty() || !node.expression.empty())) {
            std::ostringstream expr;
            if (!content.empty()) {
                for (size_t i = 0; i < content.size(); ++i) {
                    if (i > 0) expr << ' ';
                    expr << content[i];
                }
            } else {
                expr << node.expression;
            }
            inlineText = escapeHtml(expressionText(expr.str(), env));
        }
        if (head == "button") {
            auto clickIt = std::find(words.begin(), words.end(), "click");
            if (clickIt != words.end() && ++clickIt != words.end()) {
                const auto invocation = parseComponentActionInvocation(*clickIt, env);
                const std::string actionName = invocation.value("name", std::string{});
                if (!actionName.empty()) {
                    attrHtml += " data-jtml-direct-component-id=\"" +
                        escapeAttr(instance.id) + "\"";
                    attrHtml += " data-jtml-direct-component-action=\"" +
                        escapeAttr(actionName) + "\"";
                    attrHtml += " data-jtml-direct-component-args=\"" +
                        escapeAttr(invocation["args"].dump()) + "\"";
                }
            }
            return "<button type=\"button\"" + attrHtml + ">" +
                   inlineText + children + "</button>";
        }
        return "<" + tag + attrHtml + ">" + inlineText + children + "</" + tag + ">";
    };
    auto renderComponent = [&](const RuntimeComponentInstance& instance) {
        const auto* definition = findComponentDefinition(
            instance.component, instance.definitionModule, instance.moduleId);
        auto env = instance.environment ? instance.environment : globalEnv;
        if (!definition || !env) return std::string{};
        std::string out;
        for (const auto& node : definition->bodyPlan) {
            if (node.renderRoot && node.kind == "template") {
                out += renderNode(*definition, instance, node, env, "");
            }
        }
        const std::string descendantPrefix = instance.id + "__";
        for (auto it = dynamicComponentInstances.begin();
             it != dynamicComponentInstances.end();) {
            const bool ownedDescendant = it->first.rfind(descendantPrefix, 0) == 0;
            if (ownedDescendant &&
                it->second.lastSeenRenderGeneration != componentRenderGeneration) {
                it = dynamicComponentInstances.erase(it);
            } else {
                ++it;
            }
        }
        return out;
    };

    nlohmann::json out;
    out["variables"] = nlohmann::json::object();
    out["functions"] = nlohmann::json::object();
    out["componentDefinitions"] = nlohmann::json::parse(getComponentDefinitionsJSON());
    out["components"] = nlohmann::json::array();
    if (!globalEnv) return out.dump();
    ++componentRenderGeneration;

    for (const auto& [key, info] : globalEnv->variables) {
        if (!info) continue;
        if (key.varName.rfind("__", 0) == 0) continue;
        out["variables"][key.varName] = {
            {"instance", key.instanceID},
            {"kind", info->kind == JTML::VarKind::Derived ? "derived" :
                     (info->kind == JTML::VarKind::Frozen ? "const" : "normal")},
            {"value", varValueToJson(info->currentValue)},
        };
    }
    for (const auto& [key, fn] : globalEnv->functions) {
        if (!fn) continue;
        if (key.varName.rfind("__", 0) == 0) continue;
        nlohmann::json params = nlohmann::json::array();
        for (const auto& p : fn->parameters) {
            params.push_back({
                {"name", p.name},
                {"type", p.type},
            });
        }
        out["functions"][key.varName] = {
            {"instance", key.instanceID},
            {"arity", fn->parameters.size()},
            {"params", params},
            {"async", fn->isAsync},
        };
    }
    for (const auto& instance : componentInstances) {
        nlohmann::json locals = nlohmann::json::object();
        for (const auto& [publicName, loweredName] : instance.locals) {
            nlohmann::json local = {{"lowered", loweredName}};
            auto localEnv = instance.environment ? instance.environment : globalEnv;
            JTML::CompositeKey scopedLoweredKey{localEnv->instanceID, loweredName};
            JTML::CompositeKey globalLoweredKey{globalEnv->instanceID, loweredName};
            std::shared_ptr<JTML::Environment::VarInfo> varInfo;
            auto scopedVarIt = localEnv->variables.find(scopedLoweredKey);
            if (scopedVarIt != localEnv->variables.end()) {
                varInfo = scopedVarIt->second;
            } else {
                auto globalVarIt = globalEnv->variables.find(globalLoweredKey);
                if (globalVarIt != globalEnv->variables.end()) {
                    varInfo = globalVarIt->second;
                }
            }
            if (varInfo) {
                local["kind"] = varInfo->kind == JTML::VarKind::Derived ? "derived" :
                                (varInfo->kind == JTML::VarKind::Frozen ? "const" : "normal");
                local["value"] = varValueToJson(varInfo->currentValue);
            }
            std::shared_ptr<JTML::Function> fnInfo;
            auto scopedFnIt = localEnv->functions.find(scopedLoweredKey);
            if (scopedFnIt != localEnv->functions.end()) {
                fnInfo = scopedFnIt->second;
            } else {
                auto globalFnIt = globalEnv->functions.find(globalLoweredKey);
                if (globalFnIt != globalEnv->functions.end()) {
                    fnInfo = globalFnIt->second;
                }
            }
            if (fnInfo) {
                local["function"] = true;
                local["arity"] = fnInfo->parameters.size();
            }
            locals[publicName] = local;
        }
        const std::string renderedHtml = renderComponent(instance);
        const bool renderedHtmlSupported = !renderedHtml.empty();
        out["components"].push_back({
            {"moduleId", moduleIdToJson(instance.moduleId)},
            {"definitionModule", moduleIdToJson(instance.definitionModule)},
            {"id", instance.id},
            {"component", instance.component},
            {"role", instance.role},
            {"instance", instance.instanceID},
            {"runtimeReady", instance.runtimeReady},
            {"sourceLine", instance.sourceLine},
            {"params", instance.params},
            {"locals", locals},
            {"slotPlan", bodyPlanToJson(instance.slotPlan)},
            {"renderedHtmlSupported", renderedHtmlSupported},
            {"renderedHtml", renderedHtml},
            {"runtime", {
                {"mode", "semantic-instance"},
                {"moduleId", moduleIdToJson(instance.moduleId)},
                {"definitionModule", moduleIdToJson(instance.definitionModule)},
                {"ownsEnvironment", true},
                {"bodyPlanActionExecution", true},
                {"bodyPlanTemplateRendering", renderedHtmlSupported},
                {"renderedHtmlSupported", renderedHtmlSupported},
                {"ready", instance.runtimeReady},
                {"environmentId", instance.environment ? instance.environment->instanceID : instance.instanceID},
                {"definition", instance.component},
                {"actions", stringVectorToJson(instance.actionNames)},
                {"state", stringVectorToJson(instance.stateNames)},
                {"derived", stringVectorToJson(instance.derivedNames)},
                {"effects", stringVectorToJson(instance.effectNames)},
                {"slotPlan", bodyPlanToJson(instance.slotPlan)},
                {"renderedHtml", renderedHtml},
            }},
        });
    }
    return out.dump();
}

std::string Interpreter::getComponentsJSON() {
    nlohmann::json out = nlohmann::json::array();
    if (!globalEnv) return out.dump();
    const auto state = nlohmann::json::parse(getStateJSON());
    if (state.contains("components")) return state["components"].dump();
    return out.dump();
}

std::string Interpreter::getComponentDefinitionsJSON() {
    auto moduleIdToJson = [](jtml::SemanticModuleId id) {
        return id == jtml::InvalidSemanticModuleId ? nlohmann::json(nullptr) : nlohmann::json(id);
    };
    auto componentBodyPlanToJson = [](const auto& bodyPlan) {
        nlohmann::json out = nlohmann::json::array();
        for (const auto& node : bodyPlan) {
            out.push_back({
                {"definitionModule", node.definitionModule == jtml::InvalidSemanticModuleId
                    ? nlohmann::json(nullptr)
                    : nlohmann::json(node.definitionModule)},
                {"sourceLine", node.sourceLine},
                {"indent", node.indent},
                {"parentIndex", node.parentIndex},
                {"childIndices", node.childIndices},
                {"kind", node.kind},
                {"head", node.head},
                {"name", node.name},
                {"text", node.text},
                {"operator", node.operatorToken},
                {"expression", node.expression},
                {"keyExpression", node.keyExpression},
                {"reads", node.reads},
                {"writes", node.writes},
                {"renderRoot", node.renderRoot},
            });
        }
        return out;
    };

    nlohmann::json out = nlohmann::json::array();
    for (const auto& def : componentDefinitions) {
        const bool hasRenderableTemplate = def.rootTemplateNodeCount > 0;
        out.push_back({
            {"moduleId", moduleIdToJson(def.moduleId)},
            {"name", def.name},
            {"sourceLine", def.sourceLine},
            {"params", def.params},
            {"emits", def.emits},
            {"emitArity", def.emitArity},
            {"emitPayloads", def.emitPayloads},
            {"emitPayloadTypes", def.emitPayloadTypes},
            {"body", def.body},
            {"bodyPlan", componentBodyPlanToJson(def.bodyPlan)},
            {"localState", stringVectorToJson(def.localState)},
            {"localDerived", stringVectorToJson(def.localDerived)},
            {"localActions", stringVectorToJson(def.localActions)},
            {"localEffects", stringVectorToJson(def.localEffects)},
            {"eventBindings", stringVectorToJson(def.eventBindings)},
            {"hasSlot", def.hasSlot},
            {"bodyNodeCount", def.bodyNodeCount},
            {"rootTemplateNodeCount", def.rootTemplateNodeCount},
            {"slotCount", def.slotCount},
            {"runtimePlan", {
                {"mode", "semantic-instance"},
                {"moduleId", moduleIdToJson(def.moduleId)},
                {"ownsEnvironment", true},
                {"bodyPlanActionExecution", true},
                {"bodyPlanTemplateRendering", hasRenderableTemplate},
                {"renderedHtmlSupported", hasRenderableTemplate},
                {"actions", stringVectorToJson(def.localActions)},
                {"emits", stringVectorToJson(def.emits)},
                {"emitArity", def.emitArity},
                {"emitPayloads", def.emitPayloads},
                {"emitPayloadTypes", def.emitPayloadTypes},
                {"state", stringVectorToJson(def.localState)},
                {"derived", stringVectorToJson(def.localDerived)},
                {"effects", stringVectorToJson(def.localEffects)},
                {"bodyPlan", componentBodyPlanToJson(def.bodyPlan)},
                {"bodyNodeCount", def.bodyNodeCount},
                {"rootTemplateNodeCount", def.rootTemplateNodeCount},
                {"slotCount", def.slotCount},
                {"hasSlot", def.hasSlot},
            }},
        });
    }
    return out.dump();
}

bool Interpreter::dispatchEvent(const std::string& elementId,
                                const std::string& eventType,
                                const nlohmann::json& args,
                                std::string& updatedBindingsJSON,
                                std::string& errorOut) {
    if (!globalEnv) {
        errorOut = "No active program";
        return false;
    }
    auto it = globalEnv->bindings.find(elementId);
    if (it == globalEnv->bindings.end()) {
        errorOut = "No bindings for element: " + elementId;
        return false;
    }
    for (const auto& binding : it->second) {
        if (binding.bindingType != "attribute_event") continue;
        if (binding.attribute != eventType) continue;

        try {
            if (eventCarriesBrowserValue(eventType)) {
                if (!args.is_array() || args.size() < 2) {
                    errorOut = eventType + " requires a browser value argument";
                    return false;
                }
                if (binding.expression->getExprType() != ExpressionStatementNodeType::FunctionCall) {
                    errorOut = eventType + " handler must be a function call";
                    return false;
                }
                auto funcCall = std::static_pointer_cast<FunctionCallExpressionStatementNode>(binding.expression);
                auto eventEnv = environmentForInstance(binding.varName.instanceID);
                if (!eventEnv) eventEnv = globalEnv;
                JTML::CompositeKey funcKey{eventEnv->instanceID, funcCall->functionName};
                auto func = eventEnv->getFunction(funcKey);
                if (!func) {
                    errorOut = "Function not found: " + funcCall->functionName;
                    return false;
                }
                std::vector<std::shared_ptr<JTML::VarValue>> funcArgs;
                funcArgs.push_back(jsonToVarValue(args[1], eventEnv));
                executeFunction(func, funcArgs, nullptr);
            } else {
                // Events without browser-supplied values evaluate the bound
                // expression, which for the current grammar is a function call.
                auto eventEnv = environmentForInstance(binding.varName.instanceID);
                if (!eventEnv) eventEnv = globalEnv;
                evaluateExpression(binding.expression.get(), eventEnv);
            }
            recalcAllDirty();
            updatedBindingsJSON = getBindingsJSON();
            return true;
        } catch (const std::exception& e) {
            errorOut = e.what();
            return false;
        }
    }
    errorOut = "No " + eventType + " binding for element " + elementId;
    return false;
}

bool Interpreter::dispatchComponentAction(const std::string& componentId,
                                          const std::string& actionName,
                                          const nlohmann::json& args,
                                          std::string& updatedBindingsJSON,
                                          std::string& errorOut) {
    if (!globalEnv) {
        errorOut = "No active program";
        return false;
    }
    if (componentId.empty()) {
        errorOut = "Missing componentId";
        return false;
    }
    if (actionName.empty()) {
        errorOut = "Missing component action";
        return false;
    }

    auto* instance = findComponentInstance(componentId);
    if (!instance) {
        errorOut = "Component instance not found: " + componentId;
        return false;
    }
    if (!instance->environment) {
        errorOut = "Component instance has no runtime environment: " + componentId;
        return false;
    }

    const auto* definition = findComponentDefinition(
        instance->component, instance->definitionModule, instance->moduleId);

    auto dispatchEmittedEvent = [&](const std::string& emittedName,
                                    const nlohmann::json& emittedArgs) -> bool {
        const auto handlerIt = instance->eventHandlers.find(emittedName);
        if (handlerIt == instance->eventHandlers.end() ||
            handlerIt->second.name.empty() ||
            instance->parentId.empty()) {
            return false;
        }
        if (definition && !definition->emits.empty() &&
            std::find(definition->emits.begin(), definition->emits.end(), emittedName) ==
                definition->emits.end()) {
            errorOut = "Component '" + instance->component + "' does not emit event '" +
                       emittedName + "'";
            return false;
        }
        if (definition) {
            const auto arityIt = definition->emitArity.find(emittedName);
            if (arityIt != definition->emitArity.end()) {
                const int got = emittedArgs.is_array() ? static_cast<int>(emittedArgs.size()) : 0;
                if (got != arityIt->second) {
                    errorOut = "Component event '" + emittedName + "' expected " +
                               std::to_string(arityIt->second) + " payload argument(s), got " +
                               std::to_string(got);
                    return false;
                }
            }
            const auto typeIt = definition->emitPayloadTypes.find(emittedName);
            if (typeIt != definition->emitPayloadTypes.end()) {
                const int got = emittedArgs.is_array() ? static_cast<int>(emittedArgs.size()) : 0;
                for (int i = 0; i < static_cast<int>(typeIt->second.size()); ++i) {
                    const nlohmann::json value = i < got ? emittedArgs.at(i) : nlohmann::json(nullptr);
                    if (!emittedPayloadTypeMatches(value, typeIt->second[i])) {
                        std::string payloadName = std::to_string(i);
                        const auto payloadIt = definition->emitPayloads.find(emittedName);
                        if (payloadIt != definition->emitPayloads.end() &&
                            i < static_cast<int>(payloadIt->second.size()) &&
                            !payloadIt->second[static_cast<size_t>(i)].empty()) {
                            payloadName = payloadIt->second[static_cast<size_t>(i)];
                        }
                        errorOut = "Component '" + definition->name + "' event '" + emittedName +
                                   "' payload '" + payloadName + "' expected type '" +
                                   typeIt->second[i] + "'";
                        if (definition->sourceLine > 0) {
                            errorOut += " at component definition line " +
                                        std::to_string(definition->sourceLine);
                        }
                        return false;
                    }
                }
            }
        }
        nlohmann::json forwardedArgs = nlohmann::json::array();
        if (handlerIt->second.args.is_array()) {
            for (const auto& arg : handlerIt->second.args) {
                forwardedArgs.push_back(arg);
            }
        }
        if (emittedArgs.is_array()) {
            for (const auto& arg : emittedArgs) {
                forwardedArgs.push_back(arg);
            }
        }
        return dispatchComponentAction(
            instance->parentId,
            handlerIt->second.name,
            forwardedArgs,
            updatedBindingsJSON,
            errorOut);
    };

    auto hasDeclaredAction = [&](const std::string& name) {
        if (instance->actionNames.empty()) return true;
        return std::find(instance->actionNames.begin(), instance->actionNames.end(), name) != instance->actionNames.end();
    };
    if (!hasDeclaredAction(actionName)) {
        if (dispatchEmittedEvent(actionName, args)) return true;
        if (!errorOut.empty()) return false;
        nlohmann::json available = stringVectorToJson(instance->actionNames);
        errorOut = "Component action not declared on " + componentId + "." + actionName +
                   "; available actions: " + available.dump();
        return false;
    }

    auto bodyPlanChildren = [](const RuntimeComponentDefinition& def,
                               const BodyPlanNode& node) {
        std::vector<const BodyPlanNode*> children;
        for (int index : node.childIndices) {
            if (index < 0 || static_cast<size_t>(index) >= def.bodyPlan.size()) continue;
            children.push_back(&def.bodyPlan[static_cast<size_t>(index)]);
        }
        return children;
    };
    auto bodyPlanElseSibling = [](const RuntimeComponentDefinition& def,
                                  const BodyPlanNode& node) -> const BodyPlanNode* {
        auto it = std::find_if(def.bodyPlan.begin(), def.bodyPlan.end(),
            [&](const BodyPlanNode& candidate) { return &candidate == &node; });
        if (it == def.bodyPlan.end()) return nullptr;
        for (++it; it != def.bodyPlan.end(); ++it) {
            if (it->parentIndex == node.parentIndex) {
                return it->name == "else" ? &(*it) : nullptr;
            }
        }
        return nullptr;
    };
    auto words = [](const std::string& value) {
        std::vector<std::string> out;
        std::istringstream input(value);
        std::string word;
        while (input >> word) out.push_back(word);
        return out;
    };
    auto setOrDefine = [](const std::shared_ptr<JTML::Environment>& env,
                          const std::string& name,
                          const std::shared_ptr<JTML::VarValue>& value) {
        JTML::CompositeKey key{env->instanceID, name};
        if (env->hasVariable(key)) env->setVariable(key, value);
        else env->defineVariable(key, value, JTML::VarKind::Normal);
    };
    auto runtimeLocalName = [&](const std::string& publicName) {
        auto localIt = instance->locals.find(publicName);
        return localIt == instance->locals.end() ? publicName : localIt->second;
    };
    auto parseExpressionValue = [&](const std::string& expr) -> std::shared_ptr<JTML::VarValue> {
        const std::string source = "jtml 2\nlet __jtml_expr = " + expr + "\n";
        const std::string lowered = jtml::normalizeSourceSyntax(source, jtml::SyntaxMode::Friendly);
        Lexer lexer(lowered);
        auto tokens = lexer.tokenize();
        if (!lexer.getErrors().empty()) {
            throw std::runtime_error("Could not tokenize component body-plan expression: " + expr);
        }
        Parser parser(std::move(tokens));
        auto parsed = parser.parseProgram();
        if (!parser.getErrors().empty() || parsed.empty() ||
            parsed.front()->getType() != ASTNodeType::DefineStatement) {
            throw std::runtime_error("Could not parse component body-plan expression: " + expr);
        }
        const auto& define = static_cast<const DefineStatementNode&>(*parsed.front());
        return evaluateExpression(define.expression.get(), instance->environment);
    };
    auto splitCallArgs = [](const std::string& source) {
        std::vector<std::string> out;
        std::string current;
        char quote = '\0';
        int depth = 0;
        auto trim = [](std::string value) {
            value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            }));
            value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) {
                return !std::isspace(ch);
            }).base(), value.end());
            return value;
        };
        for (size_t i = 0; i < source.size(); ++i) {
            const char ch = source[i];
            if (quote != '\0') {
                current.push_back(ch);
                if (ch == '\\' && i + 1 < source.size()) {
                    current.push_back(source[++i]);
                } else if (ch == quote) {
                    quote = '\0';
                }
                continue;
            }
            if (ch == '"' || ch == '\'') {
                quote = ch;
                current.push_back(ch);
                continue;
            }
            if (ch == '(' || ch == '[' || ch == '{') ++depth;
            if ((ch == ')' || ch == ']' || ch == '}') && depth > 0) --depth;
            if (ch == ',' && depth == 0) {
                auto item = trim(current);
                if (!item.empty()) out.push_back(item);
                current.clear();
                continue;
            }
            current.push_back(ch);
        }
        auto item = trim(current);
        if (!item.empty()) out.push_back(item);
        return out;
    };
    std::string bodyPlanError;
    auto runBodyPlanAction = [&]() -> bool {
        if (!definition) return false;
        auto actionIt = std::find_if(definition->bodyPlan.begin(), definition->bodyPlan.end(),
            [&](const BodyPlanNode& node) {
                return node.kind == "action" && node.name == actionName;
            });
        if (actionIt == definition->bodyPlan.end()) return false;
        auto failBodyPlan = [&](const BodyPlanNode* node,
                                const std::string& reason) -> bool {
            if (!bodyPlanError.empty()) return false;
            bodyPlanError = "Unsupported component body-plan action '" +
                            instance->component + "." + actionName + "'";
            if (definition->sourceLine > 0) {
                bodyPlanError += " from component definition line " +
                                 std::to_string(definition->sourceLine);
            }
            if (node && node->sourceLine > 0) {
                bodyPlanError += ", body line " +
                                 std::to_string(node->sourceLine);
            }
            bodyPlanError += ": " + reason;
            if (node && !node->text.empty()) {
                bodyPlanError += " near `" + node->text + "`";
            }
            return false;
        };

        const auto actionWords = words(actionIt->text);
        std::map<std::string, std::shared_ptr<JTML::VarValue>> previousActionParams;
        std::set<std::string> hadPreviousActionParams;
        for (size_t i = 2; i < actionWords.size(); ++i) {
            const size_t argIndex = i - 2;
            JTML::CompositeKey key{instance->environment->instanceID, actionWords[i]};
            if (instance->environment->hasVariable(key)) {
                hadPreviousActionParams.insert(actionWords[i]);
                previousActionParams[actionWords[i]] = instance->environment->getVariable(key);
            }
            auto value = (args.is_array() && argIndex < args.size())
                ? jsonToVarValue(args[argIndex], instance->environment)
                : std::make_shared<JTML::VarValue>(std::string{});
            setOrDefine(instance->environment, actionWords[i], value);
        }

        auto applyAssignment = [&](const BodyPlanNode& node) -> bool {
            if (!node.childIndices.empty()) {
                return failBodyPlan(&node, "assignment nodes cannot have child statements");
            }
            const std::string targetName = runtimeLocalName(node.name);
            const std::string op = node.operatorToken.empty() ? "=" : node.operatorToken;
            if (op != "=" && op != "+=" && op != "-=" && op != "*=" && op != "/=" && op != "%=") {
                return failBodyPlan(&node, "unsupported assignment operator `" + op + "`");
            }
            const auto next = parseExpressionValue(node.expression);
            JTML::CompositeKey key{instance->environment->instanceID, targetName};
            if (op == "=") {
                setOrDefine(instance->environment, targetName, next);
            } else if (op == "+=") {
                std::shared_ptr<JTML::VarValue> current;
                try {
                    current = instance->environment->getVariable(key);
                } catch (...) {
                    current = std::make_shared<JTML::VarValue>(std::string{});
                }
                if (current && current->isNumber() && next && next->isNumber()) {
                    setOrDefine(instance->environment, targetName,
                                std::make_shared<JTML::VarValue>(
                                    current->getNumber() + next->getNumber()));
                } else {
                    setOrDefine(instance->environment, targetName,
                                std::make_shared<JTML::VarValue>(
                                    (current ? current->toString() : std::string{}) +
                                    (next ? next->toString() : std::string{})));
                }
            } else if (op == "-=" || op == "*=" || op == "/=" || op == "%=") {
                auto current = instance->environment->getVariable(key);
                const double left = current && current->isNumber()
                    ? current->getNumber()
                    : std::stod(current ? current->toString() : "0");
                const double right = next && next->isNumber()
                    ? next->getNumber()
                    : std::stod(next ? next->toString() : "0");
                double result = left;
                if (op == "-=") result = left - right;
                else if (op == "*=") result = left * right;
                else if (op == "/=") result = left / right;
                else if (op == "%=") result = std::fmod(left, right);
                setOrDefine(instance->environment, targetName,
                            std::make_shared<JTML::VarValue>(result));
            }
            return true;
        };
        std::function<bool(const std::vector<const BodyPlanNode*>&)> runStatements;
        runStatements = [&](const std::vector<const BodyPlanNode*>& nodes) -> bool {
            for (const auto* node : nodes) {
                if (!node) continue;
                if (node->name == "else") continue;
                if (node->kind == "assignment") {
                    if (!applyAssignment(*node)) return false;
                    continue;
                }
                if (node->kind == "call" && !node->name.empty()) {
                    if (!node->childIndices.empty()) {
                        return failBodyPlan(node, "call nodes cannot have child statements");
                    }
                    nlohmann::json callArgs = nlohmann::json::array();
                    std::vector<std::shared_ptr<JTML::VarValue>> callValues;
                    for (const auto& argExpr : splitCallArgs(node->expression)) {
                        auto value = parseExpressionValue(argExpr);
                        callValues.push_back(value);
                        callArgs.push_back(varValueToJson(value));
                    }
                    auto nestedAction = std::find_if(definition->bodyPlan.begin(), definition->bodyPlan.end(),
                        [&](const BodyPlanNode& candidate) {
                            return candidate.kind == "action" && candidate.name == node->name;
                        });
                    if (nestedAction == definition->bodyPlan.end()) {
                        if (dispatchEmittedEvent(node->name, callArgs)) continue;
                        return failBodyPlan(node, "call target is not a local action or declared emitted event");
                    }
                    const auto nestedWords = words(nestedAction->text);
                    std::map<std::string, std::shared_ptr<JTML::VarValue>> previousParams;
                    std::set<std::string> hadPreviousParams;
                    for (size_t i = 2; i < nestedWords.size(); ++i) {
                        const size_t argIndex = i - 2;
                        JTML::CompositeKey key{instance->environment->instanceID, nestedWords[i]};
                        if (instance->environment->hasVariable(key)) {
                            hadPreviousParams.insert(nestedWords[i]);
                            previousParams[nestedWords[i]] = instance->environment->getVariable(key);
                        }
                        auto value = argIndex < callValues.size()
                            ? callValues[argIndex]
                            : std::make_shared<JTML::VarValue>(std::string{});
                        setOrDefine(instance->environment, nestedWords[i], value);
                    }
                    const bool nestedOk = runStatements(bodyPlanChildren(*definition, *nestedAction));
                    for (size_t i = 2; i < nestedWords.size(); ++i) {
                        const auto& param = nestedWords[i];
                        JTML::CompositeKey key{instance->environment->instanceID, param};
                        if (hadPreviousParams.count(param)) {
                            setOrDefine(instance->environment, param, previousParams[param]);
                        } else {
                            instance->environment->variables.erase(key);
                        }
                    }
                    if (!nestedOk) return false;
                    continue;
                }
                if (node->kind == "template" && node->name == "if") {
                    const std::string condition = node->expression.empty()
                        ? ([&] {
                            const auto conditionWords = words(node->text);
                            return conditionWords.size() > 1
                                ? node->text.substr(node->text.find(conditionWords[1]))
                                : std::string{};
                        })()
                        : node->expression;
                    const auto value = parseExpressionValue(condition);
                    if (value && isTruthy(value->toString())) {
                        if (!runStatements(bodyPlanChildren(*definition, *node))) return false;
                    } else if (const auto* elseNode = bodyPlanElseSibling(*definition, *node)) {
                        if (!runStatements(bodyPlanChildren(*definition, *elseNode))) return false;
                    }
                    continue;
                }
                if (node->kind == "template" && node->name == "for") {
                    const std::string raw = node->expression.empty()
                        ? ([&] {
                            const auto loopWords = words(node->text);
                            return loopWords.size() > 1
                                ? node->text.substr(node->text.find(loopWords[1]))
                                : std::string{};
                        })()
                        : node->expression;
                    auto trim = [](const std::string& value) {
                        const auto start = value.find_first_not_of(" \t\r\n");
                        if (start == std::string::npos) return std::string{};
                        const auto end = value.find_last_not_of(" \t\r\n");
                        return value.substr(start, end - start + 1);
                    };
                    const auto inPos = raw.find(" in ");
                    if (inPos == std::string::npos) {
                        return failBodyPlan(node, "for loop must use `item in collection`");
                    }
                    const std::string itemName = trim(raw.substr(0, inPos));
                    const std::string listExpr = trim(raw.substr(inPos + 4));
                    if (itemName.empty() || listExpr.empty()) {
                        return failBodyPlan(node, "for loop is missing an iterator or collection expression");
                    }
                    const auto value = parseExpressionValue(listExpr);
                    if (!value || (!value->isArray() && !value->isString())) {
                        return failBodyPlan(node, "for loop action bodies currently support arrays and strings");
                    }
                    JTML::CompositeKey iteratorKey{instance->environment->instanceID, itemName};
                    std::shared_ptr<JTML::VarValue> previous;
                    const bool hadPrevious = instance->environment->hasVariable(iteratorKey);
                    if (hadPrevious) {
                        previous = instance->environment->getVariable(iteratorKey);
                    }
                    if (value->isArray()) {
                        for (const auto& item : value->getArray()->getArrayData()) {
                            setOrDefine(instance->environment, itemName, item);
                            if (!runStatements(bodyPlanChildren(*definition, *node))) return false;
                        }
                    } else {
                        for (const auto& c : value->getString()) {
                            setOrDefine(instance->environment, itemName,
                                        std::make_shared<JTML::VarValue>(std::string(1, c)));
                            if (!runStatements(bodyPlanChildren(*definition, *node))) return false;
                        }
                    }
                    if (hadPrevious) {
                        setOrDefine(instance->environment, itemName, previous);
                    } else {
                        instance->environment->variables.erase(iteratorKey);
                    }
                    continue;
                }
                if (node->kind == "template" && node->name == "while") {
                    const std::string condition = node->expression.empty()
                        ? ([&] {
                            const auto conditionWords = words(node->text);
                            return conditionWords.size() > 1
                                ? node->text.substr(node->text.find(conditionWords[1]))
                                : std::string{};
                        })()
                        : node->expression;
                    int guard = 0;
                    while (true) {
                        const auto value = parseExpressionValue(condition);
                        if (!value || !isTruthy(value->toString())) break;
                        if (++guard > 10000) {
                            return failBodyPlan(node, "while loop exceeded the direct-execution guard");
                        }
                        if (!runStatements(bodyPlanChildren(*definition, *node))) return false;
                    }
                    continue;
                }
                return failBodyPlan(node, "unsupported statement kind `" + node->kind + "`");
            }
            return true;
        };
        const bool ok = runStatements(bodyPlanChildren(*definition, *actionIt));
        for (size_t i = 2; i < actionWords.size(); ++i) {
            const auto& param = actionWords[i];
            JTML::CompositeKey key{instance->environment->instanceID, param};
            if (hadPreviousActionParams.count(param)) {
                setOrDefine(instance->environment, param, previousActionParams[param]);
            } else {
                instance->environment->variables.erase(key);
            }
        }
        if (!ok) return false;
        return true;
    };

    bool bodyPlanRan = false;
    try {
        bodyPlanRan = runBodyPlanAction();
    } catch (const std::exception& e) {
        bodyPlanError = e.what();
    }
    if (bodyPlanRan) {
        try {
            recalcAllDirty();
            updatedBindingsJSON = getBindingsJSON();
            return true;
        } catch (const std::exception& e) {
            errorOut = e.what();
            return false;
        }
    }

    if (dispatchEmittedEvent(actionName, args)) return true;
    if (!errorOut.empty()) return false;

    std::string runtimeActionName = actionName;
    auto loweredIt = instance->locals.find(actionName);
    if (loweredIt != instance->locals.end()) runtimeActionName = loweredIt->second;

    JTML::CompositeKey actionKey{instance->environment->instanceID, actionName};
    std::shared_ptr<JTMLInterpreter::Function> fn;
    try {
        fn = instance->environment->getFunction(actionKey);
    } catch (...) {
        fn = nullptr;
    }
    if (!fn && runtimeActionName != actionName) {
        actionKey = JTML::CompositeKey{instance->environment->instanceID, runtimeActionName};
        try {
            fn = instance->environment->getFunction(actionKey);
        } catch (...) {
            fn = nullptr;
        }
    }

    if (!fn) {
        nlohmann::json available = stringVectorToJson(instance->actionNames);
        errorOut = bodyPlanError.empty()
            ? "Component action not found: " + componentId + "." + actionName +
              "; available actions: " + available.dump()
            : bodyPlanError;
        return false;
    }

    try {
        std::vector<std::shared_ptr<JTML::VarValue>> funcArgs;
        if (args.is_array()) {
            for (const auto& arg : args) funcArgs.push_back(jsonToVarValue(arg, instance->environment));
        }
        executeFunction(fn, funcArgs, nullptr);
        recalcAllDirty();
        updatedBindingsJSON = getBindingsJSON();
        return true;
    } catch (const std::exception& e) {
        errorOut = bodyPlanError.empty()
            ? e.what()
            : bodyPlanError + "; compatibility fallback error: " + e.what();
        return false;
    }
}

// In jtml_interpreter.cpp

void Interpreter::populateBindings(websocketpp::connection_hdl hdl) {
    try {
        // Debug log: Starting the populateBindings process
        std::cout << "[DEBUG] Starting populateBindings for WebSocket connection.\n";

        if (!globalEnv) {
            std::cerr << "[ERROR] Global environment is not initialized.\n";
            throw std::runtime_error("Global environment is not initialized.");
        }

        nlohmann::json message;
        message["type"] = "populateBindings";
        message["bindings"] = nlohmann::json::parse(getBindingsJSON());

        std::string messageStr = message.dump();
        std::cout << "[DEBUG] Serialized message: " << messageStr << "\n";

        wsServer->sendMessage(hdl, messageStr);

        // Debug log: Message sent
        std::cout << "[DEBUG] Sent populateBindings to frontend. Message size: " << messageStr.size() << " bytes\n";
    } catch (const std::exception& e) {
        // Log the error and optionally send an error message to the frontend
        std::cerr << "[ERROR] Failed to populate bindings: " << e.what() << "\n";
        wsServer->sendMessage(hdl, R"({"type": "error", "message": "Failed to populate bindings"})");
    }
}


struct ReturnException : public std::exception {
    std::shared_ptr<JTML::VarValue> value;

    ReturnException(std::shared_ptr<JTML::VarValue> val) : value(val) {}

    const char* what() const noexcept override {
        return "ReturnException";
    }
};

struct BreakException : public std::exception {
    std::shared_ptr<JTML::VarValue> value;

    BreakException(std::shared_ptr<JTML::VarValue> val) : value(val) {}

    const char* what() const noexcept override {
        return "Unexpected break outside loop";
    }
};

struct ContinueException : public std::exception {
    std::shared_ptr<JTML::VarValue> value;

    ContinueException(std::shared_ptr<JTML::VarValue> val) : value(val) {}

    const char* what() const noexcept override {
        return "Unexpected break outside loop";
    }
};

void Interpreter::handleFrontendMessage(const std::string& msg, websocketpp::connection_hdl hdl) {
    try {
        // Parse the incoming message as JSON
        auto parsedMessage = nlohmann::json::parse(msg);

        // Validate message structure
        if (!parsedMessage.contains("type")) {
            throw std::runtime_error("Message missing 'type' field.");
        }

        std::string type = parsedMessage["type"].get<std::string>();

        if (type == "event") {
            // Extract event details
            std::string elementIdStr = parsedMessage["elementId"].get<std::string>();
            std::string eventType = parsedMessage["eventType"].get<std::string>();
            std::vector<nlohmann::json> args = parsedMessage.value("args", std::vector<nlohmann::json>());

            std::cout << "[DEBUG] Event received: ElementID=" << elementIdStr
                      << ", EventType=" << eventType << "\n";



            // Find the binding for the given elementId and attribute (eventType)
            JTML::CompositeKey elementVarKey = {globalEnv->instanceID, elementIdStr};
            auto bindingsIt = globalEnv->bindings.find(elementIdStr);
            if (bindingsIt != globalEnv->bindings.end()) {
                bool bindingFound = false;

                for (const auto& binding : bindingsIt->second) {

                    if (binding.elementId == elementIdStr &&
                        binding.bindingType == "attribute_event" &&
                        binding.attribute == eventType) {
                        bindingFound = true;

                        std::cout << "[DEBUG] Found binding: ElementID=" << elementIdStr
                                  << ", Attribute=" << eventType << "\n";


                    if (eventCarriesBrowserValue(eventType)) {
                        // Value-bearing browser events pass their value as the sole argument.
                        std::vector<std::shared_ptr<JTML::VarValue>> functionArgs;
                        const auto& eventArgs = parsedMessage["args"];
                        if (!eventArgs.is_array() || eventArgs.size() < 2) {
                            throw std::runtime_error(eventType + " event is missing its browser value.");
                        }
                        // Ensure the expression is a function call
                        if (binding.expression->getExprType() != ExpressionStatementNodeType::FunctionCall) {
                            std::cerr << "[ERROR] " << eventType << " binding expression is not a function call.\n";
                            renderer->sendError(eventType + " binding expression is not a function call.");
                            continue;
                        }

                        // Cast to FunctionCallExpressionStatementNode to access functionName
                        auto funcCallExpr = std::static_pointer_cast<FunctionCallExpressionStatementNode>(binding.expression);
                        std::string functionName = funcCallExpr->functionName;

                        auto eventEnv = environmentForInstance(binding.varName.instanceID);
                        if (!eventEnv) eventEnv = globalEnv;
                        functionArgs.push_back(jsonToVarValue(eventArgs[1], eventEnv));

                        // Retrieve the function from the environment
                        JTML::CompositeKey funcKey{ eventEnv->instanceID, functionName };
                        auto func = eventEnv->getFunction(funcKey);
                        if (!func) {
                            std::cerr << "[ERROR] Function '" << functionName << "' not found.\n";
                            renderer->sendError("Function '" + functionName + "' not found.");
                            continue;
                        }

                        // Execute the function with args
                        auto result = executeFunction(func, functionArgs, nullptr);

                        // Recalculate dirty bindings and push updated state to all clients
                        recalcAllDirty();
                        try {
                            nlohmann::json msg;
                            msg["type"] = "populateBindings";
                            msg["bindings"] = nlohmann::json::parse(getBindingsJSON());
                            wsServer->broadcastMessage(msg.dump());
                        } catch (...) {}
                        break;
                    }

                    // Evaluate the derived expression (assumes it's a function call or similar)
                    auto eventEnv = environmentForInstance(binding.varName.instanceID);
                    if (!eventEnv) eventEnv = globalEnv;
                    evaluateExpression(binding.expression.get(), eventEnv);

                    recalcAllDirty();
                    try {
                        nlohmann::json msg;
                        msg["type"] = "populateBindings";
                        msg["bindings"] = nlohmann::json::parse(getBindingsJSON());
                        wsServer->broadcastMessage(msg.dump());
                    } catch (...) {}
                    break;
                }

            }

            if (!bindingFound) {
                std::cerr << "[WARNING] No binding found for ElementID=" << elementIdStr
                            << ", EventType=" << eventType << "\n";
                renderer->sendError("No binding found for the triggered event.");
            }
        } else {
            std::cerr << "[WARNING] No bindings registered for event type: " << eventType << "\n";
            renderer->sendError("No bindings registered for event type: " + eventType + " and element name: " + elementIdStr);
        }
    } else {
            std::cerr << "[WARNING] Unrecognized message type: " << type << "\n";
            renderer->sendError("Unrecognized message type: " + type);
        }
    }
    catch (const nlohmann::json::exception& e) {
        std::cerr << "[ERROR] JSON parsing failed: " << e.what() << "\n";
        renderer->sendError("Invalid JSON message.");
    }
    catch (const std::exception& e) {
        std::cerr << "[ERROR] handleFrontendMessage exception: " << e.what() << "\n";
        renderer->sendError(e.what());
    }
}





std::shared_ptr<JTML::Environment> Interpreter::getCurrentEnvironment() const {
    return currentEnv;
}


void Interpreter::interpret(const JtmlElementNode& root) {
    // Recursively process the JtmlElementNode
    std::cout << "Interpreting element: " << root.tagName << "\n";

    // Process attributes
    for (const auto& attr : root.attributes) {
        std::cout << "  Attribute: " << attr.key << " = " << attr.value->toString() << "\n";
    }

    // Process child nodes
    for (const auto& child : root.content) {
        interpretNode(*child);
    }
}

// Interpret a single AST node (e.g., JtmlElementNode)
void Interpreter::interpret(const ASTNode& node) {
    interpretNode(node);
}

// Interpret a vector of AST nodes (e.g., the entire program)
void Interpreter::interpret(const std::vector<std::unique_ptr<ASTNode>>& program) {
    registerComponentInstances(program);
    for (const auto& node : program) {
            std::cout << " " << node->toString() << "\n";
            interpretNode(*node);

    }
    registerComponentInstances(program);
    // After all statements in a block => recalc
    recalcAllDirty();
}

// Interpret directly from code (string)
void Interpreter::interpret(const std::string& code) {
    try {
        Lexer lexer(jtml::normalizeSourceSyntax(code));
        auto tokens = lexer.tokenize();

        // Check for lexer errors
        const auto& lexErrors = lexer.getErrors();
        if (!lexErrors.empty()) {
            for (const auto& err : lexErrors) {
                handleError(err);
            }
            return;
        }

        Parser parser(std::move(tokens));
        auto program = parser.parseProgram();

        // Optionally, check for parser errors if your Parser provides such functionality

        interpret(program);
    }
    catch (const std::exception& e) {
        handleError(std::string("Interpretation error: ") + e.what());
    }
}

// Interpret a single AST node by delegating to specific methods
void Interpreter::interpretNode(const ASTNode& node) {
    std::cout << "Interpreting node " << node.toString() << "\n";
    try {
        switch (node.getType()) {
            case ASTNodeType::JtmlElement:
                interpretElement(static_cast<const JtmlElementNode&>(node));
                break;
            case ASTNodeType::BlockStatement:
                interpretBlockStatement(static_cast<const BlockStatementNode&>(node));
                break;
            case ASTNodeType::ShowStatement:
                interpretShow(static_cast<const ShowStatementNode&>(node));
                break;
            case ASTNodeType::DefineStatement:
                interpretDefine(static_cast<const DefineStatementNode&>(node));
                break;
            case ASTNodeType::AssignmentStatement:
                interpretAssignment(static_cast<const AssignmentStatementNode&>(node));
                break;
            case ASTNodeType::ExpressionStatement:
                interpretExpression(static_cast<const ExpressionNode&>(node));
                break;
            case ASTNodeType::DeriveStatement:
                interpretDerive(static_cast<const DeriveStatementNode&>(node));
                break;
            case ASTNodeType::UnbindStatement:
                interpretUnbind(static_cast<const UnbindStatementNode&>(node));
                break;
            case ASTNodeType::StoreStatement:
                interpretStore(static_cast<const StoreStatementNode&>(node));
                break;
            case ASTNodeType::IfStatement:
                interpretIf(static_cast<const IfStatementNode&>(node));
                break;
            case ASTNodeType::WhileStatement:
                interpretWhile(static_cast<const WhileStatementNode&>(node));
                break;
            case ASTNodeType::BreakStatement:
                interpretBreak(static_cast<const BreakStatementNode&>(node));
                break;
            case ASTNodeType::ContinueStatement:
                interpretContinue(static_cast<const ContinueStatementNode&>(node));
                break;
            case ASTNodeType::ForStatement:
                interpretFor(static_cast<const ForStatementNode&>(node));
                break;
            case ASTNodeType::TryExceptThen:
                interpretTryExceptThen(static_cast<const TryExceptThenNode&>(node));
                break;
            case ASTNodeType::ThrowStatement:
                interpretThrow(static_cast<const ThrowStatementNode&>(node));
                break;
            case ASTNodeType::ImportStatement:
                interpretImport(static_cast<const ImportStatementNode&>(node));
                break;
            case ASTNodeType::SubscribeStatement:
                interpretSubscribe(static_cast<const SubscribeStatementNode&>(node));
                break;
            case ASTNodeType::UnsubscribeStatement:
                interpretUnsubscribe(static_cast<const UnsubscribeStatementNode&>(node));
                break;
            case ASTNodeType::FunctionDeclaration:
                interpretFunctionDeclaration(static_cast<const FunctionDeclarationNode&>(node));
                break;
            case ASTNodeType::ClassDeclaration:
                interpretClassDeclaration(static_cast<const ClassDeclarationNode&>(node));
                break;
            case ASTNodeType::ReturnStatement:
                std::cout << "ReturnStatement node" << node.toString();
                interpretReturn(static_cast<const ReturnStatementNode&>(node));
                break;
            case ASTNodeType::NoOp:
                break;
            default:
                handleError("Unknown ASTNodeType encountered during interpretation.");
        }
    } catch (const ReturnException&) {
        // Allow ReturnException to propagate
        throw;
    } catch (const BreakException&) {
        throw;
    } catch (const ContinueException&) {
        throw;
    } catch (const std::exception& e) {
        handleError("Node Interpretation Error: " + std::string(e.what()));
    }
}

// ------------------- Interpretation Methods -------------------

void Interpreter::interpretElement(const JtmlElementNode& elem) {
    std::cout << "[DEBUG] Interpreting Element: <" << elem.tagName << ">\n";
    nodeID++;
    if (!attrValueFor(elem, "data-jtml-component-def").empty()) {
        std::cout << "[DEBUG] Registered Component Definition <" << elem.tagName << ">\n";
        return;
    }
    // Create a new environment for this element


    // Evaluate attributes or do any binding registrations (transpile-time binding approach)

    auto previousEnv = currentEnv;
    RuntimeComponentInstance* componentInstance = findComponentInstanceForElement(elem);
    if (componentInstance && componentInstance->environment) {
        currentEnv = componentInstance->environment;
    }

    try {
        interpretElementAttributes(elem);
    } catch (...) {
        currentEnv = previousEnv;
        throw;
    }

    // Component wrappers are a semantic boundary produced by Friendly JTML.
    // Their children may include local state/action declarations before the
    // actual DOM subtree, so execute every child node instead of applying the
    // stricter "HTML element content" allow-list.
    if (isComponentInstanceElement(elem)) {
        try {
            for (auto& child : elem.content) {
                interpretNode(*child);
            }
        } catch (...) {
            currentEnv = previousEnv;
            throw;
        }
        currentEnv = previousEnv;
        std::cout << "[DEBUG] Exiting Component Instance <" << elem.tagName << ">\n";
        return;
    }

    // Now interpret child statements: only show, if, for, while, or nested element
    try {
        for (auto& child : elem.content) {
            switch(child->getType()) {
                case ASTNodeType::ShowStatement:
                    interpretShowElement(static_cast<const ShowStatementNode&>(*child));
                    break;
                case ASTNodeType::IfStatement:
                    interpretIfElement(static_cast<const IfStatementNode&>(*child));
                    break;
                case ASTNodeType::ForStatement:
                    interpretForElement(static_cast<const ForStatementNode&>(*child));
                    break;
                case ASTNodeType::WhileStatement:
                    interpretWhileElement(static_cast<const WhileStatementNode&>(*child));
                    break;
                case ASTNodeType::JtmlElement:
                    interpretElement(static_cast<const JtmlElementNode&>(*child));
                    break;
                default:
                    // not allowed inside an element
                    std::cerr << "[ERROR] Disallowed statement '"
                              << child->toString()
                              << "' inside <" << elem.tagName << ">\n";
                    break;
            }
        }
    } catch (...) {
        currentEnv = previousEnv;
        throw;
    }

    currentEnv = previousEnv;


    std::cout << "[DEBUG] Exiting Element <" << elem.tagName << ">\n";
}

bool Interpreter::isComponentInstanceElement(const JtmlElementNode& elem) const {
    return !attrValueFor(elem, "data-jtml-instance").empty();
}

void Interpreter::collectComponentInstances(const ASTNode& node,
                                            std::vector<RuntimeComponentInstance>& out) {
    if (node.getType() != ASTNodeType::JtmlElement) return;
    const auto& elem = static_cast<const JtmlElementNode&>(node);
    const std::string id = attrValueFor(elem, "data-jtml-instance");
    if (!id.empty()) {
        RuntimeComponentInstance instance;
        instance.moduleId =
            parseSemanticModuleId(attrValueFor(elem, "data-jtml-component-module"));
        instance.definitionModule =
            parseSemanticModuleId(attrValueFor(elem, "data-jtml-component-definition-module"));
        instance.id = id;
        instance.component = attrValueFor(elem, "data-jtml-component");
        instance.role = attrValueFor(elem, "data-jtml-component-role");
        if (instance.role.empty()) instance.role = "component";
        instance.instanceID = parseInstanceId(attrValueFor(elem, "data-jtml-instance-id"), id);
        if (instance.instanceID == 0) {
            instance.instanceID = static_cast<JTML::InstanceID>(out.size() + 1);
        }
        try {
            const std::string sourceLine = attrValueFor(elem, "data-jtml-source-line");
            instance.sourceLine = sourceLine.empty() ? 0 : std::stoi(sourceLine);
        } catch (...) {
            instance.sourceLine = 0;
        }
        instance.params = parseRuntimeMap(attrValueFor(elem, "data-jtml-component-params"));
        instance.locals = parseRuntimeMap(attrValueFor(elem, "data-jtml-component-locals"));
        out.push_back(std::move(instance));
    }

    for (const auto& child : elem.content) {
        collectComponentInstances(*child, out);
    }
}

void Interpreter::collectComponentDefinitions(const ASTNode& node,
                                              std::vector<RuntimeComponentDefinition>& out) {
    if (node.getType() != ASTNodeType::JtmlElement) return;
    const auto& elem = static_cast<const JtmlElementNode&>(node);
    const std::string name = attrValueFor(elem, "data-jtml-component-def");
    if (!name.empty()) {
        RuntimeComponentDefinition def;
        def.moduleId =
            parseSemanticModuleId(attrValueFor(elem, "data-jtml-component-def-module"));
        def.name = name;
        try {
            const std::string sourceLine = attrValueFor(elem, "data-jtml-source-line");
            def.sourceLine = sourceLine.empty() ? 0 : std::stoi(sourceLine);
        } catch (...) {
            def.sourceLine = 0;
        }
        def.params = parseRuntimeList(attrValueFor(elem, "data-jtml-component-def-params"));
        def.emits = parseRuntimeList(attrValueFor(elem, "data-jtml-component-def-emits"));
        def.emitArity = parseRuntimeIntMap(attrValueFor(elem, "data-jtml-component-def-emit-arity"));
        def.emitPayloads = parseRuntimeStringListMap(attrValueFor(elem, "data-jtml-component-def-emit-payloads"));
        def.emitPayloadTypes = parseRuntimeStringListMap(attrValueFor(elem, "data-jtml-component-def-emit-payload-types"));
        def.body = hexDecode(attrValueFor(elem, "data-jtml-component-body-hex"));

        auto existing = std::find_if(out.begin(), out.end(),
            [&](const RuntimeComponentDefinition& current) {
                return current.name == def.name &&
                       current.moduleId == def.moduleId;
            });
        if (existing == out.end()) out.push_back(std::move(def));
    }

    for (const auto& child : elem.content) {
        collectComponentDefinitions(*child, out);
    }
}

void Interpreter::attachComponentEnvironment(RuntimeComponentInstance& instance) {
    if (!instance.environment) {
        instance.environment = std::make_shared<JTML::Environment>(
            globalEnv,
            instance.instanceID,
            renderer.get());
    }
    instance.environment->setRenderer(renderer.get());
    if (const auto* definition = findComponentDefinition(
            instance.component, instance.definitionModule, instance.moduleId)) {
        instance.stateNames = definition->localState;
        instance.derivedNames = definition->localDerived;
        instance.actionNames = definition->localActions;
        instance.effectNames = definition->localEffects;
    }

    for (const auto& [paramName, paramValue] : instance.params) {
        JTML::CompositeKey paramKey{instance.environment->instanceID, paramName};
        if (!instance.environment->hasVariable(paramKey)) {
            instance.environment->defineVariable(
                paramKey,
                std::make_shared<JTML::VarValue>(paramValue));
        }
    }

    for (const auto& [publicName, loweredName] : instance.locals) {
        JTML::CompositeKey globalKey{globalEnv->instanceID, loweredName};
        JTML::CompositeKey localPublicKey{instance.environment->instanceID, publicName};
        JTML::CompositeKey localLoweredKey{instance.environment->instanceID, loweredName};
        auto scopedVarIt = instance.environment->variables.find(localLoweredKey);
        if (scopedVarIt != instance.environment->variables.end()) {
            instance.environment->variables[localPublicKey] = scopedVarIt->second;
            instance.environment->nameToId[localPublicKey] = instance.environment->getVarID(localLoweredKey);
            continue;
        }
        auto varIt = globalEnv->variables.find(globalKey);
        if (varIt != globalEnv->variables.end()) {
            instance.environment->variables[localPublicKey] = varIt->second;
            instance.environment->nameToId[localPublicKey] = globalEnv->getVarID(globalKey);
        }
        auto scopedFnIt = instance.environment->functions.find(localLoweredKey);
        if (scopedFnIt != instance.environment->functions.end()) {
            instance.environment->functions[localPublicKey] = scopedFnIt->second;
            continue;
        }
        auto fnIt = globalEnv->functions.find(globalKey);
        if (fnIt != globalEnv->functions.end()) {
            instance.environment->functions[localPublicKey] = fnIt->second;
        }
    }
    instance.runtimeReady = true;
}

void Interpreter::registerComponentInstances(const std::vector<std::unique_ptr<ASTNode>>& program) {
    std::unordered_map<std::string, std::shared_ptr<JTML::Environment>> existingById;
    std::unordered_map<JTML::InstanceID, std::shared_ptr<JTML::Environment>> existingByInstance;
    for (const auto& instance : componentInstances) {
        if (!instance.environment) continue;
        existingById[instance.id] = instance.environment;
        existingByInstance[instance.instanceID] = instance.environment;
    }

    componentInstances.clear();
    componentDefinitions.clear();

    const auto plan = jtml::buildRuntimePlan(program);
    if (!plan.componentDefinitions.empty() || !plan.componentInstances.empty()) {
        for (const auto& plannedDef : plan.componentDefinitions) {
            RuntimeComponentDefinition def;
            def.moduleId = plannedDef.moduleId;
            def.name = plannedDef.name;
            def.sourceLine = plannedDef.sourceLine;
            def.params = plannedDef.params;
            def.emits = plannedDef.emits;
            def.emitArity = plannedDef.emitArity;
            def.emitPayloads = plannedDef.emitPayloads;
            def.emitPayloadTypes = plannedDef.emitPayloadTypes;
            def.body = plannedDef.bodySource;
            for (const auto& plannedNode : plannedDef.bodyPlan) {
                def.bodyPlan.push_back({
                    plannedNode.definitionModule,
                    plannedNode.sourceLine,
                    plannedNode.indent,
                    plannedNode.parentIndex,
                    plannedNode.childIndices,
                    plannedNode.kind,
                    plannedNode.head,
                    plannedNode.name,
                    plannedNode.text,
                    plannedNode.operatorToken,
                    plannedNode.expression,
                    plannedNode.keyExpression,
                    plannedNode.reads,
                    plannedNode.writes,
                    plannedNode.renderRoot,
                });
            }
            def.localState = plannedDef.localState;
            def.localDerived = plannedDef.localDerived;
            def.localActions = plannedDef.localActions;
            def.localEffects = plannedDef.localEffects;
            def.eventBindings = plannedDef.eventBindings;
            def.hasSlot = plannedDef.hasSlot;
            def.bodyNodeCount = plannedDef.bodyNodeCount;
            def.rootTemplateNodeCount = plannedDef.rootTemplateNodeCount;
            def.slotCount = plannedDef.slotCount;

            auto existing = std::find_if(componentDefinitions.begin(), componentDefinitions.end(),
                [&](const RuntimeComponentDefinition& current) {
                    return current.name == def.name &&
                           current.moduleId == def.moduleId;
                });
            if (existing == componentDefinitions.end()) componentDefinitions.push_back(std::move(def));
        }

        auto propertiesToMap = [](const std::vector<jtml::SemanticProperty>& properties) {
            std::map<std::string, std::string> out;
            for (const auto& property : properties) {
                out[property.name] = property.value;
            }
            return out;
        };

        for (const auto& plannedInstance : plan.componentInstances) {
            RuntimeComponentInstance instance;
            instance.moduleId = plannedInstance.moduleId;
            instance.definitionModule = plannedInstance.definitionModule;
            instance.id = plannedInstance.id;
            instance.component = plannedInstance.component;
            instance.role = plannedInstance.role.empty() ? "component" : plannedInstance.role;
            instance.sourceLine = plannedInstance.sourceLine;
            instance.instanceID = static_cast<JTML::InstanceID>(plannedInstance.instanceId);
            if (instance.instanceID == 0) {
                instance.instanceID = parseInstanceId("", instance.id);
            }
            if (instance.instanceID == 0) {
                instance.instanceID = static_cast<JTML::InstanceID>(componentInstances.size() + 1);
            }
            instance.params = propertiesToMap(plannedInstance.params);
            instance.locals = propertiesToMap(plannedInstance.locals);
            for (const auto& plannedNode : plannedInstance.slotPlan) {
                instance.slotPlan.push_back({
                    plannedNode.definitionModule,
                    plannedNode.sourceLine,
                    plannedNode.indent,
                    plannedNode.parentIndex,
                    plannedNode.childIndices,
                    plannedNode.kind,
                    plannedNode.head,
                    plannedNode.name,
                    plannedNode.text,
                    plannedNode.operatorToken,
                    plannedNode.expression,
                    plannedNode.keyExpression,
                    plannedNode.reads,
                    plannedNode.writes,
                    plannedNode.renderRoot,
                });
            }
            componentInstances.push_back(std::move(instance));
        }
    } else {
        for (const auto& node : program) {
            if (!node) continue;
            collectComponentDefinitions(*node, componentDefinitions);
            collectComponentInstances(*node, componentInstances);
        }
    }

    for (auto& instance : componentInstances) {
        auto byId = existingById.find(instance.id);
        if (byId != existingById.end()) {
            instance.environment = byId->second;
        } else {
            auto byInstance = existingByInstance.find(instance.instanceID);
            if (byInstance != existingByInstance.end()) {
                instance.environment = byInstance->second;
            }
        }
        attachComponentEnvironment(instance);
    }
}

const Interpreter::RuntimeComponentDefinition*
Interpreter::findComponentDefinition(const std::string& name) const {
    return findComponentDefinition(
        name, jtml::InvalidSemanticModuleId, jtml::InvalidSemanticModuleId);
}

const Interpreter::RuntimeComponentDefinition*
Interpreter::findComponentDefinition(const std::string& name,
                                     jtml::SemanticModuleId preferredModule,
                                     jtml::SemanticModuleId fallbackModule) const {
    auto matches = [&](const RuntimeComponentDefinition& definition,
                       jtml::SemanticModuleId moduleId) {
        return moduleId != jtml::InvalidSemanticModuleId &&
               definition.name == name &&
               definition.moduleId == moduleId;
    };
    auto preferredIt = std::find_if(
        componentDefinitions.begin(), componentDefinitions.end(),
        [&](const RuntimeComponentDefinition& definition) {
            return matches(definition, preferredModule);
        });
    if (preferredIt != componentDefinitions.end()) return &(*preferredIt);

    auto fallbackIt = std::find_if(
        componentDefinitions.begin(), componentDefinitions.end(),
        [&](const RuntimeComponentDefinition& definition) {
            return matches(definition, fallbackModule);
        });
    if (fallbackIt != componentDefinitions.end()) return &(*fallbackIt);

    auto definitionIt = std::find_if(
        componentDefinitions.begin(), componentDefinitions.end(),
        [&](const RuntimeComponentDefinition& definition) {
            return definition.name == name;
        });
    return definitionIt == componentDefinitions.end() ? nullptr : &(*definitionIt);
}

Interpreter::RuntimeComponentInstance* Interpreter::findComponentInstance(const std::string& id) {
    auto instanceIt = std::find_if(
        componentInstances.begin(), componentInstances.end(),
        [&](const RuntimeComponentInstance& instance) {
            return instance.id == id || std::to_string(instance.instanceID) == id;
        });
    if (instanceIt != componentInstances.end()) return &(*instanceIt);
    auto dynamicIt = dynamicComponentInstances.find(id);
    if (dynamicIt != dynamicComponentInstances.end()) return &dynamicIt->second;
    auto dynamicByInstance = std::find_if(
        dynamicComponentInstances.begin(), dynamicComponentInstances.end(),
        [&](const auto& entry) {
            return std::to_string(entry.second.instanceID) == id;
        });
    return dynamicByInstance == dynamicComponentInstances.end()
        ? nullptr
        : &dynamicByInstance->second;
}

Interpreter::RuntimeComponentInstance* Interpreter::findComponentInstanceForElement(const JtmlElementNode& elem) {
    const std::string id = attrValueFor(elem, "data-jtml-instance");
    if (id.empty()) return nullptr;
    return findComponentInstance(id);
}

std::shared_ptr<JTML::Environment> Interpreter::environmentForInstance(JTML::InstanceID instanceID) const {
    if (!globalEnv || instanceID == globalEnv->instanceID) return globalEnv;
    auto instanceIt = std::find_if(
        componentInstances.begin(), componentInstances.end(),
        [&](const RuntimeComponentInstance& instance) {
            return instance.instanceID == instanceID;
        });
    if (instanceIt != componentInstances.end() && instanceIt->environment) {
        return instanceIt->environment;
    }
    auto dynamicIt = std::find_if(
        dynamicComponentInstances.begin(), dynamicComponentInstances.end(),
        [&](const auto& entry) {
            return entry.second.instanceID == instanceID;
        });
    if (dynamicIt != dynamicComponentInstances.end() && dynamicIt->second.environment) {
        return dynamicIt->second.environment;
    }
    return globalEnv;
}
// In jtml_interpreter.cpp

void Interpreter::interpretElementAttributes(const JtmlElementNode& elem) {
    auto bindingEnv = currentEnv ? currentEnv : globalEnv;
    // Iterate through all attributes of the element
    for (const auto& attr : elem.attributes) {

        const std::string& attrName = attr.key;
        const auto classification = classifyJtmlAttribute(attr);
        if (classification.kind == JtmlAttributeKind::Event) {
            const TranspiledBinding* transpiledBinding = transpiler.getAttributeBinding(attr);
            const std::unique_ptr<ExpressionStatementNode>& attrValue = attr.value;
            std::string derivedVarName;
            std::string elementId;
            if (transpiledBinding) {
                derivedVarName = transpiledBinding->variableName;
                elementId = transpiledBinding->elementId;
            } else {
                uniqueVarID++;
                derivedVarName = "attr_" + std::to_string(uniqueVarID);
                elementId = derivedVarName;
            }

            JTML::CompositeKey attrKey{ bindingEnv->instanceID, derivedVarName };
                    // Create a BindingInfo for the attribute
            JTML::BindingInfo binding;
            binding.varName = attrKey;
            binding.elementId = elementId;
            binding.bindingType = "attribute_event";
            binding.attribute = attrName;
            binding.expression = attrValue->clone();

            // Register the binding
            globalEnv->registerBinding(binding);

        } else if (classification.kind == JtmlAttributeKind::Reactive) {
            const TranspiledBinding* transpiledBinding = transpiler.getAttributeBinding(attr);
            const std::unique_ptr<ExpressionStatementNode>& attrValue = attr.value;
            // Register attribute bindings similar to content bindings
            std::string derivedVarName;
            std::string elementId;
            if (transpiledBinding) {
                derivedVarName = transpiledBinding->variableName;
                elementId = transpiledBinding->elementId;
            } else {
                uniqueVarID++;
                derivedVarName = "attr_" + std::to_string(uniqueVarID);
                elementId = derivedVarName;
            }

            JTML::CompositeKey attrKey{ bindingEnv->instanceID, derivedVarName };

            // Gather dependencies
            std::vector<JTML::CompositeKey> deps;
            gatherDeps(attrValue.get(), deps, bindingEnv);

            // Clone the expression
            std::unique_ptr<ExpressionStatementNode> clonedExpr = attrValue->clone();

            // Define an evaluator lambda
            auto evaluator = [this, bindingEnv](const ExpressionStatementNode* expr) -> std::shared_ptr<JTML::VarValue> {
                return evaluateExpression(expr, bindingEnv);
            };

            // Derive the variable
            bindingEnv->deriveVariable(attrKey, std::move(clonedExpr), deps, evaluator);

            // Create a BindingInfo for the attribute
            JTML::BindingInfo binding;
            binding.varName = attrKey;
            binding.elementId = elementId;
            binding.bindingType = "attribute";
            binding.attribute = attrName;
            binding.expression = attrValue->clone();

            // Register the binding
            globalEnv->registerBinding(binding);
        }
    }
}

// Helper function to determine if an attribute is an event handler
bool Interpreter::isEventAttribute(const std::string& attrName) const {
    return attrName.rfind("on", 0) == 0 && attrName.size() > 2; // Starts with "on"
}

// Helper function to extract event type from attribute name
std::string Interpreter::extractEventType(const std::string& attrName) const {
    if (attrName.size() > 2) {
        return "on" + attrName.substr(2); // Keep the "on" prefix
    }
    return attrName;
}

// Helper function to check if an expression contains dynamic bindings
bool Interpreter::containsExpression(const ExpressionStatementNode* exprNode) const {
    // Implement logic to determine if the expression contains variables or operations
    // that require dynamic binding
    // For simplicity, assume all non-literal expressions require binding
    if (!exprNode) return false;
    switch (exprNode->getExprType()) {
        case ExpressionStatementNodeType::Variable:
        case ExpressionStatementNodeType::Binary:
        case ExpressionStatementNodeType::Unary:
        case ExpressionStatementNodeType::FunctionCall:
        case ExpressionStatementNodeType::Subscript:
            return true;
        default:
            return false;
    }
}


void Interpreter::interpretShowElement(const ShowStatementNode& node) {
    auto bindingEnv = currentEnv ? currentEnv : globalEnv;
    nodeID++;
    const std::string* transpiledName = transpiler.getNodeBinding(node, "show");
    std::string derivedName;
    if (transpiledName) {
        derivedName = *transpiledName;
    } else {
        uniqueVarID++;
        derivedName = "expr_" + std::to_string(uniqueVarID);
    }
    std::vector<JTML::CompositeKey> deps;
    gatherDeps(node.expr.get(), deps, bindingEnv);

    auto cloned = node.expr->clone();
    auto eval   = [this, bindingEnv](const ExpressionStatementNode* expr){
        return evaluateExpression(expr, bindingEnv);
    };

    JTML::CompositeKey showKey{ bindingEnv->instanceID, derivedName };
    bindingEnv->deriveVariable(showKey, std::move(cloned), deps, eval);

    // binding
    JTML::BindingInfo b;
    b.varName     = showKey;
    b.elementId = derivedName;
    b.bindingType = "content"; // e.g. content or "show"
    b.expression = node.expr->clone();
    globalEnv->registerBinding(b);

    // interpret once => log or similar
    auto val = bindingEnv->getVariable(showKey);
    std::cout << "[SHOW] " << derivedName << " => "
              << (val ? val->toString() : "(null)") << "\n";
}


void Interpreter::interpretWhileElement(const WhileStatementNode& node) {
    auto bindingEnv = currentEnv ? currentEnv : globalEnv;
    nodeID++;
    const std::string* transpiledName = transpiler.getNodeBinding(node, "while");
    std::string derivedName;
    if (transpiledName) {
        derivedName = *transpiledName;
    } else {
        uniqueVarID++;
        derivedName = "cond_" + std::to_string(uniqueVarID);
    }
    // gather deps, define variable
    std::vector<JTML::CompositeKey> deps;
    gatherDeps(node.condition.get(), deps, bindingEnv);

    auto cloned = node.condition->clone();
    auto eval = [this, bindingEnv](const ExpressionStatementNode* expr){
        return evaluateExpression(expr, bindingEnv);
    };

    JTML::CompositeKey condKey{ bindingEnv->instanceID, derivedName };
    bindingEnv->deriveVariable(condKey, std::move(cloned), deps, eval);

    // binding
    JTML::BindingInfo b;
    b.varName     = condKey;
    b.elementId = derivedName;
    b.bindingType = "while";
    b.expression = node.condition->clone();
    globalEnv->registerBinding(b);

    // server side while loop if you want
    while(true) {
        bool condVal = isTruthy(bindingEnv->getVariable(condKey)->toString());
        if(!condVal) break;

        try {
            for (auto& stmt : node.body) {
                interpretNode(*stmt);
            }
        } catch (const BreakException&) {
            break;
        } catch (const ContinueException&) {
            continue;
        }
        recalcAllDirty();
    }
}

void Interpreter::interpretIfElement(const IfStatementNode& node) {
    auto bindingEnv = currentEnv ? currentEnv : globalEnv;
    nodeID++;
    const std::string* transpiledName = transpiler.getNodeBinding(node, "if");
    std::string derivedName;
    if (transpiledName) {
        derivedName = *transpiledName;
    } else {
        uniqueVarID++;
        derivedName = "cond_" + std::to_string(uniqueVarID);
    }
    std::vector<JTML::CompositeKey> deps;
    gatherDeps(node.condition.get(), deps, bindingEnv);

    // define the derived variable
    auto clonedExpr = node.condition->clone();
    auto evaluator  = [this, bindingEnv](const ExpressionStatementNode* expr){
        return evaluateExpression(expr, bindingEnv);
    };

    JTML::CompositeKey condKey{ bindingEnv->instanceID, derivedName };
    bindingEnv->deriveVariable(condKey, std::move(clonedExpr), deps, evaluator);

    // register an "if" binding
    JTML::BindingInfo bind;
    bind.varName = condKey;
    bind.elementId = derivedName;
    bind.bindingType = "if";
    bind.expression = node.condition->clone();
    globalEnv->registerBinding(bind);

    // Walk BOTH branches so every element-level binding is registered up
    // front, even for the branch that isn't initially live. The transpiler
    // already emits IDs for both branches and toggles their visibility on
    // the client; without this pass the interpreter's binding map only
    // knows the initially-taken branch, so dispatching an event on an
    // element that becomes visible after the user flips the predicate
    // (e.g. clicking "Login" after "Logout" in the auth-store sample)
    // fails with "No bindings for element: ...". Using the same dispatch
    // table as `interpretElement`'s child loop avoids accidentally firing
    // imperative statements that the body validator already rejects.
    auto interpretElementChild = [this](const ASTNode& child) {
        switch (child.getType()) {
            case ASTNodeType::ShowStatement:
                interpretShowElement(static_cast<const ShowStatementNode&>(child));
                break;
            case ASTNodeType::IfStatement:
                interpretIfElement(static_cast<const IfStatementNode&>(child));
                break;
            case ASTNodeType::ForStatement:
                interpretForElement(static_cast<const ForStatementNode&>(child));
                break;
            case ASTNodeType::WhileStatement:
                interpretWhileElement(static_cast<const WhileStatementNode&>(child));
                break;
            case ASTNodeType::JtmlElement:
                interpretElement(static_cast<const JtmlElementNode&>(child));
                break;
            default:
                std::cerr << "[ERROR] Disallowed statement '" << child.toString()
                          << "' inside if/else branch\n";
                break;
        }
    };
    for (auto& stmt : node.thenStatements) {
        interpretElementChild(*stmt);
    }
    for (auto& stmt : node.elseStatements) {
        interpretElementChild(*stmt);
    }
}


void Interpreter::interpretForElement(const ForStatementNode& node) {
    auto bindingEnv = currentEnv ? currentEnv : globalEnv;
    nodeID++;
    const std::string* transpiledName = transpiler.getNodeBinding(node, "for");
    std::string derivedName;
    if (transpiledName) {
        derivedName = *transpiledName;
    } else {
        uniqueVarID++;
        derivedName = "range_" + std::to_string(uniqueVarID);
    }

    std::vector<JTML::CompositeKey> deps;
    gatherDeps(node.iterableExpression.get(), deps, bindingEnv);

    // define the derived variable
    auto clonedExpr = node.iterableExpression->clone();
    auto evaluator  = [this, bindingEnv](const ExpressionStatementNode* expr){
        return evaluateExpression(expr, bindingEnv);
    };

    JTML::CompositeKey condKey{ bindingEnv->instanceID, derivedName };
    bindingEnv->deriveVariable(condKey, std::move(clonedExpr), deps, evaluator);

    // register an "if" binding
    JTML::BindingInfo bind;
    bind.varName = condKey;
    bind.bindingType = "for";
    bind.elementId = derivedName;
    bind.expression = node.iterableExpression->clone();
    globalEnv->registerBinding(bind);


}



void Interpreter::interpretBlockStatement(const BlockStatementNode& block) {
    std::cout << "[DEBUG] Entering BlockStatement with "
              << block.statements.size() << " statements.\n";

    auto previousEnv = currentEnv;
    currentEnv = std::make_shared<JTML::Environment>(previousEnv);

    try {
        for (const auto& stmt : block.statements) {
            std::cout << "Interpreting node " << stmt->toString() << "\n";
            interpretNode(*stmt);
        }
    } catch (...) {
        // Ensure environment restoration even on exception
        currentEnv = previousEnv;
        throw;
    }

    currentEnv = previousEnv;

    std::cout << "[DEBUG] Exiting BlockStatement.\n";
}

void Interpreter::interpretShow(const ShowStatementNode& stmt) {
    if (!stmt.expr) {
        std::cout << "[SHOW] (empty?)\n";
        return;
    }
    // Evaluate the expression => store it in a new server var like expr_X

    // Evaluate
    auto val = evaluateExpression(stmt.expr.get(), currentEnv);

    std::cout << "[SHOW] " <<  val->toString() << "\n";
    // Whenever val changes, the environment can push updateBinding messages
}

void Interpreter::interpretExpression(const ExpressionNode& node) {
    // Evaluate the expression contained in the node
    auto result = evaluateExpression(node.expression.get(), currentEnv);

    // Optionally, handle side effects or log the result
    std::cout << "[DEBUG] Evaluated expression: " << result->toString() << "\n";
}

void Interpreter::interpretDefine(const DefineStatementNode& stmt) {
    try {
        std::shared_ptr<JTML::VarValue> valPtr = evaluateExpression(stmt.expression.get(), currentEnv);
        JTML::CompositeKey varKey = { currentEnv->instanceID, stmt.identifier };

        // 3. Set the variable in the global environment using JTML::CompositeKey
        if (stmt.isConst) {
            currentEnv->defineVariable(varKey, valPtr, JTML::VarKind::Frozen);
        } else {
            currentEnv->defineVariable(varKey, valPtr, JTML::VarKind::Normal);
        }

        // Only mark dirty and recalculate if in the global or instance scope
        if (currentEnv->isGlobalEnvironment()) {
            currentEnv->recalcDirty([this](JTML::VarID varID) {
                updateVariable(varID, currentEnv);
            });
        }

        // Enhanced Logging: Separate value and type information
        std::cout << (stmt.isConst ? "[CONST] " : "[DEFINE] ")
                  << currentEnv->getCompositeName(varKey) << " = " << valPtr->toString();
        if (valPtr->isNumber()) {
            std::cout << " (Number)";
        }
        else if (valPtr->isString()) {
            std::cout << " (String)";
        }
        else if (valPtr->isBool()) {
            std::cout << " (Boolean)";
        }
        else if (valPtr->isArray()) {
            std::cout << " (Array)";
        }
        else if (valPtr->isDict()) {
            std::cout << " (Dictionary)";
        }
        std::cout << "\n";
    } catch (const ReturnException&) {
        // Allow ReturnException to propagate
        throw;
    } catch (const std::exception& e) {
        handleError(std::string("Define Statement Error: ") + e.what());
    }
}


void Interpreter::interpretAssignment(const AssignmentStatementNode& stmt) {
    // 1) Evaluate the RHS
    auto newVal = evaluateExpression(stmt.rhs.get(), currentEnv);
    std::cout << " (Assignment RHS: " << newVal->toString() << "\n";
    auto markBaseObjectDirty = [this](const ExpressionStatementNode* base) {
        if (!base || base->getExprType() != ExpressionStatementNodeType::Variable) return;
        const auto& baseNode = static_cast<const VariableExpressionStatementNode&>(*base);
        auto env = currentEnv;
        while (env) {
            JTML::CompositeKey key{ env->instanceID, baseNode.name };
            if (env->variables.find(key) != env->variables.end()) {
                env->markDirty(key);
                return;
            }
            env = env->parent;
        }
    };

    // 2) Evaluate the LHS (determine its type and handle accordingly)
    switch (stmt.lhs->getExprType()) {
        case ExpressionStatementNodeType::Variable: {
            // Handle simple variable assignment
            const auto& varNode = static_cast<const VariableExpressionStatementNode&>(*stmt.lhs);
            JTML::CompositeKey varKey = { currentEnv->instanceID, varNode.name };
            currentEnv->setVariable(varKey, newVal);
            break;
        }

        case ExpressionStatementNodeType::ObjectPropertyAccess: {
            // Handle property access assignment (e.g., obj.prop)
            const auto& propNode = static_cast<const ObjectPropertyAccessExpressionNode&>(*stmt.lhs);
            auto baseVal = evaluateExpression(propNode.base.get(), currentEnv);

            if (baseVal->isDict()) {
                baseVal->getDict()->set(propNode.propertyName, newVal);
                markBaseObjectDirty(propNode.base.get());
                break;
            }

            if (!baseVal->isObject()) {
                throw std::runtime_error("Cannot assign to non-object property: " + propNode.propertyName);
            }

            auto& objHandle = baseVal->getObjectHandle();
            JTML::CompositeKey propKey = { objHandle.instanceEnv->instanceID, propNode.propertyName };

            // Set the property using JTML::CompositeKey
            objHandle.instanceEnv->setVariable(propKey, newVal);
            break;
        }

        case ExpressionStatementNodeType::Subscript: {
            // Handle subscript assignment (e.g., arr[idx] or dict[key])
            const auto& subNode = static_cast<const SubscriptExpressionStatementNode&>(*stmt.lhs);
            auto baseVal = evaluateExpression(subNode.base.get(), currentEnv);
            auto indexVal = evaluateExpression(subNode.index.get(), currentEnv);

            if (baseVal->isArray()) {
                auto reactiveArray = baseVal->getArray();
                double idxNum = getNumericValue(indexVal);
                int idx = static_cast<int>(idxNum);

                if (idx < 0 || static_cast<size_t>(idx) >= reactiveArray->size()) {
                    throw std::runtime_error("Array index out of range: " + std::to_string(idx));
                }

                reactiveArray->set(idx, newVal);
                markBaseObjectDirty(subNode.base.get());
            }
            else if (baseVal->isDict()) {
                auto reactiveDict = baseVal->getDict();
                std::string key = getStringValue(indexVal);
                reactiveDict->set(key, newVal);
                markBaseObjectDirty(subNode.base.get());
            }
            else {
                throw std::runtime_error("Cannot subscript-assign to non-array/non-dict type");
            }
            break;
        }

        default:
            throw std::runtime_error("Invalid left-hand side expression in assignment");
    }

    // 3) Recalculate dirty variables, log the assignment, etc.
    std::cout << "[ASSIGN] LHS=";
    switch (stmt.lhs->getExprType()) {
        case ExpressionStatementNodeType::Variable: {
            const auto& varNode = static_cast<const VariableExpressionStatementNode&>(*stmt.lhs);
            JTML::CompositeKey varKey = { currentEnv->instanceID, varNode.name };
            std::cout << varKey.varName << " (InstanceID: " << varKey.instanceID << ")";
            break;
        }
        case ExpressionStatementNodeType::ObjectPropertyAccess: {
            const auto& propNode = static_cast<const ObjectPropertyAccessExpressionNode&>(*stmt.lhs);
            JTML::CompositeKey propKey = { currentEnv->instanceID, propNode.propertyName };
            std::cout << propKey.varName << " (InstanceID: " << propKey.instanceID << ")";
            break;
        }
        case ExpressionStatementNodeType::Subscript: {
            const auto& subNode = static_cast<const SubscriptExpressionStatementNode&>(*stmt.lhs);
            std::cout << subNode.toString();
            break;
        }
        default:
            std::cout << "Unknown LHS";
            break;
    }
    std::cout << " => RHS=" << newVal->toString() << "\n";
    currentEnv->recalcDirty([this](JTML::VarID varID) {
                        updateVariable(varID, currentEnv);
                    });

}


void Interpreter::interpretClassDeclaration(const ClassDeclarationNode& node) {
    if (classDeclarations.find(node.name) != classDeclarations.end()) {
        throw std::runtime_error("Class already defined: " + node.name);
    }

    std::cout << "[DEBUG] Interpreting ClassDeclarationNode: " << node.toString() << "\n";

    // Move the members to the new class declaration
    auto membersCopy = std::vector<std::unique_ptr<ASTNode>>{};
    for (const auto& member : node.members) {
        membersCopy.push_back(member->clone()); // Assuming `ASTNode` has a `clone` method
    }

    classDeclarations[node.name] = std::make_shared<ClassDeclarationNode>(
        node.name, node.parentName, std::move(membersCopy)
    );

    std::cout << "Class '" << node.name << "' defined.\n";
}
 void Interpreter::interpretDerive(const DeriveStatementNode& stmt) {
        try {
            // Ensure currentEnv is valid
            if (!currentEnv) {
                throw std::runtime_error("No active environment for derive statement.");
            }

            // Retrieve the current InstanceID from the environment
            JTML::InstanceID currentID = currentEnv->instanceID;

            // Gather dependencies from the expression
            std::vector<JTML::CompositeKey> deps;
            gatherDeps(stmt.expression.get(), deps, currentEnv);

            // Clone the expression to store in the derived variable
            std::unique_ptr<ExpressionStatementNode> newExpr = stmt.expression->clone();

            // Define a lambda to evaluate expressions using the Interpreter's evaluateExpression
            auto evaluator = [this](const ExpressionStatementNode* exprNode) -> std::shared_ptr<JTML::VarValue> {
                return this->evaluateExpression(exprNode, this->currentEnv);
            };

            // Create JTML::CompositeKey for the variable
            JTML::CompositeKey key = { currentID, stmt.identifier };

            std::cout << "[DEBUG] About to rum derive on " << currentEnv->getCompositeName(key) << " derived from dependencies: ";
            for (const auto& dep : deps) {
                std::cout << currentEnv->getCompositeName(dep) << " ";
            }
            std::cout << "\n";

            // Define or update the derived variable by calling Environment's method
            currentEnv->deriveVariable(key, std::move(newExpr), std::move(deps), evaluator);

            std::cout << "[DERIVE] " << currentEnv->getCompositeName(key) << " derived from dependencies: ";
            for (const auto& dep : deps) {
                std::cout << dep.varName << " ";
            }
            std::cout << "\n";

        } catch (const std::exception& e) {
            handleError("Derive Statement Error: " + std::string(e.what()));
        }
    }

    // Adapted interpretUnbind
void Interpreter::interpretUnbind(const UnbindStatementNode& stmt) {
    try {
        // Ensure currentEnv is valid
        if (!currentEnv) {
            throw std::runtime_error("No active environment for unbind statement.");
        }

        // Retrieve the current InstanceID from the environment
        JTML::InstanceID currentID = currentEnv->instanceID;

        // Create JTML::CompositeKey for the variable
        JTML::CompositeKey key = { currentID, stmt.identifier };

        // Unbind the variable by calling Environment's method
        currentEnv->unbindVariable(key);

        // Logging
        std::cout << "[UNBIND] " << key.varName << " has been unbound from environment.\n";
    } catch (const std::exception& e) {
        handleError("Unbind Statement Error: " + std::string(e.what()));
    }
}

void Interpreter::interpretStore(const StoreStatementNode& stmt) {
    try {
        storeVariable(stmt.targetScope, stmt.variableName);
        std::cout << "[STORE] " << stmt.variableName << " => Scope: " << stmt.targetScope << "\n";
    } catch (const ReturnException&) {
        // Allow ReturnException to propagate
        throw;
    } catch (const std::exception& e) {
        handleError(std::string("Store Statement Error: ") + e.what());
    }
}

void Interpreter::interpretIf(const IfStatementNode& node) {
    try {
        bool conditionResult = evaluateCondition(node.condition.get(), currentEnv);
        std::cout << "[IF] Condition evaluated to: " << (conditionResult ? "true" : "false") << "\n";
        if (conditionResult) {
            // "Truthy" condition
            for (const auto& stmt : node.thenStatements) {
                interpretNode(*stmt);
            }
        } else if (!node.elseStatements.empty()) {
            // "Falsey" condition with an else block
            for (const auto& stmt : node.elseStatements) {
                interpretNode(*stmt);
            }
        }
    } catch (const ReturnException&) {
        // Allow ReturnException to propagate
        throw;
    } catch (const ContinueException&) {
        throw;
    } catch (const BreakException&) {
        throw;
    } catch (const std::exception& e) {
        handleError("If Statement Error: " + std::string(e.what()));
    }
}

void Interpreter::interpretWhile(const WhileStatementNode& node) {
    try {
        while (true) {
            if (!evaluateCondition(node.condition.get(), currentEnv)) {
                // Condition is "falsey" => break
                break;
            }

            // Interpret the loop body
            try {
                for (const auto& stmt : node.body) {
                    interpretNode(*stmt);
                }
            } catch (const BreakException&) {
                break;
            } catch (const ContinueException&) {
                continue;
            }

            // Recalculate derived variables if required
            recalcDirty(currentEnv);
        }
    } catch (const ReturnException&) {
        // Allow ReturnException to propagate
        throw;
    } catch (const std::exception& e) {
        handleError("While Interpretation Error: " + std::string(e.what()));
    }
}


void Interpreter::interpretFor(const ForStatementNode& node) {
    // Determine the current environment
    std::shared_ptr<JTML::Environment> env = currentEnv; // Assuming currentEnv is a member variable

    try {
        // Evaluate the iterable expression => returns a VarValue
        auto iterableVal = evaluateExpression(node.iterableExpression.get(), env);

        if (!node.rangeEndExpr) {
            // "for (i in someCollection)" logic
            if (iterableVal->isArray()) {
                const auto& arr = iterableVal->getArray();
                for (size_t idx = 0; idx < arr->size(); ++idx) {
                    assignLoopIterator(env, node.iteratorName, (*arr)[idx]);

                    try {
                        // Interpret each statement in the loop body
                        for (const auto& stmt : node.body) {
                            interpretNode(*stmt);
                        }
                    } catch (const BreakException&) {
                        break; // Exit the loop
                    } catch (const ContinueException&) {
                        continue; // Proceed to the next iteration
                    }

                    // Recalculate dirty variables if necessary
                    env->recalcDirty([this](JTML::VarID varID) {
                        updateVariable(varID, currentEnv);
                    });
                }
            }
            else if (iterableVal->isString()) {
                // For each character in the string
                const auto& s = iterableVal->getString();
                for (char c : s) {
                    // Create a VarValue for the current character
                    auto cVal = std::make_shared<JTML::VarValue>(std::string(1, c));

                    assignLoopIterator(env, node.iteratorName, cVal);

                    try {
                        // Interpret each statement in the loop body
                        for (const auto& stmt : node.body) {
                            interpretNode(*stmt);
                        }
                    } catch (const BreakException&) {
                        break; // Exit the loop
                    } catch (const ContinueException&) {
                        continue; // Proceed to the next iteration
                    }

                    // Recalculate dirty variables if necessary
                    env->recalcDirty([this](JTML::VarID varID) {
                        updateVariable(varID, currentEnv);
                    });
                }
            }
            else {
                throw std::runtime_error("Iterable in 'for' loop is neither an array nor a string.");
            }
        }
        else {
            // "for (i in X..Y)" logic
            auto endVal = evaluateExpression(node.rangeEndExpr.get(), env);
            double startNum = getNumericValue(iterableVal);
            double endNum   = getNumericValue(endVal);
            int startI = static_cast<int>(startNum);
            int endI   = static_cast<int>(endNum);

            for (int i = startI; i <= endI; i++) {
                // Create a VarValue for the current index
                auto iVal = std::make_shared<JTML::VarValue>(static_cast<double>(i));

                assignLoopIterator(env, node.iteratorName, iVal);

                try {
                    // Interpret each statement in the loop body
                    for (const auto& stmt : node.body) {
                        interpretNode(*stmt);
                    }
                } catch (const BreakException&) {
                    break; // Exit the loop
                } catch (const ContinueException&) {
                    continue; // Proceed to the next iteration
                }

                // Recalculate dirty variables if necessary
                env->recalcDirty([this](JTML::VarID varID) {
                    updateVariable(varID, currentEnv);
                });
            }
        }
    } catch (const ReturnException&) {
        // Allow ReturnException to propagate
        throw;
    } catch (const std::exception& e) {
        handleError("For Loop Interpretation Error: " + std::string(e.what()));
    }
}

void Interpreter::interpretBreak(const BreakStatementNode& node) {
    throw BreakException();
}

void Interpreter::interpretContinue(const ContinueStatementNode& node) {
    throw ContinueException();
}
void Interpreter::interpretTryExceptThen(const TryExceptThenNode& node) {
    std::shared_ptr<JTML::Environment> env = currentEnv; // Assuming currentEnv is a member variable
    bool errorOccurred = false;
    std::string errorMessage;

    // 1) Try block
    try {
        interpret(node.tryBlock);
    } catch (const ReturnException&) {
        // Allow ReturnException to propagate
        throw;
    } catch (const std::exception& e) {
        errorOccurred = true;
        errorMessage = e.what();
    }

    // 2) Except block
    if (errorOccurred && node.hasCatch) {
        // If there's a catch variable like 'except err'
        if (!node.catchIdentifier.empty()) {
            // Store the error message in environment using JTML::CompositeKey
            JTML::CompositeKey errKey = { env->instanceID, node.catchIdentifier };
            auto errVal = std::make_shared<JTML::VarValue>(errorMessage);
            env->setVariable(errKey, errVal);
        }
        try {
            interpret(node.catchBlock);
        } catch (const ReturnException&) {
            // Allow ReturnException to propagate
            throw;
        } catch (const std::exception& e) {
            handleError("Try-Except Block Interpretation Error: " + std::string(e.what()));
        }
    }

    // 3) Then (finally) block
    if (node.hasFinally) {
        try {
            interpret(node.finallyBlock);
        } catch (const ReturnException&) {
            // Allow ReturnException to propagate
            throw;
        } catch (const std::exception& e) {
            handleError("Finally Block Interpretation Error: " + std::string(e.what()));
        }
    } else if (errorOccurred && !node.hasCatch) {
        // Rethrow if we had an error but no catch
        throw std::runtime_error(errorMessage);
    }
}
void Interpreter::interpretThrow(const ThrowStatementNode& node) {
    try {
        std::string msg;
        if (node.expression) {
            msg = getStringValue(evaluateExpression(node.expression.get(), currentEnv));
        } else {
            msg = "Unspecified error";
        }
        // Throw it as a runtime_error
        throw std::runtime_error(msg);
    } catch (const ReturnException&) {
        // Allow ReturnException to propagate
        throw;
    } catch (const std::exception& e) {
        handleError(std::string("Throw Statement Error: ") + e.what());
    }
}

void Interpreter::interpretImport(const ImportStatementNode& stmt) {
    try {
        std::ifstream input(stmt.path);
        if (!input) {
            throw std::runtime_error("Could not open import path '" + stmt.path + "'");
        }

        std::stringstream buffer;
        buffer << input.rdbuf();

        Lexer lexer(jtml::normalizeSourceSyntax(buffer.str()));
        auto tokens = lexer.tokenize();
        if (!lexer.getErrors().empty()) {
            throw std::runtime_error("Lexer error in import '" + stmt.path + "': " + lexer.getErrors().front());
        }

        Parser parser(std::move(tokens));
        auto program = parser.parseProgram();
        if (!parser.getErrors().empty()) {
            throw std::runtime_error("Parser error in import '" + stmt.path + "': " + parser.getErrors().front());
        }

        // Runtime imports contribute declarations/state to the current
        // environment, but they must not replace the parent page's component
        // registry while the parent program is still being interpreted.
        for (const auto& importedNode : program) {
            if (importedNode) interpretNode(*importedNode);
        }
        recalcAllDirty();
        std::cout << "[IMPORT] " << stmt.path << "\n";
    } catch (const ReturnException&) {
        throw;
    } catch (const std::exception& e) {
        handleError("Import Statement Error: " + std::string(e.what()));
    }
}

void Interpreter::interpretReturn(const ReturnStatementNode& node) {
    std::cout << "[DEBUG] Interpreting ReturnStatementNode: " << node.toString() << "\n";

    if (!inFunctionContext) {
        handleError("Return statement outside function context");
        return;
    }

    try {
        // Initialize the return value
        std::shared_ptr<JTML::VarValue> returnValue;

        // Evaluate the return expression if it exists
        if (node.expr) {
            std::cout << "[DEBUG] node.expr: " << node.expr->toString() << "\n";
            returnValue = evaluateExpression(node.expr.get(), currentEnv);
        } else {
            // Default return value: an empty string
            static const std::shared_ptr<JTML::VarValue> defaultReturnValue =
                std::make_shared<JTML::VarValue>(JTML::ValueVariant{""});
            returnValue = defaultReturnValue;
        }

        // Log the return value
        std::cout << "[RETURN] " << (returnValue ? returnValue->toString() : "void") << "\n";

        // Propagate the return value through a ReturnException
        throw ReturnException(returnValue);

    } catch (const ReturnException&) {
        // Explicitly allow ReturnException to propagate
        throw;
    } catch (const std::exception& e) {
        std::cout << "[DEBUG] Function is not following ReturnException path! "<< "\n";
        handleError("Return Statement Error: " + std::string(e.what()));
    }
}

// jtml_interpreter.cpp

void Interpreter::interpretSubscribe(const SubscribeStatementNode& node) {
    try {
        // Get the function to be subscribed
        JTML::CompositeKey funcKey = { currentEnv->instanceID, node.functionName };
        std::shared_ptr<JTML::Function> func = currentEnv->getFunction(funcKey);
        if (!func) {
            throw std::runtime_error("Function '" + node.functionName + "' not defined.");
        }

        // Get the VarID of the variable to subscribe to
        JTML::CompositeKey key = { currentEnv->instanceID, node.variableName };

        // Get the VarID of the variable to subscribe to using JTML::CompositeKey
        JTML::VarID varID = currentEnv->getVarID(key);

        // Check if the variable is an array or dict
        auto varValue = currentEnv->getVariable(key);
        if (!varValue) {
            throw std::runtime_error("Variable '" + key.varName + "' not found for subscription.");
        }

        auto subscriptionEnv = currentEnv;
        std::function<void()> callback;

        if (varValue->isArray()) {
            auto reactiveArray = varValue->getArray();
            callback = [this, reactiveArray, func, key, subscriptionEnv]() {
                try {
                    std::vector<std::shared_ptr<JTML::VarValue>> args;
                    if (func->parameters.size() == 1) {
                        args.push_back(subscriptionEnv->getVariable(key));
                    } else {
                        for (const auto& elem : reactiveArray->getArrayData()) {
                            args.push_back(elem);
                        }
                    }
                    executeFunction(func, args, nullptr);
                } catch (const std::exception& e) {
                    std::cerr << "[ERROR] Callback execution failed for function '"
                              << func->name << "': " << e.what() << "\n";
                }
            };
        }
        else if (varValue->isDict()) {
            auto reactiveDict = varValue->getDict();
            callback = [this, reactiveDict, func, key, subscriptionEnv]() {
                try {
                    std::vector<std::shared_ptr<JTML::VarValue>> args;
                    if (func->parameters.size() == 1) {
                        args.push_back(subscriptionEnv->getVariable(key));
                    } else {
                        for (const auto& [k, v] : reactiveDict->getDictData()) {
                            args.push_back(v);
                        }
                    }
                    executeFunction(func, args, nullptr);
                } catch (const std::exception& e) {
                    std::cerr << "[ERROR] Callback execution failed for function '"
                              << func->name << "': " << e.what() << "\n";
                }
            };
        }
        else {
            callback = [this, func, key, subscriptionEnv]() {
                try {
                    std::vector<std::shared_ptr<JTML::VarValue>> args;
                    if (func->parameters.size() == 1) {
                        args.push_back(subscriptionEnv->getVariable(key));
                    } else if (!func->parameters.empty()) {
                        throw std::runtime_error("Scalar subscription callbacks take 0 or 1 parameter.");
                    }
                    executeFunction(func, args, nullptr);
                } catch (const std::exception& e) {
                    std::cerr << "[ERROR] Callback execution failed for function '"
                              << func->name << "': " << e.what() << "\n";
                }
            };
        }

        // Subscribe the callback to the variable
        JTML::SubscriptionID subID = currentEnv->subscribeFunctionToVariable(key, node.functionName, callback);

        std::cout << "[SUBSCRIBE] Function '" << node.functionName
                  << "' subscribed to variable '" << key.varName << "'\n";
    } catch (const std::exception& e) {
        handleError("Subscribe Statement Error: " + std::string(e.what()));
    }
}


void Interpreter::interpretUnsubscribe(const UnsubscribeStatementNode& node) {
    try {
        // Get the VarID of the variable to unsubscribe from
         JTML::CompositeKey key = { currentEnv->instanceID, node.variableName };

        // Function name to unsubscribe
        std::string funcName = node.functionName;

        // Unsubscribe the function from the variable
        currentEnv->unsubscribeFunctionFromVariable(key, funcName);

        std::cout << "[UNSUBSCRIBE] Function '" << funcName
                  << "' unsubscribed from variable '" << node.variableName << "'\n";
    } catch (const std::exception& e) {
        handleError("Unsubscribe Statement Error: " + std::string(e.what()));
    }
}

void Interpreter::interpretFunctionDeclaration(const FunctionDeclarationNode& decl)
{
    std::cout << "[DEBUG] Interpreting FunctionDeclarationNode: " << decl.toString() << "\n";
    // 1) Clone body for safe storage in the new function
    std::vector<std::unique_ptr<ASTNode>> clonedBody;
    clonedBody.reserve(decl.body.size());
    for (const auto& stmt : decl.body) {
        clonedBody.push_back(stmt->clone());
    }

    // 2) Build a Function object
    auto newFunc = std::make_shared<JTML::Function>(
        decl.name,
        decl.parameters,
        decl.returnType,
        std::move(clonedBody),
        currentEnv,  // This environment is the 'closure'
        decl.isAsync
    );

    // 3) Define the function by name in the current environment
    JTML::CompositeKey funcKey = { currentEnv->instanceID, decl.name };
    currentEnv->defineFunction(funcKey, newFunc);

    std::cout << "[DEBUG] Defined function '" << decl.name << "' with ";
    std::cout << decl.parameters.size() << " parameters:\n";
    for (const auto& stmt :newFunc->body) {
        std::cout << stmt->toString() << ", ";
    }
    std::cout << "returning " << (decl.returnType) << "\n";


    // 4) For *nested* function declarations in the body, define them too
    //    but skip normal statements (like 'if', 'return', etc.).
    //    We'll interpret normal statements only at call time.
    collectAllNestedFunctions(decl.body, newFunc->closure);
}

void Interpreter::collectAllNestedFunctions(
    const std::vector<std::unique_ptr<ASTNode>>& stmts,
    const std::shared_ptr<JTML::Environment>& closureEnv
)
{
    for (const auto& stmt : stmts) {
        if (!stmt) continue;

        switch (stmt->getType()) {
        case ASTNodeType::FunctionDeclaration: {
            // We found a nested function
            auto& nestedFuncDecl = static_cast<const FunctionDeclarationNode&>(*stmt);

            // Temporarily switch environment to closureEnv
            auto oldEnv = currentEnv;
            currentEnv = closureEnv;

            // interpretNode(...) will call interpretFunctionDeclaration(...),
            // which creates & defines the nested function in closureEnv
            std::vector<std::unique_ptr<ASTNode>> clonedBody;
            clonedBody.reserve(nestedFuncDecl.body.size());
            for (const auto& stmt : nestedFuncDecl.body) {
                clonedBody.push_back(stmt->clone());
            }

            // 2) Build a Function object
            auto newFunc = std::make_shared<JTML::Function>(
                nestedFuncDecl.name,
                nestedFuncDecl.parameters,
                nestedFuncDecl.returnType,
                std::move(clonedBody),
                currentEnv,  // This environment is the 'closure'
                nestedFuncDecl.isAsync
            );

            JTML::CompositeKey funcKey = { currentEnv->instanceID, nestedFuncDecl.name };

            // 3) Define the function by name in the current environment
            currentEnv->defineFunction(funcKey, newFunc);
            currentEnv = oldEnv;
            break;
        }
        case ASTNodeType::IfStatement: {
            // We do NOT interpret the condition or run the blocks,
            // but we do gather function declarations inside then/else blocks
            auto& ifNode = static_cast<const IfStatementNode&>(*stmt);
            collectAllNestedFunctions(ifNode.thenStatements, closureEnv);
            collectAllNestedFunctions(ifNode.elseStatements, closureEnv);
            break;
        }
        case ASTNodeType::WhileStatement: {
            auto& whileNode = static_cast<const WhileStatementNode&>(*stmt);
            collectAllNestedFunctions(whileNode.body, closureEnv);
            break;
        }
        case ASTNodeType::ForStatement: {
            auto& forNode = static_cast<const ForStatementNode&>(*stmt);
            collectAllNestedFunctions(forNode.body, closureEnv);
            break;
        }
        case ASTNodeType::BlockStatement: {
            auto& blockNode = static_cast<const BlockStatementNode&>(*stmt);
            collectAllNestedFunctions(blockNode.statements, closureEnv);
            break;
        }
        // ... similarly handle TryExceptThen or others if they can contain child statements
        default:
            // Normal statements (Return, Show, etc.) => do nothing here
            break;
        }
    }
}

std::shared_ptr<JTML::VarValue> Interpreter::executeFunction(
    const std::shared_ptr<JTML::Function>& func,
    const std::vector<std::shared_ptr<JTML::VarValue>>& args,
    const std::shared_ptr<JTML::VarValue>& thisPtr,
    bool dispatchAsync
) {
    if (dispatchAsync && func->isAsync) {
        auto asyncFunc = func;
        auto asyncArgs = args;
        auto asyncThis = thisPtr;
        std::thread([this, asyncFunc, asyncArgs, asyncThis]() {
            try {
                executeFunction(asyncFunc, asyncArgs, asyncThis, false);
            } catch (const std::exception& e) {
                handleError("Async Function Execution Error: " + std::string(e.what()));
            }
        }).detach();
        return std::make_shared<JTML::VarValue>(JTML::ValueVariant{""});
    }

    // Check argument count
    if (args.size() != func->parameters.size()) {
        throw std::runtime_error("Function '" + func->name + "' expects " +
            std::to_string(func->parameters.size()) + " arguments, got " +
            std::to_string(args.size()));
    }

    // Manage function context
    bool previousContext = inFunctionContext;
    inFunctionContext = true;

    // Limit Recursion Depth
    static std::mutex recursionMutex;
    std::lock_guard<std::mutex> lock(recursionMutex);
    static int recursionDepth = 0;
    const int MAX_RECURSION_DEPTH = 1000;
    if (++recursionDepth > MAX_RECURSION_DEPTH) {
        throw std::runtime_error("Maximum recursion depth exceeded in function '" + func->name + "'");
    }

    // Create a new environment for the function execution
    auto funcEnv = std::make_shared<JTML::Environment>(
        func->closure, // Parent environment (closure)
        JTML::InstanceIDGenerator::getNextID(),
        currentEnv->renderer
    ); // Closure for accessing outer variables
    std::shared_ptr<JTML::Environment> previousEnv = currentEnv;
    currentEnv = funcEnv; // Update current environment

    // Bind parameters locally using JTML::CompositeKey
    for (size_t i = 0; i < func->parameters.size(); ++i) {
        const std::string& paramName = func->parameters[i].name;
        JTML::CompositeKey paramKey = { funcEnv->instanceID, paramName };
        funcEnv->defineVariable(paramKey, args[i], JTML::VarKind::Normal);
    }

    // Bind 'this' if applicable
    if (thisPtr) {
        JTML::CompositeKey thisKey = { funcEnv->instanceID, "this" };
        funcEnv->defineVariable(thisKey, thisPtr, JTML::VarKind::Normal);
    }

    // Debug: Print function environment variables
    std::cout << "[DEBUG] Function '" << func->name << "' environment (InstanceID: "
              << funcEnv->instanceID << ") variables:\n";
    for (const auto& [key, varInfo] : funcEnv->variables) {
        std::cout << "  " << funcEnv->getCompositeName(key) << " = "
                  << (varInfo->currentValue ? varInfo->currentValue->toString() : "undefined")
                  << "\n";
    }

    // Initialize return value
    std::shared_ptr<JTML::VarValue> returnValue = nullptr;

    try {
        // Interpret each statement in the function body
        for (const auto& stmt : func->body) {
            std::cout << "[DEBUG] Executing statement in function '" << func->name << "': "
                      << stmt->toString() << "\n";
            interpretNode(*stmt);
        }
    } catch (const ReturnException& re) {
        std::cout << "[DEBUG] Function '" << func->name << "' returned with value: "
                  << re.value->toString() << "\n";
        returnValue = re.value;
    } catch (const std::exception& e) {
        // Restore previous environment and context before handling the error
        currentEnv = previousEnv; // Restore previous environment
        inFunctionContext = previousContext;
        handleError("Function Execution Error: " + std::string(e.what()));
    }

    // Restore previous environment and context
    currentEnv = previousEnv; // Restore previous environment
    inFunctionContext = previousContext;

    // Debug: Print parent environment variables
    std::cout << "[DEBUG] Parent environment variables after function execution:\n";
    for (const auto& [key, varInfo] : currentEnv->variables) {
        std::cout << "  " << currentEnv->getCompositeName(key) << " = "
                  << (varInfo->currentValue ? varInfo->currentValue->toString() : "undefined")
                  << "\n";
    }

    --recursionDepth;

    // If no return statement, return a default value
    if (!returnValue) {
        return std::make_shared<JTML::VarValue>(JTML::ValueVariant{""});
    }

    return returnValue;
}

// Store a variable to a different scope
void Interpreter::storeVariable(const std::string& scope, const std::string& varName) {
    JTML::CompositeKey sourceKey = { currentEnv->instanceID, varName };
    auto value = currentEnv->getVariable(sourceKey);

    std::shared_ptr<JTML::Environment> targetEnv = nullptr;
    if (scope == "main") {
        targetEnv = globalEnv;
    } else if (scope == "current") {
        targetEnv = currentEnv;
    } else {
        throw std::runtime_error("Unsupported store scope '" + scope + "'. Supported scopes: main, current.");
    }

    JTML::CompositeKey targetKey = { targetEnv->instanceID, varName };
    targetEnv->setVariable(targetKey, value);
    std::cout << "[STORE] Variable '" << varName << "' stored to scope '" << scope << "'.\n";
}

std::shared_ptr<JTML::VarValue> Interpreter::instantiateClass(
    const ClassDeclarationNode& classNode,
    const std::vector<std::unique_ptr<ExpressionStatementNode>>& arguments,
    std::shared_ptr<JTML::Environment> parentEnv
) {
    // Create an environment for the object with a unique InstanceID
    auto objEnv = std::make_shared<JTML::Environment>(
        parentEnv,
        JTML::InstanceIDGenerator::getNextID(),
        currentEnv->renderer
    );

    std::shared_ptr<JTML::Function> constructorFunc = nullptr;

    auto previousEnv = currentEnv;
    currentEnv = objEnv;

    std::function<void(const ClassDeclarationNode&)> applyClassMembers =
        [&](const ClassDeclarationNode& node) {
            if (!node.parentName.empty()) {
                auto parentIt = classDeclarations.find(node.parentName);
                if (parentIt == classDeclarations.end()) {
                    throw std::runtime_error("Parent class '" + node.parentName + "' not found for '" + node.name + "'.");
                }
                applyClassMembers(*parentIt->second);
            }

            for (const auto& member : node.members) {
                if (member->getType() == ASTNodeType::DefineStatement) {
                    const auto& defNode = static_cast<const DefineStatementNode&>(*member);
                    JTML::CompositeKey propKey = { objEnv->instanceID, defNode.identifier };
                    auto value = defNode.expression
                        ? evaluateExpression(defNode.expression.get(), objEnv)
                        : std::make_shared<JTML::VarValue>();
                    if (defNode.isConst) {
                        objEnv->defineVariable(propKey, value, JTML::VarKind::Frozen);
                    } else {
                        objEnv->setVariable(propKey, value);
                    }
                } else if (member->getType() == ASTNodeType::DeriveStatement) {
                    const auto& deriveNode = static_cast<const DeriveStatementNode&>(*member);
                    JTML::CompositeKey propKey = { objEnv->instanceID, deriveNode.identifier };
                    std::vector<JTML::CompositeKey> deps;
                    gatherDeps(deriveNode.expression.get(), deps, objEnv);
                    auto evaluator = [this, objEnv](const ExpressionStatementNode* exprNode) {
                        return this->evaluateExpression(exprNode, objEnv);
                    };
                    objEnv->deriveVariable(propKey, deriveNode.expression->clone(), std::move(deps), evaluator);
                } else if (member->getType() == ASTNodeType::FunctionDeclaration) {
                    const auto& funcNode = static_cast<const FunctionDeclarationNode&>(*member);

                    std::vector<std::unique_ptr<ASTNode>> clonedBody;
                    clonedBody.reserve(funcNode.body.size());
                    for (const auto& stmt : funcNode.body) {
                        clonedBody.push_back(stmt->clone());
                    }

                    JTML::CompositeKey methodKey = { objEnv->instanceID, funcNode.name };
                    auto newFunc = std::make_shared<JTML::Function>(
                        funcNode.name,
                        funcNode.parameters,
                        funcNode.returnType,
                        std::move(clonedBody),
                        objEnv,
                        funcNode.isAsync
                    );

                    objEnv->defineFunction(methodKey, newFunc);
                    if (&node == &classNode && funcNode.name == "constructor") {
                        constructorFunc = newFunc;
                    }
                }
            }
        };

    try {
        applyClassMembers(classNode);
    } catch (...) {
        currentEnv = previousEnv;
        throw;
    }

    currentEnv = previousEnv;

    if (constructorFunc) {
        // Evaluate arguments
        std::vector<std::shared_ptr<JTML::VarValue>> argValues;
        for (const auto& argExpr : arguments) {
            argValues.push_back(evaluateExpression(argExpr.get(), objEnv));
        }

        // Construct JTML::CompositeKey for the constructor
        JTML::CompositeKey constructorKey = { objEnv->instanceID, "constructor" };

        // Retrieve the constructor function using JTML::CompositeKey
        auto retrievedConstructor = objEnv->getFunction(constructorKey);
        if (!retrievedConstructor) {
            throw std::runtime_error("Constructor function not found in class '" + classNode.name + "'.");
        }

        // "this" = the current object wrapped in VarValue
        auto thisVarValue = std::make_shared<JTML::VarValue>(JTML::ValueVariant{ JTML::ObjectHandle{ objEnv } });

        // Execute the constructor function with arguments and 'this' bound
        executeFunction(retrievedConstructor, argValues, thisVarValue);
    }

    // Return the object wrapped in a VarValue
    return std::make_shared<JTML::VarValue>(JTML::ValueVariant{ JTML::ObjectHandle{ objEnv } });
}

// ------------------- Expression Evaluation -------------------

bool Interpreter::evaluateCondition(const ExpressionStatementNode* condition, std::shared_ptr<JTML::Environment> env) {
    if (!condition) {
        throw std::runtime_error("Null condition encountered.");
    }

    std::shared_ptr<JTML::VarValue> condValPtr = evaluateExpression(condition, env);
    std::string condVal = getStringValue(condValPtr);

    // Use isTruthy for truthiness evaluation
    try {
        return isTruthy(condVal);
    } catch (const std::exception& e) {
        throw std::runtime_error("Error evaluating condition: " + std::string(e.what()));
    }
}

// (F) Evaluate an expression and return its string value
std::shared_ptr<JTML::VarValue> Interpreter::evaluateExpression(const ExpressionStatementNode* exprNode, std::shared_ptr<JTML::Environment> env) {

    if (!exprNode) {
        throw std::runtime_error("Null expression node encountered.");
    }

    switch (exprNode->getExprType()) {
        case ExpressionStatementNodeType::Binary: {
            const auto* binExpr = static_cast<const BinaryExpressionStatementNode*>(exprNode);
            std::shared_ptr<JTML::VarValue> leftVal  = evaluateExpression(binExpr->left.get(), env);
            std::shared_ptr<JTML::VarValue> rightVal = evaluateExpression(binExpr->right.get(), env);
            const std::string& op = binExpr->op;

            // For logical ops "&&" and "||", etc...
            if (op == "&&" || op == "||") {
                bool leftBool  = isTruthy(leftVal->toString());
                bool rightBool = isTruthy(rightVal->toString());
                if (op == "&&") {
                    return std::make_shared<JTML::VarValue>(leftBool && rightBool ? "true" : "false");
                }
                else {
                    // op == "||"
                    bool resultBool = (leftBool || rightBool);
                    return std::make_shared<JTML::VarValue>(resultBool ? "true" : "false");
                }
            }
             if (binExpr->op == "==" || binExpr->op == "!=" ||
                binExpr->op == "<"  || binExpr->op == "<=" ||
                binExpr->op == ">"  || binExpr->op == ">=" )
            {
                // Decide if it's numeric or string compare
                // 1) If both are numeric => numeric compare
                // 2) Else if both are string => string compare
                // 3) Else fallback or convert if you want

                if (leftVal->isNumber() && rightVal->isNumber()) {
                    double ln = leftVal->getNumber();
                    double rn = rightVal->getNumber();
                    return std::make_shared<JTML::VarValue>(JTML::ValueVariant{performNumericCompare(op, ln, rn) });
                }
                else if (leftVal->isString() && rightVal->isString()) {
                    const std::string& ls = leftVal->toString();
                    const std::string& rs = rightVal->toString();
                    return std::make_shared<JTML::VarValue>(JTML::ValueVariant{performStringCompare(op, ls, rs) });
                }
                else {
                    // If they differ in type, you can define a rule
                    // e.g., coerce everything to string or numeric?
                    // For demonstration, let's coerce to string:
                    std::string ls = leftVal->toString();
                    std::string rs = rightVal->toString();
                    std::cout << "Comparing variables of different types!"<< ls << " "<< rs << "\n";
                    return std::make_shared<JTML::VarValue>(JTML::ValueVariant{ performStringCompare(op, ls, rs)} );
                }
            }

            if (op == "+") {
                if (leftVal->isString() || rightVal->isString()) {
                    return std::make_shared<JTML::VarValue>(leftVal->toString() + rightVal->toString());
                }
                else if (leftVal->isNumber() && rightVal->isNumber()) {
                    double result = leftVal->getNumber() + rightVal->getNumber();
                    return std::make_shared<JTML::VarValue>(result);
                }
                else {
                    throw std::runtime_error("Invalid types for '+' operation");
                }
            }

            // (B) The operators -, *, /, %
           else if (op == "-" || op == "*" || op == "/" || op == "%") {
                if (!leftVal->isNumber() || !rightVal->isNumber()) {
                    throw std::runtime_error("Arithmetic operators require numeric types");
                }
                double ln = leftVal->getNumber();
                double rn = rightVal->getNumber();
                double result;

                if (op == "-") {
                    result = ln - rn;
                }
                else if (op == "*") {
                    result = ln * rn;
                }
                else if (op == "/") {
                    if (rn == 0.0) throw std::runtime_error("Division by zero");
                    result = ln / rn;
                }
                else { // op == "%"
                    int li = static_cast<int>(ln);
                    int ri = static_cast<int>(rn);
                    if (ri == 0) throw std::runtime_error("Modulo by zero");
                    result = static_cast<double>(li % ri);
                }

                return std::make_shared<JTML::VarValue>(result);
            }
            // If we get here => unrecognized op
            throw std::runtime_error("Unsupported binary operator: " + op);
        }

        case ExpressionStatementNodeType::Unary: {
            const auto* unaryExpr = static_cast<const UnaryExpressionStatementNode*>(exprNode);
            std::shared_ptr<JTML::VarValue> operandVal = evaluateExpression(unaryExpr->right.get(), env);
            const std::string& op = unaryExpr->op;

            if (op == "!") {
                // logical NOT => just interpret truthiness
                bool val = isTruthy(operandVal->toString());
                return std::make_shared<JTML::VarValue>(val ? "false" : "true");
            }
            else if (op == "-") {
                // numeric negation => warn or try convert
                if (!operandVal->isNumber()) {
                    std::cerr << "[Warning] Using unary '-' on a non-numeric value.\n";
                }
                double num = 0.0;
                try {
                    num = std::stod(operandVal->toString());
                } catch (...) {
                    throw std::runtime_error("Invalid numeric format in unary '-'");
                }
                double result = -num;
                return std::make_shared<JTML::VarValue>(std::to_string(result));
            }
            else {
                throw std::runtime_error("Unsupported unary operator: " + op);
            }
        }

      case ExpressionStatementNodeType::Variable: {
            const auto* varExpr = static_cast<const VariableExpressionStatementNode*>(exprNode);
            // Construct JTML::CompositeKey for the variable
            JTML::CompositeKey varKey = { env->instanceID, varExpr->name };
            auto varVal = env->getVariable(varKey);

            std::cout << "[EVAL] Variable " << env->getCompositeName(varKey) << " (InstanceID: " << varKey.instanceID
                      << ") = " << varVal->toString() << "\n";

            // Check if the variable is dirty and needs to be updated
            JTML::VarID varID = env->getVarID(varKey);

            if (env->dirtyVars.count(varID)) {
                updateVariable(varID, env);
            }

            return varVal;
        }

        case ExpressionStatementNodeType::StringLiteral: {
            const auto* strExpr = static_cast<const StringLiteralExpressionStatementNode*>(exprNode);
            return std::make_shared<JTML::VarValue>(JTML::ValueVariant{strExpr->value});
        }

        case ExpressionStatementNodeType::CompositeString: {
            const auto* composite = static_cast<const CompositeStringExpressionStatementNode*>(exprNode);
            std::string result;

            for (const auto& part : composite->parts) {
                result += evaluateExpression(part.get(), env)->toString();
            }

            return std::make_shared<JTML::VarValue>(JTML::ValueVariant(result));
        }

        case ExpressionStatementNodeType::EmbeddedVariable: {
            const auto* embExpr = static_cast<const EmbeddedVariableExpressionStatementNode*>(exprNode);
            return evaluateExpression(embExpr->embeddedExpression.get(), env);
        }

        case ExpressionStatementNodeType::NumberLiteral: {
            const auto* numExpr = static_cast<const NumberLiteralExpressionStatementNode*>(exprNode);
            return std::make_shared<JTML::VarValue>(JTML::ValueVariant{numExpr->value});
        }

        case ExpressionStatementNodeType::BooleanLiteral: {
            const auto* boolExpr = static_cast<const BooleanLiteralExpressionStatementNode*>(exprNode);
            return std::make_shared<JTML::VarValue>(JTML::ValueVariant{boolExpr->value ? "true" : "false"});
        }

        case ExpressionStatementNodeType::Conditional: {
            const auto* condExpr = static_cast<const ConditionalExpressionStatementNode*>(exprNode);
            auto condition = evaluateExpression(condExpr->condition.get(), env);
            if (isTruthy(condition ? condition->toString() : "")) {
                return evaluateExpression(condExpr->whenTrue.get(), env);
            }
            return evaluateExpression(condExpr->whenFalse.get(), env);
        }

        case ExpressionStatementNodeType::ArrayLiteral: {
            const auto* arrNode = static_cast<const ArrayLiteralExpressionStatementNode*>(exprNode);

            JTML::CompositeKey tempKey = {env->instanceID, "__temp__array_" + std::to_string(uniqueArrayVarID++)};

            auto array = std::make_shared<JTML::ReactiveArray>(env, tempKey);

            // Evaluate each element in the array literal and add to the reactive array

            for (const auto& elemExpr : arrNode->elements) {
                auto elemValue = evaluateExpression(elemExpr.get(), env);
                array->push(elemValue);
            }


            return std::make_shared<JTML::VarValue>(array);
        }

        case ExpressionStatementNodeType::DictionaryLiteral: {
            const auto* dictNode = static_cast<const DictionaryLiteralExpressionStatementNode*>(exprNode);

            // Use the variable name if already defined or create a new one
            JTML::CompositeKey tempKey = {env->instanceID, "__temp__dict_" + std::to_string(uniqueDictVarID++)};


            // Create the reactive dict using the defined or derived key
            auto dict = std::make_shared<JTML::ReactiveDict>(env, tempKey);


            // Evaluate each element in the dict literal and add to the reactive dict
            for (auto& entry : dictNode->entries) {
                std::string key = entry.key.text;
                auto value = evaluateExpression(entry.value.get(), env);
                if (!value) {
                    handleError("Dictionary value evaluation failed");
                    value = std::make_shared<JTML::VarValue>("<error>");
                }
                dict->set(key, value);
            }


            return std::make_shared<JTML::VarValue>(dict);
        }

        case ExpressionStatementNodeType::Subscript: {
            auto* subExpr = static_cast<const SubscriptExpressionStatementNode*>(exprNode);
            auto baseVal  = evaluateExpression(subExpr->base.get(), env);
            auto indexVal = evaluateExpression(subExpr->index.get(), env);

            if (baseVal->isArray()) {
                auto reactiveArray = baseVal->getArray();
                double idxNum = getNumericValue(indexVal);
                int idx = static_cast<int>(idxNum);

                if (idx < 0 || static_cast<size_t>(idx) >= reactiveArray->size()) {
                    throw std::runtime_error("Array index out of bounds: " + std::to_string(idx));
                }

                return reactiveArray->get(idx);
            }
            else if (baseVal->isDict()) {
                auto reactiveDict = baseVal->getDict();
                std::string key = getStringValue(indexVal);
                return reactiveDict->get(key);
            }
            else {
                throw std::runtime_error("Subscript on non-array/non-dict");
            }
        }

        case ExpressionStatementNodeType::FunctionCall: {
            const auto* callExpr = static_cast<const FunctionCallExpressionStatementNode*>(exprNode);

            // Check if the function name corresponds to a class for instantiation
            auto classIt = classDeclarations.find(callExpr->functionName);
            if (classIt != classDeclarations.end()) {
                // Instantiate the class
                return instantiateClass(*classIt->second, callExpr->arguments, env);
            }

            std::cout << "Function call: " << callExpr->toString() << "\n";

            if (!callExpr) {
                throw std::runtime_error("FunctionCallExpressionStatementNode is null.");
            }

            // Construct JTML::CompositeKey for the function
            JTML::CompositeKey funcKey = { env->instanceID, callExpr->functionName };
            auto func = env->getFunction(funcKey);
            if (!func) {
                throw std::runtime_error("Function '" + callExpr->functionName + "' not found in the current environment.");
            }

            // Evaluate arguments
            std::vector<std::shared_ptr<JTML::VarValue>> args;
            for (size_t i = 0; i < callExpr->arguments.size(); ++i) {
                if (!callExpr->arguments[i]) {
                    throw std::runtime_error("Null argument at index " + std::to_string(i) +
                                            " in function call to '" + callExpr->functionName + "'.");
                }
                args.push_back(evaluateExpression(callExpr->arguments[i].get(), env));
            }

            // Debug logging
            std::cout << "[DEBUG] Calling function: " << func->name << "\n";
            for (const auto& arg : args) {
                std::cout << "[DEBUG] Argument value: " << (arg ? arg->toString() : "null") << "\n";
            }

            // Display function body for debugging
            for (const auto& stmt : func->body) {
                std::cout << "[DEBUG] Body of function " << func->name << " value: " << stmt->toString() << "\n";
            }

            // Display current environment variables
            std::cout << "[DEBUG] Current environment before function call:\n";
            for (const auto& [key, varInfo] : env->variables) {
                std::cout << "  " << env->getCompositeName(key) << " = "
                          << (varInfo->currentValue ? varInfo->currentValue->toString() : "undefined")
                          << "\n";
            }

            // Display closure environment variables
            std::cout << "[DEBUG] Closure for function " << func->name << ":\n";
            for (const auto& [key, varInfo] : func->closure->variables) {
                std::cout << "  " << env->getCompositeName(key) << " = "
                          << (varInfo->currentValue ? varInfo->currentValue->toString() : "undefined")
                          << "\n";
            }

            // Execute the function
            try {
                return executeFunction(func, args, nullptr); // 'this' is nullptr for regular functions
            } catch (const std::exception& e) {
                throw std::runtime_error("Error during execution of function '" + callExpr->functionName + "': " + e.what());
            }
        }

        case ExpressionStatementNodeType::ObjectPropertyAccess: {
            const auto* propAccess = static_cast<const ObjectPropertyAccessExpressionNode*>(exprNode);

            std::shared_ptr<JTML::VarValue> baseVal = evaluateExpression(propAccess->base.get(), env);

            if (baseVal->isDict()) {
                auto dict = baseVal->getDict();
                auto value = dict->get(propAccess->propertyName);
                if (!value) {
                    return std::make_shared<JTML::VarValue>();
                }
                return value;
            }

            if (!baseVal->isObject()) {
                return std::make_shared<JTML::VarValue>();
            }

            // Retrieve the property from the object's environment
            auto objHandle = baseVal->getObjectHandle();
            JTML::CompositeKey propKey = { objHandle.instanceEnv->instanceID, propAccess->propertyName };
            auto propertyVal = objHandle.instanceEnv->getVariable(propKey);

            if (!propertyVal) {
                return std::make_shared<JTML::VarValue>();
            }

            std::cout << "[EVAL] Accessing property '" << propKey.varName << "' (InstanceID: " << propKey.instanceID
                      << ") = " << propertyVal->toString() << "\n";

            return propertyVal;
        }

        case ExpressionStatementNodeType::ObjectMethodCall: {
            const auto* methodCall = static_cast<const ObjectMethodCallExpressionNode*>(exprNode);

            // Evaluate the base object
            std::shared_ptr<JTML::VarValue> baseVal = evaluateExpression(methodCall->base.get(), env);

            if (baseVal->isArray()) {
                auto array = baseVal->getArray();
                const std::string& methodName = methodCall->methodName;

                // Evaluate the arguments
                std::vector<std::shared_ptr<JTML::VarValue>> args;
                for (const auto& argExpr : methodCall->arguments) {
                    args.push_back(evaluateExpression(argExpr.get(), env));
                }

                // Call the array method
                if (methodName == "push") {
                    if (args.size() != 1) {
                        throw std::runtime_error("push() expects exactly 1 argument.");
                    }
                    array->push(args[0]);
                    return std::make_shared<JTML::VarValue>(array->size()); // Return new length
                }
                else if (methodName == "pop") {
                    if (!args.empty()) {
                        throw std::runtime_error("pop() expects no arguments.");
                    }
                    auto poppedValue = array->pop();
                    return poppedValue; // Return the popped value
                }
                else if (methodName == "size") {
                    if (!args.empty()) {
                        throw std::runtime_error("size() expects no arguments.");
                    }
                    auto sizeValue = array->size();
                    return std::make_shared<JTML::VarValue>(sizeValue);
                }
                else {
                    throw std::runtime_error("Unsupported array method: " + methodName);
                }
            }

            if (!baseVal->isObject()) {
                throw std::runtime_error("Attempted to call a method on a non-object.");
            }

            // Retrieve the method from the object's environment
            auto objHandle = baseVal->getObjectHandle();
            JTML::CompositeKey methodKey = { objHandle.instanceEnv->instanceID, methodCall->methodName };
            auto methodFunc = objHandle.instanceEnv->getFunction(methodKey);

            if (!methodFunc) {
                throw std::runtime_error("Method '" + methodCall->methodName + "' not found in object.");
            }

            // Evaluate the arguments
            std::vector<std::shared_ptr<JTML::VarValue>> args;
            for (const auto& argExpr : methodCall->arguments) {
                args.push_back(evaluateExpression(argExpr.get(), env));
            }

            // Execute the method with 'this' bound to the object
            std::shared_ptr<JTML::VarValue> returnValue = executeFunction(methodFunc, args, baseVal);

            std::cout << "[EVAL] Executed method '" << methodCall->methodName << "' on object (InstanceID: "
                      << objHandle.instanceEnv->instanceID << ")\n";

            return returnValue;
        }

        default:
            throw std::runtime_error("Unknown ExpressionStatementNodeType encountered during evaluation.");
    }
}

bool Interpreter::isTruthy(const std::string& value) {
    // Boolean literals
    if (value == "true") return true;
    if (value == "false") return false;

    // Numeric values: Non-zero is truthy
    try {
        return std::stod(value) != 0.0;
    } catch (const std::invalid_argument&) {
        // Non-numeric, proceed to check as string
    }

    // Strings: Non-empty is truthy
    return !value.empty();
}

bool Interpreter::performNumericCompare(const std::string& op, double ln, double rn) {
    if (op == "==") return ln == rn;
    if (op == "!=") return ln != rn;
    if (op == "<")  return ln <  rn;
    if (op == "<=") return ln <= rn;
    if (op == ">")  return ln >  rn;
    if (op == ">=") return ln >= rn;
    throw std::runtime_error("Invalid numeric comparison operator: " + op);
}

bool Interpreter::performStringCompare(const std::string& op, const std::string& ls, const std::string& rs) {
    if (op == "==") return (ls == rs);
    if (op == "!=") return (ls != rs);
    if (op == "<")  return (ls <  rs);  // lexicographic
    if (op == "<=") return (ls <= rs);
    if (op == ">")  return (ls >  rs);
    if (op == ">=") return (ls >= rs);
    throw std::runtime_error("Invalid string comparison operator: " + op);
}

// (G) Gather dependencies from an expression
// Interpreter.cpp

void Interpreter::gatherDeps(
    const ExpressionStatementNode* exprNode,
    std::vector<JTML::CompositeKey>& out,
    std::shared_ptr<JTML::Environment> env
) {
    if (!exprNode) return;

    std::cout << "[DEBUG 0] gatherDeps Element: <" << exprNode->toString() << ">\n";

    switch (exprNode->getExprType()) {
        case ExpressionStatementNodeType::Variable: {
            const auto* varExpr = static_cast<const VariableExpressionStatementNode*>(exprNode);
            JTML::CompositeKey varKey = { env->instanceID, varExpr->name };

            try {
                auto varValue = env->getVariable(varKey);
                if (!varValue) {
                    throw std::runtime_error("Variable '" + varExpr->name + "' not found.");
                }

                // Add the variable itself as a dependency
                out.push_back(varKey);
                std::cout << "[GATHER_DEPS] Dependency found: "
                          << env->getCompositeName(varKey) << "\n";

                // Determine the VarKind and gather additional dependencies accordingly


            } catch (const std::exception& e) {
                handleError("Dependency Gathering Error: " + std::string(e.what()));
            }

            break;
        }

        case ExpressionStatementNodeType::Binary: {
            const auto* binExpr = static_cast<const BinaryExpressionStatementNode*>(exprNode);
            gatherDeps(binExpr->left.get(), out, env);
            gatherDeps(binExpr->right.get(), out, env);
            break;
        }

        case ExpressionStatementNodeType::Unary: {
            const auto* unaryExpr = static_cast<const UnaryExpressionStatementNode*>(exprNode);
            gatherDeps(unaryExpr->right.get(), out, env);
            break;
        }

        case ExpressionStatementNodeType::StringLiteral: {
            break;
        }

        case ExpressionStatementNodeType::CompositeString: {
            const auto* compositeExpr = static_cast<const CompositeStringExpressionStatementNode*>(exprNode);
            for (const auto& part : compositeExpr->parts) {
                gatherDeps(part.get(), out, env);
            }
            break;
        }

        case ExpressionStatementNodeType::EmbeddedVariable: {
            const auto* embeddedExpr = static_cast<const EmbeddedVariableExpressionStatementNode*>(exprNode);
            gatherDeps(embeddedExpr->embeddedExpression.get(), out, env);
            break;
        }

        case ExpressionStatementNodeType::NumberLiteral:
            // Literals have no dependencies
            break;

        case ExpressionStatementNodeType::ArrayLiteral: {
            const auto* arrNode = static_cast<const ArrayLiteralExpressionStatementNode*>(exprNode);

            // Iterate through each element in the array literal
            for (const auto& elemExpr : arrNode->elements) {
                // Recursively gather dependencies from each element
                gatherDeps(elemExpr.get(), out, env);
            }
            break;
        }

        case ExpressionStatementNodeType::DictionaryLiteral: {
            const auto* dictNode = static_cast<const DictionaryLiteralExpressionStatementNode*>(exprNode);

            // Iterate through each entry in the dictionary
            for (const auto& entry : dictNode->entries) {
                // Gather dependencies from the value expressions
                gatherDeps(entry.value.get(), out, env);
            }

            // Keys are usually static (strings/identifiers) and don't add to dependencies
            break;
        }

        case ExpressionStatementNodeType::Subscript: {
            auto* sub = static_cast<const SubscriptExpressionStatementNode*>(exprNode);

            // First, gather dependencies from the base
            gatherDeps(sub->base.get(), out, env);

            // Then gather the index or key
            gatherDeps(sub->index.get(), out, env);

            // Add dependency for the full array or dictionary and specific element/key
            if (sub->base->getExprType() == ExpressionStatementNodeType::Variable) {
                const auto* varNode = static_cast<const VariableExpressionStatementNode*>(sub->base.get());
                JTML::CompositeKey fullArrayKey = { env->instanceID, varNode->name };
                out.emplace_back(fullArrayKey);
                std::cout << "[GATHER_DEPS] Dependency found: "
                          << env->getCompositeName(fullArrayKey) << "\n";

                // Specific element dependency
                // Construct a unique key for the subscripted element, e.g., "a.array_1[0]"
                std::string specificName = varNode->name + "["; // Start of subscript
                if (sub->index->getExprType() == ExpressionStatementNodeType::NumberLiteral) {
                    const auto* indexLiteral = static_cast<const NumberLiteralExpressionStatementNode*>(sub->index.get());
                    specificName += std::to_string(static_cast<int>(indexLiteral->value)) + "]";
                } else if (sub->index->getExprType() == ExpressionStatementNodeType::StringLiteral) {
                    const auto* indexLiteral = static_cast<const StringLiteralExpressionStatementNode*>(sub->index.get());
                    specificName += indexLiteral->value + "]";
                } else {
                    // Handle other index types if necessary
                    specificName += "unknown]";
                }
                JTML::CompositeKey specificKey = { env->instanceID, specificName };
                out.emplace_back(specificKey);
                std::cout << "[GATHER_DEPS] Specific Subscript Dependency added: "
                          << env->getCompositeName(specificKey) << "\n";
            }
            break;
        }

        case ExpressionStatementNodeType::ObjectPropertyAccess: {
            const auto* prop = static_cast<const ObjectPropertyAccessExpressionNode*>(exprNode);
            gatherDeps(prop->base.get(), out, env);
            if (prop->base->getExprType() == ExpressionStatementNodeType::Variable) {
                const auto* varNode = static_cast<const VariableExpressionStatementNode*>(prop->base.get());
                JTML::CompositeKey objectKey = { env->instanceID, varNode->name };
                out.emplace_back(objectKey);
                std::cout << "[GATHER_DEPS] Object property dependency found: "
                          << env->getCompositeName(objectKey) << "\n";
            }
            break;
        }

        default:
            // Handle other expression types as needed
            break;
    }
}


// ------------------- Batch Updates -------------------


// ------------------- Cycle Detection -------------------

// (E) Detect cycle using DFS

void Interpreter::recalcDirty(std::shared_ptr<JTML::Environment> env) {
    env->recalcDirty([this, &env](JTML::VarID varID) {
        this->updateVariable(varID, env);
    });
}

void Interpreter::recalcAllDirty() {
    if (globalEnv) recalcDirty(globalEnv);
    for (auto& instance : componentInstances) {
        if (instance.environment) recalcDirty(instance.environment);
    }
}

void Interpreter::updateVariable(JTML::VarID varID, std::shared_ptr<JTML::Environment> env) {

    JTML::CompositeKey key = env->idToKey[varID];
    auto it = env->variables.find(key);
    if (it == env->variables.end()) {
        for (auto& instance : componentInstances) {
            if (!instance.environment || instance.environment->instanceID != key.instanceID) continue;
            auto targetIt = instance.environment->variables.find(key);
            if (targetIt != instance.environment->variables.end()) {
                JTML::VarID targetVarID = instance.environment->getVarID(key);
                updateVariable(targetVarID, instance.environment);
                return;
            }
        }
        handleError("Attempted to update undefined variable '" + env->getCompositeName(key) +
            "' in InstanceID " + std::to_string(env->instanceID));
        return;
    }

    if (it->second->kind == JTML::VarKind::Derived && it->second->expression) {
        try {
            std::shared_ptr<JTML::VarValue> newValue = evaluateExpression(it->second->expression.get(), env);
            std::cout << "[UPDATE] Evaluated " << key.varName << " = " << newValue->toString() << "\n";
            if (getStringValue(newValue) != getStringValue(it->second->currentValue)) {
                it->second->currentValue = newValue;
                std::cout << "[UPDATE] " << key.varName << " updated to " << newValue->toString() << "\n";
                // Emit events after value is updated and consistent (topological order guaranteed by recalcDirty)
                env->emitEvents(varID);
                // Note: Do NOT mark dependents dirty here - they are already included in the topological sort
            }
        }
        catch (const std::exception& e) {
            handleError("Error updating derived variable '" + key.varName + "': " + e.what());
        }
    }
    else {
        std::cout << "[SKIP] Normal variable '" << key.varName << "' does not require updates.\n";
    }
    // Normal variables do not require updates
}


double Interpreter::getNumericValue(const std::shared_ptr<JTML::VarValue>& valPtr) {
    if (!valPtr) throw std::runtime_error("Null VarValue");
    if (valPtr->isNumber()) {
        return valPtr->getNumber();
    }
    else if (valPtr->isBool()) {
        return valPtr->getBool() ? 1.0 : 0.0;
    }
    else if (valPtr->isString()) {
        // attempt to parse the string as double
        const auto& s = valPtr->getString();
        try {
            return std::stod(s);
        } catch (...) {
            throw std::runtime_error("Cannot convert string to number: " + s);
        }
    }
    // If it's array or dict, throw
    throw std::runtime_error("Cannot convert array/dict to number");
}

std::string Interpreter::getStringValue(const std::shared_ptr<JTML::VarValue>& valPtr) {
    if (!valPtr) throw std::runtime_error("Null VarValue");
    if (valPtr->isString()) {
        return valPtr->getString();
    }
    else if (valPtr->isNumber()) {
        double d = valPtr->getNumber();
        std::ostringstream oss; oss << d;
        return oss.str();
    }
    else if (valPtr->isBool()) {
        return valPtr->getBool() ? "true" : "false";
    }
    else if (valPtr->isArray()) {
        // convert array to string e.g. "[...]"
        // or you can do a custom print
        auto& arr = valPtr->getArray()->getArrayData();
        std::ostringstream oss;
        oss << "[";
        for (size_t i = 0; i < arr.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << getStringValue(arr[i]);
        }
        oss << "]";
        return oss.str();
    }
    else if (valPtr->isDict()) {
        auto& dict = valPtr->getDict()->getDictData();
        std::ostringstream oss;
        oss << "{";
        bool first = true;
        for (auto& kv : dict) {
            if (!first) oss << ", ";
            first = false;
            oss << "\"" << kv.first << "\": " << getStringValue(kv.second);
        }
        oss << "}";
        return oss.str();
    }
    throw std::runtime_error("Unknown type in getStringValue");
}


// ------------------- Error Handling -------------------

// Handle and report errors
void Interpreter::handleError(const std::string& message) {
    std::cerr << "Interpreter Error: " << message << "\n";
    // Depending on requirements, you might throw exceptions or handle errors differently
}


std::string toString(ASTNodeType nodeType) {
    switch (nodeType) {
        case ASTNodeType::DefineStatement: return "DefineStatement";
        case ASTNodeType::DeriveStatement: return "DeriveStatement";
        case ASTNodeType::UnbindStatement: return "UnbindStatement";
        case ASTNodeType::StoreStatement: return "StoreStatement";
        case ASTNodeType::ImportStatement: return "ImportStatement";
        case ASTNodeType::ThrowStatement: return "ThrowStatement";
        case ASTNodeType::ShowStatement: return "ShowStatement";
        case ASTNodeType::AssignmentStatement: return "AssignmentStatement";
        case ASTNodeType::ExpressionStatement: return "ExpressionStatement";
        case ASTNodeType::FunctionDeclaration: return "FunctionDeclaration";
        case ASTNodeType::ClassDeclaration: return "ClassDeclaration";
        case ASTNodeType::ReturnStatement: return "ReturnStatement";
        case ASTNodeType::BreakStatement: return "BreakStatement";
        case ASTNodeType::ContinueStatement: return "ContinueStatement";
        case ASTNodeType::SubscribeStatement: return "SubscribeStatement";
        case ASTNodeType::IfStatement: return "IfStatement";
        case ASTNodeType::WhileStatement: return "WhileStatement";
        case ASTNodeType::ForStatement: return "ForStatement";
        case ASTNodeType::BlockStatement: return "BlockStatement";
        case ASTNodeType::TryExceptThen: return "TryExceptThen";
        case ASTNodeType::JtmlElement: return "JtmlElement";
        // Add other ASTNodeTypes here as needed
        default: return "Unknown";
    }
}
