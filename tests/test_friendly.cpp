#include "jtml/browser_runtime_assets.h"
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

nlohmann::json clientManifestFromHtml(const std::string& html) {
    const std::string open = "<script type=\"application/json\" id=\"__jtml_client_manifest\">";
    const std::string close = "</script>";
    const auto start = html.find(open);
    EXPECT_NE(start, std::string::npos) << html;
    const auto end = html.find(close, start);
    EXPECT_NE(end, std::string::npos) << html;
    return nlohmann::json::parse(
        html.substr(start + open.size(), end - (start + open.size())));
}

// ---------------------------------------------------------------------------
// Syntax Detection
// ---------------------------------------------------------------------------

TEST(FriendlySyntax, HeaderEnablesAutoMode) {
    EXPECT_TRUE(jtml::isFriendlySyntax("jtml 2\npage\n  h1 \"Hi\"\n"));
    EXPECT_TRUE(jtml::isFriendlySyntax("jtl 1\nlet count = 0\n"));
    EXPECT_TRUE(jtml::isJtlCoreSyntax("jtl 1\nlet count = 0\n"));
    EXPECT_FALSE(jtml::isJtlCoreSyntax("jtml 2\npage\n  h1 \"Hi\"\n"));
    EXPECT_FALSE(jtml::isFriendlySyntax("define count = 0\\\\\n"));
}

TEST(FriendlySyntax, LooksLikeFriendlyDetectsMoreKeywords) {
    EXPECT_TRUE(jtml::looksLikeFriendlySyntax("jtl 1\nlet count = 0\n"));
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
    EXPECT_TRUE(jtml::looksLikeFriendlySyntax("css raw\n  .host-widget { display: block; }\n"));
    EXPECT_TRUE(jtml::looksLikeFriendlySyntax("html raw \"<host-widget></host-widget>\"\n"));
    EXPECT_TRUE(jtml::looksLikeFriendlySyntax("theme\n  color primary \"#155e75\"\n"));
    EXPECT_TRUE(jtml::looksLikeFriendlySyntax("store auth\n  let user = \"Ada\"\n"));
    EXPECT_TRUE(jtml::looksLikeFriendlySyntax("export make Card\n  text \"Hi\"\n"));
    EXPECT_FALSE(jtml::looksLikeFriendlySyntax("define count = 0\\\\\n"));
}

TEST(FriendlySyntax, JtlCoreHeaderLowersThroughFriendlyPipeline) {
    std::string classic = normalizeOk(
        "jtl 1\n"
        "\n"
        "let total = 0\n"
        "get doubled = total * 2\n"
        "\n"
        "when add amount\n"
        "  total += amount\n"
        "\n"
        "if doubled > 2\n"
        "  show doubled\n");

    EXPECT_NE(classic.find("define total = 0\\\\"), std::string::npos);
    EXPECT_NE(classic.find("derive doubled = total * 2\\\\"), std::string::npos);
    EXPECT_NE(classic.find("function add(amount)\\\\"), std::string::npos);
    EXPECT_NE(classic.find("total += amount\\\\"), std::string::npos);
    EXPECT_NE(classic.find("if (doubled > 2)\\\\"), std::string::npos);
}

TEST(FriendlySyntax, RawHtmlAndCssEscapeHatchesLowerAndTranspile) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "css raw\n"
        "  .third-party-widget { display: grid; }\n"
        "  .third-party-widget iframe { border: 0; }\n"
        "page\n"
        "  h1 \"Interop\"\n"
        "  html raw \"<third-party-widget data-mode=\\\"demo\\\"></third-party-widget>\"\n");

    EXPECT_NE(classic.find("@style\\\\"), std::string::npos);
    EXPECT_NE(classic.find(".third-party-widget { display: grid; }"), std::string::npos);
    EXPECT_EQ(classic.find("[data-jtml-app] .third-party-widget"), std::string::npos);
    EXPECT_NE(classic.find("@raw\\\\"), std::string::npos);
    EXPECT_NE(classic.find("<third-party-widget data-mode=\\\"demo\\\"></third-party-widget>"), std::string::npos);

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    transpiler.setBrowserLocalRuntime(true);
    std::string html = transpiler.transpile(program);
    EXPECT_NE(html.find("<style>"), std::string::npos);
    EXPECT_NE(html.find(".third-party-widget iframe { border: 0; }"), std::string::npos);
    EXPECT_NE(html.find("<third-party-widget data-mode=\"demo\"></third-party-widget>"), std::string::npos);
    EXPECT_EQ(html.find("&lt;third-party-widget"), std::string::npos);
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
    transpiler.setBrowserLocalRuntime(true);
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

TEST(FriendlySyntax, ThemeAndUiPrimitivesLowerToSemanticHtmlAndCss) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "theme\n"
        "  color primary \"#155e75\"\n"
        "  space md 14\n"
        "  radius lg \"1rem\"\n"
        "page\n"
        "  shell align stretch\n"
        "    sidebar\n"
        "      navlink \"Home\" to \"/\" active-class \"active\"\n"
        "    content\n"
        "      panel title \"Usage\" pad lg shadow md width wide surface raised\n"
        "        grid cols 2 gap md\n"
        "          metric \"Users\" users.total \"Active\" tone good\n"
        "          card tone primary\n"
        "            h2 \"Ready\"\n");

    EXPECT_NE(classic.find("@style\\\\"), std::string::npos);
    EXPECT_NE(classic.find("--jtml-color-primary: #155e75;"), std::string::npos);
    EXPECT_NE(classic.find("--jtml-space-md: 14px;"), std::string::npos);
    EXPECT_NE(classic.find("--jtml-radius-lg: 1rem;"), std::string::npos);
    EXPECT_NE(classic.find("[data-jtml-app] .jtml-align-stretch"), std::string::npos);
    EXPECT_NE(classic.find("[data-jtml-app] .jtml-width-wide"), std::string::npos);
    EXPECT_NE(classic.find("[data-jtml-app] .jtml-surface-raised"), std::string::npos);
    EXPECT_NE(classic.find("@div class=\"jtml-shell jtml-align-stretch\" data-jtml-ui=\"shell\"\\\\"), std::string::npos);
    EXPECT_NE(classic.find("@section class=\"jtml-panel jtml-pad-lg jtml-shadow-md jtml-width-wide jtml-surface-raised\" data-jtml-ui=\"panel\"\\\\"), std::string::npos);
    EXPECT_NE(classic.find("@h2 class=\"jtml-panel-title\"\\\\"), std::string::npos);
    EXPECT_NE(classic.find("@div class=\"jtml-grid jtml-cols-2 jtml-gap-md\" data-jtml-ui=\"grid\"\\\\"), std::string::npos);
    EXPECT_NE(classic.find("@article class=\"jtml-metric jtml-tone-good\" data-jtml-ui=\"metric\"\\\\"), std::string::npos);
    EXPECT_NE(classic.find("data-jtml-link=\"true\""), std::string::npos);

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    transpiler.setBrowserLocalRuntime(true);
    std::string html = transpiler.transpile(program);
    EXPECT_NE(html.find("class=\"jtml-shell jtml-align-stretch\""), std::string::npos);
    EXPECT_NE(html.find("class=\"jtml-metric jtml-tone-good\""), std::string::npos);
    EXPECT_NE(html.find("--jtml-color-primary: #155e75;"), std::string::npos);
    EXPECT_NE(html.find("[data-jtml-app] .jtml-panel"), std::string::npos);
}

TEST(FriendlySyntax, OverlayPrimitivesSupportTitleSugar) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "when closeModal\n"
        "  let open = false\n"
        "page\n"
        "  modal title \"Confirm action\"\n"
        "    button \"Close\" click closeModal\n"
        "  drawer title \"Filters\"\n"
        "    button \"Hide\" click closeModal\n"
        "  toast title \"Saved\"\n"
        "    text \"Your changes were saved.\"\n");

    EXPECT_NE(classic.find("@section class=\"jtml-modal\" data-jtml-ui=\"modal\" role=\"dialog\" aria-modal=\"true\" tabindex=\"-1\"\\\\"),
              std::string::npos) << classic;
    EXPECT_NE(classic.find("@aside class=\"jtml-drawer\" data-jtml-ui=\"drawer\" role=\"dialog\" aria-modal=\"true\" tabindex=\"-1\"\\\\"),
              std::string::npos) << classic;
    EXPECT_NE(classic.find("@div class=\"jtml-toast\" data-jtml-ui=\"toast\" role=\"status\" aria-live=\"polite\"\\\\"),
              std::string::npos) << classic;
    EXPECT_NE(classic.find("show \"Confirm action\"\\\\"), std::string::npos) << classic;
    EXPECT_NE(classic.find("show \"Filters\"\\\\"), std::string::npos) << classic;
    EXPECT_NE(classic.find("show \"Saved\"\\\\"), std::string::npos) << classic;
}

TEST(FriendlySyntax, NavigationPrimitivesEmitAccessibilityRoles) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "when choose\n"
        "  let selected = \"usage\"\n"
        "page\n"
        "  tabs\n"
        "    tab \"Usage\" click choose\n");

    EXPECT_NE(classic.find("@div class=\"jtml-tabs\" data-jtml-ui=\"tabs\" role=\"tablist\"\\\\"),
              std::string::npos) << classic;
    EXPECT_NE(classic.find("@button class=\"jtml-tab\" data-jtml-ui=\"tab\" role=\"tab\" type=\"button\" onClick=choose()\\\\"),
              std::string::npos) << classic;
}

TEST(FriendlySyntax, FeedbackPrimitivesEmitAccessibilityRoles) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "page\n"
        "  alert \"Saved\"\n"
        "  error \"Failed\"\n"
        "  loading \"Loading users\"\n"
        "  empty \"No users yet\"\n");

    EXPECT_NE(classic.find("@div class=\"jtml-alert\" data-jtml-ui=\"alert\" role=\"alert\"\\\\"),
              std::string::npos) << classic;
    EXPECT_NE(classic.find("@div class=\"jtml-error\" data-jtml-ui=\"error\" role=\"alert\"\\\\"),
              std::string::npos) << classic;
    EXPECT_NE(classic.find("@div class=\"jtml-loading\" data-jtml-ui=\"loading\" role=\"status\" aria-live=\"polite\" aria-busy=\"true\"\\\\"),
              std::string::npos) << classic;
    EXPECT_NE(classic.find("@div class=\"jtml-empty\" data-jtml-ui=\"empty\" role=\"status\" aria-live=\"polite\"\\\\"),
              std::string::npos) << classic;
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
    transpiler.setBrowserLocalRuntime(true);
    std::string html = transpiler.transpile(program);
    EXPECT_NE(html.find("data-jtml-route"), std::string::npos);
    EXPECT_NE(html.find("function applyRoutes()"), std::string::npos);
    EXPECT_NE(html.find("const __jtml_routes = []"), std::string::npos);
    EXPECT_NE(html.find("function collectRouteBindings()"), std::string::npos);
    EXPECT_NE(html.find("window.jtml = Object.assign(window.jtml || {}, {"), std::string::npos);
    EXPECT_NE(html.find("getRoutes: function ()"), std::string::npos);
    EXPECT_NE(html.find("getCurrentRoute: function ()"), std::string::npos);
    EXPECT_NE(html.find("navigate: function (path)"), std::string::npos);
    EXPECT_NE(html.find("jtml:routes-ready"), std::string::npos);
    EXPECT_NE(html.find("jtml:route-change"), std::string::npos);
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
    transpiler.setBrowserLocalRuntime(true);
    std::string html = transpiler.transpile(program);
    EXPECT_NE(html.find("data-jtml-route=\"/user/:id\""), std::string::npos);
    EXPECT_NE(html.find("data-jtml-route-params=\"id\""), std::string::npos);
    EXPECT_NE(html.find("data-jtml-expr=\"id\""), std::string::npos);
    EXPECT_NE(html.find("function matchRouteParams"), std::string::npos);
    EXPECT_NE(html.find("clientState[name]"), std::string::npos);
    EXPECT_NE(html.find("clientState['activeRouteName'] = record.name"), std::string::npos);
    EXPECT_NE(html.find("params: Object.assign({}, params)"), std::string::npos);
}

TEST(FriendlySyntax, BrowserLocalManifestCarriesRouteTable) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "let users = fetch \"/api/users\" lazy\n"
        "route \"/\" as Home\n"
        "route \"/user/:id\" as UserProfile load users\n"
        "make Home\n"
        "  page\n"
        "    h1 \"Home\"\n"
        "make UserProfile id\n"
        "  page\n"
        "    show id\n");

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    transpiler.setBrowserLocalRuntime(true);
    std::string html = transpiler.transpile(program);

    const auto manifest = clientManifestFromHtml(html);
    ASSERT_EQ(manifest["routes"].size(), 2u) << manifest.dump(2);
    EXPECT_EQ(manifest["routes"][0]["path"], "/");
    EXPECT_EQ(manifest["routes"][0]["name"], "Home");
    EXPECT_TRUE(manifest["routes"][0]["params"].empty());
    EXPECT_TRUE(manifest["routes"][0]["load"].empty());
    EXPECT_EQ(manifest["routes"][1]["path"], "/user/:id");
    EXPECT_EQ(manifest["routes"][1]["name"], "UserProfile");
    ASSERT_EQ(manifest["routes"][1]["params"].size(), 1u);
    EXPECT_EQ(manifest["routes"][1]["params"][0], "id");
    ASSERT_EQ(manifest["routes"][1]["load"].size(), 1u);
    EXPECT_EQ(manifest["routes"][1]["load"][0], "users");
    ASSERT_EQ(manifest["fetches"].size(), 1u) << manifest.dump(2);
    EXPECT_EQ(manifest["fetches"][0]["name"], "users");
    EXPECT_EQ(manifest["fetches"][0]["url"], "/api/users");
    EXPECT_EQ(manifest["fetches"][0]["method"], "GET");
    EXPECT_TRUE(manifest["fetches"][0]["lazy"]);
    EXPECT_NE(html.find("routeManifest: routeManifest"), std::string::npos) << html;
    EXPECT_NE(html.find("fetchManifest: fetchManifest"), std::string::npos) << html;
    EXPECT_NE(html.find("const manifestRoutes = (window.jtml && Array.isArray(window.jtml.routeManifest))"),
              std::string::npos) << html;
    EXPECT_NE(html.find("const manifestFetches = (window.jtml && Array.isArray(window.jtml.fetchManifest))"),
              std::string::npos) << html;
    EXPECT_NE(html.find("registerFetchBinding(fetch, !!fetch.lazy)"), std::string::npos) << html;
    EXPECT_NE(html.find("if (manifestRoutes.length)"), std::string::npos) << html;
    EXPECT_NE(html.find("params: Array.isArray(manifest.params) ? manifest.params.slice() : []"),
              std::string::npos) << html;
    EXPECT_NE(html.find("collectRouteBindings();"), std::string::npos) << html;
}

TEST(FriendlySyntax, BrowserLocalFetchUrlsInterpolateRouteParamsAtRequestTime) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "let user = fetch \"/api/users/{id}\" lazy stale keep\n"
        "route \"/user/:id\" as UserProfile load user\n"
        "make UserProfile id\n"
        "  page\n"
        "    show user.loading\n"
        "    show user.url\n");

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    transpiler.setBrowserLocalRuntime(true);
    std::string html = transpiler.transpile(program);

    const auto manifest = clientManifestFromHtml(html);
    ASSERT_EQ(manifest["fetches"].size(), 1u) << manifest.dump(2);
    EXPECT_EQ(manifest["fetches"][0]["name"], "user");
    EXPECT_EQ(manifest["fetches"][0]["url"], "/api/users/{id}");
    ASSERT_EQ(manifest["routes"].size(), 1u) << manifest.dump(2);
    EXPECT_EQ(manifest["routes"][0]["path"], "/user/:id");
    EXPECT_EQ(manifest["routes"][0]["name"], "UserProfile");
    ASSERT_EQ(manifest["routes"][0]["params"].size(), 1u);
    EXPECT_EQ(manifest["routes"][0]["params"][0], "id");
    ASSERT_EQ(manifest["routes"][0]["load"].size(), 1u);
    EXPECT_EQ(manifest["routes"][0]["load"][0], "user");
    EXPECT_NE(html.find("function resolveFetchUrl(url)"), std::string::npos) << html;
    EXPECT_NE(html.find("const resolvedUrl = resolveFetchUrl(url);"), std::string::npos) << html;
    EXPECT_NE(html.find("const response = await fetch(resolvedUrl, options);"), std::string::npos) << html;
    EXPECT_NE(html.find("resolveFetchUrl: resolveFetchUrl"), std::string::npos) << html;
    EXPECT_NE(html.find("clientState[name] = Object.prototype.hasOwnProperty.call(params, name) ? params[name] : '';"),
              std::string::npos) << html;
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
    EXPECT_NE(html.find("function registerFetchBinding(record, lazy)"), std::string::npos);
    EXPECT_NE(html.find("}, marker.getAttribute('data-lazy') === 'true');"), std::string::npos);
    EXPECT_NE(html.find("if (!lazy) __jtml_fetch_fns[name](false)"), std::string::npos);
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
        "let mode = \"ready\"\n"
        "let items = [\"Ada\", \"Grace\"]\n"
        "page\n"
        "  if ready\n"
        "    text \"Ready\"\n"
        "  else\n"
        "    text \"Waiting\"\n"
        "  if mode == \"ready\"\n"
        "    text \"Mode ready\"\n"
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
    EXPECT_NE(html.find("data-jtml-cond-expr=\"(mode == &quot;ready&quot;)\""),
              std::string::npos) << html;
    EXPECT_EQ(html.find("data-jtml-cond-expr=\"(mode == ready)\""),
              std::string::npos) << html;

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

