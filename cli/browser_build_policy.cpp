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

// Traverse retained import edges, then preserve each owner's declaration vector.
std::vector<std::string> planBrowserStateOrder(const jtml::RuntimeProjectPlan& project) {
    using Module = jtml::RuntimeModulePlan;
    std::map<jtml::SemanticModuleId, const Module*> modules;
    std::map<jtml::SemanticModuleId, std::map<std::string, std::vector<size_t>>> positions;
    std::set<std::string> owned;
    for (const auto& module : project.modules) {
        modules[module.id] = &module;
        if (!module.clientExecutable) continue;
        auto& local = positions[module.id];
        for (size_t i = 0; i < module.plan.state.size(); ++i) {
            local[module.plan.state[i].name].push_back(i);
            owned.insert(module.plan.state[i].name);
        }
    }
    std::map<jtml::SemanticModuleId, size_t> ranks;
    std::set<jtml::SemanticModuleId> visiting;
    std::vector<const Module*> orderedModules;
    std::function<void(const Module&)> visit = [&](const Module& module) {
        if (ranks.count(module.id)) return;
        if (!visiting.insert(module.id).second)
            throw std::runtime_error(module.path + ": E_MODULE_CYCLE: cyclic browser initialization import");
        for (const auto id : module.initializationImports) {
            const auto dependency = modules.find(id);
            if (dependency == modules.end())
                throw std::runtime_error(module.path + ": E_INIT_UNRESOLVED: missing browser initialization import");
            visit(*dependency->second);
        }
        visiting.erase(module.id);
        ranks.emplace(module.id, orderedModules.size());
        orderedModules.push_back(&module);
    };
    if (const auto entry = modules.find(project.entry); entry != modules.end()) visit(*entry->second);
    for (const auto& module : project.modules) visit(module);
    auto fail = [](const Module& module, const jtml::RuntimePlanBinding& binding,
                   const std::string& code, const std::string& reason) {
        std::string at = module.path;
        if (binding.sourceSpan.original) {
            const auto& origin = *binding.sourceSpan.original;
            const auto pos = origin.source->coordinates(origin.begin);
            at = origin.source->file + ":" + std::to_string(pos.first) + ":" + std::to_string(pos.second);
        } else at += ":" + std::to_string(binding.sourceSpan.startLine) + ":" + std::to_string(binding.sourceSpan.startColumn);
        throw std::runtime_error(at + ": " + code + ": " + reason +
            "; use earlier module-local state and structured initializer expressions");
    };
    bool legacy = false;
    std::vector<std::string> order;
    for (const auto* entry : orderedModules) {
        const auto& module = *entry;
        if (!module.clientExecutable) continue;
        const auto& local = positions.at(module.id);
        for (size_t i = 0; i < module.plan.state.size(); ++i) {
            const auto& binding = module.plan.state[i];
            if (binding.name == "__proto__") fail(module, binding, "E_INIT_UNSUPPORTED", "reserved state binding `__proto__`");
            // UI lowering can repeat a physical slot, e.g. scene `into` defaults.
            // Do not reinterpret those records as new independent declarations.
            if (local.at(binding.name).size() != 1) { legacy = true; continue; }
            const auto ir = jtml::parseExpressionIr(binding.expr);
            if (!ir || !ir->supported || !jtml::expressionIrSupportsDirectJs(*ir))
                fail(module, binding, "E_INIT_UNSUPPORTED", "unsupported initializer for `" + binding.name + "`");
            for (const auto& dependency : jtml::expressionIrDependencies(*ir)) {
                const auto root = dependency.substr(0, dependency.find_first_of(".["));
                const auto own = local.find(root);
                if (own != local.end()) {
                    if (own->second.size() != 1) { legacy = true; continue; }
                    if (own->second.front() >= i)
                        fail(module, binding, "E_INIT_FORWARD", (root == binding.name ? "self-reference to `" : "forward reference to `") + root + "`");
                    continue;
                }
                if (std::any_of(module.plan.derived.begin(), module.plan.derived.end(), [&](const auto& b) { return b.name == root; }))
                    fail(module, binding, "E_INIT_DERIVED", "initializer reads get `" + root + "` before derived initialization");
                // Named import authority and legacy whole-file scopes are both
                // retained identities. Never search unrelated project globals.
                struct ImportRead { const Module* owner = nullptr; bool derived = false; };
                auto named = [&](const Module& scope) -> ImportRead {
                    for (const auto& symbol : scope.importedSymbols) if (symbol.name == root) {
                        const auto owner = modules.find(symbol.module);
                        if (owner == modules.end() || !owner->second->clientExecutable) continue;
                        if (symbol.kind == "get") return {owner->second, true};
                        if ((symbol.kind == "let" || symbol.kind == "const" || symbol.kind == "store") &&
                            positions.at(symbol.module).count(root)) return {owner->second};
                    }
                    return {};
                };
                std::set<jtml::SemanticModuleId> seen;
                std::function<ImportRead(const Module&)> sideEffects = [&](const Module& scope) -> ImportRead {
                    for (const auto id : scope.initializationSideEffects) {
                        const auto owner = modules.find(id);
                        if (owner == modules.end() || !seen.insert(id).second) continue;
                        const auto& imported = *owner->second;
                        if (!imported.clientExecutable) continue;
                        if (positions.at(id).count(root)) return {&imported};
                        if (std::any_of(imported.plan.derived.begin(), imported.plan.derived.end(), [&](const auto& b) { return b.name == root; })) return {&imported, true};
                        if (const auto value = named(imported); value.owner) return value;
                        if (const auto value = sideEffects(imported); value.owner) return value;
                    }
                    return {};
                };
                const auto direct = named(module);
                const auto imported = direct.owner ? direct : sideEffects(module);
                if (imported.derived) fail(module, binding, "E_INIT_DERIVED", "initializer reads imported get `" + root + "`");
                if (!imported.owner || root == "__proto__")
                    fail(module, binding, "E_INIT_UNRESOLVED", "unresolved or unimported initializer binding `" + root + "`");
                if (ranks.at(imported.owner->id) >= ranks.at(module.id))
                    fail(module, binding, "E_INIT_FORWARD", "imported initializer owner does not precede `" + root + "`");
                if (positions.at(imported.owner->id).at(root).size() != 1) legacy = true;
            }
            order.push_back(binding.name);
        }
    }
    for (const auto& binding : project.linkedPlan.state) if (!owned.count(binding.name)) legacy = true;
    return legacy ? std::vector<std::string>{} : order;
}

