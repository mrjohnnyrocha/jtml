// cli/cmd_serve.cpp — `jtml serve`, `jtml studio`, `jtml demo`, `jtml tutorial`.
//
// `serve`          : transpile once, start HTTP + WebSocket, stream bindings.
// `serve --watch`  : poll source mtime; hot-swap interpreter on change.
// `studio`/`demo`  : full IDE — editor, diagnostics, formatter, preview, tutorial.
#include "commands.h"
#include "diagnostic_json.h"
#include "studio_shell.h"

#include "jtml/formatter.h"
#include "jtml/fix.h"
#include "jtml/interpreter.h"
#include "jtml/linter.h"
#include "jtml/transpiler.h"
#include "httplib.h"
#include "json.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace jtml::cli {

using jtml::JtmlFormatter;
using jtml::JtmlLinter;

namespace {

/* ── Shared parse helper ─────────────────────────────────── */
std::vector<std::unique_ptr<ASTNode>> parseProgramFromNormalizedSource(const std::string& code) {
    Lexer lexer(code);
    auto tokens = lexer.tokenize();
    if (!lexer.getErrors().empty()) {
        std::ostringstream oss;
        for (const auto& e : lexer.getErrors()) oss << "Lexer Error: " << e << "\n";
        throw std::runtime_error(oss.str());
    }
    Parser parser(std::move(tokens));
    auto program = parser.parseProgram();
    if (!parser.getErrors().empty()) {
        std::ostringstream oss;
        for (const auto& e : parser.getErrors()) oss << "Parser Error: " << e << "\n";
        throw std::runtime_error(oss.str());
    }
    return program;
}

std::vector<std::unique_ptr<ASTNode>> parseProgramFromSource(const std::string& code) {
    return parseProgramFromNormalizedSource(jtml::normalizeSourceSyntax(code));
}

std::string withInitialBindings(std::string html, const std::string& bindingsJson) {
    const std::string injection =
        "<script>window.__jtml_bindings = " + bindingsJson + ";</script>";
    auto pos = html.find("<script>");
    if (pos != std::string::npos) html.insert(pos, injection);
    else                          html.append(injection);
    return html;
}

nlohmann::json runtimeContractJson(uint16_t wsPort) {
    return {
        {"version", versionString()},
        {"endpoints", {
            {"health", "/api/health"},
            {"bindings", "/api/bindings"},
            {"state", "/api/state"},
            {"components", "/api/components"},
            {"componentDefinitions", "/api/component-definitions"},
            {"runtime", "/api/runtime"},
            {"event", "/api/event"},
            {"componentAction", "/api/component-action"},
        }},
        {"eventRequest", {
            {"elementId", "elem_1"},
            {"eventType", "onClick"},
            {"args", nlohmann::json::array({"handlerName()"})},
        }},
        {"transport", {
            {"http", "/api/event"},
            {"websocket", "ws://localhost:" + std::to_string(wsPort)},
        }},
    };
}

nlohmann::json runtimeSnapshotJson(Interpreter& interpreter, uint16_t wsPort) {
    return {
        {"ok", true},
        {"contract", runtimeContractJson(wsPort)},
        {"bindings", nlohmann::json::parse(interpreter.getBindingsJSON())},
        {"state", nlohmann::json::parse(interpreter.getStateJSON())},
        {"componentDefinitions", nlohmann::json::parse(interpreter.getComponentDefinitionsJSON())},
    };
}

void setJson(httplib::Response& res, const nlohmann::json& body) {
    res.set_content(body.dump(), "application/json");
}

bool looksLikeEmail(const std::string& email) {
    const auto at = email.find('@');
    const auto dot = email.find('.', at == std::string::npos ? 0 : at + 1);
    return at != std::string::npos && at > 0 && dot != std::string::npos && dot + 1 < email.size();
}

std::string displayNameFromEmail(const std::string& email) {
    const auto at = email.find('@');
    std::string local = at == std::string::npos ? email : email.substr(0, at);
    std::replace(local.begin(), local.end(), '.', ' ');
    std::replace(local.begin(), local.end(), '_', ' ');
    std::replace(local.begin(), local.end(), '-', ' ');
    if (local.empty()) return "Demo User";
    bool newWord = true;
    for (char& ch : local) {
        if (std::isspace(static_cast<unsigned char>(ch))) {
            newWord = true;
            continue;
        }
        ch = static_cast<char>(newWord
            ? std::toupper(static_cast<unsigned char>(ch))
            : std::tolower(static_cast<unsigned char>(ch)));
        newWord = false;
    }
    return local;
}

void installDemoDataApi(httplib::Server& svr) {
    auto users = [] {
        return nlohmann::json::array({
            {{"id", "ada"},   {"name", "Ada Lovelace"},   {"email", "ada@example.com"}},
            {{"id", "grace"}, {"name", "Grace Hopper"},   {"email", "grace@example.com"}},
            {{"id", "katherine"}, {"name", "Katherine Johnson"}, {"email", "katherine@example.com"}},
        });
    };

    svr.Get("/api/users", [users](const httplib::Request&, httplib::Response& res) {
        setJson(res, users());
    });
    svr.Get("/api/posts", [](const httplib::Request&, httplib::Response& res) {
        setJson(res, nlohmann::json::array({
            {{"id", 1}, {"title", "Build with Friendly JTML"}, {"status", "published"}},
            {{"id", 2}, {"title", "Ship a local Studio demo"}, {"status", "draft"}},
        }));
    });
    svr.Post("/api/login", [](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body = nlohmann::json::object();
        try {
            if (!req.body.empty()) body = nlohmann::json::parse(req.body);
        } catch (...) {
            body = nlohmann::json::object();
        }
        const std::string email = body.value("email", std::string{"ada@example.com"});
        if (!looksLikeEmail(email)) {
            res.status = 422;
            setJson(res, {
                {"ok", false},
                {"error", "Enter a valid email address"},
            });
            return;
        }
        setJson(res, {
            {"ok", true},
            {"user", {{"name", displayNameFromEmail(email)}, {"email", email}}},
            {"token", "demo-token"},
        });
    });
}

void installRuntimeApi(httplib::Server& svr,
                       std::unique_ptr<Interpreter>& interpreter,
                       uint16_t wsPort) {
    svr.set_default_headers({
        {"Access-Control-Allow-Origin",  "*"},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type"},
    });
    svr.Options(R"(/api/.*)", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });
    installDemoDataApi(svr);
    svr.Get("/api/health", [wsPort](const httplib::Request&, httplib::Response& res) {
        setJson(res, {
            {"ok", true},
            {"version", versionString()},
            {"contract", runtimeContractJson(wsPort)},
        });
    });
    svr.Get("/api/bindings", [&interpreter](const httplib::Request&, httplib::Response& res) {
        try {
            setJson(res, {{"ok", true}, {"bindings", nlohmann::json::parse(interpreter->getBindingsJSON())}});
        } catch (const std::exception& e) {
            setJson(res, errorResponseJson(e.what()));
        }
    });
    svr.Get("/api/state", [&interpreter](const httplib::Request&, httplib::Response& res) {
        try {
            setJson(res, {{"ok", true}, {"state", nlohmann::json::parse(interpreter->getStateJSON())}});
        } catch (const std::exception& e) {
            setJson(res, errorResponseJson(e.what()));
        }
    });
    svr.Get("/api/components", [&interpreter](const httplib::Request&, httplib::Response& res) {
        try {
            setJson(res, {{"ok", true}, {"components", nlohmann::json::parse(interpreter->getComponentsJSON())}});
        } catch (const std::exception& e) {
            setJson(res, errorResponseJson(e.what()));
        }
    });
    svr.Get("/api/component-definitions", [&interpreter](const httplib::Request&, httplib::Response& res) {
        try {
            setJson(res, {{"ok", true}, {"componentDefinitions", nlohmann::json::parse(interpreter->getComponentDefinitionsJSON())}});
        } catch (const std::exception& e) {
            setJson(res, errorResponseJson(e.what()));
        }
    });
    svr.Get("/api/runtime", [&interpreter, wsPort](const httplib::Request&, httplib::Response& res) {
        try {
            setJson(res, runtimeSnapshotJson(*interpreter, wsPort));
        } catch (const std::exception& e) {
            setJson(res, errorResponseJson(e.what()));
        }
    });
    svr.Post("/api/event", [&interpreter, wsPort](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            std::string id   = body.value("elementId", std::string{});
            std::string ev   = body.value("eventType",  std::string{});
            nlohmann::json args = body.value("args", nlohmann::json::array());
            std::string bindings, err;
            bool ok = interpreter->dispatchEvent(id, ev, args, bindings, err);
            nlohmann::json j; j["ok"] = ok;
            if (ok) {
                j["bindings"] = nlohmann::json::parse(bindings);
                j["state"] = nlohmann::json::parse(interpreter->getStateJSON());
                j["contract"] = runtimeContractJson(wsPort);
            } else {
                j["error"] = err;
            }
            setJson(res, j);
        } catch (const std::exception& e) {
            setJson(res, errorResponseJson(e.what()));
        }
    });
    svr.Post("/api/component-action", [&interpreter, wsPort](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            std::string componentId = body.value("componentId", std::string{});
            std::string action = body.value("action", std::string{});
            nlohmann::json args = body.value("args", nlohmann::json::array());
            std::string bindings, err;
            bool ok = interpreter->dispatchComponentAction(componentId, action, args, bindings, err);
            nlohmann::json j; j["ok"] = ok;
            if (ok) {
                j["bindings"] = nlohmann::json::parse(bindings);
                j["state"] = nlohmann::json::parse(interpreter->getStateJSON());
                j["components"] = nlohmann::json::parse(interpreter->getComponentsJSON());
                j["contract"] = runtimeContractJson(wsPort);
            } else {
                j["error"] = err;
            }
            setJson(res, j);
        } catch (const std::exception& e) {
            setJson(res, errorResponseJson(e.what()));
        }
    });
}

