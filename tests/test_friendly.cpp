#include "jtml/friendly.h"
#include "jtml/environment.h"
#include "jtml/interpreter.h"
#include "jtml/lexer.h"
#include "jtml/parser.h"
#include "jtml/transpiler.h"
#include "json.hpp"

#include <gtest/gtest.h>
#include <string>

namespace {

std::string normalizeOk(const std::string& src) {
    std::string classic = jtml::normalizeSourceSyntax(src, jtml::SyntaxMode::Friendly);
    Lexer lex(classic);
    auto tokens = lex.tokenize();
    EXPECT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    (void)parser.parseProgram();
    EXPECT_TRUE(parser.getErrors().empty()) << classic;
    return classic;
}

// ---------------------------------------------------------------------------
// Syntax Detection
// ---------------------------------------------------------------------------

TEST(FriendlySyntax, HeaderEnablesAutoMode) {
    EXPECT_TRUE(jtml::isFriendlySyntax("jtml 2\npage\n  h1 \"Hi\"\n"));
    EXPECT_FALSE(jtml::isFriendlySyntax("define count = 0\\\\\n"));
}

TEST(FriendlySyntax, LooksLikeFriendlyDetectsMoreKeywords) {
    EXPECT_TRUE(jtml::looksLikeFriendlySyntax("let count = 0\n"));
    EXPECT_TRUE(jtml::looksLikeFriendlySyntax("const PI = 3.14\n"));
    EXPECT_TRUE(jtml::looksLikeFriendlySyntax("when save\n  let x = 1\n"));
    EXPECT_TRUE(jtml::looksLikeFriendlySyntax("page\n  h1 \"Hi\"\n"));
    EXPECT_TRUE(jtml::looksLikeFriendlySyntax("use \"./lib.jtml\"\n"));
    EXPECT_TRUE(jtml::looksLikeFriendlySyntax("show \"hello\"\n"));
    EXPECT_TRUE(jtml::looksLikeFriendlySyntax("for x in items\n  show x\n"));
    EXPECT_TRUE(jtml::looksLikeFriendlySyntax("if count > 0\n  show count\n"));
    EXPECT_TRUE(jtml::looksLikeFriendlySyntax("while running\n  show \"go\"\n"));
    EXPECT_TRUE(jtml::looksLikeFriendlySyntax("style\n  .card\n    color: red\n"));
    EXPECT_TRUE(jtml::looksLikeFriendlySyntax("store auth\n  let user = \"Ada\"\n"));
    EXPECT_TRUE(jtml::looksLikeFriendlySyntax("export make Card\n  text \"Hi\"\n"));
    EXPECT_FALSE(jtml::looksLikeFriendlySyntax("define count = 0\\\\\n"));
}

TEST(FriendlySyntax, ExportModifierIsErasedForClassicCompatibility) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "export let title: string = \"Module title\"\n"
        "export when save\n"
        "  let title = \"Saved\"\n"
        "export make Card label\n"
        "  box class \"card\"\n"
        "    h2 label\n"
        "export store auth\n"
        "  let user = \"Ada\"\n"
        "page\n"
        "  Card title\n");

    EXPECT_NE(classic.find("define title: string = \"Module title\"\\\\"), std::string::npos);
    EXPECT_NE(classic.find("function save()\\\\"), std::string::npos);
    EXPECT_NE(classic.find("data-jtml-instance=\"Card_"), std::string::npos);
    EXPECT_NE(classic.find("define auth = {user: \"Ada\"}\\\\"), std::string::npos);
    EXPECT_EQ(classic.find("export"), std::string::npos);
}

TEST(FriendlySyntax, ScopedStyleBlockLowersAndTranspiles) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "style\n"
        "  .card, .panel\n"
        "    background: #fffdf8\n"
        "    color: #17212b\n"
        "    border-radius: 8px\n"
        "  .card:hover\n"
        "    transform: translateY(-2px)\n"
        "page\n"
        "  box class \"card\"\n"
        "    h1 \"Styled\"\n");

    EXPECT_NE(classic.find("@style\\\\"), std::string::npos);
    EXPECT_NE(classic.find("[data-jtml-app] .card, [data-jtml-app] .panel"), std::string::npos);
    EXPECT_NE(classic.find("background: #fffdf8;"), std::string::npos);
    EXPECT_NE(classic.find("color: #17212b;"), std::string::npos);
    EXPECT_NE(classic.find("border-radius: 8px;"), std::string::npos);

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    std::string html = transpiler.transpile(program);
    auto styleStart = html.find("<style>");
    auto styleEnd = html.find("</style>");
    ASSERT_NE(styleStart, std::string::npos);
    ASSERT_NE(styleEnd, std::string::npos);
    std::string styleHtml = html.substr(styleStart, styleEnd - styleStart);
    EXPECT_NE(html.find("<body data-jtml-app>"), std::string::npos);
    EXPECT_NE(styleHtml.find("[data-jtml-app] .card:hover"), std::string::npos);
    EXPECT_EQ(styleHtml.find("{{"), std::string::npos);
}

TEST(FriendlySyntax, RoutesExpandToRoutedSectionsAndRuntime) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "route \"/\" as Home\n"
        "route \"/about\" as About\n"
        "make Home\n"
        "  page\n"
        "    h1 \"Home\"\n"
        "    link \"About\" href \"#/about\"\n"
        "make About\n"
        "  page\n"
        "    h1 \"About\"\n"
        "    link \"Home\" href \"#/\"\n");

    EXPECT_EQ(classic.find("make Home"), std::string::npos);
    EXPECT_NE(classic.find("@section data-jtml-route=\"/\" data-jtml-route-name=\"Home\" data-jtml-route-params=\"\"\\\\"),
              std::string::npos);
    EXPECT_NE(classic.find("@section data-jtml-route=\"/about\" data-jtml-route-name=\"About\" data-jtml-route-params=\"\"\\\\"),
              std::string::npos);

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    std::string html = transpiler.transpile(program);
    EXPECT_NE(html.find("data-jtml-route"), std::string::npos);
    EXPECT_NE(html.find("function applyRoutes()"), std::string::npos);
    EXPECT_NE(html.find("hashchange"), std::string::npos);
}

TEST(FriendlySyntax, RouteParamsBecomeClientStateBindings) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "route \"/user/:id\" as UserProfile\n"
        "make UserProfile id\n"
        "  page\n"
        "    show id\n");

    EXPECT_NE(classic.find("define id = \"\"\\\\"), std::string::npos);
    EXPECT_NE(classic.find("data-jtml-route=\"/user/:id\""), std::string::npos);
    EXPECT_NE(classic.find("data-jtml-route-params=\"id\""), std::string::npos);

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    std::string html = transpiler.transpile(program);
    EXPECT_NE(html.find("data-jtml-route=\"/user/:id\""), std::string::npos);
    EXPECT_NE(html.find("data-jtml-route-params=\"id\""), std::string::npos);
    EXPECT_NE(html.find("data-jtml-expr=\"id\""), std::string::npos);
    EXPECT_NE(html.find("function matchRouteParams"), std::string::npos);
    EXPECT_NE(html.find("clientState[name]"), std::string::npos);
}