TEST(FriendlySyntax, BrowserLocalLoopBodiesUseScopedExpressionsAndAttributes) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "let users = [{ name: \"Ada\", avatar: \"/ada.png\", active: true }]\n"
        "page\n"
        "  for user in users\n"
        "    box class \"person\"\n"
        "      image src user.avatar alt user.name\n"
        "      if user.active\n"
        "        text user.name\n");

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    transpiler.setBrowserLocalRuntime(true);
    std::string html = transpiler.transpile(program);

    EXPECT_NE(html.find("function applyScopedTemplates(root, scope)"), std::string::npos)
        << html;
    EXPECT_NE(html.find("function applyScopedAttributes(root, scope)"), std::string::npos)
        << html;
    EXPECT_NE(html.find("evaluateClientExpression(attr.value, scope)"), std::string::npos)
        << html;
    EXPECT_NE(html.find("renderLoopBody(body, iterator, nestedItem, scope)"), std::string::npos)
        << html;
    EXPECT_NE(html.find("data-jtml-attr-src-expr=&quot;user.avatar&quot;"), std::string::npos)
        << html;
    EXPECT_NE(html.find("data-jtml-attr-alt-expr=&quot;user.name&quot;"), std::string::npos)
        << html;
    EXPECT_NE(html.find("data-jtml-cond-expr=&quot;user.active&quot;"), std::string::npos)
        << html;
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
    EXPECT_NE(html.find("data-jtml-expr=\"users.error\""), std::string::npos);
    EXPECT_NE(html.find("data-jtml-cond-expr=\"users.loading\""), std::string::npos);
    EXPECT_NE(html.find("data-jtml-for-expr=&quot;users.data&quot;"), std::string::npos);
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
    transpiler.setBrowserLocalRuntime(true);
    std::string html = transpiler.transpile(program);
    const auto manifest = clientManifestFromHtml(html);
    ASSERT_EQ(manifest["fetches"].size(), 1u) << manifest.dump(2);
    EXPECT_EQ(manifest["fetches"][0]["name"], "users");
    EXPECT_EQ(manifest["fetches"][0]["url"], "/api/users");
    EXPECT_EQ(manifest["fetches"][0]["method"], "GET");
    EXPECT_EQ(manifest["fetches"][0]["timeoutMs"], "2500");
    EXPECT_EQ(manifest["fetches"][0]["retryCount"], "2");
    EXPECT_EQ(manifest["fetches"][0]["stalePolicy"], "keep");
    EXPECT_EQ(manifest["fetches"][0]["refreshAction"], "reloadUsers");
    EXPECT_NE(html.find("new AbortController()"), std::string::npos);
    EXPECT_NE(html.find("maxRetries"), std::string::npos);
    EXPECT_NE(html.find("stalePolicy === 'keep'"), std::string::npos);
    EXPECT_NE(html.find("function createFetchState(previous, patch)"), std::string::npos);
    EXPECT_NE(html.find("status: 0"), std::string::npos);
    EXPECT_NE(html.find("ok: false"), std::string::npos);
    EXPECT_NE(html.find("hasData: false"), std::string::npos);
    EXPECT_NE(html.find("const hasPreviousData = !!previous.hasData"), std::string::npos);
    EXPECT_NE(html.find("updatedAt: 0"), std::string::npos);
    EXPECT_NE(html.find("status: response.status"), std::string::npos);
    EXPECT_NE(html.find("ok: true"), std::string::npos);
    EXPECT_NE(html.find("hasData: true"), std::string::npos);
    EXPECT_NE(html.find("updatedAt: Date.now()"), std::string::npos);
    EXPECT_NE(html.find("stale: keepStale && hasPreviousData"), std::string::npos);
    EXPECT_NE(html.find("refreshFetch: function (name)"), std::string::npos);
    EXPECT_NE(html.find("Unknown JTML fetch: "), std::string::npos);
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

TEST(FriendlySyntax, InvalidateGroupsAndAllRefreshFetchSets) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "let users = fetch \"/api/users\" group people lazy\n"
        "let teams = fetch \"/api/teams\" group people lazy\n"
        "let audit = fetch \"/api/audit\" group ops lazy\n"
        "when savePeople\n"
        "  invalidate group people\n"
        "when resetEverything\n"
        "  invalidate all\n"
        "page\n"
        "  button \"Save\" click savePeople\n"
        "  button \"Reset\" click resetEverything\n");

    EXPECT_NE(classic.find("data-group=\"people\""), std::string::npos) << classic;
    EXPECT_NE(classic.find("data-jtml-invalidate-action=\"savePeople\""), std::string::npos) << classic;
    EXPECT_NE(classic.find("data-jtml-invalidate-groups=\"people\""), std::string::npos) << classic;
    EXPECT_NE(classic.find("data-jtml-invalidate-action=\"resetEverything\""), std::string::npos) << classic;
    EXPECT_NE(classic.find("data-jtml-invalidate-all=\"true\""), std::string::npos) << classic;

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    transpiler.setBrowserLocalRuntime(true);
    std::string html = transpiler.transpile(program);
    EXPECT_NE(html.find("\"group\":\"people\""), std::string::npos) << html;
    EXPECT_NE(html.find("const __jtml_fetch_groups = {}"), std::string::npos) << html;
    EXPECT_NE(html.find("fetchGroups: __jtml_fetch_groups"), std::string::npos) << html;
    EXPECT_NE(html.find("__jtml_fetch_groups[group]"), std::string::npos) << html;
    EXPECT_NE(html.find("const names = new Set(Array.isArray(spec) ? spec : (spec.fetches || []));"),
              std::string::npos) << html;
    EXPECT_NE(html.find("Object.keys(__jtml_fetch_fns).forEach(function (name) { names.add(name); });"),
              std::string::npos) << html;
}

TEST(FriendlySyntax, FetchCacheKeysDedupeAndTimedRevalidationReachBrowserRuntime) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "let userId = \"42\"\n"
        "let user = fetch \"/api/users/{userId}\" key userId dedupe every 30000 background stale keep group people lazy refresh reloadUser\n"
        "page\n"
        "  button \"Reload\" click reloadUser\n"
        "  text user.data.name\n");

    EXPECT_NE(classic.find("data-cache-key-expr=\"userId\""), std::string::npos) << classic;
    EXPECT_NE(classic.find("data-revalidate-ms=\"30000\""), std::string::npos) << classic;
    EXPECT_NE(classic.find("data-dedupe=\"true\""), std::string::npos) << classic;
    EXPECT_NE(classic.find("data-background=\"true\""), std::string::npos) << classic;

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    transpiler.setBrowserLocalRuntime(true);
    std::string html = transpiler.transpile(program);
    EXPECT_NE(html.find("\"cacheKeyExpr\":\"userId\""), std::string::npos) << html;
    EXPECT_NE(html.find("\"revalidateMs\":\"30000\""), std::string::npos) << html;
    EXPECT_NE(html.find("\"dedupe\":true"), std::string::npos) << html;
    EXPECT_NE(html.find("\"background\":true"), std::string::npos) << html;
    EXPECT_NE(html.find("function resolveFetchKey(name, resolvedUrl, method, bodyExpr, cacheKeyExpr)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("previous.hasData && previous.key === resolvedKey"), std::string::npos) << html;
    EXPECT_NE(html.find("const __jtml_fetch_timers = {}"), std::string::npos) << html;
    EXPECT_NE(html.find("setInterval(function ()"), std::string::npos) << html;
    EXPECT_NE(html.find("document.hidden"), std::string::npos) << html;
    EXPECT_NE(html.find("return __jtml_fetch_fns[name](true);"), std::string::npos) << html;
    EXPECT_NE(html.find("fetchTimers: __jtml_fetch_timers"), std::string::npos) << html;
}

TEST(FriendlySyntax, BrowserRuntimeAssetsExposeOwnedChunks) {
    const auto chunks = jtml::browserRuntimeAssetChunks();
    ASSERT_EQ(chunks.size(), 4u);
    EXPECT_STREQ(chunks[0].name, "prelude");
    EXPECT_STREQ(chunks[1].name, "components");
    EXPECT_STREQ(chunks[2].name, "data-platform");
    EXPECT_STREQ(chunks[3].name, "boot-transport");
    EXPECT_NE(std::string(chunks[0].source).find("<script>"), std::string::npos);
    EXPECT_NE(std::string(chunks[1].source).find("function renderDirectComponent"),
              std::string::npos);
    EXPECT_NE(std::string(chunks[2].source).find("function startFetchBindings"),
              std::string::npos);
    EXPECT_NE(std::string(chunks[3].source).find("window.addEventListener('hashchange'"),
              std::string::npos);
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
    EXPECT_NE(classic.find("@button onClick=auth_logout() type=\"button\"\\\\"), std::string::npos);

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    transpiler.setBrowserLocalRuntime(true);
    std::string html = transpiler.transpile(program);
    EXPECT_NE(html.find("\"state\":{\"auth\":\"{\\\"user\\\": \\\"Ada\\\", \\\"token\\\": \\\"abc\\\"}\""),
              std::string::npos) << html;
    EXPECT_NE(html.find("\"actions\":{\"auth_logout\""),
              std::string::npos) << html;
    EXPECT_NE(html.find("\"lhs\":\"auth.user\""),
              std::string::npos) << html;
    EXPECT_NE(html.find("\"expr\":\"\\\"\\\"\""),
              std::string::npos) << html;
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

TEST(FriendlySyntax, StoreActionWithStringArgumentLowersAndDispatches) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "store filterState\n"
        "  let selectedRegion = \"All\"\n"
        "  when pickRegion region\n"
        "    selectedRegion = region\n"
        "page\n"
        "  text \"Region: {filterState.selectedRegion}\"\n"
        "  button \"Portugal\" click filterState.pickRegion(\"Portugal\")\n");

    EXPECT_NE(classic.find("define filterState = {selectedRegion: \"All\"}\\"),
              std::string::npos) << classic;
    EXPECT_NE(classic.find("function filterState_pickRegion(region)\\\\"),
              std::string::npos) << classic;
    EXPECT_NE(classic.find("filterState.selectedRegion = region\\\\"),
              std::string::npos) << classic;
    EXPECT_NE(classic.find("@button onClick=filterState_pickRegion(\"Portugal\") type=\"button\"\\\\"),
              std::string::npos) << classic;

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    transpiler.setBrowserLocalRuntime(true);
    std::string html = transpiler.transpile(program);
    EXPECT_NE(html.find("\"actions\":{\"filterState_pickRegion\""), std::string::npos)
        << html;
    EXPECT_NE(html.find("\"params\":[\"region\"]"), std::string::npos) << html;
    EXPECT_NE(html.find("\"lhs\":\"filterState.selectedRegion\""), std::string::npos)
        << html;
    EXPECT_NE(html.find("filterState_pickRegion(\\&quot;Portugal\\&quot;)"), std::string::npos)
        << html;

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

    std::string updatedBindings;
    std::string error;
    ASSERT_TRUE(interpreter.dispatchEvent(elementId, "onClick", nlohmann::json::array(),
                                          updatedBindings, error)) << error;
    auto state = nlohmann::json::parse(interpreter.getStateJSON());
    ASSERT_TRUE(state["variables"].contains("filterState")) << state.dump(2);
    EXPECT_EQ(state["variables"]["filterState"]["value"]["selectedRegion"], "Portugal");
}

TEST(FriendlySyntax, StoreBlockQualifiesFieldReadsInsideExpressions) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "store appState\n"
        "  let darkMode = false\n"
        "  let sidebarOpen = true\n"
        "  get themeLabel = \"Theme dark: {darkMode}\"\n"
        "  when toggleTheme\n"
        "    darkMode = !darkMode\n"
        "  when toggleSidebar\n"
        "    sidebarOpen = !sidebarOpen\n"
        "page\n"
        "  text appState.themeLabel\n"
        "  button \"Theme\" click appState.toggleTheme\n");

    EXPECT_NE(classic.find("derive themeLabel = \"Theme dark: \" + appState.darkMode"),
              std::string::npos) << classic;
    EXPECT_NE(classic.find("appState.darkMode = !appState.darkMode\\\\"), std::string::npos)
        << classic;
    EXPECT_NE(classic.find("appState.sidebarOpen = !appState.sidebarOpen\\\\"), std::string::npos)
        << classic;

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;
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
    EXPECT_NE(classic.find("@button onClick=save() type=\"button\"\\\\"), std::string::npos);
    EXPECT_NE(classic.find("function setEmail(value)\\\\"), std::string::npos);
}

TEST(FriendlySyntax, ReadableEventAliasesLowerToClassicEvents) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "when handleKey\n"
        "  show \"key\"\n"
        "when handleDouble\n"
        "  show \"double\"\n"
        "when handleDrop\n"
        "  show \"drop\"\n"
        "page\n"
        "  input \"Search\" key-down handleKey\n"
        "  button \"Open\" double-click handleDouble\n"
        "  dropzone \"Drop\" dragover handleDrop\n");

    EXPECT_NE(classic.find("@input placeholder=\"Search\" onKeyDown=handleKey()\\\\"),
              std::string::npos) << classic;
    EXPECT_NE(classic.find("@button onDblClick=handleDouble() type=\"button\"\\\\"), std::string::npos)
        << classic;
    EXPECT_NE(classic.find("data-jtml-dropzone=\"true\""), std::string::npos) << classic;
    EXPECT_NE(classic.find("onDragOver=handleDrop()"), std::string::npos) << classic;
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
        "  count++\n"
        "  count--\n"
        "page\n"
        "  show count\n");

    EXPECT_NE(classic.find("count += 1\\\\"), std::string::npos);
    EXPECT_NE(classic.find("count = count + 1\\\\"), std::string::npos);
    EXPECT_NE(classic.find("count -= 1\\\\"), std::string::npos);
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

TEST(FriendlySyntax, ComponentCallsValidateDeclaredEmittedEvents) {
    EXPECT_NO_THROW(
        (void)jtml::normalizeSourceSyntax(
            "jtml 2\n"
            "make Child emits picked\n"
            "  button \"Pick\" click picked\n"
            "make Parent\n"
            "  when choose\n"
            "    show \"ok\"\n"
            "  box\n"
            "    Child on picked choose\n"
            "page\n"
            "  Parent\n",
            jtml::SyntaxMode::Friendly));

    EXPECT_THROW(
        (void)jtml::normalizeSourceSyntax(
            "jtml 2\n"
            "make Child emits saved\n"
            "  button \"Pick\" click picked\n"
            "make Parent\n"
            "  when choose\n"
            "    show \"ok\"\n"
            "  box\n"
            "    Child on picked choose\n"
            "page\n"
            "  Parent\n",
            jtml::SyntaxMode::Friendly),
        std::runtime_error);
}

