#include "jtml/friendly.h"
#include "jtml/expression_ir.h"
#include "jtml/language_catalog.h"
#include "jtml/lexer.h"
#include "jtml/parser.h"
#include "jtml/runtime_plan.h"
#include "jtml/runtime_plan_json.h"
#include "jtml/runtime/static_update_plan_emitter.h"
#include "jtml/semantic.h"
#include "jtml/semantic/module_graph.h"
#include "jtml/transpiler.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::vector<std::unique_ptr<ASTNode>> parseFriendly(const std::string& source) {
    const std::string classic = jtml::normalizeSourceSyntax(source, jtml::SyntaxMode::Friendly);
    Lexer lexer(classic);
    auto tokens = lexer.tokenize();
    EXPECT_TRUE(lexer.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    EXPECT_TRUE(parser.getErrors().empty()) << classic;
    return program;
}

std::string hexEncode(const std::string& value) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.reserve(value.size() * 2);
    for (unsigned char ch : value) {
        out.push_back(digits[ch >> 4]);
        out.push_back(digits[ch & 0x0f]);
    }
    return out;
}

bool contains(const std::vector<std::string>& values, const std::string& value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

int lineContaining(const std::string& source, const std::string& needle) {
    std::istringstream input(source);
    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        if (line.find(needle) != std::string::npos) return lineNumber;
    }
    return 0;
}

bool hasEdge(const jtml::SemanticProgram& semantic,
             const std::string& from,
             const std::string& to,
             const std::string& kind) {
    return std::any_of(semantic.dependencies.begin(), semantic.dependencies.end(), [&](const auto& edge) {
        return edge.from == from && edge.to == to && edge.kind == kind;
    });
}

bool hasModifier(const jtml::SemanticProgram& semantic,
                 const std::string& primitive,
                 const std::string& modifier,
                 const std::string& value) {
    return std::any_of(semantic.uiModifiers.begin(), semantic.uiModifiers.end(), [&](const auto& item) {
        return item.primitive == primitive && item.modifier == modifier && item.value == value;
    });
}

const jtml::SemanticRoute* findRouteRecord(const jtml::SemanticProgram& semantic,
                                           const std::string& path) {
    auto it = std::find_if(semantic.routeRecords.begin(), semantic.routeRecords.end(), [&](const auto& route) {
        return route.path == path;
    });
    return it == semantic.routeRecords.end() ? nullptr : &*it;
}

const jtml::SemanticFetch* findFetchRecord(const jtml::SemanticProgram& semantic,
                                           const std::string& name) {
    auto it = std::find_if(semantic.fetchRecords.begin(), semantic.fetchRecords.end(), [&](const auto& fetch) {
        return fetch.name == name;
    });
    return it == semantic.fetchRecords.end() ? nullptr : &*it;
}

const jtml::SemanticImport* findImportRecord(const jtml::SemanticProgram& semantic,
                                             const std::string& specifier) {
    auto it = std::find_if(semantic.importRecords.begin(), semantic.importRecords.end(), [&](const auto& import) {
        return import.specifier == specifier;
    });
    return it == semantic.importRecords.end() ? nullptr : &*it;
}

std::vector<const jtml::SemanticImport*> findImportRecords(
        const jtml::SemanticProgram& semantic,
        const std::string& specifier) {
    std::vector<const jtml::SemanticImport*> matches;
    for (const auto& import : semantic.importRecords) {
        if (import.specifier == specifier) matches.push_back(&import);
    }
    return matches;
}

const jtml::SemanticComponentDefinition* findComponentDefinition(
        const jtml::SemanticProgram& semantic,
        const std::string& name) {
    auto it = std::find_if(
        semantic.componentDefinitions.begin(),
        semantic.componentDefinitions.end(),
        [&](const auto& definition) { return definition.name == name; });
    return it == semantic.componentDefinitions.end() ? nullptr : &*it;
}

const jtml::RuntimePlanBinding* findRuntimeBinding(
        const std::vector<jtml::RuntimePlanBinding>& bindings,
        const std::string& name) {
    auto it = std::find_if(
        bindings.begin(),
        bindings.end(),
        [&](const auto& binding) { return binding.name == name; });
    return it == bindings.end() ? nullptr : &*it;
}

const jtml::RuntimePlanAction* findRuntimeAction(
        const std::vector<jtml::RuntimePlanAction>& actions,
        const std::string& name) {
    auto it = std::find_if(
        actions.begin(),
        actions.end(),
        [&](const auto& action) { return action.name == name; });
    return it == actions.end() ? nullptr : &*it;
}

} // namespace

TEST(SemanticProgram, ExtractsFriendlyAppShapeAndAttributeKinds) {
    const std::string source =
        "jtml 2\n"
        "\n"
        "use Util from \"./util.jtml\"\n"
        "let users = fetch \"/api/users\"\n"
        "let title = \"Ops\"\n"
        "get heading = \"{title} users\"\n"
        "\n"
        "effect title\n"
        "  let title = title\n"
        "\n"
        "store auth\n"
        "  let user = \"Ada\"\n"
        "\n"
        "when reload\n"
        "  title = heading\n"
        "  invalidate users\n"
        "\n"
        "make Card\n"
        "  box class \"card\"\n"
        "    h2 \"Card\"\n"
        "\n"
        "route \"/dashboard\" as Card\n"
        "\n"
        "page\n"
        "  box class \"shell\" aria-label \"Users view\"\n"
        "    h1 heading\n"
        "    button \"Reload\" click reload\n"
        "    Card\n";

    auto program = parseFriendly(source);
    const auto semantic = jtml::analyzeSemanticProgram(program, source);

    EXPECT_TRUE(contains(semantic.state, "title")) << semantic.state.size();
    EXPECT_TRUE(contains(semantic.state, "users")) << semantic.state.size();
    EXPECT_TRUE(contains(semantic.derived, "heading")) << semantic.derived.size();
    EXPECT_TRUE(contains(semantic.actions, "reload")) << semantic.actions.size();
    EXPECT_TRUE(contains(semantic.components, "Card")) << semantic.components.size();
    EXPECT_TRUE(contains(semantic.stores, "auth")) << semantic.stores.size();
    EXPECT_TRUE(contains(semantic.effects, "title")) << semantic.effects.size();
    EXPECT_TRUE(contains(semantic.fetches, "users")) << semantic.fetches.size();
    ASSERT_EQ(semantic.fetchRecords.size(), 1u) << semantic.fetchRecords.size();
    EXPECT_EQ(semantic.fetchRecords.front().name, "users");
    EXPECT_EQ(semantic.fetchRecords.front().url, "/api/users");
    EXPECT_EQ(semantic.fetchRecords.front().method, "GET");
    EXPECT_TRUE(contains(semantic.imports, "./util.jtml")) << semantic.imports.size();
    ASSERT_EQ(semantic.routes.size(), 1u) << semantic.routes.size();
    EXPECT_EQ(semantic.routes.front(), "/dashboard -> Card");
    ASSERT_EQ(semantic.routeRecords.size(), 1u) << semantic.routeRecords.size();
    EXPECT_EQ(semantic.routeRecords.front().path, "/dashboard");
    EXPECT_EQ(semantic.routeRecords.front().component, "Card");
    ASSERT_EQ(semantic.componentDefinitions.size(), 1u) << semantic.componentDefinitions.size();
    EXPECT_EQ(semantic.componentDefinitions.front().name, "Card");
    ASSERT_EQ(semantic.componentInstances.size(), 2u) << semantic.componentInstances.size();
    EXPECT_EQ(semantic.componentInstances.front().component, "Card");

    EXPECT_GE(semantic.attributes.literal, 2);
    EXPECT_GE(semantic.attributes.passthrough, 1);
    EXPECT_GE(semantic.attributes.event, 1);
    EXPECT_GE(semantic.attributes.special, 1);
    EXPECT_FALSE(semantic.dependencies.empty());
    EXPECT_TRUE(hasEdge(semantic, "heading", "title", "derives"));
    EXPECT_TRUE(hasEdge(semantic, "route:/dashboard", "component:Card", "renders"));
    EXPECT_TRUE(hasEdge(semantic, "ui:show", "heading", "renders"));
    EXPECT_TRUE(hasEdge(semantic, "action:reload", "title", "writes"));
    EXPECT_TRUE(hasEdge(semantic, "action:reload", "heading", "reads"));
    EXPECT_TRUE(hasEdge(semantic, "ui:@button.onClick", "reload", "triggers"));
    EXPECT_TRUE(hasEdge(semantic, "module", "./util.jtml", "imports"));
}

TEST(SemanticProgram, CapturesSemanticStylingPrimitives) {
    const std::string source =
        "jtml 2\n"
        "theme\n"
        "  color primary \"#155e75\"\n"
        "  space md 14\n"
        "page\n"
        "  panel title \"Usage\" tone primary\n"
        "    grid cols 2 gap md\n"
        "      metric \"Users\" users.total \"Active\" tone good\n";

    auto program = parseFriendly(source);
    const auto semantic = jtml::analyzeSemanticProgram(program, source);

    EXPECT_TRUE(contains(semantic.uiPrimitives, "panel"));
    EXPECT_TRUE(contains(semantic.uiPrimitives, "grid"));
    EXPECT_TRUE(contains(semantic.uiPrimitives, "metric"));
    EXPECT_TRUE(hasModifier(semantic, "panel", "tone", "primary"));
    EXPECT_TRUE(hasModifier(semantic, "grid", "cols", "2"));
    EXPECT_TRUE(hasModifier(semantic, "grid", "gap", "md"));
    EXPECT_TRUE(hasModifier(semantic, "metric", "tone", "good"));
    EXPECT_GE(semantic.styleBlocks, 1);
    EXPECT_EQ(semantic.authorThemeTokenCount, 2);
    EXPECT_GE(semantic.themeTokenCount, 2);
    EXPECT_TRUE(hasEdge(semantic, "ui:@section", "primitive:panel", "uses"));
    EXPECT_TRUE(hasEdge(semantic, "ui:@div", "primitive:grid", "uses"));
    EXPECT_TRUE(hasEdge(semantic, "ui:@article", "primitive:metric", "uses"));
    EXPECT_TRUE(hasEdge(semantic, "primitive:grid", "cols:2", "modifies"));
}

TEST(SemanticUiCatalog, ListsImplementedPrimitiveKitAndModifiers) {
    const auto& ui = jtml::semanticUiCatalog();

    auto primitiveNamed = [&](const std::string& name) {
        return std::find_if(ui.primitives.begin(), ui.primitives.end(), [&](const auto& primitive) {
            return primitive.name == name;
        }) != ui.primitives.end();
    };
    auto modifierNamed = [&](const std::string& name) {
        return std::find_if(ui.modifiers.begin(), ui.modifiers.end(), [&](const auto& modifier) {
            return modifier.name == name;
        }) != ui.modifiers.end();
    };

    EXPECT_TRUE(primitiveNamed("panel"));
    EXPECT_TRUE(primitiveNamed("grid"));
    EXPECT_TRUE(primitiveNamed("metric"));
    EXPECT_TRUE(primitiveNamed("modal"));
    EXPECT_TRUE(modifierNamed("cols"));
    EXPECT_TRUE(modifierNamed("tone"));
    EXPECT_TRUE(modifierNamed("surface"));
    EXPECT_TRUE(contains(ui.themeTokenKinds, "color"));
    EXPECT_TRUE(contains(ui.themeTokenKinds, "shadow"));
}

TEST(SemanticProgram, ComponentDefinitionsExposeOwnedSemantics) {
    const std::string source =
        "jtml 2\n"
        "make Card title\n"
        "  let open = true\n"
        "  get status = open ? \"Open\" : \"Closed\"\n"
        "  when toggle\n"
        "    open = !open\n"
        "  effect open\n"
        "    open = open\n"
        "  panel title title\n"
        "    button \"Toggle\" click toggle\n"
        "    text status\n"
        "    slot\n"
        "page\n"
        "  Card \"Account\"\n"
        "    text \"Child content\"\n";

    auto program = parseFriendly(source);
    const auto semantic = jtml::analyzeSemanticProgram(program, source);
    const auto* card = findComponentDefinition(semantic, "Card");
    ASSERT_NE(card, nullptr);

    EXPECT_EQ(card->params, std::vector<std::string>({"title"}));
    EXPECT_TRUE(contains(card->localState, "open"));
    EXPECT_TRUE(contains(card->localDerived, "status"));
    EXPECT_TRUE(contains(card->localActions, "toggle"));
    EXPECT_TRUE(contains(card->localEffects, "open"));
    EXPECT_TRUE(contains(card->eventBindings, "toggle"));
    EXPECT_TRUE(card->hasSlot);
}

TEST(SemanticProgram, ComponentDefinitionsExposeDeclaredEmits) {
    const std::string source =
        "jtml 2\n"
        "make Child emits picked(name: string) cancelled\n"
        "  button \"Pick\" click picked(\"Ada\")\n"
        "page\n"
        "  Child\n";

    auto program = parseFriendly(source);
    const auto semantic = jtml::analyzeSemanticProgram(program, source);
    const auto* child = findComponentDefinition(semantic, "Child");
    ASSERT_NE(child, nullptr);
    ASSERT_EQ(child->emits.size(), 2u);
    EXPECT_TRUE(contains(child->emits, "cancelled"));
    EXPECT_TRUE(contains(child->emits, "picked"));
    ASSERT_TRUE(child->emitArity.count("picked"));
    EXPECT_EQ(child->emitArity.at("picked"), 1);
    ASSERT_TRUE(child->emitPayloads.count("picked"));
    ASSERT_EQ(child->emitPayloads.at("picked").size(), 1u);
    EXPECT_EQ(child->emitPayloads.at("picked")[0], "name");
    ASSERT_TRUE(child->emitPayloadTypes.count("picked"));
    ASSERT_EQ(child->emitPayloadTypes.at("picked").size(), 1u);
    EXPECT_EQ(child->emitPayloadTypes.at("picked")[0], "string");
    ASSERT_TRUE(child->emitArity.count("cancelled"));
    EXPECT_EQ(child->emitArity.at("cancelled"), 0);
    ASSERT_TRUE(child->emitPayloads.count("cancelled"));
    EXPECT_TRUE(child->emitPayloads.at("cancelled").empty());

    const auto plan = jtml::buildRuntimePlan(program, semantic);
    ASSERT_EQ(plan.componentDefinitions.size(), 1u);
    EXPECT_EQ(plan.componentDefinitions[0].emits, child->emits);
    EXPECT_EQ(plan.componentDefinitions[0].emitArity.at("picked"), 1);
    ASSERT_TRUE(plan.componentDefinitions[0].emitPayloads.count("picked"));
    EXPECT_EQ(plan.componentDefinitions[0].emitPayloads.at("picked")[0], "name");
    ASSERT_TRUE(plan.componentDefinitions[0].emitPayloadTypes.count("picked"));
    EXPECT_EQ(plan.componentDefinitions[0].emitPayloadTypes.at("picked")[0], "string");
}

TEST(SemanticProgram, ComponentDefinitionManifestsExposeOwnedSemantics) {
    const std::string source =
        "jtml 2\n"
        "make Counter label\n"
        "  let count = 0\n"
        "  when add\n"
        "    count += 1\n"
        "  box\n"
        "    button \"+\" click add\n"
        "    slot\n"
        "page\n"
        "  Counter \"First\"\n";

    auto program = parseFriendly(source);
    JtmlTranspiler transpiler;
    transpiler.setBrowserLocalRuntime(true);
    const std::string html = transpiler.transpile(program);

    EXPECT_NE(html.find("\"localState\":[\"count\"]"), std::string::npos) << html;
    EXPECT_NE(html.find("\"localActions\":[\"add\"]"), std::string::npos) << html;
    EXPECT_NE(html.find("\"eventBindings\":[\"add\"]"), std::string::npos) << html;
    EXPECT_NE(html.find("\"hasSlot\":true"), std::string::npos) << html;
    EXPECT_NE(html.find("\"bodyNodeCount\":"), std::string::npos) << html;
    EXPECT_NE(html.find("\"rootTemplateNodeCount\":1"), std::string::npos) << html;
    EXPECT_NE(html.find("\"slotCount\":1"), std::string::npos) << html;
    EXPECT_NE(html.find("runtimePlan: {"), std::string::npos) << html;
    EXPECT_NE(html.find("localState: Array.isArray(record.localState)"), std::string::npos)
        << html;
}

TEST(SemanticProgram, WarnsOnMismatchedSemanticUiModifiers) {
    const std::string source =
        "jtml 2\n"
        "page\n"
        "  card cols 3\n"
        "    text \"Cards do not own grid columns\"\n"
        "  shell tone danger\n"
        "    content\n"
        "      text \"Tone belongs on content surfaces\"\n";

    auto program = parseFriendly(source);
    const auto semantic = jtml::analyzeSemanticProgram(program, source);
    const auto usage = jtml::analyzeSemanticUsage(semantic);

    EXPECT_TRUE(hasModifier(semantic, "card", "cols", "3"));
    EXPECT_TRUE(hasModifier(semantic, "shell", "tone", "danger"));
    EXPECT_TRUE(std::any_of(usage.warnings.begin(), usage.warnings.end(), [](const auto& warning) {
        return warning.code == "JTML_UI_COLS_ON_NON_GRID";
    }));
    EXPECT_TRUE(std::any_of(usage.warnings.begin(), usage.warnings.end(), [](const auto& warning) {
        return warning.code == "JTML_UI_TONE_ON_LAYOUT";
    }));
}

TEST(SemanticProgram, WarnsOnInvalidSemanticUiModifierValues) {
    const std::string source =
        "jtml 2\n"
        "page\n"
        "  grid cols 9 gap huge\n"
        "    card tone loud\n"
        "      text \"Invalid visual tokens\"\n";

    auto program = parseFriendly(source);
    const auto semantic = jtml::analyzeSemanticProgram(program, source);
    const auto usage = jtml::analyzeSemanticUsage(semantic);

    EXPECT_TRUE(hasModifier(semantic, "grid", "cols", "9"));
    EXPECT_TRUE(hasModifier(semantic, "grid", "gap", "huge"));
    EXPECT_TRUE(hasModifier(semantic, "card", "tone", "loud"));

    const auto invalidValueWarnings = std::count_if(
        usage.warnings.begin(),
        usage.warnings.end(),
        [](const auto& warning) {
            return warning.code == "JTML_UI_INVALID_MODIFIER_VALUE";
        });
    EXPECT_EQ(invalidValueWarnings, 3);
}