TEST(FriendlySyntax, RouteAwareLinksLowerToHashLinks) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "page\n"
        "  link \"About\" to \"/about\"\n");

    EXPECT_NE(classic.find("@a href=\"javascript:void(0)\" data-jtml-href=\"#/about\" data-jtml-link=\"true\"\\\\"),
              std::string::npos);
    EXPECT_NE(classic.find("show \"About\"\\\\"), std::string::npos);
}

TEST(FriendlySyntax, RouteCanLoadLazyFetchesWhenMatched) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "let users = fetch \"/api/users\" lazy stale keep\n"
        "make Users\n"
        "  page\n"
        "    for user in users.data\n"
        "      text user.name\n"
        "route \"/users\" as Users load users\n");

    EXPECT_NE(classic.find("data-jtml-fetch=\"users\""), std::string::npos);
    EXPECT_NE(classic.find("data-lazy=\"true\""), std::string::npos);
    EXPECT_NE(classic.find("data-jtml-route=\"/users\""), std::string::npos);
    EXPECT_NE(classic.find("data-jtml-route-load=\"users\""), std::string::npos);

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    std::string html = transpiler.transpile(program);
    EXPECT_NE(html.find("const lazy = marker.getAttribute('data-lazy') === 'true'"), std::string::npos);
    EXPECT_NE(html.find("if (!lazy) __jtml_fetch_fns[name]()"), std::string::npos);
    EXPECT_NE(html.find("function runRouteLoads(route)"), std::string::npos);
    EXPECT_NE(html.find("runRouteLoads(route)"), std::string::npos);
}

TEST(FriendlySyntax, WildcardRouteLowersAsNotFoundFallback) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "route \"/\" as Home\n"
        "route \"*\" as NotFound\n"
        "make Home\n"
        "  page\n"
        "    h1 \"Home\"\n"
        "make NotFound\n"
        "  page\n"
        "    h1 \"Not found\"\n");

    EXPECT_NE(classic.find("data-jtml-route=\"*\""), std::string::npos);
    EXPECT_NE(classic.find("data-jtml-route-name=\"NotFound\""), std::string::npos);

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    std::string html = transpiler.transpile(program);
    EXPECT_NE(html.find("data-jtml-route=\"*\""), std::string::npos);
    EXPECT_NE(html.find("trim() === '*'"), std::string::npos);
}

TEST(FriendlySyntax, TemplateBindingsExposeConditionsAndLoops) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "let ready = true\n"
        "let items = [\"Ada\", \"Grace\"]\n"
        "page\n"
        "  if ready\n"
        "    text \"Ready\"\n"
        "  else\n"
        "    text \"Waiting\"\n"
        "  for item in items\n"
        "    text item\n");

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    std::string html = transpiler.transpile(program);
    EXPECT_NE(html.find("function applyTemplates"), std::string::npos);

    InterpreterConfig config;
    config.startWebSocket = false;
    Interpreter interpreter(transpiler, config);
    interpreter.interpret(program);

    auto bindings = nlohmann::json::parse(interpreter.getBindingsJSON());
    ASSERT_TRUE(bindings.contains("conditions"));
    ASSERT_TRUE(bindings.contains("loops"));
    ASSERT_FALSE(bindings["conditions"].empty());
    ASSERT_FALSE(bindings["loops"].empty());
    bool sawLoop = false;
    for (auto it = bindings["loops"].begin(); it != bindings["loops"].end(); ++it) {
        EXPECT_NE(html.find("id=\"" + it.key() + "\""), std::string::npos)
            << "loop binding must point to a real DOM container";
        if (it.value().is_array() && it.value().size() == 2 &&
            it.value()[0] == "Ada" && it.value()[1] == "Grace") {
            sawLoop = true;
        }
    }
    EXPECT_TRUE(sawLoop) << bindings.dump(2);
}

TEST(FriendlySyntax, FetchDeclarationsExposeClientRuntimeBindings) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "let users = fetch \"/api/users\"\n"
        "page\n"
        "  if users.loading\n"
        "    text \"Loading\"\n"
        "  else\n"
        "    for user in users.data\n"
        "      text user.name\n"
        "  show users.error\n");

    EXPECT_NE(classic.find("define users = {loading: true, data: [], error: \"\"}\\"),
              std::string::npos);
    EXPECT_NE(classic.find("@template data-jtml-fetch=\"users\" data-url=\"/api/users\" data-method=\"GET\"\\"),
              std::string::npos);

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    std::string html = transpiler.transpile(program);
    EXPECT_NE(html.find("data-jtml-fetch=\"users\""), std::string::npos);
    EXPECT_NE(html.find("data-url=\"/api/users\""), std::string::npos);
    EXPECT_NE(html.find("data-jtml-expr=\"(users.error)\""), std::string::npos);
    EXPECT_NE(html.find("data-jtml-cond-expr=\"(users.loading)\""), std::string::npos);
    EXPECT_NE(html.find("data-jtml-for-expr=&quot;(users.data)&quot;"), std::string::npos);
    EXPECT_NE(html.find("function startFetchBindings()"), std::string::npos);
    EXPECT_NE(html.find("function applyClientState()"), std::string::npos);
    EXPECT_NE(html.find("\\s*\\)?\\s*\\}\\}"), std::string::npos);

    InterpreterConfig config;
    config.startWebSocket = false;
    Interpreter interpreter(transpiler, config);
    interpreter.interpret(program);
    auto bindings = nlohmann::json::parse(interpreter.getBindingsJSON());
    ASSERT_TRUE(bindings.contains("conditions"));
    ASSERT_FALSE(bindings["conditions"].empty());
}

TEST(FriendlySyntax, FetchDeclarationsSupportMethodAndBody) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "let email = \"ada@example.com\"\n"
        "let result = fetch \"/api/login\" method \"POST\" body { email: email } cache \"no-store\" credentials \"include\"\n"
        "page\n"
        "  show result.loading\n");

    EXPECT_NE(classic.find("data-jtml-fetch=\"result\""), std::string::npos);
    EXPECT_NE(classic.find("data-method=\"POST\""), std::string::npos);
    EXPECT_NE(classic.find("data-body-expr=\"{ email: email }\""), std::string::npos);
    EXPECT_NE(classic.find("data-cache=\"no-store\""), std::string::npos);
    EXPECT_NE(classic.find("data-credentials=\"include\""), std::string::npos);

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;
    JtmlTranspiler transpiler;
    std::string html = transpiler.transpile(program);
    EXPECT_NE(html.find("data-body-expr=\"{ email: email }\""), std::string::npos);
    EXPECT_NE(html.find("data-cache=\"no-store\""), std::string::npos);
    EXPECT_NE(html.find("data-credentials=\"include\""), std::string::npos);
    EXPECT_NE(html.find("function evaluateClientBodyExpression"), std::string::npos);
    EXPECT_NE(html.find("options.cache = cachePolicy"), std::string::npos);
    EXPECT_NE(html.find("options.credentials = credentialsPolicy"), std::string::npos);
    EXPECT_NE(html.find("content-type"), std::string::npos);

    InterpreterConfig config;
    config.startWebSocket = false;
    Interpreter interpreter(transpiler, config);
    EXPECT_NO_THROW(interpreter.interpret(program));
    auto bindings = nlohmann::json::parse(interpreter.getBindingsJSON());
    ASSERT_TRUE(bindings.contains("state"));
    EXPECT_EQ(bindings["state"]["email"], "ada@example.com");
}

