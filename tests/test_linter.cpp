// tests/test_linter.cpp — verifies the linter's diagnostic behaviour on
// tight, focused inputs. The goal is to pin down each check so regressions
// surface immediately.
#include "jtml/lexer.h"
#include "jtml/friendly.h"
#include "jtml/linter.h"
#include "jtml/parser.h"
#include "jtml/transpiler.h"
#include "cli/util.h"

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
    for (const auto& diag : d) {
        if (diag.message.find("unreachable") != std::string::npos) {
            sawUnreachable = true;
            EXPECT_EQ(diag.line, 3);
            EXPECT_EQ(diag.column, 5);
        }
    }
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

TEST(Linter, ActionWhileLoopRemainsValidControlFlow) {
    auto d = lintSource(
        "define count = 0\\\\\n"
        "function advance()\\\\\n"
        "    while (count < 3)\\\\\n"
        "        count = count + 1\\\\\n"
        "    \\\\\n"
        "\\\\\n");
    EXPECT_EQ(countErrors(d), 0);
    for (const auto& diag : d) {
        EXPECT_NE(diag.code, "JTML_TEMPLATE_WHILE") << diag.message;
    }
}

TEST(Linter, TemplateWhileLoopWarnsWithSourceFirstCode) {
    auto d = lintSource(
        "define keepGoing = true\\\\\n"
        "@div\\\\\n"
        "    while (keepGoing)\\\\\n"
        "        show \"loop\"\\\\\n"
        "    \\\\\n"
        "#\\\\\n");
    bool sawTemplateWhile = false;
    for (const auto& diag : d) {
        if (diag.code == "JTML_TEMPLATE_WHILE") {
            sawTemplateWhile = true;
            EXPECT_EQ(diag.severity, LintDiagnostic::Severity::Warning);
            EXPECT_NE(diag.hint.find("for"), std::string::npos) << diag.hint;
        }
    }
    EXPECT_TRUE(sawTemplateWhile);
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


namespace {
std::vector<std::unique_ptr<ASTNode>> parseFriendlyForLint(const std::string& source) {
    jtml::cli::CompilationSource input;
    input.source = jtml::MappedSource::original("lint-friendly.jtml", source);
    input.syntax = jtml::SyntaxMode::Friendly;
    auto normalized = jtml::cli::normalizeCompilationSource(input);
    Lexer lexer(normalized.text, normalized.provenance);
    Parser parser(lexer.tokenize());
    auto program = parser.parseProgram();
    EXPECT_TRUE(lexer.getErrors().empty());
    EXPECT_TRUE(parser.getErrors().empty());
    return program;
}
std::vector<LintDiagnostic> lintFriendly(const std::string& source) {
    auto program = parseFriendlyForLint(source);
    jtml::JtmlLinter linter;
    return linter.lint(program);
}
}

TEST(Linter, FriendlyActionLocalsAreLexicalDeclarationsAfterLoweringAndClone) {
    const std::string source =
        "jtml 2\n"
        "make Counter initial\n"
        "  let count = initial\n"
        "  get doubled = count + count\n"
        "  when advance step\n"
        "    let next: number = doubled + step\n"
        "    let items = [next]\n"
        "    for item in items\n"
        "      let local = item + next\n"
        "      count = local\n"
        "    if next > 0\n"
        "      let label = next\n"
        "      show label\n"
        "  button \"Advance\" click advance(1)\n"
        "page\n"
        "  Counter 2\n"
        "  Counter 3\n";
    auto program = parseFriendlyForLint(source);
    std::vector<std::unique_ptr<ASTNode>> cloned;
    for (const auto& node : program) cloned.push_back(node->clone());
    jtml::JtmlLinter linter;
    EXPECT_EQ(countErrors(linter.lint(program)), 0);
    EXPECT_EQ(countErrors(linter.lint(cloned)), 0);
    // Compatibility text is still assignment-based: runtime behavior is unchanged.
    EXPECT_NE(jtml::friendlyToClassic(source).find("_next ="), std::string::npos);
    EXPECT_EQ(jtml::friendlyToClassic(source).find("define __Counter_1_next"), std::string::npos);
}

TEST(Linter, FriendlyMissingLocalAndInitializerStillFailAtOriginalConstruct) {
    const auto diagnostics = lintFriendly(
        "jtml 2\n"
        "make Counter\n"
        "  when advance\n"
        "    let local = missing\n"
        "    ghost = local\n"
        "  button \"Advance\" click advance\n"
        "page\n"
        "  Counter\n");
    ASSERT_EQ(countErrors(diagnostics), 2);
    EXPECT_NE(diagnostics[0].message.find("missing"), std::string::npos);
    EXPECT_NE(diagnostics[1].message.find("ghost"), std::string::npos);
    EXPECT_EQ(diagnostics[0].line, 4);
    EXPECT_EQ(diagnostics[0].column, 5);
    EXPECT_EQ(diagnostics[1].line, 5);
    EXPECT_EQ(diagnostics[1].column, 5);
    for (const auto& diagnostic : diagnostics) {
        EXPECT_EQ(diagnostic.file, "lint-friendly.jtml");
        EXPECT_EQ(diagnostic.component, "Counter");
        EXPECT_FALSE(diagnostic.sourceExact); // Construct attribution, not exact token spans.
    }
}

TEST(Linter, FriendlyLocalsDoNotEscapeBlocksActionsOrComponents) {
    const auto diagnostics = lintFriendly(
        "jtml 2\n"
        "make First\n"
        "  when one\n"
        "    if true\n"
        "      let branch = 1\n"
        "      show branch\n"
        "    show branch\n"
        "    for item in [1]\n"
        "      let loopLocal = item\n"
        "      show loopLocal\n"
        "    show loopLocal\n"
        "    let private = 1\n"
        "  when two\n"
        "    show private\n"
        "  button \"One\" click one\n"
        "  button \"Two\" click two\n"
        "make Second\n"
        "  when three\n"
        "    show private\n"
        "  button \"Three\" click three\n"
        "page\n"
        "  First\n"
        "  Second\n");
    EXPECT_EQ(countErrors(diagnostics), 4);
}

TEST(Linter, FriendlyInitializerDoesNotBindItselfAndLocalTypesAreRetained) {
    auto diagnostics = lintFriendly(
        "jtml 2\n"
        "when run\n"
        "  let unknown = unknown + 1\n"
        "  let amount: number = \"wrong\"\n");
    ASSERT_EQ(countErrors(diagnostics), 2);
    EXPECT_EQ(diagnostics[0].code, "JTML_UNDEFINED_NAME");
    EXPECT_EQ(diagnostics[1].code, "JTML_TYPE_MISMATCH");
}

TEST(Linter, FriendlyLocalShadowingChecksInitializerInEnclosingScope) {
    const auto diagnostics = lintFriendly(
        "jtml 2\n"
        "let outer: number = 1\n"
        "when run param\n"
        "  let outer: string = \"local\"\n"
        "  let valid: string = outer\n"
        "  if true\n"
        "    let outer: number = 2\n"
        "    let valid: number = outer\n"
        "  let after: string = outer\n"
        "  let param = param + 1\n"
        "  show param\n");
    EXPECT_EQ(countErrors(diagnostics), 0);
}


TEST(Linter, UntypedFriendlyLocalShadowsOuterTypeAndFunctionArity) {
    const auto diagnostics = lintFriendly(
        "jtml 2\n"
        "let value: number = 1\n"
        "when callback first second\n"
        "  show first + second\n"
        "when run supplied\n"
        "  let value = supplied\n"
        "  let label: string = value\n"
        "  let callback = supplied\n"
        "  callback()\n");
    EXPECT_EQ(countErrors(diagnostics), 0);
}

TEST(Linter, ClassicAssignmentDoesNotIntroduceBinding) {
    EXPECT_EQ(countErrors(lintSource("missing = 1\\\\\nshow missing\\\\\n")), 2);
}


TEST(Linter, DeclaredComponentCallbackFixturePassesAndUnknownCallbackStillFails) {
    const auto root = std::filesystem::path(JTML_SOURCE_DIR) /
        "tests/fixtures/diagnostics/emitted-callbacks";
    jtml::JtmlLinter linter;
    auto program = jtml::cli::parseProgramFromFile((root / "binding-valid.jtml").string());
    EXPECT_EQ(countErrors(linter.lint(program)), 0);
    std::vector<std::unique_ptr<ASTNode>> cloned;
    for (const auto& node : program) cloned.push_back(node->clone());
    EXPECT_EQ(countErrors(linter.lint(cloned)), 0);
    program = jtml::cli::parseProgramFromFile((root / "binding-invalid.jtml").string());
    const auto diagnostics = linter.lint(program);
    ASSERT_EQ(countErrors(diagnostics), 1);
    EXPECT_NE(diagnostics[0].message.find("call to undefined function 'absent'"), std::string::npos);
    EXPECT_EQ(diagnostics[0].line, 3);
    EXPECT_EQ(diagnostics[0].column, 3);
    EXPECT_EQ(diagnostics[0].component, "Child");
    EXPECT_EQ(diagnostics[0].file, (root / "binding-invalid.jtml").string());
    EXPECT_FALSE(diagnostics[0].sourceExact);
}

TEST(Linter, ComponentCallbackPayloadArityAndMissingArgumentsRemainChecked) {
    const std::string source =
        "jtml 2\n"
        "make Sender value emits saved(item: number)\n"
        "  button \"Save\" click saved(value)\n"
        "page\n"
        "  Sender 7\n";
    EXPECT_EQ(countErrors(lintFriendly(source)), 0);
    std::string badArity = source;
    badArity.replace(badArity.find("saved(value)"), std::string("saved(value)").size(), "saved()");
    auto diagnostics = lintFriendly(badArity);
    ASSERT_EQ(countErrors(diagnostics), 1);
    EXPECT_EQ(diagnostics[0].code, "JTML_ARITY");
    EXPECT_EQ(diagnostics[0].line, 3);
    EXPECT_EQ(diagnostics[0].component, "Sender");
    std::string missing = source;
    missing.replace(missing.find("saved(value)"), std::string("saved(value)").size(), "saved(absent)");
    diagnostics = lintFriendly(missing);
    ASSERT_EQ(countErrors(diagnostics), 1);
    EXPECT_EQ(diagnostics[0].code, "JTML_UNDEFINED_NAME");
    EXPECT_NE(diagnostics[0].message.find("absent"), std::string::npos);
}

TEST(Linter, ComponentCallbacksDoNotLeakToNestedSiblingOrPageScopes) {
    const auto diagnostics = lintFriendly(
        "jtml 2\n"
        "make Leaf\n"
        "  button \"Invalid child\" click saved\n"
        "make Sender emits saved\n"
        "  button \"Valid parent\" click saved\n"
        "  Leaf\n"
        "make Sibling\n"
        "  button \"Invalid sibling\" click saved\n"
        "page\n"
        "  Sender\n"
        "  Sibling\n"
        "  button \"Invalid outside\" click saved\n");
    ASSERT_EQ(countErrors(diagnostics), 3);
    EXPECT_EQ(diagnostics[0].component, "Leaf");
    EXPECT_EQ(diagnostics[1].component, "Sibling");
    EXPECT_EQ(diagnostics[2].component, "");
    EXPECT_EQ(diagnostics[0].line, 3);
    EXPECT_EQ(diagnostics[1].line, 8);
    EXPECT_EQ(diagnostics[2].line, 12);
    for (const auto& diagnostic : diagnostics) {
        EXPECT_NE(diagnostic.message.find("call to undefined function 'saved'"), std::string::npos);
    }
}

TEST(Linter, ComponentCallbackContractsAreDistinctAndShadowOuterFunctions) {
    const auto diagnostics = lintFriendly(
        "jtml 2\n"
        "when saved\n"
        "  show 0\n"
        "make Zero emits saved\n"
        "  button \"Valid zero\" click saved\n"
        "make One emits saved(item: number)\n"
        "  button \"Valid one\" click saved(7)\n"
        "  button \"Invalid one\" click saved\n"
        "page\n"
        "  Zero\n"
        "  One\n"
        "  button \"Valid global\" click saved\n");
    ASSERT_EQ(countErrors(diagnostics), 1);
    EXPECT_EQ(diagnostics[0].code, "JTML_ARITY");
    EXPECT_EQ(diagnostics[0].component, "One");
    EXPECT_EQ(diagnostics[0].line, 8);
}

TEST(Linter, ActionBindingsShadowComponentCallbackArity) {
    const auto diagnostics = lintFriendly(
        "jtml 2\n"
        "make Sender emits saved(item: number)\n"
        "  when run saved\n"
        "    saved()\n"
        "  button \"Run\" click saved(7)\n"
        "page\n"
        "  Sender\n");
    // Action parameters already have unknown callable arity. Do not import the
    // outer emit contract through a nearer lexical binding of the same name.
    EXPECT_EQ(countErrors(diagnostics), 0);
}


TEST(Linter, KeyboardPayloadArityPreservesOrdinaryCallChecks) {
    for (const std::string event : {"keydown", "keyup", "key-down", "key-up"}) {
        auto valid = lintFriendly("jtml 2\nmake Editor\n  when key payload\n    show payload\n  input " + event + " key\npage\n  Editor\n");
        EXPECT_EQ(countErrors(valid), 0) << event;
        auto invalid = lintFriendly("jtml 2\nmake Editor\n  when key\n    show 1\n  input " + event + " key\npage\n  Editor\n");
        ASSERT_EQ(countErrors(invalid), 1) << event;
        EXPECT_EQ(invalid.front().code, "JTML_ARITY");
        EXPECT_EQ(invalid.front().line, 5);
        EXPECT_EQ(invalid.front().component, "Editor");
        auto excessive = lintFriendly("jtml 2\nwhen key payload\n  show payload\npage\n  input " + event + " key(1)\n");
        EXPECT_EQ(countErrors(excessive), 1) << event;
    }
    EXPECT_EQ(countErrors(lintFriendly("jtml 2\nwhen key payload\n  show payload\npage\n  button \"Go\" click key\n")), 1);
    EXPECT_EQ(countErrors(lintFriendly("jtml 2\nwhen key payload\n  show payload\nkey()\n")), 1);
}

TEST(Linter, ActionWhileInsideComponentDoesNotHideTemplateWhile) {
    auto d = lintFriendly("jtml 2\nmake Counter\n  let count = 0\n  when advance\n    if count < 2\n      while count < 2\n        count += 1\n  while count < 2\n    show count\n  button \"Go\" click advance\npage\n  Counter\n");
    EXPECT_EQ(countErrors(d), 0);
    ASSERT_EQ(d.size(), 1u);
    EXPECT_EQ(d.front().code, "JTML_TEMPLATE_WHILE");
    EXPECT_EQ(d.front().line, 8);
    EXPECT_EQ(d.front().component, "Counter");
}


TEST(Linter, StandaloneCallbackExpressionsPreserveCallerArguments) {
    for (const std::string argument : {"task", "{ \"id\": \"task-1\" }", "(task)", "tasks[0]", "identity(task)"}) {
        auto d = lintFriendly("jtml 2\nwhen receive value\n  show value\nwhen identity value\n  return value\nlet task = { \"id\": \"task-1\" }\nlet tasks = [task]\nmake Sender record\n  when send\n    receive(record.id)\n  button \"Send\" click send\npage\n  Sender " + argument + "\n");
        EXPECT_EQ(countErrors(d), 0) << argument;
    }
    auto missing = lintFriendly("jtml 2\nwhen receive value\n  show value\nmake Sender record\n  when send\n    receive(absent.id)\n  button \"Send\" click send\npage\n  Sender { \"id\": 1 }\n");
    ASSERT_EQ(countErrors(missing), 1);
    EXPECT_EQ(missing.front().line, 6);
    EXPECT_EQ(missing.front().component, "Sender");
    EXPECT_EQ(missing.front().code, "JTML_UNDEFINED_NAME");
}

TEST(Linter, StandaloneCallsBindLocalsAndPreserveLiteralAndMemberNames) {
    auto d = lintFriendly("jtml 2\nwhen receive value\n  show value\nmake Sender record\n  let id = 99\n  when send\n    let trimmed = record.id\n    receive(trimmed)\n    receive(\"trimmed\")\n    receive({ \"trimmed\": trimmed }.trimmed)\n    receive([record.id][0])\n  button \"Send\" click send\nlet task = { \"id\": 1 }\npage\n  Sender task\n  Sender task\n");
    EXPECT_EQ(countErrors(d), 0);
    auto escaped = lintFriendly("jtml 2\nwhen receive value\n  show value\nmake Sender\n  when first\n    let trimmed = 1\n    receive(trimmed)\n  when second\n    receive(trimmed)\n  button \"Send\" click second\npage\n  Sender\n");
    ASSERT_EQ(countErrors(escaped), 1);
    EXPECT_EQ(escaped.front().line, 9);
}


TEST(Linter, InputSettersRetainOwnerAndInputOriginThroughClone) {
    const std::string source = "jtml 2\nmake Editor\n  let text = \"\"\n  box\n    input into text\n    input into text\npage\n  Editor\n  Editor\n";
    auto program = parseFriendlyForLint(source);
    jtml::JtmlLinter linter;
    EXPECT_EQ(countErrors(linter.lint(program)), 0);
    std::vector<std::unique_ptr<ASTNode>> clones;
    for (const auto& node : program) clones.push_back(node->clone());
    EXPECT_EQ(countErrors(linter.lint(clones)), 0);
    auto invalid = lintFriendly("jtml 2\nmake Editor\n  input into missing\npage\n  Editor\n");
    ASSERT_EQ(countErrors(invalid), 2);
    for (const auto& d : invalid) {
        EXPECT_EQ(d.line, 3);
        EXPECT_EQ(d.column, 3);
        EXPECT_EQ(d.component, "Editor");
        EXPECT_EQ(d.file, "lint-friendly.jtml");
        EXPECT_FALSE(d.sourceExact);
    }
}

TEST(Linter, InputSettersCannotBorrowSiblingStateOrEscapeToModule) {
    auto d = lintFriendly("jtml 2\nmake Owner\n  let text = \"\"\n  input into text\nmake Other\n  input into text\npage\n  Owner\n  Other\nsetOwner1Text(\"escaped\")\n");
    EXPECT_GE(countErrors(d), 3);
    bool escaped = false;
    for (const auto& diag : d) {
        if (diag.line == 10) escaped = true;
        if (diag.line == 6) EXPECT_EQ(diag.component, "Other");
    }
    EXPECT_TRUE(escaped);
}