TEST(SemanticProgram, WarnsOnUnlabeledSemanticUiSurfacesAndOverlays) {
    const std::string source =
        "jtml 2\n"
        "page\n"
        "  panel\n"
        "    text \"Missing label\"\n"
        "  card\n"
        "    text \"Also missing label\"\n"
        "  modal\n"
        "    text \"Confirm\"\n"
        "  drawer aria-label \"Filters\"\n"
        "    text \"Labeled overlay\"\n"
        "  panel title \"Usage\"\n"
        "    text \"Labeled surface\"\n";

    auto program = parseFriendly(source);
    const auto semantic = jtml::analyzeSemanticProgram(program, source);
    const auto usage = jtml::analyzeSemanticUsage(semantic);

    EXPECT_GE(semantic.uiUses.size(), 5u);
    EXPECT_TRUE(std::any_of(semantic.uiUses.begin(), semantic.uiUses.end(), [](const auto& use) {
        return use.primitive == "panel" && use.hasTitle;
    }));
    EXPECT_TRUE(std::any_of(semantic.uiUses.begin(), semantic.uiUses.end(), [](const auto& use) {
        return use.primitive == "drawer" && use.hasAriaLabel;
    }));

    const auto surfaceWarnings = std::count_if(
        usage.warnings.begin(),
        usage.warnings.end(),
        [](const auto& warning) {
            return warning.code == "JTML_UI_SURFACE_UNLABELED";
        });
    const auto overlayWarnings = std::count_if(
        usage.warnings.begin(),
        usage.warnings.end(),
        [](const auto& warning) {
            return warning.code == "JTML_UI_OVERLAY_UNLABELED";
        });
    EXPECT_EQ(surfaceWarnings, 2);
    EXPECT_EQ(overlayWarnings, 1);
}

TEST(SemanticProgram, WarnsOnOverlayWithoutDismissAction) {
    const std::string source =
        "jtml 2\n"
        "let open = true\n"
        "when closeModal\n"
        "  open = false\n"
        "page\n"
        "  modal title \"Confirm\"\n"
        "    text \"Missing exit\"\n"
        "  drawer aria-label \"Filters\"\n"
        "    button \"Close\" click closeModal\n";

    auto program = parseFriendly(source);
    const auto semantic = jtml::analyzeSemanticProgram(program, source);
    const auto usage = jtml::analyzeSemanticUsage(semantic);

    EXPECT_TRUE(std::any_of(semantic.uiUses.begin(), semantic.uiUses.end(), [](const auto& use) {
        return use.primitive == "drawer" && use.hasDismissAction;
    }));
    EXPECT_TRUE(std::any_of(semantic.uiUses.begin(), semantic.uiUses.end(), [](const auto& use) {
        return use.primitive == "modal" && use.hasTitle && !use.hasDismissAction;
    }));
    EXPECT_TRUE(std::any_of(usage.warnings.begin(), usage.warnings.end(), [](const auto& warning) {
        return warning.code == "JTML_UI_OVERLAY_WITHOUT_DISMISS";
    }));
}

TEST(SemanticProgram, WarnsOnUnwiredSemanticUiFormsAndTabs) {
    const std::string source =
        "jtml 2\n"
        "let email = \"\"\n"
        "let selected = \"overview\"\n"
        "when choose\n"
        "  selected = \"overview\"\n"
        "page\n"
        "  field\n"
        "    text \"Missing control\"\n"
        "  field\n"
        "    input \"Email\" into email\n"
        "  tabs\n"
        "    text \"Missing tab child\"\n"
        "  tabs\n"
        "    tab \"Overview\" click choose\n"
        "  tab \"Decorative\"\n";

    auto program = parseFriendly(source);
    const auto semantic = jtml::analyzeSemanticProgram(program, source);
    const auto usage = jtml::analyzeSemanticUsage(semantic);

    EXPECT_TRUE(std::any_of(semantic.uiUses.begin(), semantic.uiUses.end(), [](const auto& use) {
        return use.primitive == "field" && use.hasControl;
    }));
    EXPECT_TRUE(std::any_of(semantic.uiUses.begin(), semantic.uiUses.end(), [](const auto& use) {
        return use.primitive == "tabs" && use.hasTabChild;
    }));
    EXPECT_TRUE(std::any_of(semantic.uiUses.begin(), semantic.uiUses.end(), [](const auto& use) {
        return use.primitive == "tab" && use.hasActionBinding;
    }));

    EXPECT_TRUE(std::any_of(usage.warnings.begin(), usage.warnings.end(), [](const auto& warning) {
        return warning.code == "JTML_UI_FIELD_WITHOUT_CONTROL";
    }));
    EXPECT_TRUE(std::any_of(usage.warnings.begin(), usage.warnings.end(), [](const auto& warning) {
        return warning.code == "JTML_UI_TABS_EMPTY";
    }));
    EXPECT_TRUE(std::any_of(usage.warnings.begin(), usage.warnings.end(), [](const auto& warning) {
        return warning.code == "JTML_UI_TAB_WITHOUT_ACTION";
    }));
}

TEST(SemanticProgram, WarnsOnUnlabeledFieldControl) {
    const std::string source =
        "jtml 2\n"
        "let name = \"\"\n"
        "let email = \"\"\n"
        "page\n"
        "  field\n"
        "    input into name\n"
        "  field\n"
        "    text \"Email\"\n"
        "    input into email\n";

    auto program = parseFriendly(source);
    const auto semantic = jtml::analyzeSemanticProgram(program, source);
    const auto usage = jtml::analyzeSemanticUsage(semantic);

    EXPECT_TRUE(std::any_of(semantic.uiUses.begin(), semantic.uiUses.end(), [](const auto& use) {
        return use.primitive == "field" && use.hasControl && use.hasLabel;
    }));
    EXPECT_TRUE(std::any_of(semantic.uiUses.begin(), semantic.uiUses.end(), [](const auto& use) {
        return use.primitive == "field" && use.hasControl && !use.hasLabel;
    }));
    EXPECT_TRUE(std::any_of(usage.warnings.begin(), usage.warnings.end(), [](const auto& warning) {
        return warning.code == "JTML_UI_FIELD_UNLABELED";
    }));
}

TEST(SemanticProgram, WarnsOnInvalidComponentIntoTargets) {
    const std::string source =
        "jtml 2\n"
        "make Form initial\n"
        "  let email = \"\"\n"
        "  get label = email\n"
        "  field\n"
        "    input \"Email\" into email\n"
        "    input \"Missing\" into missing\n"
        "    input \"Derived\" into label\n"
        "page\n"
        "  Form \"seed\"\n";

    auto program = parseFriendly(source);
    const auto semantic = jtml::analyzeSemanticProgram(program, source);
    const auto* form = findComponentDefinition(semantic, "Form");
    ASSERT_NE(form, nullptr);
    EXPECT_TRUE(contains(form->localBindings, "email"));
    EXPECT_TRUE(contains(form->localBindings, "missing"));
    EXPECT_TRUE(contains(form->localBindings, "label"));

    const auto usage = jtml::analyzeSemanticUsage(semantic);
    auto hasWarning = [&](const std::string& code) {
        return std::any_of(usage.warnings.begin(), usage.warnings.end(), [&](const auto& warning) {
            return warning.code == code;
        });
    };
    EXPECT_FALSE(std::any_of(usage.warnings.begin(), usage.warnings.end(), [](const auto& warning) {
        return warning.message.find("into email") != std::string::npos;
    })) << "valid local let binding should stay clean";
    EXPECT_TRUE(hasWarning("JTML_UNKNOWN_INTO_TARGET"));
    EXPECT_TRUE(hasWarning("JTML_INTO_DERIVED_TARGET"));
}

TEST(SemanticProgram, WarnsOnSemanticIntoTargetsThatCannotBeWritten) {
    jtml::SemanticProgram semantic;
    semantic.componentDefinitions.push_back({
        "Form",
        {"initial"},
        {},
        {},
        {},
        {},
        {"email"},
        {"label"},
        {},
        {},
        {},
        {"email", "missing", "label", "initial", "user.name"},
        "",
        false,
        0,
        0,
        0,
        1
    });

    const auto usage = jtml::analyzeSemanticUsage(semantic);
    auto hasWarning = [&](const std::string& code) {
        return std::any_of(usage.warnings.begin(), usage.warnings.end(), [&](const auto& warning) {
            return warning.code == code;
        });
    };

    EXPECT_TRUE(hasWarning("JTML_UNKNOWN_INTO_TARGET"));
    EXPECT_TRUE(hasWarning("JTML_INTO_DERIVED_TARGET"));
    EXPECT_TRUE(hasWarning("JTML_INTO_PARAM_TARGET"));
    EXPECT_TRUE(hasWarning("JTML_INVALID_INTO_TARGET"));
}

TEST(SemanticProgram, CapturesRawHtmlEscapeHatches) {
    const std::string source =
        "jtml 2\n"
        "css raw\n"
        "  third-party-card { display: block; }\n"
        "extern notify from \"host.notify\"\n"
        "page\n"
        "  html raw \"<third-party-card></third-party-card>\"\n"
        "  button \"Notify\" click notify(\"Saved\")\n";

    auto program = parseFriendly(source);
    const auto semantic = jtml::analyzeSemanticProgram(program, source);
    const auto usage = jtml::analyzeSemanticUsage(semantic);

    EXPECT_EQ(semantic.styleBlocks, 1);
    EXPECT_EQ(semantic.rawCssBlocks, 1);
    EXPECT_EQ(semantic.rawHtmlBlocks, 1);
    ASSERT_EQ(semantic.externs.size(), 1u);
    EXPECT_EQ(semantic.externs.front(), "notify");
    EXPECT_TRUE(hasEdge(semantic, "extern:notify", "host.notify", "calls"));
    EXPECT_TRUE(std::any_of(usage.warnings.begin(), usage.warnings.end(), [](const auto& warning) {
        return warning.code == "JTML_RAW_CSS_ESCAPE_HATCH";
    }));
    EXPECT_TRUE(std::any_of(usage.warnings.begin(), usage.warnings.end(), [](const auto& warning) {
        return warning.code == "JTML_RAW_HTML_ESCAPE_HATCH";
    }));
    EXPECT_TRUE(std::any_of(usage.warnings.begin(), usage.warnings.end(), [](const auto& warning) {
        return warning.code == "JTML_EXTERN_ESCAPE_HATCH";
    }));
}

TEST(SemanticProgram, WarnsWhenSemanticPrimitiveUsesInlineStyle) {
    const std::string source =
        "jtml 2\n"
        "page\n"
        "  panel title \"Usage\" style \"padding: 24px\"\n"
        "    text \"Prefer modifiers\"\n";

    auto program = parseFriendly(source);
    const auto semantic = jtml::analyzeSemanticProgram(program, source);
    const auto usage = jtml::analyzeSemanticUsage(semantic);

    EXPECT_EQ(semantic.rawStyleAttributeCount, 1);
    EXPECT_EQ(semantic.semanticPrimitiveRawStyleCount, 1);
    EXPECT_TRUE(std::any_of(usage.warnings.begin(), usage.warnings.end(), [](const auto& warning) {
        return warning.code == "JTML_RAW_STYLE_ON_UI_PRIMITIVE";
    }));
}

TEST(SemanticProgram, CanonicalizesFriendlyRouteFallbacks) {
    const std::string source =
        "jtml 2\n"
        "\n"
        "make Home\n"
        "  page\n"
        "    h1 \"Home\"\n"
        "make UserProfile id\n"
        "  page\n"
        "    h1 id\n"
        "make NotFound\n"
        "  page\n"
        "    h1 \"Missing\"\n"
        "\n"
        "route \"/\" as Home\n"
        "route \"/user/:id\" as UserProfile\n"
        "route \"*\" as NotFound\n";

    auto program = parseFriendly(source);
    const auto semantic = jtml::analyzeSemanticProgram(program, source);

    ASSERT_EQ(semantic.routes.size(), 3u) << semantic.routes.size();
    EXPECT_TRUE(contains(semantic.routes, "/ -> Home"));
    EXPECT_TRUE(contains(semantic.routes, "/user/:id -> UserProfile"));
    EXPECT_TRUE(contains(semantic.routes, "* -> NotFound"));
    ASSERT_EQ(semantic.routeRecords.size(), 3u) << semantic.routeRecords.size();
    const auto* userRoute = findRouteRecord(semantic, "/user/:id");
    ASSERT_NE(userRoute, nullptr);
    EXPECT_EQ(userRoute->component, "UserProfile");
    ASSERT_EQ(userRoute->params.size(), 1u);
    EXPECT_EQ(userRoute->params.front(), "id");
    EXPECT_TRUE(hasEdge(semantic, "route:/user/:id", "component:UserProfile", "renders"));
}

TEST(SemanticProgram, PreservesRouteDeclarationPrecedenceThroughClientManifest) {
    // Both orders are deliberate: preserve first-match semantics, do not rank
    // wildcard/static/parameter patterns or alphabetize route names.
    for (const bool fallbackFirst : {false, true}) {
        const std::string fallback = "route \"*\" as Missing\n";
        const std::string source = std::string("jtml 2\n") +
            (fallbackFirst ? fallback : "") +
            "route \"/\" as Home\n"
            "route \"/user/new\" as NewUser\n"
            "route \"/user/:id\" as Profile\n" +
            (fallbackFirst ? "" : fallback) +
            // Preserve the old exact-record deduplication without moving its first occurrence.
            "route \"/user/new\" as NewUser\n"
            "make Home\n  h1 \"Home\"\n"
            "make NewUser\n  h1 \"New\"\n"
            "make Profile id\n  h1 id\n"
            "make Missing\n  h1 \"Missing\"\n";
        auto program = parseFriendly(source);
        const auto semantic = jtml::analyzeSemanticProgram(program, source);
        const auto plan = jtml::buildRuntimePlan(program, semantic);
        const auto manifest = jtml::runtimePlanToClientJson(plan);
        std::vector<std::string> expected = {"/", "/user/new", "/user/:id"};
        expected.insert(fallbackFirst ? expected.begin() : expected.end(), "*");
        ASSERT_EQ(semantic.routeRecords.size(), expected.size());
        ASSERT_EQ(plan.routes.size(), expected.size());
        ASSERT_EQ(manifest["routes"].size(), expected.size());
        for (size_t i = 0; i < expected.size(); ++i) {
            EXPECT_EQ(semantic.routeRecords[i].path, expected[i]);
            EXPECT_EQ(plan.routes[i].path, expected[i]);
            EXPECT_EQ(manifest["routes"][i]["path"], expected[i]);
        }
    }
}

TEST(SemanticProgram, CapturesFetchOptionsInStructuredRecords) {
    const std::string source =
        "jtml 2\n"
        "\n"
        "let email = \"ada@example.com\"\n"
        "let login = fetch \"/api/login\" method \"POST\" body { email: email } cache \"no-store\" credentials \"include\" timeout 2500 retry 2 stale keep group auth key email dedupe every 30000 background lazy refresh reloadLogin\n"
        "\n"
        "page\n"
        "  button \"Retry\" click reloadLogin\n";

    auto program = parseFriendly(source);
    const auto semantic = jtml::analyzeSemanticProgram(program, source);

    const auto* login = findFetchRecord(semantic, "login");
    ASSERT_NE(login, nullptr);
    EXPECT_EQ(login->url, "/api/login");
    EXPECT_EQ(login->method, "POST");
    EXPECT_EQ(login->bodyExpr, "{ email: email }");
    EXPECT_EQ(login->cache, "no-store");
    EXPECT_EQ(login->credentials, "include");
    EXPECT_EQ(login->timeoutMs, "2500");
    EXPECT_EQ(login->retryCount, "2");
    EXPECT_EQ(login->stalePolicy, "keep");
    EXPECT_EQ(login->group, "auth");
    EXPECT_EQ(login->cacheKeyExpr, "email");
    EXPECT_EQ(login->revalidateMs, "30000");
    EXPECT_TRUE(login->dedupe);
    EXPECT_TRUE(login->background);
    EXPECT_EQ(login->refreshAction, "reloadLogin");
    EXPECT_TRUE(login->lazy);
    EXPECT_TRUE(hasEdge(semantic, "fetch:login", "reloadLogin", "refresh-action"));
}

TEST(SemanticProgram, CapturesRouteLoadsInStructuredRecords) {
    const std::string source =
        "jtml 2\n"
        "\n"
        "let users = fetch \"/api/users\" lazy\n"
        "make Users\n"
        "  page\n"
        "    h1 \"Users\"\n"
        "route \"/users\" as Users load users\n";

    auto program = parseFriendly(source);
    const auto semantic = jtml::analyzeSemanticProgram(program, source);

    const auto* usersRoute = findRouteRecord(semantic, "/users");
    ASSERT_NE(usersRoute, nullptr);
    EXPECT_EQ(usersRoute->component, "Users");
    ASSERT_EQ(usersRoute->loads.size(), 1u);
    EXPECT_EQ(usersRoute->loads.front(), "users");
    EXPECT_TRUE(hasEdge(semantic, "route:/users", "users", "loads"));
}

TEST(SemanticProgram, CapturesFriendlyImportShapes) {
    const std::string source =
        "jtml 2\n"
        "\n"
        "use \"./side-effects.jtml\"\n"
        "use Widget from \"./widget.jtml\"\n"
        "use { formatMoney, parseDate } from \"./money.jtml\"\n"
        "\n"
        "page\n"
        "  h1 \"Imports\"\n";

    auto program = parseFriendly(source);
    const auto semantic = jtml::analyzeSemanticProgram(program, source);

    ASSERT_EQ(semantic.imports.size(), 3u) << semantic.imports.size();
    ASSERT_EQ(semantic.importRecords.size(), 3u) << semantic.importRecords.size();
    EXPECT_TRUE(contains(semantic.imports, "./side-effects.jtml"));
    EXPECT_TRUE(contains(semantic.imports, "./widget.jtml"));
    EXPECT_TRUE(contains(semantic.imports, "./money.jtml"));
    const auto* sideEffect = findImportRecord(semantic, "./side-effects.jtml");
    ASSERT_NE(sideEffect, nullptr);
    EXPECT_EQ(sideEffect->kind, "side-effect");
    EXPECT_TRUE(sideEffect->names.empty());
    EXPECT_EQ(sideEffect->sourceLine, 3);
    EXPECT_EQ(sideEffect->sourceColumn, 1);
    const auto* widget = findImportRecord(semantic, "./widget.jtml");
    ASSERT_NE(widget, nullptr);
    EXPECT_EQ(widget->kind, "named");
    ASSERT_EQ(widget->names.size(), 1u);
    EXPECT_EQ(widget->names[0], "Widget");
    EXPECT_EQ(widget->sourceLine, 4);
    EXPECT_EQ(widget->sourceColumn, 1);
    const auto* money = findImportRecord(semantic, "./money.jtml");
    ASSERT_NE(money, nullptr);
    EXPECT_EQ(money->kind, "destructured");
    ASSERT_EQ(money->names.size(), 2u);
    EXPECT_EQ(money->names[0], "formatMoney");
    EXPECT_EQ(money->names[1], "parseDate");
    EXPECT_EQ(money->sourceLine, 5);
    EXPECT_EQ(money->sourceColumn, 1);
    EXPECT_TRUE(hasEdge(semantic, "module", "./side-effects.jtml", "imports"));
    EXPECT_TRUE(hasEdge(semantic, "module", "./widget.jtml", "imports"));
    EXPECT_TRUE(hasEdge(semantic, "module", "./money.jtml", "imports"));
}

