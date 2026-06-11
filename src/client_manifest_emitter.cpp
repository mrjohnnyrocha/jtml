#include "jtml/client_manifest_emitter.h"
#include "jtml/expression_source.h"

#include <sstream>

namespace jtml {
namespace {

std::string semanticPropertiesJson(const std::vector<SemanticProperty>& properties) {
    std::ostringstream out;
    out << '{';
    for (size_t i = 0; i < properties.size(); ++i) {
        if (i) out << ',';
        out << jsonString(properties[i].name) << ':' << jsonString(properties[i].value);
    }
    out << '}';
    return out.str();
}

std::string clientStatementsJson(const std::vector<RuntimePlanStatement>& nodes) {
    std::ostringstream out;
    out << '[';
    bool first = true;
    auto sep = [&]() {
        if (!first) out << ',';
        first = false;
    };
    for (const auto& stmt : nodes) {
        if (stmt.kind == "assign") {
            sep();
            out << "{\"kind\":\"assign\",\"lhs\":" << jsonString(stmt.lhs)
                << ",\"expr\":" << jsonString(stmt.expr) << '}';
        } else if (stmt.kind == "if") {
            sep();
            out << "{\"kind\":\"if\",\"condition\":" << jsonString(stmt.condition)
                << ",\"then\":" << clientStatementsJson(stmt.thenStatements)
                << ",\"else\":" << clientStatementsJson(stmt.elseStatements)
                << '}';
        }
    }
    out << ']';
    return out.str();
}

std::string componentBodyPlanJson(const std::vector<RuntimePlanComponentBodyNode>& nodes) {
    std::ostringstream out;
    out << '[';
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (i) out << ',';
        const auto& node = nodes[i];
        out << "{\"indent\":" << node.indent
            << ",\"parentIndex\":" << node.parentIndex
            << ",\"kind\":" << jsonString(node.kind)
            << ",\"head\":" << jsonString(node.head)
            << ",\"name\":" << jsonString(node.name)
            << ",\"text\":" << jsonString(node.text)
            << ",\"renderRoot\":" << (node.renderRoot ? "true" : "false")
            << "}";
    }
    out << ']';
    return out.str();
}

} // namespace

std::string emitClientManifestScript(const std::vector<std::unique_ptr<ASTNode>>& program) {
    return emitClientManifestScript(buildRuntimePlan(program));
}

