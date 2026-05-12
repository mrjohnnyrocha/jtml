// tests/test_linter.cpp — verifies the linter's diagnostic behaviour on
// tight, focused inputs. The goal is to pin down each check so regressions
// surface immediately.
#include "jtml/lexer.h"
#include "jtml/friendly.h"
#include "jtml/linter.h"
#include "jtml/parser.h"
#include "jtml/transpiler.h"

#include <gtest/gtest.h>

namespace {

std::vector<LintDiagnostic> lintSource(const std::string& src) {
    Lexer lex(src);
    auto tokens = lex.tokenize();
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    jtml::JtmlLinter linter;
    return linter.lint(program);
}

int countErrors(const std::vector<LintDiagnostic>& diags) {
    int n = 0;
    for (const auto& d : diags)
        if (d.severity == LintDiagnostic::Severity::Error) ++n;
    return n;
}

TEST(Linter, CleanProgramHasNoDiagnostics) {
    auto d = lintSource("define x = 1\\\\\n");
    EXPECT_TRUE(d.empty()) << "clean program flagged " << d.size() << " issues";
}

TEST(Linter, UndefinedVariableIsError) {
    auto d = lintSource("show missing\\\\\n");
    EXPECT_GE(countErrors(d), 1);
}

TEST(Linter, ConditionalBranchesAreVisited) {
    auto d = lintSource(
        "define ok = true\\\\\n"
        "define label = ok ? knownGood : \"fallback\"\\\\\n");
    bool sawKnownGood = false;
    for (const auto& diag : d)
        if (diag.message.find("knownGood") != std::string::npos) sawKnownGood = true;
    EXPECT_TRUE(sawKnownGood);
}

TEST(Linter, UnreachableCodeAfterReturnIsWarning) {
    auto d = lintSource(
        "function noop()\\\\\n"
        "    return\\\\\n"
        "    return\\\\\n"   // unreachable
        "\\\\\n");
    bool sawUnreachable = false;
    for (const auto& diag : d)
        if (diag.message.find("unreachable") != std::string::npos) sawUnreachable = true;
    EXPECT_TRUE(sawUnreachable);
}

TEST(Linter, MutualRecursionDoesNotFalselyReportUndefined) {
    // Pre-pass must make top-level function names visible before visiting bodies.
    auto d = lintSource(
        "function a()\\\\\n"
        "    b()\\\\\n"
        "\\\\\n"
        "function b()\\\\\n"
        "    a()\\\\\n"
        "\\\\\n");
    EXPECT_EQ(countErrors(d), 0) << "mutual recursion wrongly flagged";
}

TEST(Linter, ForIteratorIsBoundInBody) {
    auto d = lintSource(
        "define items = [1, 2, 3]\\\\\n"
        "for (x in items)\\\\\n"
        "    show x\\\\\n"
        "\\\\\n");
    EXPECT_EQ(countErrors(d), 0);
}

TEST(Linter, TypedDefineReportsPrimitiveMismatch) {
    auto d = lintSource("define count: number = \"zero\"\\\\\n");
    bool sawType = false;
    for (const auto& diag : d) {
        if (diag.code == "JTML_TYPE_MISMATCH") sawType = true;
    }
    EXPECT_TRUE(sawType);
}

TEST(Linter, TypedDerivedValueAcceptsPrimitiveMatch) {
    auto d = lintSource(
        "define count: number = 1\\\\\n"
        "derive label: string = \"Count: \" + count\\\\\n");
    EXPECT_EQ(countErrors(d), 0);
}

TEST(Linter, FriendlyTypedActionParameterAndComponentParameterNormalize) {
    const std::string classic = jtml::normalizeSourceSyntax(
        "jtml 2\n\n"
        "let count: number = 0\n"
        "when add step: number\n"
        "  let count: number = count + step\n"
        "make Stat title: string value: number\n"
        "  text title\n"
        "  strong value\n"
        "page\n"
        "  Stat \"Current value\" count\n",
        jtml::SyntaxMode::Friendly);
    EXPECT_NE(classic.find("define count: number = 0"), std::string::npos);
    EXPECT_NE(classic.find("function add(step)"), std::string::npos);
    EXPECT_NE(classic.find("count = count + step"), std::string::npos);
    EXPECT_EQ(classic.find("count: number = count + step"), std::string::npos);

    auto d = lintSource(classic);
    EXPECT_EQ(countErrors(d), 0);
}

TEST(Linter, FunctionCallArityMismatchIsError) {
    auto d = lintSource(
        "function save(id, label)\\\\\n"
        "    show label\\\\\n"
        "\\\\\n"
        "save(1)\\\\\n");
    bool sawArity = false;
    for (const auto& diag : d) {
        if (diag.code == "JTML_ARITY") sawArity = true;
    }
    EXPECT_TRUE(sawArity);
}

TEST(Linter, FunctionCallArityMatchPasses) {
    auto d = lintSource(
        "function save(id, label)\\\\\n"
        "    show label\\\\\n"
        "\\\\\n"
        "save(1, \"ok\")\\\\\n");
    EXPECT_EQ(countErrors(d), 0);
}

TEST(Linter, InputEventCountsBrowserValueArgument) {
    auto d = lintSource(
        "function setName(value)\\\\\n"
        "    show value\\\\\n"
        "\\\\\n"
        "@input onInput=setName()\\\\\n"
        "#\n");
    EXPECT_EQ(countErrors(d), 0);
}

TEST(Linter, FriendlyExternActionIsKnownAndClientIntercepted) {
    const std::string classic = jtml::normalizeSourceSyntax(
        "jtml 2\n\n"
        "extern notify from \"host.notify\"\n"
        "page\n"
        "  button \"Notify\" click notify(\"Saved\")\n",
        jtml::SyntaxMode::Friendly);
    EXPECT_NE(classic.find("@meta data-jtml-extern-action=\"notify\" data-window=\"host.notify\"\\\\"),
              std::string::npos);

    auto d = lintSource(classic);
    EXPECT_EQ(countErrors(d), 0);

    Lexer lex(classic);
    auto tokens = lex.tokenize();
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    ASSERT_TRUE(parser.getErrors().empty()) << classic;
    JtmlTranspiler transpiler;
    const std::string html = transpiler.transpile(program);
    EXPECT_NE(html.find("data-jtml-extern-action=\"notify\""), std::string::npos);
    EXPECT_NE(html.find("const __jtml_extern_fns = {}"), std::string::npos);
    EXPECT_NE(html.find("startExternBindings()"), std::string::npos);
}

TEST(Linter, FriendlyRouteLayoutWrapsRouteBody) {
    const std::string classic = jtml::normalizeSourceSyntax(
        "jtml 2\n\n"
        "make Shell\n"
        "  box class \"shell\"\n"
        "    nav\n"
        "      link \"Home\" to \"/\"\n"
        "    slot\n"
        "make Home\n"
        "  section\n"
        "    h1 \"Home\"\n"
        "route \"/\" as Home layout Shell\n",
        jtml::SyntaxMode::Friendly);
    EXPECT_NE(classic.find("@section data-jtml-route=\"/\""), std::string::npos);
    EXPECT_NE(classic.find("data-jtml-layout=\"Shell\""), std::string::npos);
    EXPECT_NE(classic.find("data-jtml-component=\"Shell\""), std::string::npos);
    EXPECT_NE(classic.find("data-jtml-component-role=\"layout\""), std::string::npos);
    EXPECT_NE(classic.find("@nav"), std::string::npos);
    EXPECT_NE(classic.find("@h1"), std::string::npos);

    auto d = lintSource(classic);
    EXPECT_EQ(countErrors(d), 0);
}

TEST(Linter, MediaAccessibilityWarningsAreStructured) {
    auto d = lintSource(
        "@main\\\\\n"
        "    @img src=\"/photo.jpg\"\\\\\n"
        "    @iframe src=\"/widget\"\\\\\n"
        "    @canvas width=\"320\" height=\"180\"\\\\\n"
        "    #\n"
        "    @input type=\"file\"\\\\\n"
        "    #\n"
        "#\n");

    int mediaWarnings = 0;
    for (const auto& diag : d) {
        if (diag.code == "JTML_MEDIA_ACCESSIBILITY") ++mediaWarnings;
    }
    EXPECT_GE(mediaWarnings, 4);
    EXPECT_EQ(countErrors(d), 0);
}

TEST(Linter, AccessibleMediaPassesWithoutWarnings) {
    const std::string classic = jtml::normalizeSourceSyntax(
        "jtml 2\n"
        "let selected = \"\"\n"
        "page\n"
        "  image src \"/photo.jpg\" alt \"Team photo\"\n"
        "  video src \"/demo.mp4\" controls\n"
        "  audio src \"/intro.mp3\" controls\n"
        "  embed src \"/widget\" title \"Widget preview\"\n"
        "  canvas aria-label \"Sales chart\" width \"320\" height \"180\"\n"
        "  scene3d \"Product model\" scene productScene camera orbit controls orbit renderer three into sceneState width \"640\" height \"360\"\n"
        "  file \"Choose image\" accept \"image/*\" into selected\n",
        jtml::SyntaxMode::Friendly);
    auto d = lintSource(classic);
    for (const auto& diag : d) {
        EXPECT_NE(diag.code, "JTML_MEDIA_ACCESSIBILITY") << diag.message;
    }
    EXPECT_EQ(countErrors(d), 0);
}

TEST(Linter, Scene3DProductionWarningsAreStructured) {
    const std::string classic = jtml::normalizeSourceSyntax(
        "jtml 2\n"
        "page\n"
        "  scene3d \"Product model\" scene productScene renderer mystery width \"640\"\n",
        jtml::SyntaxMode::Friendly);
    auto d = lintSource(classic);

    int sceneWarnings = 0;
    for (const auto& diag : d) {
        if (diag.code == "JTML_MEDIA_ACCESSIBILITY" &&
            diag.message.find("scene3d") != std::string::npos) {
            ++sceneWarnings;
        }
    }
    EXPECT_GE(sceneWarnings, 2);
    EXPECT_EQ(countErrors(d), 0);
}

TEST(Linter, MediaControllerStateIsDefinedByFriendlyInto) {
    const std::string classic = jtml::normalizeSourceSyntax(
        "jtml 2\n"
        "page\n"
        "  video src \"/demo.mp4\" controls into player\n"
        "  show \"Paused: {player.paused}\"\n"
        "  button \"Play\" click player.play\n",
        jtml::SyntaxMode::Friendly);
    auto d = lintSource(classic);
    EXPECT_EQ(countErrors(d), 0);
}

} // namespace