std::string mimeTypeFor(const std::filesystem::path& path) {
    const std::string ext = path.extension().string();
    if (ext == ".html") return "text/html";
    if (ext == ".css")  return "text/css";
    if (ext == ".js")   return "application/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".svg")  return "image/svg+xml";
    if (ext == ".png")  return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif")  return "image/gif";
    if (ext == ".webp") return "image/webp";
    if (ext == ".ico")  return "image/x-icon";
    if (ext == ".txt")  return "text/plain";
    if (ext == ".wasm") return "application/wasm";
    return "application/octet-stream";
}

bool tryServeStaticAsset(const std::filesystem::path& root,
                         const httplib::Request& req,
                         httplib::Response& res) {
    namespace fs = std::filesystem;
    std::string rawPath = req.path;
    if (rawPath.empty() || rawPath == "/" || rawPath.rfind("/api/", 0) == 0) {
        return false;
    }
    while (!rawPath.empty() && rawPath.front() == '/') rawPath.erase(rawPath.begin());

    fs::path requested = fs::weakly_canonical(root / rawPath);
    fs::path canonicalRoot = fs::weakly_canonical(root);
    std::error_code ec;
    auto rel = fs::relative(requested, canonicalRoot, ec);
    if (ec || rel.empty() || rel.native().rfind("..", 0) == 0 ||
        !fs::is_regular_file(requested) || requested.extension() == ".jtml") {
        return false;
    }

    std::ifstream file(requested, std::ios::binary);
    if (!file.is_open()) return false;
    std::ostringstream body;
    body << file.rdbuf();
    res.set_content(body.str(), mimeTypeFor(requested));
    return true;
}