TEST(FriendlySyntax, MissingFetchObjectPropertiesAreFalseyAtStartup) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "let email = \"ada@example.com\"\n"
        "let login = fetch \"/api/login\" method \"POST\" body { email: email }\n"
        "page\n"
        "  if login.data.user\n"
        "    text \"Signed in as {login.data.user.name}\"\n"
        "  else\n"
        "    text \"Waiting for login\"\n");

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    InterpreterConfig config;
    config.startWebSocket = false;
    Interpreter interpreter(transpiler, config);
    EXPECT_NO_THROW(interpreter.interpret(program));
}

TEST(FriendlySyntax, FetchDeclarationsSupportTimeoutRetryAndStalePolicy) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "let users = fetch \"/api/users\" timeout 2500 retry 2 stale keep refresh reloadUsers\n"
        "page\n"
        "  show users.loading\n");

    EXPECT_NE(classic.find("data-jtml-fetch=\"users\""), std::string::npos);
    EXPECT_NE(classic.find("data-timeout-ms=\"2500\""), std::string::npos);
    EXPECT_NE(classic.find("data-retry=\"2\""), std::string::npos);
    EXPECT_NE(classic.find("data-stale=\"keep\""), std::string::npos);

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    std::string html = transpiler.transpile(program);
    EXPECT_NE(html.find("new AbortController()"), std::string::npos);
    EXPECT_NE(html.find("maxRetries"), std::string::npos);
    EXPECT_NE(html.find("stalePolicy === 'keep'"), std::string::npos);
}

TEST(FriendlySyntax, InvalidateRefreshesFetchAfterActionDispatch) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "let users = fetch \"/api/users\" stale keep\n"
        "when saveUser\n"
        "  let saved = true\n"
        "  invalidate users\n"
        "page\n"
        "  button \"Save\" click saveUser\n"
        "  for user in users.data\n"
        "    text user.name\n");

    EXPECT_NE(classic.find("data-jtml-fetch=\"users\""), std::string::npos);
    EXPECT_NE(classic.find("data-jtml-invalidate-action=\"saveUser\""), std::string::npos);
    EXPECT_NE(classic.find("data-jtml-invalidate-fetches=\"users\""), std::string::npos);

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    std::string html = transpiler.transpile(program);
    EXPECT_NE(html.find("const __jtml_fetch_fns = {}"), std::string::npos);
    EXPECT_NE(html.find("const __jtml_invalidate_fns = {}"), std::string::npos);
    EXPECT_NE(html.find("function startInvalidationBindings()"), std::string::npos);
    EXPECT_NE(html.find("await runInvalidations(fnName)"), std::string::npos);
}

TEST(FriendlySyntax, EffectLowersToSubscriptionFunction) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "let count = 0\n"
        "let message = \"Idle\"\n"
        "effect count\n"
        "  let message = \"Count changed\"\n"
        "page\n"
        "  show message\n");

    EXPECT_NE(classic.find("function __effect_count_"), std::string::npos);
    EXPECT_NE(classic.find("message = \"Count changed\"\\\\"), std::string::npos);
    EXPECT_NE(classic.find("subscribe __effect_count_"), std::string::npos);
    EXPECT_NE(classic.find(" to count\\\\"), std::string::npos);
}

TEST(FriendlySyntax, StoreBlockLowersToSharedDictionaryState) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "store auth\n"
        "  let user: string = \"Ada\"\n"
        "  let token = \"abc\"\n"
        "  when logout\n"
        "    let user = \"\"\n"
        "    let token = \"\"\n"
        "page\n"
        "  show auth.user\n"
        "  button \"Logout\" click auth.logout\n");

    EXPECT_NE(classic.find("define auth = {user: \"Ada\", token: \"abc\"}\\"),
              std::string::npos);
    EXPECT_NE(classic.find("function auth_logout()\\\\"), std::string::npos);
    EXPECT_NE(classic.find("auth.user = \"\"\\\\"), std::string::npos);
    EXPECT_NE(classic.find("auth.token = \"\"\\\\"), std::string::npos);
    EXPECT_NE(classic.find("show auth.user\\\\"), std::string::npos);
    EXPECT_NE(classic.find("@button onClick=auth_logout()\\\\"), std::string::npos);

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    std::string html = transpiler.transpile(program);
    const std::string marker = "sendEvent('";
    const auto eventPos = html.find(marker);
    ASSERT_NE(eventPos, std::string::npos) << html;
    const auto idStart = eventPos + marker.size();
    const auto idEnd = html.find("'", idStart);
    ASSERT_NE(idEnd, std::string::npos) << html;
    const std::string elementId = html.substr(idStart, idEnd - idStart);

    InterpreterConfig config;
    config.startWebSocket = false;
    Interpreter interpreter(transpiler, config);
    interpreter.interpret(program);
    auto bindings = nlohmann::json::parse(interpreter.getBindingsJSON());
    bool sawAda = false;
    for (auto it = bindings["content"].begin(); it != bindings["content"].end(); ++it) {
        if (it.value() == "Ada") sawAda = true;
    }
    EXPECT_TRUE(sawAda) << bindings.dump(2);

    std::string updatedBindings;
    std::string error;
    ASSERT_TRUE(interpreter.dispatchEvent(elementId, "onClick", nlohmann::json::array(),
                                          updatedBindings, error)) << error;
    auto updated = nlohmann::json::parse(updatedBindings);
    bool sawEmptyUser = false;
    for (auto it = updated["content"].begin(); it != updated["content"].end(); ++it) {
        if (it.value() == "") sawEmptyUser = true;
    }
    EXPECT_TRUE(sawEmptyUser) << updated.dump(2);

    auto state = nlohmann::json::parse(interpreter.getStateJSON());
    ASSERT_TRUE(state["variables"].contains("auth")) << state.dump(2);
    EXPECT_EQ(state["variables"]["auth"]["value"]["user"], "");
    EXPECT_EQ(state["variables"]["auth"]["value"]["token"], "");
}

// ---------------------------------------------------------------------------
// Core Friendly Statements
// ---------------------------------------------------------------------------

TEST(FriendlySyntax, PageElementsEventsAndInputLowerToClassic) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "let email = \"\"\n"
        "when save\n"
        "  let email = email + \"!\"\n"
        "page class \"app\"\n"
        "  input \"Email\" into email required\n"
        "  button \"Save\" click save\n");

    EXPECT_NE(classic.find("@main class=\"app\"\\\\"), std::string::npos);
    EXPECT_NE(classic.find("@input placeholder=\"Email\" value=email onInput=setEmail() required\\\\"),
              std::string::npos);
    EXPECT_NE(classic.find("@button onClick=save()\\\\"), std::string::npos);
    EXPECT_NE(classic.find("function setEmail(value)\\\\"), std::string::npos);
}

TEST(FriendlySyntax, InterpolatedStringsLowerToConcatenation) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "let count = 2\n"
        "page\n"
        "  show \"Count: {count}\"\n");

    EXPECT_NE(classic.find("show \"Count: \" + count\\\\"), std::string::npos);
}

