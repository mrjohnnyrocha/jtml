#include "jtml/client_manifest_emitter.h"
#include "jtml/expression_source.h"
#include "jtml/runtime_plan_json.h"

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

std::string wrapClientManifestJson(const std::string& manifestJson) {
    auto escaped = manifestJson;
    auto replaceAll = [](std::string& value,
                         const std::string& needle,
                         const std::string& replacement) {
        size_t pos = 0;
        while ((pos = value.find(needle, pos)) != std::string::npos) {
            value.replace(pos, needle.size(), replacement);
            pos += replacement.size();
        }
    };
    replaceAll(escaped, "&", "\\u0026");
    replaceAll(escaped, "<", "\\u003c");
    replaceAll(escaped, ">", "\\u003e");
    replaceAll(escaped, "\xE2\x80\xA8", "\\u2028");
    replaceAll(escaped, "\xE2\x80\xA9", "\\u2029");
    std::ostringstream out;
    out << "<script type=\"application/json\" id=\"__jtml_client_manifest\">"
        << escaped
        << "</script>\n";
    return out.str();
}

} // namespace

std::string emitClientManifestScript(const std::vector<std::unique_ptr<ASTNode>>& program) {
    return emitClientManifestScript(buildRuntimePlan(program));
}

std::string emitClientManifestScript(const RuntimePlan& plan) {
    return wrapClientManifestJson(runtimePlanToClientJson(plan).dump());
}

std::string emitClientManifestScript(const RuntimeProjectPlan& plan) {
    return wrapClientManifestJson(runtimeProjectPlanToClientJson(plan).dump());
}

} // namespace jtml