/* ── Tutorial lesson loading ─────────────────────────────── */
struct Lesson {
    std::string slug;
    std::string title;
    std::string prose;
    std::string code;
};

struct StudioDoc {
    std::string slug;
    std::string title;
    std::string category;
    std::string prose;
};

std::string extractTitle(const std::string& markdown, const std::string& fallback) {
    std::istringstream iss(markdown);
    std::string line;
    while (std::getline(iss, line)) {
        auto p = line.find_first_not_of(" \t");
        if (p == std::string::npos) continue;
        if (line[p] == '#') {
            auto after = line.find_first_not_of("# \t", p);
            if (after != std::string::npos) return line.substr(after);
        }
        break;
    }
    return fallback;
}

std::vector<Lesson> loadLessons(const std::filesystem::path& root) {
    std::vector<Lesson> lessons;
    if (!std::filesystem::exists(root)) return lessons;
    std::vector<std::filesystem::path> dirs;
    for (const auto& entry : std::filesystem::directory_iterator(root))
        if (entry.is_directory()) dirs.push_back(entry.path());
    std::sort(dirs.begin(), dirs.end());
    for (const auto& dir : dirs) {
        auto proseFile = dir / "lesson.md";
        auto codeFile  = dir / "code.jtml";
        if (!std::filesystem::exists(proseFile) || !std::filesystem::exists(codeFile))
            continue;
        Lesson l;
        l.slug  = dir.filename().string();
        l.prose = readFile(proseFile.string());
        l.code  = readFile(codeFile.string());
        l.title = extractTitle(l.prose, l.slug);
        lessons.push_back(std::move(l));
    }
    return lessons;
}

