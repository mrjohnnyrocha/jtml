#include "jtml/friendly.h"
#include "jtml/language_catalog.h"
#include "jtml/lexer.h"
#include "jtml/linter.h"
#include "jtml/parser.h"
#include "jtml/transpiler.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <map>
#include <stdexcept>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct StudioSample {
    std::string name;
    std::string code;
};

std::string readFile(const std::filesystem::path& path) {
    std::ifstream in(path);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

std::vector<StudioSample> extractStudioSamples() {
    const auto shellPath = std::filesystem::path(JTML_SOURCE_DIR) / "cli" / "studio_shell.cpp";
    const std::string shell = readFile(shellPath);
    const auto samplesStart = shell.find("const SAMPLES = [");
    const auto samplesEnd = shell.find("];", samplesStart);
    EXPECT_NE(samplesStart, std::string::npos) << "Studio shell samples not found";
    EXPECT_NE(samplesEnd, std::string::npos) << "Studio shell sample array end not found";
    if (samplesStart == std::string::npos || samplesEnd == std::string::npos) {
        throw std::runtime_error("Studio shell samples not found");
    }

    std::vector<StudioSample> samples;
    size_t cursor = samplesStart;
    while (cursor != std::string::npos && cursor < samplesEnd) {
        const auto nameKey = shell.find("name: \"", cursor);
        if (nameKey == std::string::npos || nameKey >= samplesEnd) break;
        const auto nameStart = nameKey + std::string("name: \"").size();
        const auto nameEnd = shell.find('"', nameStart);
        if (nameEnd == std::string::npos) throw std::runtime_error("Malformed Studio sample name");

        const auto codeKey = shell.find("code: `", nameEnd);
        if (codeKey == std::string::npos) {
            throw std::runtime_error("Malformed Studio sample code: " +
                                     shell.substr(nameStart, nameEnd - nameStart));
        }
        const auto codeStart = codeKey + std::string("code: `").size();
        size_t codeEnd = codeStart;
        while (codeEnd < shell.size()) {
            if (shell[codeEnd] == '`' && (codeEnd == codeStart || shell[codeEnd - 1] != '\\')) break;
            ++codeEnd;
        }
        if (codeEnd >= shell.size()) {
            throw std::runtime_error("Unterminated Studio sample code: " +
                                     shell.substr(nameStart, nameEnd - nameStart));
        }

        samples.push_back({shell.substr(nameStart, nameEnd - nameStart),
                           shell.substr(codeStart, codeEnd - codeStart)});
        cursor = codeEnd + 1;
    }
    return samples;
}

} // namespace

TEST(StudioSamples, AllEmbeddedExamplesParseLintAndTranspile) {
    const auto samples = extractStudioSamples();
    ASSERT_GE(samples.size(), 10u);

    for (const auto& sample : samples) {
        const std::string normalized = jtml::normalizeSourceSyntax(sample.code, jtml::SyntaxMode::Auto);

        Lexer lexer(normalized);
        auto tokens = lexer.tokenize();
        ASSERT_TRUE(lexer.getErrors().empty()) << sample.name << "\n" << normalized;

        Parser parser(std::move(tokens));
        auto program = parser.parseProgram();
        ASSERT_TRUE(parser.getErrors().empty()) << sample.name << "\n" << normalized;

        jtml::JtmlLinter linter;
        int errors = 0;
        for (const auto& diagnostic : linter.lint(program)) {
            if (diagnostic.severity == LintDiagnostic::Severity::Error) ++errors;
        }
        EXPECT_EQ(errors, 0) << sample.name;

        JtmlTranspiler transpiler;
        const std::string html = transpiler.transpile(program);
        EXPECT_NE(html.find("<!DOCTYPE html>"), std::string::npos) << sample.name;
    }
}

TEST(StudioSamples, CoversProductionFeatureFamilies) {
    const auto samples = extractStudioSamples();
    std::set<std::string> names;
    for (const auto& sample : samples) names.insert(sample.name);

    const std::vector<std::string> expected = {
        "counter.jtml",
        "form.jtml",
        "dashboard.jtml",
        "fetch.jtml",
        "fetch-post.jtml",
        "store.jtml",
        "effects.jtml",
        "routes.jtml",
        "media.jtml",
        "charts.jtml",
        "animation.jtml",
        "components.jtml",
    };

    for (const auto& name : expected) {
        EXPECT_TRUE(names.count(name)) << "missing Studio sample: " << name;
    }
}

TEST(StudioShell, HighlightsModernFriendlySyntax) {
    const auto shellPath = std::filesystem::path(JTML_SOURCE_DIR) / "cli" / "studio_shell.cpp";
    const std::string shell = readFile(shellPath);

    EXPECT_NE(shell.find("click|input|change|submit"), std::string::npos);
    EXPECT_NE(shell.find("load|guard|require"), std::string::npos);
    EXPECT_NE(shell.find("active-class"), std::string::npos);
    EXPECT_NE(shell.find("data-jtml-route-guard"), std::string::npos);
    EXPECT_NE(shell.find("shell|topbar|sidebar|content|panel|card|metric"), std::string::npos);
    EXPECT_NE(shell.find("cols|gap|pad|radius|shadow|tone|align|justify|width|surface"), std::string::npos);
}

