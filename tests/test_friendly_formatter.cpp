// tests/test_friendly_formatter.cpp — Tests for the Friendly JTML formatter
// (AST → Friendly v2 syntax) and the classic-to-friendly migration round-trip.
#include "jtml/friendly_formatter.h"
#include "jtml/friendly.h"
#include "jtml/formatter.h"
#include "jtml/lexer.h"
#include "jtml/parser.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>

namespace {

std::string readFile(const std::string& path) {
    std::ifstream ifs(path);
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

// Parse classic source → AST → friendly format
std::string classicToFriendly(const std::string& classicSrc) {
    Lexer lex(classicSrc);
    auto tokens = lex.tokenize();
    if (!lex.getErrors().empty()) return {};
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    if (!parser.getErrors().empty()) return {};
    JtmlFriendlyFormatter fmt;
    return fmt.format(program);
}

// Parse friendly source → normalize → parse → re-check
bool friendlyRoundTrips(const std::string& friendlySrc) {
    std::string classic = jtml::normalizeSourceSyntax(friendlySrc, jtml::SyntaxMode::Friendly);
    Lexer lex(classic);
    auto tokens = lex.tokenize();
    if (!lex.getErrors().empty()) return false;
    Parser parser(std::move(tokens));
    (void)parser.parseProgram();
    return parser.getErrors().empty();
}

// ---------------------------------------------------------------------------
// Output contains friendly header
// ---------------------------------------------------------------------------

TEST(FriendlyFormatter, OutputStartsWithJtml2Header) {
    std::string classic = "define x = 1\\\\\n";
    std::string friendly = classicToFriendly(classic);
    EXPECT_EQ(friendly.substr(0, 7), "jtml 2\n");
}

TEST(FriendlyFormatter, SourceFormatterPreservesHighLevelFriendlyConstructs) {
    std::string source =
        "jtml 2\n"
        "\n"
        "store auth   \n"
        "    let user = \"Ada\"\n"
        "    when logout\n"
        "        let user = \"\"\n"
        "\n"
        "let users = fetch \"/api/users\" refresh reloadUsers\n"
        "route \"*\" as NotFound\n";

    std::string formatted = jtml::formatFriendlySource(source);
    EXPECT_NE(formatted.find("store auth\n"), std::string::npos);
    EXPECT_NE(formatted.find("  let user = \"Ada\"\n"), std::string::npos);
    EXPECT_NE(formatted.find("  when logout\n"), std::string::npos);
    EXPECT_NE(formatted.find("    let user = \"\"\n"), std::string::npos);
    EXPECT_NE(formatted.find("let users = fetch \"/api/users\" refresh reloadUsers\n"), std::string::npos);
    EXPECT_NE(formatted.find("route \"*\" as NotFound\n"), std::string::npos);
    EXPECT_EQ(formatted.find("@template"), std::string::npos);
}

TEST(FriendlyFormatter, SourceFormatterAddsHeaderForFriendlyBody) {
    std::string formatted = jtml::formatFriendlySource(
        "let count = 0\n"
        "page\n"
        "    h1 \"Counter\"\n");

    EXPECT_EQ(formatted.substr(0, 7), "jtml 2\n");
    EXPECT_NE(formatted.find("  h1 \"Counter\"\n"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Keyword translations
// ---------------------------------------------------------------------------

TEST(FriendlyFormatter, DefineBecomesLet) {
    std::string friendly = classicToFriendly("define count = 0\\\\\n");
    EXPECT_NE(friendly.find("let count = 0"), std::string::npos);
    EXPECT_EQ(friendly.find("define"), std::string::npos);
}

TEST(FriendlyFormatter, ConstStaysConst) {
    std::string friendly = classicToFriendly("const PI = 3\\\\\n");
    EXPECT_NE(friendly.find("const PI = 3"), std::string::npos);
}

TEST(FriendlyFormatter, DeriveBecomesGet) {
    std::string friendly = classicToFriendly(
        "define x = 1\\\\\n"
        "derive doubled = x * 2\\\\\n");
    EXPECT_NE(friendly.find("get doubled = "), std::string::npos);
    EXPECT_EQ(friendly.find("derive"), std::string::npos);
}

TEST(FriendlyFormatter, FunctionBecomesWhen) {
    std::string friendly = classicToFriendly(
        "function increment()\\\\\n"
        "    define x = 1\\\\\n"
        "\\\\\n");
    EXPECT_NE(friendly.find("when increment"), std::string::npos);
    EXPECT_EQ(friendly.find("function"), std::string::npos);
}

TEST(FriendlyFormatter, ImportBecomesUse) {
    std::string friendly = classicToFriendly("import \"./lib.jtml\"\\\\\n");
    EXPECT_NE(friendly.find("use \"./lib.jtml\""), std::string::npos);
    EXPECT_EQ(friendly.find("import"), std::string::npos);
}

TEST(FriendlyFormatter, ShowPreserved) {
    std::string friendly = classicToFriendly("show \"hello\"\\\\\n");
    EXPECT_NE(friendly.find("show \"hello\""), std::string::npos);
}

// ---------------------------------------------------------------------------
// Control flow
// ---------------------------------------------------------------------------

TEST(FriendlyFormatter, IfElseFormattedWithoutParens) {
    std::string friendly = classicToFriendly(
        "if (x > 0)\\\\\n"
        "    show x\\\\\n"
        "\\\\\n"
        "else \\\\\n"
        "    show 0\\\\\n"
        "\\\\\n");
    EXPECT_NE(friendly.find("if x > 0"), std::string::npos);
    EXPECT_NE(friendly.find("else"), std::string::npos);
    // Should NOT have parentheses around condition in friendly syntax
    EXPECT_EQ(friendly.find("if ("), std::string::npos);
}

TEST(FriendlyFormatter, ForLoopFormattedCleanly) {
    std::string friendly = classicToFriendly(
        "for (i in items)\\\\\n"
        "    show i\\\\\n"
        "\\\\\n");
    EXPECT_NE(friendly.find("for i in items"), std::string::npos);
}

TEST(FriendlyFormatter, WhileLoopFormatted) {
    std::string friendly = classicToFriendly(
        "while (running)\\\\\n"
        "    show \"go\"\\\\\n"
        "\\\\\n");
    EXPECT_NE(friendly.find("while running"), std::string::npos);
}

TEST(FriendlyFormatter, TryCatchFinallyFormatted) {
    std::string friendly = classicToFriendly(
        "try\\\\\n"
        "    show 1\\\\\n"
        "\\\\\n"
        "except(e)\\\\\n"
        "    show e\\\\\n"
        "\\\\\n"
        "then\\\\\n"
        "    show \"done\"\\\\\n"
        "\\\\\n");
    EXPECT_NE(friendly.find("try"), std::string::npos);
    EXPECT_NE(friendly.find("catch e"), std::string::npos);
    EXPECT_NE(friendly.find("finally"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Element reverse-mapping
// ---------------------------------------------------------------------------

TEST(FriendlyFormatter, MainBecomesPage) {
    std::string friendly = classicToFriendly(
        "@main\\\\\n"
        "    show \"hi\"\\\\\n"
        "#\n");
    EXPECT_NE(friendly.find("page"), std::string::npos);
    EXPECT_EQ(friendly.find("@main"), std::string::npos);
}

TEST(FriendlyFormatter, DivBecomesBox) {
    std::string friendly = classicToFriendly(
        "@main\\\\\n"
        "    @div class=\"container\"\\\\\n"
        "        show \"hi\"\\\\\n"
        "    #\n"
        "#\n");
    EXPECT_NE(friendly.find("box class \"container\""), std::string::npos);
}

TEST(FriendlyFormatter, PBecomesText) {
    std::string friendly = classicToFriendly(
        "@main\\\\\n"
        "    @p\\\\\n"
        "        show \"paragraph\"\\\\\n"
        "    #\n"
        "#\n");
    EXPECT_NE(friendly.find("text"), std::string::npos);
}

TEST(FriendlyFormatter, ABecomesLink) {
    std::string friendly = classicToFriendly(
        "@main\\\\\n"
        "    @a href=\"/home\"\\\\\n"
        "        show \"Home\"\\\\\n"
        "    #\n"
        "#\n");
    EXPECT_NE(friendly.find("link"), std::string::npos);
}

TEST(FriendlyFormatter, UlBecomesList) {
    std::string friendly = classicToFriendly(
        "@main\\\\\n"
        "    @ul\\\\\n"
        "        @li\\\\\n"
        "            show \"item\"\\\\\n"
        "        #\n"
        "    #\n"
        "#\n");
    EXPECT_NE(friendly.find("list"), std::string::npos);
    EXPECT_NE(friendly.find("item"), std::string::npos);
}

TEST(FriendlyFormatter, OlBecomesListOrdered) {
    std::string friendly = classicToFriendly(
        "@main\\\\\n"
        "    @ol\\\\\n"
        "        @li\\\\\n"
        "            show \"step\"\\\\\n"
        "        #\n"
        "    #\n"
        "#\n");
    EXPECT_NE(friendly.find("list ordered"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Event and into reverse-mapping
// ---------------------------------------------------------------------------

TEST(FriendlyFormatter, OnClickBecomesClick) {
    std::string friendly = classicToFriendly(
        "@main\\\\\n"
        "    @button onClick=save()\\\\\n"
        "        show \"Save\"\\\\\n"
        "    #\n"
        "#\n");
    EXPECT_NE(friendly.find("click save"), std::string::npos);
    EXPECT_EQ(friendly.find("onClick"), std::string::npos);
}

TEST(FriendlyFormatter, FileInputReverseMapsToFileAlias) {
    std::string friendly = classicToFriendly(
        "@main\\\\\n"
        "    @input type=\"file\" aria-label=\"Choose image\" accept=\"image/*\" onChange=setSelected()\\\\\n"
        "    #\n"
        "#\n"
        "function setSelected(value)\\\\\n"
        "    selected = value\\\\\n"
        "\\\\\n");
    EXPECT_NE(friendly.find("file \"Choose image\" accept \"image/*\" into selected"), std::string::npos);
    EXPECT_EQ(friendly.find("type \"file\""), std::string::npos);
}

TEST(FriendlyFormatter, DropzoneReverseMapsToDropzoneAlias) {
    std::string friendly = classicToFriendly(
        "@main\\\\\n"
        "    @input type=\"file\" data-jtml-dropzone=\"true\" multiple aria-label=\"Drop media\" accept=\"image/*\" onChange=setAssets()\\\\\n"
        "    #\n"
        "#\n"
        "function setAssets(value)\\\\\n"
        "    assets = value\\\\\n"
        "\\\\\n");
    EXPECT_NE(friendly.find("dropzone \"Drop media\" accept \"image/*\" into assets"), std::string::npos);
    EXPECT_EQ(friendly.find("data-jtml-dropzone"), std::string::npos);
}

TEST(FriendlyFormatter, MediaControllerReverseMapsToInto) {
    std::string friendly = classicToFriendly(
        "@video src=\"/demo.mp4\" controls data-jtml-media-controller=\"player\"\\\\\n"
        "#\n");
    EXPECT_NE(friendly.find("video src \"/demo.mp4\" controls into player"), std::string::npos);
    EXPECT_EQ(friendly.find("data-jtml-media-controller"), std::string::npos);
}

TEST(FriendlyFormatter, ChartReverseMapsToFriendlyChartSyntax) {
    std::string friendly = classicToFriendly(
        "@svg role=\"img\" aria-label=\"Revenue\" data-jtml-chart=\"bar\" data-jtml-chart-data=\"rows\" data-jtml-chart-by=\"month\" data-jtml-chart-value=\"total\" data-jtml-chart-color=\"#2563eb\" width=\"640\" height=\"320\" viewBox=\"0 0 640 320\"\\\\\n"
        "#\n");
    EXPECT_NE(friendly.find("chart bar data rows by month value total label \"Revenue\" color \"#2563eb\""), std::string::npos);
    EXPECT_EQ(friendly.find("data-jtml-chart"), std::string::npos);
}

TEST(FriendlyFormatter, Scene3DReverseMapsToFriendlySceneSyntax) {
    std::string friendly = classicToFriendly(
        "@canvas data-jtml-scene3d=\"true\" role=\"img\" aria-label=\"Product model\" data-jtml-scene=\"productScene\" data-jtml-camera=\"orbit\" data-jtml-controls=\"orbit\" data-jtml-renderer=\"three\" data-jtml-scene3d-controller=\"sceneState\" width=\"640\" height=\"360\"\\\\\n"
        "#\n");
    EXPECT_NE(friendly.find("scene3d \"Product model\" scene productScene camera orbit controls orbit renderer three into sceneState width \"640\" height \"360\""), std::string::npos);
    EXPECT_EQ(friendly.find("data-jtml-scene3d"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Round-trip: classic → friendly → re-parse
// ---------------------------------------------------------------------------

TEST(FriendlyFormatter, MigratedOutputReParsesCleanly) {
    std::string classic =
        "define count = 0\\\\\n"
        "derive doubled = count * 2\\\\\n"
        "function increment()\\\\\n"
        "    count = count + 1\\\\\n"
        "\\\\\n"
        "@main\\\\\n"
        "    @h1\\\\\n"
        "        show \"Counter\"\\\\\n"
        "    #\n"
        "    show doubled\\\\\n"
        "    @button onClick=increment()\\\\\n"
        "        show \"Add\"\\\\\n"
        "    #\n"
        "#\n";

    std::string friendly = classicToFriendly(classic);
    ASSERT_FALSE(friendly.empty());
    EXPECT_TRUE(friendlyRoundTrips(friendly))
        << "Migrated output did not re-parse:\n" << friendly;
}

TEST(FriendlyFormatter, MigrationRoundTripOnBundledClassicExamples) {
    namespace fs = std::filesystem;
    fs::path examples = fs::path(__FILE__).parent_path().parent_path() / "examples";
    ASSERT_TRUE(fs::exists(examples));

    int checked = 0;
    for (const auto& entry : fs::directory_iterator(examples)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".jtml") continue;
        // Skip files that are already friendly syntax
        std::string src = readFile(entry.path().string());
        if (jtml::isFriendlySyntax(src) || jtml::looksLikeFriendlySyntax(src)) continue;

        std::string friendly = classicToFriendly(src);
        if (friendly.empty()) continue; // parse error in original

        bool ok = friendlyRoundTrips(friendly);
        EXPECT_TRUE(ok)
            << "Migration round-trip failed for " << entry.path().filename()
            << "\nFriendly output:\n" << friendly.substr(0, 500);
        if (ok) ++checked;
    }
    EXPECT_GT(checked, 0) << "no classic examples were round-tripped";
}

} // namespace