std::vector<StudioDoc> loadStudioDocs(const std::filesystem::path& root) {
    std::vector<StudioDoc> docs;
    if (!std::filesystem::exists(root)) return docs;

    const std::vector<std::pair<std::string, std::string>> curated = {
        {"README.md", "Overview"},
        {"language-reference.md", "Reference"},
        {"ai-authoring-contract.md", "AI native"},
        {"media-graphics-roadmap.md", "Media"},
        {"jtml-competitive-features-roadmap.md", "Roadmap"},
        {"jtml-friendly-grammar-and-implementation-plan.md", "Language"},
        {"runtime-http-contract.md", "Runtime"},
        {"language-server.md", "Tooling"},
        {"deployment.md", "Deployment"},
        {"embedding-c-api.md", "Interop"},
        {"jtml-element-dictionary.md", "Elements"},
    };

    for (const auto& [filename, category] : curated) {
        const auto file = root / filename;
        if (!std::filesystem::exists(file)) continue;
        StudioDoc doc;
        doc.slug = file.stem().string();
        doc.prose = readFile(file.string());
        doc.title = extractTitle(doc.prose, doc.slug);
        doc.category = category;
        docs.push_back(std::move(doc));
    }
    return docs;
}

/* ── `jtml serve` (one-shot) ─────────────────────────────── */
static void serveOnce(const std::vector<std::unique_ptr<ASTNode>>& program,
                      const std::string& inputFile,
                      int port) {
    JtmlTranspiler transpiler;
    InterpreterConfig cfg;
    cfg.wsPort = static_cast<uint16_t>(port + 80);
    transpiler.setWebSocketPort(cfg.wsPort);
    std::string html = transpiler.transpile(program);

    std::unique_ptr<Interpreter> interpreter;
    {
        SilenceStdout silence;
        interpreter = std::make_unique<Interpreter>(transpiler, cfg);
        interpreter->interpret(program);
    }
    html = withInitialBindings(std::move(html), interpreter->getBindingsJSON());

    httplib::Server svr;
    installRuntimeApi(svr, interpreter, cfg.wsPort);
    svr.Get("/", [&html](const httplib::Request&, httplib::Response& res) {
        res.set_content(html, "text/html");
    });
    const std::filesystem::path staticRoot = std::filesystem::path(inputFile).parent_path();
    svr.Get(R"(/.*)", [staticRoot](const httplib::Request& req, httplib::Response& res) {
        if (!tryServeStaticAsset(staticRoot, req, res)) {
            res.status = 404;
            res.set_content("Not found", "text/plain");
        }
    });

    std::cout << "jtml serve: http://localhost:" << port
              << "  (WebSocket on " << cfg.wsPort << ")\n" << std::flush;
    SilenceStdout silence;
    svr.listen("0.0.0.0", port);
}