TEST(SemanticProgram, CapturesFriendlyReExportImportRecords) {
    const std::string source =
        "jtml 2\n"
        "export use Card from \"./card.jtml\"\n";

    auto program = parseFriendly(source);
    const auto semantic = jtml::analyzeSemanticProgram(program, source);

    const auto* record = findImportRecord(semantic, "./card.jtml");
    ASSERT_NE(record, nullptr);
    EXPECT_EQ(record->kind, "named");
    ASSERT_EQ(record->names.size(), 1u);
    EXPECT_EQ(record->names[0], "Card");
    EXPECT_TRUE(record->reExport);
    EXPECT_EQ(record->sourceLine, 2);
    EXPECT_EQ(record->sourceColumn, 1);
    ASSERT_EQ(semantic.exportRecords.size(), 1u);
    EXPECT_EQ(semantic.exportRecords[0].name, "Card");
    EXPECT_EQ(semantic.exportRecords[0].kind, "re-export");
    EXPECT_EQ(semantic.exportRecords[0].specifier, "./card.jtml");
    EXPECT_TRUE(semantic.exportRecords[0].reExport);
    EXPECT_EQ(semantic.exportRecords[0].sourceLine, 2);
    EXPECT_TRUE(hasEdge(semantic, "module:re-export", "./card.jtml", "imports"));
    EXPECT_TRUE(hasEdge(semantic, "module", "Card", "re-exports"));
}

TEST(SemanticProgram, CapturesFriendlyExportedDeclarations) {
    const std::string source =
        "jtml 2\n"
        "export let theme = \"dark\"\n"
        "export get label = \"Theme {theme}\"\n"
        "export when save\n"
        "  show \"Saved\"\n"
        "export make Card title\n"
        "  text title\n"
        "make Home\n"
        "  text \"Home\"\n"
        "export route \"/\" as Home\n";

    auto program = parseFriendly(source);
    const auto semantic = jtml::analyzeSemanticProgram(program, source);

    ASSERT_EQ(semantic.exportRecords.size(), 5u);
    EXPECT_EQ(semantic.exportRecords[0].name, "Card");
    EXPECT_EQ(semantic.exportRecords[0].kind, "make");
    EXPECT_EQ(semantic.exportRecords[1].name, "Home");
    EXPECT_EQ(semantic.exportRecords[1].kind, "route");
    EXPECT_EQ(semantic.exportRecords[2].name, "label");
    EXPECT_EQ(semantic.exportRecords[2].kind, "get");
    EXPECT_EQ(semantic.exportRecords[3].name, "save");
    EXPECT_EQ(semantic.exportRecords[3].kind, "when");
    EXPECT_EQ(semantic.exportRecords[4].name, "theme");
    EXPECT_EQ(semantic.exportRecords[4].kind, "let");
    EXPECT_TRUE(hasEdge(semantic, "module", "Card", "exports"));
    EXPECT_TRUE(hasEdge(semantic, "module", "theme", "exports"));
}

TEST(SemanticProgram, KeepsMultipleImportRecordsForSameSpecifier) {
    const std::string source =
        "jtml 2\n"
        "use Card from \"./ui.jtml\"\n"
        "use Button from \"./ui.jtml\"\n";

    auto program = parseFriendly(source);
    const auto semantic = jtml::analyzeSemanticProgram(program, source);

    const auto records = findImportRecords(semantic, "./ui.jtml");
    ASSERT_EQ(records.size(), 2u) << semantic.importRecords.size();
    EXPECT_EQ(records[0]->kind, "named");
    EXPECT_EQ(records[1]->kind, "named");
}

TEST(SemanticProject, BuildsModuleRecordsFromLinkedProgram) {
    jtml::SemanticProgram semantic;
    semantic.moduleFiles = {
        "/app/index.jtml",
        "/app/components/ui.jtml",
    };
    semantic.importRecords.push_back({
        "./components/ui.jtml",
        "named",
        {"Card"},
        false,
    });

    const auto project = jtml::buildSemanticProject(semantic, "/app/index.jtml");

    ASSERT_EQ(project.modules.size(), 2u);
    EXPECT_EQ(project.entry, 0u);
    EXPECT_EQ(project.modules[0].path, "/app/index.jtml");
    ASSERT_EQ(project.modules[0].imports.size(), 1u);
    EXPECT_EQ(project.modules[0].imports[0].specifier, "./components/ui.jtml");
    ASSERT_EQ(project.modules[0].imports[0].names.size(), 1u);
    EXPECT_EQ(project.modules[0].imports[0].names[0], "Card");
    EXPECT_EQ(project.modules[0].imports[0].importer, project.entry);
    EXPECT_EQ(project.modules[0].imports[0].resolved, 1u);
    EXPECT_EQ(project.modules[0].imports[0].resolvedPath, "/app/components/ui.jtml");
}

TEST(SemanticProject, UnresolvedImportsUseExplicitInvalidModuleId) {
    jtml::SemanticProgram semantic;
    semantic.moduleFiles = {
        "/app/index.jtml",
    };
    semantic.importRecords.push_back({
        "./missing.jtml",
        "named",
        {"Missing"},
        false,
    });

    const auto project = jtml::buildSemanticProject(semantic, "/app/index.jtml");

    ASSERT_EQ(project.modules.size(), 1u);
    ASSERT_EQ(project.modules[0].imports.size(), 1u);
    EXPECT_EQ(project.modules[0].imports[0].importer, project.entry);
    EXPECT_EQ(project.modules[0].imports[0].resolved, jtml::InvalidSemanticModuleId);
    EXPECT_EQ(project.modules[0].imports[0].resolvedPath, "/app/missing.jtml");
}

TEST(SemanticProject, PerFileModuleSourcesPreserveImporterOwnership) {
    auto dashboardAst = parseFriendly(
        "jtml 2\n"
        "use appState from \"../stores/app-state.jtml\"\n"
        "export make Dashboard\n"
        "  text appState.theme\n");
    auto dashboardIr = jtml::summarizeSemanticAst(dashboardAst, "friendly");
    auto dashboardOwnedAst = jtml::cloneSemanticAst(dashboardAst, "friendly");

    std::vector<jtml::SemanticModuleSource> modules = {
        {
            "/app/index.jtml",
            jtml::analyzeSemanticProgram({}, "jtml 2\nuse Dashboard from \"./pages/dashboard.jtml\"\n"),
        },
        {
            "/app/pages/dashboard.jtml",
            jtml::analyzeSemanticProgram(
                dashboardAst,
                "jtml 2\n"
                "use appState from \"../stores/app-state.jtml\"\n"
                "export make Dashboard\n"
                "  text appState.theme\n"),
            dashboardIr,
            dashboardOwnedAst,
        },
        {
            "/app/stores/app-state.jtml",
            jtml::analyzeSemanticProgram({}, "jtml 2\nexport store appState\n  let theme = \"light\"\n"),
        },
    };

    const auto project = jtml::buildSemanticProject(modules, "/app/index.jtml");

    ASSERT_EQ(project.modules.size(), 3u);
    EXPECT_EQ(project.entry, 0u);
    ASSERT_EQ(project.modules[0].imports.size(), 1u);
    ASSERT_EQ(project.modules[1].imports.size(), 1u);
    ASSERT_EQ(project.modules[2].exports.size(), 1u);
    ASSERT_EQ(project.modules[1].semantic.components.size(), 1u);
    ASSERT_EQ(project.modules[2].semantic.stores.size(), 1u);
    EXPECT_EQ(project.modules[0].imports[0].importer, 0u);
    EXPECT_EQ(project.modules[0].imports[0].resolved, 1u);
    ASSERT_EQ(project.modules[0].imports[0].resolvedSymbols.size(), 1u);
    EXPECT_EQ(project.modules[0].imports[0].resolvedSymbols[0].name, "Dashboard");
    EXPECT_EQ(project.modules[0].imports[0].resolvedSymbols[0].kind, "make");
    EXPECT_EQ(project.modules[0].imports[0].resolvedSymbols[0].module, 1u);
    EXPECT_EQ(project.modules[1].imports[0].importer, 1u);
    EXPECT_EQ(project.modules[1].imports[0].resolved, 2u);
    ASSERT_EQ(project.modules[1].imports[0].resolvedSymbols.size(), 1u);
    EXPECT_EQ(project.modules[1].imports[0].resolvedSymbols[0].name, "appState");
    EXPECT_EQ(project.modules[1].imports[0].resolvedSymbols[0].kind, "store");
    EXPECT_EQ(project.modules[1].imports[0].resolvedSymbols[0].module, 2u);
    EXPECT_EQ(project.modules[1].semantic.components[0], "Dashboard");
    EXPECT_TRUE(project.modules[1].ir.available);
    EXPECT_EQ(project.modules[1].ir.syntax, "friendly");
    EXPECT_GE(project.modules[1].ir.totalNodeCount, 1u);
    ASSERT_TRUE(project.modules[1].ast);
    EXPECT_TRUE(project.modules[1].ast->available);
    EXPECT_EQ(project.modules[1].ast->syntax, "friendly");
    ASSERT_EQ(project.modules[1].ast->nodes.size(), dashboardAst.size());
    ASSERT_TRUE(project.modules[1].ast->nodes[0]);
    EXPECT_NE(project.modules[1].ast->nodes[0].get(), dashboardAst[0].get());
    EXPECT_EQ(project.modules[1].ast->nodes[0]->getType(), dashboardAst[0]->getType());
    ASSERT_FALSE(project.modules[1].ir.topLevelNodes.empty());
    EXPECT_EQ(project.modules[1].ir.topLevelNodes[0].kind, "JtmlElement");
    const auto hasImportCount = std::any_of(
        project.modules[1].ir.nodeCounts.begin(),
        project.modules[1].ir.nodeCounts.end(),
        [](const auto& count) {
            return count.kind == "ImportStatement" && count.count == 1u;
        });
    EXPECT_TRUE(hasImportCount);
    EXPECT_EQ(project.modules[2].semantic.stores[0], "appState");
    EXPECT_EQ(project.modules[2].exports[0].name, "appState");
    EXPECT_EQ(project.modules[2].exports[0].kind, "store");
    EXPECT_TRUE(project.modules[2].imports.empty());
}

TEST(SemanticProject, AnalysisReportsUnresolvedImportsAndMissingExports) {
    std::vector<jtml::SemanticModuleSource> modules = {
        {
            "/app/index.jtml",
            jtml::analyzeSemanticProgram(
                {},
                "jtml 2\n"
                "use Missing from \"./missing.jtml\"\n"
                "use { Card, Button } from \"./ui.jtml\"\n"),
        },
        {
            "/app/ui.jtml",
            jtml::analyzeSemanticProgram(
                {},
                "jtml 2\n"
                "export make Card title\n"
                "  text title\n"),
        },
    };

    const auto project = jtml::buildSemanticProject(modules, "/app/index.jtml");
    const auto issues = jtml::analyzeSemanticProject(project);

    ASSERT_EQ(issues.size(), 2u);
    EXPECT_EQ(issues[0].code, "JTML_UNRESOLVED_IMPORT");
    EXPECT_EQ(issues[0].specifier, "./missing.jtml");
    EXPECT_EQ(issues[0].resolvedPath, "/app/missing.jtml");
    EXPECT_EQ(issues[0].path, "/app/index.jtml");
    EXPECT_EQ(issues[0].line, 2u);
    EXPECT_EQ(issues[0].column, 1u);
    EXPECT_EQ(issues[1].code, "JTML_MISSING_EXPORT");
    EXPECT_EQ(issues[1].specifier, "./ui.jtml");
    EXPECT_EQ(issues[1].path, "/app/index.jtml");
    EXPECT_EQ(issues[1].line, 3u);
    EXPECT_EQ(issues[1].column, 1u);
    ASSERT_EQ(issues[1].requested.size(), 1u);
    ASSERT_EQ(issues[1].available.size(), 1u);
    EXPECT_EQ(issues[1].requested[0], "Button");
    EXPECT_EQ(issues[1].available[0], "Card");
}

TEST(SemanticProject, AnalysisReportsRecoverableModuleParseIssues) {
    jtml::SemanticModuleIr brokenIr;
    brokenIr.available = false;
    brokenIr.syntax = "unknown";
    brokenIr.parseError = "Parser error at line 3, column 5: Expected expression";
    brokenIr.parseErrorLine = 3;
    brokenIr.parseErrorColumn = 5;

    std::vector<jtml::SemanticModuleSource> modules = {
        {
            "/app/index.jtml",
            jtml::analyzeSemanticProgram(
                {},
                "jtml 2\n"
                "use \"./broken.jtml\"\n"),
        },
        {
            "/app/broken.jtml",
            jtml::analyzeSemanticProgram({}, "jtml 2\nmake Broken\n  if\n"),
            brokenIr,
            nullptr,
        },
    };

    const auto project = jtml::buildSemanticProject(modules, "/app/index.jtml");
    const auto issues = jtml::analyzeSemanticProject(project);

    ASSERT_EQ(issues.size(), 1u);
    EXPECT_EQ(issues[0].code, "JTML_MODULE_PARSE");
    EXPECT_EQ(issues[0].module, 1u);
    EXPECT_EQ(issues[0].path, "/app/broken.jtml");
    EXPECT_EQ(issues[0].resolvedPath, "/app/broken.jtml");
    EXPECT_EQ(issues[0].line, 3u);
    EXPECT_EQ(issues[0].column, 5u);
    EXPECT_NE(issues[0].message.find("Cannot parse module /app/broken.jtml"),
              std::string::npos);
}

TEST(SemanticProject, ImportIdentityFollowsReExportsToUltimateSymbol) {
    std::vector<jtml::SemanticModuleSource> modules = {
        {
            "/app/index.jtml",
            jtml::analyzeSemanticProgram(
                {},
                "jtml 2\n"
                "use Card from \"./components/index.jtml\"\n"),
        },
        {
            "/app/components/index.jtml",
            jtml::analyzeSemanticProgram(
                {},
                "jtml 2\n"
                "export use Card from \"./card.jtml\"\n"),
        },
        {
            "/app/components/card.jtml",
            jtml::analyzeSemanticProgram(
                {},
                "jtml 2\n"
                "export make Card title\n"
                "  h2 title\n"),
        },
    };

    const auto project = jtml::buildSemanticProject(modules, "/app/index.jtml");

    ASSERT_EQ(project.modules.size(), 3u);
    ASSERT_EQ(project.modules[0].imports.size(), 1u);
    ASSERT_EQ(project.modules[0].imports[0].resolvedSymbols.size(), 1u);
    EXPECT_EQ(project.modules[0].imports[0].resolvedSymbols[0].name, "Card");
    EXPECT_EQ(project.modules[0].imports[0].resolvedSymbols[0].kind, "make");
    EXPECT_EQ(project.modules[0].imports[0].resolvedSymbols[0].module, 2u);
    ASSERT_EQ(project.modules[1].imports.size(), 1u);
    ASSERT_EQ(project.modules[1].imports[0].resolvedSymbols.size(), 1u);
    EXPECT_EQ(project.modules[1].imports[0].resolvedSymbols[0].name, "Card");
    EXPECT_EQ(project.modules[1].imports[0].resolvedSymbols[0].kind, "make");
    EXPECT_EQ(project.modules[1].imports[0].resolvedSymbols[0].module, 2u);
}

TEST(SemanticProject, AnalysisReportsUnresolvedReExportTargets) {
    std::vector<jtml::SemanticModuleSource> modules = {
        {
            "/app/index.jtml",
            jtml::analyzeSemanticProgram(
                {},
                "jtml 2\n"
                "use Card from \"./components/index.jtml\"\n"),
        },
        {
            "/app/components/index.jtml",
            jtml::analyzeSemanticProgram(
                {},
                "jtml 2\n"
                "export use Card from \"./missing-card.jtml\"\n"),
        },
    };

    const auto project = jtml::buildSemanticProject(modules, "/app/index.jtml");
    ASSERT_EQ(project.modules.size(), 2u);
    ASSERT_EQ(project.modules[0].imports.size(), 1u);
    EXPECT_TRUE(project.modules[0].imports[0].resolvedSymbols.empty());

    const auto issues = jtml::analyzeSemanticProject(project);
    const auto unresolvedReExport = std::find_if(
        issues.begin(),
        issues.end(),
        [](const auto& issue) {
            return issue.code == "JTML_UNRESOLVED_REEXPORT";
        });
    ASSERT_NE(unresolvedReExport, issues.end());
    EXPECT_EQ(unresolvedReExport->module, 0u);
    EXPECT_EQ(unresolvedReExport->specifier, "./components/index.jtml");
    ASSERT_EQ(unresolvedReExport->requested.size(), 1u);
    EXPECT_EQ(unresolvedReExport->requested[0], "Card");
    EXPECT_EQ(unresolvedReExport->resolvedPath, "/app/components/missing-card.jtml");
}