TEST(FriendlySyntax, ComponentCallsValidateEmittedPayloadArity) {
    EXPECT_NO_THROW(
        (void)jtml::normalizeSourceSyntax(
            "jtml 2\n"
            "make Child emits picked(item)\n"
            "  button \"Pick\" click picked(\"Ada\")\n"
            "make Parent\n"
            "  when choose source item\n"
            "    show item\n"
            "  box\n"
            "    Child on picked choose(\"team\")\n"
            "page\n"
            "  Parent\n",
            jtml::SyntaxMode::Friendly));

    EXPECT_THROW(
        (void)jtml::normalizeSourceSyntax(
            "jtml 2\n"
            "make Child emits picked(item)\n"
            "  button \"Pick\" click picked(\"Ada\")\n"
            "make Parent\n"
            "  when choose item\n"
            "    show item\n"
            "  box\n"
            "    Child on picked choose(\"team\")\n"
            "page\n"
            "  Parent\n",
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
        "    polygon points \"240,24 292,112 188,112\" fill \"#f59e0b\" opacity \"0.7\"\n"
        "    svgtext x \"20\" y \"24\" fill \"#334155\" font-size \"14\" \"Revenue\"\n");

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
    EXPECT_NE(classic.find("@text x=\"20\" y=\"24\" fill=\"#334155\" font-size=\"14\"\\\\"),
              std::string::npos);
    EXPECT_NE(classic.find("show \"Revenue\"\\\\"),
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

TEST(FriendlySyntax, MultiSeriesChartsLowerAndRenderAsGroupedStackedAndLine) {
    const std::string classic = normalizeOk(
        "jtml 2\n"
        "let revenue = [{ \"month\": \"Jan\", \"sales\": 12, \"expenses\": 7 }, { \"month\": \"Feb\", \"sales\": 18, \"expenses\": 9 }]\n"
        "page\n"
        "  chart bar data revenue by month values sales expenses series \"Sales,Expenses\" colors \"#0f766e,#b42318\" label \"Revenue split\" stacked legend grid max 40 ticks 5 annotate \"Launch\" at \"Feb\" value sales color \"#9333ea\" export svg png csv\n"
        "  chart line data revenue by month values sales expenses series \"Sales,Expenses\" colors \"#0f766e,#2563eb\" label \"Revenue trend\" legend\n");

    EXPECT_NE(classic.find("data-jtml-chart=\"bar\""), std::string::npos) << classic;
    EXPECT_NE(classic.find("data-jtml-chart-values=\"sales,expenses\""), std::string::npos) << classic;
    EXPECT_NE(classic.find("data-jtml-chart-series=\"Sales,Expenses\""), std::string::npos) << classic;
    EXPECT_NE(classic.find("data-jtml-chart-colors=\"#0f766e,#b42318\""), std::string::npos) << classic;
    EXPECT_NE(classic.find("data-jtml-chart-stacked=\"true\""), std::string::npos) << classic;
    EXPECT_NE(classic.find("data-jtml-chart-max=40"), std::string::npos) << classic;
    EXPECT_NE(classic.find("data-jtml-chart-ticks=5"), std::string::npos) << classic;
    EXPECT_NE(classic.find("data-jtml-chart-annotations=\"Launch|Feb|sales|#9333ea\""), std::string::npos) << classic;
    EXPECT_NE(classic.find("data-jtml-chart-export=\"svg,png,csv\""), std::string::npos) << classic;
    EXPECT_NE(classic.find("data-jtml-chart=\"line\""), std::string::npos) << classic;

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;
    JtmlTranspiler transpiler;
    const std::string html = transpiler.transpile(program);
    EXPECT_NE(html.find("data-jtml-chart-values=\"sales,expenses\""), std::string::npos) << html;
    EXPECT_NE(html.find("const valueFields = splitCsv"), std::string::npos) << html;
    EXPECT_NE(html.find("const series = valueFields.map"), std::string::npos) << html;
    EXPECT_NE(html.find("rowValues[index].forEach"), std::string::npos) << html;
    EXPECT_NE(html.find("series.forEach(function (s, seriesIndex)"), std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-chart-max=\"40.000000000000000\""), std::string::npos) << html;
    EXPECT_NE(html.find("const requestedTicks = Math.max"), std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-chart-annotations=\"Launch|Feb|sales|#9333ea\""), std::string::npos) << html;
    EXPECT_NE(html.find("const annotations = parseAnnotations"), std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-chart-export=\"svg,png,csv\""), std::string::npos) << html;
    EXPECT_NE(html.find("function syncChartExportControls"), std::string::npos) << html;
    EXPECT_NE(html.find("downloadChartBlob"), std::string::npos) << html;
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

TEST(FriendlySyntax, BrowserLocalRuntimeAppliesReactiveAttributes) {
    const std::string classic = normalizeOk(
        "jtml 2\n"
        "let selected = {preview: \"/avatar.png\", name: \"Avatar\"}\n"
        "page\n"
        "  image src selected.preview alt selected.name\n"
        "  link \"Profile\" href selected.preview title selected.name\n");

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    transpiler.setBrowserLocalRuntime(true);
    const std::string html = transpiler.transpile(program);

    EXPECT_NE(html.find("data-jtml-attr-src-expr=\"selected.preview\""), std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-attr-alt-expr=\"selected.name\""), std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-attr-href-expr=\"selected.preview\""), std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-attr-title-expr=\"selected.name\""), std::string::npos) << html;
    EXPECT_NE(html.find("function applyClientAttributes()"), std::string::npos) << html;
    EXPECT_NE(html.find("applyClientAttributes();"), std::string::npos) << html;
    EXPECT_NE(html.find("const browserLocalRuntime = true;"), std::string::npos) << html;
}

TEST(FriendlySyntax, BrowserLocalRuntimeEvaluatesLogicalExpressions) {
    const std::string classic = normalizeOk(
        "jtml 2\n"
        "let ready = true\n"
        "let blocked = false\n"
        "page\n"
        "  if ready && !blocked\n"
        "    text \"Ready\"\n"
        "  else\n"
        "    text \"Blocked\"\n");

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    transpiler.setBrowserLocalRuntime(true);
    const std::string html = transpiler.transpile(program);

    EXPECT_NE(html.find("data-jtml-cond-expr=\"(ready &amp;&amp; !blocked)\""), std::string::npos) << html;
    EXPECT_NE(html.find("splitTopLevelToken(expr, '||')"), std::string::npos) << html;
    EXPECT_NE(html.find("splitTopLevelToken(expr, '&&')"), std::string::npos) << html;
    EXPECT_NE(html.find("value: !part.value"), std::string::npos) << html;
}

TEST(FriendlySyntax, BrowserLocalImageProcessingWritesObservableState) {
    const std::string classic = normalizeOk(
        "jtml 2\n"
        "let photo = {preview: \"/photo.png\", name: \"Photo\"}\n"
        "let thumb = image photo resize 128 128 fit cover\n"
        "page\n"
        "  image src thumb.preview alt \"Thumbnail\"\n");

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    transpiler.setBrowserLocalRuntime(true);
    const std::string html = transpiler.transpile(program);

    EXPECT_NE(html.find("data-jtml-image-proc=\"resize\""), std::string::npos) << html;
    EXPECT_NE(html.find("assignClientPath(into, { preview: preview"), std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-attr-src-expr=\"thumb.preview\""), std::string::npos) << html;
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

TEST(FriendlySyntax, ActionButtonsDefaultToSafeButtonType) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "when save\n"
        "  show \"saved\"\n"
        "when login\n"
        "  show \"login\"\n"
        "page\n"
        "  button \"Save\" click save\n"
        "  button \"Submit\" click login type submit\n");

    EXPECT_NE(classic.find("@button onClick=save() type=\"button\"\\\\"), std::string::npos)
        << classic;
    EXPECT_NE(classic.find("@button onClick=login() type=submit\\\\"), std::string::npos)
        << classic;
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
    transpiler.setBrowserLocalRuntime(true);
    std::string html = transpiler.transpile(program);
    EXPECT_NE(html.find("\"componentDefinitions\":[{"), std::string::npos) << html;
    EXPECT_NE(html.find("\"name\":\"Badge\""), std::string::npos) << html;
    EXPECT_NE(html.find("\"params\":[\"label\"]"),
              std::string::npos) << html;
    EXPECT_NE(html.find("\"bodyPlan\":[{"), std::string::npos) << html;
    EXPECT_NE(html.find("function renderDirectComponent(instance)"), std::string::npos) << html;
    EXPECT_NE(html.find("directComponentExecution: true"), std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-direct-component-action"), std::string::npos) << html;
    EXPECT_NE(html.find("\"kind\":\"template\""), std::string::npos) << html;
    EXPECT_NE(html.find("\"name\":\"text\""), std::string::npos) << html;
    EXPECT_NE(html.find("\"parentIndex\":-1"), std::string::npos) << html;
    EXPECT_NE(html.find("\"renderRoot\":true"), std::string::npos) << html;
    EXPECT_NE(html.find("\"componentInstances\":[{"), std::string::npos) << html;
    EXPECT_NE(html.find("\"id\":\"Badge_"), std::string::npos) << html;
    EXPECT_NE(html.find("\"component\":\"Badge\""), std::string::npos) << html;
    EXPECT_NE(html.find("\"params\":{\"label\":\"Gold\"}"), std::string::npos) << html;
    EXPECT_NE(html.find("const componentDefinitionManifest = Array.isArray(manifest.componentDefinitions)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("const manifestInstances = (window.jtml && Array.isArray(window.jtml.componentInstanceManifest))"),
              std::string::npos) << html;
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

TEST(FriendlySyntax, DirectComponentBodyPlanRendererCoversConditionalsAndLoops) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make ListCard title\n"
        "  let visible = true\n"
        "  let items = [\"A\", \"B\"]\n"
        "  let people = { \"ada\": \"Ada\", \"grace\": \"Grace\" }\n"
        "  card\n"
        "    h2 title\n"
        "    if visible\n"
        "      for item in items key item\n"
        "        text item\n"
        "      for person in people\n"
        "        text person\n"
        "page\n"
        "  ListCard \"Names\"\n");

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    transpiler.setBrowserLocalRuntime(true);
    std::string html = transpiler.transpile(program);
    EXPECT_NE(html.find("if (head === 'if')"), std::string::npos) << html;
    EXPECT_NE(html.find("if (head === 'for')"), std::string::npos) << html;
    EXPECT_NE(html.find("const dynamicComponentRenderStack = []"), std::string::npos) << html;
    EXPECT_NE(html.find("function pruneDynamicComponentSubtree(parentId, activeIds)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("pruneDynamicComponentSubtree(instance.id, renderedDynamicIds)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("\"text\":\"if visible\""), std::string::npos) << html;
    EXPECT_NE(html.find("\"expressionPlan\":{\"kind\":\"path\",\"root\":\"visible\""),
              std::string::npos) << html;
    EXPECT_NE(html.find("\"text\":\"for item in items key item\""), std::string::npos) << html;
    EXPECT_NE(html.find("\"loopPlan\":{\"collectionExpression\":\"items\""),
              std::string::npos) << html;
    EXPECT_NE(html.find("\"collectionPlan\":{\"kind\":\"path\",\"root\":\"items\""),
              std::string::npos) << html;
    EXPECT_NE(html.find("\"keyExpression\":\"item\""), std::string::npos) << html;
    EXPECT_NE(html.find("\"keyExpressionPlan\":{\"kind\":\"path\",\"root\":\"item\""),
              std::string::npos) << html;
    EXPECT_NE(html.find("\"text\":\"for person in people\""), std::string::npos) << html;
    EXPECT_NE(html.find("evaluateCompiledComponentExpressionResult("),
              std::string::npos) << html;
    EXPECT_NE(html.find("else if (typeof values === 'object') values = Object.values(values);"),
              std::string::npos) << html;
}

TEST(FriendlySyntax, DirectComponentBodyPlanRendererHandlesElseAsControlFlow) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make Notice\n"
        "  let ready = false\n"
        "  card\n"
        "    if ready\n"
        "      text \"Ready\"\n"
        "    else\n"
        "      text \"Waiting\"\n"
        "page\n"
        "  Notice\n");

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    transpiler.setBrowserLocalRuntime(true);
    std::string html = transpiler.transpile(program);
    EXPECT_NE(html.find("function componentElseSibling(definition, node)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("if (head === 'else') return '';"),
              std::string::npos) << html;
    EXPECT_NE(html.find("html = elseNode ? renderComponentChildren(definition, instance, elseNode, scope) : '';"),
              std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-direct-region=\"if\""),
              std::string::npos) << html;
    EXPECT_NE(html.find("\"text\":\"else\""), std::string::npos) << html;
    EXPECT_NE(html.find("\"text\":\"text \\\"Waiting\\\"\""), std::string::npos) << html;
}

