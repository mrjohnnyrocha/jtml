// cli/cmd_lsp.cpp — minimal Language Server Protocol implementation.
//
// This is intentionally dependency-free: editors can launch `jtml lsp` over
// stdio and get parse/lint diagnostics plus whole-document formatting backed
// by the same compiler paths as the CLI and Studio.
#include "commands.h"
#include "diagnostic_json.h"

#include "jtml/fix.h"
#include "jtml/formatter.h"
#include "jtml/friendly_formatter.h"
#include "jtml/linter.h"
#include "jtml/module_resolver.h"
#include "jtml/refactor.h"
#include "json.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace jtml::cli {
namespace {

struct TextDocument {
    std::string uri;
    std::string path;
    std::string text;
};

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string percentDecode(const std::string& value) {
    std::string out;
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            const std::string hex = value.substr(i + 1, 2);
            char* end = nullptr;
            long code = std::strtol(hex.c_str(), &end, 16);
            if (end && *end == '\0') {
                out.push_back(static_cast<char>(code));
                i += 2;
                continue;
            }
        }
        out.push_back(value[i]);
    }
    return out;
}

std::string pathFromUri(const std::string& uri) {
    const std::string prefix = "file://";
    if (uri.rfind(prefix, 0) != 0) return uri;
    std::string path = percentDecode(uri.substr(prefix.size()));
    if (path.rfind("localhost/", 0) == 0) path = path.substr(9);
    return path;
}

std::vector<std::string> linesOf(const std::string& text) {
    std::vector<std::string> lines;
    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) lines.push_back(line);
    if (lines.empty()) lines.push_back("");
    return lines;
}