TEST(SemanticProject, AnalysisReportsReExportCycles) {
    std::vector<jtml::SemanticModuleSource> modules = {
        {
            "/app/index.jtml",
            jtml::analyzeSemanticProgram(
                {},
                "jtml 2\n"
                "use Card from \"./components/a.jtml\"\n"),
        },
        {
            "/app/components/a.jtml",
            jtml::analyzeSemanticProgram(
                {},
                "jtml 2\n"
                "export use Card from \"./b.jtml\"\n"),
        },
        {
            "/app/components/b.jtml",
            jtml::analyzeSemanticProgram(
                {},
                "jtml 2\n"
                "export use Card from \"./a.jtml\"\n"),
        },
    };

    const auto project = jtml::buildSemanticProject(modules, "/app/index.jtml");
    ASSERT_EQ(project.modules.size(), 3u);
    ASSERT_EQ(project.modules[0].imports.size(), 1u);
    EXPECT_TRUE(project.modules[0].imports[0].resolvedSymbols.empty());

    const auto issues = jtml::analyzeSemanticProject(project);
    const auto cycle = std::find_if(
        issues.begin(),
        issues.end(),
        [](const auto& issue) {
            return issue.code == "JTML_REEXPORT_CYCLE";
        });
    ASSERT_NE(cycle, issues.end());
    EXPECT_EQ(cycle->module, 0u);
    EXPECT_EQ(cycle->specifier, "./components/a.jtml");
    ASSERT_EQ(cycle->requested.size(), 1u);
    EXPECT_EQ(cycle->requested[0], "Card");
}

TEST(SemanticProgram, UsageAnalysisComesFromSemanticGraph) {
    const std::string source =
        "jtml 2\n"
        "\n"
        "let count = 0\n"
        "let hero = \"/hero.png\"\n"
        "let items = [\"A\", \"B\"]\n"
        "let draft = \"hidden\"\n"
        "let hidden = \"secret\"\n"
        "let users = fetch \"/api/users\"\n"
        "get label = \"Count {count}\"\n"
        "get unused = \"not shown\"\n"
        "\n"
        "when add\n"
        "  count += 1\n"
        "  draft = hidden\n"
        "\n"
        "when never\n"
        "  hidden = \"changed\"\n"
        "\n"
        "page\n"
        "  show label\n"
        "  image src hero alt \"Hero\"\n"
        "  if count > 0\n"
        "    text \"Positive\"\n"
        "  while count < 3\n"
        "    text \"Loop guard\"\n"
        "  for item in items\n"
        "    text item\n"
        "  button \"Add\" click add\n";

    auto program = parseFriendly(source);
    const auto semantic = jtml::analyzeSemanticProgram(program, source);
    const auto usage = jtml::analyzeSemanticUsage(semantic);

    EXPECT_TRUE(contains(usage.observedState, "count"));
    EXPECT_TRUE(contains(usage.observedState, "hero"));
    EXPECT_TRUE(contains(usage.observedState, "items"));
    EXPECT_TRUE(contains(usage.observedDerived, "label"));
    EXPECT_TRUE(contains(usage.boundActions, "add"));
    EXPECT_TRUE(contains(usage.unboundActions, "never"));
    EXPECT_TRUE(contains(usage.zombieState, "draft"));
    EXPECT_TRUE(contains(usage.zombieState, "hidden"));
    EXPECT_TRUE(contains(usage.unusedDerived, "unused"));
    EXPECT_FALSE(contains(usage.deadState, "users"));
    ASSERT_FALSE(usage.actionProfiles.empty());
    EXPECT_TRUE(std::any_of(usage.actionProfiles.begin(), usage.actionProfiles.end(), [](const auto& profile) {
        return profile.name == "add" &&
               contains(profile.writes, "count") &&
               contains(profile.reads, "hidden") &&
               contains(profile.triggers, "ui") &&
               profile.hasVisibleEffect;
    }));
    EXPECT_TRUE(std::any_of(usage.warnings.begin(), usage.warnings.end(), [](const auto& warning) {
        return warning.code == "JTML_UNUSED_DERIVED" && warning.message.find("unused") != std::string::npos;
    }));
    EXPECT_TRUE(std::any_of(usage.warnings.begin(), usage.warnings.end(), [](const auto& warning) {
        return warning.code == "JTML_UNBOUND_ACTION" && warning.message.find("never") != std::string::npos;
    }));
}

TEST(RuntimePlan, OwnsBrowserLocalRuntimeShapeBeforeManifestEmission) {
    const std::string source =
        "jtml 2\n"
        "\n"
        "let count = 0\n"
        "let users = fetch \"/api/users\" retry 2 refresh reloadUsers\n"
        "get label = \"Count {count}\"\n"
        "\n"
        "when add step\n"
        "  if step\n"
        "    count += step\n"
        "  else\n"
        "    count += 1\n"
        "\n"
        "when reloadUsers\n"
        "  invalidate users\n"
        "\n"
        "make Counter title\n"
        "  let draft = \"\"\n"
        "  card title title\n"
        "    input \"Draft\" into draft\n"
        "    text label\n"
        "\n"
        "route \"/:title\" as Counter\n"
        "\n"
        "page\n"
        "  Counter \"Home\"\n"
        "  button \"Add\" click add(1)\n";

    auto program = parseFriendly(source);
    const auto semantic = jtml::analyzeSemanticProgram(program, source);
    const auto plan = jtml::buildRuntimePlan(program, semantic);

    const auto* count = findRuntimeBinding(plan.state, "count");
    ASSERT_NE(count, nullptr);
    EXPECT_EQ(count->expr, "0.000000000000000");
    EXPECT_NE(findRuntimeBinding(plan.state, "users"), nullptr);
    EXPECT_NE(findRuntimeBinding(plan.state, "title"), nullptr);
    const auto* label = findRuntimeBinding(plan.derived, "label");
    ASSERT_NE(label, nullptr);
    EXPECT_EQ(label->expr, "(\"Count \" + count)");

    ASSERT_EQ(plan.fetches.size(), 1u);
    EXPECT_EQ(plan.fetches[0].name, "users");
    EXPECT_EQ(plan.fetches[0].url, "/api/users");
    EXPECT_EQ(plan.fetches[0].retryCount, "2");
    EXPECT_EQ(plan.fetches[0].refreshAction, "reloadUsers");

    ASSERT_EQ(plan.routes.size(), 1u);
    EXPECT_EQ(plan.routes[0].path, "/:title");
    EXPECT_EQ(plan.routes[0].component, "Counter");

    const auto* add = findRuntimeAction(plan.actions, "add");
    ASSERT_NE(add, nullptr);
    ASSERT_EQ(add->params.size(), 1u);
    EXPECT_EQ(add->params[0], "step");
    ASSERT_EQ(add->body.size(), 1u);
    EXPECT_EQ(add->body[0].kind, "if");
    EXPECT_EQ(add->body[0].condition, "step");
    ASSERT_EQ(add->body[0].thenStatements.size(), 1u);
    EXPECT_EQ(add->body[0].thenStatements[0].lhs, "count");
    EXPECT_EQ(add->body[0].thenStatements[0].expr, "(count + step)");
    ASSERT_EQ(add->body[0].elseStatements.size(), 1u);
    EXPECT_EQ(add->body[0].elseStatements[0].expr, "(count + 1.000000000000000)");
    EXPECT_NE(findRuntimeAction(plan.actions, "reloadUsers"), nullptr);

    const auto planJson = jtml::runtimePlanToClientJson(plan);
    ASSERT_TRUE(planJson.contains("statePlans")) << planJson.dump(2);
    EXPECT_EQ(planJson["statePlans"]["count"].value("producer", ""),
              "typed-ir") << planJson.dump(2);
    ASSERT_TRUE(planJson.contains("derivedPlans")) << planJson.dump(2);
    EXPECT_EQ(planJson["derivedPlans"]["label"].value("producer", ""),
              "typed-ir") << planJson.dump(2);
    ASSERT_TRUE(planJson.contains("actions")) << planJson.dump(2);
    ASSERT_TRUE(planJson["actions"].is_object()) << planJson.dump(2);
    ASSERT_TRUE(planJson["actions"].contains("add")) << planJson.dump(2);
    const auto& actionJson = planJson["actions"]["add"];
    ASSERT_TRUE(actionJson.contains("body")) << actionJson.dump(2);
    ASSERT_EQ(actionJson["body"][0].value("kind", ""), "if") << actionJson.dump(2);
    EXPECT_EQ(actionJson["body"][0]["conditionPlan"].value("producer", ""),
              "typed-ir") << actionJson.dump(2);
    EXPECT_EQ(actionJson["body"][0]["then"][0]["exprPlan"].value("producer", ""),
              "typed-ir") << actionJson.dump(2);

    ASSERT_EQ(plan.componentDefinitions.size(), 1u);
    EXPECT_EQ(plan.componentDefinitions[0].name, "Counter");
    EXPECT_NE(plan.componentDefinitions[0].bodySource.find("text label"),
              std::string::npos);
    EXPECT_FALSE(plan.componentDefinitions[0].bodyHex.empty());
    ASSERT_GE(plan.componentDefinitions[0].bodyPlan.size(), 2u);
    EXPECT_TRUE(std::any_of(
        plan.componentDefinitions[0].bodyPlan.begin(),
        plan.componentDefinitions[0].bodyPlan.end(),
        [](const auto& node) {
            return node.kind == "template" && node.name == "card" && node.renderRoot;
        }));
    EXPECT_TRUE(std::any_of(
        plan.componentDefinitions[0].bodyPlan.begin(),
        plan.componentDefinitions[0].bodyPlan.end(),
        [](const auto& node) {
            return node.kind == "template" && node.name == "text" && !node.renderRoot;
        }));
    auto cardIt = std::find_if(
        plan.componentDefinitions[0].bodyPlan.begin(),
        plan.componentDefinitions[0].bodyPlan.end(),
        [](const auto& node) {
            return node.kind == "template" && node.name == "card" && node.renderRoot;
    });
    ASSERT_NE(cardIt, plan.componentDefinitions[0].bodyPlan.end());
    EXPECT_EQ(cardIt->sourceLine, 18);
    EXPECT_GT(cardIt->sourceColumn, 0);
    const int cardIndex = static_cast<int>(
        std::distance(plan.componentDefinitions[0].bodyPlan.begin(), cardIt));
    EXPECT_FALSE(cardIt->childIndices.empty());
    EXPECT_EQ(cardIt->parentIndex, -1);
    EXPECT_TRUE(std::any_of(
        plan.componentDefinitions[0].bodyPlan.begin(),
        plan.componentDefinitions[0].bodyPlan.end(),
        [&](const auto& node) {
            return node.kind == "template" && node.name == "text" &&
                   node.parentIndex == cardIndex &&
                   node.expression == "label" &&
                   contains(node.reads, "label") &&
                   node.writes.empty();
        }));
    EXPECT_TRUE(std::any_of(
        plan.componentDefinitions[0].bodyPlan.begin(),
        plan.componentDefinitions[0].bodyPlan.end(),
        [](const auto& node) {
            return node.kind == "template" && node.name == "card" &&
                   contains(node.reads, "title") &&
                   node.writes.empty();
        }));
    EXPECT_TRUE(std::any_of(
        plan.componentDefinitions[0].bodyPlan.begin(),
        plan.componentDefinitions[0].bodyPlan.end(),
        [](const auto& node) {
            return node.kind == "template" && node.name == "input" &&
                   node.sourceLine == 19 &&
                   node.sourceColumn > 0 &&
                   contains(node.writes, "draft");
        }));
    EXPECT_TRUE(std::any_of(
        plan.componentInstances.begin(),
        plan.componentInstances.end(),
        [](const auto& instance) { return instance.component == "Counter"; }));
}

TEST(RuntimePlan, ComponentInstancesCarryAuthoredSlotPlansAndSplitParams) {
    const std::string source =
        "jtml 2\n"
        "make Card title tone\n"
        "  panel class tone\n"
        "    h2 title\n"
        "    slot\n"
        "page\n"
        "  Card \"Status\" \"good\"\n"
        "    text \"Ready\"\n";

    auto program = parseFriendly(source);
    const auto semantic = jtml::analyzeSemanticProgram(program, source);
    const auto plan = jtml::buildRuntimePlan(program, semantic);

    ASSERT_EQ(plan.componentDefinitions.size(), 1u);
    ASSERT_EQ(plan.componentDefinitions[0].params.size(), 2u);
    EXPECT_EQ(plan.componentDefinitions[0].params[0], "title");
    EXPECT_EQ(plan.componentDefinitions[0].params[1], "tone");
    ASSERT_EQ(plan.componentInstances.size(), 1u);
    EXPECT_FALSE(plan.componentInstances[0].slotHex.empty());
    EXPECT_NE(plan.componentInstances[0].slotSource.find("text \"Ready\""),
              std::string::npos);
    ASSERT_EQ(plan.componentInstances[0].slotPlan.size(), 1u);
    EXPECT_EQ(plan.componentInstances[0].slotPlan[0].kind, "template");
    EXPECT_EQ(plan.componentInstances[0].slotPlan[0].name, "text");
    EXPECT_EQ(plan.componentInstances[0].slotPlan[0].expression, "\"Ready\"");
}

TEST(RuntimePlan, ComponentBodySourceLinesSurviveBlankLinesCommentsAndSlots) {
    const std::string source =
        "jtml 2\n"
        "\n"
        "// first component starts after a blank line\n"
        "make Card title\n"
        "  let localTitle = title\n"
        "\n"
        "  panel title localTitle\n"
        "    // nested body comment should not become a node\n"
        "    text localTitle\n"
        "    slot\n"
        "\n"
        "make Host\n"
        "  Card \"Welcome\"\n"
        "    text \"Slotted\"\n"
        "\n"
        "page\n"
        "  Host\n";

    auto program = parseFriendly(source);
    const auto semantic = jtml::analyzeSemanticProgram(program, source);
    const auto plan = jtml::buildRuntimePlan(program, semantic);

    auto findDefinition = [&](const std::string& name) {
        return std::find_if(
            plan.componentDefinitions.begin(),
            plan.componentDefinitions.end(),
            [&](const auto& definition) { return definition.name == name; });
    };

    auto card = findDefinition("Card");
    ASSERT_NE(card, plan.componentDefinitions.end());
    auto cardNode = [&](const std::string& kind, const std::string& name) {
        return std::find_if(card->bodyPlan.begin(), card->bodyPlan.end(),
                            [&](const auto& node) {
                                return node.kind == kind && node.name == name;
                            });
    };
    const auto localTitle = cardNode("state", "localTitle");
    ASSERT_NE(localTitle, card->bodyPlan.end());
    EXPECT_EQ(localTitle->sourceLine, lineContaining(source, "let localTitle = title"));
    EXPECT_EQ(localTitle->sourceColumn, 3);

    const auto panel = cardNode("template", "panel");
    ASSERT_NE(panel, card->bodyPlan.end());
    EXPECT_EQ(panel->sourceLine, lineContaining(source, "panel title localTitle"));
    EXPECT_EQ(panel->sourceColumn, 3);
    const int panelIndex =
        static_cast<int>(std::distance(card->bodyPlan.begin(), panel));

    const auto text = cardNode("template", "text");
    ASSERT_NE(text, card->bodyPlan.end());
    EXPECT_EQ(text->sourceLine, lineContaining(source, "text localTitle"));
    EXPECT_EQ(text->sourceColumn, 5);
    EXPECT_EQ(text->parentIndex, panelIndex);

    const auto slot = std::find_if(card->bodyPlan.begin(), card->bodyPlan.end(),
                                   [](const auto& node) {
                                       return node.kind == "slot" &&
                                              node.head == "slot";
                                   });
    ASSERT_NE(slot, card->bodyPlan.end());
    EXPECT_EQ(slot->sourceLine, lineContaining(source, "slot"));
    EXPECT_EQ(slot->sourceColumn, 5);
    EXPECT_EQ(slot->parentIndex, panelIndex);

    auto host = findDefinition("Host");
    ASSERT_NE(host, plan.componentDefinitions.end());
    auto hostCard = std::find_if(host->bodyPlan.begin(), host->bodyPlan.end(),
                                 [](const auto& node) {
                                     return node.kind == "template" &&
                                            node.name == "Card";
                                 });
    ASSERT_NE(hostCard, host->bodyPlan.end());
    EXPECT_EQ(hostCard->sourceLine, lineContaining(source, "Card \"Welcome\""));
    EXPECT_EQ(hostCard->sourceColumn, 3);

    const auto cardInstance = std::find_if(
        plan.componentInstances.begin(),
        plan.componentInstances.end(),
        [](const auto& instance) {
            return instance.component == "Card" && !instance.slotPlan.empty();
        });
    ASSERT_NE(cardInstance, plan.componentInstances.end());
    ASSERT_EQ(cardInstance->slotPlan.size(), 1u);
    EXPECT_EQ(cardInstance->slotPlan[0].sourceLine,
              lineContaining(source, "text \"Slotted\""));
    EXPECT_EQ(cardInstance->slotPlan[0].sourceColumn, 5);
}

TEST(RuntimePlan, CapitalizedComponentCallParamsAreReactiveReads) {
    const std::string source =
        "jtml 2\n"
        "make Child selected\n"
        "  text selected\n"
        "make Host\n"
        "  let selected = \"Alpha\"\n"
        "  Child selected\n"
        "page\n"
        "  Host\n";

    auto program = parseFriendly(source);
    const auto semantic = jtml::analyzeSemanticProgram(program, source);
    const auto plan = jtml::buildRuntimePlan(program, semantic);

    auto host = std::find_if(
        plan.componentDefinitions.begin(),
        plan.componentDefinitions.end(),
        [](const auto& definition) { return definition.name == "Host"; });
    ASSERT_NE(host, plan.componentDefinitions.end());
    EXPECT_TRUE(std::any_of(
        host->bodyPlan.begin(),
        host->bodyPlan.end(),
        [](const auto& node) {
            return node.kind == "template" &&
                   node.name == "Child" &&
                   node.expression == "selected" &&
                   contains(node.reads, "selected");
        }));
}

