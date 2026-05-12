#define JTML_C_API_BUILD

#include "jtml/c_api.h"

#include "jtml/friendly.h"
#include "jtml/interpreter.h"
#include "jtml/lexer.h"
#include "jtml/parser.h"
#include "jtml/transpiler.h"
#include "json.hpp"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#ifndef JTML_VERSION_STRING
#define JTML_VERSION_STRING "0.0.0"
#endif

#ifndef JTML_VERSION_SUFFIX_STRING
#define JTML_VERSION_SUFFIX_STRING "dev"
#endif

struct jtml_context {
    std::unique_ptr<JtmlTranspiler> transpiler;
    std::unique_ptr<Interpreter> interpreter;
    std::string lastError;
    std::string lastHtml;
};

namespace {
char* copyString(const std::string& value) {
    auto* out = static_cast<char*>(std::malloc(value.size() + 1));
    if (!out) return nullptr;
    std::memcpy(out, value.c_str(), value.size() + 1);
    return out;
}

void setOut(char** target, const std::string& value) {
    if (target) *target = copyString(value);
}

std::string joinErrors(const std::vector<std::string>& errors) {
    std::ostringstream out;
    for (size_t i = 0; i < errors.size(); ++i) {
        if (i > 0) out << "\n";
        out << errors[i];
    }
    return out.str();
}

bool compileSource(const char* source,
                   JtmlTranspiler& transpiler,
                   std::vector<std::unique_ptr<ASTNode>>& program,
                   std::string& html,
                   std::string& error) {
    if (!source) {
        error = "source is null";
        return false;
    }

    try {
        const std::string classic = jtml::normalizeSourceSyntax(source);
        Lexer lexer(classic);
        auto tokens = lexer.tokenize();
        if (!lexer.getErrors().empty()) {
            error = joinErrors(lexer.getErrors());
            return false;
        }

        Parser parser(std::move(tokens));
        program = parser.parseProgram();
        if (!parser.getErrors().empty()) {
            error = joinErrors(parser.getErrors());
            return false;
        }

        html = transpiler.transpile(program);
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

nlohmann::json parseArgs(const char* argsJson) {
    if (!argsJson || std::strlen(argsJson) == 0) return nlohmann::json::array();
    auto parsed = nlohmann::json::parse(argsJson);
    if (!parsed.is_array()) {
        throw std::runtime_error("args_json must be a JSON array");
    }
    return parsed;
}

int fail(jtml_context* ctx, const std::string& error, char** errorOut) {
    if (ctx) ctx->lastError = error;
    setOut(errorOut, error);
    return 0;
}
}

const char* jtml_abi_version(void) {
    static const std::string version = [] {
        std::string value = JTML_VERSION_STRING;
        std::string suffix = JTML_VERSION_SUFFIX_STRING;
        if (!suffix.empty() && suffix != "release") value += "-" + suffix;
        return value;
    }();
    return version.c_str();
}

void jtml_free(char* value) {
    std::free(value);
}

jtml_context* jtml_create(void) {
    auto* ctx = new jtml_context();
    ctx->transpiler = std::make_unique<JtmlTranspiler>();
    return ctx;
}

void jtml_destroy(jtml_context* ctx) {
    delete ctx;
}

const char* jtml_last_error(jtml_context* ctx) {
    if (!ctx) return "context is null";
    return ctx->lastError.c_str();
}

int jtml_render(const char* source, char** html_out, char** error_out) {
    if (html_out) *html_out = nullptr;
    if (error_out) *error_out = nullptr;

    JtmlTranspiler transpiler;
    std::vector<std::unique_ptr<ASTNode>> program;
    std::string html;
    std::string error;
    if (!compileSource(source, transpiler, program, html, error)) {
        setOut(error_out, error);
        return 0;
    }

    setOut(html_out, html);
    return 1;
}

int jtml_load(jtml_context* ctx, const char* source, char** html_out, char** error_out) {
    if (html_out) *html_out = nullptr;
    if (error_out) *error_out = nullptr;
    if (!ctx) return fail(nullptr, "context is null", error_out);

    auto transpiler = std::make_unique<JtmlTranspiler>();
    std::vector<std::unique_ptr<ASTNode>> program;
    std::string html;
    std::string error;
    if (!compileSource(source, *transpiler, program, html, error)) {
        return fail(ctx, error, error_out);
    }

    InterpreterConfig config;
    config.startWebSocket = false;
    auto interpreter = std::make_unique<Interpreter>(*transpiler, config);
    try {
        interpreter->interpret(program);
    } catch (const std::exception& e) {
        return fail(ctx, e.what(), error_out);
    }

    ctx->transpiler = std::move(transpiler);
    ctx->interpreter = std::move(interpreter);
    ctx->lastHtml = html;
    ctx->lastError.clear();
    setOut(html_out, html);
    return 1;
}

char* jtml_bindings(jtml_context* ctx) {
    if (!ctx || !ctx->interpreter) return copyString("{}");
    return copyString(ctx->interpreter->getBindingsJSON());
}

char* jtml_state(jtml_context* ctx) {
    if (!ctx || !ctx->interpreter) return copyString("{}");
    return copyString(ctx->interpreter->getStateJSON());
}

char* jtml_components(jtml_context* ctx) {
    if (!ctx || !ctx->interpreter) return copyString("[]");
    return copyString(ctx->interpreter->getComponentsJSON());
}

char* jtml_component_definitions(jtml_context* ctx) {
    if (!ctx || !ctx->interpreter) return copyString("[]");
    return copyString(ctx->interpreter->getComponentDefinitionsJSON());
}

int jtml_dispatch(jtml_context* ctx,
                  const char* element_id,
                  const char* event_type,
                  const char* args_json,
                  char** bindings_out,
                  char** error_out) {
    if (bindings_out) *bindings_out = nullptr;
    if (error_out) *error_out = nullptr;
    if (!ctx) return fail(nullptr, "context is null", error_out);
    if (!ctx->interpreter) return fail(ctx, "no program loaded", error_out);
    if (!element_id || !event_type) return fail(ctx, "element_id and event_type are required", error_out);

    try {
        std::string bindings;
        std::string error;
        const bool ok = ctx->interpreter->dispatchEvent(
            element_id,
            event_type,
            parseArgs(args_json),
            bindings,
            error);
        if (!ok) return fail(ctx, error, error_out);
        ctx->lastError.clear();
        setOut(bindings_out, bindings);
        return 1;
    } catch (const std::exception& e) {
        return fail(ctx, e.what(), error_out);
    }
}

int jtml_component_action(jtml_context* ctx,
                          const char* component_id,
                          const char* action_name,
                          const char* args_json,
                          char** bindings_out,
                          char** error_out) {
    if (bindings_out) *bindings_out = nullptr;
    if (error_out) *error_out = nullptr;
    if (!ctx) return fail(nullptr, "context is null", error_out);
    if (!ctx->interpreter) return fail(ctx, "no program loaded", error_out);
    if (!component_id || !action_name) return fail(ctx, "component_id and action_name are required", error_out);

    try {
        std::string bindings;
        std::string error;
        const bool ok = ctx->interpreter->dispatchComponentAction(
            component_id,
            action_name,
            parseArgs(args_json),
            bindings,
            error);
        if (!ok) return fail(ctx, error, error_out);
        ctx->lastError.clear();
        setOut(bindings_out, bindings);
        return 1;
    } catch (const std::exception& e) {
        return fail(ctx, e.what(), error_out);
    }
}
