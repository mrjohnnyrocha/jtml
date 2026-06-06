#include "jtml/friendly.h"
#include "jtml/language_catalog.h"
#include "jtml/lexer.h"
#include "jtml/linter.h"
#include "jtml/parser.h"
#include "jtml/transpiler.h"
#include "json.hpp"

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

struct StudioReferenceSection {
    std::string title;
    std::vector<std::string> terms;
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
    const auto samplesStart = shell.find("let SAMPLES = [");
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

std::vector<StudioSample> loadExternalStudioSamples() {
    const auto root = std::filesystem::path(JTML_SOURCE_DIR);
    const auto manifestPath = root / "studio" / "samples" / "manifest.json";
    const auto manifest = nlohmann::json::parse(readFile(manifestPath));
    std::vector<StudioSample> samples;
    for (const auto& item : manifest) {
        const auto file = root / "studio" / "samples" / item.value("file", item.value("name", ""));
        samples.push_back({
            item.value("name", file.filename().string()),
            readFile(file),
        });
    }
    return samples;
}

std::vector<StudioReferenceSection> loadExternalStudioReference() {
    const auto root = std::filesystem::path(JTML_SOURCE_DIR);
    const auto catalogPath = root / "studio" / "reference" / "catalog.json";
    const auto catalog = nlohmann::json::parse(readFile(catalogPath));
    std::vector<StudioReferenceSection> sections;
    for (const auto& section : catalog) {
        StudioReferenceSection out;
        out.title = section.value("title", "");
        for (const auto& row : section.value("rows", nlohmann::json::array())) {
            out.terms.push_back(row.value("term", ""));
            EXPECT_FALSE(row.value("description", "").empty()) << out.title;
        }
        sections.push_back(std::move(out));
    }
    return sections;
}

nlohmann::json loadExternalStudioSidebarCatalog() {
    const auto root = std::filesystem::path(JTML_SOURCE_DIR);
    return nlohmann::json::parse(readFile(root / "studio" / "sidebar" / "catalog.json"));
}

} // namespace