TEST(RuntimePlan, BodyPlanReadsComeFromParsedExpressionAst) {
    const std::string source =
        "jtml 2\n"
        "make Inspector users activeIndex fallback\n"
        "  let meta = { \"label\": users.data[activeIndex].name, \"literal\": \"users.fake\" }\n"
        "  get title = users.data[activeIndex].name ? users.data[activeIndex].name : fallback\n"
        "  card title title\n"
        "    text users.data[activeIndex].email\n"
        "    metric \"Users\" users.total \"Active\" tone good gap md\n"
        "    image src users.data[activeIndex].avatar alt \"Profile\"\n"
        "let seedUsers = []\n"
        "page\n"
        "  Inspector seedUsers 0 \"n/a\"\n";

    auto program = parseFriendly(source);
    const auto semantic = jtml::analyzeSemanticProgram(program, source);
    const auto plan = jtml::buildRuntimePlan(program, semantic);
    ASSERT_EQ(plan.componentDefinitions.size(), 1u);
    const auto& bodyPlan = plan.componentDefinitions[0].bodyPlan;

    auto findNode = [&](const std::string& kind, const std::string& name) {
        return std::find_if(bodyPlan.begin(), bodyPlan.end(), [&](const auto& node) {
            return node.kind == kind && node.name == name;
        });
    };

    auto stateIt = findNode("state", "meta");
    ASSERT_NE(stateIt, bodyPlan.end());
    EXPECT_TRUE(contains(stateIt->reads, "users"));
    EXPECT_TRUE(contains(stateIt->reads, "users.data"));
    EXPECT_TRUE(contains(stateIt->reads, "users.data[activeIndex]"));
    EXPECT_TRUE(contains(stateIt->reads, "users.data[activeIndex].name"));
    EXPECT_TRUE(contains(stateIt->reads, "activeIndex"));
    EXPECT_FALSE(contains(stateIt->reads, "label"));
    EXPECT_FALSE(contains(stateIt->reads, "literal"));
    EXPECT_FALSE(contains(stateIt->reads, "users.fake"));

    auto derivedIt = findNode("derived", "title");
    ASSERT_NE(derivedIt, bodyPlan.end());
    EXPECT_TRUE(contains(derivedIt->reads, "users"));
    EXPECT_TRUE(contains(derivedIt->reads, "users.data"));
    EXPECT_TRUE(contains(derivedIt->reads, "users.data[activeIndex]"));
    EXPECT_TRUE(contains(derivedIt->reads, "users.data[activeIndex].name"));
    EXPECT_TRUE(contains(derivedIt->reads, "activeIndex"));
    EXPECT_TRUE(contains(derivedIt->reads, "fallback"));
    EXPECT_FALSE(contains(derivedIt->reads, "data"));
    EXPECT_FALSE(contains(derivedIt->reads, "name"));

    auto cardIt = std::find_if(bodyPlan.begin(), bodyPlan.end(), [](const auto& node) {
        return node.kind == "template" && node.name == "card";
    });
    ASSERT_NE(cardIt, bodyPlan.end());
    EXPECT_TRUE(contains(cardIt->reads, "title"));

    auto metricIt = std::find_if(bodyPlan.begin(), bodyPlan.end(), [](const auto& node) {
        return node.kind == "template" && node.name == "metric";
    });
    ASSERT_NE(metricIt, bodyPlan.end());
    EXPECT_TRUE(contains(metricIt->reads, "users"));
    EXPECT_TRUE(contains(metricIt->reads, "users.total"));
    EXPECT_FALSE(contains(metricIt->reads, "total"));
    EXPECT_FALSE(contains(metricIt->reads, "good"));
    EXPECT_FALSE(contains(metricIt->reads, "md"));

    auto imageIt = std::find_if(bodyPlan.begin(), bodyPlan.end(), [](const auto& node) {
        return node.kind == "template" && node.name == "image";
    });
    ASSERT_NE(imageIt, bodyPlan.end());
    EXPECT_TRUE(contains(imageIt->reads, "users"));
    EXPECT_TRUE(contains(imageIt->reads, "users.data"));
    EXPECT_TRUE(contains(imageIt->reads, "users.data[activeIndex]"));
    EXPECT_TRUE(contains(imageIt->reads, "users.data[activeIndex].avatar"));
    EXPECT_TRUE(contains(imageIt->reads, "activeIndex"));
    EXPECT_TRUE(imageIt->writes.empty());
    EXPECT_FALSE(contains(imageIt->reads, "Profile"));
}

TEST(RuntimePlan, ConditionalReadSetsConservativelyCoverSupportedExpressions) {
    struct Case { std::string expression; std::vector<std::string> required; };
    const std::vector<Case> cases = {
        {"title == \"After\"", {"title"}},
        {"count + offset >= limit", {"count", "offset", "limit"}},
        {"enabled && title != fallback", {"enabled", "title", "fallback"}},
        {"enabled || selected", {"enabled", "selected"}},
        {"!enabled", {"enabled"}},
        {"item.score > limit", {"item", "item.score", "limit"}},
        {"items[index].score > limit", {"items", "index", "limit"}},
        {"enabled ? title : fallback", {"enabled", "title", "fallback"}},
        {"(count * offset - limit) / 2 != 0", {"count", "offset", "limit"}},
    };
    for (const auto& test : cases) {
        SCOPED_TRACE(test.expression);
        const auto ir = jtml::parseExpressionIr(test.expression);
        ASSERT_TRUE(ir);
        ASSERT_TRUE(jtml::expressionIrSupportsDirectJs(*ir));
        const auto dependencies = jtml::expressionIrDependencies(*ir);
        const std::string source = "jtml 2\nmake Probe title count offset limit enabled selected fallback item items index\n"
            "  if " + test.expression + "\n    p \"Then\"\n  else\n    p \"Else\"\n"
            "page\n  p \"Probe\"\n";
        auto program = parseFriendly(source);
        const auto plan = jtml::buildRuntimePlan(program);
        ASSERT_EQ(plan.componentDefinitions.size(), 1u);
        const auto& body = plan.componentDefinitions.front().bodyPlan;
        const auto node = std::find_if(body.begin(), body.end(), [](const auto& candidate) {
            return candidate.name == "if";
        });
        ASSERT_NE(node, body.end());
        jtml::RuntimeProjectPlan project;
        project.linkedPlan = plan;
        const auto module = jtml::emitStaticComponentModuleAsset(project);
        for (const auto& required : test.required) {
            EXPECT_NE(module.find("!writes['" + required + "']"), std::string::npos)
                << "Static update selector missing " << required;
            EXPECT_TRUE(contains(dependencies, required)) << "Expression IR missing " << required;
            EXPECT_TRUE(contains(node->reads, required)) << "Runtime Plan missing " << required;
        }
    }
}

TEST(RuntimePlan, ConditionalBranchesOwnAllReadsAndUnsupportedCallsFailClosed) {
    const std::string source =
        "jtml 2\nmake Branch enabled title fallback\n"
        "  if enabled\n    show title\n  else\n    show fallback\n"
        "page\n  p \"Branch\"\n";
    auto program = parseFriendly(source);
    jtml::RuntimeProjectPlan project;
    project.linkedPlan = jtml::buildRuntimePlan(program);
    const auto module = jtml::emitStaticComponentModuleAsset(project);
    EXPECT_NE(module.find("!writes['enabled'] && !writes['fallback'] && !writes['title']"), std::string::npos);
    EXPECT_NE(module.find("1:0,2:0,3:2"), std::string::npos)
        << "else descendants must be suppressed by the owning if region";
    EXPECT_TRUE(jtml::validateStaticComponentModuleForProduction(project).empty());

    auto unsupported = parseFriendly(
        "jtml 2\nmake Unsupported title\n  if mystery(title)\n    p \"Then\"\npage\n  p \"Unsupported\"\n");
    project.linkedPlan = jtml::buildRuntimePlan(unsupported);
    const auto issues = jtml::validateStaticComponentModuleForProduction(project);
    ASSERT_FALSE(issues.empty());
    EXPECT_NE(issues.front().find("unsupported production conditional expression"), std::string::npos);
    EXPECT_NE(issues.front().find("Unsupported"), std::string::npos);
}

TEST(RuntimePlan, BorrowedParameterWritesFailClosedThroughAliases) {
    for (const auto& assignment : {"item.score = 1", "alias.inner.score = 1", "item = {}"}) {
        SCOPED_TRACE(assignment);
        auto program = parseFriendly(
            "jtml 2\nmake Borrowed item\n  let alias = { \"inner\": item }\n  when mutate\n    " +
            std::string(assignment) + "\n  p \"Borrowed\"\npage\n  p \"Borrowed\"\n");
        jtml::RuntimeProjectPlan project;
        project.linkedPlan = jtml::buildRuntimePlan(program);
        const auto issues = jtml::validateStaticComponentModuleForProduction(project);
        ASSERT_FALSE(issues.empty());
        EXPECT_NE(issues.front().find("borrowed value"), std::string::npos);
        EXPECT_NE(issues.front().find("parameters are read-only"), std::string::npos);
    }
}

TEST(RuntimePlan, ProductionWritesRequireOwnedLetRootsWithoutTaintingTargetIndices) {
    for (const auto& statement : {"row.score = 1", "argument.score = 1"}) {
        auto program = parseFriendly(
            "jtml 2\nmake Unowned\n  when mutate argument\n    " + std::string(statement) +
            "\n  p \"Unowned\"\npage\n  p \"Unowned\"\n");
        jtml::RuntimeProjectPlan project;
        project.linkedPlan = jtml::buildRuntimePlan(program);
        EXPECT_FALSE(jtml::validateStaticComponentModuleForProduction(project).empty());
    }
    auto owned = parseFriendly(
        "jtml 2\nmake Owned index\n  let items = [{ \"score\": 0 }]\n"
        "  when mutate\n    items[index].score = 1\n  p \"Owned\"\npage\n  p \"Owned\"\n");
    jtml::RuntimeProjectPlan project;
    project.linkedPlan = jtml::buildRuntimePlan(owned);
    EXPECT_TRUE(jtml::validateStaticComponentModuleForProduction(project).empty());
}

TEST(RuntimePlan, LoopIteratorOwnershipFollowsLexicalCollectionsAndAliases) {
    struct Case { std::string header; std::string declarations; std::string action; bool accepted; };
    const std::vector<Case> cases = {
        {"items", "  let items = [{ \"score\": 0 }]\n", "  when mutate\n    items[0].score += 1\n", false},
        {"", "  let row = { \"score\": 0 }\n", "  when helper row\n    let temporary = 0\n  when mutate\n    row.score += 1\n", true},
        {"items", "  let alias = {}\n", "  when mutate\n    for row in items\n      alias = row\n      alias.score += 1\n", false},
        {"", "  let alias = {}\n", "  when mutate items\n    for row in items\n      alias = row\n      alias.score += 1\n", false},
        {"items", "  let alias = {}\n  let next = {}\n", "  when mutate\n    for row in items\n      alias = { \"inner\": row }\n      next = alias\n      next.inner.score += 1\n", false},
        {"items", "  let alias = {}\n", "  when mutate\n    for group in items\n      for row in group.rows\n        alias = row\n        alias.score += 1\n", false},
        {"row", "  let owned = [{ \"score\": 0 }]\n", "  when mutate\n    for row in owned\n      row.score += 1\n", true},
        {"items", "  let alias = {}\n", "  when mutate\n    let ownedItems = [{ \"score\": 0 }]\n    for row in ownedItems\n      alias = row\n      alias.score += 1\n", true},
        {"row", "  let owned = [{ \"score\": 0 }]\n  let alias = {}\n", "  when mutate\n    for row in owned\n      row.score += 1\n    alias = row\n    alias.score += 1\n", false},
        {"", "  let items = [{ \"score\": 0 }]\n  let alias = {}\n", "  when mutate items\n    for row in items\n      alias = row\n      alias.score += 1\n", false},
    };
    for (const auto& test : cases) {
        const auto source = "jtml 2\nmake Candidate " + test.header + "\n" + test.declarations + test.action + "  p \"Candidate\"\npage\n  p \"Test\"\n";
        SCOPED_TRACE(source);
        auto program = parseFriendly(source);
        jtml::RuntimeProjectPlan project;
        project.linkedPlan = jtml::buildRuntimePlan(program);
        const auto issues = jtml::validateStaticComponentModuleForProduction(project);
        EXPECT_EQ(issues.empty(), test.accepted);
        if (!test.accepted) {
            ASSERT_FALSE(issues.empty());
            EXPECT_NE(issues.front().find("borrowed"), std::string::npos) << issues.front();
            EXPECT_NE(issues.front().find("Candidate"), std::string::npos);
            EXPECT_NE(issues.front().find("line "), std::string::npos);
        }
    }
}

TEST(RuntimePlan, ProductionRejectsSharedScopeDeclarationCollisions) {
    struct Case { std::string declarations; std::string action; bool accepted; };
    const std::vector<Case> cases = {
        // Original review repro, deeper nesting, else, and a borrowed alias.
        {"  let item = { \"score\": 0 }\n", "  when mutate\n    if true\n      let item = input\n    item.score += 1\n", false},
        {"  let item = { \"score\": 0 }\n", "  when mutate\n    if true\n      if true\n        let item = input\n    item.score += 1\n", false},
        {"  let item = { \"score\": 0 }\n", "  when mutate\n    if false\n      let harmless = 0\n    else\n      let item = input\n    item.score += 1\n", false},
        {"  let alias = input\n  let item = { \"score\": 0 }\n", "  when mutate\n    if true\n      let item = alias\n    item.score += 1\n", false},
        {"  let items = [{ \"score\": 0 }]\n", "  when mutate\n    if true\n      let items = input\n    for row in items\n      row.score += 1\n", false},
        {"", "  when mutate\n    let input = [{ \"score\": 0 }]\n    for row in input\n      row.score += 1\n", false},
        // Both sibling branches, independent actions, formals and iterators share storage.
        {"", "  when mutate\n    if true\n      let local = input\n    else\n      let local = {}\n", false},
        {"", "  when capture\n    let local = input\n  when mutate\n    let local = {}\n", false},
        {"", "  when mutate input\n    let input = {}\n", false},
        {"  let rows = [{}]\n", "  when mutate\n    for row in rows\n      let row = input\n", false},
        {"  let item = {}\n", "  when mutate\n    if true\n      get item = input\n", false},
        // Unique locals and writes to enclosing owned state remain supported.
        {"  let item = { \"score\": 0 }\n", "  when mutate\n    if true\n      let borrowed = input\n      let local = { \"score\": 0 }\n      local.score += 1\n    item.score += 1\n", true},
    };
    for (const auto& test : cases) {
        const auto source = "jtml 2\nmake Candidate input\n" + test.declarations + test.action + "  p \"Candidate\"\npage\n  p \"Test\"\n";
        SCOPED_TRACE(source);
        auto program = parseFriendly(source);
        jtml::RuntimeProjectPlan project;
        project.linkedPlan = jtml::buildRuntimePlan(program);
        const auto issues = jtml::validateStaticComponentModuleForProduction(project);
        EXPECT_EQ(issues.empty(), test.accepted);
        if (!test.accepted) {
            ASSERT_FALSE(issues.empty());
            EXPECT_NE(issues.front().find("nested declaration"), std::string::npos) << issues.front();
            EXPECT_NE(issues.front().find("Candidate"), std::string::npos);
            EXPECT_NE(issues.front().find("line "), std::string::npos);
        }
    }
}

TEST(RuntimePlan, ProductionAssignmentsValidateRhsEntireLvalueAndOperator) {
    const std::vector<std::pair<std::string, bool>> cases = {
        {"result = input.push(1)", false},
        {"items[mystery()].score = 1", false},
        {"items[index + mystery()].score = 1", false},
        {"result = enabled ? 1 : mystery()", false},
        {"items[index].score = 1", true},
        {"items[index + 0].score += 1", true},
        {"result += 1", true}, {"result -= 1", true}, {"result *= 2", true},
        {"result /= 2", true}, {"result %= 2", true},
    };
    for (const auto& test : cases) {
        SCOPED_TRACE(test.first);
        auto program = parseFriendly("jtml 2\nmake Assignment\n  let input = []\n  let result = 0\n"
            "  let items = [{ \"score\": 0 }]\n  let index = 0\n  let enabled = true\n"
            "  when mutate\n    " + test.first + "\n  p \"Assignment\"\npage\n  p \"Test\"\n");
        jtml::RuntimeProjectPlan project;
        project.linkedPlan = jtml::buildRuntimePlan(program);
        const auto issues = jtml::validateStaticComponentModuleForProduction(project);
        EXPECT_EQ(issues.empty(), test.second);
        if (!test.second) {
            ASSERT_FALSE(issues.empty());
            EXPECT_NE(issues.front().find("unsupported production assignment"), std::string::npos);
            EXPECT_NE(issues.front().find("Assignment"), std::string::npos);
            EXPECT_NE(issues.front().find("statically analyzable indices and RHS"), std::string::npos);
        }
        if (test.second) {
            auto& body = project.linkedPlan.componentDefinitions.front().bodyPlan;
            const auto assignment = std::find_if(body.begin(), body.end(), [](const auto& node) { return node.kind == "assignment"; });
            ASSERT_NE(assignment, body.end());
            assignment->operatorToken = "^=";
            const auto invalid = jtml::validateStaticComponentModuleForProduction(project);
            EXPECT_FALSE(invalid.empty()) << "unsupported compound operator accepted";
        }
    }
}

TEST(RuntimePlan, LoopShadowingPreservesOuterCollectionDependency) {
    for (const bool derived : {false, true}) {
        SCOPED_TRACE(derived ? "derived collection" : "direct collection");
        auto program = parseFriendly(
            std::string("jtml 2\nmake Rows\n  let item = [1, 2]\n") +
            (derived ? "  get rows = item\n  for item in rows key item\n" : "  for item in item key item\n") +
            "    show item\npage\n  p \"Rows\"\n");
        jtml::RuntimeProjectPlan project;
        project.linkedPlan = jtml::buildRuntimePlan(program);
        const auto asset = jtml::emitStaticUpdatePlanAsset(project);
        const auto regionIndex = derived ? "2" : "1";
        EXPECT_NE(asset.find(std::string("\"entriesByRead\":{\"item\":[{\"index\":") + regionIndex + ",\"kind\":\"for\""), std::string::npos)
            << "outer item must select the loop region, not just the row-local show";
    }
}

TEST(RuntimePlan, ObservableAssignmentsInvalidatePathAndRoot) {
    const std::string source =
        "jtml 2\nmake Writes\n  let item = { \"score\": 0 }\n  let items = []\n  let index = 0\n"
        "  when change\n    item.score += 1\n    items[index].score = 2\n"
        "  if item.score > 0\n    p \"Changed\"\npage\n  p \"Writes\"\n";
    auto program = parseFriendly(source);
    const auto plan = jtml::buildRuntimePlan(program);
    ASSERT_EQ(plan.componentDefinitions.size(), 1u);
    const auto& body = plan.componentDefinitions.front().bodyPlan;
    size_t assignments = 0;
    for (const auto& node : body) {
        if (node.kind != "assignment") continue;
        ++assignments;
        EXPECT_TRUE(node.name == "item.score" || node.name == "items[index].score");
        const auto root = node.name == "item.score" ? "item" : "items";
        EXPECT_TRUE(contains(node.writes, node.name));
        EXPECT_TRUE(contains(node.writes, root));
        if (node.name == "items[index].score") EXPECT_TRUE(contains(node.reads, "index"));
    }
    EXPECT_EQ(assignments, 2u);
}

