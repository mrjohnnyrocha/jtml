#include "jtml/friendly.h"
#include "jtml/language_catalog.h"
#include "jtml/lexer.h"
#include "jtml/parser.h"
#include "jtml/runtime_plan.h"
#include "jtml/semantic.h"
#include "jtml/semantic/module_graph.h"
#include "jtml/transpiler.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
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

bool contains(const std::vector<std::string>& values, const std::string& value) {
    return std::find(values.begin(), values.end(), value) != values.end();
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
    const auto* widget = findImportRecord(semantic, "./widget.jtml");
    ASSERT_NE(widget, nullptr);
    EXPECT_EQ(widget->kind, "named");
    ASSERT_EQ(widget->names.size(), 1u);
    EXPECT_EQ(widget->names[0], "Widget");
    const auto* money = findImportRecord(semantic, "./money.jtml");
    ASSERT_NE(money, nullptr);
    EXPECT_EQ(money->kind, "destructured");
    ASSERT_EQ(money->names.size(), 2u);
    EXPECT_EQ(money->names[0], "formatMoney");
    EXPECT_EQ(money->names[1], "parseDate");
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
    EXPECT_TRUE(hasEdge(semantic, "module:re-export", "./card.jtml", "imports"));
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
        "  card title title\n"
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
    const int cardIndex = static_cast<int>(
        std::distance(plan.componentDefinitions[0].bodyPlan.begin(), cardIt));
    EXPECT_TRUE(std::any_of(
        plan.componentDefinitions[0].bodyPlan.begin(),
        plan.componentDefinitions[0].bodyPlan.end(),
        [&](const auto& node) {
            return node.kind == "template" && node.name == "text" &&
                   node.parentIndex == cardIndex;
        }));
    EXPECT_TRUE(std::any_of(
        plan.componentInstances.begin(),
        plan.componentInstances.end(),
        [](const auto& instance) { return instance.component == "Counter"; }));
}
