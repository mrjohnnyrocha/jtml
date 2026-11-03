#include "browser_build_policy.h"
#include "jtml/browser_runtime_emitter.h"
#include "jtml/expression_ir.h"

#include <algorithm>
#include <functional>

#include <filesystem>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace jtml::cli {
namespace {

bool isHiddenOrNonPublicSegment(const std::filesystem::path& segment) {
    const auto name = segment.filename().string();
    if (name.empty()) return false;
    if (name[0] == '.') return true;
    if (name == "node_modules" || name == "build" || name == "dist" ||
        name == "cmake-build-debug" || name == "cmake-build-release" ||
        name == "jtml_modules" || name == "target") {
        return true;
    }
    return false;
}

// Certify only getters actually consumed by root action expressions. Ownership
// comes from retained module plans/import identities, never the linked name map.
void validateActionGetReads(const jtml::RuntimeProjectPlan& project) {
    using Module = jtml::RuntimeModulePlan;
    using Binding = jtml::RuntimePlanBinding;
    struct Resolved { const Module* owner; const Binding* binding; bool derived; };
    std::map<jtml::SemanticModuleId, const Module*> modules;
    std::set<std::string> derivedNames, valueNames;
    for (const auto& module : project.modules) if (module.clientExecutable) {
        modules[module.id] = &module;
        for (const auto& binding : module.plan.state) valueNames.insert(binding.name);
        for (const auto& binding : module.plan.derived) { derivedNames.insert(binding.name); valueNames.insert(binding.name); }
    }
    auto fail = [](const Module& module, const SourceSpan& span, const std::string& reason) {
        std::string at = module.path;
        if (span.original) {
            const auto& origin = *span.original;
            const auto pos = origin.source->coordinates(origin.begin);
            at = origin.source->file + ":" + std::to_string(pos.first) + ":" + std::to_string(pos.second);
        }
        throw std::runtime_error(at + ": action-local get read is unsupported: " + reason +
            "; use an owned, acyclic structured scalar formula");
    };
    auto local = [](const Module& module, const std::string& name) -> Resolved {
        for (const auto& binding : module.plan.state)
            if (binding.name == name) return {&module, &binding, false};
        for (const auto& binding : module.plan.derived)
            if (binding.name == name) return {&module, &binding, true};
        return {nullptr, nullptr, false};
    };
    auto resolve = [&](const Module& module, const std::string& name) -> Resolved {
        const auto own = local(module, name);
        if (own.binding) return own;
        for (const auto& symbol : module.importedSymbols) if (symbol.name == name &&
            (symbol.kind == "let" || symbol.kind == "const" || symbol.kind == "get")) {
            const auto found = modules.find(symbol.module);
            if (found != modules.end()) {
                const auto target = local(*found->second, name);
                if (target.binding && target.derived == (symbol.kind == "get")) return target;
            }
        }
        return {nullptr, nullptr, false};
    };
    std::set<std::pair<jtml::SemanticModuleId, std::string>> active, complete;
    std::function<void(Resolved)> prove = [&](Resolved resolved) {
        const auto& module = *resolved.owner;
        const auto& binding = *resolved.binding;
        if (binding.name == "__proto__") fail(module, binding.sourceSpan, "reserved storage binding `__proto__`");
        if (std::count_if(module.plan.derived.begin(), module.plan.derived.end(), [&](const auto& b) { return b.name == binding.name; }) != 1)
            fail(module, binding.sourceSpan, "ambiguous get `" + binding.name + "`");
        const auto key = std::make_pair(module.id, binding.name);
        if (complete.count(key)) return;
        if (!active.insert(key).second) fail(module, binding.sourceSpan, "cycle involving `" + binding.name + "`");
        const auto ir = jtml::parseExpressionIr(binding.expr);
        if (!ir || !ir->supported || !jtml::expressionIrSupportsDirectJs(*ir) ||
            ir->valueType == jtml::ExpressionValueType::Array ||
            ir->valueType == jtml::ExpressionValueType::Object ||
            ir->valueType == jtml::ExpressionValueType::Null)
            fail(module, binding.sourceSpan, "get `" + binding.name + "` requires a supported scalar expression plan");
        for (const auto& dependency : jtml::expressionIrDependencies(*ir)) {
            const auto root = dependency.substr(0, dependency.find_first_of(".["));
            const auto target = resolve(module, root);
            if (!target.binding || root == "__proto__") fail(module, binding.sourceSpan, "unresolved, reserved or unimported binding `" + root + "`");
            if (!target.derived && derivedNames.count(root)) fail(module, binding.sourceSpan, "ambiguous value/get `" + root + "`");
            if (target.derived) prove(target);
        }
        active.erase(key);
        complete.insert(key);
    };
    for (const auto& entry : modules) {
        const auto& module = *entry.second;
        for (const auto& action : module.plan.actions) {
            bool consumesGet = false;
            auto inspect = [&](const std::string& expression) {
                const auto ir = jtml::parseExpressionIr(expression);
                if (!ir) return;
                for (const auto& dependency : jtml::expressionIrDependencies(*ir)) {
                    const auto root = dependency.substr(0, dependency.find_first_of(".["));
                    if (std::find(action.params.begin(), action.params.end(), root) != action.params.end() ||
                        !derivedNames.count(root)) continue;
                    const auto target = resolve(module, root);
                    if (!target.binding) fail(module, action.sourceSpan, "unimported get `" + root + "`");
                    if (!target.derived) fail(module, action.sourceSpan, "ambiguous value/get `" + root + "`");
                    consumesGet = true;
                    prove(target);
                }
            };
            std::function<void(const std::vector<jtml::RuntimePlanStatement>&)> walk = [&](const auto& statements) {
                for (const auto& statement : statements) {
                    inspect(statement.expr);
                    inspect(statement.condition);
                    if (statement.lhs.find('[') != std::string::npos) inspect(statement.lhs);
                    walk(statement.thenStatements);
                    walk(statement.elseStatements);
                }
            };
            walk(action.body);
            if (!consumesGet) continue;
            if (!module.ast) fail(module, action.sourceSpan, "missing retained action binding provenance");
            std::function<void(const std::vector<std::unique_ptr<ASTNode>>&)> locals = [&](const auto& nodes) {
                for (const auto& node : nodes) {
                    std::string name;
                    if (node->getType() == ASTNodeType::DefineStatement)
                        name = static_cast<const DefineStatementNode&>(*node).identifier;
                    if (node->getType() == ASTNodeType::AssignmentStatement) {
                        const auto& write = static_cast<const AssignmentStatementNode&>(*node);
                        if (write.localDeclaration && write.lhs->getExprType() == ExpressionStatementNodeType::Variable)
                            name = static_cast<const VariableExpressionStatementNode&>(*write.lhs).name;
                    }
                    if (valueNames.count(name)) fail(module, node->sourceSpan, "local declaration shadows root storage `" + name + "`");
                    if (node->getType() == ASTNodeType::IfStatement) {
                        const auto& branch = static_cast<const IfStatementNode&>(*node);
                        locals(branch.thenStatements); locals(branch.elseStatements);
                    }
                }
            };
            for (const auto& node : module.ast->nodes)
                if (node->getType() == ASTNodeType::FunctionDeclaration &&
                    static_cast<const FunctionDeclarationNode&>(*node).name == action.name)
                    locals(static_cast<const FunctionDeclarationNode&>(*node).body);
        }
    }
}

} // namespace