TEST(RuntimePlan, CanonicalExpressionPlanCoversCompositeOperators) {
    const auto directExpression = jtml::compileRuntimeExpressionPlan("users.data[index].name");
    ASSERT_EQ(directExpression["kind"], "member") << directExpression.dump(2);
    EXPECT_EQ(directExpression["producer"], "typed-ir") << directExpression.dump(2);
    EXPECT_EQ(directExpression["base"]["kind"], "subscript") << directExpression.dump(2);
    ASSERT_TRUE(directExpression.contains("sourceSpan")) << directExpression.dump(2);
    EXPECT_EQ(directExpression["sourceSpan"]["startLine"], 1) << directExpression.dump(2);
    EXPECT_EQ(directExpression["sourceSpan"]["startColumn"], 1) << directExpression.dump(2);
    EXPECT_GT(directExpression["sourceSpan"]["endColumn"].get<int>(),
              directExpression["sourceSpan"]["startColumn"].get<int>()) << directExpression.dump(2);

    const auto partial = jtml::compileRuntimeExpressionPlan("users.data extra");
    EXPECT_EQ(partial["kind"], "source") << partial.dump(2);
    EXPECT_FALSE(partial.contains("producer")) << partial.dump(2);
    EXPECT_FALSE(partial.contains("sourceSpan")) << partial.dump(2);

    const auto plan = jtml::compileRuntimeExpressionPlan(
        "(visible && title != \"Hidden\") ? items : empty");

    ASSERT_EQ(plan["kind"], "conditional") << plan.dump(2);
    EXPECT_EQ(plan["producer"], "typed-ir") << plan.dump(2);
    ASSERT_TRUE(plan.contains("test")) << plan.dump(2);
    EXPECT_EQ(plan["test"]["kind"], "binary") << plan.dump(2);
    EXPECT_EQ(plan["test"]["producer"], "typed-ir") << plan.dump(2);
    EXPECT_EQ(plan["test"]["operator"], "&&") << plan.dump(2);
    ASSERT_TRUE(plan["test"].contains("right")) << plan.dump(2);
    EXPECT_EQ(plan["test"]["right"]["kind"], "binary") << plan.dump(2);
    EXPECT_EQ(plan["test"]["right"]["operator"], "!=") << plan.dump(2);
    EXPECT_EQ(plan["whenTrue"]["kind"], "path") << plan.dump(2);
    EXPECT_EQ(plan["whenTrue"]["root"], "items") << plan.dump(2);
    EXPECT_EQ(plan["whenFalse"]["kind"], "path") << plan.dump(2);
    EXPECT_EQ(plan["whenFalse"]["root"], "empty") << plan.dump(2);

    const auto nested = jtml::compileRuntimeExpressionPlan(
        "title == \"Open\" ? item : \"none\"");
    ASSERT_EQ(nested["kind"], "conditional") << nested.dump(2);
    EXPECT_EQ(nested["producer"], "typed-ir") << nested.dump(2);
    EXPECT_EQ(nested["test"]["operator"], "==") << nested.dump(2);
    EXPECT_EQ(nested["whenTrue"]["kind"], "path") << nested.dump(2);
    EXPECT_EQ(nested["whenFalse"]["kind"], "literal") << nested.dump(2);

    const auto member = jtml::compileRuntimeExpressionPlan("users.data.active.name");
    ASSERT_EQ(member["kind"], "path") << member.dump(2);
    EXPECT_EQ(member["producer"], "typed-ir") << member.dump(2);
    ASSERT_EQ(member["segments"].size(), 4u) << member.dump(2);
    EXPECT_EQ(member["segments"][0], "users") << member.dump(2);
    EXPECT_EQ(member["segments"][3], "name") << member.dump(2);

    const auto subscript = jtml::compileRuntimeExpressionPlan("users.data[index].name");
    ASSERT_EQ(subscript["kind"], "member") << subscript.dump(2);
    EXPECT_EQ(subscript["producer"], "typed-ir") << subscript.dump(2);
    ASSERT_TRUE(subscript.contains("base")) << subscript.dump(2);
    EXPECT_EQ(subscript["property"], "name") << subscript.dump(2);
    ASSERT_EQ(subscript["base"]["kind"], "subscript") << subscript.dump(2);
    EXPECT_EQ(subscript["base"]["base"]["kind"], "path") << subscript.dump(2);
    EXPECT_EQ(subscript["base"]["index"]["kind"], "path") << subscript.dump(2);

    const auto array = jtml::compileRuntimeExpressionPlan("[title, \"Ready\", count + 1]");
    ASSERT_EQ(array["kind"], "array") << array.dump(2);
    EXPECT_EQ(array["producer"], "typed-ir") << array.dump(2);
    ASSERT_EQ(array["elements"].size(), 3u) << array.dump(2);
    EXPECT_EQ(array["elements"][0]["kind"], "path") << array.dump(2);
    EXPECT_EQ(array["elements"][2]["kind"], "binary") << array.dump(2);

    const auto object = jtml::compileRuntimeExpressionPlan("{ \"name\": title, \"count\": count }");
    ASSERT_EQ(object["kind"], "object") << object.dump(2);
    EXPECT_EQ(object["producer"], "typed-ir") << object.dump(2);
    EXPECT_EQ(object["valueType"], "object") << object.dump(2);
    ASSERT_EQ(object["entries"].size(), 2u) << object.dump(2);
    EXPECT_EQ(object["entries"][0]["key"], "name") << object.dump(2);
    EXPECT_EQ(object["entries"][0]["value"]["kind"], "path") << object.dump(2);

    const auto stringConcat = jtml::compileRuntimeExpressionPlan("\"Hello, \" + title");
    ASSERT_EQ(stringConcat["kind"], "binary") << stringConcat.dump(2);
    EXPECT_EQ(stringConcat["valueType"], "string") << stringConcat.dump(2);

    const auto interpolated = jtml::compileRuntimeExpressionPlan("\"Count {count}\"");
    ASSERT_EQ(interpolated["kind"], "binary") << interpolated.dump(2);
    EXPECT_EQ(interpolated["producer"], "typed-ir") << interpolated.dump(2);
    EXPECT_EQ(interpolated["valueType"], "string") << interpolated.dump(2);
    EXPECT_EQ(interpolated["operator"], "+") << interpolated.dump(2);
    EXPECT_EQ(interpolated["right"]["kind"], "path") << interpolated.dump(2);
    EXPECT_EQ(interpolated["right"]["root"], "count") << interpolated.dump(2);

    const auto escapedBraces = jtml::compileRuntimeExpressionPlan("\"Use \\{ and \\}\"");
    ASSERT_EQ(escapedBraces["kind"], "literal") << escapedBraces.dump(2);
    EXPECT_EQ(escapedBraces["value"], "Use { and }") << escapedBraces.dump(2);

    const auto quotedBrace = jtml::compileRuntimeExpressionPlan("\"Key {labels[\\\"}\\\"]}\"");
    ASSERT_EQ(quotedBrace["kind"], "binary") << quotedBrace.dump(2);
    EXPECT_EQ(quotedBrace["right"]["kind"], "subscript") << quotedBrace.dump(2);
    EXPECT_EQ(quotedBrace["right"]["index"]["kind"], "literal") << quotedBrace.dump(2);
    EXPECT_EQ(quotedBrace["right"]["index"]["value"], "}") << quotedBrace.dump(2);

    const auto objectInterpolation = jtml::compileRuntimeExpressionPlan(
        "\"Payload {{ \\\"name\\\": title }}\"");
    ASSERT_EQ(objectInterpolation["kind"], "binary") << objectInterpolation.dump(2);
    EXPECT_EQ(objectInterpolation["right"]["kind"], "object") << objectInterpolation.dump(2);
    ASSERT_EQ(objectInterpolation["right"]["entries"].size(), 1u)
        << objectInterpolation.dump(2);
    EXPECT_EQ(objectInterpolation["right"]["entries"][0]["key"], "name")
        << objectInterpolation.dump(2);

    const auto malformedInterpolation = jtml::compileRuntimeExpressionPlan("\"Keep {count + }\"");
    ASSERT_EQ(malformedInterpolation["kind"], "literal") << malformedInterpolation.dump(2);
    EXPECT_EQ(malformedInterpolation["value"], "Keep {count + }")
        << malformedInterpolation.dump(2);

    const auto numericMath = jtml::compileRuntimeExpressionPlan("count + 1");
    ASSERT_EQ(numericMath["kind"], "binary") << numericMath.dump(2);
    EXPECT_EQ(numericMath["valueType"], "unknown") << numericMath.dump(2);

    jtml::ExpressionTypeEnvironment environment;
    environment.symbols["count"] = jtml::ExpressionValueType::Number;
    environment.symbols["title"] = jtml::ExpressionValueType::String;
    environment.symbols["visible"] = jtml::ExpressionValueType::Boolean;

    const auto typedNumericMath = jtml::compileRuntimeExpressionPlan("count + 1", environment);
    ASSERT_EQ(typedNumericMath["kind"], "binary") << typedNumericMath.dump(2);
    EXPECT_EQ(typedNumericMath["producer"], "typed-ir") << typedNumericMath.dump(2);
    EXPECT_EQ(typedNumericMath["valueType"], "number") << typedNumericMath.dump(2);
    EXPECT_EQ(typedNumericMath["left"]["valueType"], "number") << typedNumericMath.dump(2);
    EXPECT_EQ(typedNumericMath["right"]["valueType"], "number") << typedNumericMath.dump(2);

    const auto typedStringConcat = jtml::compileRuntimeExpressionPlan("title + count", environment);
    ASSERT_EQ(typedStringConcat["kind"], "binary") << typedStringConcat.dump(2);
    EXPECT_EQ(typedStringConcat["valueType"], "string") << typedStringConcat.dump(2);

    const auto typedConditional = jtml::compileRuntimeExpressionPlan(
        "visible ? title : \"Hidden\"", environment);
    ASSERT_EQ(typedConditional["kind"], "conditional") << typedConditional.dump(2);
    EXPECT_EQ(typedConditional["test"]["valueType"], "boolean") << typedConditional.dump(2);
    EXPECT_EQ(typedConditional["valueType"], "string") << typedConditional.dump(2);

    const auto comparison = jtml::compileRuntimeExpressionPlan("count > 3");
    ASSERT_EQ(comparison["kind"], "binary") << comparison.dump(2);
    EXPECT_EQ(comparison["valueType"], "boolean") << comparison.dump(2);

    const auto range = jtml::compileRuntimeExpressionPlan("1..1000");
    ASSERT_EQ(range["kind"], "range") << range.dump(2);
    EXPECT_EQ(range["producer"], "typed-ir") << range.dump(2);
    EXPECT_EQ(range["valueType"], "array") << range.dump(2);
    EXPECT_EQ(range["start"]["kind"], "literal") << range.dump(2);
    EXPECT_EQ(range["end"]["kind"], "literal") << range.dump(2);
    EXPECT_EQ(range["directJs"], true) << range.dump(2);
    const auto rangeJs = range["jsExpression"].get<std::string>();
    EXPECT_NE(rangeJs.find("Number.isFinite"), std::string::npos) << rangeJs;
    EXPECT_EQ(rangeJs.find("new Function"), std::string::npos) << rangeJs;
    EXPECT_EQ(rangeJs.find("eval("), std::string::npos) << rangeJs;

    jtml::ExpressionTypeEnvironment rangeEnvironment;
    rangeEnvironment.symbols["start"] = jtml::ExpressionValueType::Number;
    rangeEnvironment.symbols["end"] = jtml::ExpressionValueType::Number;
    const auto dynamicRange = jtml::compileRuntimeExpressionPlan("start..end", rangeEnvironment);
    ASSERT_EQ(dynamicRange["kind"], "range") << dynamicRange.dump(2);
    EXPECT_EQ(dynamicRange["producer"], "typed-ir") << dynamicRange.dump(2);
    EXPECT_EQ(dynamicRange["valueType"], "array") << dynamicRange.dump(2);
    EXPECT_EQ(dynamicRange["start"]["kind"], "path") << dynamicRange.dump(2);
    EXPECT_EQ(dynamicRange["end"]["kind"], "path") << dynamicRange.dump(2);
    EXPECT_EQ(dynamicRange["directJs"], true) << dynamicRange.dump(2);
    EXPECT_NE(dynamicRange["jsExpression"].get<std::string>().find("scope[\"start\"]"),
              std::string::npos) << dynamicRange.dump(2);
}

TEST(RuntimePlan, TypeEnvironmentAnnotatesClientExpressionPlansFromBindings) {
    const std::string source =
        "jtml 2\n"
        "let count = 0\n"
        "let title = \"Count\"\n"
        "get label = title + count\n"
        "when inc\n"
        "  count = count + 1\n"
        "page\n"
        "  text label\n"
        "  button \"Inc\" click inc\n";

    auto program = parseFriendly(source);
    const auto plan = jtml::buildRuntimePlan(program);
    const auto client = jtml::runtimePlanToClientJson(plan);

    ASSERT_TRUE(client.contains("statePlans")) << client.dump(2);
    EXPECT_EQ(client["statePlans"]["count"]["valueType"], "number") << client.dump(2);
    EXPECT_EQ(client["statePlans"]["title"]["valueType"], "string") << client.dump(2);

    ASSERT_TRUE(client.contains("derivedPlans")) << client.dump(2);
    EXPECT_EQ(client["derivedPlans"]["label"]["valueType"], "string") << client.dump(2);
    EXPECT_EQ(client["derivedPlans"]["label"]["left"]["valueType"], "string")
        << client.dump(2);
    EXPECT_EQ(client["derivedPlans"]["label"]["right"]["valueType"], "number")
        << client.dump(2);

    ASSERT_TRUE(client.contains("actions")) << client.dump(2);
    ASSERT_TRUE(client["actions"].contains("inc")) << client.dump(2);
    ASSERT_FALSE(client["actions"]["inc"]["body"].empty()) << client.dump(2);
    const auto& assignment = client["actions"]["inc"]["body"][0];
    ASSERT_EQ(assignment["kind"], "assign") << client.dump(2);
    ASSERT_TRUE(assignment.contains("exprPlan")) << client.dump(2);
    EXPECT_EQ(assignment["exprPlan"]["valueType"], "number") << assignment.dump(2);
    EXPECT_EQ(assignment["exprPlan"]["left"]["valueType"], "number") << assignment.dump(2);
}

TEST(RuntimePlan, ClientExpressionPlansOmitCompilerHintsButExplainKeepsThem) {
    const std::string source =
        "jtml 2\n"
        "let count = 0\n"
        "when inc\n"
        "  count = count + 1\n"
        "make Counter\n"
        "  let local = count + 1\n"
        "  text local\n"
        "page\n"
        "  Counter\n";

    auto program = parseFriendly(source);
    const auto plan = jtml::buildRuntimePlan(program);
    const auto client = jtml::runtimePlanToClientJson(plan);
    const auto explain = jtml::runtimePlanToExplainJson(plan);

    const auto clientDump = client.dump();
    EXPECT_EQ(clientDump.find("\"sourceSpan\""), std::string::npos) << client.dump(2);
    EXPECT_EQ(clientDump.find("\"directJs\""), std::string::npos) << client.dump(2);
    EXPECT_EQ(clientDump.find("\"jsExpression\""), std::string::npos) << client.dump(2);

    ASSERT_TRUE(explain.contains("componentDefinitions")) << explain.dump(2);
    const auto explainDump = explain.dump();
    EXPECT_NE(explainDump.find("\"sourceSpan\""), std::string::npos)
        << explain.dump(2);
    EXPECT_NE(explainDump.find("\"directJs\""), std::string::npos)
        << explain.dump(2);
    EXPECT_NE(explainDump.find("\"jsExpression\""), std::string::npos)
        << explain.dump(2);
}

TEST(RuntimePlan, TypeEnvironmentAnnotatesComponentBodyPlansFromLocalState) {
    const std::string source =
        "jtml 2\n"
        "make Counter title\n"
        "  let count = 0\n"
        "  get label = title + count\n"
        "  when inc\n"
        "    count = count + 1\n"
        "  card\n"
        "    text label\n"
        "    button \"Inc\" click inc\n"
        "page\n"
        "  Counter \"Clicks\"\n";

    auto program = parseFriendly(source);
    const auto plan = jtml::buildRuntimePlan(program);
    const auto client = jtml::runtimePlanToClientJson(plan);

    ASSERT_TRUE(client.contains("componentDefinitions")) << client.dump(2);
    ASSERT_FALSE(client["componentDefinitions"].empty()) << client.dump(2);
    const auto& definition = client["componentDefinitions"][0];
    ASSERT_EQ(definition["name"], "Counter") << definition.dump(2);
    ASSERT_TRUE(definition.contains("bodyPlan")) << definition.dump(2);

    const nlohmann::json* stateNode = nullptr;
    const nlohmann::json* derivedNode = nullptr;
    const nlohmann::json* assignmentNode = nullptr;
    const nlohmann::json* textNode = nullptr;
    for (const auto& node : definition["bodyPlan"]) {
        const auto kind = node.value("kind", "");
        const auto name = node.value("name", "");
        if (kind == "state" && name == "count") stateNode = &node;
        if (kind == "derived" && name == "label") derivedNode = &node;
        if (kind == "assignment" && name == "count") assignmentNode = &node;
        if (kind == "template" && name == "text") textNode = &node;
    }

    ASSERT_NE(stateNode, nullptr) << definition.dump(2);
    EXPECT_EQ((*stateNode)["expressionPlan"]["valueType"], "number") << stateNode->dump(2);

    ASSERT_NE(derivedNode, nullptr) << definition.dump(2);
    EXPECT_EQ((*derivedNode)["expressionPlan"]["valueType"], "unknown") << derivedNode->dump(2);
    EXPECT_EQ((*derivedNode)["expressionPlan"]["right"]["valueType"], "number")
        << derivedNode->dump(2);

    ASSERT_NE(assignmentNode, nullptr) << definition.dump(2);
    EXPECT_EQ((*assignmentNode)["expressionPlan"]["valueType"], "number")
        << assignmentNode->dump(2);
    EXPECT_EQ((*assignmentNode)["expressionPlan"]["left"]["valueType"], "number")
        << assignmentNode->dump(2);

    ASSERT_NE(textNode, nullptr) << definition.dump(2);
    ASSERT_FALSE((*textNode)["wordPlans"].empty()) << textNode->dump(2);
    EXPECT_NE(textNode->dump().find("\"root\":\"label\""), std::string::npos)
        << textNode->dump(2);
}