TEST(FriendlySyntax, DirectComponentSlotsNestedCallsAttributesAndActionArgsUseBodyPlan) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make Frame title\n"
        "  card class \"frame\" style \"padding: 12px\" aria-label title\n"
        "    h2 title class \"heading\"\n"
        "    slot\n"
        "make Picker\n"
        "  let selected = \"\"\n"
        "  when pick value\n"
        "    selected = value\n"
        "  Frame \"Chooser\"\n"
        "    text selected\n"
        "    button \"Ada\" click pick(\"Ada Lovelace\") class \"primary\"\n"
        "page\n"
        "  Picker\n");

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    transpiler.setBrowserLocalRuntime(true);
    std::string html = transpiler.transpile(program);
    const auto manifest = clientManifestFromHtml(html);
    ASSERT_FALSE(manifest["componentInstances"].empty()) << manifest.dump(2);
    EXPECT_NE(manifest["componentInstances"].dump().find("\"slotPlan\""), std::string::npos)
        << manifest.dump(2);
    EXPECT_EQ(manifest.dump().find("slotHex"), std::string::npos) << manifest.dump(2);
    EXPECT_EQ(manifest.dump().find("slotSource"), std::string::npos) << manifest.dump(2);
    EXPECT_NE(html.find("function renderComponentSlot(instance, scope, name)"), std::string::npos) << html;
    EXPECT_NE(html.find("function renderNestedComponentCall(parentDefinition, parentInstance, node, scope, name, directNodeAttr)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("node && node.definitionModule"), std::string::npos) << html;
    EXPECT_NE(html.find("const dynamicComponentInstances = {}"), std::string::npos) << html;
    EXPECT_NE(html.find("function registerDynamicComponentInstance(instance)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-direct-instance"), std::string::npos) << html;
    EXPECT_NE(html.find("function parseComponentActionInvocation(raw, scope)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function compileComponentActionInvocation(raw)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("const argExpressions = argSource ? splitTopLevelList(argSource) : []"),
              std::string::npos) << html;
    EXPECT_NE(html.find("argPlans: argExpressions.map(compileComponentExpressionPlan)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function evaluateCompiledComponentActionInvocation(compiled, scope)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function compileComponentExpressionPlan(expr)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function stripOuterComponentExpressionParens(expr)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function findTopLevelComponentOperator(expr, operators)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("kind: 'conditional'"),
              std::string::npos) << html;
    EXPECT_NE(html.find("kind: 'binary'"),
              std::string::npos) << html;
    EXPECT_NE(html.find("if (plan.kind === 'binary')"),
              std::string::npos) << html;
    EXPECT_NE(html.find("if (plan.kind === 'conditional')"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function renderComponentAttributes(parts, extra)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-direct-component-args"), std::string::npos) << html;
    EXPECT_NE(html.find("\"params\":[\"title\"]"), std::string::npos) << html;
    EXPECT_NE(html.find("\"params\":[]"), std::string::npos) << html;
}

TEST(FriendlySyntax, DirectNestedComponentActionsUseNestedRuntimeIdentity) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make Inner\n"
        "  let count = 0\n"
        "  when add\n"
        "    count += 1\n"
        "  card class \"inner\"\n"
        "    text count\n"
        "    button \"+\" click add\n"
        "make Outer\n"
        "  box class \"outer\"\n"
        "    Inner\n"
        "page\n"
        "  Outer\n");

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    transpiler.setBrowserLocalRuntime(true);
    std::string html = transpiler.transpile(program);
    EXPECT_NE(html.find("function findRuntimeComponentInstance(id)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("registerDynamicComponentInstance(nestedInstance);"),
              std::string::npos) << html;
    EXPECT_NE(html.find("const instance = findRuntimeComponentInstance(componentId);"),
              std::string::npos) << html;
    EXPECT_NE(html.find("if (!instance.element) instance.element = componentElementFor(instance.id);"),
              std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-nested-component=\"true\""), std::string::npos) << html;
    EXPECT_NE(html.find("\"name\":\"Inner\""), std::string::npos) << html;
    EXPECT_NE(html.find("\"name\":\"Outer\""), std::string::npos) << html;
}

TEST(FriendlySyntax, DirectComponentRendererPreservesSemanticUiModifiers) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make UsagePanel\n"
        "  panel title \"Usage\" pad lg shadow md width wide surface raised\n"
        "    grid cols 2 gap md\n"
        "      metric \"Users\" 42 \"Active\" tone good\n"
        "      card tone primary\n"
        "        text \"Ready\"\n"
        "page\n"
        "  UsagePanel\n");

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    transpiler.setBrowserLocalRuntime(true);
    std::string html = transpiler.transpile(program);
    EXPECT_NE(html.find("function semanticUiRenderExtras(head, parts)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("isSemanticUiModifierName(token)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-ui-' + modifier.name"),
              std::string::npos) << html;
    EXPECT_NE(html.find("jtml-' + modifier.name + suffix"),
              std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-ui"), std::string::npos) << html;
}

TEST(FriendlySyntax, LiveRuntimeScriptUsesBodyPlanHashesAndFailsClosed) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make Card\n"
        "  card tone good\n"
        "    text \"Ready\"\n"
        "page\n"
        "  Card\n");

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    std::string html = transpiler.transpile(program);
    EXPECT_NE(html.find("function applyLiveBodyPlanRender(payload)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("liveBodyPlanLastPatch"),
              std::string::npos) << html;
    EXPECT_NE(html.find("mode: 'live-body-plan-render'"),
              std::string::npos) << html;
    EXPECT_NE(html.find("unsupportedIds: unsupportedIds"),
              std::string::npos) << html;
    EXPECT_NE(html.find("missingElementIds: missingElementIds"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function jtmlStableHash(value)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("new TextEncoder().encode(value)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function hasLiveBodyPlanServerRender()"),
              std::string::npos) << html;
    EXPECT_NE(html.find("if (browserLocalRuntime || !payload) return;"),
              std::string::npos) << html;
    EXPECT_NE(html.find("const res = await fetch('/api/rendered-components');"),
              std::string::npos) << html;
    EXPECT_NE(html.find("el.dataset.jtmlLiveBodyPlanRenderedHash === htmlHash"),
              std::string::npos) << html;
    EXPECT_NE(html.find("el.dataset.jtmlLiveBodyPlanTransport = 'body-plan';"),
              std::string::npos) << html;
    EXPECT_NE(html.find("refreshLiveBodyPlanRender();"),
              std::string::npos) << html;
    EXPECT_NE(html.find("hasLiveBodyPlanServerRender()) return;"),
              std::string::npos) << html;
    EXPECT_NE(html.find("if (data && data.renderedComponents) applyLiveBodyPlanRender(data.renderedComponents);"),
              std::string::npos) << html;
    EXPECT_NE(html.find("component.renderedHtmlSupported === true"),
              std::string::npos) << html;
    EXPECT_NE(html.find("runtime.renderedHtmlSupported === true"),
              std::string::npos) << html;
    EXPECT_EQ(html.find("el.dataset.jtmlLiveBodyPlanTransport === 'initial'"),
              std::string::npos) << html;
    EXPECT_EQ(html.find("el.dataset.jtmlLiveBodyPlanRendered === 'initial'"),
              std::string::npos) << html;
    EXPECT_NE(html.find("async function runLiveComponentAction(componentId, actionName, args)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("fetch('/api/component-action'"),
              std::string::npos) << html;
    EXPECT_NE(html.find("await runLiveComponentAction(componentId, actionName, args)"),
              std::string::npos) << html;
}

TEST(FriendlySyntax, DirectComponentActionsSupportGuardsLoopsAndPlusEqualsSemantics) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make Mixed\n"
        "  let count = 1\n"
        "  let remainder = 10\n"
        "  let label = \"A\"\n"
        "  when add\n"
        "    count += 2\n"
        "    remainder %= 4\n"
        "    label += \"B\"\n"
        "  when localLet\n"
        "    let next = count + 5\n"
        "    get replacement = \"local\"\n"
        "    count = next\n"
        "    label = replacement\n"
        "  when loopUp\n"
        "    while count < 4\n"
        "      count += 1\n"
        "  when guarded\n"
        "    if count\n"
        "      label = \"unsafe\"\n"
        "  box\n"
        "    text label\n"
        "    button \"Add\" click add\n"
        "    button \"Local let\" click localLet\n"
        "    button \"Loop\" click loopUp\n"
        "    button \"Guard\" click guarded\n"
        "page\n"
        "  Mixed\n");

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    transpiler.setBrowserLocalRuntime(true);
    std::string html = transpiler.transpile(program);
    EXPECT_NE(html.find("function applyComponentAssignment(scope, node)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function evaluateComponentBodyNodeExpression(node, fallbackExpression, scope)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("node && node.expressionPlan || compileComponentExpressionPlan(expr)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("const next = evaluateComponentBodyNodeExpression(node, node.expression || '', scope);"),
              std::string::npos) << html;
    EXPECT_NE(html.find("const targetParts = parseClientPathSegments(node.name, scope);"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function applyComponentStateDeclaration(scope, node)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("scope[node.name] = evaluateComponentBodyNodeExpression(node, node.expression || '', scope);"),
              std::string::npos) << html;
    EXPECT_NE(html.find("const plan = compileComponentExpressionPlan(expr);"),
              std::string::npos) << html;
    EXPECT_NE(html.find("args.push(compiled && compiled.found ? compiled.value : evaluateComponentValue(expr, scope));"),
              std::string::npos) << html;
    EXPECT_NE(html.find("node.kind === 'state' || node.kind === 'derived'"),
              std::string::npos) << html;
    EXPECT_NE(html.find("typeof current === 'number' && typeof next === 'number'"),
              std::string::npos) << html;
    EXPECT_NE(html.find("String(current == null ? '' : current) + String(next == null ? '' : next)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("op === '%='"),
              std::string::npos) << html;
    EXPECT_NE(html.find("\"reads\":[\"count\"]"),
              std::string::npos) << html;
    EXPECT_NE(html.find("\"writes\":[\"count\"]"),
              std::string::npos) << html;
    EXPECT_NE(html.find("\"writes\":[\"label\"]"),
              std::string::npos) << html;
    EXPECT_NE(html.find("node.kind === 'template' && head === 'if'"),
              std::string::npos) << html;
    EXPECT_NE(html.find("node.kind === 'template' && head === 'for'"),
              std::string::npos) << html;
    EXPECT_NE(html.find("node.kind === 'template' && head === 'while'"),
              std::string::npos) << html;
    EXPECT_NE(html.find("const loopPlan = node && node.loopPlan || {}"),
              std::string::npos) << html;
    EXPECT_NE(html.find("loopPlan.collectionPlan || compileComponentExpressionPlan(collectionExpr)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("scope[loopItemName] = values[itemIndex];"),
              std::string::npos) << html;
    EXPECT_NE(html.find("runComponentPlanStatements(definition, scope, componentNodeChildren(definition, elseNode), instance, changes, actionContext)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("return false;"),
              std::string::npos) << html;
    EXPECT_NE(html.find("runDirectComponentFallbackAction"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function recordComponentBodyPlanFallback(instance, definition, node, reason, actionContext)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("directComponentFallbacks: componentBodyPlanFallbacks"),
              std::string::npos) << html;
    EXPECT_NE(html.find("action: actionContext && actionContext.name || ''"),
              std::string::npos) << html;
    EXPECT_NE(html.find("actionSourceLine: actionContext && actionContext.sourceLine || 0"),
              std::string::npos) << html;
    EXPECT_NE(html.find("actionText: actionContext && actionContext.text || ''"),
              std::string::npos) << html;
    EXPECT_NE(html.find("bodySourceLine: node && node.sourceLine || 0"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function componentRenderedReadSet(definition)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function componentWritesAffectRender(definition, writes)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function componentNodeReadSet(definition, node)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function patchDirectComponentFromWrites(instance, definition, writes)"),
              std::string::npos) << html;
    EXPECT_EQ(html.find("function patchDirectComponentNodeInPlace(instance, definition, node, scope)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function patchDirectComponentElementAttributes(el, parts, extra)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function componentAttributeMap(parts, extra)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function componentPatchOperationKind(definition, node)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function compileComponentElementParts(words, start, stopWords)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function evaluateCompiledComponentElementParts(plan, scope)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function compileComponentActionInvocation(raw)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function evaluateCompiledComponentActionInvocation(compiled, scope)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("const componentUpdatePlanCache = {}"),
              std::string::npos) << html;
    EXPECT_NE(html.find("jtml:static-update-plans-ready"),
              std::string::npos) << html;
    EXPECT_NE(html.find("jtml:static-component-modules-ready"),
              std::string::npos) << html;
    EXPECT_NE(html.find("staticUpdatePlansLoaded: true"),
              std::string::npos) << html;
    EXPECT_NE(html.find("staticComponentPlanIndexLoaded: true"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function compileComponentUpdatePlan(definition)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function staticComponentUpdatePlanForKey(key)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("const asset = window.__jtml_static_component_plan_index || window.__jtml_static_update_plans"),
              std::string::npos) << html;
    EXPECT_NE(html.find("const staticPlan = staticComponentUpdatePlanForKey(key);"),
              std::string::npos) << html;
    EXPECT_NE(html.find("static-update-plan-interpreter"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function compileComponentPatchOperation(definition, node, index)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function executeComponentPatchOperation(instance, definition, operation, scope)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function generateComponentUpdateFunctionSource(plan)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function compileGeneratedComponentUpdateFunction(plan)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function compileStaticComponentUpdateFunction(plan)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function compileInterpretedComponentUpdateFunction(plan)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("staticUpdateFunctionsLoaded: !!window.__jtml_static_update_functions"),
              std::string::npos) << html;
    EXPECT_NE(html.find("staticComponentModulesLoaded: !!window.__jtml_static_component_modules"),
              std::string::npos) << html;
    EXPECT_NE(html.find("staticComponentPlanIndex: window.__jtml_static_component_plan_index || null"),
              std::string::npos) << html;
    EXPECT_NE(html.find("staticComponentPlanIndexAsset: !!window.__jtml_static_component_plan_index"),
              std::string::npos) << html;
    EXPECT_NE(html.find("staticComponentPlanIndexLoaded: !!window.__jtml_static_component_plan_index"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function renderStaticComponentRoots(instance, definition, scope)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("static-production-create-function"),
              std::string::npos) << html;
    EXPECT_NE(html.find("plan.generatedSource = generateComponentUpdateFunctionSource(plan)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("plan.update = compileGeneratedComponentUpdateFunction(plan) ||"),
              std::string::npos) << html;
    EXPECT_NE(html.find("const dynamicGeneratedUpdateFunctions = false;"),
              std::string::npos) << html;
    EXPECT_NE(html.find("dynamic generated update functions disabled; using CSP-safe interpreted update plan"),
              std::string::npos) << html;
    EXPECT_NE(html.find("cspSafeUpdatePlans: !dynamicGeneratedUpdateFunctions"),
              std::string::npos) << html;
    EXPECT_NE(html.find("const factory = new Function('h', plan.generatedSource)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("generated-production-update-function"),
              std::string::npos) << html;
    EXPECT_NE(html.find("const entriesByRead = {}"),
              std::string::npos) << html;
    EXPECT_NE(html.find("const unsafeEntries = []"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function addComponentUpdateIndex(index, entry)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function componentPlanAffectedEntries(plan, changed)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function firstUnsafeAffectedRenderNodeFromPlan(plan, definition, changed)"),
              std::string::npos) << html;
    EXPECT_EQ(html.find("function firstUnsafeAffectedRenderNode(definition, changed)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function recordCompiledPatchFallback(instance, definition, node, reason, changed)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("directComponentPatchFallback"),
              std::string::npos) << html;
    EXPECT_NE(html.find("operation: operation"),
              std::string::npos) << html;
    EXPECT_NE(html.find("partsPlan = compileComponentElementParts(words, 1, { click: true })"),
              std::string::npos) << html;
    EXPECT_NE(html.find("base.clickInvocation = compileComponentActionInvocation(base.clickRaw)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("base.operation = 'container-attrs'"),
              std::string::npos) << html;
    EXPECT_NE(html.find("base.operation = 'nested-component'"),
              std::string::npos) << html;
    EXPECT_NE(html.find("if (operation.operation === 'container-attrs')"),
              std::string::npos) << html;
    EXPECT_NE(html.find("if (operation.operation === 'nested-component')"),
              std::string::npos) << html;
    EXPECT_NE(html.find("return '<span' + directNodeAttr +"),
              std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-direct-region=\"if\""),
              std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-direct-region=\"for\""),
              std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-direct-region=\"slot\""),
              std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-direct-list-key"),
              std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-direct-list-index"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function recordDirectListLifecycle(instance, definition, node, nodeIndex, keys)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function renderComponentForItems(definition, instance, node, scope, words)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function patchComponentForRegion(instance, definition, node, scope, el)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function patchKeyedListItemWrapperInPlace(wrapper, item)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function patchElementFromTemplateInPlace(current, next, stats)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function pruneDynamicComponentListItem(parentId, nodeIndex, key)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("directComponentKeyedListPatch"),
              std::string::npos) << html;
    EXPECT_NE(html.find("mode: 'keyed-for-region-patch'"),
              std::string::npos) << html;
    EXPECT_NE(html.find("patchComponentForRegion(instance, definition, node, scope, el)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("keyed for-region patch requires unique next keys"),
              std::string::npos) << html;
    EXPECT_NE(html.find("directComponentListLifecycle"),
              std::string::npos) << html;
    EXPECT_NE(html.find("mode: 'keyed-list-markers'"),
              std::string::npos) << html;
    EXPECT_NE(html.find("insertedKeys: inserted"),
              std::string::npos) << html;
    EXPECT_NE(html.find("removedKeys: removed"),
              std::string::npos) << html;
    EXPECT_NE(html.find("movedKeys: moved"),
              std::string::npos) << html;
    EXPECT_NE(html.find("previousKeys: previousKeys"),
              std::string::npos) << html;
    EXPECT_NE(html.find("retainedKeys: retainedKeys"),
              std::string::npos) << html;
    EXPECT_NE(html.find("prunedDynamicInstances: prunedDynamicInstances"),
              std::string::npos) << html;
    EXPECT_NE(html.find("itemElementPatches: itemElementPatches"),
              std::string::npos) << html;
    EXPECT_NE(html.find("itemTextPatches: itemTextPatches"),
              std::string::npos) << html;
    EXPECT_NE(html.find("reconciliation: 'below-wrapper'"),
              std::string::npos) << html;
    EXPECT_NE(html.find("reordered: movedKeys.length > 0"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function renderComponentForRegion(definition, instance, node, scope, words)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("if (head === 'if' || head === 'for' || head === 'slot') return 'region'"),
              std::string::npos) << html;
    EXPECT_NE(html.find("base.operation = 'region'"),
              std::string::npos) << html;
    EXPECT_NE(html.find("if (operation.operation === 'region')"),
              std::string::npos) << html;
    EXPECT_NE(html.find("operationCount: plan && plan.__lastOperationCount || 0"),
              std::string::npos) << html;
    EXPECT_NE(html.find("indexedReadCount: Object.keys(plan && plan.entriesByRead || {}).length"),
              std::string::npos) << html;
    EXPECT_NE(html.find("unsafeEntryCount: (plan && plan.unsafeEntries || []).length"),
              std::string::npos) << html;
    EXPECT_NE(html.find("generatedSourceLength: String(plan && plan.generatedSource || '').length"),
              std::string::npos) << html;
    EXPECT_NE(html.find("'indexed-compiled-update-function'"),
              std::string::npos) << html;
    EXPECT_NE(html.find("node.keyExpression || ''"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function applyCompiledComponentUpdatePlan(instance, definition, changed, scope)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("directComponentCompiledUpdatePlan"),
              std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-direct-body-node"),
              std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-direct-managed-attrs"),
              std::string::npos) << html;
    EXPECT_NE(html.find("directComponentPatchCount"),
              std::string::npos) << html;
    EXPECT_NE(html.find("function noteComponentActionResult(instance, definition, actionName, writes, renderSkipped)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("directComponentLastAction"),
              std::string::npos) << html;
    EXPECT_NE(html.find("const actionContext = {"),
              std::string::npos) << html;
    EXPECT_NE(html.find("name: actionNode.name || actionName || ''"),
              std::string::npos) << html;
    EXPECT_NE(html.find("const shouldRender = componentWritesAffectRender(definition, Object.keys(changes.writes || {}));"),
              std::string::npos) << html;
    EXPECT_NE(html.find("!patchDirectComponentFromWrites(instance, definition, changes.writes)"),
              std::string::npos) << html;
    EXPECT_EQ(html.find("patchDirectComponentNodeInPlace(instance, definition, node, scope)"),
              std::string::npos) << html;
    EXPECT_NE(html.find("compiled body-plan update unavailable; full direct rerender required"),
              std::string::npos) << html;
    EXPECT_NE(html.find("renderDirectComponent(instance);"),
              std::string::npos) << html;

    JtmlTranspiler liveTranspiler;
    InterpreterConfig config;
    config.startWebSocket = false;
    Interpreter interpreter(liveTranspiler, config);
    interpreter.interpret(program);
    auto state = nlohmann::json::parse(interpreter.getStateJSON());
    std::string componentId;
    for (const auto& component : state["components"]) {
        if (component.value("component", "") == "Mixed") {
            componentId = component.value("id", "");
        }
    }
    ASSERT_FALSE(componentId.empty()) << state.dump(2);
    std::string bindings;
    std::string error;
    ASSERT_TRUE(interpreter.dispatchComponentAction(
        componentId,
        "add",
        nlohmann::json::array(),
        bindings,
        error)) << error;
    state = nlohmann::json::parse(interpreter.getStateJSON());
    bool foundMixed = false;
    for (const auto& component : state["components"]) {
        if (component.value("id", "") != componentId) continue;
        foundMixed = true;
        EXPECT_EQ(component["locals"]["count"]["value"], 3);
        EXPECT_EQ(component["locals"]["remainder"]["value"], 2);
        EXPECT_EQ(component["locals"]["label"]["value"], "AB");
    }
    EXPECT_TRUE(foundMixed) << state.dump(2);

    ASSERT_TRUE(interpreter.dispatchComponentAction(
        componentId,
        "loopUp",
        nlohmann::json::array(),
        bindings,
        error)) << error;
    state = nlohmann::json::parse(interpreter.getStateJSON());
    foundMixed = false;
    for (const auto& component : state["components"]) {
        if (component.value("id", "") != componentId) continue;
        foundMixed = true;
        EXPECT_EQ(component["locals"]["count"]["value"], 4);
    }
    EXPECT_TRUE(foundMixed) << state.dump(2);
}

TEST(FriendlySyntax, DirectComponentActionsCanCallOtherLocalActions) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make Counter\n"
        "  let count = 0\n"
        "  let amount = 100\n"
        "  when incBy amount\n"
        "    count += amount\n"
        "  when addTwice\n"
        "    incBy(2)\n"
        "    incBy(3)\n"
        "  card\n"
        "    text count\n"
        "    button \"Add twice\" click addTwice\n"
        "page\n"
        "  Counter\n");

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler browserTranspiler;
    browserTranspiler.setBrowserLocalRuntime(true);
    const std::string browserHtml = browserTranspiler.transpile(program);
    EXPECT_NE(browserHtml.find("function componentActionArgsFromExpression(expression, scope)"),
              std::string::npos) << browserHtml;
    EXPECT_NE(browserHtml.find("node.kind === 'call' && node.name"),
              std::string::npos) << browserHtml;
    EXPECT_NE(browserHtml.find("emitComponentEvent(instance, node.name, callArgs)"),
              std::string::npos) << browserHtml;
    EXPECT_NE(browserHtml.find("hadPrevious[param] = Object.prototype.hasOwnProperty.call(scope, param);"),
              std::string::npos) << browserHtml;
    EXPECT_NE(browserHtml.find("if (hadPrevious[param]) scope[param] = previous[param];"),
              std::string::npos) << browserHtml;
    EXPECT_NE(browserHtml.find("else delete scope[param];"),
              std::string::npos) << browserHtml;
    const auto manifest = clientManifestFromHtml(browserHtml);
    bool foundCallNode = false;
    for (const auto& definition : manifest["componentDefinitions"]) {
        if (definition.value("name", "") != "Counter") continue;
        for (const auto& node : definition["bodyPlan"]) {
            if (node.value("kind", "") == "call" && node.value("name", "") == "incBy") {
                foundCallNode = true;
            }
        }
    }
    EXPECT_TRUE(foundCallNode) << manifest.dump(2);

    JtmlTranspiler liveTranspiler;
    InterpreterConfig config;
    config.startWebSocket = false;
    Interpreter interpreter(liveTranspiler, config);
    interpreter.interpret(program);

    auto state = nlohmann::json::parse(interpreter.getStateJSON());
    std::string componentId;
    for (const auto& component : state["components"]) {
        if (component.value("component", "") == "Counter") {
            componentId = component.value("id", "");
            break;
        }
    }
    ASSERT_FALSE(componentId.empty()) << state.dump(2);

    std::string bindings;
    std::string error;
    ASSERT_TRUE(interpreter.dispatchComponentAction(
        componentId,
        "addTwice",
        nlohmann::json::array(),
        bindings,
        error)) << error;

    state = nlohmann::json::parse(interpreter.getStateJSON());
    bool foundCounter = false;
    for (const auto& component : state["components"]) {
        if (component.value("id", "") != componentId) continue;
        foundCounter = true;
        EXPECT_EQ(component["locals"]["count"]["value"], 5) << component.dump(2);
        EXPECT_EQ(component["locals"]["amount"]["value"], 100) << component.dump(2);
        EXPECT_NE(component.value("renderedHtml", "").find(">5</p>"), std::string::npos)
            << component.dump(2);
    }
    EXPECT_TRUE(foundCounter) << state.dump(2);
}

TEST(FriendlySyntax, DynamicGeneratedUpdateFunctionsAreExplicitlyOptIn) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make Counter\n"
        "  let count = 0\n"
        "  when add\n"
        "    count += 1\n"
        "  text count\n"
        "page\n"
        "  Counter\n");

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler secureTranspiler;
    secureTranspiler.setBrowserLocalRuntime(true);
    const std::string secureHtml = secureTranspiler.transpile(program);
    EXPECT_NE(secureHtml.find("const dynamicGeneratedUpdateFunctions = false;"),
              std::string::npos) << secureHtml;

    JtmlTranspiler devTranspiler;
    devTranspiler.setBrowserLocalRuntime(true);
    devTranspiler.setDynamicGeneratedUpdateFunctions(true);
    const std::string devHtml = devTranspiler.transpile(program);
    EXPECT_NE(devHtml.find("const dynamicGeneratedUpdateFunctions = true;"),
              std::string::npos) << devHtml;
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

TEST(FriendlySyntax, ComponentLocalUnaryExpressionsAreIsolated) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make Toggle\n"
        "  let open = true\n"
        "  when flip\n"
        "    open = !open\n"
        "  button \"Flip\" click flip\n"
        "page\n"
        "  Toggle\n");

    EXPECT_NE(classic.find("__Toggle_1_open = !__Toggle_1_open"), std::string::npos)
        << classic;
    EXPECT_EQ(classic.find("open = (! open)"), std::string::npos) << classic;
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
    EXPECT_EQ(state["componentDefinitions"][0]["runtimePlan"]["mode"], "semantic-instance");
    EXPECT_EQ(state["componentDefinitions"][0]["runtimePlan"]["actions"][0], "add");
    EXPECT_EQ(state["componentDefinitions"][0]["runtimePlan"]["state"][0], "count");
    ASSERT_GE(state["componentDefinitions"][0]["bodyPlan"].size(), 4);
    EXPECT_EQ(state["componentDefinitions"][0]["bodyPlan"][0]["kind"], "state");
    EXPECT_EQ(state["componentDefinitions"][0]["bodyPlan"][0]["parentIndex"], -1);
    EXPECT_EQ(state["componentDefinitions"][0]["bodyPlan"][1]["kind"], "action");
    EXPECT_EQ(state["componentDefinitions"][0]["bodyPlan"][1]["parentIndex"], -1);
    ASSERT_EQ(state["componentDefinitions"][0]["bodyPlan"][1]["childIndices"].size(), 1);
    EXPECT_EQ(state["componentDefinitions"][0]["bodyPlan"][1]["childIndices"][0], 2);
    EXPECT_EQ(state["componentDefinitions"][0]["bodyPlan"][2]["kind"], "assignment");
    EXPECT_EQ(state["componentDefinitions"][0]["bodyPlan"][2]["parentIndex"], 1);
    EXPECT_EQ(state["componentDefinitions"][0]["bodyPlan"][2]["name"], "count");
    EXPECT_EQ(state["componentDefinitions"][0]["bodyPlan"][2]["operator"], "+=");
    EXPECT_EQ(state["componentDefinitions"][0]["bodyPlan"][2]["expression"], "1");
    EXPECT_EQ(state["componentDefinitions"][0]["bodyPlan"][3]["kind"], "template");
    EXPECT_EQ(state["componentDefinitions"][0]["bodyPlan"][3]["parentIndex"], -1);
    EXPECT_EQ(state["componentDefinitions"][0]["bodyPlan"][3]["renderRoot"], true);
    EXPECT_EQ(state["componentDefinitions"][0]["bodyPlan"][3]["expression"], "");
    ASSERT_GE(state["componentDefinitions"][0]["runtimePlan"]["bodyPlan"].size(), 5);
    EXPECT_EQ(state["componentDefinitions"][0]["runtimePlan"]["bodyPlan"][4]["text"], "show count");
    EXPECT_EQ(state["componentDefinitions"][0]["runtimePlan"]["bodyPlan"][4]["parentIndex"], 3);
    EXPECT_EQ(state["componentDefinitions"][0]["rootTemplateNodeCount"], 1);
    EXPECT_NE(state["componentDefinitions"][0]["body"].get<std::string>().find("let count = 0"),
              std::string::npos);
    ASSERT_TRUE(state.contains("components")) << state.dump(2);
    ASSERT_EQ(state["components"].size(), 2);
    EXPECT_EQ(state["components"][0]["component"], "Counter");
    EXPECT_EQ(state["components"][0]["runtime"]["mode"], "semantic-instance");
    EXPECT_EQ(state["components"][0]["runtime"]["ready"], true);
    EXPECT_EQ(state["components"][0]["runtime"]["actions"][0], "add");
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

    ASSERT_FALSE(interpreter.dispatchComponentAction(
        components[1]["id"].get<std::string>(), "missing", nlohmann::json::array(),
        bindings, error));
    EXPECT_NE(error.find("available actions"), std::string::npos) << error;
}

TEST(FriendlySyntax, DirectComponentActionFallbackDiagnosticsKeepBodySourceLines) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make Worker\n"
        "  let value = \"\"\n"
        "  when run\n"
        "    try\n"
        "      value = \"done\"\n"
        "  card\n"
        "    button \"Run\" click run\n"
        "page\n"
        "  Worker\n");

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler browserTranspiler;
    browserTranspiler.setBrowserLocalRuntime(true);
    const auto manifest = clientManifestFromHtml(browserTranspiler.transpile(program));
    ASSERT_EQ(manifest["componentDefinitions"].size(), 1) << manifest.dump(2);
    const auto& bodyPlan = manifest["componentDefinitions"][0]["bodyPlan"];
    ASSERT_GE(bodyPlan.size(), 4) << manifest.dump(2);
    EXPECT_TRUE(std::any_of(bodyPlan.begin(), bodyPlan.end(), [](const auto& node) {
        return node["text"] == "try" &&
               node["kind"] == "template" &&
               node["sourceLine"].template get<int>() > 0;
    })) << manifest.dump(2);

    JtmlTranspiler liveTranspiler;
    InterpreterConfig config;
    config.startWebSocket = false;
    Interpreter interpreter(liveTranspiler, config);
    interpreter.interpret(program);
    auto components = nlohmann::json::parse(interpreter.getComponentsJSON());
    ASSERT_EQ(components.size(), 1) << components.dump(2);
    std::string bindings;
    std::string error;
    EXPECT_FALSE(interpreter.dispatchComponentAction(
        components[0]["id"].get<std::string>(),
        "run",
        nlohmann::json::array(),
        bindings,
        error));
    EXPECT_NE(error.find("Unsupported component body-plan action 'Worker.run'"),
              std::string::npos) << error;
    EXPECT_NE(error.find("component definition line"), std::string::npos) << error;
    EXPECT_NE(error.find("body line"), std::string::npos) << error;
    EXPECT_NE(error.find("near `try`"), std::string::npos) << error;
}

TEST(FriendlySyntax, InterpreterLiveComponentsExposeSlotsAndRunBodyPlanActionArgs) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make Picker\n"
        "  let selected = \"\"\n"
        "  when pick value\n"
        "    selected = value\n"
        "  card\n"
        "    slot\n"
        "    text selected\n"
        "page\n"
        "  Picker\n"
        "    text \"Choose one\"\n");

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
    interpreter.interpret(program);

    auto components = nlohmann::json::parse(interpreter.getComponentsJSON());
    ASSERT_EQ(components.size(), 1) << components.dump(2);
    EXPECT_EQ(components[0]["runtime"]["bodyPlanActionExecution"], true);
    ASSERT_TRUE(components[0].contains("slotPlan")) << components.dump(2);
    ASSERT_EQ(components[0]["slotPlan"].size(), 1) << components.dump(2);
    EXPECT_EQ(components[0]["slotPlan"][0]["text"], "text \"Choose one\"");

    std::string bindings;
    std::string error;
    ASSERT_TRUE(interpreter.dispatchComponentAction(
        components[0]["id"].get<std::string>(),
        "pick",
        nlohmann::json::array({"Ada"}),
        bindings,
        error)) << error;

    auto state = nlohmann::json::parse(interpreter.getStateJSON());
    ASSERT_EQ(state["components"].size(), 1) << state.dump(2);
    EXPECT_EQ(state["components"][0]["locals"]["selected"]["value"], "Ada");
    EXPECT_EQ(state["components"][0]["runtime"]["bodyPlanTemplateRendering"], true);
    EXPECT_NE(state["components"][0]["renderedHtml"].get<std::string>().find("Choose one"),
              std::string::npos) << state.dump(2);
    EXPECT_NE(state["components"][0]["renderedHtml"].get<std::string>().find("Ada"),
              std::string::npos) << state.dump(2);
}

TEST(FriendlySyntax, InterpreterLiveBodyPlanActionsFailClosedAndPreservePlusEqualsSemantics) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make Mixed\n"
        "  let count = 1\n"
        "  let label = \"A\"\n"
        "  let items = [1, 2, 3]\n"
        "  let letters = \"xy\"\n"
        "  let weights = { \"a\": 4, \"b\": 5 }\n"
        "  when add\n"
        "    count += 2\n"
        "    label += \"B\"\n"
        "  when localLet\n"
        "    let next = count + 5\n"
        "    get replacement = \"local\"\n"
        "    count = next\n"
        "    label = replacement\n"
        "  when sum\n"
        "    count = 0\n"
        "    for item in items\n"
        "      count += item\n"
        "    if count == 6\n"
        "      label = \"summed\"\n"
        "  when spell\n"
        "    label = \"\"\n"
        "    for letter in letters\n"
        "      label += letter\n"
        "  when sumDict\n"
        "    count = 0\n"
        "    for value in weights\n"
        "      count += value\n"
        "  when guarded\n"
        "    count += 1\n"
        "    if count\n"
        "      label = \"guarded\"\n"
        "  box\n"
        "    text label\n"
        "page\n"
        "  Mixed\n");

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
    interpreter.interpret(program);

    auto components = nlohmann::json::parse(interpreter.getComponentsJSON());
    ASSERT_EQ(components.size(), 1) << components.dump(2);
    const auto componentId = components[0]["id"].get<std::string>();

    std::string bindings;
    std::string error;
    ASSERT_TRUE(interpreter.dispatchComponentAction(
        componentId, "add", nlohmann::json::array(), bindings, error)) << error;

    auto state = nlohmann::json::parse(interpreter.getStateJSON());
    ASSERT_EQ(state["components"].size(), 1) << state.dump(2);
    EXPECT_EQ(state["components"][0]["locals"]["count"]["value"], 3);
    EXPECT_EQ(state["components"][0]["locals"]["label"]["value"], "AB");

    ASSERT_TRUE(interpreter.dispatchComponentAction(
        componentId, "localLet", nlohmann::json::array(), bindings, error)) << error;

    state = nlohmann::json::parse(interpreter.getStateJSON());
    ASSERT_EQ(state["components"].size(), 1) << state.dump(2);
    EXPECT_EQ(state["components"][0]["locals"]["count"]["value"], 8);
    EXPECT_EQ(state["components"][0]["locals"]["label"]["value"], "local");
    EXPECT_EQ(state["components"][0]["locals"]["next"]["value"], 8);
    EXPECT_EQ(state["components"][0]["locals"]["replacement"]["value"], "local");
    EXPECT_NE(state["components"][0].value("renderedHtml", "").find(">local</p>"),
              std::string::npos) << state.dump(2);

    ASSERT_TRUE(interpreter.dispatchComponentAction(
        componentId, "sum", nlohmann::json::array(), bindings, error)) << error;

    state = nlohmann::json::parse(interpreter.getStateJSON());
    ASSERT_EQ(state["components"].size(), 1) << state.dump(2);
    EXPECT_EQ(state["components"][0]["locals"]["count"]["value"], 6);
    EXPECT_EQ(state["components"][0]["locals"]["label"]["value"], "summed");

    ASSERT_TRUE(interpreter.dispatchComponentAction(
        componentId, "spell", nlohmann::json::array(), bindings, error)) << error;

    state = nlohmann::json::parse(interpreter.getStateJSON());
    ASSERT_EQ(state["components"].size(), 1) << state.dump(2);
    EXPECT_EQ(state["components"][0]["locals"]["label"]["value"], "xy");

    ASSERT_TRUE(interpreter.dispatchComponentAction(
        componentId, "sumDict", nlohmann::json::array(), bindings, error)) << error;

    state = nlohmann::json::parse(interpreter.getStateJSON());
    ASSERT_EQ(state["components"].size(), 1) << state.dump(2);
    EXPECT_EQ(state["components"][0]["locals"]["count"]["value"], 9);

    ASSERT_TRUE(interpreter.dispatchComponentAction(
        componentId, "guarded", nlohmann::json::array(), bindings, error)) << error;

    state = nlohmann::json::parse(interpreter.getStateJSON());
    ASSERT_EQ(state["components"].size(), 1) << state.dump(2);
    EXPECT_EQ(state["components"][0]["locals"]["count"]["value"], 10);
    EXPECT_EQ(state["components"][0]["locals"]["label"]["value"], "guarded");
}

TEST(FriendlySyntax, DirectComponentActionsMutateMemberAndSubscriptPaths) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make ProfileCard\n"
        "  let profile = { \"name\": \"Ada\", \"stats\": [1, 5], \"nested\": { \"city\": \"Lisbon\" } }\n"
        "  let clicks = 0\n"
        "  let activity = []\n"
        "  when mutate index\n"
        "    clicks++\n"
        "    profile.name = \"Grace\"\n"
        "    profile.stats[index] += 1\n"
        "    profile.stats[1]--\n"
        "    profile.nested.city = \"Porto\"\n"
        "    profile.created.city = \"Coimbra\"\n"
        "    activity[0].label = \"Opened\"\n"
        "  card\n"
        "    text profile.name\n"
        "    text profile.stats[0]\n"
        "    text profile.stats[1]\n"
        "    text profile.nested.city\n"
        "    text profile.created.city\n"
        "    text clicks\n"
        "    text activity[0].label\n"
        "page\n"
        "  ProfileCard\n");

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler browserTranspiler;
    browserTranspiler.setBrowserLocalRuntime(true);
    const std::string browserHtml = browserTranspiler.transpile(program);
    EXPECT_NE(browserHtml.find("function parseClientPathSegments(path, scope)"),
              std::string::npos) << browserHtml;
    EXPECT_NE(browserHtml.find("function assignObjectPath(root, parts, value)"),
              std::string::npos) << browserHtml;
    EXPECT_NE(browserHtml.find("const targetParts = parseClientPathSegments(node.name, scope);"),
              std::string::npos) << browserHtml;
    EXPECT_NE(browserHtml.find("scope[targetParts[0]] = typeof targetParts[1] === 'number' ? [] : {};"),
              std::string::npos) << browserHtml;
    EXPECT_NE(browserHtml.find("target[key] = typeof parts[i + 1] === 'number' ? [] : {};"),
              std::string::npos) << browserHtml;
    EXPECT_NE(browserHtml.find("return assignObjectPath(scope[targetParts[0]], targetParts.slice(1), assigned);"),
              std::string::npos) << browserHtml;

    const auto manifest = clientManifestFromHtml(browserHtml);
    ASSERT_EQ(manifest["componentDefinitions"].size(), 1) << manifest.dump(2);
    bool sawNameWrite = false;
    bool sawStatsWrite = false;
    bool sawClicksIncrement = false;
    bool sawStatsDecrement = false;
    for (const auto& node : manifest["componentDefinitions"][0]["bodyPlan"]) {
        if (node.value("kind", "") != "assignment") continue;
        if (node.value("name", "") == "clicks") {
            sawClicksIncrement = true;
            EXPECT_EQ(node.value("operator", ""), "+=") << node.dump(2);
            EXPECT_EQ(node.value("expression", ""), "1") << node.dump(2);
        }
        if (node.value("name", "") == "profile.name") {
            sawNameWrite = true;
            EXPECT_NE(node["writes"].dump().find("\"profile\""), std::string::npos)
                << node.dump(2);
        }
        if (node.value("name", "") == "profile.stats[1]") {
            sawStatsDecrement = true;
            EXPECT_EQ(node.value("operator", ""), "-=") << node.dump(2);
            EXPECT_EQ(node.value("expression", ""), "1") << node.dump(2);
        }
        if (node.value("name", "") == "profile.stats[index]") {
            sawStatsWrite = true;
            EXPECT_NE(node["writes"].dump().find("\"profile\""), std::string::npos)
                << node.dump(2);
            EXPECT_NE(node["reads"].dump().find("\"index\""), std::string::npos)
                << node.dump(2);
        }
    }
    EXPECT_TRUE(sawNameWrite) << manifest.dump(2);
    EXPECT_TRUE(sawStatsWrite) << manifest.dump(2);
    EXPECT_TRUE(sawClicksIncrement) << manifest.dump(2);
    EXPECT_TRUE(sawStatsDecrement) << manifest.dump(2);

    JtmlTranspiler liveTranspiler;
    InterpreterConfig config;
    config.startWebSocket = false;
    Interpreter interpreter(liveTranspiler, config);
    interpreter.interpret(program);

    auto components = nlohmann::json::parse(interpreter.getComponentsJSON());
    ASSERT_EQ(components.size(), 1) << components.dump(2);
    const auto componentId = components[0]["id"].get<std::string>();

    std::string bindings;
    std::string error;
    ASSERT_TRUE(interpreter.dispatchComponentAction(
        componentId, "mutate", nlohmann::json::array({0}), bindings, error)) << error;

    auto state = nlohmann::json::parse(interpreter.getStateJSON());
    ASSERT_EQ(state["components"].size(), 1) << state.dump(2);
    const auto& component = state["components"][0];
    EXPECT_EQ(component["locals"]["profile"]["value"]["name"], "Grace")
        << component.dump(2);
    EXPECT_EQ(component["locals"]["profile"]["value"]["stats"][0], 2)
        << component.dump(2);
    EXPECT_EQ(component["locals"]["profile"]["value"]["stats"][1], 4)
        << component.dump(2);
    EXPECT_EQ(component["locals"]["clicks"]["value"], 1)
        << component.dump(2);
    EXPECT_EQ(component["locals"]["profile"]["value"]["nested"]["city"], "Porto")
        << component.dump(2);
    EXPECT_EQ(component["locals"]["profile"]["value"]["created"]["city"], "Coimbra")
        << component.dump(2);
    EXPECT_EQ(component["locals"]["activity"]["value"][0]["label"], "Opened")
        << component.dump(2);
    EXPECT_NE(component.value("renderedHtml", "").find(">Grace</p>"),
              std::string::npos) << component.dump(2);
    EXPECT_NE(component.value("renderedHtml", "").find(">2</p>"),
              std::string::npos) << component.dump(2);
    EXPECT_NE(component.value("renderedHtml", "").find(">4</p>"),
              std::string::npos) << component.dump(2);
    EXPECT_NE(component.value("renderedHtml", "").find(">Porto</p>"),
              std::string::npos) << component.dump(2);
    EXPECT_NE(component.value("renderedHtml", "").find(">Coimbra</p>"),
              std::string::npos) << component.dump(2);
    EXPECT_NE(component.value("renderedHtml", "").find(">Opened</p>"),
              std::string::npos) << component.dump(2);
}

TEST(FriendlySyntax, InterpreterLiveBodyPlanTemplateSupportReflectsRenderedHtml) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make Worker\n"
        "  let count = 0\n"
        "  when add\n"
        "    count += 1\n"
        "page\n"
        "  Worker\n");

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
    interpreter.interpret(program);

    auto definitions = nlohmann::json::parse(interpreter.getComponentDefinitionsJSON());
    ASSERT_EQ(definitions.size(), 1) << definitions.dump(2);
    EXPECT_EQ(definitions[0]["runtimePlan"]["bodyPlanActionExecution"], true);
    EXPECT_EQ(definitions[0]["runtimePlan"]["bodyPlanTemplateRendering"], false);
    EXPECT_EQ(definitions[0]["runtimePlan"]["renderedHtmlSupported"], false);

    auto state = nlohmann::json::parse(interpreter.getStateJSON());
    ASSERT_EQ(state["components"].size(), 1) << state.dump(2);
    EXPECT_EQ(state["components"][0]["runtime"]["bodyPlanActionExecution"], true);
    EXPECT_EQ(state["components"][0]["runtime"]["bodyPlanTemplateRendering"], false);
    EXPECT_EQ(state["components"][0]["runtime"]["renderedHtmlSupported"], false);
    EXPECT_EQ(state["components"][0]["renderedHtmlSupported"], false);
    EXPECT_EQ(state["components"][0]["renderedHtml"], "");

    std::string bindings;
    std::string error;
    ASSERT_TRUE(interpreter.dispatchComponentAction(
        state["components"][0]["id"].get<std::string>(),
        "add",
        nlohmann::json::array(),
        bindings,
        error)) << error;

    state = nlohmann::json::parse(interpreter.getStateJSON());
    ASSERT_EQ(state["components"].size(), 1) << state.dump(2);
    EXPECT_EQ(state["components"][0]["locals"]["count"]["value"], 1);
    EXPECT_EQ(state["components"][0]["runtime"]["bodyPlanTemplateRendering"], false);
    EXPECT_EQ(state["components"][0]["renderedHtmlSupported"], false);
}

TEST(FriendlySyntax, BrowserAndLiveComponentBodyPlanMetadataStayInParity) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make Picker title\n"
        "  let selected = \"\"\n"
        "  when pick value\n"
        "    selected = value\n"
        "  card\n"
        "    h2 title\n"
        "    slot\n"
        "    button \"Ada\" click pick(\"Ada\")\n"
        "page\n"
        "  Picker \"Chooser\"\n"
        "    text \"Choose one\"\n");

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler browserTranspiler;
    browserTranspiler.setBrowserLocalRuntime(true);
    const auto clientManifest = clientManifestFromHtml(browserTranspiler.transpile(program));
    ASSERT_EQ(clientManifest["componentDefinitions"].size(), 1) << clientManifest.dump(2);
    ASSERT_EQ(clientManifest["componentInstances"].size(), 1) << clientManifest.dump(2);

    JtmlTranspiler liveTranspiler;
    InterpreterConfig config;
    config.startWebSocket = false;
    Interpreter interpreter(liveTranspiler, config);
    interpreter.interpret(program);
    const auto liveState = nlohmann::json::parse(interpreter.getStateJSON());
    ASSERT_EQ(liveState["componentDefinitions"].size(), 1) << liveState.dump(2);
    ASSERT_EQ(liveState["components"].size(), 1) << liveState.dump(2);

    EXPECT_EQ(clientManifest["componentDefinitions"][0]["name"],
              liveState["componentDefinitions"][0]["name"]);
    EXPECT_EQ(clientManifest["componentDefinitions"][0]["params"],
              liveState["componentDefinitions"][0]["params"]);
    EXPECT_EQ(clientManifest["componentDefinitions"][0]["bodyPlan"][1]["text"],
              liveState["componentDefinitions"][0]["bodyPlan"][1]["text"]);
    EXPECT_EQ(clientManifest["componentDefinitions"][0]["bodyPlan"][2]["operator"],
              liveState["componentDefinitions"][0]["bodyPlan"][2]["operator"]);
    EXPECT_EQ(clientManifest["componentInstances"][0]["slotPlan"][0]["text"],
              liveState["components"][0]["slotPlan"][0]["text"]);
    EXPECT_EQ(liveState["components"][0]["runtime"]["bodyPlanActionExecution"], true);
    EXPECT_EQ(liveState["components"][0]["runtime"]["bodyPlanTemplateRendering"], true);
    EXPECT_EQ(liveState["componentDefinitions"][0]["runtimePlan"]["bodyPlanActionExecution"], true);
    EXPECT_EQ(liveState["componentDefinitions"][0]["runtimePlan"]["bodyPlanTemplateRendering"], true);
    EXPECT_NE(liveState["components"][0]["renderedHtml"].get<std::string>().find("<h2"),
              std::string::npos) << liveState.dump(2);
    EXPECT_NE(liveState["components"][0]["renderedHtml"].get<std::string>().find(">Chooser</h2>"),
              std::string::npos) << liveState.dump(2);
    EXPECT_NE(liveState["components"][0]["renderedHtml"].get<std::string>().find("Choose one"),
              std::string::npos) << liveState.dump(2);
    EXPECT_NE(liveState["components"][0]["renderedHtml"].get<std::string>().find("<button"),
              std::string::npos) << liveState.dump(2);
    EXPECT_NE(liveState["components"][0]["renderedHtml"].get<std::string>().find("data-jtml-direct-component-id="),
              std::string::npos) << liveState.dump(2);
    EXPECT_NE(liveState["components"][0]["renderedHtml"].get<std::string>().find("data-jtml-direct-component-action=\"pick\""),
              std::string::npos) << liveState.dump(2);
    EXPECT_NE(liveState["components"][0]["renderedHtml"].get<std::string>().find("data-jtml-direct-component-args=\"[&quot;Ada&quot;]\""),
              std::string::npos) << liveState.dump(2);
}

TEST(FriendlySyntax, InterpreterLiveBodyPlanRendererSupportsNestedComponentsAndSlots) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make Frame title\n"
        "  card class \"frame\"\n"
        "    h2 title\n"
        "    slot\n"
        "make Picker\n"
        "  let selected = \"\"\n"
        "  let people = { \"grace\": \"Grace\" }\n"
        "  when pick value\n"
        "    selected = value\n"
        "  Frame \"Chooser\"\n"
        "    text \"Choose one\"\n"
        "    for person in people\n"
        "      text person\n"
        "    button \"Ada\" click pick(\"Ada\")\n"
        "page\n"
        "  Picker\n");

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
    interpreter.interpret(program);

    auto state = nlohmann::json::parse(interpreter.getStateJSON());
    ASSERT_GE(state["components"].size(), 1) << state.dump(2);
    std::string html;
    for (const auto& component : state["components"]) {
        if (!component.contains("renderedHtml")) continue;
        const std::string candidate = component["renderedHtml"].get<std::string>();
        if (candidate.find("class=\"frame jtml-card\"") != std::string::npos &&
            candidate.find("Choose one") != std::string::npos &&
            candidate.find(">Grace</p>") != std::string::npos) {
            html = candidate;
            break;
        }
    }
    ASSERT_FALSE(html.empty()) << state.dump(2);
    EXPECT_NE(html.find("class=\"frame jtml-card\""), std::string::npos) << state.dump(2);
    EXPECT_NE(html.find("data-jtml-ui=\"card\""), std::string::npos) << state.dump(2);
    EXPECT_NE(html.find(">Chooser</h2>"), std::string::npos) << state.dump(2);
    EXPECT_NE(html.find("Choose one"), std::string::npos) << state.dump(2);
    EXPECT_NE(html.find(">Grace</p>"), std::string::npos) << state.dump(2);
    EXPECT_NE(html.find("<button"), std::string::npos) << state.dump(2);
    EXPECT_NE(html.find("data-jtml-direct-component-action=\"pick\""),
              std::string::npos) << state.dump(2);
    EXPECT_NE(html.find("data-jtml-direct-component-args=\"[&quot;Ada&quot;]\""),
              std::string::npos) << state.dump(2);
    for (const auto& component : state["components"]) {
        EXPECT_EQ(component["runtime"]["bodyPlanTemplateRendering"], true);
    }
}

TEST(FriendlySyntax, InterpreterLiveBodyPlanRendererSupportsNamedSlotInsertionSites) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make Layout\n"
        "  card class \"layout\"\n"
        "    header\n"
        "      slot header\n"
        "    main\n"
        "      slot\n"
        "    footer\n"
        "      slot footer\n"
        "page\n"
        "  Layout\n"
        "    slot header\n"
        "      h1 \"Named title\"\n"
        "    text \"Default body\"\n"
        "    slot footer\n"
        "      button \"Done\"\n");

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler browserTranspiler;
    browserTranspiler.setBrowserLocalRuntime(true);
    const std::string browserHtml = browserTranspiler.transpile(program);
    const auto manifest = clientManifestFromHtml(browserHtml);
    ASSERT_EQ(manifest["componentInstances"].size(), 1) << manifest.dump(2);
    ASSERT_EQ(manifest["componentInstances"][0]["slotPlan"].size(), 5)
        << manifest["componentInstances"][0]["slotPlan"].dump(2);
    EXPECT_EQ(manifest["componentInstances"][0]["slotPlan"][0]["text"], "slot header");
    EXPECT_EQ(manifest["componentInstances"][0]["slotPlan"][1]["text"], "h1 \"Named title\"");
    EXPECT_EQ(manifest["componentInstances"][0]["slotPlan"][2]["text"], "text \"Default body\"");
    EXPECT_EQ(manifest["componentInstances"][0]["slotPlan"][3]["text"], "slot footer");
    EXPECT_EQ(manifest["componentInstances"][0]["slotPlan"][4]["text"], "button \"Done\"");
    EXPECT_NE(browserHtml.find("function componentSlotName(node)"), std::string::npos)
        << browserHtml;
    EXPECT_NE(browserHtml.find("renderComponentSlot(instance, scope, words[1] || '')"),
              std::string::npos) << browserHtml;

    JtmlTranspiler liveTranspiler;
    InterpreterConfig config;
    config.startWebSocket = false;
    Interpreter interpreter(liveTranspiler, config);
    interpreter.interpret(program);

    auto state = nlohmann::json::parse(interpreter.getStateJSON());
    ASSERT_EQ(state["components"].size(), 1) << state.dump(2);
    ASSERT_EQ(state["components"][0]["slotPlan"].size(), 5) << state.dump(2);
    const std::string html = state["components"][0]["renderedHtml"].get<std::string>();
    EXPECT_NE(html.find("<header"), std::string::npos) << html;
    EXPECT_NE(html.find(">Named title</h1>"), std::string::npos) << html;
    EXPECT_NE(html.find("<main"), std::string::npos) << html;
    EXPECT_NE(html.find(">Default body</p>"), std::string::npos) << html;
    EXPECT_NE(html.find("<footer"), std::string::npos) << html;
    EXPECT_NE(html.find(">Done</button>"), std::string::npos) << html;
    EXPECT_LT(html.find(">Named title</h1>"), html.find(">Default body</p>")) << html;
    EXPECT_LT(html.find(">Default body</p>"), html.find(">Done</button>")) << html;
    EXPECT_EQ(html.find("slot header"), std::string::npos) << html;
    EXPECT_EQ(html.find("slot footer"), std::string::npos) << html;
}

