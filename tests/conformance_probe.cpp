// Test adapter only: uses the real frontend, plans and public interpreter APIs.
#include "cli/util.h"
#include "jtml/interpreter.h"
#include "jtml/runtime_plan.h"
#include "jtml/runtime_plan_json.h"
#include <fstream>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        if (argc != 4) throw std::runtime_error("usage: probe source request.json result.json");
        const auto request = nlohmann::json::parse(jtml::cli::readFile(argv[2]));
        auto program = jtml::cli::parseProgramFromFile(argv[1]);
        const auto semantic = jtml::analyzeSemanticProgram(program, jtml::cli::readFile(argv[1]));
        const auto plan = jtml::buildRuntimePlan(program, semantic);
        nlohmann::json result = {{"plan", jtml::runtimePlanToExplainJson(plan)},
                                 {"snapshots", nlohmann::json::array()},
                                 {"dispatch", nlohmann::json::array()}};
        result["lowered"] = jtml::cli::normalizeCompilationSource(
            jtml::cli::loadCompilationSource(argv[1], jtml::SyntaxMode::Auto)).text;
        result["origins"] = nlohmann::json::array();
        result["semanticComponents"] = nlohmann::json::array();
        for (const auto& definition : semantic.componentDefinitions) {
            result["semanticComponents"].push_back({{"name", definition.name},
                {"state", definition.localState}, {"derived", definition.localDerived},
                {"actions", definition.localActions}});
        }
        for (const auto& definition : plan.componentDefinitions) {
            for (const auto& node : definition.bodyPlan) {
                nlohmann::json origin = nullptr;
                if (node.sourceSpan.original) {
                    const auto& range = *node.sourceSpan.original;
                    const auto position = range.source->coordinates(range.begin);
                    origin = {{"file", range.source->file}, {"line", position.first},
                              {"column", position.second}, {"exact", node.sourceSpan.originalExact}};
                }
                result["origins"].push_back({{"component", definition.name},
                    {"text", node.text}, {"origin", origin}});
            }
        }
        JtmlTranspiler transpiler;
        transpiler.transpile(program);
        {
            Interpreter interpreter(transpiler, InterpreterConfig{0, false});
            interpreter.interpret(program);
            result["snapshots"].push_back(nlohmann::json::parse(interpreter.getStateJSON()));
            for (const auto& step : request.at("steps")) {
                const auto components = nlohmann::json::parse(interpreter.getComponentsJSON());
                std::string id;
                int ordinal = step.value("ordinal", 0);
                for (const auto& component : components) {
                    if (component.at("component") == step.at("component") && ordinal-- == 0) {
                        id = component.at("id").get<std::string>();
                        break;
                    }
                }
                if (id.empty()) throw std::runtime_error("requested native component is absent");
                std::string bindings, error;
                const bool ok = interpreter.dispatchComponentAction(id, step.at("action"),
                    step.value("args", nlohmann::json::array()), bindings, error);
                result["dispatch"].push_back({{"ok", ok}, {"error", error}});
                result["snapshots"].push_back(nlohmann::json::parse(interpreter.getStateJSON()));
            }
            interpreter.waitForAsync();
        }
        std::ofstream output(argv[3]);
        output << result.dump(2) << '\n';
        if (!output) throw std::runtime_error("cannot write result");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "conformance adapter: " << error.what() << '\n';
        return 1;
    }
}