TEST(RuntimePlan, TypeEnvironmentAnnotatesRangeLoopLocalsInBodyPlans) {
    const std::string source =
        "jtml 2\n"
        "make RangeList\n"
        "  for index in 1..3 key index\n"
        "    show index + 1\n"
        "page\n"
        "  RangeList\n";

    auto program = parseFriendly(source);
    const auto plan = jtml::buildRuntimePlan(program);
    const auto client = jtml::runtimePlanToClientJson(plan);

    ASSERT_TRUE(client.contains("componentDefinitions")) << client.dump(2);
    ASSERT_FALSE(client["componentDefinitions"].empty()) << client.dump(2);
    const auto& definition = client["componentDefinitions"][0];
    ASSERT_EQ(definition["name"], "RangeList") << definition.dump(2);

    const nlohmann::json* loopNode = nullptr;
    const nlohmann::json* renderedNode = nullptr;
    for (const auto& node : definition["bodyPlan"]) {
        if (node.value("head", "") == "for" || node.value("name", "") == "for") {
            loopNode = &node;
        }
        if (node.value("expression", "") == "index + 1") {
            renderedNode = &node;
        }
    }

    ASSERT_NE(loopNode, nullptr) << definition.dump(2);
    ASSERT_TRUE(loopNode->contains("loopPlan")) << loopNode->dump(2);
    EXPECT_EQ((*loopNode)["loopPlan"]["item"], "index") << loopNode->dump(2);
    EXPECT_EQ((*loopNode)["loopPlan"]["collectionPlan"]["kind"], "range")
        << loopNode->dump(2);
    EXPECT_EQ((*loopNode)["keyExpressionPlan"]["kind"], "path")
        << loopNode->dump(2);
    EXPECT_EQ((*loopNode)["keyExpressionPlan"]["valueType"], "number")
        << loopNode->dump(2);

    ASSERT_NE(renderedNode, nullptr) << definition.dump(2);
    ASSERT_TRUE(renderedNode->contains("expressionPlan")) << renderedNode->dump(2);
    EXPECT_EQ((*renderedNode)["expressionPlan"]["kind"], "binary")
        << renderedNode->dump(2);
    EXPECT_EQ((*renderedNode)["expressionPlan"]["valueType"], "number")
        << renderedNode->dump(2);
    EXPECT_EQ((*renderedNode)["expressionPlan"]["left"]["root"], "index")
        << renderedNode->dump(2);
    EXPECT_EQ((*renderedNode)["expressionPlan"]["left"]["valueType"], "number")
        << renderedNode->dump(2);
}

TEST(RuntimePlan, StaticUpdatePlanAssetUsesRangeLoopLocalTypeEnvironment) {
    const std::string source =
        "jtml 2\n"
        "make RangeList\n"
        "  let offset = 0\n"
        "  when bump\n"
        "    offset = offset + 1\n"
        "  for index in 1..3 key index\n"
        "    show index + offset\n"
        "  button \"Bump\" click bump\n"
        "page\n"
        "  RangeList\n";

    auto program = parseFriendly(source);
    const auto plan = jtml::buildRuntimePlan(program);
    jtml::RuntimeProjectPlan project;
    project.linkedPlan = plan;

    const auto asset = jtml::emitStaticUpdatePlanAsset(project);
    const auto sourcePos = asset.find("\"expression\":\"index + offset\"");
    ASSERT_NE(sourcePos, std::string::npos) << asset;
    const auto slice = asset.substr(sourcePos, 900);
    EXPECT_NE(slice.find("\"valueType\":\"number\""), std::string::npos)
        << slice;
    EXPECT_NE(slice.find("\"root\":\"index\""), std::string::npos)
        << slice;
    EXPECT_NE(slice.find("\"root\":\"offset\""), std::string::npos)
        << slice;
}

TEST(RuntimePlan, StaticUpdatePlanAssetUsesComponentTypeEnvironment) {
    const std::string source =
        "jtml 2\n"
        "make Counter title\n"
        "  let count = 0\n"
        "  get label = title + count\n"
        "  when inc\n"
        "    count = count + 1\n"
        "  card\n"
        "    text label\n"
        "    show count + 2\n"
        "    button \"Inc\" click inc\n"
        "page\n"
        "  Counter \"Clicks\"\n";

    auto program = parseFriendly(source);
    const auto plan = jtml::buildRuntimePlan(program);
    jtml::RuntimeProjectPlan project;
    project.linkedPlan = plan;

    const auto asset = jtml::emitStaticUpdatePlanAsset(project);
    const auto sourcePos = asset.find("\"expression\":\"count + 1\"");
    ASSERT_NE(sourcePos, std::string::npos) << asset;
    const auto assignmentSlice = asset.substr(sourcePos, 600);
    EXPECT_NE(assignmentSlice.find("\"valueType\":\"number\""), std::string::npos)
        << assignmentSlice;
    EXPECT_NE(assignmentSlice.find("\"root\":\"count\""), std::string::npos)
        << assignmentSlice;

    const auto renderedExpressionPos = asset.find("\"expression\":\"count + 2\"");
    ASSERT_NE(renderedExpressionPos, std::string::npos) << asset;
    const auto renderedExpressionSlice = asset.substr(renderedExpressionPos, 600);
    EXPECT_NE(renderedExpressionSlice.find("\"valueType\":\"number\""), std::string::npos)
        << renderedExpressionSlice;
}

TEST(RuntimePlan, StaticUpdatePlanAssetLowersRouteLinkAttributes) {
    const std::string source =
        "jtml 2\n"
        "make Home\n"
        "  link \"About\" to \"/about\" active-class \"active\"\n"
        "page\n"
        "  Home\n";

    auto program = parseFriendly(source);
    const auto plan = jtml::buildRuntimePlan(program);
    jtml::RuntimeProjectPlan project;
    project.linkedPlan = plan;

    const auto asset = jtml::emitStaticUpdatePlanAsset(project);
    EXPECT_NE(asset.find("delete next.to"), std::string::npos) << asset;
    EXPECT_NE(asset.find("addAttr('href', 'javascript:void(0)')"), std::string::npos) << asset;
    EXPECT_NE(asset.find("addAttr('data-jtml-href'"), std::string::npos) << asset;
    EXPECT_NE(asset.find("addAttr('data-jtml-link', 'true')"), std::string::npos) << asset;
    EXPECT_NE(asset.find("addAttr('data-jtml-active-class'"), std::string::npos) << asset;
}

TEST(RuntimePlan, TypedExpressionIrOwnsDependenciesJsonAndJsCodegen) {
    auto ir = jtml::parseExpressionIr(
        "(visible && users.data[index].active) ? [title, count + 1] : { \"fallback\": empty }");
    ASSERT_TRUE(ir);

    const auto deps = jtml::expressionIrDependencies(*ir);
    std::string depsText;
    for (const auto& dep : deps) depsText += dep + " ";
    auto containsDep = [&](const std::string& value) {
        return std::find(deps.begin(), deps.end(), value) != deps.end();
    };
    EXPECT_TRUE(containsDep("visible")) << depsText;
    EXPECT_TRUE(containsDep("users.data[index].active")) << depsText;
    EXPECT_TRUE(containsDep("users")) << depsText;
    EXPECT_TRUE(containsDep("index")) << depsText;
    EXPECT_TRUE(containsDep("title")) << depsText;
    EXPECT_TRUE(containsDep("count")) << depsText;
    EXPECT_TRUE(containsDep("empty")) << depsText;

    const auto plan = jtml::expressionIrToRuntimePlanJson(*ir);
    ASSERT_EQ(plan["kind"], "conditional") << plan.dump(2);
    EXPECT_EQ(plan["producer"], "typed-ir") << plan.dump(2);
    EXPECT_EQ(plan["directJs"], true) << plan.dump(2);
    EXPECT_NE(plan["jsExpression"].get<std::string>().find("scope[\"visible\"]"),
              std::string::npos) << plan.dump(2);
    EXPECT_EQ(plan["test"]["kind"], "binary") << plan.dump(2);
    EXPECT_EQ(plan["whenTrue"]["kind"], "array") << plan.dump(2);
    EXPECT_EQ(plan["whenFalse"]["kind"], "object") << plan.dump(2);

    ASSERT_TRUE(jtml::expressionIrSupportsDirectJs(*ir));
    const auto js = jtml::expressionIrToJsExpression(*ir);
    EXPECT_NE(js.find("scope[\"visible\"]"), std::string::npos) << js;
    EXPECT_NE(js.find("scope[\"users\"]"), std::string::npos) << js;
    EXPECT_NE(js.find("scope[\"index\"]"), std::string::npos) << js;
    EXPECT_EQ(js.find("new Function"), std::string::npos) << js;
    EXPECT_EQ(js.find("eval("), std::string::npos) << js;
}

TEST(RuntimePlan, ExpressionIrCodegenEscapesHostileLiteralText) {
    auto ir = jtml::parseExpressionIr(
        "{ \"\\\"]; globalThis.__jtmlInjected = true; //\": title, "
        "\"body\": \"</script><script>alert(1)</script>\" }");
    ASSERT_TRUE(ir);
    ASSERT_TRUE(jtml::expressionIrSupportsDirectJs(*ir));

    const auto js = jtml::expressionIrToJsExpression(*ir);
    EXPECT_NE(js.find("({["), std::string::npos) << js;
    EXPECT_NE(js.find("globalThis.__jtmlInjected"), std::string::npos) << js;
    EXPECT_EQ(js.find("new Function"), std::string::npos) << js;
    EXPECT_EQ(js.find("eval("), std::string::npos) << js;

    const auto plan = jtml::expressionIrToRuntimePlanJson(*ir);
    ASSERT_EQ(plan["kind"], "object") << plan.dump(2);
    EXPECT_EQ(plan["valueType"], "object") << plan.dump(2);
    EXPECT_EQ(plan["directJs"], true) << plan.dump(2);

    auto protoIr = jtml::parseExpressionIr("{ \"__proto__\": title }");
    ASSERT_TRUE(protoIr);
    const auto protoJs = jtml::expressionIrToJsExpression(*protoIr);
    EXPECT_NE(protoJs.find("[\"__proto__\"]"), std::string::npos) << protoJs;
    EXPECT_EQ(protoJs.find("\"__proto__\":"), std::string::npos) << protoJs;
}

TEST(RuntimePlan, BodyPlanWritesIncludeRootObservableForMemberAssignments) {
    const std::string source =
        "jtml 2\n"
        "make ProfileCard\n"
        "  let profile = { \"name\": \"Ada\", \"stats\": [1] }\n"
        "  when rename\n"
        "    profile.name = \"Grace\"\n"
        "    profile.stats[0] += 1\n"
        "  card\n"
        "    text profile.name\n"
        "page\n"
        "  ProfileCard\n";

    auto program = parseFriendly(source);
    const auto plan = jtml::buildRuntimePlan(program);
    ASSERT_EQ(plan.componentDefinitions.size(), 1u);
    const auto& definition = plan.componentDefinitions.front();

    const jtml::RuntimePlanComponentBodyNode* nameWrite = nullptr;
    const jtml::RuntimePlanComponentBodyNode* statsWrite = nullptr;
    for (const auto& node : definition.bodyPlan) {
        if (node.kind == "assignment" && node.name == "profile.name") nameWrite = &node;
        if (node.kind == "assignment" && node.name == "profile.stats[0]") statsWrite = &node;
    }

    ASSERT_NE(nameWrite, nullptr);
    EXPECT_TRUE(contains(nameWrite->writes, "profile.name"));
    EXPECT_TRUE(contains(nameWrite->writes, "profile"));

    ASSERT_NE(statsWrite, nullptr);
    EXPECT_TRUE(contains(statsWrite->writes, "profile.stats[0]"));
    EXPECT_TRUE(contains(statsWrite->writes, "profile"));
    EXPECT_TRUE(contains(statsWrite->reads, "profile"));
    EXPECT_FALSE(contains(statsWrite->reads, "stats"));
}

TEST(RuntimePlan, ProjectPlanUsesRetainedPerFileAsts) {
    const std::string entrySource =
        "jtml 2\n"
        "use Dashboard from \"./pages/dashboard.jtml\"\n"
        "page\n"
        "  text \"Home\"\n";
    const std::string dashboardSource =
        "jtml 2\n"
        "use appState from \"../stores/app-state.jtml\"\n"
        "export make Dashboard title\n"
        "  panel title title\n"
        "    text appState.user\n";
    const std::string storeSource =
        "jtml 2\n"
        "export store appState\n"
        "  let user = \"Ada\"\n";

    auto entryAst = parseFriendly(entrySource);
    auto dashboardAst = parseFriendly(dashboardSource);
    auto storeAst = parseFriendly(storeSource);

    std::vector<jtml::SemanticModuleSource> modules = {
        {
            "/app/index.jtml",
            jtml::analyzeSemanticProgram(entryAst, entrySource),
            jtml::summarizeSemanticAst(entryAst, "friendly"),
            jtml::cloneSemanticAst(entryAst, "friendly"),
        },
        {
            "/app/pages/dashboard.jtml",
            jtml::analyzeSemanticProgram(dashboardAst, dashboardSource),
            jtml::summarizeSemanticAst(dashboardAst, "friendly"),
            jtml::cloneSemanticAst(dashboardAst, "friendly"),
        },
        {
            "/app/stores/app-state.jtml",
            jtml::analyzeSemanticProgram(storeAst, storeSource),
            jtml::summarizeSemanticAst(storeAst, "friendly"),
            jtml::cloneSemanticAst(storeAst, "friendly"),
        },
    };

    auto linkedSemantic = jtml::analyzeSemanticProgram(entryAst, entrySource);
    linkedSemantic.moduleFiles = {
        "/app/index.jtml",
        "/app/pages/dashboard.jtml",
        "/app/stores/app-state.jtml",
    };
    const auto project = jtml::buildSemanticProject(modules, "/app/index.jtml", linkedSemantic);
    const auto projectPlan = jtml::buildRuntimePlan(project);

    ASSERT_EQ(projectPlan.entry, 0u);
    ASSERT_EQ(projectPlan.modules.size(), 3u);
    EXPECT_TRUE(projectPlan.modules[0].astAvailable);
    EXPECT_TRUE(projectPlan.modules[1].astAvailable);
    EXPECT_TRUE(projectPlan.modules[2].astAvailable);
    EXPECT_TRUE(projectPlan.modules[0].clientExecutable);
    EXPECT_TRUE(projectPlan.modules[1].clientExecutable);
    EXPECT_TRUE(projectPlan.modules[2].clientExecutable);
    EXPECT_EQ(projectPlan.modules[1].syntax, "friendly");
    ASSERT_EQ(projectPlan.modules[1].plan.componentDefinitions.size(), 1u);
    EXPECT_EQ(projectPlan.modules[1].plan.componentDefinitions[0].name, "Dashboard");
    ASSERT_EQ(projectPlan.modules[1].plan.componentDefinitions[0].params.size(), 1u);
    EXPECT_EQ(projectPlan.modules[1].plan.componentDefinitions[0].params[0], "title");
    EXPECT_FALSE(projectPlan.modules[1].plan.componentDefinitions[0].bodyPlan.empty());
    ASSERT_EQ(projectPlan.modules[2].plan.semantic.stores.size(), 1u);
    EXPECT_EQ(projectPlan.modules[2].plan.semantic.stores[0], "appState");
    ASSERT_EQ(projectPlan.modules[2].plan.semantic.state.size(), 1u);
    EXPECT_EQ(projectPlan.modules[2].plan.semantic.state[0], "appState");
}