TEST(FriendlySyntax, InterpreterLiveNestedComponentCallsCarryNamedSlotPlans) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make Frame\n"
        "  card class \"frame\"\n"
        "    header\n"
        "      slot header\n"
        "    main\n"
        "      slot\n"
        "    footer\n"
        "      slot footer\n"
        "make Parent\n"
        "  panel title \"Parent\"\n"
        "    Frame\n"
        "      slot header\n"
        "        h1 \"Nested title\"\n"
        "      text \"Nested body\"\n"
        "      slot footer\n"
        "        button \"Nested done\"\n"
        "page\n"
        "  Parent\n");

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler browserTranspiler;
    browserTranspiler.setBrowserLocalRuntime(true);
    const std::string browserHtml = browserTranspiler.transpile(program);
    EXPECT_NE(browserHtml.find("function slotPlanForComponentNode(definition, node)"),
              std::string::npos) << browserHtml;
    EXPECT_NE(browserHtml.find("slotPlan: slotPlanForComponentNode(parentDefinition, node)"),
              std::string::npos) << browserHtml;

    JtmlTranspiler liveTranspiler;
    InterpreterConfig config;
    config.startWebSocket = false;
    Interpreter interpreter(liveTranspiler, config);
    interpreter.interpret(program);

    auto state = nlohmann::json::parse(interpreter.getStateJSON());
    std::string html;
    for (const auto& component : state["components"]) {
        if (component.value("component", "") != "Parent" ||
            !component.contains("renderedHtml")) {
            continue;
        }
        html = component["renderedHtml"].get<std::string>();
        break;
    }
    ASSERT_FALSE(html.empty()) << state.dump(2);
    EXPECT_NE(html.find("data-jtml-component=\"Frame\""), std::string::npos) << html;
    EXPECT_NE(html.find(">Nested title</h1>"), std::string::npos) << html;
    EXPECT_NE(html.find(">Nested body</p>"), std::string::npos) << html;
    EXPECT_NE(html.find(">Nested done</button>"), std::string::npos) << html;
    EXPECT_LT(html.find(">Nested title</h1>"), html.find(">Nested body</p>")) << html;
    EXPECT_LT(html.find(">Nested body</p>"), html.find(">Nested done</button>")) << html;
    EXPECT_EQ(html.find("slot header"), std::string::npos) << html;
    EXPECT_EQ(html.find("slot footer"), std::string::npos) << html;
}