std::string trimCopy(std::string value) {
    auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

bool startsWithWord(const std::string& text, const std::string& word) {
    if (text.rfind(word, 0) != 0) return false;
    return text.size() == word.size() ||
           std::isspace(static_cast<unsigned char>(text[word.size()]));
}

std::string wordAtPosition(const TextDocument& doc, int line, int character) {
    const auto lines = linesOf(doc.text);
    if (line < 0 || line >= static_cast<int>(lines.size())) return "";
    const std::string& text = lines[static_cast<size_t>(line)];
    int pos = std::max(0, std::min(character, static_cast<int>(text.size())));
    int start = pos;
    int end = pos;
    auto isWord = [](char ch) {
        return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-';
    };
    while (start > 0 && isWord(text[static_cast<size_t>(start - 1)])) --start;
    while (end < static_cast<int>(text.size()) && isWord(text[static_cast<size_t>(end)])) ++end;
    if (start == end && pos < static_cast<int>(text.size()) && isWord(text[static_cast<size_t>(pos)])) {
        end = pos + 1;
    }
    return text.substr(static_cast<size_t>(start), static_cast<size_t>(end - start));
}

std::filesystem::path tempPathFor(const TextDocument& doc) {
    std::filesystem::path base = doc.path.empty()
        ? std::filesystem::temp_directory_path() / "untitled.jtml"
        : std::filesystem::path(doc.path);
    const auto dir = base.has_parent_path() ? base.parent_path() : std::filesystem::current_path();
    const auto stem = base.stem().string().empty() ? "document" : base.stem().string();
    return dir / ("." + stem + ".jtml-lsp." + std::to_string(
        static_cast<long long>(std::hash<std::string>{}(doc.uri))) + ".jtml");
}

class TempFile {
public:
    TempFile(const TextDocument& doc) : path_(tempPathFor(doc)) {
        std::ofstream ofs(path_);
        if (!ofs.is_open()) {
            throw std::runtime_error("Cannot write temporary LSP file: " + path_.string());
        }
        ofs << doc.text;
    }

    ~TempFile() {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

nlohmann::json lspRangeFor(const std::string& text, int oneBasedLine, int oneBasedColumn) {
    const auto lines = linesOf(text);
    const int line = std::max(0, std::min(oneBasedLine > 0 ? oneBasedLine - 1 : 0,
                                          static_cast<int>(lines.size()) - 1));
    const int maxColumn = static_cast<int>(lines[static_cast<size_t>(line)].size());
    const int start = std::max(0, std::min(oneBasedColumn > 0 ? oneBasedColumn - 1 : 0, maxColumn));
    const int end = maxColumn > start ? start + 1 : start;
    return {
        {"start", {{"line", line}, {"character", start}}},
        {"end", {{"line", line}, {"character", end}}},
    };
}

nlohmann::json fullDocumentRange(const std::string& text) {
    const auto lines = linesOf(text);
    const int lastLine = std::max(0, static_cast<int>(lines.size()) - 1);
    return {
        {"start", {{"line", 0}, {"character", 0}}},
        {"end", {{"line", lastLine}, {"character", static_cast<int>(lines.back().size())}}},
    };
}

nlohmann::json lineRange(int zeroBasedLine, const std::string& line) {
    return {
        {"start", {{"line", zeroBasedLine}, {"character", 0}}},
        {"end", {{"line", zeroBasedLine}, {"character", static_cast<int>(line.size())}}},
    };
}

int lspSeverity(const std::string& severity) {
    const std::string lower = toLower(severity);
    if (lower == "warning") return 2;
    if (lower == "info") return 3;
    if (lower == "hint") return 4;
    return 1;
}

nlohmann::json diagnosticToLsp(const std::string& text, const nlohmann::json& diagnostic) {
    std::string message = diagnostic.value("message", "JTML diagnostic");
    if (diagnostic.contains("hint") && diagnostic["hint"].is_string() &&
        !diagnostic["hint"].get<std::string>().empty()) {
        message += "\n\nHint: " + diagnostic["hint"].get<std::string>();
    }
    if (diagnostic.contains("example") && diagnostic["example"].is_string() &&
        !diagnostic["example"].get<std::string>().empty()) {
        message += "\n\nExample:\n" + diagnostic["example"].get<std::string>();
    }

    nlohmann::json out = {
        {"range", lspRangeFor(text, diagnostic.value("line", 1), diagnostic.value("column", 1))},
        {"severity", lspSeverity(diagnostic.value("severity", "error"))},
        {"source", "jtml"},
        {"message", message},
    };
    if (diagnostic.contains("code") && diagnostic["code"].is_string() &&
        !diagnostic["code"].get<std::string>().empty()) {
        out["code"] = diagnostic["code"];
    }
    return out;
}

std::vector<Diagnostic> parseDiagnosticsFor(const TextDocument& doc, const std::filesystem::path& path) {
    try {
        SilenceStdout silence;
        (void)parseProgramFromFile(path.string(), SyntaxMode::Auto);
        return {};
    } catch (const std::exception& e) {
        return diagnosticsFromMessageBlock(e.what(), DiagnosticSeverity::Error);
    }
}

std::vector<Diagnostic> parseFriendlyDiagnosticsForOpenDocument(const TextDocument& doc) {
    if (!(isFriendlySyntax(doc.text) || looksLikeFriendlySyntax(doc.text))) return {};

    FriendlyClassicResult lowered;
    try {
        lowered = friendlyToClassicWithSourceMap(doc.text);
    } catch (const std::exception& e) {
        return diagnosticsFromMessageBlock(e.what(), DiagnosticSeverity::Error);
    }

    Lexer lexer(lowered.classicSource);
    auto tokens = lexer.tokenize();
    if (!lexer.getErrors().empty()) {
        std::ostringstream oss;
        for (const auto& error : lexer.getErrors()) oss << "Lexer Error: " << error << "\n";
        auto diagnostics = diagnosticsFromMessageBlock(oss.str(), DiagnosticSeverity::Error);
        remapDiagnosticLines(diagnostics, lowered.classicLineToFriendlyLine);
        return diagnostics;
    }

    Parser parser(std::move(tokens));
    (void)parser.parseProgram();
    if (parser.getErrors().empty()) return {};

    std::ostringstream oss;
    for (const auto& error : parser.getErrors()) oss << "Parser Error: " << error << "\n";
    auto diagnostics = diagnosticsFromMessageBlock(oss.str(), DiagnosticSeverity::Error);
    remapDiagnosticLines(diagnostics, lowered.classicLineToFriendlyLine);
    return diagnostics;
}

nlohmann::json collectDiagnosticsFor(const TextDocument& doc) {
    TempFile tmp(doc);
    const auto parseDiagnostics = parseDiagnosticsFor(doc, tmp.path());
    if (!parseDiagnostics.empty()) {
        // For single-file Friendly syntax, preserve the original source-line
        // remapping. If the document has imports, the full CLI compilation
        // path above is more truthful because it resolves the module graph
        // before component expansion.
        const bool hasImports = std::regex_search(
            doc.text, std::regex(R"((^|\n)\s*(use|import)\b)"));
        if (!hasImports) {
            const auto friendlyParseDiagnostics = parseFriendlyDiagnosticsForOpenDocument(doc);
            if (!friendlyParseDiagnostics.empty()) {
                return diagnosticsToJson(friendlyParseDiagnostics);
            }
        }
        return diagnosticsToJson(parseDiagnostics);
    }

    std::vector<std::unique_ptr<ASTNode>> program;
    {
        SilenceStdout silence;
        program = parseProgramFromFile(tmp.path().string(), SyntaxMode::Auto);
    }

    JtmlLinter linter;
    return lintDiagnosticsToJson(linter.lint(program));
}

std::string formatDocument(const TextDocument& doc) {
    TempFile tmp(doc);
    const bool friendlyInput =
        isFriendlySyntax(doc.text) || looksLikeFriendlySyntax(doc.text);

    if (friendlyInput) {
        SilenceStdout silence;
        (void)parseProgramFromFile(tmp.path().string(), SyntaxMode::Friendly);
        return formatFriendlySource(doc.text);
    }

    std::vector<std::unique_ptr<ASTNode>> program;
    {
        SilenceStdout silence;
        program = parseProgramFromFile(tmp.path().string(), SyntaxMode::Classic);
    }
    JtmlFormatter formatter;
    return formatter.format(program);
}

nlohmann::json completionItem(const std::string& label,
                              int kind,
                              const std::string& detail,
                              const std::string& insertText = "",
                              bool snippet = false) {
    nlohmann::json item = {
        {"label", label},
        {"kind", kind},
        {"detail", detail},
    };
    if (!insertText.empty()) item["insertText"] = insertText;
    if (snippet) item["insertTextFormat"] = 2;
    return item;
}

nlohmann::json completionItems() {
    nlohmann::json items = nlohmann::json::array();
    items.push_back(completionItem(
        "jtml 2", 14, "Friendly JTML file header", "jtml 2\n\n$0", true));
    items.push_back(completionItem(
        "page", 14, "Root page block", "page\n  $0", true));
    items.push_back(completionItem(
        "let", 14, "Reactive state declaration", "let ${1:name} = ${2:value}", true));
    items.push_back(completionItem(
        "get", 14, "Derived value declaration", "get ${1:name} = ${2:expression}", true));
    items.push_back(completionItem(
        "when", 14, "Action declaration", "when ${1:save}\n  $0", true));
    items.push_back(completionItem(
        "make", 14, "Component declaration", "make ${1:Card} ${2:title}\n  box class \"${3:card}\"\n    h2 ${2:title}\n    slot", true));
    items.push_back(completionItem(
        "export make", 14, "Public module component", "export make ${1:Card} ${2:title}\n  box class \"${3:card}\"\n    h2 ${2:title}\n    slot", true));
    items.push_back(completionItem(
        "route", 14, "Client-side route", "route \"${1:/}\" as ${2:Home}", true));
    items.push_back(completionItem(
        "route layout", 15, "Route wrapped in a shared layout", "route \"${1:/}\" as ${2:Home} layout ${3:AppLayout}", true));
    items.push_back(completionItem(
        "route load", 15, "Route-level lazy data load", "route \"${1:/items}\" as ${2:Items} load ${3:items}", true));
    items.push_back(completionItem(
        "fetch", 14, "Reactive async data", "let ${1:items} = fetch \"${2:/api/items}\" timeout ${3:2500} retry ${4:2} stale keep refresh ${5:reloadItems}", true));
    items.push_back(completionItem(
        "fetch lazy", 15, "Lazy route-loaded fetch", "let ${1:items} = fetch \"${2:/api/items}\" lazy stale keep", true));
    items.push_back(completionItem(
        "invalidate", 14, "Refresh fetch after action", "invalidate ${1:items}", true));
    items.push_back(completionItem(
        "store", 14, "Shared store declaration", "store ${1:auth}\n  let ${2:user} = \"\"\n\n  when ${3:logout}\n    let ${2:user} = \"\"", true));
    items.push_back(completionItem(
        "effect", 14, "Reactive side effect", "effect ${1:value}\n  $0", true));
    items.push_back(completionItem(
        "style", 14, "Scoped CSS block", "style\n  .${1:card}\n    padding: 16px", true));
    items.push_back(completionItem(
        "extern", 14, "Browser host action", "extern ${1:notify} from \"${2:host.notify}\"", true));
    items.push_back(completionItem(
        "input into", 15, "Input bound into state", "input \"${1:Label}\" into ${2:value}", true));
    items.push_back(completionItem(
        "link to", 15, "Route link", "link \"${1:Home}\" to \"${2:/}\" active-class \"${3:active}\"", true));

    for (const std::string& tag : {
             "box", "text", "h1", "h2", "h3", "p", "button", "form", "section",
             "article", "nav", "list", "item", "image", "video", "audio"}) {
        items.push_back(completionItem(tag, 10, "Friendly element shorthand"));
    }
    return items;
}

std::map<std::string, std::string> hoverDocs() {
    return {
        {"jtml", "`jtml 2` enables Friendly JTML syntax for this file."},
        {"let", "`let name = value` declares reactive state."},
        {"get", "`get name = expression` declares a derived value."},
        {"when", "`when action` declares an action that can be called from events."},
        {"page", "`page` declares the root page content."},
        {"make", "`make Component` declares a reusable Friendly component."},
        {"export", "`export` marks a top-level Friendly declaration as public for named imports."},
        {"slot", "`slot` marks where component children or route layout content is inserted."},
        {"route", "`route \"/path\" as Component` declares a hash-based client route."},
        {"layout", "`layout AppLayout` wraps route content in a zero-parameter layout component."},
        {"load", "`route \"/path\" as Page load data` starts named lazy fetches when a route matches."},
        {"fetch", "`fetch \"url\"` creates a reactive `{ loading, data, error, stale, attempts }` value."},
        {"lazy", "`fetch \"url\" lazy` registers a fetch without starting it until a route or action triggers it."},
        {"store", "`store name` groups shared state and namespaced actions."},
        {"effect", "`effect value` runs when a reactive value changes."},
        {"style", "`style` declares scoped CSS for the page."},
        {"extern", "`extern action from \"host.path\"` calls a browser-provided host function."},
        {"guard", "`guard \"/path\" require value else \"/login\"` protects a route."},
        {"redirect", "`redirect \"/path\"` navigates to a route from an action."},
        {"invalidate", "`invalidate name` refreshes a named fetch after the current action dispatches."},
        {"activeRoute", "`activeRoute` is the current normalized route path."},
        {"into", "`input \"Label\" into value` binds browser input into JTML state."},
    };
}

nlohmann::json hoverFor(const TextDocument& doc, int line, int character) {
    const std::string word = wordAtPosition(doc, line, character);
    const auto docs = hoverDocs();
    auto it = docs.find(word);
    if (it != docs.end()) {
        return {
            {"contents", {{"kind", "markdown"}, {"value", it->second}}},
        };
    }
    return nullptr;
}

std::optional<std::string> firstMatch(const std::string& text, const std::regex& pattern, size_t group = 1) {
    std::smatch match;
    if (!std::regex_search(text, match, pattern) || match.size() <= group) return std::nullopt;
    return match[group].str();
}

nlohmann::json symbolFor(const std::string& name,
                         int kind,
                         int line,
                         const std::string& text,
                         const std::string& detail = "",
                         bool exported = false) {
    nlohmann::json symbol = {
        {"name", name},
        {"kind", kind},
        {"range", lineRange(line, text)},
        {"selectionRange", lineRange(line, text)},
    };
    if (!detail.empty()) symbol["detail"] = detail;
    if (exported) symbol["exported"] = true;
    return symbol;
}

nlohmann::json documentSymbolsForText(const std::string& text) {
    const auto lines = linesOf(text);
    nlohmann::json symbols = nlohmann::json::array();
    const std::regex componentPattern(R"(^make\s+([A-Z][A-Za-z0-9_]*))");
    // Friendly `make foo a b` — lowercase first letter — is a function-style
    // declaration, not a component. Symbol scanners that gate cross-file LSP
    // features (rename, references, signature help, document highlights) must
    // see it so call sites like `foo(...)` are recognised as user symbols.
    const std::regex friendlyFunctionPattern(R"(^make\s+([a-z_][A-Za-z0-9_]*))");
    // Classic `function foo(args)` — the bundled examples (chat_app.jtml,
    // user_interactions.jtml, drag_and_drop_tasks.jtml, keywords.jtml,
    // live_input.jtml) all use this form, so cross-file LSP must index it too.
    const std::regex classicFunctionPattern(R"(^function\s+([A-Za-z_][A-Za-z0-9_]*))");
    const std::regex actionPattern(R"(^when\s+([A-Za-z_][A-Za-z0-9_]*))");
    const std::regex storePattern(R"(^store\s+([A-Za-z_][A-Za-z0-9_]*))");
    const std::regex statePattern(R"(^(let|get|const)\s+([A-Za-z_][A-Za-z0-9_]*))");
    const std::regex routePattern(R"(^route\s+("[^"]*"|'[^']*')\s+as\s+([A-Z][A-Za-z0-9_]*))");
    const std::regex effectPattern(R"(^effect\s+([A-Za-z_][A-Za-z0-9_]*))");
    const std::regex externPattern(R"(^extern\s+([A-Za-z_][A-Za-z0-9_]*))");

    for (size_t i = 0; i < lines.size(); ++i) {
        std::string trimmed = trimCopy(lines[i]);
        if (trimmed.empty() || startsWithWord(trimmed, "//")) continue;
        bool exported = false;
        if (startsWithWord(trimmed, "export")) {
            exported = true;
            trimmed = trimCopy(trimmed.substr(std::string("export").size()));
            if (trimmed.empty()) continue;
        }

        if (auto name = firstMatch(trimmed, componentPattern)) {
            symbols.push_back(symbolFor(*name, 5, static_cast<int>(i), lines[i],
                                        exported ? "exported component" : "component", exported));
        } else if (auto name = firstMatch(trimmed, friendlyFunctionPattern)) {
            symbols.push_back(symbolFor(*name, 12, static_cast<int>(i), lines[i],
                                        exported ? "exported function" : "function", exported));
        } else if (auto name = firstMatch(trimmed, classicFunctionPattern)) {
            symbols.push_back(symbolFor(*name, 12, static_cast<int>(i), lines[i], "function"));
        } else if (auto name = firstMatch(trimmed, actionPattern)) {
            symbols.push_back(symbolFor(*name, 12, static_cast<int>(i), lines[i],
                                        exported ? "exported action" : "action", exported));
        } else if (auto name = firstMatch(trimmed, storePattern)) {
            symbols.push_back(symbolFor(*name, 19, static_cast<int>(i), lines[i],
                                        exported ? "exported store" : "store", exported));
        } else if (std::smatch match; std::regex_search(trimmed, match, statePattern)) {
            symbols.push_back(symbolFor(match[2].str(), match[1].str() == "get" ? 7 : 13,
                                        static_cast<int>(i), lines[i],
                                        exported ? "exported " + match[1].str() : match[1].str(),
                                        exported));
        } else if (std::smatch match; std::regex_search(trimmed, match, routePattern)) {
            symbols.push_back(symbolFor("route " + match[1].str(), 2, static_cast<int>(i),
                                        lines[i], "as " + match[2].str()));
        } else if (auto name = firstMatch(trimmed, effectPattern)) {
            symbols.push_back(symbolFor("effect " + *name, 12, static_cast<int>(i), lines[i], "effect"));
        } else if (auto name = firstMatch(trimmed, externPattern)) {
            symbols.push_back(symbolFor(*name, 12, static_cast<int>(i), lines[i],
                                        exported ? "exported extern" : "extern", exported));
        }
    }
    return symbols;
}

nlohmann::json documentSymbolsFor(const TextDocument& doc) {
    return documentSymbolsForText(doc.text);
}

// ---------------------------------------------------------------------------
// Cross-file module graph for the LSP. Resolves `use "path"`,
// `use Name from "path"`, and classic `import "path"` statements relative to
// the importing document so go-to-definition, hover, and completion walk
// imported files. Cycle-guarded via canonical paths.
// ---------------------------------------------------------------------------
struct ImportSpec {
    std::filesystem::path path;
    std::vector<std::string> names;
    bool sideEffect = false;
};

std::vector<std::string> parseImportNames(std::string text) {
    std::vector<std::string> names;
    text = trimCopy(std::move(text));
    if (text.empty()) return names;
    if (text.front() == '{') {
        if (text.back() == '}') text = text.substr(1, text.size() - 2);
        std::stringstream ss(text);
        std::string item;
        while (std::getline(ss, item, ',')) {
            item = trimCopy(item);
            item.erase(std::remove_if(item.begin(), item.end(), [](unsigned char ch) {
                return !(std::isalnum(ch) || ch == '_');
            }), item.end());
            if (!item.empty()) names.push_back(item);
        }
    } else {
        text.erase(std::remove_if(text.begin(), text.end(), [](unsigned char ch) {
            return !(std::isalnum(ch) || ch == '_');
        }), text.end());
        if (!text.empty()) names.push_back(text);
    }
    return names;
}

std::vector<ImportSpec> importSpecsFor(
        const std::string& sourceText,
        const std::filesystem::path& sourcePath) {
    namespace fs = std::filesystem;
    std::vector<ImportSpec> specs;
    if (sourcePath.empty()) return specs;
    const auto lines = linesOf(sourceText);
    static const std::regex sideEffectUse(R"(^\s*use\s+\"([^\"]+)\")");
    static const std::regex namedUse(R"(^\s*use\s+(.+?)\s+from\s+\"([^\"]+)\")");
    static const std::regex classicImport(R"(^\s*import\s+\"([^\"]+)\")");
    for (const auto& raw : lines) {
        std::smatch match;
        ImportSpec spec;
        std::string rawPath;
        if (std::regex_search(raw, match, sideEffectUse)) {
            rawPath = match[1].str();
            spec.sideEffect = true;
        } else if (std::regex_search(raw, match, namedUse)) {
            spec.names = parseImportNames(match[1].str());
            rawPath = match[2].str();
        } else if (std::regex_search(raw, match, classicImport)) {
            rawPath = match[1].str();
            spec.sideEffect = true;
        } else {
            continue;
        }
        fs::path candidate = resolveJtmlModulePath(rawPath, sourcePath);
        std::error_code ec;
        fs::path canonical = fs::weakly_canonical(candidate, ec);
        if (!ec && !canonical.empty()) candidate = canonical;
        if (fs::exists(candidate)) {
            spec.path = candidate;
            specs.push_back(std::move(spec));
        }
    }
    return specs;
}

std::vector<std::filesystem::path> importedFilesFor(
        const std::string& sourceText,
        const std::filesystem::path& sourcePath) {
    std::vector<std::filesystem::path> paths;
    for (const auto& spec : importSpecsFor(sourceText, sourcePath)) {
        paths.push_back(spec.path);
    }
    return paths;
}

std::string fileUriFor(const std::filesystem::path& path) {
    return "file://" + path.string();
}

std::string readFileSafe(const std::filesystem::path& path) {
    std::ifstream ifs(path);
    if (!ifs) return {};
    std::ostringstream buf; buf << ifs.rdbuf();
    return buf.str();
}

// Walk the module graph starting at `seedText` / `seedPath`, invoking `visit`
// for each (path, text, symbols) tuple. Skips the seed file itself — callers
// have already consulted the open document. Cycle-guarded.
void forEachImportedModule(
        const std::string& seedText,
        const std::filesystem::path& seedPath,
        const std::function<bool(const std::filesystem::path&,
                                 const std::string&,
                                 const nlohmann::json&)>& visit) {
    namespace fs = std::filesystem;
    std::vector<ImportSpec> queue;
    std::vector<std::string> visited;
    if (!seedPath.empty()) visited.push_back(seedPath.string());

    for (const auto& spec : importSpecsFor(seedText, seedPath)) queue.push_back(spec);

    while (!queue.empty()) {
        auto spec = queue.front();
        queue.erase(queue.begin());
        const fs::path path = spec.path;
        const std::string key = path.string();
        if (std::find(visited.begin(), visited.end(), key) != visited.end()) continue;
        visited.push_back(key);

        const std::string text = readFileSafe(path);
        if (text.empty()) continue;
        const auto symbols = documentSymbolsForText(text);
        nlohmann::json visibleSymbols = symbols;
        const bool moduleUsesExports = std::any_of(symbols.begin(), symbols.end(), [](const auto& symbol) {
            return symbol.value("exported", false);
        });
        if (moduleUsesExports && !spec.sideEffect) {
            visibleSymbols = nlohmann::json::array();
            for (const auto& symbol : symbols) {
                if (!symbol.value("exported", false)) continue;
                if (!symbol.contains("name") || !symbol["name"].is_string()) continue;
                const std::string name = symbol["name"].get<std::string>();
                if (std::find(spec.names.begin(), spec.names.end(), name) != spec.names.end()) {
                    visibleSymbols.push_back(symbol);
                }
            }
        }
        if (!visit(path, text, visibleSymbols)) return;

        // Transitive: enqueue this module's imports so a chain `A -> B -> C`
        // resolves go-to-definition through both hops.
        for (const auto& nested : importSpecsFor(text, path)) queue.push_back(nested);
    }
}

nlohmann::json completionItemsFor(const TextDocument& doc) {
    nlohmann::json items = completionItems();
    auto pushSymbols = [&items](const nlohmann::json& symbols, const std::string& origin) {
        for (const auto& symbol : symbols) {
            if (!symbol.contains("name") || !symbol["name"].is_string()) continue;
            const std::string name = symbol["name"].get<std::string>();
            if (name.rfind("route ", 0) == 0 || name.rfind("effect ", 0) == 0) continue;
            const std::string detail = symbol.value("detail", "JTML symbol");
            items.push_back(completionItem(
                name, symbol.value("kind", 6),
                origin.empty() ? detail : detail + " (from " + origin + ")"));
        }
    };
    pushSymbols(documentSymbolsFor(doc), {});
    forEachImportedModule(
        doc.text, doc.path,
        [&](const std::filesystem::path& path, const std::string&,
            const nlohmann::json& symbols) {
            pushSymbols(symbols, path.filename().string());
            return true;
        });
    return items;
}

nlohmann::json hoverForSymbol(const TextDocument& doc, int line, int character) {
    const std::string word = wordAtPosition(doc, line, character);
    if (word.empty()) return nullptr;
    for (const auto& symbol : documentSymbolsFor(doc)) {
        if (!symbol.contains("name") || !symbol["name"].is_string()) continue;
        if (symbol["name"].get<std::string>() != word) continue;
        const std::string detail = symbol.value("detail", "symbol");
        return {
            {"contents", {{"kind", "markdown"},
                          {"value", "`" + word + "`\n\nJTML " + detail}}},
        };
    }
    nlohmann::json result = nullptr;
    forEachImportedModule(
        doc.text, doc.path,
        [&](const std::filesystem::path& path, const std::string&,
            const nlohmann::json& symbols) {
            for (const auto& symbol : symbols) {
                if (!symbol.contains("name") || !symbol["name"].is_string()) continue;
                if (symbol["name"].get<std::string>() != word) continue;
                const std::string detail = symbol.value("detail", "symbol");
                result = {
                    {"contents", {{"kind", "markdown"},
                                  {"value", "`" + word + "`\n\nJTML " + detail +
                                            " — imported from `" +
                                            path.filename().string() + "`"}}},
                };
                return false;  // stop the walk
            }
            return true;
        });
    return result;
}

nlohmann::json definitionFor(const TextDocument& doc, int line, int character) {
    const std::string word = wordAtPosition(doc, line, character);
    if (word.empty()) return nullptr;
    for (const auto& symbol : documentSymbolsFor(doc)) {
        if (!symbol.contains("name") || !symbol["name"].is_string()) continue;
        if (symbol["name"].get<std::string>() != word) continue;
        return {
            {"uri", doc.uri},
            {"range", symbol["selectionRange"]},
        };
    }
    nlohmann::json result = nullptr;
    forEachImportedModule(
        doc.text, doc.path,
        [&](const std::filesystem::path& path, const std::string&,
            const nlohmann::json& symbols) {
            for (const auto& symbol : symbols) {
                if (!symbol.contains("name") || !symbol["name"].is_string()) continue;
                if (symbol["name"].get<std::string>() != word) continue;
                result = {
                    {"uri", fileUriFor(path)},
                    {"range", symbol["selectionRange"]},
                };
                return false;  // stop the walk
            }
            return true;
        });
    return result;
}

bool readMessage(nlohmann::json& message) {
    std::string line;
    size_t contentLength = 0;

    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;

        const std::string lower = toLower(line);
        const std::string header = "content-length:";
        if (lower.rfind(header, 0) == 0) {
            const std::string value = line.substr(header.size());
            contentLength = static_cast<size_t>(std::stoul(value));
        }
    }

    if (contentLength == 0) return false;
    std::string body(contentLength, '\0');
    std::cin.read(body.data(), static_cast<std::streamsize>(contentLength));
    if (std::cin.gcount() != static_cast<std::streamsize>(contentLength)) return false;
    message = nlohmann::json::parse(body, nullptr, false);
    return !message.is_discarded();
}

void writeMessage(const nlohmann::json& message) {
    const std::string body = message.dump();
    std::cout << "Content-Length: " << body.size() << "\r\n\r\n" << body;
    std::cout.flush();
}

nlohmann::json responseFor(const nlohmann::json& request, nlohmann::json result) {
    return {
        {"jsonrpc", "2.0"},
        {"id", request["id"]},
        {"result", std::move(result)},
    };
}

nlohmann::json errorFor(const nlohmann::json& request, int code, const std::string& message) {
    return {
        {"jsonrpc", "2.0"},
        {"id", request.value("id", nlohmann::json(nullptr))},
        {"error", {{"code", code}, {"message", message}}},
    };
}

void publishDiagnostics(const TextDocument& doc) {
    nlohmann::json diagnostics = nlohmann::json::array();
    try {
        for (const auto& diagnostic : collectDiagnosticsFor(doc)) {
            diagnostics.push_back(diagnosticToLsp(doc.text, diagnostic));
        }
    } catch (const std::exception& e) {
        diagnostics.push_back({
            {"range", lspRangeFor(doc.text, 1, 1)},
            {"severity", 1},
            {"source", "jtml"},
            {"code", "JTML_LSP_ERROR"},
            {"message", e.what()},
        });
    }

    writeMessage({
        {"jsonrpc", "2.0"},
        {"method", "textDocument/publishDiagnostics"},
        {"params", {{"uri", doc.uri}, {"diagnostics", diagnostics}}},
    });
}

std::string textFromContentChanges(const nlohmann::json& params, const std::string& current) {
    if (!params.contains("contentChanges") || !params["contentChanges"].is_array() ||
        params["contentChanges"].empty()) {
        return current;
    }
    const auto& last = params["contentChanges"].back();
    if (last.contains("text") && last["text"].is_string()) {
        return last["text"].get<std::string>();
    }
    return current;
}

// ---------------------------------------------------------------------------
// Workspace symbols + rename. Both reuse a single "scan every .jtml file under
// the workspace root, prefer in-memory open-document text" routine so
// behaviour stays consistent across queries.
// ---------------------------------------------------------------------------
struct WorkspaceFile {
    std::filesystem::path path;
    std::string uri;
    std::string text;
};

std::string normalizeWorkspaceRoot(const nlohmann::json& params) {
    if (params.contains("rootUri") && params["rootUri"].is_string()) {
        const std::string uri = params["rootUri"].get<std::string>();
        if (!uri.empty()) return pathFromUri(uri);
    }
    if (params.contains("rootPath") && params["rootPath"].is_string()) {
        const std::string path = params["rootPath"].get<std::string>();
        if (!path.empty()) return path;
    }
    if (params.contains("workspaceFolders") && params["workspaceFolders"].is_array() &&
        !params["workspaceFolders"].empty()) {
        const auto& folder = params["workspaceFolders"][0];
        if (folder.contains("uri") && folder["uri"].is_string()) {
            return pathFromUri(folder["uri"].get<std::string>());
        }
    }
    return {};
}

bool shouldSkipDirectory(const std::filesystem::path& path) {
    const std::string name = path.filename().string();
    if (name.empty() || name[0] == '.') return true;
    if (name == "build" || name == "dist" || name == "node_modules" ||
        name == "third_party" || name == "target") {
        return true;
    }
    return false;
}

std::vector<WorkspaceFile> scanWorkspace(
        const std::string& root,
        const std::map<std::string, TextDocument>& openDocuments) {
    namespace fs = std::filesystem;
    std::vector<WorkspaceFile> files;
    if (root.empty() || !fs::exists(root)) {
        // No workspace context — expose open documents and the imports they
        // pull in via `use` / `import`, so single-file editor sessions still
        // get workspace symbols across the module graph.
        std::vector<std::string> seen;
        auto record = [&](const std::string& uri, const fs::path& path,
                          const std::string& text) {
            std::error_code canonEc;
            fs::path canonical = fs::weakly_canonical(path, canonEc);
            if (canonEc) canonical = path;
            const std::string key = canonical.string();
            if (std::find(seen.begin(), seen.end(), key) != seen.end()) return;
            seen.push_back(key);
            WorkspaceFile entry;
            entry.path = canonical;
            entry.uri = uri.empty() ? "file://" + canonical.string() : uri;
            entry.text = text;
            files.push_back(std::move(entry));
        };
        for (const auto& [uri, doc] : openDocuments) {
            record(uri, doc.path, doc.text);
            if (doc.path.empty()) continue;
            forEachImportedModule(doc.text, doc.path,
                [&](const fs::path& p, const std::string& text,
                    const nlohmann::json&) {
                    record({}, p, text);
                    return true;
                });
        }
        return files;
    }
    std::vector<std::string> seenPaths;
    std::error_code ec;
    auto pushFile = [&](const fs::path& path, const std::string& text) {
        std::error_code canonEc;
        fs::path canonical = fs::weakly_canonical(path, canonEc);
        if (canonEc) canonical = path;
        const std::string key = canonical.string();
        if (std::find(seenPaths.begin(), seenPaths.end(), key) != seenPaths.end()) return;
        seenPaths.push_back(key);
        WorkspaceFile entry;
        entry.path = canonical;
        entry.uri = "file://" + canonical.string();
        entry.text = text;
        files.push_back(std::move(entry));
    };
    // Index open documents first so their in-memory text wins over what is
    // currently on disk.
    std::vector<std::string> openCanonicalPaths;
    for (const auto& [uri, doc] : openDocuments) {
        if (doc.path.empty()) continue;
        std::error_code canonEc;
        fs::path canonical = fs::weakly_canonical(doc.path, canonEc);
        if (canonEc) canonical = doc.path;
        openCanonicalPaths.push_back(canonical.string());
        pushFile(canonical, doc.text);
    }
    for (auto it = fs::recursive_directory_iterator(root, ec);
         !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) break;
        if (it->is_directory()) {
            if (shouldSkipDirectory(it->path())) {
                it.disable_recursion_pending();
            }
            continue;
        }
        if (it->path().extension() != ".jtml") continue;
        std::string text = readFileSafe(it->path());
        pushFile(it->path(), text);
    }
    // Open documents not under the workspace root still belong in the result
    // so a user can rename across an out-of-tree open file too.
    for (const auto& [uri, doc] : openDocuments) {
        if (doc.path.empty()) continue;
        std::error_code canonEc;
        fs::path canonical = fs::weakly_canonical(doc.path, canonEc);
        if (canonEc) canonical = doc.path;
        const std::string key = canonical.string();
        if (std::find(seenPaths.begin(), seenPaths.end(), key) == seenPaths.end()) {
            pushFile(canonical, doc.text);
        }
    }
    return files;
}

bool isIdentifierChar(char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
}

struct Reference {
    int line;       // 0-based
    int startCol;   // 0-based
    int endCol;     // 0-based exclusive
};

// Scan a single source for word-boundary occurrences of `name` that are not
// inside a string literal or a `//` comment. This is conservative enough for
// rename: it never edits text the parser would not see as an identifier.
std::vector<Reference> findReferencesInText(const std::string& text, const std::string& name) {
    std::vector<Reference> refs;
    if (name.empty()) return refs;
    const auto lines = linesOf(text);
    for (size_t li = 0; li < lines.size(); ++li) {
        const std::string& line = lines[li];
        bool inString = false;
        char stringQuote = 0;
        for (size_t i = 0; i < line.size(); ) {
            const char ch = line[i];
            if (inString) {
                if (ch == '\\' && i + 1 < line.size()) { i += 2; continue; }
                if (ch == stringQuote) inString = false;
                ++i; continue;
            }
            if (ch == '"' || ch == '\'') { inString = true; stringQuote = ch; ++i; continue; }
            if (ch == '/' && i + 1 < line.size() && line[i + 1] == '/') break;
            const bool boundaryStart = i == 0 || !isIdentifierChar(line[i - 1]);
            if (boundaryStart &&
                i + name.size() <= line.size() &&
                line.compare(i, name.size(), name) == 0) {
                const size_t end = i + name.size();
                const bool boundaryEnd = end == line.size() || !isIdentifierChar(line[end]);
                if (boundaryEnd) {
                    refs.push_back({
                        static_cast<int>(li),
                        static_cast<int>(i),
                        static_cast<int>(end),
                    });
                    i = end;
                    continue;
                }
            }
            ++i;
        }
    }
    return refs;
}

bool symbolListContains(const nlohmann::json& symbols, const std::string& name) {
    for (const auto& symbol : symbols) {
        if (!symbol.contains("name") || !symbol["name"].is_string()) continue;
        if (symbol["name"].get<std::string>() == name) return true;
    }
    return false;
}

// True if `name` is declared somewhere reachable from `doc` — either the open
// document itself or any module it transitively imports. Rename uses this to
// gate edits so we never rewrite arbitrary identifiers (e.g. tag names).
bool isReachableUserSymbol(const TextDocument& doc, const std::string& name) {
    if (symbolListContains(documentSymbolsFor(doc), name)) return true;
    bool found = false;
    forEachImportedModule(
        doc.text, doc.path,
        [&](const std::filesystem::path&, const std::string&,
            const nlohmann::json& symbols) {
            if (symbolListContains(symbols, name)) {
                found = true;
                return false;  // stop the walk
            }
            return true;
        });
    return found;
}

int workspaceSymbolKind(const std::string& detail) {
    if (detail == "component") return 5;
    if (detail == "action" || detail == "effect" || detail == "extern") return 12;
    if (detail == "store") return 19;
    if (detail == "get") return 7;
    return 13;
}

nlohmann::json workspaceSymbolsFor(
        const std::string& root,
        const std::map<std::string, TextDocument>& openDocuments,
        const std::string& query) {
    nlohmann::json items = nlohmann::json::array();
    const std::string queryLower = toLower(query);
    const auto files = scanWorkspace(root, openDocuments);
    for (const auto& file : files) {
        const auto symbols = documentSymbolsForText(file.text);
        for (const auto& symbol : symbols) {
            if (!symbol.contains("name") || !symbol["name"].is_string()) continue;
            const std::string name = symbol["name"].get<std::string>();
            if (!queryLower.empty() && toLower(name).find(queryLower) == std::string::npos) continue;
            const std::string detail = symbol.value("detail", "symbol");
            items.push_back({
                {"name", name},
                {"kind", symbol.value("kind", workspaceSymbolKind(detail))},
                {"containerName", file.path.filename().string()},
                {"location", {
                    {"uri", file.uri},
                    {"range", symbol.value("selectionRange", nlohmann::json::object())},
                }},
            });
        }
    }
    return items;
}

nlohmann::json prepareRenameFor(const TextDocument& doc, int line, int character) {
    const std::string word = wordAtPosition(doc, line, character);
    if (word.empty()) return nullptr;
    if (!isReachableUserSymbol(doc, word)) {
        // Conservative: only allow rename for symbols declared in the open
        // document or in a module it imports. This rejects keywords, tag
        // names, and arbitrary identifiers we cannot safely rewrite.
        return nullptr;
    }
    const auto lines = linesOf(doc.text);
    if (line < 0 || line >= static_cast<int>(lines.size())) return nullptr;
    const std::string& current = lines[static_cast<size_t>(line)];
    int start = std::max(0, std::min(character, static_cast<int>(current.size())));
    int end = start;
    while (start > 0 && isIdentifierChar(current[static_cast<size_t>(start - 1)])) --start;
    while (end < static_cast<int>(current.size()) && isIdentifierChar(current[static_cast<size_t>(end)])) ++end;
    return {
        {"start", {{"line", line}, {"character", start}}},
        {"end",   {{"line", line}, {"character", end}}},
    };
}

nlohmann::json referencesFor(
        const std::string& root,
        const std::map<std::string, TextDocument>& openDocuments,
        const TextDocument& doc,
        int line, int character,
        bool includeDeclaration) {
    const std::string name = wordAtPosition(doc, line, character);
    if (name.empty()) return nlohmann::json::array();
    if (!isReachableUserSymbol(doc, name)) return nlohmann::json::array();

    nlohmann::json locations = nlohmann::json::array();
    const auto files = scanWorkspace(root, openDocuments);
    // Index symbol-declaration line ranges per file so we can honour
    // `context.includeDeclaration`. The declaration is the line where the
    // identifier first appears in `documentSymbolsForText`.
    auto declarationLineFor = [&](const std::string& text) -> int {
        for (const auto& symbol : documentSymbolsForText(text)) {
            if (!symbol.contains("name") || !symbol["name"].is_string()) continue;
            if (symbol["name"].get<std::string>() != name) continue;
            if (symbol.contains("selectionRange") &&
                symbol["selectionRange"].contains("start") &&
                symbol["selectionRange"]["start"].contains("line")) {
                return symbol["selectionRange"]["start"].value("line", -1);
            }
        }
        return -1;
    };
    for (const auto& file : files) {
        const auto refs = findReferencesInText(file.text, name);
        if (refs.empty()) continue;
        const int declarationLine = declarationLineFor(file.text);
        for (const auto& ref : refs) {
            if (!includeDeclaration && ref.line == declarationLine) continue;
            locations.push_back({
                {"uri", file.uri},
                {"range", {
                    {"start", {{"line", ref.line}, {"character", ref.startCol}}},
                    {"end",   {{"line", ref.line}, {"character", ref.endCol}}},
                }},
            });
        }
    }
    return locations;
}

nlohmann::json renameFor(
        const std::string& root,
        const std::map<std::string, TextDocument>& openDocuments,
        const TextDocument& doc,
        int line, int character,
        const std::string& newName) {
    const std::string oldName = wordAtPosition(doc, line, character);
    if (oldName.empty() || newName.empty() || oldName == newName) return nullptr;
    if (!isReachableUserSymbol(doc, oldName)) return nullptr;

    // Delegate the per-file rewrite to the canonical scanner in core. This
    // keeps the LSP rename and `jtml refactor rename` byte-for-byte
    // identical: same boundary rules, same string/comment handling, same
    // ranges. We only need to translate the edits into LSP TextEdit shape.
    nlohmann::json changes = nlohmann::json::object();
    const auto files = scanWorkspace(root, openDocuments);
    bool any = false;
    for (const auto& file : files) {
        const auto rename = jtml::renameSymbolInSource(file.text, oldName, newName);
        if (!rename.changed) continue;
        nlohmann::json edits = nlohmann::json::array();
        for (const auto& edit : rename.edits) {
            edits.push_back({
                {"range", {
                    {"start", {{"line", edit.line}, {"character", edit.startColumn}}},
                    {"end",   {{"line", edit.line}, {"character", edit.endColumn}}},
                }},
                {"newText", newName},
            });
        }
        changes[file.uri] = edits;
        any = true;
    }
    if (!any) return nullptr;
    return nlohmann::json{{"changes", changes}};
}

// ---------------------------------------------------------------------------
// Signature help. We extract parameter lists for `make`, `when`, and classic
// `function` declarations by re-scanning every workspace file when the user
// is inside a `name(...)` call and the symbol is reachable from the open
// document. This is intentionally text-driven — we never invoke the parser
// here — so signature help stays available even on broken in-flight code.
// ---------------------------------------------------------------------------
struct CallContext {
    std::string name;
    int activeParameter = 0;
};

std::optional<CallContext> callContextAtPosition(const TextDocument& doc, int line, int character) {
    const auto lines = linesOf(doc.text);
    if (line < 0 || line >= static_cast<int>(lines.size())) return std::nullopt;
    const std::string& current = lines[static_cast<size_t>(line)];
    int depth = 0;
    int activeParam = 0;
    int paramOpen = -1;
    int upTo = std::min(static_cast<int>(current.size()), character);
    for (int i = upTo - 1; i >= 0; --i) {
        const char ch = current[static_cast<size_t>(i)];
        if (ch == ')') ++depth;
        else if (ch == '(') {
            if (depth == 0) { paramOpen = i; break; }
            --depth;
        } else if (ch == ',' && depth == 0) {
            ++activeParam;
        }
    }
    if (paramOpen < 0) return std::nullopt;
    int j = paramOpen - 1;
    while (j >= 0 && std::isspace(static_cast<unsigned char>(current[static_cast<size_t>(j)]))) --j;
    int end = j + 1;
    while (j >= 0 && isIdentifierChar(current[static_cast<size_t>(j)])) --j;
    int start = j + 1;
    if (start == end) return std::nullopt;
    CallContext ctx;
    ctx.name = current.substr(static_cast<size_t>(start), static_cast<size_t>(end - start));
    ctx.activeParameter = activeParam;
    return ctx;
}

// Returns the parameter names, in source order, for the first declaration of
// `name` matching `make`, `when`, or `function` syntax. Empty vector means no
// declaration was found in this text.
std::vector<std::string> declarationParameters(const std::string& text, const std::string& name) {
    if (name.empty()) return {};
    const auto lines = linesOf(text);
    const std::regex makeWhen("^\\s*(make|when)\\s+" + name +
                              "(?:\\s+(.*))?$");
    const std::regex classicFn("^\\s*function\\s+" + name + "\\s*\\(([^)]*)\\)");
    auto split = [](const std::string& body, char sep) {
        std::vector<std::string> parts;
        std::string current;
        for (char ch : body) {
            if (ch == sep) {
                std::string trimmed = trimCopy(current);
                if (!trimmed.empty()) parts.push_back(trimmed);
                current.clear();
            } else {
                current.push_back(ch);
            }
        }
        std::string trimmed = trimCopy(current);
        if (!trimmed.empty()) parts.push_back(trimmed);
        return parts;
    };
    for (const auto& line : lines) {
        std::smatch match;
        if (std::regex_search(line, match, makeWhen)) {
            return split(match.size() > 2 ? match[2].str() : std::string{}, ' ');
        }
        if (std::regex_search(line, match, classicFn)) {
            return split(match[1].str(), ',');
        }
    }
    return {};
}

nlohmann::json signatureHelpFor(
        const std::string& root,
        const std::map<std::string, TextDocument>& openDocuments,
        const TextDocument& doc, int line, int character) {
    auto ctx = callContextAtPosition(doc, line, character);
    if (!ctx) return nullptr;
    if (!isReachableUserSymbol(doc, ctx->name)) return nullptr;

    std::vector<std::string> params;
    // Search the open document first — unsaved changes win over disk content.
    params = declarationParameters(doc.text, ctx->name);
    if (params.empty()) {
        const auto files = scanWorkspace(root, openDocuments);
        for (const auto& file : files) {
            params = declarationParameters(file.text, ctx->name);
            if (!params.empty()) break;
        }
    }
    if (params.empty()) return nullptr;

    std::ostringstream label;
    label << ctx->name << "(";
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) label << ", ";
        label << params[i];
    }
    label << ")";
    nlohmann::json paramInfos = nlohmann::json::array();
    for (const auto& p : params) paramInfos.push_back({{"label", p}});

    int active = ctx->activeParameter;
    if (params.empty()) active = 0;
    else if (active >= static_cast<int>(params.size())) {
        active = static_cast<int>(params.size()) - 1;
    }

    return {
        {"signatures", nlohmann::json::array({
            {
                {"label", label.str()},
                {"parameters", paramInfos},
            }
        })},
        {"activeSignature", 0},
        {"activeParameter", active},
    };
}