std::string emitClientManifestScript(const RuntimePlan& plan) {
    std::ostringstream json;
    json << "{\"state\":{";
    bool firstState = true;
    auto stateSep = [&]() {
        if (!firstState) json << ',';
        firstState = false;
    };

    std::ostringstream derived;
    derived << "\"derived\":{";
    bool firstDerived = true;
    auto derivedSep = [&]() {
        if (!firstDerived) derived << ',';
        firstDerived = false;
    };

    std::ostringstream actions;
    actions << "\"actions\":{";
    bool firstAction = true;
    auto actionSep = [&]() {
        if (!firstAction) actions << ',';
        firstAction = false;
    };

    auto stringVectorJson = [](const std::vector<std::string>& values) {
        std::ostringstream out;
        out << '[';
        for (size_t i = 0; i < values.size(); ++i) {
            if (i) out << ',';
            out << jsonString(values[i]);
        }
        out << ']';
        return out.str();
    };
    std::ostringstream routes;
    routes << "\"routes\":[";
    for (size_t i = 0; i < plan.routes.size(); ++i) {
        if (i) routes << ',';
        const auto& route = plan.routes[i];
        routes << "{\"path\":" << jsonString(route.path)
               << ",\"name\":" << jsonString(route.component)
               << ",\"params\":[";
        for (size_t paramIndex = 0; paramIndex < route.params.size(); ++paramIndex) {
            if (paramIndex) routes << ',';
            routes << jsonString(route.params[paramIndex]);
        }
        routes << "],\"load\":[";
        for (size_t loadIndex = 0; loadIndex < route.loads.size(); ++loadIndex) {
            if (loadIndex) routes << ',';
            routes << jsonString(route.loads[loadIndex]);
        }
        routes << "]}";
    }

    std::ostringstream fetches;
    fetches << "\"fetches\":[";
    for (size_t i = 0; i < plan.fetches.size(); ++i) {
        if (i) fetches << ',';
        const auto& fetch = plan.fetches[i];
        fetches << "{\"name\":" << jsonString(fetch.name)
                << ",\"url\":" << jsonString(fetch.url)
                << ",\"method\":" << jsonString(fetch.method.empty() ? "GET" : fetch.method)
                << ",\"bodyExpr\":" << jsonString(fetch.bodyExpr)
                << ",\"refreshAction\":" << jsonString(fetch.refreshAction)
                << ",\"cache\":" << jsonString(fetch.cache)
                << ",\"credentials\":" << jsonString(fetch.credentials)
                << ",\"timeoutMs\":" << jsonString(fetch.timeoutMs)
                << ",\"retryCount\":" << jsonString(fetch.retryCount)
                << ",\"stalePolicy\":" << jsonString(fetch.stalePolicy.empty() ? "clear" : fetch.stalePolicy)
                << ",\"group\":" << jsonString(fetch.group)
                << ",\"cacheKeyExpr\":" << jsonString(fetch.cacheKeyExpr)
                << ",\"revalidateMs\":" << jsonString(fetch.revalidateMs)
                << ",\"dedupe\":" << (fetch.dedupe ? "true" : "false")
                << ",\"background\":" << (fetch.background ? "true" : "false")
                << ",\"lazy\":" << (fetch.lazy ? "true" : "false")
                << "}";
    }

    std::ostringstream componentDefinitions;
    componentDefinitions << "\"componentDefinitions\":[";
    for (size_t i = 0; i < plan.componentDefinitions.size(); ++i) {
        if (i) componentDefinitions << ',';
        const auto& definition = plan.componentDefinitions[i];
        componentDefinitions << "{\"name\":" << jsonString(definition.name)
                             << ",\"params\":[";
        for (size_t paramIndex = 0; paramIndex < definition.params.size(); ++paramIndex) {
            if (paramIndex) componentDefinitions << ',';
            componentDefinitions << jsonString(definition.params[paramIndex]);
        }
        componentDefinitions << "],\"localState\":" << stringVectorJson(definition.localState)
                             << ",\"localDerived\":" << stringVectorJson(definition.localDerived)
                             << ",\"localActions\":" << stringVectorJson(definition.localActions)
                             << ",\"localEffects\":" << stringVectorJson(definition.localEffects)
                             << ",\"eventBindings\":" << stringVectorJson(definition.eventBindings)
                             << ",\"bodyHex\":" << jsonString(definition.bodyHex)
                             << ",\"bodyPlan\":" << componentBodyPlanJson(definition.bodyPlan)
                             << ",\"hasSlot\":" << (definition.hasSlot ? "true" : "false")
                             << ",\"bodyNodeCount\":" << definition.bodyNodeCount
                             << ",\"rootTemplateNodeCount\":" << definition.rootTemplateNodeCount
                             << ",\"slotCount\":" << definition.slotCount
                             << ",\"sourceLine\":" << definition.sourceLine
                             << "}";
    }

    std::ostringstream componentInstances;
    componentInstances << "\"componentInstances\":[";
    for (size_t i = 0; i < plan.componentInstances.size(); ++i) {
        if (i) componentInstances << ',';
        const auto& instance = plan.componentInstances[i];
        componentInstances << "{\"id\":" << jsonString(instance.id)
                           << ",\"component\":" << jsonString(instance.component)
                           << ",\"instanceId\":" << instance.instanceId
                           << ",\"role\":" << jsonString(instance.role.empty() ? "component" : instance.role)
                           << ",\"params\":" << semanticPropertiesJson(instance.params)
                           << ",\"locals\":" << semanticPropertiesJson(instance.locals)
                           << ",\"sourceLine\":" << instance.sourceLine
                           << "}";
    }

    for (const auto& binding : plan.state) {
        stateSep();
        json << jsonString(binding.name) << ':' << jsonString(binding.expr);
    }

    for (const auto& binding : plan.derived) {
        derivedSep();
        derived << jsonString(binding.name) << ':' << jsonString(binding.expr);
    }

    for (const auto& action : plan.actions) {
        actionSep();
        actions << jsonString(action.name) << ":{\"params\":[";
        for (size_t i = 0; i < action.params.size(); ++i) {
            if (i) actions << ',';
            actions << jsonString(action.params[i]);
        }
        actions << "],\"body\":" << clientStatementsJson(action.body) << '}';
    }

    json << "},";
    derived << "},";
    routes << "],";
    fetches << "],";
    componentDefinitions << "],";
    componentInstances << "],";
    actions << "}}";
    json << derived.str() << routes.str() << fetches.str()
         << componentDefinitions.str() << componentInstances.str() << actions.str();

    std::ostringstream out;
    out << "<script type=\"application/json\" id=\"__jtml_client_manifest\">"
        << json.str()
        << "</script>\n";
    return out.str();
}


} // namespace jtml
