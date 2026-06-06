#include "jtml/client_manifest_emitter.h"
#include "jtml/expression_source.h"
#include "jtml/semantic.h"

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

std::string clientStatementsJson(const std::vector<std::unique_ptr<ASTNode>>& nodes) {
    std::ostringstream out;
    out << '[';
    bool first = true;
    auto sep = [&]() {
        if (!first) out << ',';
        first = false;
    };
    for (const auto& node : nodes) {
        if (!node) continue;
        if (node->getType() == ASTNodeType::AssignmentStatement) {
            const auto& stmt = static_cast<const AssignmentStatementNode&>(*node);
            sep();
            out << "{\"kind\":\"assign\",\"lhs\":"
                << jsonString(expressionSource(stmt.lhs.get()))
                << ",\"expr\":" << jsonString(expressionSource(stmt.rhs.get())) << '}';
        } else if (node->getType() == ASTNodeType::DefineStatement) {
            const auto& stmt = static_cast<const DefineStatementNode&>(*node);
            sep();
            out << "{\"kind\":\"assign\",\"lhs\":"
                << jsonString(stmt.identifier)
                << ",\"expr\":" << jsonString(expressionSource(stmt.expression.get())) << '}';
        } else if (node->getType() == ASTNodeType::IfStatement) {
            const auto& stmt = static_cast<const IfStatementNode&>(*node);
            sep();
            out << "{\"kind\":\"if\",\"condition\":"
                << jsonString(expressionSource(stmt.condition.get()))
                << ",\"then\":" << clientStatementsJson(stmt.thenStatements)
                << ",\"else\":" << clientStatementsJson(stmt.elseStatements)
                << '}';
        }
    }
    out << ']';
    return out.str();
}

} // namespace

std::string emitClientManifestScript(const std::vector<std::unique_ptr<ASTNode>>& program) {
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

    const auto semantic = jtml::analyzeSemanticProgram(program);
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
    for (size_t i = 0; i < semantic.routeRecords.size(); ++i) {
        if (i) routes << ',';
        const auto& route = semantic.routeRecords[i];
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
    for (size_t i = 0; i < semantic.fetchRecords.size(); ++i) {
        if (i) fetches << ',';
        const auto& fetch = semantic.fetchRecords[i];
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
    for (size_t i = 0; i < semantic.componentDefinitions.size(); ++i) {
        if (i) componentDefinitions << ',';
        const auto& definition = semantic.componentDefinitions[i];
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
                             << ",\"hasSlot\":" << (definition.hasSlot ? "true" : "false")
                             << ",\"bodyNodeCount\":" << definition.bodyNodeCount
                             << ",\"rootTemplateNodeCount\":" << definition.rootTemplateNodeCount
                             << ",\"slotCount\":" << definition.slotCount
                             << ",\"sourceLine\":" << definition.sourceLine
                             << "}";
    }

    std::ostringstream componentInstances;
    componentInstances << "\"componentInstances\":[";
    for (size_t i = 0; i < semantic.componentInstances.size(); ++i) {
        if (i) componentInstances << ',';
        const auto& instance = semantic.componentInstances[i];
        componentInstances << "{\"id\":" << jsonString(instance.id)
                           << ",\"component\":" << jsonString(instance.component)
                           << ",\"instanceId\":" << instance.instanceId
                           << ",\"role\":" << jsonString(instance.role.empty() ? "component" : instance.role)
                           << ",\"params\":" << semanticPropertiesJson(instance.params)
                           << ",\"locals\":" << semanticPropertiesJson(instance.locals)
                           << ",\"sourceLine\":" << instance.sourceLine
                           << "}";
    }

    for (const auto& node : program) {
        if (!node) continue;
        if (node->getType() == ASTNodeType::DefineStatement) {
            const auto& stmt = static_cast<const DefineStatementNode&>(*node);
            stateSep();
            json << jsonString(stmt.identifier) << ':' << jsonString(expressionSource(stmt.expression.get()));
        } else if (node->getType() == ASTNodeType::DeriveStatement) {
            const auto& stmt = static_cast<const DeriveStatementNode&>(*node);
            derivedSep();
            derived << jsonString(stmt.identifier) << ':' << jsonString(expressionSource(stmt.expression.get()));
        } else if (node->getType() == ASTNodeType::FunctionDeclaration) {
            const auto& stmt = static_cast<const FunctionDeclarationNode&>(*node);
            actionSep();
            actions << jsonString(stmt.name) << ":{\"params\":[";
            for (size_t i = 0; i < stmt.parameters.size(); ++i) {
                if (i) actions << ',';
                actions << jsonString(stmt.parameters[i].name);
            }
            actions << "],\"body\":" << clientStatementsJson(stmt.body) << '}';
        }
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