// Document highlights: in-file occurrences only. Reuses the rename-grade
// reference scanner so we never highlight inside strings or comments.
nlohmann::json documentHighlightsFor(const TextDocument& doc, int line, int character) {
    nlohmann::json out = nlohmann::json::array();
    const std::string name = wordAtPosition(doc, line, character);
    if (name.empty() || !isReachableUserSymbol(doc, name)) return out;
    for (const auto& ref : findReferencesInText(doc.text, name)) {
        out.push_back({
            {"range", {
                {"start", {{"line", ref.line}, {"character", ref.startCol}}},
                {"end",   {{"line", ref.line}, {"character", ref.endCol}}},
            }},
            {"kind", 2},  // LSP DocumentHighlightKind.Read
        });
    }
    return out;
}

// Selection range: returns a nested LSP SelectionRange tree per requested
// position. The hierarchy reflects Friendly JTML's indentation: starting from
// the smallest meaningful selection (the word at the cursor) and expanding
// outward through the trimmed line, the full line, every enclosing
// indentation block, and finally the whole document. AI-assisted editors
// drive "expand selection" / "shrink selection" with this tree.
nlohmann::json selectionRangesFor(const TextDocument& doc, const nlohmann::json& positions) {
    nlohmann::json out = nlohmann::json::array();
    if (!positions.is_array()) return out;

    const auto lines = linesOf(doc.text);
    const int total = static_cast<int>(lines.size());

    auto indentOf = [&](int li) -> int {
        if (li < 0 || li >= total) return -1;
        const std::string& s = lines[static_cast<size_t>(li)];
        int i = 0;
        while (i < static_cast<int>(s.size()) &&
               (s[static_cast<size_t>(i)] == ' ' || s[static_cast<size_t>(i)] == '\t')) {
            ++i;
        }
        if (i == static_cast<int>(s.size())) return -1;  // blank line
        return i;
    };
    auto lineEndCol = [&](int li) {
        return (li >= 0 && li < total)
            ? static_cast<int>(lines[static_cast<size_t>(li)].size())
            : 0;
    };

    auto isWord = [](char ch) {
        return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-';
    };

    for (const auto& pos : positions) {
        int line = pos.value("line", 0);
        int character = pos.value("character", 0);
        if (total == 0) {
            out.push_back({{"range", {
                {"start", {{"line", 0}, {"character", 0}}},
                {"end",   {{"line", 0}, {"character", 0}}},
            }}});
            continue;
        }
        if (line < 0) line = 0;
        if (line >= total) line = total - 1;

        // ranges accumulates innermost -> outermost selection ranges. We
        // dedupe on push so identical ranges (e.g. word == trimmed line on a
        // single-token line) collapse into one node.
        std::vector<std::array<int, 4>> ranges;  // {sL, sC, eL, eC}
        auto push = [&](int sL, int sC, int eL, int eC) {
            if (eL < sL || (eL == sL && eC < sC)) return;
            std::array<int, 4> r{sL, sC, eL, eC};
            if (!ranges.empty() && ranges.back() == r) return;
            ranges.push_back(r);
        };

        // 1. Word at the cursor.
        const std::string& cur = lines[static_cast<size_t>(line)];
        const int p = std::max(0, std::min(character, static_cast<int>(cur.size())));
        int wStart = p, wEnd = p;
        while (wStart > 0 && isWord(cur[static_cast<size_t>(wStart - 1)])) --wStart;
        while (wEnd < static_cast<int>(cur.size()) && isWord(cur[static_cast<size_t>(wEnd)])) ++wEnd;
        if (wStart == wEnd && p < static_cast<int>(cur.size()) && isWord(cur[static_cast<size_t>(p)])) {
            wEnd = p + 1;
        }
        if (wEnd > wStart) push(line, wStart, line, wEnd);

        // 2. Trimmed line content (first non-whitespace to end of line).
        int firstNonWs = 0;
        while (firstNonWs < static_cast<int>(cur.size()) &&
               (cur[static_cast<size_t>(firstNonWs)] == ' ' ||
                cur[static_cast<size_t>(firstNonWs)] == '\t')) {
            ++firstNonWs;
        }
        if (firstNonWs < static_cast<int>(cur.size())) {
            push(line, firstNonWs, line, static_cast<int>(cur.size()));
        }

        // 3. Full line including leading whitespace.
        push(line, 0, line, static_cast<int>(cur.size()));

        // 4. Enclosing indentation blocks. If the cursor is on a blank line,
        // we anchor to the nearest non-blank line above so blank trailing
        // lines still expand to their parent block.
        int anchor = line;
        while (anchor >= 0 && indentOf(anchor) < 0) --anchor;
        if (anchor >= 0) {
            std::vector<int> blockStarts{anchor};
            int seekIndent = indentOf(anchor);
            for (int up = anchor - 1; up >= 0; --up) {
                const int ind = indentOf(up);
                if (ind < 0) continue;
                if (ind < seekIndent) {
                    blockStarts.push_back(up);
                    seekIndent = ind;
                    if (seekIndent == 0) break;
                }
            }
            for (int startLi : blockStarts) {
                const int blockIndent = indentOf(startLi);
                int endLi = startLi;
                for (int down = startLi + 1; down < total; ++down) {
                    const int ind = indentOf(down);
                    if (ind < 0) {
                        endLi = down;  // tentatively absorb blank
                        continue;
                    }
                    if (ind > blockIndent) endLi = down;
                    else break;
                }
                // Trim trailing blank lines off the block boundary.
                while (endLi > startLi && indentOf(endLi) < 0) --endLi;
                push(startLi, 0, endLi, lineEndCol(endLi));
            }
        }

        // 5. Whole document.
        push(0, 0, total - 1, lineEndCol(total - 1));

        // Build the SelectionRange tree: outermost first, with each layer
        // becoming the `parent` of the next. The returned node is the
        // innermost selection (smallest range covering the cursor).
        nlohmann::json node = nullptr;
        for (auto it = ranges.rbegin(); it != ranges.rend(); ++it) {
            nlohmann::json layer = {
                {"range", {
                    {"start", {{"line", (*it)[0]}, {"character", (*it)[1]}}},
                    {"end",   {{"line", (*it)[2]}, {"character", (*it)[3]}}},
                }},
            };
            if (!node.is_null()) layer["parent"] = node;
            node = layer;
        }
        out.push_back(node.is_null() ? nlohmann::json::object() : node);
    }
    return out;
}