BrowserBuildMode browserBuildModeFromOptions(const Options& o) {
    const int requestedModes = (o.prod ? 1 : 0) + (o.compat ? 1 : 0) + (o.debug ? 1 : 0);
    if (requestedModes > 1) {
        throw std::runtime_error(
            "browser build modes are mutually exclusive; choose exactly one of --prod, --compat, or --debug");
    }
    if (o.compat) return BrowserBuildMode::Compatibility;
    if (o.debug) return BrowserBuildMode::Debug;
    return BrowserBuildMode::Production;
}

const char* browserBuildModeName(BrowserBuildMode mode) {
    switch (mode) {
        case BrowserBuildMode::Production: return "prod";
        case BrowserBuildMode::Compatibility: return "compat";
        case BrowserBuildMode::Debug: return "debug";
    }
    return "prod";
}

bool browserBuildModeEmitsCompatibilityAsset(BrowserBuildMode mode) {
    return mode == BrowserBuildMode::Compatibility || mode == BrowserBuildMode::Debug;
}

bool browserBuildModeAllowsImportStubs(BrowserBuildMode mode) {
    return mode == BrowserBuildMode::Compatibility || mode == BrowserBuildMode::Debug;
}

std::string emitBrowserBuildAppBootstrap(BrowserBuildMode mode) {
    const bool compatibility = browserBuildModeEmitsCompatibilityAsset(mode);
    const bool debug = mode == BrowserBuildMode::Debug;
    std::ostringstream out;
    out
        << "/* Generated by jtml build --target browser. */\n"
        << "(function(){\n"
        << "  window.jtml = window.jtml || {};\n"
        << "  window.jtml.productionAssets = Object.assign({}, window.jtml.productionAssets || {}, {\n"
        << "    runtime: './jtml-runtime.js',\n"
        << "    components: './components/index.js',\n"
        << "    app: './app.js'";
    if (compatibility) {
        out << ",\n"
            << "    compatibilityUpdatePlans: './jtml-update-plans.js'";
    }
    out
        << "\n"
        << "  });\n"
        << "  window.jtml.browserBuild = {\n"
        << "    version: 1,\n"
        << "    mode: '" << browserBuildModeName(mode) << "',\n"
        << "    cleanProduction: " << (mode == BrowserBuildMode::Production ? "true" : "false") << ",\n"
        << "    compatibility: " << (compatibility ? "true" : "false") << ",\n"
        << "    debug: " << (debug ? "true" : "false") << ",\n"
        << "    assets: window.jtml.productionAssets,\n"
        << "    diagnostics: {\n"
        << "      sourceSpans: " << (debug ? "true" : "false") << ",\n"
        << "      fallbackTelemetry: " << (debug ? "true" : "false") << "\n"
        << "    }\n"
        << "  };\n"
        << "  window.jtml.productionMode = window.jtml.browserBuild.mode;\n"
        << "}());\n";
    return out.str() + jtml::emitBrowserRootEventBindings();
}