// Certify getter closures from retained module plans/import identities. The
// resulting dependency-first order is shared by initialization and publication.
std::vector<std::string> planBrowserDerivedOrder(const jtml::RuntimeProjectPlan& project) {
    using Module = jtml::RuntimeModulePlan;
    using Binding = jtml::RuntimePlanBinding;
    struct Resolved { const Module* owner; const Binding* binding; bool derived; };
    std::map<jtml::SemanticModuleId, const Module*> modules;
    // Actual shared value slots, with their defining owners (ownership collisions
    // have already been rejected). These are not lexical name sets.
    std::map<std::string, Resolved> storageOwners;
    for (const auto& module : project.modules) if (module.clientExecutable) {
        modules[module.id] = &module;
        for (const auto& binding : module.plan.state) storageOwners.emplace(binding.name, Resolved{&module, &binding, false});
        for (const auto& binding : module.plan.derived) storageOwners.insert_or_assign(binding.name, Resolved{&module, &binding, true});
        for (const auto& fetch : module.plan.fetches) storageOwners.emplace(fetch.name, Resolved{&module, nullptr, false});
    }
    auto location = [](const Module& module, const SourceSpan& span) {
        std::string at = module.path;
        if (span.original) {
            const auto& origin = *span.original;
            const auto pos = origin.source->coordinates(origin.begin);
            at = origin.source->file + ":" + std::to_string(pos.first) + ":" + std::to_string(pos.second);
        } else if (span.valid()) {
            at += ":" + std::to_string(span.startLine) + ":" + std::to_string(span.startColumn);
        }
        return at;
    };
    auto fail = [&](const Module& module, const SourceSpan& span, const std::string& reason) {
        throw std::runtime_error(location(module, span) + ": browser get is unsupported: " + reason +
            "; use an owned, acyclic structured formula");
    };
    auto local = [](const Module& module, const std::string& name) -> Resolved {
        for (const auto& binding : module.plan.state)
            if (binding.name == name) return {&module, &binding, false};
        for (const auto& binding : module.plan.derived)
            if (binding.name == name) return {&module, &binding, true};
        return {nullptr, nullptr, false};
    };
    auto resolve = [&](const Module& module, const std::string& name, bool publication = false) -> Resolved {
        const auto own = local(module, name);
        if (own.binding) return own;
        for (const auto& symbol : module.importedSymbols) if (symbol.name == name &&
            (symbol.kind == "let" || symbol.kind == "const" || symbol.kind == "get" ||
             (publication && symbol.kind == "store"))) {
            const auto found = modules.find(symbol.module);
            if (found != modules.end()) {
                const auto target = local(*found->second, name);
                if (!target.binding || target.derived != (symbol.kind == "get")) continue;
                if (symbol.kind == "store") {
                    // Retained export identity must name its owner's lowered store
                    // object. Its members are external state leaves, not getters.
                    const auto initial = jtml::parseExpressionIr(target.binding->expr);
                    if (!initial || initial->valueType != jtml::ExpressionValueType::Object) continue;
                }
                return target;
            }
        }
        return {nullptr, nullptr, false};
    };
    auto ambiguous = [](Resolved target) {
        const auto sameName = [&](const auto& b) { return b.name == target.binding->name; };
        const auto derived = std::count_if(target.owner->plan.derived.begin(), target.owner->plan.derived.end(), sameName);
        return derived && (derived != 1 ||
            std::any_of(target.owner->plan.state.begin(), target.owner->plan.state.end(), sameName));
    };
    std::set<std::pair<jtml::SemanticModuleId, std::string>> active, complete;
    std::vector<std::string> order;
    std::function<void(Resolved, bool)> prove = [&](Resolved resolved, bool scalarRead) {
        const auto& module = *resolved.owner;
        const auto& binding = *resolved.binding;
        if (binding.name == "__proto__") fail(module, binding.sourceSpan, "reserved storage binding `__proto__`");
        if (ambiguous(resolved))
            fail(module, binding.sourceSpan, "ambiguous get `" + binding.name + "`");
        const auto key = std::make_pair(module.id, binding.name);
        if (complete.count(key)) return;
        if (!active.insert(key).second) fail(module, binding.sourceSpan, "E_DERIVED_CYCLE: cycle involving `" + binding.name + "`");
        const auto ir = jtml::parseExpressionIr(binding.expr);
        if (!ir || !ir->supported || !jtml::expressionIrSupportsDirectJs(*ir) ||
            (scalarRead && (ir->valueType == jtml::ExpressionValueType::Array ||
                            ir->valueType == jtml::ExpressionValueType::Object ||
                            ir->valueType == jtml::ExpressionValueType::Null)))
            fail(module, binding.sourceSpan, "E_UNSUPPORTED_EXPR: get `" + binding.name + "` requires a supported " + std::string(scalarRead ? "scalar " : "") + "expression plan");
        for (const auto& dependency : jtml::expressionIrDependencies(*ir)) {
            const auto root = dependency.substr(0, dependency.find_first_of(".["));
            const auto target = resolve(module, root, !scalarRead);
            if (!target.binding || root == "__proto__") fail(module, binding.sourceSpan, "unresolved, reserved or unimported binding `" + root + "`");
            if (ambiguous(target)) fail(module, binding.sourceSpan, "ambiguous value/get `" + root + "`");
            if (target.derived) prove(target, scalarRead);
        }
        active.erase(key);
        complete.insert(key);
        order.push_back(binding.name);
    };
    auto hasFetch = [](const Module& owner, const std::string& name) {
        return std::any_of(owner.plan.fetches.begin(), owner.plan.fetches.end(),
            [&](const auto& fetch) { return fetch.name == name; });
    };
    for (const auto& entry : modules) {
        const auto& module = *entry.second;
        std::vector<const Module*> wholeFileScopes;
        std::set<jtml::SemanticModuleId> seen{module.id};
        std::function<void(const Module&)> collectWholeFileScopes = [&](const Module& scope) {
            const auto& imports = scope.syntax == "classic"
                ? scope.initializationImports : scope.initializationSideEffects;
            for (const auto id : imports) {
                const auto owner = modules.find(id);
                if (owner == modules.end() || !seen.insert(id).second) continue;
                wholeFileScopes.push_back(owner->second);
                collectWholeFileScopes(*owner->second);
            }
        };
        collectWholeFileScopes(module);
        for (const auto& action : module.plan.actions) {
            bool consumesGet = false;
            auto rejectRead = [&](const SourceSpan& span, const std::string& code, const std::string& reason) {
                throw std::runtime_error(location(module, span.valid() ? span : action.sourceSpan) +
                    ": " + code + ": root action `" + action.name + "` " + reason);
            };
            using Scope = std::set<std::string>;
            const Scope parameters(action.params.begin(), action.params.end());
            auto inspect = [&](const std::string& expression, const SourceSpan& span, const Scope& scope) {
                if (expression.empty()) return;
                const auto ir = jtml::parseExpressionIr(expression);
                if (!ir || !ir->supported || !jtml::expressionIrSupportsDirectJs(*ir))
                    rejectRead(span, "E_UNSUPPORTED_EXPR", "requires a supported expression plan for `" + expression + "`");
                std::function<void(const jtml::ExpressionIr&)> visit = [&](const auto& node) {
                    if (node.kind == jtml::ExpressionIrKind::Path) {
                        const auto& name = node.root;
                        if (name == "__proto__")
                            rejectRead(span, "E_UNRESOLVED", "reads reserved binding `" + name + "`");
                        if (scope.count(name)) return;
                        auto target = resolve(module, name, true);
                        if (!target.binding) {
                            if (hasFetch(module, name)) return;
                            for (const auto& symbol : module.importedSymbols) {
                                const auto owner = modules.find(symbol.module);
                                if (symbol.name == name && (symbol.kind == "fetch" || symbol.kind == "let") &&
                                    owner != modules.end() && hasFetch(*owner->second, name)) return;
                            }
                            for (const auto* owner : wholeFileScopes) {
                                if (hasFetch(*owner, name)) return;
                                const auto imported = resolve(*owner, name, true);
                                if (imported.binding && !imported.derived) {
                                    target = imported;
                                    break;
                                }
                            }
                        }
                        if (!target.binding) {
                            const auto occupied = storageOwners.find(name);
                            if (occupied != storageOwners.end() && occupied->second.derived)
                                rejectRead(span, "E_UNRESOLVED", "reads unimported get `" + name + "`");
                            rejectRead(span, "E_UNRESOLVED", "reads unresolved or unimported binding `" + name + "`");
                        }
                        if (ambiguous(target)) fail(module, span, "ambiguous value/get `" + name + "`");
                        if (target.derived) {
                            consumesGet = true;
                            prove(target, true);
                        }
                    }
                    for (const auto& child : node.children) visit(*child);
                    for (const auto& member : node.entries) visit(*member.value);
                };
                visit(*ir);
            };
            std::function<void(const std::vector<jtml::RuntimePlanStatement>&, Scope)> walk =
                [&](const auto& statements, Scope scope) {
                    for (const auto& statement : statements) {
                        inspect(statement.expr, statement.sourceSpan, scope);
                        inspect(statement.condition, statement.sourceSpan, scope);
                        if (statement.lhs.find_first_of(".[") != std::string::npos)
                            inspect(statement.lhs, statement.sourceSpan, scope);
                        if (!statement.localBinding.empty()) {
                            const auto occupied = storageOwners.find(statement.localBinding);
                            // Locals share runtime slots. If an initializer produces no value,
                            // an unprotected local could otherwise read another owner's value.
                            if (occupied != storageOwners.end() && !parameters.count(statement.localBinding) &&
                                (occupied->second.derived || occupied->second.owner != &module))
                                rejectRead(statement.sourceSpan, "E_UNRESOLVED", "local declaration shadows root storage `" +
                                    statement.localBinding + "` owned by " + occupied->second.owner->path);
                            scope.insert(statement.localBinding);
                        }
                        if (statement.kind == "if") {
                            walk(statement.thenStatements, scope);
                            walk(statement.elseStatements, scope);
                        }
                    }
                };
            walk(action.body, parameters);
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
                    const auto occupied = storageOwners.find(name);
                    // Parameter slots are restored after dispatch and masked from
                    // getter closures, including a local declaration in that slot.
                    if (occupied != storageOwners.end() &&
                        std::find(action.params.begin(), action.params.end(), name) == action.params.end())
                        fail(module, node->sourceSpan, "local declaration shadows root storage `" + name +
                            "` owned by " + occupied->second.owner->path);
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
    // Action reads were checked first with the stronger scalar-result contract.
    // Publication preserves existing structured collection getters as well.
    for (const auto& entry : modules)
        for (const auto& binding : entry.second->plan.derived)
            prove({entry.second, &binding, true}, false);
    return order;
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
void validateBrowserRuntimeBindingOwnership(jtml::RuntimeProjectPlan& project) {
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
    project.stateOrder = planBrowserStateOrder(project);
    project.derivedOrder = planBrowserDerivedOrder(project);
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