// Build a CodeAction list for the given document range. The first slice
// surfaces `jtml fix` as a single quick-fix that rewrites the whole document
// when there is anything mechanically repairable, and per-diagnostic granular
// actions for the LSP-published JTML_FIX_* codes that share the same edit.
// CodeActions returned here use a `WorkspaceEdit` so editors apply them with a
// single user gesture.
std::string camelActionNameFromLabel(const std::string& label) {
    std::vector<std::string> words;
    std::string current;
    for (char ch : label) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            current.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        } else if (!current.empty()) {
            words.push_back(current);
            current.clear();
        }
    }
    if (!current.empty()) words.push_back(current);
    if (words.empty()) return "handleClick";

    std::string name = words.front();
    for (size_t i = 1; i < words.size(); ++i) {
        if (words[i].empty()) continue;
        words[i][0] = static_cast<char>(std::toupper(static_cast<unsigned char>(words[i][0])));
        name += words[i];
    }
    if (!name.empty() && std::isdigit(static_cast<unsigned char>(name[0]))) {
        name = "action" + name;
    }
    return name.empty() ? "handleClick" : name;
}

std::string uniqueActionName(const TextDocument& doc, const std::string& base) {
    std::string candidate = base.empty() ? "handleClick" : base;
    auto exists = [&](const std::string& name) {
        return symbolListContains(documentSymbolsFor(doc), name);
    };
    if (!exists(candidate)) return candidate;
    for (int i = 2; i < 1000; ++i) {
        std::string next = candidate + std::to_string(i);
        if (!exists(next)) return next;
    }
    return candidate + "Next";
}