/* ── `jtml serve --watch` ────────────────────────────────── */
static void serveWatching(const std::string& inputFile, int port, SyntaxMode syntax) {
    std::mutex  htmlMutex;
    std::string currentHtml;

    JtmlTranspiler transpiler;
    InterpreterConfig cfg;
    cfg.wsPort = static_cast<uint16_t>(port + 80);
    transpiler.setWebSocketPort(cfg.wsPort);
    std::unique_ptr<Interpreter> interpreter;
    {
        SilenceStdout silence;
        interpreter = std::make_unique<Interpreter>(transpiler, cfg);
    }

    auto rebuild = [&](bool firstLoad) {
        try {
            auto program = parseProgramFromFile(inputFile, syntax);
            std::string html = transpiler.transpile(program);
            {
                SilenceStdout silence;
                if (firstLoad) interpreter->interpret(program);
                else           interpreter->reload(program);
            }
            html = withInitialBindings(std::move(html), interpreter->getBindingsJSON());
            std::lock_guard<std::mutex> lk(htmlMutex);
            currentHtml = std::move(html);
            std::cerr << (firstLoad ? "[watch] built " : "[watch] reloaded ") << inputFile << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[watch] error: " << e.what() << "\n";
        }
    };

    rebuild(true);

    std::atomic<bool> stopFlag{false};
    std::thread watcher([&]() {
        namespace fs = std::filesystem;
        std::map<fs::path, fs::file_time_type> lastWrites;
        auto refreshFiles = [&]() {
            std::map<fs::path, fs::file_time_type> writes;
            for (const auto& f : collectSourceFiles(inputFile, syntax))
                try { writes[f] = fs::last_write_time(f); } catch (...) {}
            return writes;
        };
        try { lastWrites = refreshFiles(); } catch (...) {}
        while (!stopFlag.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            try {
                auto writes = refreshFiles();
                if (writes != lastWrites) { lastWrites = writes; rebuild(false); }
            } catch (...) {}
        }
    });

    httplib::Server svr;
    installRuntimeApi(svr, interpreter, cfg.wsPort);
    svr.Get("/", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lk(htmlMutex);
        res.set_content(currentHtml, "text/html");
    });
    const std::filesystem::path staticRoot = std::filesystem::path(inputFile).parent_path();
    svr.Get(R"(/.*)", [staticRoot](const httplib::Request& req, httplib::Response& res) {
        if (!tryServeStaticAsset(staticRoot, req, res)) {
            res.status = 404;
            res.set_content("Not found", "text/plain");
        }
    });

    std::cout << "jtml serve --watch: http://localhost:" << port
              << "  (WebSocket on " << cfg.wsPort << ")\n"
              << "Watching: " << inputFile << "\n" << std::flush;
    SilenceStdout silence;
    svr.listen("0.0.0.0", port);
    stopFlag.store(true);
    if (watcher.joinable()) watcher.join();
}