TEST(FriendlySyntax, ConditionalExpressionsLowerAndParse) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "let submitted = false\n"
        "let email = \"ada@example.com\"\n"
        "get label = submitted ? \"Subscribed: {email}\" : \"Enter your email below.\"\n"
        "page\n"
        "  show label\n");

    EXPECT_NE(classic.find("derive label = submitted ? (\"Subscribed: \" + email) : \"Enter your email below.\"\\\\"),
              std::string::npos);
}

TEST(FriendlySyntax, PlainAndCompoundAssignmentsLower) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "let count = 0\n"
        "when add\n"
        "  count += 1\n"
        "  count = count + 1\n"
        "page\n"
        "  show count\n");

    EXPECT_NE(classic.find("count += 1\\\\"), std::string::npos);
    EXPECT_NE(classic.find("count = count + 1\\\\"), std::string::npos);
}

TEST(FriendlySyntax, TypeAnnotationsAreAcceptedAndPreservedForTooling) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "let count: number = 0\n"
        "const name: string = \"Ada\"\n"
        "get label: string = \"Hello {name}\"\n"
        "when save email: string age:number\n"
        "  let count: number = count + 1\n"
        "make Badge title: string\n"
        "  text title\n"
        "page\n"
        "  Badge label\n"
        "  show count\n");

    EXPECT_NE(classic.find("define count: number = 0\\\\"), std::string::npos);
    EXPECT_NE(classic.find("const name: string = \"Ada\"\\\\"), std::string::npos);
    EXPECT_NE(classic.find("derive label: string = \"Hello \" + name\\\\"), std::string::npos);
    EXPECT_NE(classic.find("function save(email, age)\\\\"), std::string::npos);
    EXPECT_NE(classic.find("count = count + 1\\\\"), std::string::npos);
    EXPECT_NE(classic.find("show label\\\\"), std::string::npos);
    EXPECT_NE(classic.find(": number"), std::string::npos);
    EXPECT_NE(classic.find(": string"), std::string::npos);
}

TEST(FriendlySyntax, TryCatchFinallyLowerToClassicErrorHandling) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "page\n"
        "  try\n"
        "    show \"trying\"\n"
        "  catch error\n"
        "    show error\n"
        "  finally\n"
        "    show \"done\"\n");

    EXPECT_NE(classic.find("try\\\\"), std::string::npos);
    EXPECT_NE(classic.find("except(error)\\\\"), std::string::npos);
    EXPECT_NE(classic.find("then\\\\"), std::string::npos);
}

TEST(FriendlySyntax, ComponentsExpandWithParamsAndSlots) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make Card title\n"
        "  section class \"card\"\n"
        "    h2 title\n"
        "    slot\n"
        "page\n"
        "  Card \"Revenue\"\n"
        "    p \"42k\"\n");

    EXPECT_EQ(classic.find("make Card"), std::string::npos);
    EXPECT_NE(classic.find("@section class=\"card\"\\\\"), std::string::npos);
    EXPECT_NE(classic.find("show \"Revenue\"\\\\"), std::string::npos);
    EXPECT_NE(classic.find("show \"42k\"\\\\"), std::string::npos);
}

TEST(FriendlySyntax, ComponentInstancesGetIsolatedLocalNames) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make Counter label\n"
        "  let count = 0\n"
        "  when add\n"
        "    count += 1\n"
        "  box\n"
        "    h2 label\n"
        "    show count\n"
        "    button \"+\" click add\n"
        "page\n"
        "  Counter \"First\"\n"
        "  Counter \"Second\"\n");

    EXPECT_NE(classic.find("define __Counter_1_count = 0\\\\"), std::string::npos);
    EXPECT_NE(classic.find("function __Counter_1_add()\\\\"), std::string::npos);
    EXPECT_NE(classic.find("__Counter_1_count += 1\\\\"), std::string::npos);
    EXPECT_NE(classic.find("show __Counter_1_count\\\\"), std::string::npos);
    EXPECT_NE(classic.find("onClick=__Counter_1_add()"), std::string::npos);
    EXPECT_NE(classic.find("define __Counter_2_count = 0\\\\"), std::string::npos);
    EXPECT_NE(classic.find("function __Counter_2_add()\\\\"), std::string::npos);
    EXPECT_NE(classic.find("show __Counter_2_count\\\\"), std::string::npos);
    EXPECT_NE(classic.find("onClick=__Counter_2_add()"), std::string::npos);
}

TEST(FriendlySyntax, ComponentCallsValidateArguments) {
    EXPECT_THROW(
        (void)jtml::normalizeSourceSyntax(
            "make Card title\n"
            "  h2 title\n"
            "page\n"
            "  Card\n",
            jtml::SyntaxMode::Friendly),
        std::runtime_error);
}

TEST(FriendlySyntax, UndefinedUppercaseComponentIsAnError) {
    EXPECT_THROW(
        (void)jtml::normalizeSourceSyntax("page\n  Card \"Revenue\"\n", jtml::SyntaxMode::Friendly),
        std::runtime_error);
}

// ---------------------------------------------------------------------------
// Const Declarations
// ---------------------------------------------------------------------------

TEST(FriendlySyntax, ConstDeclarationLowersToClassicConst) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "const PI = 3.14\n"
        "const MAX = 100\n"
        "page\n"
        "  show PI\n");

    EXPECT_NE(classic.find("const PI = 3.14\\\\"), std::string::npos);
    EXPECT_NE(classic.find("const MAX = 100\\\\"), std::string::npos);
}

// ---------------------------------------------------------------------------
// While Loops
// ---------------------------------------------------------------------------

TEST(FriendlySyntax, WhileLoopLowersToClassicWhile) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "let running = true\n"
        "when stop\n"
        "  let running = false\n"
        "page\n"
        "  show \"running\"\n");

    EXPECT_NE(classic.find("define running = true\\\\"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Element Dictionary Aliases
// ---------------------------------------------------------------------------

TEST(FriendlySyntax, TextElementMapsToP) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "page\n"
        "  text \"Hello world\"\n");

    EXPECT_NE(classic.find("@p\\\\"), std::string::npos);
    EXPECT_NE(classic.find("show \"Hello world\"\\\\"), std::string::npos);
}

TEST(FriendlySyntax, BoxElementMapsToDiv) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "page\n"
        "  box class \"container\"\n"
        "    h1 \"Title\"\n");

    EXPECT_NE(classic.find("@div class=\"container\"\\\\"), std::string::npos);
}

TEST(FriendlySyntax, LinkElementMapsToA) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "page\n"
        "  link \"Home\" href \"/\"\n");

    EXPECT_NE(classic.find("@a href=\"/\"\\\\"), std::string::npos);
    EXPECT_NE(classic.find("show \"Home\"\\\\"), std::string::npos);
}

TEST(FriendlySyntax, ImageElementMapsToImg) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "page\n"
        "  image src \"/logo.png\" alt \"Logo\"\n");

    EXPECT_NE(classic.find("@img src=\"/logo.png\" alt=\"Logo\"\\\\"), std::string::npos);
}

TEST(FriendlySyntax, EmbedElementMapsToIframe) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "page\n"
        "  embed src \"https://example.com\"\n");

    EXPECT_NE(classic.find("@iframe src=\"https://example.com\"\\\\"), std::string::npos);
}