// Validate retained executable module records, never the duplicate linked projection.
// Values share clientState; actions and fetch refresh aliases share event dispatch.
void validateBrowserRuntimeBindingOwnership(const jtml::RuntimeProjectPlan& project) {
    struct Owner {
        jtml::SemanticModuleId module;
        std::string category;
        std::string location;
    };
    std::map<std::pair<std::string, std::string>, Owner> owners;
    std::vector<std::string> issues;
    for (const auto& module : project.modules) {
        if (!module.clientExecutable) continue;
        auto add = [&](const std::string& space, const std::string& name,
                       const std::string& category, const SourceSpan& span) {
            if (name.empty()) return;
            std::string location = module.path;
            if (span.original) {
                const auto& origin = *span.original;
                const auto position = origin.source->coordinates(origin.begin);
                location = origin.source->file + ":" + std::to_string(position.first) +
                    ":" + std::to_string(position.second);
            }
            const Owner current{module.id, category, location};
            const auto inserted = owners.emplace(std::make_pair(space, name), current);
            const auto& previous = inserted.first->second;
            if (!inserted.second && previous.module != module.id) {
                issues.push_back("browser runtime binding collision `" + name + "` (" +
                    previous.category + " at " + previous.location + "; " + category +
                    " at " + location + "). Distinct executable modules share the " +
                    space + " namespace. Give these declarations distinct names and update "
                    "their references; module-scoped browser storage is unsupported for this shape.");
            }
        };
        for (const auto& binding : module.plan.state)
            add("value", binding.name, "state", binding.sourceSpan);
        for (const auto& binding : module.plan.derived)
            add("value", binding.name, "derived", binding.sourceSpan);
        for (const auto& action : module.plan.actions)
            add("action", action.name, "action", action.sourceSpan);
        for (const auto& fetch : module.plan.fetches) {
            add("value", fetch.name, "fetch", {});
            add("action", fetch.refreshAction, "fetch refresh action", {});
        }
    }
    if (!issues.empty()) {
        std::ostringstream message;
        message << "Cannot emit clean browser production assets:\n";
        for (const auto& issue : issues) message << "- " << issue << "\n";
        if (!project.scalarStorageIssue.empty()) message << "Scalar isolation was not proved: " << project.scalarStorageIssue << "\n";
        throw std::runtime_error(message.str());
    }
    validateActionGetReads(project);
}

bool shouldCopyBrowserBuildAsset(const std::filesystem::path& rel) {
    const auto normalized = rel.lexically_normal();
    for (const auto& part : normalized) {
        if (isHiddenOrNonPublicSegment(part)) return false;
    }
    const auto filename = normalized.filename().string();
    if (filename == ".env" || filename.rfind(".env.", 0) == 0 ||
        filename == "jtml.lock" || filename == "package-lock.json") {
        return false;
    }

    auto it = normalized.begin();
    if (it != normalized.end()) {
        const auto first = it->string();
        if (first == "public" || first == "assets") return true;
    }

    if (std::distance(normalized.begin(), normalized.end()) == 1) {
        static const std::set<std::string> rootAssets = {
            "favicon.ico", "robots.txt", "sitemap.xml",
            "manifest.json", "site.webmanifest", "browserconfig.xml",
            "humans.txt",
        };
        return rootAssets.count(filename) > 0;
    }
    return false;
}

} // namespace jtml::cli