TEST(StudioSamples, AllExternalExamplesParseLintAndTranspile) {
    const auto samples = loadExternalStudioSamples();
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
    const auto samples = loadExternalStudioSamples();
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

TEST(StudioSamples, CoreSamplesUseSemanticUiPrimitives) {
    const auto samples = loadExternalStudioSamples();
    const std::set<std::string> coreNames = {
        "counter.jtml",
        "form.jtml",
        "dashboard.jtml",
        "fetch.jtml",
        "store.jtml",
        "effects.jtml",
    };

    std::map<std::string, std::string> byName;
    for (const auto& sample : samples) {
        byName[sample.name] = sample.code;
    }

    for (const auto& name : coreNames) {
        ASSERT_TRUE(byName.count(name)) << "missing Studio sample: " << name;
        const auto& code = byName[name];
        EXPECT_NE(code.find("theme\n"), std::string::npos) << name;
        EXPECT_NE(code.find("panel title"), std::string::npos) << name;
        EXPECT_NE(code.find("metric "), std::string::npos) << name;
        EXPECT_NE(code.find("stack gap"), std::string::npos) << name;
    }
}

TEST(StudioSamples, NavigationSamplesUseSemanticPanels) {
    const auto samples = loadExternalStudioSamples();
    const std::set<std::string> navigationNames = {
        "routes.jtml",
        "redirect.jtml",
    };

    std::map<std::string, std::string> byName;
    for (const auto& sample : samples) {
        byName[sample.name] = sample.code;
    }

    for (const auto& name : navigationNames) {
        ASSERT_TRUE(byName.count(name)) << "missing Studio sample: " << name;
        const auto& code = byName[name];
        EXPECT_NE(code.find("theme\n"), std::string::npos) << name;
        EXPECT_NE(code.find("panel title"), std::string::npos) << name;
        EXPECT_NE(code.find("stack gap"), std::string::npos) << name;
        EXPECT_NE(code.find("route "), std::string::npos) << name;
    }
}

TEST(StudioSamples, MediaAndCompositionSamplesUseSemanticUiPrimitives) {
    const auto samples = loadExternalStudioSamples();
    const std::set<std::string> names = {
        "media.jtml",
        "charts.jtml",
        "animation.jtml",
        "components.jtml",
    };

    std::map<std::string, std::string> byName;
    for (const auto& sample : samples) {
        byName[sample.name] = sample.code;
    }

    for (const auto& name : names) {
        ASSERT_TRUE(byName.count(name)) << "missing Studio sample: " << name;
        const auto& code = byName[name];
        EXPECT_NE(code.find("theme\n"), std::string::npos) << name;
        EXPECT_NE(code.find("panel title"), std::string::npos) << name;
        EXPECT_NE(code.find("card"), std::string::npos) << name;
        EXPECT_NE(code.find("grid cols"), std::string::npos) << name;
        EXPECT_NE(code.find("stack gap"), std::string::npos) << name;
    }
}

TEST(StudioSamples, ShellKeepsEmbeddedFallbackAndLoadsExternalCatalog) {
    const auto embedded = extractStudioSamples();
    const auto external = loadExternalStudioSamples();
    ASSERT_EQ(embedded.size(), external.size());

    const auto shellPath = std::filesystem::path(JTML_SOURCE_DIR) / "cli" / "studio_shell.cpp";
    const std::string shell = readFile(shellPath);
    EXPECT_NE(shell.find("/api/studio/samples"), std::string::npos);
    EXPECT_NE(shell.find("workspace = loadWorkspace();"), std::string::npos);
    EXPECT_NE(shell.find("let SAMPLES = ["), std::string::npos);
}

TEST(StudioReference, ExternalCatalogCoversModernLanguageSurface) {
    const auto sections = loadExternalStudioReference();
    ASSERT_GE(sections.size(), 10u);

    std::string all;
    for (const auto& section : sections) {
        EXPECT_FALSE(section.title.empty());
        EXPECT_GE(section.terms.size(), 3u) << section.title;
        all += section.title + "\n";
        for (const auto& term : section.terms) all += term + "\n";
    }

    for (const auto& expected : {
        "jtml 2",
        "Compatibility backend",
        "let x = expr",
        "get x = expr",
        "button \"Save\" click save",
        "for item in items",
        "video src \"demo.mp4\" controls into player",
        "chart bar data rows by label value total",
        "let users = fetch \"/api/users\"",
        "route \"/\" as Home",
        "guard \"/admin\" require signedIn else \"/login\"",
        "make Card title",
        "store auth",
        "theme",
        "extern notify from \"host.notify\"",
        "Run / Cmd+Enter",
    }) {
        EXPECT_NE(all.find(expected), std::string::npos) << expected;
    }
}

TEST(StudioReference, ShellLoadsExternalReferenceCatalogWithFallbackMarkup) {
    const auto shellPath = std::filesystem::path(JTML_SOURCE_DIR) / "cli" / "studio_shell.cpp";
    const std::string shell = readFile(shellPath);

    EXPECT_NE(shell.find("/api/studio/reference"), std::string::npos);
    EXPECT_NE(shell.find("let referenceCatalog = [];"), std::string::npos);
    EXPECT_NE(shell.find("function renderReferenceCatalog()"), std::string::npos);
    EXPECT_NE(shell.find("id=\"reference-body\""), std::string::npos);
    EXPECT_NE(shell.find("Which syntax should I choose?"), std::string::npos);
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
    const std::string reference = readFile(root / "docs" / "reference" / "language-reference.md");
    const std::string aiContract = readFile(root / "docs" / "reference" / "ai-authoring-contract.md");
    const std::string lspGuide = readFile(root / "docs" / "tooling" / "language-server.md");
    const std::string cliUsage = readFile(root / "cli" / "util.cpp");

    for (const auto& text : {docsIndex, aiContract, lspGuide}) {
        EXPECT_NE(text.find("jtml keywords --json"), std::string::npos);
    }
    EXPECT_NE(docsIndex.find("jtml ui --json"), std::string::npos);
    EXPECT_NE(reference.find("jtml ui --json"), std::string::npos);
    EXPECT_NE(cliUsage.find("jtml keywords [--json]"), std::string::npos);
    EXPECT_NE(cliUsage.find("jtml ui [--json]"), std::string::npos);
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

TEST(StudioShell, SidebarSearchRevealsFullExampleLibrary) {
    const auto shellPath = std::filesystem::path(JTML_SOURCE_DIR) / "cli" / "studio_shell.cpp";
    const std::string shell = readFile(shellPath);
    const auto catalog = loadExternalStudioSidebarCatalog();

    ASSERT_TRUE(catalog.contains("sampleCategories"));
    ASSERT_TRUE(catalog.contains("pinnedTemplates"));
    EXPECT_GE(catalog["sampleCategories"].size(), 5u);
    EXPECT_GE(catalog["pinnedTemplates"].size(), 7u);
    EXPECT_NE(shell.find("/api/studio/sidebar"), std::string::npos);
    EXPECT_NE(shell.find("let studioCatalog = {"), std::string::npos);
    EXPECT_NE(shell.find("if (!query && !pinnedTemplates.has(s.name)) return;"), std::string::npos);
    EXPECT_NE(shell.find("SAMPLES.forEach((s, i) =>"), std::string::npos);
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