TEST(FriendlySyntax, ListAndItemMapToUlLi) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "page\n"
        "  list\n"
        "    item \"First\"\n"
        "    item \"Second\"\n");

    EXPECT_NE(classic.find("@ul\\\\"), std::string::npos);
    EXPECT_NE(classic.find("@li\\\\"), std::string::npos);
}

TEST(FriendlySyntax, OrderedListMapsToOl) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "page\n"
        "  list ordered\n"
        "    item \"Step 1\"\n"
        "    item \"Step 2\"\n");

    EXPECT_NE(classic.find("@ol\\\\"), std::string::npos);
    EXPECT_EQ(classic.find("@ul"), std::string::npos);
}

TEST(FriendlySyntax, CheckboxElementMapsToInputCheckbox) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "let agreed = false\n"
        "page\n"
        "  checkbox \"Accept Terms\" into agreed\n");

    EXPECT_NE(classic.find("@input type=\"checkbox\""), std::string::npos);
    EXPECT_NE(classic.find("value=agreed"), std::string::npos);
    EXPECT_NE(classic.find("onChange=setAgreed()"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Destructured Imports
// ---------------------------------------------------------------------------

TEST(FriendlySyntax, DestructuredImportLowersToClassicImport) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "use { formatMoney, parseDate } from \"./money.jtml\"\n"
        "page\n"
        "  show \"ready\"\n");

    EXPECT_NE(classic.find("import \"./money.jtml\"\\\\"), std::string::npos);
}

TEST(FriendlySyntax, NamedImportLowersToClassicImport) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "use Button from \"./button.jtml\"\n"
        "page\n"
        "  show \"ready\"\n");

    EXPECT_NE(classic.find("import \"./button.jtml\"\\\\"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Media Elements
// ---------------------------------------------------------------------------

TEST(FriendlySyntax, VideoElementWithBooleanAttrs) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "page\n"
        "  video src \"/demo.mp4\" controls\n");

    EXPECT_NE(classic.find("@video src=\"/demo.mp4\" controls\\\\"), std::string::npos);
}

TEST(FriendlySyntax, AudioElementWithAutoplay) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "page\n"
        "  audio src \"/sound.mp3\" autoplay\n");

    EXPECT_NE(classic.find("@audio src=\"/sound.mp3\" autoplay\\\\"), std::string::npos);
}

TEST(FriendlySyntax, FileInputLowersWithAccessibleLabelAndFileState) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "let selected = \"\"\n"
        "page\n"
        "  file \"Choose image\" accept \"image/*\" into selected\n");

    EXPECT_NE(classic.find("@input type=\"file\" aria-label=\"Choose image\" accept=\"image/*\" onChange=setSelected()\\\\"),
              std::string::npos);
    EXPECT_EQ(classic.find("value=selected"), std::string::npos);
}

TEST(FriendlySyntax, DropzoneLowersToFileInputWithDropzoneMarker) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "let assets = []\n"
        "page\n"
        "  dropzone \"Drop media\" accept \"image/*,video/*\" into assets\n");

    EXPECT_NE(classic.find("@input type=\"file\" data-jtml-dropzone=\"true\" multiple aria-label=\"Drop media\" accept=\"image/*,video/*\" onChange=setAssets()\\\\"),
              std::string::npos);
}

TEST(FriendlySyntax, MediaIntoLowersToControllerMetadataAndState) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "page\n"
        "  video src \"/demo.mp4\" controls into player\n"
        "  show \"Paused: {player.paused}\"\n");

    EXPECT_NE(classic.find("@video src=\"/demo.mp4\" controls data-jtml-media-controller=\"player\"\\\\"),
              std::string::npos);
    EXPECT_NE(classic.find("define player = {currentTime: 0, duration: 0, paused: true"),
              std::string::npos);
}

TEST(FriendlySyntax, MediaControllerActionsAreClientIntercepted) {
    const std::string classic = normalizeOk(
        "jtml 2\n"
        "page\n"
        "  video src \"/demo.mp4\" controls into player\n"
        "  button \"Play\" click player.play\n"
        "  button \"Pause\" click player.pause\n");

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;
    JtmlTranspiler transpiler;
    const std::string html = transpiler.transpile(program);
    EXPECT_NE(html.find("data-jtml-media-controller=\"player\""), std::string::npos);
    EXPECT_NE(html.find("const __jtml_media_actions = {}"), std::string::npos);
    EXPECT_NE(html.find("startMediaControllerBindings()"), std::string::npos);
    EXPECT_NE(html.find("__jtml_media_actions[name + '.play']"), std::string::npos);
}

TEST(FriendlySyntax, GraphicAliasesLowerToAccessibleSvgShapes) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "page\n"
        "  graphic aria-label \"Revenue chart\" width \"320\" height \"120\" viewBox \"0 0 320 120\"\n"
        "    bar x \"20\" y \"40\" width \"70\" height \"60\" fill \"#0f766e\"\n"
        "    dot cx \"260\" cy \"60\" r \"18\" fill \"#2563eb\"\n"
        "    line x1 \"20\" y1 \"104\" x2 \"300\" y2 \"104\" stroke \"#475569\" stroke-width \"2\"\n"
        "    path d \"M20 90 C90 20 180 120 300 40\" fill \"none\" stroke \"#9333ea\" stroke-width \"3\"\n"
        "    polyline points \"20,20 70,50 120,30\" fill \"none\" stroke \"#111827\"\n"
        "    polygon points \"240,24 292,112 188,112\" fill \"#f59e0b\" opacity \"0.7\"\n");

    EXPECT_NE(classic.find("@svg role=\"img\" aria-label=\"Revenue chart\" width=\"320\" height=\"120\" viewBox=\"0 0 320 120\"\\\\"),
              std::string::npos);
    EXPECT_NE(classic.find("@rect x=\"20\" y=\"40\" width=\"70\" height=\"60\" fill=\"#0f766e\"\\\\"),
              std::string::npos);
    EXPECT_NE(classic.find("@circle cx=\"260\" cy=\"60\" r=\"18\" fill=\"#2563eb\"\\\\"),
              std::string::npos);
    EXPECT_NE(classic.find("@line x1=\"20\" y1=\"104\" x2=\"300\" y2=\"104\" stroke=\"#475569\" stroke-width=\"2\"\\\\"),
              std::string::npos);
    EXPECT_NE(classic.find("@path d=\"M20 90 C90 20 180 120 300 40\" fill=\"none\" stroke=\"#9333ea\" stroke-width=\"3\"\\\\"),
              std::string::npos);
    EXPECT_NE(classic.find("@polyline points=\"20,20 70,50 120,30\" fill=\"none\" stroke=\"#111827\"\\\\"),
              std::string::npos);
    EXPECT_NE(classic.find("@polygon points=\"240,24 292,112 188,112\" fill=\"#f59e0b\" opacity=\"0.7\"\\\\"),
              std::string::npos);
}