TEST(LanguageSurface, MiniReferenceAndEditorGrammarCoverModernKeywords) {
    const auto root = std::filesystem::path(JTML_SOURCE_DIR);
    const std::string readme = readFile(root / "README.md");
    const std::string reference = readFile(root / "docs" / "reference" / "language-reference.md");
    const std::string studio = readFile(root / "cli" / "studio_shell.cpp");
    const std::string vscode = readFile(root / "editors" / "vscode" / "syntaxes" / "jtml.tmLanguage.json");

    const auto required = jtml::friendlyKeywords();

    for (const auto& keyword : required) {
        EXPECT_NE(readme.find(keyword), std::string::npos) << "README missing " << keyword;
        EXPECT_NE(reference.find(keyword), std::string::npos) << "language reference missing " << keyword;
        EXPECT_NE(studio.find(keyword), std::string::npos) << "Studio reference/highlighter missing " << keyword;
        EXPECT_NE(vscode.find(keyword), std::string::npos) << "VS Code grammar missing " << keyword;
    }

    EXPECT_NE(reference.find("## Friendly Keyword Index"), std::string::npos);
    const std::string lsp = readFile(root / "cli" / "cmd_lsp.cpp");
    EXPECT_NE(lsp.find("languageCatalog().friendlyGroups"), std::string::npos);
    EXPECT_NE(studio.find("Semantic UI"), std::string::npos);
    EXPECT_NE(studio.find("html raw"), std::string::npos);
    EXPECT_NE(studio.find("css raw"), std::string::npos);
    EXPECT_EQ(readme.find("Lowered Classic"), std::string::npos);
    EXPECT_EQ(reference.find("Lowered Classic"), std::string::npos);
}

TEST(LanguageSurface, ToolingDocsPointToKeywordCatalog) {
    const auto root = std::filesystem::path(JTML_SOURCE_DIR);
    const std::string docsIndex = readFile(root / "docs" / "README.md");
    const std::string aiContract = readFile(root / "docs" / "reference" / "ai-authoring-contract.md");
    const std::string lspGuide = readFile(root / "docs" / "tooling" / "language-server.md");
    const std::string cliUsage = readFile(root / "cli" / "util.cpp");

    for (const auto& text : {docsIndex, aiContract, lspGuide}) {
        EXPECT_NE(text.find("jtml keywords --json"), std::string::npos);
    }
    EXPECT_NE(cliUsage.find("jtml keywords [--json]"), std::string::npos);
}

TEST(StudioShell, SeparatesNavigationWorkspaceAndDocumentTools) {
    const auto shellPath = std::filesystem::path(JTML_SOURCE_DIR) / "cli" / "studio_shell.cpp";
    const std::string shell = readFile(shellPath);

    EXPECT_NE(shell.find("id=\"project-tree\""), std::string::npos);
    EXPECT_NE(shell.find("id=\"new-project\""), std::string::npos);
    EXPECT_NE(shell.find("id=\"new-folder\""), std::string::npos);
    EXPECT_NE(shell.find("id=\"new-file\""), std::string::npos);
    EXPECT_NE(shell.find("id=\"workspace-create\""), std::string::npos);
    EXPECT_NE(shell.find("class=\"editor-tools\""), std::string::npos);
    EXPECT_NE(shell.find("id=\"inspector-diagnostics\""), std::string::npos);
    EXPECT_NE(shell.find("id=\"inspector-artifacts\""), std::string::npos);
    EXPECT_NE(shell.find("id=\"inspector-reference\""), std::string::npos);
    EXPECT_NE(shell.find("Templates"), std::string::npos);
    EXPECT_NE(shell.find(".sb-section[hidden] { display: none; }"), std::string::npos);
    EXPECT_NE(shell.find("data-mode=\"diagnostics\""), std::string::npos);
    EXPECT_EQ(shell.find("header #run"), std::string::npos);
    EXPECT_NE(shell.find("selectLesson(0)"), std::string::npos);
    EXPECT_NE(shell.find("workspace:"), std::string::npos);
    EXPECT_EQ(shell.find("prompt("), std::string::npos);
}

TEST(StudioShell, DefaultsToFriendlyAndHtmlArtifacts) {
    const auto shellPath = std::filesystem::path(JTML_SOURCE_DIR) / "cli" / "studio_shell.cpp";
    const std::string shell = readFile(shellPath);

    EXPECT_NE(shell.find("let artifactMode    = \"html\";"), std::string::npos);
    EXPECT_NE(shell.find("id=\"artifact-html-tab\">HTML</button>"), std::string::npos);
    EXPECT_NE(shell.find("id=\"artifact-classic-tab\">Compatibility IR</button>"), std::string::npos);
    EXPECT_NE(shell.find("Friendly JTML 2"), std::string::npos);
    EXPECT_NE(shell.find("Compatibility backend"), std::string::npos);
    EXPECT_EQ(shell.find(">Lowered Classic<"), std::string::npos);
    EXPECT_EQ(shell.find("Artifacts → Classic"), std::string::npos);
}