nlohmann::json eventActionQuickFixes(const TextDocument& doc,
                                     const nlohmann::json& contextDiagnostics) {
    nlohmann::json actions = nlohmann::json::array();
    if (!contextDiagnostics.is_array()) return actions;
    const auto lines = linesOf(doc.text);
    static const std::regex quotedLabel(R"JTML("([^"]*)")JTML");
    for (const auto& diag : contextDiagnostics) {
        if (diag.value("code", "") != "JTML_EVENT_ACTION") continue;
        if (!diag.contains("range") || !diag["range"].contains("start")) continue;
        const int lineNo = diag["range"]["start"].value("line", 0);
        if (lineNo < 0 || lineNo >= static_cast<int>(lines.size())) continue;
        const std::string& line = lines[static_cast<size_t>(lineNo)];
        if (line.find("click") == std::string::npos) continue;

        std::string label;
        std::smatch match;
        if (std::regex_search(line, match, quotedLabel) && match.size() > 1) {
            label = match[1].str();
        }
        const std::string action = uniqueActionName(doc, camelActionNameFromLabel(label));
        const int endCol = static_cast<int>(line.size());
        std::string insertion = "\nwhen " + action + "\n  show \"" +
                                (label.empty() ? "Clicked" : label) + "\"\n";
        if (!doc.text.empty() && doc.text.back() != '\n') insertion = "\n" + insertion;

        nlohmann::json edits = nlohmann::json::array();
        edits.push_back({
            {"range", {
                {"start", {{"line", lineNo}, {"character", endCol}}},
                {"end",   {{"line", lineNo}, {"character", endCol}}},
            }},
            {"newText", " " + action},
        });
        // If the broken event line is also the end of the document, avoid two
        // separate insertions at the exact same position. Some LSP clients
        // apply equal-position insertions in unspecified order.
        if (lineNo == static_cast<int>(lines.size() - 1)) {
            edits[0]["newText"] = " " + action + insertion;
        } else {
            edits.push_back({
                {"range", {
                    {"start", {{"line", static_cast<int>(lines.size() - 1)},
                                 {"character", static_cast<int>(lines.back().size())}}},
                    {"end",   {{"line", static_cast<int>(lines.size() - 1)},
                                 {"character", static_cast<int>(lines.back().size())}}},
                }},
                {"newText", insertion},
            });
        }
        actions.push_back({
            {"title", "Create `when " + action + "` and wire this click"},
            {"kind", "quickfix"},
            {"diagnostics", nlohmann::json::array({diag})},
            {"edit", {{"changes", {{doc.uri, edits}}}}},
            {"isPreferred", true},
        });
    }
    return actions;
}

nlohmann::json codeActionsFor(
        const TextDocument& doc,
        const nlohmann::json& contextDiagnostics) {
    nlohmann::json actions = nlohmann::json::array();
    for (const auto& action : eventActionQuickFixes(doc, contextDiagnostics)) {
        actions.push_back(action);
    }

    auto fix = jtml::fixSource(doc.text);
    if (!fix.changed) return actions;

    nlohmann::json edit = {
        {"changes", {
            {doc.uri, nlohmann::json::array({
                {
                    {"range", fullDocumentRange(doc.text)},
                    {"newText", fix.source},
                },
            })},
        }},
    };

    // The umbrella action runs the same repair pipeline as `jtml fix -w`.
    actions.push_back({
        {"title", "Run jtml fix on this document"},
        {"kind", "source.fixAll.jtml"},
        {"edit", edit},
        {"isPreferred", true},
    });

    // For every diagnostic in the request whose code matches one of the
    // mechanical repair codes the fix engine emits, attach a per-diagnostic
    // quick-fix that points at the same edit. Editors render these next to
    // the squiggle so users see context-relevant repair offers.
    if (contextDiagnostics.is_array()) {
        for (const auto& diag : contextDiagnostics) {
            const std::string code = diag.value("code", "");
            std::string title;
            if (code == "JTML_FIX_HEADER")          title = "Add `jtml 2` Friendly header";
            else if (code == "JTML_FIX_TABS")       title = "Replace tab indentation with spaces";
            else if (code == "JTML_FIX_TRAILING_SPACE") title = "Remove trailing whitespace";
            else if (code == "JTML_FIX_FINAL_NEWLINE") title = "Add final newline";
            else if (code == "JTML_INDENTATION")    title = "Normalize indentation";
            else continue;
            actions.push_back({
                {"title", title},
                {"kind", "quickfix"},
                {"diagnostics", nlohmann::json::array({diag})},
                {"edit", edit},
            });
        }
    }
    return actions;
}

} // namespace

int cmdLsp(const Options&) {
    std::map<std::string, TextDocument> documents;
    std::string workspaceRoot;
    bool shutdownRequested = false;

    nlohmann::json request;
    while (readMessage(request)) {
        const std::string method = request.value("method", "");
        const bool isRequest = request.contains("id");

        try {
            if (method == "initialize") {
                workspaceRoot = normalizeWorkspaceRoot(request.value("params", nlohmann::json::object()));
                writeMessage(responseFor(request, {
                    {"serverInfo", {{"name", "jtml-lsp"}, {"version", versionString()}}},
                    {"capabilities", {
                        {"textDocumentSync", 1},
                        {"documentFormattingProvider", true},
                        {"completionProvider", {{"triggerCharacters", {" ", "\n"}}}},
                        {"hoverProvider", true},
                        {"documentSymbolProvider", true},
                        {"definitionProvider", true},
                        {"workspaceSymbolProvider", true},
                        {"renameProvider", {{"prepareProvider", true}}},
                        {"referencesProvider", true},
                        {"codeActionProvider", {{"codeActionKinds",
                            {"quickfix", "source.fixAll.jtml"}}}},
                        {"documentHighlightProvider", true},
                        {"signatureHelpProvider", {{"triggerCharacters", {"(", ","}}}},
                        {"selectionRangeProvider", true},
                    }},
                }));
            } else if (method == "initialized") {
                // Notification only.
            } else if (method == "shutdown") {
                shutdownRequested = true;
                writeMessage(responseFor(request, nullptr));
            } else if (method == "exit") {
                return shutdownRequested ? 0 : 1;
            } else if (method == "textDocument/didOpen") {
                const auto& doc = request["params"]["textDocument"];
                TextDocument stored;
                stored.uri = doc.value("uri", "");
                stored.path = pathFromUri(stored.uri);
                stored.text = doc.value("text", "");
                documents[stored.uri] = stored;
                publishDiagnostics(documents[stored.uri]);
            } else if (method == "textDocument/didChange") {
                const std::string uri = request["params"]["textDocument"].value("uri", "");
                auto& doc = documents[uri];
                doc.uri = uri;
                doc.path = pathFromUri(uri);
                doc.text = textFromContentChanges(request["params"], doc.text);
                publishDiagnostics(doc);
            } else if (method == "textDocument/didSave") {
                const std::string uri = request["params"]["textDocument"].value("uri", "");
                auto& doc = documents[uri];
                doc.uri = uri;
                doc.path = pathFromUri(uri);
                if (request["params"].contains("text") && request["params"]["text"].is_string()) {
                    doc.text = request["params"]["text"].get<std::string>();
                } else if (!doc.path.empty() && std::filesystem::exists(doc.path)) {
                    doc.text = readFile(doc.path);
                }
                publishDiagnostics(doc);
            } else if (method == "textDocument/formatting") {
                const std::string uri = request["params"]["textDocument"].value("uri", "");
                auto it = documents.find(uri);
                if (it == documents.end()) {
                    writeMessage(errorFor(request, -32602, "Document is not open"));
                    continue;
                }
                const std::string formatted = formatDocument(it->second);
                writeMessage(responseFor(request, nlohmann::json::array({
                    {
                        {"range", fullDocumentRange(it->second.text)},
                        {"newText", formatted},
                    }
                })));
            } else if (method == "textDocument/completion") {
                const std::string uri = request["params"]["textDocument"].value("uri", "");
                auto it = documents.find(uri);
                writeMessage(responseFor(request, {
                    {"isIncomplete", false},
                    {"items", it == documents.end() ? completionItems() : completionItemsFor(it->second)},
                }));
            } else if (method == "textDocument/hover") {
                const std::string uri = request["params"]["textDocument"].value("uri", "");
                auto it = documents.find(uri);
                if (it == documents.end()) {
                    writeMessage(responseFor(request, nullptr));
                    continue;
                }
                const int line = request["params"]["position"].value("line", 0);
                const int character = request["params"]["position"].value("character", 0);
                auto hover = hoverFor(it->second, line, character);
                if (hover.is_null()) hover = hoverForSymbol(it->second, line, character);
                writeMessage(responseFor(request, hover));
            } else if (method == "textDocument/documentSymbol") {
                const std::string uri = request["params"]["textDocument"].value("uri", "");
                auto it = documents.find(uri);
                if (it == documents.end()) {
                    writeMessage(responseFor(request, nlohmann::json::array()));
                    continue;
                }
                writeMessage(responseFor(request, documentSymbolsFor(it->second)));
            } else if (method == "textDocument/definition") {
                const std::string uri = request["params"]["textDocument"].value("uri", "");
                auto it = documents.find(uri);
                if (it == documents.end()) {
                    writeMessage(responseFor(request, nullptr));
                    continue;
                }
                const int line = request["params"]["position"].value("line", 0);
                const int character = request["params"]["position"].value("character", 0);
                writeMessage(responseFor(request, definitionFor(it->second, line, character)));
            } else if (method == "textDocument/references") {
                const std::string uri = request["params"]["textDocument"].value("uri", "");
                auto it = documents.find(uri);
                if (it == documents.end()) {
                    writeMessage(responseFor(request, nlohmann::json::array()));
                    continue;
                }
                const int line = request["params"]["position"].value("line", 0);
                const int character = request["params"]["position"].value("character", 0);
                bool includeDeclaration = true;
                if (request["params"].contains("context") &&
                    request["params"]["context"].contains("includeDeclaration")) {
                    includeDeclaration = request["params"]["context"]
                        .value("includeDeclaration", true);
                }
                writeMessage(responseFor(request,
                    referencesFor(workspaceRoot, documents, it->second,
                                  line, character, includeDeclaration)));
            } else if (method == "textDocument/documentHighlight") {
                const std::string uri = request["params"]["textDocument"].value("uri", "");
                auto it = documents.find(uri);
                if (it == documents.end()) {
                    writeMessage(responseFor(request, nlohmann::json::array()));
                    continue;
                }
                const int line = request["params"]["position"].value("line", 0);
                const int character = request["params"]["position"].value("character", 0);
                writeMessage(responseFor(request,
                    documentHighlightsFor(it->second, line, character)));
            } else if (method == "textDocument/signatureHelp") {
                const std::string uri = request["params"]["textDocument"].value("uri", "");
                auto it = documents.find(uri);
                if (it == documents.end()) {
                    writeMessage(responseFor(request, nullptr));
                    continue;
                }
                const int line = request["params"]["position"].value("line", 0);
                const int character = request["params"]["position"].value("character", 0);
                writeMessage(responseFor(request,
                    signatureHelpFor(workspaceRoot, documents, it->second, line, character)));
            } else if (method == "textDocument/selectionRange") {
                const std::string uri = request["params"]["textDocument"].value("uri", "");
                auto it = documents.find(uri);
                if (it == documents.end()) {
                    writeMessage(responseFor(request, nlohmann::json::array()));
                    continue;
                }
                const auto positions = request["params"].value("positions", nlohmann::json::array());
                writeMessage(responseFor(request, selectionRangesFor(it->second, positions)));
            } else if (method == "textDocument/codeAction") {
                const std::string uri = request["params"]["textDocument"].value("uri", "");
                auto it = documents.find(uri);
                if (it == documents.end()) {
                    writeMessage(responseFor(request, nlohmann::json::array()));
                    continue;
                }
                nlohmann::json contextDiagnostics = nlohmann::json::array();
                if (request["params"].contains("context") &&
                    request["params"]["context"].contains("diagnostics")) {
                    contextDiagnostics = request["params"]["context"]["diagnostics"];
                }
                writeMessage(responseFor(request,
                    codeActionsFor(it->second, contextDiagnostics)));
            } else if (method == "workspace/symbol") {
                const std::string query = request["params"].value("query", "");
                writeMessage(responseFor(request, workspaceSymbolsFor(workspaceRoot, documents, query)));
            } else if (method == "textDocument/prepareRename") {
                const std::string uri = request["params"]["textDocument"].value("uri", "");
                auto it = documents.find(uri);
                if (it == documents.end()) {
                    writeMessage(responseFor(request, nullptr));
                    continue;
                }
                const int line = request["params"]["position"].value("line", 0);
                const int character = request["params"]["position"].value("character", 0);
                writeMessage(responseFor(request, prepareRenameFor(it->second, line, character)));
            } else if (method == "textDocument/rename") {
                const std::string uri = request["params"]["textDocument"].value("uri", "");
                auto it = documents.find(uri);
                if (it == documents.end()) {
                    writeMessage(responseFor(request, nullptr));
                    continue;
                }
                const int line = request["params"]["position"].value("line", 0);
                const int character = request["params"]["position"].value("character", 0);
                const std::string newName = request["params"].value("newName", "");
                writeMessage(responseFor(request,
                    renameFor(workspaceRoot, documents, it->second, line, character, newName)));
            } else if (isRequest) {
                writeMessage(errorFor(request, -32601, "Method not found: " + method));
            }
        } catch (const std::exception& e) {
            if (isRequest) writeMessage(errorFor(request, -32603, e.what()));
        }
    }

    return 0;
}

} // namespace jtml::cli