TEST(FriendlySyntax, ChartBarLowersToRuntimeRenderedSvg) {
    const std::string classic = normalizeOk(
        "jtml 2\n"
        "let revenue = [{ \"month\": \"Jan\", \"total\": 12 }, { \"month\": \"Feb\", \"total\": 18 }]\n"
        "page\n"
        "  chart bar data revenue by month value total label \"Revenue by month\" color \"#2563eb\"\n");

    EXPECT_NE(classic.find("@svg role=\"img\" aria-label=\"Revenue by month\" data-jtml-chart=\"bar\""),
              std::string::npos);
    EXPECT_NE(classic.find("data-jtml-chart-data=\"revenue\""),
              std::string::npos);
    EXPECT_NE(classic.find("data-jtml-chart-by=\"month\""),
              std::string::npos);
    EXPECT_NE(classic.find("data-jtml-chart-value=\"total\""),
              std::string::npos);

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;
    JtmlTranspiler transpiler;
    const std::string html = transpiler.transpile(program);
    EXPECT_NE(html.find("data-jtml-chart=\"bar\""), std::string::npos);
    EXPECT_NE(html.find("function renderCharts()"), std::string::npos);
    EXPECT_NE(html.find("normalizeChartRows"), std::string::npos);
}

TEST(FriendlySyntax, Scene3DLowersToCanvasMountWithFallbackRuntime) {
    const std::string classic = normalizeOk(
        "jtml 2\n"
        "page\n"
        "  scene3d \"Product model\" scene productScene camera orbit controls orbit renderer \"three\" into sceneState width \"640\" height \"360\"\n");

    EXPECT_NE(classic.find("@canvas data-jtml-scene3d=\"true\" role=\"img\" aria-label=\"Product model\" data-jtml-scene=\"productScene\" data-jtml-camera=\"orbit\" data-jtml-controls=\"orbit\" data-jtml-renderer=\"three\" data-jtml-scene3d-controller=\"sceneState\" width=\"640\" height=\"360\"\\\\"),
              std::string::npos);
    EXPECT_NE(classic.find("define sceneState = {scene: \"\", camera: \"orbit\", controls: \"orbit\", renderer: \"auto\", status: \"idle\", hostRendered: false, width: 0, height: 0}\\\\"),
              std::string::npos);

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;
    JtmlTranspiler transpiler;
    const std::string html = transpiler.transpile(program);
    EXPECT_NE(html.find("data-jtml-scene3d=\"true\""), std::string::npos);
    EXPECT_NE(html.find("data-jtml-scene=\"productScene\""), std::string::npos);
    EXPECT_NE(html.find("data-jtml-camera=\"orbit\""), std::string::npos);
    EXPECT_NE(html.find("data-jtml-scene3d-controller=\"sceneState\""), std::string::npos);
    EXPECT_NE(html.find("function startScene3DBindings()"), std::string::npos);
    EXPECT_NE(html.find("function scene3DStateFor(canvas, spec, hostRendered, status, extra)"), std::string::npos);
    EXPECT_NE(html.find("clientState[controllerName] = scene3DStateFor"), std::string::npos);
    EXPECT_NE(html.find("window.jtml3d.render(canvas, spec)"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Select / Option Elements
// ---------------------------------------------------------------------------

TEST(FriendlySyntax, SelectAndOptionElements) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "let role = \"\"\n"
        "page\n"
        "  select into role\n"
        "    option \"Admin\" value \"admin\"\n"
        "    option \"User\" value \"user\"\n");

    EXPECT_NE(classic.find("@select value=role"), std::string::npos);
    EXPECT_NE(classic.find("@option value=\"admin\"\\\\"), std::string::npos);
    EXPECT_NE(classic.find("@option value=\"user\"\\\\"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Form Elements
// ---------------------------------------------------------------------------

TEST(FriendlySyntax, FormWithSubmitEvent) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "when loginUser\n"
        "  show \"logged in\"\n"
        "page\n"
        "  form submit loginUser\n"
        "    input \"Username\" into username\n"
        "    button \"Login\"\n");

    EXPECT_NE(classic.find("@form onSubmit=loginUser()\\\\"), std::string::npos);
}

// ---------------------------------------------------------------------------
// If / Else
// ---------------------------------------------------------------------------

TEST(FriendlySyntax, IfElseLowersCorrectly) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "let loggedIn = true\n"
        "page\n"
        "  if loggedIn\n"
        "    show \"Welcome\"\n"
        "  else\n"
        "    show \"Please log in\"\n");

    EXPECT_NE(classic.find("if (loggedIn)\\\\"), std::string::npos);
    EXPECT_NE(classic.find("else\\\\"), std::string::npos);
}

// ---------------------------------------------------------------------------
// For Loop
// ---------------------------------------------------------------------------

TEST(FriendlySyntax, ForLoopLowersCorrectly) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "page\n"
        "  for user in users\n"
        "    show user\n");

    EXPECT_NE(classic.find("for (user in users)\\\\"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Return, Throw, Break, Continue
// ---------------------------------------------------------------------------

TEST(FriendlySyntax, ReturnStatementLowers) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "when compute\n"
        "  return 42\n"
        "page\n"
        "  show \"ready\"\n");

    EXPECT_NE(classic.find("return 42\\\\"), std::string::npos);
}

TEST(FriendlySyntax, ThrowStatementLowers) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "when fail\n"
        "  throw \"error\"\n"
        "page\n"
        "  show \"ready\"\n");

    EXPECT_NE(classic.find("throw \"error\"\\\\"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Inline Text Rules
// ---------------------------------------------------------------------------

TEST(FriendlySyntax, H1InlineTextBecomesShow) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "page\n"
        "  h1 \"Hello\"\n");

    EXPECT_NE(classic.find("@h1\\\\"), std::string::npos);
    EXPECT_NE(classic.find("show \"Hello\"\\\\"), std::string::npos);
}

TEST(FriendlySyntax, StrongElementWithInlineText) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "page\n"
        "  strong \"Warning:\"\n");

    EXPECT_NE(classic.find("@strong\\\\"), std::string::npos);
    EXPECT_NE(classic.find("show \"Warning:\"\\\\"), std::string::npos);
}

TEST(FriendlySyntax, CompleteAttributeTokensDoNotConsumeFollowingAttributes) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "page\n"
        "  input placeholder=\"Email\" required\n"
        "  p \"count = {count}\"\n");

    EXPECT_NE(classic.find("@input placeholder=\"Email\" required\\\\"), std::string::npos);
    EXPECT_NE(classic.find("show \"count = \" + count\\\\"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Comments
// ---------------------------------------------------------------------------

TEST(FriendlySyntax, LineCommentsAreStripped) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "// This is a comment\n"
        "let count = 0\n"
        "page\n"
        "  h1 \"Hello\" // inline comment\n");

    EXPECT_EQ(classic.find("comment"), std::string::npos);
    EXPECT_NE(classic.find("define count = 0\\\\"), std::string::npos);
}

TEST(FriendlySyntax, HashCommentsAreStripped) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "# This is a comment\n"
        "let count = 0\n"
        "page\n"
        "  h1 \"Hello\"\n");

    EXPECT_NE(classic.find("define count = 0\\\\"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Route hardening: active-class, guard, active-route
// ---------------------------------------------------------------------------

TEST(FriendlySyntax, LinkActiveClassEmitsDataAttribute) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "page\n"
        "  link \"Home\" to \"/\" active-class \"active\"\n");

    EXPECT_NE(classic.find("href=\"javascript:void(0)\""), std::string::npos);
    EXPECT_NE(classic.find("data-jtml-href=\"#/\""), std::string::npos);
    EXPECT_NE(classic.find("data-jtml-link=\"true\""), std::string::npos);
    EXPECT_NE(classic.find("data-jtml-active-class=\"active\""), std::string::npos);
}