TEST(FriendlySyntax, InterpreterLiveNestedComponentActionsUseDynamicInstances) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make Child\n"
        "  let count = 0\n"
        "  when add\n"
        "    count += 1\n"
        "  card\n"
        "    text count\n"
        "    button \"+\" click add\n"
        "make Parent\n"
        "  panel title \"Parent\"\n"
        "    Child\n"
        "page\n"
        "  Parent\n");

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
    interpreter.interpret(program);

    auto state = nlohmann::json::parse(interpreter.getStateJSON());
    auto findParentHtml = [](const nlohmann::json& stateJson) {
        for (const auto& component : stateJson["components"]) {
            if (component.value("component", "") == "Parent" &&
                component.contains("renderedHtml")) {
                return component["renderedHtml"].get<std::string>();
            }
        }
        return std::string{};
    };
    std::string html = findParentHtml(state);
    ASSERT_FALSE(html.empty()) << state.dump(2);
    EXPECT_NE(html.find("data-jtml-nested-component=\"true\""), std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-component=\"Child\""), std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-component-parent=\"Parent_"), std::string::npos) << html;

    const std::string marker = "data-jtml-direct-component-id=\"";
    const auto idStart = html.find(marker);
    ASSERT_NE(idStart, std::string::npos) << html;
    const auto idValueStart = idStart + marker.size();
    const auto idEnd = html.find('"', idValueStart);
    ASSERT_NE(idEnd, std::string::npos) << html;
    const std::string nestedId = html.substr(idValueStart, idEnd - idValueStart);
    EXPECT_NE(nestedId.find("Parent_"), std::string::npos) << nestedId;
    EXPECT_NE(nestedId.find("__Child_"), std::string::npos) << nestedId;
    EXPECT_NE(html.find("data-jtml-direct-component-action=\"add\""), std::string::npos) << html;

    std::string bindings;
    std::string error;
    ASSERT_TRUE(interpreter.dispatchComponentAction(
        nestedId,
        "add",
        nlohmann::json::array(),
        bindings,
        error)) << error;

    state = nlohmann::json::parse(interpreter.getStateJSON());
    html = findParentHtml(state);
    ASSERT_FALSE(html.empty()) << state.dump(2);
    EXPECT_NE(html.find(">1</p>"), std::string::npos) << html;
}