TEST(RuntimePlan, ProjectPlanKeepsDuplicateComponentNamesModuleScoped) {
    jtml::SemanticProgram entrySemantic;
    entrySemantic.importRecords.push_back(
        {"./pages/home.jtml", "use", {"Home"}, false, 2, 1});

    jtml::SemanticProgram homeSemantic;
    homeSemantic.importRecords.push_back(
        {"../components/primary-card.jtml", "use", {"Card"}, false, 2, 1});
    homeSemantic.componentDefinitions.push_back(
        {"Home", {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, hexEncode("0: page\n2: Card \"Nested\"\n"),
         false, 2, 1, 0, 3});
    homeSemantic.exportRecords.push_back({"Home", "make", "", false, 3});
    homeSemantic.componentInstances.push_back(
        {"card_1", "Card", 1, "component", {{"title", "Primary"}}, {}, "", 4});

    jtml::SemanticProgram primarySemantic;
    primarySemantic.componentDefinitions.push_back(
        {"Card", {"title"}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, "", false, 1, 1, 0, 2});
    primarySemantic.exportRecords.push_back({"Card", "make", "", false, 2});

    jtml::SemanticProgram secondarySemantic;
    secondarySemantic.componentDefinitions.push_back(
        {"Card", {"title"}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, "", false, 1, 1, 0, 2});
    secondarySemantic.exportRecords.push_back({"Card", "make", "", false, 2});

    std::vector<jtml::SemanticModuleSource> modules = {
        {"/app/index.jtml", entrySemantic},
        {"/app/pages/home.jtml", homeSemantic},
        {"/app/components/primary-card.jtml", primarySemantic},
        {"/app/components/secondary-card.jtml", secondarySemantic},
    };

    auto linkedSemantic = entrySemantic;
    linkedSemantic.moduleFiles = {
        "/app/index.jtml",
        "/app/pages/home.jtml",
        "/app/components/primary-card.jtml",
        "/app/components/secondary-card.jtml",
    };
    const auto project = jtml::buildSemanticProject(modules, "/app/index.jtml", linkedSemantic);
    const auto projectPlan = jtml::buildRuntimePlan(project);

    ASSERT_EQ(projectPlan.modules.size(), 4u);
    ASSERT_EQ(projectPlan.modules[2].plan.componentDefinitions.size(), 1u);
    ASSERT_EQ(projectPlan.modules[3].plan.componentDefinitions.size(), 1u);
    EXPECT_EQ(projectPlan.modules[2].plan.componentDefinitions[0].name, "Card");
    EXPECT_EQ(projectPlan.modules[2].plan.componentDefinitions[0].moduleId,
              projectPlan.modules[2].id);
    EXPECT_EQ(projectPlan.modules[3].plan.componentDefinitions[0].name, "Card");
    EXPECT_EQ(projectPlan.modules[3].plan.componentDefinitions[0].moduleId,
              projectPlan.modules[3].id);

    ASSERT_EQ(projectPlan.modules[1].plan.componentInstances.size(), 1u);
    const auto& cardInstance = projectPlan.modules[1].plan.componentInstances[0];
    EXPECT_EQ(cardInstance.component, "Card");
    EXPECT_EQ(cardInstance.moduleId, projectPlan.modules[1].id);
    EXPECT_EQ(cardInstance.definitionModule, projectPlan.modules[2].id);
    EXPECT_NE(cardInstance.definitionModule, projectPlan.modules[3].id);

    ASSERT_EQ(projectPlan.modules[1].plan.componentDefinitions.size(), 1u);
    const auto& homeDefinition = projectPlan.modules[1].plan.componentDefinitions[0];
    ASSERT_GE(homeDefinition.bodyPlan.size(), 2u);
    auto nestedCard = std::find_if(
        homeDefinition.bodyPlan.begin(),
        homeDefinition.bodyPlan.end(),
        [](const auto& node) {
            return node.kind == "template" && node.name == "Card";
        });
    ASSERT_NE(nestedCard, homeDefinition.bodyPlan.end());
    EXPECT_EQ(nestedCard->definitionModule, projectPlan.modules[2].id);
    EXPECT_NE(nestedCard->definitionModule, projectPlan.modules[3].id);
}

TEST(RuntimePlan, ClientProjectionQualifiesOnlyUnambiguousInstanceMirrors) {
    auto program = parseFriendly("jtml 2\nmake Counter\n  p \"Counter\"\npage\n  Counter\n");
    const auto linked = jtml::buildRuntimePlan(program);
    ASSERT_FALSE(linked.componentInstances.empty());
    jtml::RuntimeProjectPlan project;
    project.entry = 0;
    project.linkedPlan = linked;
    jtml::RuntimeModulePlan module;
    module.id = 0;
    module.clientExecutable = true;
    module.plan = linked;
    for (auto& instance : module.plan.componentInstances) {
        instance.moduleId = 0;
        instance.definitionModule = 0;
    }
    project.modules.push_back(module);
    auto json = jtml::runtimeProjectPlanToClientJson(project);
    ASSERT_FALSE(json["componentInstances"].empty());
    EXPECT_EQ(json["componentInstances"][0]["moduleId"], 0);
    EXPECT_EQ(json["componentInstances"][0]["definitionModule"], 0);
    EXPECT_TRUE(json["project"]["linkedPlan"]["componentInstances"].empty());
    EXPECT_TRUE(json["project"]["linkedPlan"]["componentDefinitions"].empty());
    EXPECT_FALSE(json["project"]["modules"][0]["plan"]["componentInstances"].empty());
    // Equal names/IDs in different modules cannot authorize choosing an owner.
    module.id = 1;
    for (auto& instance : module.plan.componentInstances) {
        instance.moduleId = 1;
        instance.definitionModule = 1;
    }
    project.modules.push_back(module);
    json = jtml::runtimeProjectPlanToClientJson(project);
    EXPECT_TRUE(json["componentInstances"][0]["moduleId"].is_null());
    EXPECT_EQ(json["project"]["modules"].size(), 2u);
}


TEST(RuntimePlan, ClientProjectionRetainsRootLinkedPayloadAndExecutableModules) {
    auto program = parseFriendly(
        "jtml 2\nlet count = 0\nget label = count + 1\n"
        "when add\n  count += 1\nmake Counter\n  text label\n"
        "route \"/counter\" as Counter\npage\n  Counter\n");
    jtml::RuntimeProjectPlan project;
    project.entry = 0;
    project.linkedPlan = jtml::buildRuntimePlan(program);
    jtml::RuntimeModulePlan executable;
    executable.id = 0;
    executable.clientExecutable = true;
    executable.plan = project.linkedPlan;
    project.modules.push_back(executable);
    auto unavailable = executable;
    unavailable.id = 1;
    unavailable.clientExecutable = false;
    project.modules.push_back(unavailable);
    const auto expected = jtml::runtimePlanToClientJson(project.linkedPlan);
    const auto client = jtml::runtimeProjectPlanToClientJson(project);
    // Bootstrap consumes the root first; the empty mirror must not discard any
    // linked-only state, compiled expressions, actions, routes or definitions.
    for (const auto& field : {"state", "statePlans", "derived", "derivedPlans",
                              "actions", "routes", "fetches", "componentDefinitions",
                              "componentInstances"}) {
        EXPECT_EQ(client.at(field), expected.at(field)) << field;
    }
    EXPECT_EQ(client["project"]["modules"][0]["plan"], expected);
    EXPECT_FALSE(client["project"]["modules"][1].contains("plan"));
    // Explain/compiler consumers continue to receive the actual linked plan.
    const auto explain = jtml::runtimeProjectPlanToExplainJson(project);
    EXPECT_EQ(explain["linkedPlan"], jtml::runtimePlanToExplainJson(project.linkedPlan));
}

TEST(RuntimePlan, AdversarialTaskboardDependenciesCoverDynamicOperands) {
    struct Case { std::string expression; std::vector<std::string> roots; };
    const std::vector<Case> cases = {
        {"records[index].score + offset", {"records", "index", "offset"}},
        {"records[index].score + offset > limit && enabled", {"records", "index", "offset", "limit", "enabled"}},
        {"enabled ? records[index].score + offset : fallback", {"enabled", "records", "index", "offset", "fallback"}},
    };
    for (const auto& test : cases) {
        SCOPED_TRACE(test.expression);
        const auto ir = jtml::parseExpressionIr(test.expression);
        ASSERT_TRUE(ir);
        const auto dependencies = jtml::expressionIrDependencies(*ir);
        auto program = parseFriendly("jtml 2\nmake Probe records index offset limit enabled fallback\n  show " + test.expression + "\npage\n  p \"Probe\"\n");
        jtml::RuntimeProjectPlan project;
        project.linkedPlan = jtml::buildRuntimePlan(program);
        const auto& nodes = project.linkedPlan.componentDefinitions.front().bodyPlan;
        const auto node = std::find_if(nodes.begin(), nodes.end(), [](const auto& n) { return n.kind == "template" && n.name == "show"; });
        ASSERT_NE(node, nodes.end());
        const auto emitted = jtml::emitStaticComponentModuleAsset(project);
        for (const auto& root : test.roots) {
            EXPECT_TRUE(contains(dependencies, root)) << "Expression IR: " << root;
            EXPECT_TRUE(contains(node->reads, root)) << "Runtime Plan: " << root;
            EXPECT_NE(emitted.find("!writes['" + root + "']"), std::string::npos) << "Generated selection: " << root;
        }
    }
}

TEST(RuntimePlan, CompilerElementClassificationIsNotSerializedAsRuntimeMetadata) {
    auto program = parseFriendly("jtml 2\nmake Card title\n  h2 title data-label \"marker\"\npage\n  Card \"Revenue\"\n");
    jtml::RuntimeProjectPlan project;
    project.linkedPlan = jtml::buildRuntimePlan(program);
    const auto legacy = jtml::emitStaticUpdatePlanAsset(project);
    const auto production = jtml::emitStaticComponentModuleAsset(project);
    EXPECT_EQ(legacy.find("compilerElementParts"), std::string::npos);
    EXPECT_EQ(production.find("compilerElementParts"), std::string::npos);
}

TEST(RuntimePlan, UnaryMinusOwnsNumericTypeNestedReadsAndOperatorCodegen) {
    auto ir = jtml::parseExpressionIr("-(items[index] + offset)");
    ASSERT_TRUE(ir);
    EXPECT_EQ(ir->kind, jtml::ExpressionIrKind::Unary);
    EXPECT_EQ(ir->valueType, jtml::ExpressionValueType::Number);
    const auto reads = jtml::expressionIrDependencies(*ir);
    for (const auto& root : {"items", "index", "offset"}) {
        EXPECT_NE(std::find(reads.begin(), reads.end(), root), reads.end());
    }
    const auto plan = jtml::expressionIrToRuntimePlanJson(*ir);
    EXPECT_EQ(plan["kind"], "unary"); EXPECT_EQ(plan["operator"], "-");
    EXPECT_EQ(plan["valueType"], "number"); EXPECT_EQ(plan["directJs"], true);
    const auto js = jtml::expressionIrToJsExpression(*ir);
    EXPECT_EQ(js.find("(-("), 0u); EXPECT_EQ(js.find("!("), std::string::npos);
    auto logical = jtml::parseExpressionIr("!-(-x)");
    ASSERT_TRUE(logical); EXPECT_EQ(logical->valueType, jtml::ExpressionValueType::Boolean);
    EXPECT_EQ(jtml::expressionIrToJsExpression(*logical).find("(!("), 0u);
}

TEST(RuntimePlan, UnaryDirectJsRejectsUnknownOperatorsAndUnsupportedChildren) {
    auto ir = jtml::parseExpressionIr("-x");
    ASSERT_TRUE(ir); ASSERT_EQ(ir->kind, jtml::ExpressionIrKind::Unary);
    ASSERT_TRUE(jtml::expressionIrSupportsDirectJs(*ir));
    ir->op = "injected()";
    EXPECT_FALSE(jtml::expressionIrSupportsDirectJs(*ir));
    EXPECT_EQ(jtml::expressionIrToJsExpression(*ir), "undefined");
    auto call = jtml::parseExpressionIr("-unknown(x)");
    ASSERT_TRUE(call); EXPECT_FALSE(jtml::expressionIrSupportsDirectJs(*call));
}

TEST(RuntimePlan, ComponentArgumentWordPlansPreserveFriendlyExpressionBoundaries) {
    const std::string source =
        "jtml 2\n"
        "make Child value item record label\n"
        "  text value\n"
        "make Probe\n"
        "  let count = 100\n"
        "  let index = 0\n"
        "  let items = [3, 5]\n"
        "  Child ((count + 1) * 2) items[(index + 1) % 2] {value: count + 1} \"a \\\"quoted\\\" label\"\n"
        "page\n"
        "  Probe\n";
    auto program = parseFriendly(source);
    const auto plan = jtml::buildRuntimePlan(program, jtml::analyzeSemanticProgram(program, source));
    const auto owner = std::find_if(plan.componentDefinitions.begin(), plan.componentDefinitions.end(),
        [](const auto& definition) { return definition.name == "Probe"; });
    ASSERT_NE(owner, plan.componentDefinitions.end());
    jtml::ExpressionTypeEnvironment environment;
    environment.symbols["count"] = jtml::ExpressionValueType::Number;
    environment.symbols["index"] = jtml::ExpressionValueType::Number;
    environment.symbols["items"] = jtml::ExpressionValueType::Array;
    const auto body = jtml::runtimePlanBodyPlanToJson(owner->bodyPlan, {true, &environment});
    const auto call = std::find_if(body.begin(), body.end(), [](const auto& node) { return node.value("head", "") == "Child"; });
    ASSERT_NE(call, body.end());
    EXPECT_EQ((*call)["sourceLine"], 8);
    EXPECT_EQ((*call)["sourceColumn"], 3);
    const auto& args = (*call)["wordPlans"];
    ASSERT_EQ(args.size(), 5u) << args.dump(2);
    EXPECT_EQ(args[1]["kind"], "binary");
    EXPECT_EQ(args[1]["valueType"], "number");
    EXPECT_EQ(args[1]["left"]["operator"], "+");
    EXPECT_EQ(args[2]["kind"], "subscript");
    EXPECT_EQ(args[2]["index"]["operator"], "%");
    EXPECT_EQ(args[3]["kind"], "object");
    EXPECT_EQ(args[3]["entries"][0]["key"], "value");
    EXPECT_EQ(args[4]["kind"], "literal");
    EXPECT_EQ(args[4]["value"], "a \"quoted\" label");
    EXPECT_TRUE(contains(owner->bodyPlan.back().reads, "count"));
    EXPECT_TRUE(contains(owner->bodyPlan.back().reads, "items"));
    EXPECT_TRUE(contains(owner->bodyPlan.back().reads, "index"));
}

TEST(RuntimePlan, GlobalArgumentPlansPreserveExpressionsTypesAndLiteralText) {
    const std::string source =
        "jtml 2\nlet count = 100\nlet items = [3, 5]\nlet index = 0\n"
        "make Child value item label\n  let count = 10\n  p value\n"
        "page\n  Child ((count + 1) * 2) items[(index + 1) % 2] \"literal count & growth\"\n";
    auto program = parseFriendly(source);
    const auto plan = jtml::buildRuntimePlan(program, jtml::analyzeSemanticProgram(program, source));
    ASSERT_EQ(plan.componentInstances.size(), 1u);
    ASSERT_EQ(plan.componentInstances[0].argumentExpressions.size(), 3u);
    const auto client = jtml::runtimePlanToClientJson(plan);
    const auto& arguments = client["componentInstances"][0]["argumentPlans"];
    EXPECT_EQ(arguments["value"]["kind"], "binary");
    EXPECT_EQ(arguments["value"]["valueType"], "number");
    EXPECT_EQ(arguments["value"]["left"]["operator"], "+");
    EXPECT_EQ(arguments["item"]["kind"], "subscript");
    EXPECT_EQ(arguments["label"]["kind"], "literal");
    EXPECT_EQ(arguments["label"]["value"], "literal count & growth");
    EXPECT_FALSE(arguments["value"].contains("jsExpression"));
}

TEST(RuntimePlan, InputIntoReadsAndWritesItsStateWithoutExplicitValueAttribute) {
    auto program = parseFriendly(
        "jtml 2\nmake Editor\n  let draft = \"\"\n  let selected = \"\"\n"
        "  input into draft\n  if draft != \"\"\n    input into draft\n"
        "  file into selected\npage\n  Editor\n");
    const auto plan = jtml::buildRuntimePlan(program);
    ASSERT_EQ(plan.componentDefinitions.size(), 1u);
    size_t inputs = 0, files = 0;
    for (const auto& node : plan.componentDefinitions.front().bodyPlan) {
        if (node.head == "input") {
            ++inputs;
            EXPECT_TRUE(contains(node.reads, "draft"));
            EXPECT_TRUE(contains(node.writes, "draft"));
        } else if (node.head == "file") {
            ++files;
            EXPECT_FALSE(contains(node.reads, "selected"));
            EXPECT_TRUE(contains(node.writes, "selected"));
        }
    }
    EXPECT_EQ(inputs, 2u);
    EXPECT_EQ(files, 1u);
}

TEST(RuntimePlan, RouteLayoutsRetainCallerOwnedSlotInstances) {
    const std::string source =
        "jtml 2\n"
        "make Shell\n"
        "  page\n"
        "    h1 \"Shell\"\n"
        "    slot\n"
        "make Detail id\n"
        "  let count = 0\n"
        "  text id\n"
        "  text count\n"
        "route \"/detail/:id\" as Detail layout Shell\n";
    auto program = parseFriendly(source);
    const auto semantic = jtml::analyzeSemanticProgram(program, source);
    const auto plan = jtml::buildRuntimePlan(program, semantic);
    const auto layout = std::find_if(plan.componentInstances.begin(), plan.componentInstances.end(),
        [](const auto& instance) { return instance.role == "layout"; });
    const auto route = std::find_if(plan.componentInstances.begin(), plan.componentInstances.end(),
        [](const auto& instance) { return instance.role == "route"; });
    ASSERT_NE(layout, plan.componentInstances.end());
    ASSERT_NE(route, plan.componentInstances.end());
    ASSERT_FALSE(layout->slotPlan.empty());
    ASSERT_EQ(layout->slotPlan.size(), 1u);
    EXPECT_NE(layout->slotSource.find("Detail id\n"), std::string::npos);
    EXPECT_EQ(layout->slotPlan[0].sourceLine, layout->sourceLine);
    EXPECT_EQ(layout->slotPlan[0].name, "Detail");
    EXPECT_EQ(layout->slotPlan[0].expression, "id");
    ASSERT_EQ(route->params.size(), 1u);
    EXPECT_EQ(route->params[0].name, "id");
    EXPECT_EQ(route->params[0].value, "id");
    EXPECT_EQ(route->sourceLine, layout->sourceLine);
    ASSERT_EQ(route->locals.size(), 1u);
    EXPECT_EQ(route->locals[0].name, "count");
    EXPECT_FALSE(route->locals[0].value.empty());
    const auto client = jtml::runtimePlanToClientJson(plan).dump();
    EXPECT_EQ(client.find("\"sourceSpan\""), std::string::npos);
}

TEST(RuntimePlan, Scene3DUsesCanonicalCanvasWithoutLosingAuthoredWrites) {
    const std::string source =
        "jtml 2\nlet sceneState = {}\nmake Scene\n  main\n"
        "    scene3d \"Product room\" scene productRoom camera orbit controls orbit renderer \"three\" into sceneState width \"640\" height \"360\"\n"
        "page\n  Scene\n";
    auto program = parseFriendly(source);
    const auto plan = jtml::buildRuntimePlan(program, jtml::analyzeSemanticProgram(program, source));
    const auto definition = std::find_if(plan.componentDefinitions.begin(), plan.componentDefinitions.end(),
        [](const auto& d) { return d.name == "Scene"; });
    ASSERT_NE(definition, plan.componentDefinitions.end());
    const auto node = std::find_if(definition->bodyPlan.begin(), definition->bodyPlan.end(),
        [](const auto& n) { return n.head == "canvas"; });
    ASSERT_NE(node, definition->bodyPlan.end());
    EXPECT_EQ(node->sourceLine, 5);
    EXPECT_EQ(node->sourceColumn, 5);
    EXPECT_TRUE(node->expression.empty());
    EXPECT_NE(node->text.find("data-jtml-scene (\"productRoom\")"), std::string::npos);
    EXPECT_NE(node->text.find("data-jtml-controls (\"orbit\")"), std::string::npos);
    EXPECT_NE(node->text.find("data-jtml-scene3d-controller (\"sceneState\")"), std::string::npos);
    EXPECT_EQ(std::count(node->writes.begin(), node->writes.end(), "sceneState"), 1);
}