TEST(FriendlySyntax, GuardEmitsMetaElement) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "let token = \"\"\n"
        "guard \"/dashboard\" require token else \"/login\"\n"
        "page\n"
        "  h1 \"App\"\n");

    EXPECT_NE(classic.find("data-jtml-route-guard=\"/dashboard\""), std::string::npos);
    EXPECT_NE(classic.find("data-jtml-guard-var=\"token\""), std::string::npos);
    EXPECT_NE(classic.find("data-jtml-guard-redirect=\"/login\""), std::string::npos);
}

TEST(FriendlySyntax, GuardWithoutRedirectEmitsMetaElement) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "let isAdmin = false\n"
        "guard \"/admin\" require isAdmin\n"
        "page\n"
        "  h1 \"App\"\n");

    EXPECT_NE(classic.find("data-jtml-route-guard=\"/admin\""), std::string::npos);
    EXPECT_NE(classic.find("data-jtml-guard-var=\"isAdmin\""), std::string::npos);
    EXPECT_EQ(classic.find("data-jtml-guard-redirect"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Component instance markers
// ---------------------------------------------------------------------------

TEST(FriendlySyntax, ComponentExpansionAddsInstanceMarker) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make Badge label\n"
        "  text label\n"
        "page\n"
        "  Badge \"Gold\"\n");

    EXPECT_NE(classic.find("data-jtml-instance=\"Badge_"), std::string::npos);
    EXPECT_NE(classic.find("data-jtml-component=\"Badge\""), std::string::npos);
    EXPECT_NE(classic.find("data-jtml-instance-id="), std::string::npos);
    EXPECT_NE(classic.find("data-jtml-component-params=\"label=Gold\""), std::string::npos);
    EXPECT_NE(classic.find("data-jtml-source-line=\"5\""), std::string::npos);
    EXPECT_NE(classic.find("data-jtml-component-def=\"Badge\""), std::string::npos);
    EXPECT_NE(classic.find("data-jtml-component-def-params=\"label\""), std::string::npos);
    EXPECT_NE(classic.find("data-jtml-component-body-hex="), std::string::npos);

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    std::string html = transpiler.transpile(program);
    EXPECT_NE(html.find("window.__jtml_component_definitions"), std::string::npos);
    EXPECT_NE(html.find("getComponentDefinitions"), std::string::npos);
    EXPECT_NE(html.find("findComponentDefinition"), std::string::npos);
}

TEST(FriendlySyntax, TwoComponentCallsGetDistinctInstanceIds) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make Widget label\n"
        "  h2 label\n"
        "page\n"
        "  Widget \"A\"\n"
        "  Widget \"B\"\n");

    auto pos1 = classic.find("data-jtml-instance=\"Widget_");
    ASSERT_NE(pos1, std::string::npos);
    auto pos2 = classic.find("data-jtml-instance=\"Widget_", pos1 + 1);
    ASSERT_NE(pos2, std::string::npos);
    // Extract ids and verify they differ
    auto id1 = classic.substr(pos1, 40);
    auto id2 = classic.substr(pos2, 40);
    EXPECT_NE(id1, id2);
}

TEST(FriendlySyntax, ComponentMetadataListsIsolatedLocals) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make Counter label\n"
        "  let count = 0\n"
        "  when add\n"
        "    count += 1\n"
        "  box\n"
        "    show count\n"
        "    button \"+\" click add\n"
        "page\n"
        "  Counter \"First\"\n");

    EXPECT_NE(classic.find("data-jtml-component=\"Counter\""), std::string::npos);
    EXPECT_NE(classic.find("data-jtml-component-locals=\"add=__Counter_1_add;count=__Counter_1_count\""),
              std::string::npos);

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    std::string html = transpiler.transpile(program);
    EXPECT_NE(html.find("data-jtml-component=\"Counter\""), std::string::npos);
    EXPECT_NE(html.find("function scanComponentInstances()"), std::string::npos);
    EXPECT_NE(html.find("window.__jtml_components = componentInstances"), std::string::npos);
    EXPECT_NE(html.find("jtml:components-ready"), std::string::npos);
}

TEST(FriendlySyntax, InterpreterRegistersComponentInstancesAndLocalState) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make Counter label\n"
        "  let count = 0\n"
        "  when add\n"
        "    count += 1\n"
        "  box\n"
        "    show count\n"
        "    button \"+\" click add\n"
        "page\n"
        "  Counter \"First\"\n"
        "  Counter \"Second\"\n");

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    std::string html = transpiler.transpile(program);
    const std::string marker = "sendEvent('";
    const auto eventPos = html.find(marker);
    ASSERT_NE(eventPos, std::string::npos) << html;
    const auto idStart = eventPos + marker.size();
    const auto idEnd = html.find("'", idStart);
    ASSERT_NE(idEnd, std::string::npos) << html;
    const std::string elementId = html.substr(idStart, idEnd - idStart);

    InterpreterConfig config;
    config.startWebSocket = false;
    Interpreter interpreter(transpiler, config);
    interpreter.interpret(program);

    auto state = nlohmann::json::parse(interpreter.getStateJSON());
    ASSERT_TRUE(state.contains("componentDefinitions")) << state.dump(2);
    ASSERT_EQ(state["componentDefinitions"].size(), 1);
    EXPECT_EQ(state["componentDefinitions"][0]["name"], "Counter");
    ASSERT_EQ(state["componentDefinitions"][0]["params"].size(), 1);
    EXPECT_EQ(state["componentDefinitions"][0]["params"][0], "label");
    EXPECT_NE(state["componentDefinitions"][0]["body"].get<std::string>().find("let count = 0"),
              std::string::npos);
    ASSERT_TRUE(state.contains("components")) << state.dump(2);
    ASSERT_EQ(state["components"].size(), 2);
    EXPECT_EQ(state["components"][0]["component"], "Counter");
    EXPECT_EQ(state["components"][0]["locals"]["count"]["value"], 0);
    EXPECT_EQ(state["components"][0]["locals"]["add"]["function"], true);
    EXPECT_EQ(state["components"][1]["locals"]["count"]["value"], 0);

    std::string bindings;
    std::string error;
    ASSERT_TRUE(interpreter.dispatchEvent(elementId, "onClick", nlohmann::json::array(),
                                          bindings, error)) << error;
    state = nlohmann::json::parse(interpreter.getStateJSON());
    EXPECT_EQ(state["components"][0]["locals"]["count"]["value"], 1);
    EXPECT_EQ(state["components"][1]["locals"]["count"]["value"], 0);

    auto components = nlohmann::json::parse(interpreter.getComponentsJSON());
    ASSERT_EQ(components.size(), 2);
    ASSERT_TRUE(interpreter.dispatchComponentAction(
        components[1]["id"].get<std::string>(), "add", nlohmann::json::array(),
        bindings, error)) << error;
    state = nlohmann::json::parse(interpreter.getStateJSON());
    EXPECT_EQ(state["components"][0]["locals"]["count"]["value"], 1);
    EXPECT_EQ(state["components"][1]["locals"]["count"]["value"], 1);
}