TEST(FriendlySyntax, InterpreterLiveNestedComponentEventsDispatchToParentHandlers) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make Child emits picked(name: string)\n"
        "  card\n"
        "    button \"Pick Ada\" click picked(\"Ada\")\n"
        "make Parent\n"
        "  let chosen = \"\"\n"
        "  when choose prefix name\n"
        "    chosen = prefix + name\n"
        "  panel title \"Chooser\"\n"
        "    Child on picked choose(\"Selected\")\n"
        "    text chosen\n"
        "page\n"
        "  Parent\n");

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler browserTranspiler;
    browserTranspiler.setBrowserLocalRuntime(true);
    const std::string browserHtml = browserTranspiler.transpile(program);
    const auto manifest = clientManifestFromHtml(browserHtml);
    ASSERT_EQ(manifest["componentDefinitions"].size(), 2) << manifest.dump(2);
    bool browserChildHasEmit = false;
    for (const auto& definition : manifest["componentDefinitions"]) {
        if (definition.value("name", "") != "Child") continue;
        browserChildHasEmit = true;
        ASSERT_EQ(definition["emits"].size(), 1) << definition.dump(2);
        EXPECT_EQ(definition["emits"][0], "picked");
        ASSERT_TRUE(definition.contains("emitArity")) << definition.dump(2);
        EXPECT_EQ(definition["emitArity"]["picked"], 1) << definition.dump(2);
        ASSERT_TRUE(definition.contains("emitPayloads")) << definition.dump(2);
        ASSERT_EQ(definition["emitPayloads"]["picked"].size(), 1) << definition.dump(2);
        EXPECT_EQ(definition["emitPayloads"]["picked"][0], "name") << definition.dump(2);
        ASSERT_TRUE(definition.contains("emitPayloadTypes")) << definition.dump(2);
        ASSERT_EQ(definition["emitPayloadTypes"]["picked"].size(), 1) << definition.dump(2);
        EXPECT_EQ(definition["emitPayloadTypes"]["picked"][0], "string") << definition.dump(2);
    }
    EXPECT_TRUE(browserChildHasEmit) << manifest.dump(2);
    EXPECT_NE(browserHtml.find("function parseComponentEventHandlers(words, paramCount, scope)"),
              std::string::npos) << browserHtml;
    EXPECT_NE(browserHtml.find("function emitComponentEvent(instance, eventName, args)"),
              std::string::npos) << browserHtml;
    EXPECT_NE(browserHtml.find("function componentEventPayloadTypeMatches(value, declaredType)"),
              std::string::npos) << browserHtml;
    EXPECT_NE(browserHtml.find("return runDirectComponentAction(\n          parent.id"),
              std::string::npos) << browserHtml;

    JtmlTranspiler liveTranspiler;
    InterpreterConfig config;
    config.startWebSocket = false;
    Interpreter interpreter(liveTranspiler, config);
    interpreter.interpret(program);

    auto state = nlohmann::json::parse(interpreter.getStateJSON());
    ASSERT_EQ(state["componentDefinitions"].size(), 2) << state.dump(2);
    bool liveChildHasEmit = false;
    for (const auto& definition : state["componentDefinitions"]) {
        if (definition.value("name", "") != "Child") continue;
        liveChildHasEmit = true;
        ASSERT_EQ(definition["emits"].size(), 1) << definition.dump(2);
        EXPECT_EQ(definition["emits"][0], "picked");
        ASSERT_TRUE(definition.contains("emitArity")) << definition.dump(2);
        EXPECT_EQ(definition["emitArity"]["picked"], 1) << definition.dump(2);
        ASSERT_TRUE(definition.contains("emitPayloads")) << definition.dump(2);
        ASSERT_EQ(definition["emitPayloads"]["picked"].size(), 1) << definition.dump(2);
        EXPECT_EQ(definition["emitPayloads"]["picked"][0], "name") << definition.dump(2);
        ASSERT_TRUE(definition.contains("emitPayloadTypes")) << definition.dump(2);
        ASSERT_EQ(definition["emitPayloadTypes"]["picked"].size(), 1) << definition.dump(2);
        EXPECT_EQ(definition["emitPayloadTypes"]["picked"][0], "string") << definition.dump(2);
        ASSERT_EQ(definition["runtimePlan"]["emits"].size(), 1) << definition.dump(2);
        EXPECT_EQ(definition["runtimePlan"]["emits"][0], "picked");
        EXPECT_EQ(definition["runtimePlan"]["emitArity"]["picked"], 1) << definition.dump(2);
        EXPECT_EQ(definition["runtimePlan"]["emitPayloads"]["picked"][0], "name") << definition.dump(2);
        EXPECT_EQ(definition["runtimePlan"]["emitPayloadTypes"]["picked"][0], "string") << definition.dump(2);
    }
    EXPECT_TRUE(liveChildHasEmit) << state.dump(2);
    auto findParentHtml = [](const nlohmann::json& stateJson) {
        for (const auto& component : stateJson["components"]) {
            if (component.value("component", "") == "Parent" &&
                component.contains("renderedHtml")) {
                return component["renderedHtml"].get<std::string>();
            }
        }
        return std::string{};
    };
    std::string html = findParentHtml(state);
    ASSERT_FALSE(html.empty()) << state.dump(2);
    EXPECT_NE(html.find("data-jtml-component=\"Child\""), std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-direct-component-action=\"picked\""),
              std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-direct-component-args=\"[&quot;Ada&quot;]\""),
              std::string::npos) << html;
    EXPECT_EQ(html.find(">SelectedAda</p>"), std::string::npos) << html;

    const std::string marker = "data-jtml-direct-component-id=\"";
    const auto idStart = html.find(marker);
    ASSERT_NE(idStart, std::string::npos) << html;
    const auto idValueStart = idStart + marker.size();
    const auto idEnd = html.find('"', idValueStart);
    ASSERT_NE(idEnd, std::string::npos) << html;
    const std::string nestedId = html.substr(idValueStart, idEnd - idValueStart);
    EXPECT_NE(nestedId.find("__Child_"), std::string::npos) << nestedId;

    std::string bindings;
    std::string error;
    EXPECT_FALSE(interpreter.dispatchComponentAction(
        nestedId,
        "picked",
        nlohmann::json::array({"Ada", "extra"}),
        bindings,
        error));
    EXPECT_NE(error.find("expected 1 payload argument"), std::string::npos) << error;

    error.clear();
    ASSERT_TRUE(interpreter.dispatchComponentAction(
        nestedId,
        "picked",
        nlohmann::json::array({"Ada"}),
        bindings,
        error)) << error;

    state = nlohmann::json::parse(interpreter.getStateJSON());
    html = findParentHtml(state);
    ASSERT_FALSE(html.empty()) << state.dump(2);
    EXPECT_NE(html.find(">SelectedAda</p>"), std::string::npos) << html;
    bool foundParentState = false;
    for (const auto& component : state["components"]) {
        if (component.value("component", "") != "Parent") continue;
        foundParentState = true;
        EXPECT_EQ(component["locals"]["chosen"]["value"], "SelectedAda");
    }
    EXPECT_TRUE(foundParentState) << state.dump(2);
}

TEST(FriendlySyntax, InterpreterLiveActionBodyCanEmitDeclaredComponentEvent) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make Child emits picked(name)\n"
        "  when fire\n"
        "    picked(\"Ada Lovelace\")\n"
        "  card\n"
        "    button \"Fire\" click fire\n"
        "make Parent\n"
        "  let chosen = \"\"\n"
        "  when choose name\n"
        "    chosen = name\n"
        "  panel title \"Chooser\"\n"
        "    Child on picked choose\n"
        "    text chosen\n"
        "page\n"
        "  Parent\n");

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler liveTranspiler;
    InterpreterConfig config;
    config.startWebSocket = false;
    Interpreter interpreter(liveTranspiler, config);
    interpreter.interpret(program);

    auto state = nlohmann::json::parse(interpreter.getStateJSON());
    std::string parentId;
    std::string parentHtml;
    for (const auto& component : state["components"]) {
        if (component.value("component", "") == "Parent") {
            parentId = component.value("id", "");
            parentHtml = component.value("renderedHtml", "");
        }
    }
    ASSERT_FALSE(parentId.empty()) << state.dump(2);
    ASSERT_FALSE(parentHtml.empty()) << state.dump(2);
    const std::string marker = "data-jtml-direct-component-id=\"";
    const auto idStart = parentHtml.find(marker);
    ASSERT_NE(idStart, std::string::npos) << parentHtml;
    const auto idValueStart = idStart + marker.size();
    const auto idEnd = parentHtml.find('"', idValueStart);
    ASSERT_NE(idEnd, std::string::npos) << parentHtml;
    const std::string childId = parentHtml.substr(idValueStart, idEnd - idValueStart);
    EXPECT_NE(childId.find("__Child_"), std::string::npos) << childId;

    std::string bindings;
    std::string error;
    ASSERT_TRUE(interpreter.dispatchComponentAction(
        childId,
        "fire",
        nlohmann::json::array(),
        bindings,
        error)) << error;

    state = nlohmann::json::parse(interpreter.getStateJSON());
    bool foundParent = false;
    for (const auto& component : state["components"]) {
        if (component.value("id", "") != parentId) continue;
        foundParent = true;
        EXPECT_EQ(component["locals"]["chosen"]["value"], "Ada Lovelace") << component.dump(2);
        EXPECT_NE(component.value("renderedHtml", "").find(">Ada Lovelace</p>"), std::string::npos)
            << component.dump(2);
    }
    EXPECT_TRUE(foundParent) << state.dump(2);
}

TEST(FriendlySyntax, InterpreterLiveActionBodyRejectsWrongTypedEmitPayload) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make Child emits picked(name: number)\n"
        "  when fire\n"
        "    picked(\"Ada Lovelace\")\n"
        "  card\n"
        "    button \"Fire\" click fire\n"
        "make Parent\n"
        "  let chosen = \"\"\n"
        "  when choose name\n"
        "    chosen = name\n"
        "  panel title \"Chooser\"\n"
        "    Child on picked choose\n"
        "    text chosen\n"
        "page\n"
        "  Parent\n");

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler liveTranspiler;
    InterpreterConfig config;
    config.startWebSocket = false;
    Interpreter interpreter(liveTranspiler, config);
    interpreter.interpret(program);

    auto state = nlohmann::json::parse(interpreter.getStateJSON());
    std::string childId;
    for (const auto& component : state["components"]) {
        if (component.value("component", "") != "Parent") continue;
        const std::string html = component.value("renderedHtml", "");
        const std::string marker = "data-jtml-direct-component-id=\"";
        const auto idStart = html.find(marker);
        ASSERT_NE(idStart, std::string::npos) << html;
        const auto idValueStart = idStart + marker.size();
        const auto idEnd = html.find('"', idValueStart);
        ASSERT_NE(idEnd, std::string::npos) << html;
        childId = html.substr(idValueStart, idEnd - idValueStart);
    }
    ASSERT_FALSE(childId.empty()) << state.dump(2);

    std::string bindings;
    std::string error;
    EXPECT_FALSE(interpreter.dispatchComponentAction(
        childId,
        "fire",
        nlohmann::json::array(),
        bindings,
        error));
    EXPECT_NE(error.find("Component 'Child' event 'picked' payload 'name' expected type 'number'"),
              std::string::npos) << error;
    EXPECT_NE(error.find("component definition line"), std::string::npos) << error;

    state = nlohmann::json::parse(interpreter.getStateJSON());
    for (const auto& component : state["components"]) {
        if (component.value("component", "") != "Parent") continue;
        EXPECT_EQ(component["locals"]["chosen"]["value"], "") << component.dump(2);
    }
}