/* ── `jtml studio` (merged IDE + tutorial) ───────────────── */
static void serveStudio(int port) {
    auto lessons = loadLessons("tutorial");
    auto docs = loadStudioDocs("docs");

    static JtmlTranspiler transpiler;
    InterpreterConfig cfg;
    cfg.wsPort         = static_cast<uint16_t>(port + 80);
    cfg.startWebSocket = true;
    static std::unique_ptr<Interpreter> interpreter;
    if (!interpreter) {
        SilenceStdout silence;
        interpreter = std::make_unique<Interpreter>(transpiler, cfg);
    }

    httplib::Server svr;
    svr.set_default_headers({
        {"Access-Control-Allow-Origin",  "*"},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type"},
    });
    svr.Options(R"(/api/.*)", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });
    installDemoDataApi(svr);

    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(kStudioShellHTML, "text/html");
    });

    svr.Get("/api/lessons", [&lessons](const httplib::Request&, httplib::Response& res) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& l : lessons)
            arr.push_back({{"slug", l.slug}, {"title", l.title}});
        res.set_content(arr.dump(), "application/json");
    });

    svr.Get(R"(/api/lesson/([A-Za-z0-9_\-]+))",
            [&lessons](const httplib::Request& req, httplib::Response& res) {
        const std::string slug = req.matches[1];
        for (const auto& l : lessons) {
            if (l.slug == slug) {
                res.set_content(
                    nlohmann::json{{"slug",l.slug},{"title",l.title},{"prose",l.prose},{"code",l.code}}.dump(),
                    "application/json");
                return;
            }
        }
        res.status = 404;
        res.set_content(R"({"error":"lesson not found"})", "application/json");
    });

    svr.Get("/api/docs", [&docs](const httplib::Request&, httplib::Response& res) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& d : docs)
            arr.push_back({{"slug", d.slug}, {"title", d.title}, {"category", d.category}});
        res.set_content(arr.dump(), "application/json");
    });

    svr.Get(R"(/api/doc/([A-Za-z0-9_\-]+))",
            [&docs](const httplib::Request& req, httplib::Response& res) {
        const std::string slug = req.matches[1];
        for (const auto& d : docs) {
            if (d.slug == slug) {
                res.set_content(
                    nlohmann::json{{"slug",d.slug},{"title",d.title},{"category",d.category},{"prose",d.prose}}.dump(),
                    "application/json");
                return;
            }
        }
        res.status = 404;
        res.set_content(R"({"error":"doc not found"})", "application/json");
    });

    svr.Post("/api/run", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            std::string code = body.value("code", std::string{});
            std::string classic;
            std::vector<std::unique_ptr<ASTNode>> program;
            {
                SilenceStdout silence;
                classic = jtml::normalizeSourceSyntax(code);
                program = parseProgramFromNormalizedSource(classic);
            }
            JtmlLinter linter;
            auto diags = linter.lint(program);
            std::string html;
            {
                SilenceStdout silence;
                html = transpiler.transpile(program);
                interpreter->reload(program);
                html = withInitialBindings(std::move(html), interpreter->getBindingsJSON());
            }
            res.set_content(
                nlohmann::json{{"ok",true},{"html",html},{"classic",classic},{"diagnostics",lintDiagnosticsToJson(diags)}}.dump(),
                "application/json");
        } catch (const std::exception& e) {
            res.set_content(errorResponseJson(e.what()).dump(), "application/json");
        }
    });

    svr.Post("/api/format", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            std::string code = body.value("code", std::string{});
            const bool friendlyInput =
                jtml::isFriendlySyntax(code) || jtml::looksLikeFriendlySyntax(code);
            std::vector<std::unique_ptr<ASTNode>> program;
            {
                SilenceStdout silence;
                program = parseProgramFromSource(code);
            }
            if (friendlyInput) {
                res.set_content(
                    nlohmann::json{{"ok",true},{"code",jtml::formatFriendlySource(code)}}.dump(),
                    "application/json");
                return;
            }
            JtmlFormatter formatter;
            res.set_content(
                nlohmann::json{{"ok",true},{"code",formatter.format(program)}}.dump(),
                "application/json");
        } catch (const std::exception& e) {
            res.set_content(errorResponseJson(e.what()).dump(), "application/json");
        }
    });

    svr.Post("/api/fix", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            std::string code = body.value("code", std::string{});
            auto fixed = jtml::fixSource(code, SyntaxMode::Auto);
            res.set_content(
                nlohmann::json{
                    {"ok", true},
                    {"code", fixed.source},
                    {"changed", fixed.changed},
                    {"changes", fixChangesToJson(fixed.changes)},
                }.dump(),
                "application/json");
        } catch (const std::exception& e) {
            res.set_content(errorResponseJson(e.what()).dump(), "application/json");
        }
    });

    svr.Post("/api/lint", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            std::string code = body.value("code", std::string{});
            std::vector<std::unique_ptr<ASTNode>> program;
            {
                SilenceStdout silence;
                program = parseProgramFromSource(code);
            }
            JtmlLinter linter;
            auto diags = linter.lint(program);
            res.set_content(
                nlohmann::json{{"ok",true},{"diagnostics",lintDiagnosticsToJson(diags)}}.dump(),
                "application/json");
        } catch (const std::exception& e) {
            res.set_content(errorResponseJson(e.what()).dump(), "application/json");
        }
    });

    svr.Post("/api/export", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            std::string code = body.value("code", std::string{});
            std::string classic;
            std::vector<std::unique_ptr<ASTNode>> program;
            {
                SilenceStdout silence;
                classic = jtml::normalizeSourceSyntax(code);
                program = parseProgramFromNormalizedSource(classic);
            }
            JtmlLinter linter;
            auto diags = linter.lint(program);
            std::string html;
            {
                SilenceStdout silence;
                html = transpiler.transpile(program);
                interpreter->reload(program);
                html = withInitialBindings(std::move(html), interpreter->getBindingsJSON());
            }
            res.set_content(
                nlohmann::json{{"ok",true},{"html",html},{"classic",classic},{"diagnostics",lintDiagnosticsToJson(diags)}}.dump(),
                "application/json");
        } catch (const std::exception& e) {
            res.set_content(errorResponseJson(e.what()).dump(), "application/json");
        }
    });

    svr.Post("/api/event", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            std::string id   = body.value("elementId", std::string{});
            std::string ev   = body.value("eventType",  std::string{});
            nlohmann::json args = body.value("args", nlohmann::json::array());
            std::string bindings, err;
            bool ok;
            {
                SilenceStdout silence;
                ok = interpreter->dispatchEvent(id, ev, args, bindings, err);
            }
            nlohmann::json j; j["ok"] = ok;
            if (ok) j["bindings"] = nlohmann::json::parse(bindings);
            else    j["error"]    = err;
            res.set_content(j.dump(), "application/json");
        } catch (const std::exception& e) {
            res.set_content(errorResponseJson(e.what()).dump(), "application/json");
        }
    });

    std::cout << "JTML Studio: http://localhost:" << port
              << "  (WebSocket on " << cfg.wsPort << ")\n";
    if (!lessons.empty())
        std::cout << "Tutorial: " << lessons.size() << " lesson(s) from ./tutorial/\n";
    if (!docs.empty())
        std::cout << "Docs: " << docs.size() << " guide(s) from ./docs/\n";
    else
        std::cout << "Tutorial: no ./tutorial/ found (run from the repo root to include it)\n";
    std::cout << std::flush;

    SilenceStdout silence;
    svr.listen("0.0.0.0", port);
}

} // namespace

/* ── Public command entry points ─────────────────────────── */

int cmdServe(const Options& o) {
    if (o.watch) serveWatching(o.inputFile, o.port, o.syntax);
    else {
        auto program = parseProgramFromFile(o.inputFile, o.syntax);
        serveOnce(program, o.inputFile, o.port);
    }
    return 0;
}

int cmdDev(const Options& o) {
    namespace fs = std::filesystem;
    Options dev = o;
    fs::path input = dev.inputFile.empty() ? fs::path{"."} : fs::path{dev.inputFile};
    if (fs::is_directory(input)) input /= "index.jtml";
    if (!fs::exists(input)) {
        throw std::runtime_error("Dev input not found: " + input.string());
    }
    dev.inputFile = input.string();
    dev.watch = true;
    std::cout << "jtml dev: zero-config hot reload for " << dev.inputFile << "\n";
    return cmdServe(dev);
}

int cmdStudio(const Options& o) {
    serveStudio(o.port);
    return 0;
}

// Backward-compatible aliases — existing docs and scripts keep working.
int cmdDemo    (const Options& o) { return cmdStudio(o); }

} // namespace jtml::cli