TEST(FriendlySyntax, DerivedBindingsCanBeRedefinedDuringStudioReloads) {
    auto env = std::make_shared<JTML::Environment>(nullptr, 0, nullptr);
    const JTML::CompositeKey source{0, "count"};
    const JTML::CompositeKey derived{0, "attr_3"};

    env->defineVariable(source, std::make_shared<JTML::VarValue>(1.0), JTML::VarKind::Normal);
    auto evaluator = [](const ExpressionStatementNode*) {
        return std::make_shared<JTML::VarValue>(42.0);
    };

    Token countToken{TokenType::IDENTIFIER, "count", 0, 1, 1};
    env->deriveVariable(
        derived,
        std::make_unique<VariableExpressionStatementNode>(countToken),
        std::vector<JTML::CompositeKey>{source},
        evaluator);

    EXPECT_NO_THROW(env->deriveVariable(
        derived,
        std::make_unique<VariableExpressionStatementNode>(countToken),
        std::vector<JTML::CompositeKey>{source},
        evaluator));

    auto value = env->getVariable(derived);
    ASSERT_TRUE(value);
    EXPECT_EQ(value->toString(), "42");
}

TEST(FriendlySyntax, RouteAndLayoutComponentsEmitInstanceMetadata) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make Shell\n"
        "  box\n"
        "    slot\n"
        "make Home\n"
        "  h1 \"Home\"\n"
        "route \"/\" as Home layout Shell\n");

    EXPECT_NE(classic.find("data-jtml-component=\"Shell\""), std::string::npos);
    EXPECT_NE(classic.find("data-jtml-component-role=\"layout\""), std::string::npos);
    EXPECT_NE(classic.find("data-jtml-layout=\"Shell\""), std::string::npos);
    EXPECT_NE(classic.find("data-jtml-component=\"Home\""), std::string::npos);
    EXPECT_NE(classic.find("data-jtml-component-role=\"route\""), std::string::npos);
}

// Regression: clicking an event handler that lives in the *not initially
// taken* branch of an `if/else` used to fail with
// `No bindings for element: <id>` because the interpreter only registered
// bindings for the active branch. Studio's auth-store sample (Logout in
// the truthy branch, Login in the falsy branch) repro'd this.
TEST(FriendlyConditional, BothBranchesRegisterEventBindings) {
    const std::string src =
        "jtml 2\n"
        "\n"
        "store auth\n"
        "  let user = \"Ada\"\n"
        "  let loggedIn = true\n"
        "\n"
        "  when login\n"
        "    let user = \"Ada\"\n"
        "    let loggedIn = true\n"
        "\n"
        "  when logout\n"
        "    let user = \"\"\n"
        "    let loggedIn = false\n"
        "\n"
        "page\n"
        "  if auth.loggedIn\n"
        "    button \"Logout\" click auth.logout\n"
        "  else\n"
        "    button \"Login\" click auth.login\n";

    std::string classic = jtml::normalizeSourceSyntax(src, jtml::SyntaxMode::Friendly);
    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    std::string html = transpiler.transpile(program);

    // Both branches of the if/else live inside `data-then="..."` /
    // `data-else="..."` attributes on the wrapper, HTML-escaped, so the
    // ids appear as `sendEvent(&#39;attr_X&#39;, ...)` both times. We pull
    // every occurrence in source order.
    std::vector<std::string> elementIds;
    {
        const std::string marker = "sendEvent(&#39;";
        size_t pos = 0;
        while ((pos = html.find(marker, pos)) != std::string::npos) {
            const auto idStart = pos + marker.size();
            const auto idEnd = html.find('&', idStart);
            ASSERT_NE(idEnd, std::string::npos) << html;
            elementIds.push_back(html.substr(idStart, idEnd - idStart));
            pos = idEnd;
        }
    }
    ASSERT_EQ(elementIds.size(), 2u) << html;
    const std::string logoutId = elementIds[0];
    const std::string loginId  = elementIds[1];
    ASSERT_NE(logoutId, loginId);

    InterpreterConfig config;
    config.startWebSocket = false;
    Interpreter interpreter(transpiler, config);
    interpreter.interpret(program);

    // Dispatching `onClick` on either branch's button must succeed. Pre-fix,
    // the loginId dispatch failed with "No bindings for element: <loginId>"
    // because the else branch was never traversed at registration time.
    std::string updated;
    std::string error;
    EXPECT_TRUE(interpreter.dispatchEvent(logoutId, "onClick",
                                          nlohmann::json::array(),
                                          updated, error)) << error;
    EXPECT_TRUE(interpreter.dispatchEvent(loginId, "onClick",
                                          nlohmann::json::array(),
                                          updated, error)) << error;
}

// Regression: an `effect varName` body should re-assign outer scope
// variables when the user writes `let target = expr`. The rendered Studio
// effect example shows "Last: {last}" never updating; this test pins the
// runtime-side behaviour that mutating `last` from inside an `effect count`
// must surface in the bindings JSON after `count` changes.
TEST(FriendlyEffect, EffectBodyMutatesOuterVariableAndPropagates) {
    const std::string src =
        "jtml 2\n"
        "\n"
        "let count = 0\n"
        "let last = \"No changes yet.\"\n"
        "\n"
        "effect count\n"
        "  let last = \"Count changed to {count}\"\n"
        "\n"
        "when increment\n"
        "  count += 1\n"
        "\n"
        "page\n"
        "  p \"Count: {count}\"\n"
        "  p \"Last: {last}\"\n"
        "  button \"+\" click increment\n";

    std::string classic = jtml::normalizeSourceSyntax(src, jtml::SyntaxMode::Friendly);
    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    std::string html = transpiler.transpile(program);

    std::string elementId;
    {
        const std::string marker = "sendEvent('";
        const auto pos = html.find(marker);
        ASSERT_NE(pos, std::string::npos) << html;
        const auto idStart = pos + marker.size();
        const auto idEnd = html.find('\'', idStart);
        ASSERT_NE(idEnd, std::string::npos) << html;
        elementId = html.substr(idStart, idEnd - idStart);
    }

    InterpreterConfig config;
    config.startWebSocket = false;
    Interpreter interpreter(transpiler, config);
    interpreter.interpret(program);

    // Sanity: initial bindings carry the seed value.
    auto initial = nlohmann::json::parse(interpreter.getBindingsJSON());
    EXPECT_EQ(initial["state"]["last"], "No changes yet.")
        << initial.dump(2);

    // Click "+", then `count` becomes 1 and the subscribed effect updates
    // `last`. Both must be visible in the post-dispatch bindings JSON.
    std::string updatedJson;
    std::string error;
    ASSERT_TRUE(interpreter.dispatchEvent(elementId, "onClick",
                                          nlohmann::json::array(),
                                          updatedJson, error)) << error;
    auto updated = nlohmann::json::parse(updatedJson);
    EXPECT_EQ(updated["state"]["count"], 1) << updated.dump(2);
    EXPECT_EQ(updated["state"]["last"], "Count changed to 1") << updated.dump(2);
}

} // namespace