TEST(FriendlySyntax, InterpreterLiveNestedComponentsInsideLoopsKeepSeparateInstances) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make Child label\n"
        "  let count = 0\n"
        "  when add\n"
        "    count += 1\n"
        "  card\n"
        "    text label\n"
        "    text count\n"
        "    button \"+\" click add\n"
        "make Parent\n"
        "  let names = [\"Ada\", \"Bo\"]\n"
        "  when reorder\n"
        "    names = [\"Bo\", \"Ada\"]\n"
        "  when trim\n"
        "    names = [\"Ada\"]\n"
        "  panel title \"Team\"\n"
        "    for name in names key name\n"
        "      Child name\n"
        "page\n"
        "  Parent\n");

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
    interpreter.interpret(program);

    auto parentHtml = [](const nlohmann::json& stateJson) {
        for (const auto& component : stateJson["components"]) {
            if (component.value("component", "") == "Parent" &&
                component.contains("renderedHtml")) {
                return component["renderedHtml"].get<std::string>();
            }
        }
        return std::string{};
    };
    auto parentId = [](const nlohmann::json& stateJson) {
        for (const auto& component : stateJson["components"]) {
            if (component.value("component", "") == "Parent") {
                return component.value("id", std::string{});
            }
        }
        return std::string{};
    };
    auto extractDirectIds = [](const std::string& html) {
        std::vector<std::string> ids;
        const std::string marker = "data-jtml-direct-component-id=\"";
        size_t pos = 0;
        while ((pos = html.find(marker, pos)) != std::string::npos) {
            const auto start = pos + marker.size();
            const auto end = html.find('"', start);
            if (end == std::string::npos) break;
            ids.push_back(html.substr(start, end - start));
            pos = end + 1;
        }
        return ids;
    };

    auto state = nlohmann::json::parse(interpreter.getStateJSON());
    std::string html = parentHtml(state);
    ASSERT_FALSE(html.empty()) << state.dump(2);
    auto ids = extractDirectIds(html);
    ASSERT_EQ(ids.size(), 2) << html;
    EXPECT_NE(ids[0], ids[1]) << html;
    EXPECT_NE(ids[0].find("_for"), std::string::npos) << ids[0];
    EXPECT_NE(ids[1].find("_for"), std::string::npos) << ids[1];
    EXPECT_NE(ids[0].find("_Ada"), std::string::npos) << ids[0];
    EXPECT_NE(ids[1].find("_Bo"), std::string::npos) << ids[1];
    EXPECT_NE(html.find(">Ada</p>"), std::string::npos) << html;
    EXPECT_NE(html.find(">Bo</p>"), std::string::npos) << html;

    const std::string adaId = ids[0];
    const std::string boId = ids[1];
    std::string bindings;
    std::string error;
    ASSERT_TRUE(interpreter.dispatchComponentAction(
        adaId,
        "add",
        nlohmann::json::array(),
        bindings,
        error)) << error;

    state = nlohmann::json::parse(interpreter.getStateJSON());
    html = parentHtml(state);
    ASSERT_FALSE(html.empty()) << state.dump(2);
    EXPECT_NE(html.find(">1</p>"), std::string::npos) << html;
    EXPECT_NE(html.find(">0</p>"), std::string::npos) << html;

    const std::string owningParentId = parentId(state);
    ASSERT_FALSE(owningParentId.empty()) << state.dump(2);
    ASSERT_TRUE(interpreter.dispatchComponentAction(
        owningParentId,
        "reorder",
        nlohmann::json::array(),
        bindings,
        error)) << error;

    state = nlohmann::json::parse(interpreter.getStateJSON());
    html = parentHtml(state);
    ASSERT_FALSE(html.empty()) << state.dump(2);
    ids = extractDirectIds(html);
    ASSERT_EQ(ids.size(), 2) << html;
    EXPECT_EQ(ids[0], boId) << html;
    EXPECT_EQ(ids[1], adaId) << html;
    EXPECT_LT(html.find(">Bo</p>"), html.find(">Ada</p>")) << html;
    EXPECT_NE(html.find(">1</p>"), std::string::npos) << html;
    EXPECT_NE(html.find(">0</p>"), std::string::npos) << html;

    ASSERT_TRUE(interpreter.dispatchComponentAction(
        owningParentId,
        "trim",
        nlohmann::json::array(),
        bindings,
        error)) << error;

    state = nlohmann::json::parse(interpreter.getStateJSON());
    html = parentHtml(state);
    ASSERT_FALSE(html.empty()) << state.dump(2);
    ids = extractDirectIds(html);
    ASSERT_EQ(ids.size(), 1) << html;
    EXPECT_NE(ids[0].find("_for"), std::string::npos) << ids[0];
    EXPECT_EQ(ids[0], adaId) << html;
    EXPECT_NE(html.find(">Ada</p>"), std::string::npos) << html;
    EXPECT_EQ(html.find(">Bo</p>"), std::string::npos) << html;

    error.clear();
    EXPECT_FALSE(interpreter.dispatchComponentAction(
        boId,
        "add",
        nlohmann::json::array(),
        bindings,
        error));
    EXPECT_NE(error.find("Component instance not found"), std::string::npos) << error;
}

TEST(FriendlySyntax, InterpreterLiveBodyPlanRendererPreservesRouteLinks) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "route \"/\" as Home\n"
        "route \"/user/:id\" as UserProfile\n"
        "make Home\n"
        "  page\n"
        "    nav\n"
        "      link \"Ada\" to \"/user/ada\" active-class \"active\"\n"
        "make UserProfile id\n"
        "  page\n"
        "    link \"Back\" to \"/\"\n"
        "page\n"
        "  Home\n");

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
    interpreter.interpret(program);

    auto state = nlohmann::json::parse(interpreter.getStateJSON());
    ASSERT_GE(state["components"].size(), 1) << state.dump(2);
    const std::string html = state["components"][0]["renderedHtml"].get<std::string>();
    EXPECT_NE(html.find("<a"), std::string::npos) << html;
    EXPECT_NE(html.find("href=\"javascript:void(0)\""), std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-href=\"#/user/ada\""), std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-link=\"true\""), std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-active-class=\"active\""), std::string::npos) << html;
    EXPECT_NE(html.find(">Ada</a>"), std::string::npos) << html;
    EXPECT_EQ(html.find("to /user/ada"), std::string::npos) << html;
}

TEST(FriendlySyntax, InterpreterLiveBodyPlanRendererPreservesSemanticUiModifiers) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make UsagePanel\n"
        "  panel title \"Usage\" pad lg shadow md width wide surface raised\n"
        "    grid cols 2 gap md\n"
        "      metric \"Users\" 42 \"Active\" tone good\n"
        "      card tone primary\n"
        "        text \"Ready\"\n"
        "page\n"
        "  UsagePanel\n");

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
    interpreter.interpret(program);

    auto state = nlohmann::json::parse(interpreter.getStateJSON());
    ASSERT_EQ(state["components"].size(), 1) << state.dump(2);
    const std::string html = state["components"][0]["renderedHtml"].get<std::string>();
    EXPECT_NE(html.find("data-jtml-ui=\"panel\""), std::string::npos) << html;
    EXPECT_NE(html.find("class=\"jtml-panel jtml-pad-lg jtml-shadow-md jtml-width-wide jtml-surface-raised\""),
              std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-ui=\"grid\""), std::string::npos) << html;
    EXPECT_NE(html.find("jtml-grid jtml-cols-2 jtml-gap-md"), std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-ui=\"metric\""), std::string::npos) << html;
    EXPECT_NE(html.find("jtml-metric jtml-tone-good"), std::string::npos) << html;
    EXPECT_NE(html.find("jtml-card jtml-tone-primary"), std::string::npos) << html;
    EXPECT_EQ(html.find("pad lg"), std::string::npos) << html;
    EXPECT_EQ(html.find("tone good"), std::string::npos) << html;
}

TEST(FriendlySyntax, DirectAndLiveBodyPlanRenderersPreserveRichPlatformAttributes) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make PlatformSurface\n"
        "  form method \"post\" action \"/signup\" autocomplete \"on\" aria-label \"Signup form\" data-kind \"lead\"\n"
        "    input type \"email\" name \"email\" placeholder \"you@example.com\" required minlength 3 maxlength 80 inputmode \"email\" pattern \".+@.+\"\n"
        "    file accept \"image/png\" multiple\n"
        "    video src \"/intro.mp4\" poster \"/poster.png\" controls playsinline preload \"metadata\"\n"
        "    svg viewBox \"0 0 100 100\" aria-label \"Usage graphic\"\n"
        "      circle cx 50 cy 50 r 20 fill \"#2563eb\" stroke-width 2 data-point \"center\"\n"
        "    checkbox checked\n"
        "page\n"
        "  PlatformSurface\n");

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    const std::string browserHtml = transpiler.transpile(program);
    EXPECT_NE(browserHtml.find("accept: true"), std::string::npos) << browserHtml;
    EXPECT_NE(browserHtml.find("viewBox: true"), std::string::npos) << browserHtml;
    EXPECT_NE(browserHtml.find("playsinline: true"), std::string::npos) << browserHtml;
    EXPECT_NE(browserHtml.find("link: 'a'"), std::string::npos) << browserHtml;
    EXPECT_NE(browserHtml.find("image: 'img'"), std::string::npos) << browserHtml;

    InterpreterConfig config;
    config.startWebSocket = false;
    Interpreter interpreter(transpiler, config);
    interpreter.interpret(program);

    auto state = nlohmann::json::parse(interpreter.getStateJSON());
    ASSERT_EQ(state["components"].size(), 1) << state.dump(2);
    const std::string html = state["components"][0]["renderedHtml"].get<std::string>();
    EXPECT_NE(html.find("<form"), std::string::npos) << html;
    EXPECT_NE(html.find("method=\"post\""), std::string::npos) << html;
    EXPECT_NE(html.find("action=\"/signup\""), std::string::npos) << html;
    EXPECT_NE(html.find("autocomplete=\"on\""), std::string::npos) << html;
    EXPECT_NE(html.find("aria-label=\"Signup form\""), std::string::npos) << html;
    EXPECT_NE(html.find("data-kind=\"lead\""), std::string::npos) << html;
    EXPECT_NE(html.find("<input"), std::string::npos) << html;
    EXPECT_NE(html.find("type=\"email\""), std::string::npos) << html;
    EXPECT_NE(html.find("required"), std::string::npos) << html;
    EXPECT_NE(html.find("minlength=\"3\""), std::string::npos) << html;
    EXPECT_NE(html.find("maxlength=\"80\""), std::string::npos) << html;
    EXPECT_NE(html.find("inputmode=\"email\""), std::string::npos) << html;
    EXPECT_NE(html.find("pattern=\".+@.+\""), std::string::npos) << html;
    EXPECT_NE(html.find("type=\"file\""), std::string::npos) << html;
    EXPECT_NE(html.find("accept=\"image/png\""), std::string::npos) << html;
    EXPECT_NE(html.find("multiple"), std::string::npos) << html;
    EXPECT_NE(html.find("<video"), std::string::npos) << html;
    EXPECT_NE(html.find("poster=\"/poster.png\""), std::string::npos) << html;
    EXPECT_NE(html.find("playsinline"), std::string::npos) << html;
    EXPECT_NE(html.find("preload=\"metadata\""), std::string::npos) << html;
    EXPECT_NE(html.find("<svg"), std::string::npos) << html;
    EXPECT_NE(html.find("viewBox=\"0 0 100 100\""), std::string::npos) << html;
    EXPECT_NE(html.find("aria-label=\"Usage graphic\""), std::string::npos) << html;
    EXPECT_NE(html.find("<circle"), std::string::npos) << html;
    EXPECT_NE(html.find("cx=\"50\""), std::string::npos) << html;
    EXPECT_NE(html.find("fill=\"#2563eb\""), std::string::npos) << html;
    EXPECT_NE(html.find("stroke-width=\"2\""), std::string::npos) << html;
    EXPECT_NE(html.find("data-point=\"center\""), std::string::npos) << html;
    EXPECT_NE(html.find("type=\"checkbox\""), std::string::npos) << html;
    EXPECT_NE(html.find("checked"), std::string::npos) << html;
    EXPECT_EQ(html.find("viewBox &quot;0"), std::string::npos) << html;
    EXPECT_EQ(html.find("accept image/png"), std::string::npos) << html;
}

TEST(FriendlySyntax, LiveBodyPlanRendererKeepsInlineContentBeforeTrailingAttributes) {
    std::string classic = normalizeOk(
        "jtml 2\n"
        "make StatCard title value\n"
        "  section class \"stat-card\"\n"
        "    h2 title style \"font-size: 16px; margin: 0\"\n"
        "    strong value style \"font-size: 28px\"\n"
        "page\n"
        "  StatCard \"Revenue\" \"42k\"\n");

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    const std::string browserHtml = transpiler.transpile(program);
    EXPECT_NE(browserHtml.find("content.push(token);"),
              std::string::npos) << browserHtml;

    InterpreterConfig config;
    config.startWebSocket = false;
    Interpreter interpreter(transpiler, config);
    interpreter.interpret(program);

    auto state = nlohmann::json::parse(interpreter.getStateJSON());
    ASSERT_EQ(state["components"].size(), 1) << state.dump(2);
    const std::string html = state["components"][0]["renderedHtml"].get<std::string>();
    EXPECT_NE(html.find("<h2"), std::string::npos) << html;
    EXPECT_NE(html.find("style=\"font-size: 16px; margin: 0\""), std::string::npos) << html;
    EXPECT_NE(html.find(">Revenue</h2>"), std::string::npos) << html;
    EXPECT_NE(html.find("style=\"font-size: 28px\""), std::string::npos) << html;
    EXPECT_NE(html.find(">42k</strong>"), std::string::npos) << html;
    EXPECT_EQ(html.find("title style"), std::string::npos) << html;
    EXPECT_EQ(html.find("value style"), std::string::npos) << html;
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

TEST(FriendlySyntax, BrowserLocalRuntimeEmitsStateDerivedAndActionManifest) {
    const std::string src =
        "jtml 2\n"
        "\n"
        "let count = 0\n"
        "get label = \"Count {count}\"\n"
        "\n"
        "when add\n"
        "  count += 1\n"
        "\n"
        "page\n"
        "  text label\n"
        "  button \"Add\" click add\n";

    std::string classic = jtml::normalizeSourceSyntax(src, jtml::SyntaxMode::Friendly);
    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    transpiler.setBrowserLocalRuntime(true);
    std::string html = transpiler.transpile(program);

    EXPECT_NE(html.find("id=\"__jtml_client_manifest\""), std::string::npos) << html;
    EXPECT_NE(html.find("\"state\":{\"count\":\"0.000000000000000\"}"), std::string::npos) << html;
    EXPECT_NE(html.find("\"derived\":{\"label\":\"(\\\"Count \\\" + count)\"}"), std::string::npos) << html;
    EXPECT_NE(html.find("\"actions\":{\"add\""), std::string::npos) << html;
    EXPECT_NE(html.find("\"kind\":\"assign\""), std::string::npos) << html;
    EXPECT_NE(html.find("const browserLocalRuntime = true;"), std::string::npos) << html;
    EXPECT_NE(html.find("executeClientAction(fnName"), std::string::npos) << html;
    EXPECT_NE(html.find("data-jtml-expr=\"&quot;Add&quot;\""), std::string::npos) << html;
}

TEST(FriendlyMedia, FileInputEventCarriesObjectState) {
    const std::string src =
        "jtml 2\n"
        "\n"
        "let selectedImage = \"\"\n"
        "\n"
        "page\n"
        "  file \"Choose image\" accept \"image/*\" into selectedImage\n"
        "  if selectedImage\n"
        "    p \"Selected: {selectedImage.name}\"\n";

    std::string classic = jtml::normalizeSourceSyntax(src, jtml::SyntaxMode::Friendly);
    Lexer lex(classic);
    auto tokens = lex.tokenize();
    ASSERT_TRUE(lex.getErrors().empty()) << classic;
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;

    JtmlTranspiler transpiler;
    std::string html = transpiler.transpile(program);

    const std::string marker = "sendEvent('";
    const auto pos = html.find(marker);
    ASSERT_NE(pos, std::string::npos) << html;
    const auto idStart = pos + marker.size();
    const auto idEnd = html.find('\'', idStart);
    ASSERT_NE(idEnd, std::string::npos) << html;
    const std::string elementId = html.substr(idStart, idEnd - idStart);

    InterpreterConfig config;
    config.startWebSocket = false;
    Interpreter interpreter(transpiler, config);
    interpreter.interpret(program);

    std::string updatedJson;
    std::string error;
    ASSERT_TRUE(interpreter.dispatchEvent(
        elementId,
        "onChange",
        nlohmann::json::array({
            "setSelectedImage()",
            {
                {"name", "edp-logo.png"},
                {"type", "image/png"},
                {"size", 1234},
                {"preview", "blob:jtml-preview"},
            },
        }),
        updatedJson,
        error)) << error;

    auto updated = nlohmann::json::parse(updatedJson);
    ASSERT_TRUE(updated["state"]["selectedImage"].is_object()) << updated.dump(2);
    EXPECT_EQ(updated["state"]["selectedImage"]["name"], "edp-logo.png") << updated.dump(2);
    EXPECT_EQ(updated["state"]["selectedImage"]["preview"], "blob:jtml-preview") << updated.dump(2);
}

} // namespace
