#include "jtml/semantic_project_json.h"

namespace jtml {
namespace {

nlohmann::json semanticModuleImportsToJson(const std::vector<SemanticModuleImport>& imports) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& import : imports) {
        nlohmann::json resolvedSymbols = nlohmann::json::array();
        for (const auto& symbol : import.resolvedSymbols) {
            resolvedSymbols.push_back({
                {"name", symbol.name},
                {"kind", symbol.kind},
                {"module", symbol.module == InvalidSemanticModuleId
                    ? nlohmann::json(nullptr)
                    : nlohmann::json(symbol.module)},
            });
        }
        auto item = nlohmann::json{
            {"specifier", import.specifier},
            {"kind", import.kind},
            {"names", import.names},
            {"resolvedSymbols", std::move(resolvedSymbols)},
            {"reExport", import.reExport},
            {"importer", import.importer},
            {"resolvedPath", import.resolvedPath},
            {"span", {
                {"module", import.span.module == InvalidSemanticModuleId
                    ? nlohmann::json(nullptr)
                    : nlohmann::json(import.span.module)},
                {"line", import.span.line},
                {"column", import.span.column},
            }},
        };
        item["resolved"] = import.resolved == InvalidSemanticModuleId
            ? nlohmann::json(nullptr)
            : nlohmann::json(import.resolved);
        out.push_back(std::move(item));
    }
    return out;
}

nlohmann::json semanticModuleSummaryToJson(const SemanticProgram& semantic) {
    return {
        {"state", semantic.state},
        {"constants", semantic.constants},
        {"derived", semantic.derived},
        {"actions", semantic.actions},
        {"fetches", semantic.fetches},
        {"routes", semantic.routes},
        {"components", semantic.components},
        {"stores", semantic.stores},
        {"effects", semantic.effects},
        {"externs", semantic.externs},
        {"imports", semantic.imports},
        {"exportCount", semantic.exportRecords.size()},
        {"componentDefinitionCount", semantic.componentDefinitions.size()},
        {"componentInstanceCount", semantic.componentInstances.size()},
        {"dependencyCount", semantic.dependencies.size()},
    };
}

nlohmann::json semanticModuleIrToJson(const SemanticModuleIr& ir) {
    nlohmann::json topLevel = nlohmann::json::array();
    for (const auto& node : ir.topLevelNodes) {
        topLevel.push_back({
            {"kind", node.kind},
            {"label", node.label},
            {"depth", node.depth},
            {"childCount", node.childCount},
        });
    }

    nlohmann::json counts = nlohmann::json::array();
    for (const auto& count : ir.nodeCounts) {
        counts.push_back({
            {"kind", count.kind},
            {"count", count.count},
        });
    }

    return {
        {"available", ir.available},
        {"syntax", ir.syntax},
        {"parseError", ir.parseError},
        {"parseErrorLine", ir.parseErrorLine},
        {"parseErrorColumn", ir.parseErrorColumn},
        {"topLevelCount", ir.topLevelCount},
        {"totalNodeCount", ir.totalNodeCount},
        {"topLevelNodes", topLevel},
        {"nodeCounts", counts},
    };
}

} // namespace

nlohmann::json semanticExportRecordsToJson(const std::vector<SemanticExport>& exports) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& exportRecord : exports) {
        out.push_back({
            {"name", exportRecord.name},
            {"kind", exportRecord.kind},
            {"specifier", exportRecord.specifier},
            {"reExport", exportRecord.reExport},
            {"sourceLine", exportRecord.sourceLine},
        });
    }
    return out;
}

nlohmann::json semanticProjectIssuesToJson(const std::vector<SemanticProjectIssue>& issues) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& issue : issues) {
        out.push_back({
            {"code", issue.code},
            {"message", issue.message},
            {"module", issue.module == InvalidSemanticModuleId
                ? nlohmann::json(nullptr)
                : nlohmann::json(issue.module)},
            {"importIndex", issue.importIndex},
            {"specifier", issue.specifier},
            {"resolvedPath", issue.resolvedPath},
            {"path", issue.path},
            {"line", issue.line},
            {"column", issue.column},
            {"requested", issue.requested},
            {"available", issue.available},
        });
    }
    return out;
}

nlohmann::json semanticProjectToJson(const SemanticProject& project) {
    nlohmann::json modules = nlohmann::json::array();
    for (const auto& module : project.modules) {
        modules.push_back({
            {"id", module.id},
            {"path", module.path},
            {"imports", semanticModuleImportsToJson(module.imports)},
            {"exports", semanticExportRecordsToJson(module.exports)},
            {"semantic", semanticModuleSummaryToJson(module.semantic)},
            {"ir", semanticModuleIrToJson(module.ir)},
        });
    }

    auto entry = project.entry == InvalidSemanticModuleId
        ? nlohmann::json(nullptr)
        : nlohmann::json(project.entry);
    return {
        {"entry", std::move(entry)},
        {"modules", modules},
        {"issues", semanticProjectIssuesToJson(analyzeSemanticProject(project))},
    };
}

} // namespace jtml
