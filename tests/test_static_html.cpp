#include "cli/browser_build.h"
#include "cli/commands.h"
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {
namespace fs = std::filesystem;
struct StaticHtml : testing::Test {
    fs::path root;
    void SetUp() override {
        root = fs::temp_directory_path() / ("jtml-static-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(root);
    }
    void TearDown() override { fs::remove_all(root); }
    void put(const std::string& path, const std::string& source) {
        fs::create_directories((root/path).parent_path()); std::ofstream(root/path) << source;
    }
    std::string read(const std::string& path) {
        std::ifstream f(root/path); return {std::istreambuf_iterator<char>(f), {}};
    }
    void build(const std::string& input = "index.jtml") {
        jtml::cli::Options o; o.inputFile=(root/input).string(); o.outputFile=(root/"out").string(); o.target="static";
        jtml::cli::cmdBuildStatic(o);
    }
};
TEST_F(StaticHtml, LiteralMarkupEscapesTextAndAttributesWithoutRuntime) {
    put("index.jtml", "jtml 2\npage\n  h1 \"Olá <script>&\\\"雪\" id \"title\"\n  a \"Go\" href \"./other.html#x\" aria-label \"A & B\"\n  button \"Passive\" disabled\n");
    build(); const auto html=read("out/index.html");
    EXPECT_NE(html.find("Olá &lt;script&gt;&amp;&quot;雪"),std::string::npos) << html;
    EXPECT_NE(html.find("id=\"title\""),std::string::npos);
    EXPECT_NE(html.find("aria-label=\"A &amp; B\""),std::string::npos);
    EXPECT_NE(html.find("href=\"./other.html#x\""),std::string::npos);
    EXPECT_EQ(html.find("<script"),std::string::npos); EXPECT_EQ(html.find("data-jtml"),std::string::npos);
    EXPECT_EQ(std::distance(fs::directory_iterator(root/"out"),fs::directory_iterator{}),1);
}
TEST_F(StaticHtml, ScopedForwardingDoesNotCaptureSiblingOrCallerParameters) {
    put("index.jtml", "jtml 2\nmake Leaf title\n  p title\nmake Parent title\n  Leaf title\n  Leaf \"inner\"\n  p title\npage\n  Parent \"outer\"\n  Parent \"sibling\"\n");
    build(); const auto html=read("out/index.html");
    EXPECT_NE(html.find("<p>outer</p>"),std::string::npos) << html;
    EXPECT_NE(html.find("<p>inner</p>"),std::string::npos);
    EXPECT_NE(html.find("<p>sibling</p>"),std::string::npos);
    build(); EXPECT_EQ(read("out/index.html"),html);
}
TEST_F(StaticHtml, PrivateDuplicateNamesRemainModuleQualifiedThroughReexports) {
    put("index.jtml", "jtml 2\nuse { A } from \"./barrel.jtml\"\nuse { B } from \"./b.jtml\"\npage\n  A \"caller\"\n  B\n");
    put("barrel.jtml", "jtml 2\nexport use { A } from \"./a.jtml\"\n");
    put("a.jtml", "jtml 2\nmake Item title\n  p title\nexport make A title\n  Item title\n");
    put("b.jtml", "jtml 2\nmake Item\n  p \"private B\"\nexport make B\n  Item\n");
    build(); const auto html=read("out/index.html");
    EXPECT_NE(html.find("<p>caller</p>"),std::string::npos) << html;
    EXPECT_NE(html.find("<p>private B</p>"),std::string::npos) << html;
}
TEST_F(StaticHtml, DynamicAndUnknownConstructsRejectWithoutReplacingPreviousOutput) {
    const std::vector<std::string> sources = {
        "let x = 1\npage\n  p x", "get x = 1\npage\n  p x", "when run\n  show 1\npage\n  p \"x\"",
        "page\n  if true\n    p \"x\"", "page\n  for x in [1]\n    p x", "page\n  while false\n    p \"x\"",
        "page\n  button \"Go\" click run", "make Box\n  button \"Go\" keydown run\npage\n  Box", "page\n  input \"X\" into x", "page\n  p unknown",
        "page\n  p (1 + 2)", "page\n  div data-jtml-static-call \"Box\"", "page\n  html raw \"<b>x</b>\"", "extern notify from \"host.notify\"\npage\n  p \"x\"",
        "let x = fetch \"/api/x\"\npage\n  p x", "store x = 1\npage\n  p \"x\"", "page\n  iframe src \"./x\"",
        "page\n  a \"Bad\" href \"javascript:alert(1)\"", "page\n  img src \"data:text/html,bad\"",
        "make Box\n  slot\npage\n  Box", "make Box\n  let x = 1\n  p x\npage\n  Box",
        "make Box\n  get x = 1\n  p x\npage\n  Box", "make Box\n  when action\n    show 1\n  p \"x\"\npage\n  Box",
        "make Box\n  if true\n    p \"x\"\npage\n  Box", "make Box title\n  p title\npage\n  Box unknown"
    };
    put("out/keep.txt", "previous accepted output");
    for(const auto& source:sources) {
        SCOPED_TRACE(source); put("index.jtml","jtml 2\n"+source+"\n");
        EXPECT_THROW(build(),std::exception);
        EXPECT_EQ(read("out/keep.txt"),"previous accepted output");
        EXPECT_FALSE(fs::exists(root/"out/index.html"));
        for(const auto& p:fs::directory_iterator(root)) EXPECT_EQ(p.path().filename().string().find(".static-"),std::string::npos);
    }
}
TEST_F(StaticHtml, HiddenImportedBehaviorHasOriginalSourceAndComponentContext) {
    put("index.jtml", "jtml 2\nuse { Dangerous } from \"./child.jtml\"\npage\n  Dangerous\n");
    put("child.jtml", "jtml 2\nexport make Dangerous\n  let secret = 1\n  p secret\n");
    try { build(); FAIL()<<"expected rejection"; }
    catch(const std::exception& e) {
        const std::string message=e.what(); EXPECT_NE(message.find("child.jtml:3:3"),std::string::npos)<<message;
        EXPECT_NE(message.find("Dangerous"),std::string::npos)<<message;
        EXPECT_NE(message.find("--target browser"),std::string::npos)<<message;
    }
    EXPECT_FALSE(fs::exists(root/"out"));
}
TEST_F(StaticHtml, ImportedInitializersAndComponentCyclesCannotBeDropped) {
    put("index.jtml", "jtml 2\nuse { Box } from \"./child.jtml\"\npage\n  Box\n");
    put("child.jtml", "jtml 2\nlet hidden = 1\nexport make Box\n  p \"literal\"\n");
    EXPECT_THROW(build(),std::exception);
    put("index.jtml", "jtml 2\nmake A\n  B\nmake B\n  A\npage\n  A\n");
    EXPECT_THROW(build(),std::exception); EXPECT_FALSE(fs::exists(root/"out"));
}
TEST_F(StaticHtml, StaticTargetCannotReplaceSourceOrAcceptBrowserModeFlags) {
    put("index.jtml","jtml 2\npage\n  p \"literal\"\n");
    jtml::cli::Options o;o.inputFile=(root/"index.jtml").string();o.outputFile=root.string();o.target="static";
    EXPECT_THROW(jtml::cli::cmdBuildStatic(o),std::exception);EXPECT_TRUE(fs::exists(root/"index.jtml"));
    o.outputFile=(root/"out").string();o.compat=true;EXPECT_THROW(jtml::cli::cmdBuildStatic(o),std::exception);
}
TEST_F(StaticHtml, QuotedExamplesAreLiteralContentNotEventBindings) {
    put("index.jtml", "jtml 2\nmake Example\n  code \"button click run\\nlet x = 1\"\npage\n  Example\n");
    build();EXPECT_NE(read("out/index.html").find("button click run"),std::string::npos);
    put("index.jtml", "jtml 2\nmake Example\n  button \"Run\" click run\npage\n  Example\n");
    EXPECT_THROW(build(),std::exception);
}
TEST_F(StaticHtml, ConflictingPublicImportsCannotSelectAComponentByOrder) {
    put("index.jtml", "jtml 2\nuse Box from \"./a.jtml\"\nuse Box from \"./b.jtml\"\nmake Parent\n  Box\npage\n  Parent\n");
    put("a.jtml", "jtml 2\nexport make Box\n  p \"a\"\n");
    put("b.jtml", "jtml 2\nexport make Box\n  p \"b\"\n");
    EXPECT_THROW(build(),std::exception);EXPECT_FALSE(fs::exists(root/"out"));
}
TEST_F(StaticHtml, ExistingBrowserAndLegacyHtmlContractsRemainExplicit) {
    put("index.jtml", "jtml 2\npage\n  p \"legacy\"\n");
    jtml::cli::Options o;o.inputFile=(root/"index.jtml").string();o.outputFile=(root/"legacy.html").string();o.target="html";
    EXPECT_EQ(jtml::cli::cmdExport(o),0);
    EXPECT_NE(read("legacy.html").find("<script>"),std::string::npos);
    EXPECT_NE(read("legacy.html").find("{{\"legacy\"}}"),std::string::npos);
    o.outputFile=(root/"browser").string();o.target="browser";EXPECT_EQ(jtml::cli::cmdBuild(o),0);
    for(const auto& file:{"index.html","jtml-runtime.js","components/index.js","app.js"})
        EXPECT_TRUE(fs::is_regular_file(root/"browser"/file));
    EXPECT_NE(read("browser/index.html").find("__jtml_client_manifest"),std::string::npos);
    EXPECT_NE(read("browser/app.js").find("cleanProduction: true"),std::string::npos);
}
TEST_F(StaticHtml, NumericLiteralsHaveExactBrowserStringSemantics) {
    put("index.jtml", "jtml 2\npage\n  p 1\n  p true\n  p -1\n  p -0\n");build();
    EXPECT_NE(read("out/index.html").find("<p>1</p>"),std::string::npos);
    EXPECT_NE(read("out/index.html").find("<p>-1</p>"),std::string::npos);
    EXPECT_NE(read("out/index.html").find("<p>0</p>"),std::string::npos);
    put("index.jtml", "jtml 2\npage\n  p 1.5\n");EXPECT_THROW(build(),std::exception);
}
TEST_F(StaticHtml, DuplicateLocalDefinitionsAndParametersFailClosed) {
    put("index.jtml", "jtml 2\nmake Box\n  p \"first\"\nmake Box\n  p \"second\"\npage\n  Box\n");
    EXPECT_THROW(build(),std::exception);
    put("index.jtml", "jtml 2\nmake Box title title\n  p title\npage\n  Box \"a\" \"b\"\n");
    EXPECT_THROW(build(),std::exception);
}
TEST_F(StaticHtml, DirectoryBuildCopiesPublicAssetsSkipsSymlinksAndReplacesAtomically) {
    put("index.jtml","jtml 2\npage\n  p \"literal\"\n");put("public/a.css","p {color:blue}");
    fs::create_symlink(root/"index.jtml",root/"public/link.jtml");put("out/obsolete.txt","old");
    build(".");EXPECT_EQ(read("out/public/a.css"),"p {color:blue}");
    EXPECT_FALSE(fs::exists(root/"out/public/link.jtml"));EXPECT_FALSE(fs::exists(root/"out/obsolete.txt"));
}
}
