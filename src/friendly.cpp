#include "jtml/friendly.h"

#include <algorithm>
#include <cctype>
#include <numeric>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace jtml {
namespace {

std::vector<int>* activeFriendlySourceMap = nullptr;
int activeFriendlySourceLine = 0;

struct Line {
    int indent = 0;
    std::string text;
    int number = 0;
};

struct OpenBlock {
    int indent = 0;
    std::string close;
    std::vector<std::string> afterClose;
    int sourceLine = 0;
    std::string actionName;
};

struct ComponentDef {
    std::string name;
    std::vector<std::string> params;
    std::vector<Line> body;
    int number = 0;
};

std::string ltrim(std::string s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    return s;
}

std::string rtrim(std::string s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
    return s;
}

std::string trim(std::string s) {
    return rtrim(ltrim(std::move(s)));
}

bool startsWith(const std::string& s, const std::string& prefix) {
    return s.rfind(prefix, 0) == 0;
}

std::string stripFriendlyLineComment(const std::string& line) {
    char quoteChar = '\0';
    int braceDepth = 0;
    auto isHexColor = [&](size_t hash) {
        size_t i = hash + 1;
        size_t count = 0;
        while (i < line.size() && std::isxdigit(static_cast<unsigned char>(line[i]))) {
            ++i;
            ++count;
        }
        if (count != 3 && count != 4 && count != 6 && count != 8) return false;
        return i == line.size() || !std::isalnum(static_cast<unsigned char>(line[i]));
    };
    for (size_t i = 0; i < line.size(); ++i) {
        char ch = line[i];
        if (quoteChar != '\0') {
            if (ch == '\\' && i + 1 < line.size()) {
                ++i;
            } else if (ch == quoteChar) {
                quoteChar = '\0';
            }
            continue;
        }
        if (ch == '"' || ch == '\'') {
            quoteChar = ch;
            continue;
        }
        if (ch == '{') ++braceDepth;
        if (ch == '}' && braceDepth > 0) --braceDepth;
        if (braceDepth == 0 && ch == '/' && i + 1 < line.size() && line[i + 1] == '/') {
            return rtrim(line.substr(0, i));
        }
        if (braceDepth == 0 && ch == '#') {
            if (i == 0 || std::isspace(static_cast<unsigned char>(line[i - 1]))) {
                if (isHexColor(i)) continue;
                return rtrim(line.substr(0, i));
            }
        }
    }
    return line;
}

bool isQuoted(const std::string& token) {
    return token.size() >= 2 &&
           ((token.front() == '"' && token.back() == '"') ||
            (token.front() == '\'' && token.back() == '\''));
}

std::string unquote(const std::string& token) {
    return isQuoted(token) ? token.substr(1, token.size() - 2) : token;
}

std::string quote(const std::string& s) {
    std::ostringstream out;
    out << '"';
    for (char ch : s) {
        if (ch == '"' || ch == '\\') out << '\\';
        out << ch;
    }
    out << '"';
    return out.str();
}

std::string hexEncode(const std::string& s) {
    static constexpr char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(s.size() * 2);
    for (unsigned char ch : s) {
        out.push_back(hex[(ch >> 4) & 0x0f]);
        out.push_back(hex[ch & 0x0f]);
    }
    return out;
}

std::string joinTokens(const std::vector<std::string>& tokens, size_t start) {
    std::ostringstream out;
    for (size_t i = start; i < tokens.size(); ++i) {
        if (i > start) out << ' ';
        out << tokens[i];
    }
    return out.str();
}

std::string interpolateQuotedLiteral(const std::string& quotedLiteral) {
    const char quoteChar = quotedLiteral.front();
    std::string inner = quotedLiteral.substr(1, quotedLiteral.size() - 2);
    std::vector<std::string> parts;
    std::string literal;

    auto flushLiteral = [&]() {
        if (!literal.empty()) {
            parts.push_back(quote(literal));
            literal.clear();
        }
    };

    for (size_t i = 0; i < inner.size(); ++i) {
        char ch = inner[i];
        if (ch == '\\' && i + 1 < inner.size()) {
            literal.push_back(ch);
            literal.push_back(inner[++i]);
            continue;
        }
        if (ch == '{') {
            size_t close = inner.find('}', i + 1);
            if (close == std::string::npos) {
                return quotedLiteral;
            }
            flushLiteral();
            std::string embedded = trim(inner.substr(i + 1, close - i - 1));
            if (embedded.empty()) {
                literal += "{}";
            } else {
                parts.push_back(embedded);
            }
            i = close;
        } else {
            literal.push_back(ch);
        }
    }
    flushLiteral();

    if (parts.empty()) return std::string(1, quoteChar) + inner + std::string(1, quoteChar);
    std::ostringstream out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) out << " + ";
        out << parts[i];
    }
    return out.str();
}

std::string friendlyExpression(const std::string& expr) {
    std::string trimmed = trim(expr);
    if (!isQuoted(trimmed)) {
        std::ostringstream out;
        for (size_t i = 0; i < trimmed.size(); ++i) {
            char ch = trimmed[i];
            if (ch != '"' && ch != '\'') {
                out << ch;
                continue;
            }

            const char quoteChar = ch;
            const size_t start = i;
            ++i;
            bool closed = false;
            for (; i < trimmed.size(); ++i) {
                if (trimmed[i] == '\\' && i + 1 < trimmed.size()) {
                    ++i;
                    continue;
                }
                if (trimmed[i] == quoteChar) {
                    closed = true;
                    break;
                }
            }
            if (!closed) return trimmed;

            std::string literal = trimmed.substr(start, i - start + 1);
            std::string lowered = interpolateQuotedLiteral(literal);
            if (lowered != literal && lowered.find(" + ") != std::string::npos) {
                out << "(" << lowered << ")";
            } else {
                out << lowered;
            }
        }
        return out.str();
    }

    return interpolateQuotedLiteral(trimmed);
}

std::vector<std::string> splitTokens(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;
    char quoteChar = '\0';
    int parenDepth = 0;
    int bracketDepth = 0;
    int braceDepth = 0;

    auto flush = [&]() {
        if (!current.empty()) {
            tokens.push_back(current);
            current.clear();
        }
    };

    for (size_t i = 0; i < line.size(); ++i) {
        char ch = line[i];
        if (quoteChar != '\0') {
            current.push_back(ch);
            if (ch == '\\' && i + 1 < line.size()) {
                current.push_back(line[++i]);
            } else if (ch == quoteChar) {
                quoteChar = '\0';
            }
            continue;
        }

        if (ch == '"' || ch == '\'') {
            quoteChar = ch;
            current.push_back(ch);
            continue;
        }

        if (ch == '(') ++parenDepth;
        if (ch == ')') --parenDepth;
        if (ch == '[') ++bracketDepth;
        if (ch == ']') --bracketDepth;
        if (ch == '{') ++braceDepth;
        if (ch == '}') --braceDepth;

        if (std::isspace(static_cast<unsigned char>(ch)) &&
            parenDepth == 0 && bracketDepth == 0 && braceDepth == 0) {
            flush();
        } else {
            current.push_back(ch);
        }
    }
    if (quoteChar != '\0') {
        throw std::runtime_error("Unterminated string in friendly JTML line: " + line);
    }
    flush();
    return tokens;
}

std::vector<Line> collectLines(const std::string& source) {
    std::vector<Line> lines;
    std::istringstream in(source);
    std::string raw;
    int lineNumber = 0;
    bool skippedHeader = false;

    while (std::getline(in, raw)) {
        ++lineNumber;
        if (!raw.empty() && raw.back() == '\r') raw.pop_back();
        std::string withoutRight = stripFriendlyLineComment(rtrim(raw));
        std::string text = ltrim(withoutRight);
        if (text.empty() || startsWith(text, "//") || startsWith(text, "#")) {
            continue;
        }
        if (!skippedHeader && text == "jtml 2") {
            skippedHeader = true;
            continue;
        }
        skippedHeader = true;

        int indent = 0;
        for (char ch : withoutRight) {
            if (ch == ' ') {
                ++indent;
            } else if (ch == '\t') {
                throw std::runtime_error("Tabs are not allowed for friendly JTML indentation at line " + std::to_string(lineNumber));
            } else {
                break;
            }
        }
        lines.push_back({indent, text, lineNumber});
    }
    return lines;
}

const std::set<std::string>& eventNames() {
    static const std::set<std::string> names = {
        "click", "input", "change", "keyup", "keydown", "hover", "scroll", "submit",
        "dragover", "drop"
    };
    return names;
}

const std::set<std::string>& booleanAttributes() {
    static const std::set<std::string> names = {
        "required", "disabled", "checked", "selected", "autofocus",
        "multiple", "readonly", "hidden", "controls", "autoplay",
        "loop", "muted", "ordered"
    };
    return names;
}

const std::set<std::string>& commonAttributes() {
    static const std::set<std::string> names = {
        "class", "id", "style", "href", "src", "alt", "title", "type",
        "placeholder", "value", "name", "for", "role", "data-testid",
        "data-state", "data-jtml-route", "data-jtml-route-name",
        "data-jtml-route-params", "data-jtml-route-load", "data-jtml-link", "aria-label",
        "aria-describedby", "aria-hidden", "data-jtml-dropzone", "data-jtml-media-controller",
        "data-jtml-chart", "data-jtml-chart-data", "data-jtml-chart-by",
        "data-jtml-chart-value", "data-jtml-chart-color",
        "data-jtml-chart-axis-x", "data-jtml-chart-axis-y", "data-jtml-chart-legend",
        "data-jtml-chart-grid", "data-jtml-chart-stacked",
        "data-jtml-timeline", "data-jtml-timeline-duration", "data-jtml-timeline-easing",
        "data-jtml-timeline-animates", "data-jtml-timeline-autoplay", "data-jtml-timeline-repeat",
        "data-jtml-image-proc", "data-jtml-image-src", "data-jtml-image-into",
        "data-jtml-image-w", "data-jtml-image-h", "data-jtml-image-fit",
        "data-jtml-image-x", "data-jtml-image-y", "data-jtml-image-filter", "data-jtml-image-amount",
        "data-jtml-scene3d", "data-jtml-scene", "data-jtml-camera",
        "data-jtml-controls", "data-jtml-renderer", "data-jtml-scene3d-controller",
        "scene", "camera", "renderer",
        "method", "action", "target", "rel", "accept", "capture", "enctype",
        "poster", "preload", "playsinline", "loading", "decoding",
        "width", "height", "viewBox", "viewbox", "fill", "stroke", "x", "y", "cx", "cy", "r",
        "x1", "y1", "x2", "y2", "d", "points", "stroke-width", "stroke-linecap",
        "stroke-linejoin", "stroke-dasharray", "opacity", "fill-opacity", "stroke-opacity",
        "rx", "ry",
        "min", "max", "step", "pattern", "rows", "cols"
    };
    return names;
}

// Element dictionary: friendly JTML aliases that map to real HTML tags.
// Some aliases also inject default attributes (e.g. checkbox → input type="checkbox").
struct ElementAlias {
    std::string htmlTag;
    std::vector<std::pair<std::string, std::string>> defaultAttrs; // key → value
};

const std::map<std::string, ElementAlias>& elementAliases() {
    static const std::map<std::string, ElementAlias> aliases = {
        {"text",     {"p",      {}}},
        {"box",      {"div",    {}}},
        {"link",     {"a",      {}}},
        {"image",    {"img",    {}}},
        {"embed",    {"iframe", {}}},
        {"graphic",  {"svg",    {{"role", "\"img\""}}}},
        {"scene3d",  {"canvas", {{"data-jtml-scene3d", "\"true\""}, {"role", "\"img\""}}}},
        {"group",    {"g",      {}}},
        {"bar",      {"rect",   {}}},
        {"dot",      {"circle", {}}},
        {"line",     {"line",   {}}},
        {"path",     {"path",   {}}},
        {"polyline", {"polyline", {}}},
        {"polygon",  {"polygon", {}}},
        {"file",     {"input",  {{"type", "\"file\""}}}},
        {"dropzone", {"input",  {{"type", "\"file\""}, {"data-jtml-dropzone", "\"true\""}, {"multiple", ""}}}},
        {"checkbox", {"input",  {{"type", "\"checkbox\""}}}},
    };
    return aliases;
}

// Check if a tag name is a friendly alias
bool isElementAlias(const std::string& tag) {
    return elementAliases().count(tag) > 0;
}

// Resolve 'list' to 'ul' or 'ol' depending on whether 'ordered' appears
// in the token list. Also handles element dictionary aliases.
std::string resolveTagName(const std::string& tag, const std::vector<std::string>& tokens) {
    if (tag == "list") {
        for (const auto& t : tokens) {
            if (t == "ordered") return "ol";
        }
        return "ul";
    }
    if (tag == "item") return "li";
    auto it = elementAliases().find(tag);
    if (it != elementAliases().end()) return it->second.htmlTag;
    return tag;
}

std::string classicEventName(const std::string& event) {
    if (event == "click") return "onClick";
    if (event == "input") return "onInput";
    if (event == "change") return "onChange";
    if (event == "keyup") return "onKeyUp";
    if (event == "keydown") return "onKeyDown";
    if (event == "hover") return "onMouseOver";
    if (event == "scroll") return "onScroll";
    if (event == "submit") return "onSubmit";
    if (event == "dragover") return "onDragOver";
    if (event == "drop") return "onDrop";
    return event;
}

std::string mediaControllerInitialState() {
    return "{currentTime: 0, duration: 0, paused: true, ended: false, muted: false, volume: 1, playbackRate: 1, readyState: 0, src: \"\"}";
}

std::string scene3dControllerInitialState() {
    return "{scene: \"\", camera: \"orbit\", controls: \"orbit\", renderer: \"auto\", status: \"idle\", hostRendered: false, width: 0, height: 0}";
}

std::string actionCall(const std::string& action) {
    return action.find('(') == std::string::npos ? action + "()" : action;
}

std::string setterNameFor(const std::string& name) {
    std::string out = "set";
    bool uppercaseNext = true;
    for (char ch : name) {
        if (!std::isalnum(static_cast<unsigned char>(ch))) {
            uppercaseNext = true;
            continue;
        }
        if (uppercaseNext) {
            out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
            uppercaseNext = false;
        } else {
            out.push_back(ch);
        }
    }
    return out;
}

bool isVoidTag(const std::string& tag) {
    static const std::set<std::string> names = {
        "area", "base", "br", "col", "embed", "hr", "img", "input",
        "link", "meta", "param", "source", "track", "wbr"
    };
    // Resolve aliases before checking void status
    std::string resolved = tag;
    if (tag == "image") resolved = "img";
    else if (tag == "checkbox" || tag == "file" || tag == "dropzone") resolved = "input";
    else {
        auto it = elementAliases().find(tag);
        if (it != elementAliases().end()) resolved = it->second.htmlTag;
    }
    return names.find(resolved) != names.end();
}

bool isComponentName(const std::string& name) {
    return !name.empty() && std::isupper(static_cast<unsigned char>(name.front()));
}

bool isKnownLineKeyword(const std::string& word) {
    static const std::set<std::string> names = {
        "use", "let", "get", "const", "when", "make", "page", "show", "if", "else",
        "for", "while", "try", "catch", "finally", "return", "throw", "break",
        "continue", "slot", "style", "route", "effect", "store", "redirect", "guard",
        "invalidate", "extern", "export", "timeline", "animate"
    };
    return names.find(word) != names.end();
}

bool isFriendlyAssignmentOperator(const std::string& op) {
    return op == "=" || op == "+=" || op == "-=" || op == "*=" ||
           op == "/=" || op == "%=";
}

struct FetchSpec {
    bool valid = false;
    std::string url;
    std::string method = "\"GET\"";
    std::string bodyExpr;
    std::string refreshAction;
    std::string cache;
    std::string credentials;
    std::string timeoutMs;
    std::string retryCount;
    std::string stalePolicy;
    bool lazy = false;
};

struct TypedIdentifier {
    std::string name;
    std::string type;
};

TypedIdentifier parseTypedIdentifier(const std::vector<std::string>& tokens,
                                     size_t& index,
                                     int lineNumber) {
    if (index >= tokens.size()) {
        throw std::runtime_error("Expected identifier at line " + std::to_string(lineNumber));
    }

    TypedIdentifier out;
    std::string token = tokens[index++];
    const size_t colon = token.find(':');
    if (colon != std::string::npos) {
        out.name = token.substr(0, colon);
        out.type = token.substr(colon + 1);
        if (out.type.empty()) {
            if (index >= tokens.size()) {
                throw std::runtime_error("Expected type after ':' at line " + std::to_string(lineNumber));
            }
            out.type = tokens[index++];
        }
    } else {
        out.name = token;
        if (index < tokens.size() && tokens[index] == ":") {
            ++index;
            if (index >= tokens.size()) {
                throw std::runtime_error("Expected type after ':' at line " + std::to_string(lineNumber));
            }
            out.type = tokens[index++];
        }
    }

    if (out.name.empty()) {
        throw std::runtime_error("Expected identifier before ':' at line " + std::to_string(lineNumber));
    }
    return out;
}

std::vector<TypedIdentifier> parseTypedIdentifiers(const std::vector<std::string>& tokens,
                                                   size_t start,
                                                   int lineNumber) {
    std::vector<TypedIdentifier> out;
    size_t index = start;
    while (index < tokens.size()) {
        out.push_back(parseTypedIdentifier(tokens, index, lineNumber));
    }
    return out;
}

FetchSpec parseFetchSpec(const std::vector<std::string>& tokens, size_t start, int lineNumber) {
    FetchSpec spec;
    if (start >= tokens.size() || tokens[start] != "fetch") return spec;
    if (start + 1 >= tokens.size() || !isQuoted(tokens[start + 1])) {
        throw std::runtime_error("Expected 'fetch \"url\"' at line " + std::to_string(lineNumber));
    }

    spec.valid = true;
    spec.url = tokens[start + 1];
    size_t i = start + 2;
    while (i < tokens.size()) {
        if (tokens[i] == "method") {
            if (i + 1 >= tokens.size() || !isQuoted(tokens[i + 1])) {
                throw std::runtime_error("Expected quoted method after 'method' at line " + std::to_string(lineNumber));
            }
            spec.method = tokens[i + 1];
            i += 2;
            continue;
        }
        if (tokens[i] == "body") {
            if (i + 1 >= tokens.size()) {
                throw std::runtime_error("Expected expression after fetch body at line " + std::to_string(lineNumber));
            }
            spec.bodyExpr = friendlyExpression(tokens[i + 1]);
            i += 2;
            continue;
        }
        if (tokens[i] == "refresh") {
            if (i + 1 >= tokens.size()) {
                throw std::runtime_error("Expected action name after 'refresh' at line " + std::to_string(lineNumber));
            }
            spec.refreshAction = tokens[i + 1];
            i += 2;
            continue;
        }
        if (tokens[i] == "cache") {
            if (i + 1 >= tokens.size() || !isQuoted(tokens[i + 1])) {
                throw std::runtime_error("Expected quoted cache policy after 'cache' at line " + std::to_string(lineNumber));
            }
            spec.cache = tokens[i + 1];
            i += 2;
            continue;
        }
        if (tokens[i] == "credentials") {
            if (i + 1 >= tokens.size() || !isQuoted(tokens[i + 1])) {
                throw std::runtime_error("Expected quoted credentials policy after 'credentials' at line " + std::to_string(lineNumber));
            }
            spec.credentials = tokens[i + 1];
            i += 2;
            continue;
        }
        if (tokens[i] == "timeout") {
            if (i + 1 >= tokens.size()) {
                throw std::runtime_error("Expected milliseconds after 'timeout' at line " + std::to_string(lineNumber));
            }
            spec.timeoutMs = tokens[i + 1];
            i += 2;
            continue;
        }
        if (tokens[i] == "retry") {
            if (i + 1 >= tokens.size()) {
                throw std::runtime_error("Expected retry count after 'retry' at line " + std::to_string(lineNumber));
            }
            spec.retryCount = tokens[i + 1];
            i += 2;
            continue;
        }
        if (tokens[i] == "stale") {
            if (i + 1 >= tokens.size()) {
                throw std::runtime_error("Expected 'keep' or 'clear' after 'stale' at line " + std::to_string(lineNumber));
            }
            std::string policy = tokens[i + 1];
            if (isQuoted(policy)) policy = policy.substr(1, policy.size() - 2);
            if (policy != "keep" && policy != "clear") {
                throw std::runtime_error("Expected stale policy 'keep' or 'clear' at line " + std::to_string(lineNumber));
            }
            spec.stalePolicy = quote(policy);
            i += 2;
            continue;
        }
        if (tokens[i] == "lazy") {
            spec.lazy = true;
            i += 1;
            continue;
        }
        throw std::runtime_error("Unsupported fetch option '" + tokens[i] + "' at line " + std::to_string(lineNumber));
    }
    return spec;
}

std::string friendlyRouteHref(const std::string& target, int lineNumber) {
    if (!isQuoted(target)) {
        throw std::runtime_error("Expected quoted route target after 'to' at line " + std::to_string(lineNumber));
    }
    std::string value = target.substr(1, target.size() - 2);
    if (!value.empty() && value.front() == '#') return quote(value);
    if (!value.empty() && value.front() == '/') return quote("#" + value);
    return quote(value);
}

bool shouldTreatAsInlineText(const std::string& tag,
                             const std::vector<std::string>& tokens,
                             size_t index) {
    if (index >= tokens.size()) return false;
    const std::string& token = tokens[index];
    if (eventNames().count(token) || token == "into" ||
        booleanAttributes().count(token) || commonAttributes().count(token)) {
        return false;
    }
    // key=value tokens are HTML attributes, not inline text
    if (token.find('=') != std::string::npos) return false;
    if (isQuoted(token)) return true;
    if (tag == "input" || tag == "textarea") return false;
    if (index + 1 >= tokens.size()) return true;
    return eventNames().count(tokens[index + 1]) || tokens[index + 1] == "into";
}

std::string makeIndent(int level) {
    return std::string(static_cast<size_t>(level) * 4, ' ');
}

void emitLine(std::ostringstream& out,
              int level,
              const std::string& line,
              std::vector<int>* sourceMap = nullptr,
              int sourceLine = 0) {
    out << makeIndent(level) << line << "\n";
    if (sourceMap) {
        sourceMap->push_back(sourceLine);
    } else if (activeFriendlySourceMap) {
        activeFriendlySourceMap->push_back(activeFriendlySourceLine);
    }
}

std::string cssStringLiteral(const std::string& css) {
    std::ostringstream out;
    out << '"';
    for (char ch : css) {
        if (ch == '"' || ch == '\\') out << '\\';
        out << ch;
    }
    out << '"';
    return out.str();
}

std::string scopedSelector(const std::string& selector) {
    std::ostringstream out;
    std::istringstream parts(selector);
    std::string part;
    bool first = true;
    while (std::getline(parts, part, ',')) {
        part = trim(part);
        if (part.empty()) continue;
        if (!first) out << ", ";
        first = false;
        if (part == "body" || part == ":root") {
            out << "[data-jtml-app]";
        } else if (startsWith(part, "@")) {
            out << part;
        } else {
            out << "[data-jtml-app] " << part;
        }
    }
    return out.str();
}

std::string declarationLine(std::string declaration) {
    declaration = trim(std::move(declaration));
    if (declaration.empty()) return "";
    const char last = declaration.back();
    if (last != ';' && last != '{' && last != '}') declaration.push_back(';');
    return declaration;
}

std::string friendlyStyleBlockToCss(const std::vector<Line>& lines, size_t begin, size_t end) {
    std::ostringstream css;
    size_t i = begin;
    while (i < end) {
        const Line& selectorLine = lines[i];
        size_t blockEnd = i + 1;
        while (blockEnd < end && lines[blockEnd].indent > selectorLine.indent) ++blockEnd;

        if (blockEnd == i + 1) {
            const std::string raw = trim(selectorLine.text);
            if (!raw.empty()) css << raw << "\n";
            ++i;
            continue;
        }

        const std::string selector = scopedSelector(selectorLine.text);
        if (!selector.empty()) {
            css << selector << " {\n";
            for (size_t j = i + 1; j < blockEnd; ++j) {
                const std::string declaration = declarationLine(lines[j].text);
                if (!declaration.empty()) css << "  " << declaration << "\n";
            }
            css << "}\n";
        }
        i = blockEnd;
    }
    return css.str();
}

std::string translateUse(const std::vector<std::string>& tokens, int lineNumber) {
    // use "./path"
    if (tokens.size() == 2 && isQuoted(tokens[1])) {
        return "import " + tokens[1] + "\\\\";
    }
    // use Name from "./path"
    if (tokens.size() == 4 && tokens[2] == "from" && isQuoted(tokens[3])) {
        return "import " + tokens[3] + "\\\\";
    }
    // use { Name1, Name2 } from "./path"
    // Detect the { ... } from "path" pattern
    if (tokens.size() >= 5 && tokens[1] == "{") {
        // Find closing brace
        size_t closeBrace = 2;
        while (closeBrace < tokens.size() && tokens[closeBrace] != "}") ++closeBrace;
        if (closeBrace < tokens.size() &&
            closeBrace + 2 < tokens.size() &&
            tokens[closeBrace + 1] == "from" &&
            isQuoted(tokens[closeBrace + 2])) {
            return "import " + tokens[closeBrace + 2] + "\\\\";
        }
    }
    throw std::runtime_error("Unsupported use statement at line " + std::to_string(lineNumber));
}

size_t findBlockEnd(const std::vector<Line>& lines, size_t headerIndex) {
    const int headerIndent = lines[headerIndex].indent;
    size_t end = headerIndex + 1;
    while (end < lines.size() && lines[end].indent > headerIndent) ++end;
    return end;
}

std::vector<Line> normalizeChildBlock(const std::vector<Line>& lines,
                                      size_t begin,
                                      size_t end) {
    std::vector<Line> block;
    if (begin >= end) return block;
    const int baseIndent = lines[begin].indent;
    for (size_t i = begin; i < end; ++i) {
        Line copy = lines[i];
        copy.indent = std::max(0, copy.indent - baseIndent);
        block.push_back(copy);
    }
    return block;
}

std::vector<Line> stripExportModifiers(std::vector<Line> lines) {
    for (auto& line : lines) {
        auto tokens = splitTokens(line.text);
        if (tokens.empty() || tokens[0] != "export") continue;
        if (tokens.size() == 1) {
            throw std::runtime_error("Expected declaration after 'export' at line " +
                                     std::to_string(line.number));
        }
        const auto pos = line.text.find("export");
        line.text = trim(line.text.substr(pos + std::string("export").size()));
    }
    return lines;
}

std::string mapStoreToken(const std::string& token,
                          const std::string& storeName,
                          const std::set<std::string>& fields,
                          const std::set<std::string>& actions) {
    if (actions.count(token)) return storeName + "_" + token;
    for (const auto& action : actions) {
        if (startsWith(token, action + "(")) {
            return storeName + "_" + token;
        }
    }
    if (fields.count(token)) return storeName + "." + token;
    for (const auto& field : fields) {
        const std::string dotPrefix = field + ".";
        if (startsWith(token, dotPrefix)) {
            return storeName + "." + token;
        }
        const std::string subscriptPrefix = field + "[";
        if (startsWith(token, subscriptPrefix)) {
            return storeName + "." + token;
        }
    }
    return token;
}

std::string transformStoreLineText(const std::string& text,
                                   const std::string& storeName,
                                   const std::set<std::string>& fields,
                                   const std::set<std::string>& actions,
                                   int lineNumber) {
    auto tokens = splitTokens(text);
    if (tokens.empty()) return text;

    if (tokens[0] == "when" && tokens.size() >= 2 && actions.count(tokens[1])) {
        tokens[1] = storeName + "_" + tokens[1];
        std::ostringstream out;
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (i > 0) out << ' ';
            out << tokens[i];
        }
        return out.str();
    }

    if (tokens[0] == "let" && tokens.size() >= 4) {
        size_t nameIndex = 1;
        auto typedName = parseTypedIdentifier(tokens, nameIndex, lineNumber);
        if (fields.count(typedName.name) && nameIndex < tokens.size() && tokens[nameIndex] == "=") {
            std::ostringstream out;
            out << storeName << "." << typedName.name << " =";
            for (size_t i = nameIndex + 1; i < tokens.size(); ++i) {
                out << " " << mapStoreToken(tokens[i], storeName, fields, actions);
            }
            return out.str();
        }
    }

    for (auto& token : tokens) {
        token = mapStoreToken(token, storeName, fields, actions);
    }

    std::ostringstream out;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0) out << ' ';
        out << tokens[i];
    }
    return out.str();
}

std::string storeActionFunctionName(const std::string& storeName, const std::string& actionName) {
    return storeName + "_" + actionName;
}

std::string storeActionReference(const std::string& storeName, const std::string& actionName) {
    return storeName + "." + actionName;
}

std::string rewriteStoreActionToken(const std::string& token,
                                    const std::map<std::string, std::string>& storeActions) {
    if (isQuoted(token)) return token;
    auto it = storeActions.find(token);
    if (it != storeActions.end()) return it->second;

    for (const auto& [qualified, lowered] : storeActions) {
        if (startsWith(token, qualified + "(")) {
            return lowered + token.substr(qualified.size());
        }
    }
    return token;
}

std::vector<Line> rewriteStoreActionRefs(const std::vector<Line>& lines,
                                         const std::map<std::string, std::string>& storeActions) {
    if (storeActions.empty()) return lines;
    std::vector<Line> rewritten;
    rewritten.reserve(lines.size());
    for (const auto& line : lines) {
        auto tokens = splitTokens(line.text);
        for (auto& token : tokens) {
            token = rewriteStoreActionToken(token, storeActions);
        }
        std::ostringstream text;
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (i > 0) text << ' ';
            text << tokens[i];
        }
        Line copy = line;
        copy.text = text.str();
        rewritten.push_back(copy);
    }
    return rewritten;
}

std::vector<Line> expandStoreLines(const std::vector<Line>& lines,
                                   std::map<std::string, std::string>& storeActions) {
    std::vector<Line> expanded;
    for (size_t i = 0; i < lines.size(); ++i) {
        auto tokens = splitTokens(lines[i].text);
        if (tokens.empty() || tokens[0] != "store") {
            expanded.push_back(lines[i]);
            continue;
        }
        if (tokens.size() != 2) {
            throw std::runtime_error("Expected 'store name' at line " + std::to_string(lines[i].number));
        }

        const std::string storeName = tokens[1];
        const size_t end = findBlockEnd(lines, i);
        if (end == i + 1) {
            throw std::runtime_error("Store '" + storeName + "' needs an indented body at line " +
                                     std::to_string(lines[i].number));
        }

        std::vector<Line> body = normalizeChildBlock(lines, i + 1, end);
        std::vector<std::pair<std::string, std::string>> fieldInitializers;
        std::set<std::string> fields;
        std::set<std::string> actions;
        for (const auto& bodyLine : body) {
            if (bodyLine.indent != 0) continue;
            auto bodyTokens = splitTokens(bodyLine.text);
            if (bodyTokens.size() >= 2 && bodyTokens[0] == "when") {
                actions.insert(bodyTokens[1]);
                storeActions[storeActionReference(storeName, bodyTokens[1])] =
                    storeActionFunctionName(storeName, bodyTokens[1]);
                continue;
            }
            if (bodyTokens.size() < 4 || bodyTokens[0] != "let") continue;
            size_t nameIndex = 1;
            auto typedName = parseTypedIdentifier(bodyTokens, nameIndex, bodyLine.number);
            if (nameIndex < bodyTokens.size() && bodyTokens[nameIndex] == "=") {
                fields.insert(typedName.name);
                fieldInitializers.push_back({
                    typedName.name,
                    friendlyExpression(joinTokens(bodyTokens, nameIndex + 1))
                });
            }
        }

        std::ostringstream dict;
        dict << "let " << storeName << " = {";
        for (size_t f = 0; f < fieldInitializers.size(); ++f) {
            if (f > 0) dict << ", ";
            dict << fieldInitializers[f].first << ": " << fieldInitializers[f].second;
        }
        dict << "}";
        expanded.push_back({lines[i].indent, dict.str(), lines[i].number});

        for (const auto& bodyLine : body) {
            auto bodyTokens = splitTokens(bodyLine.text);
            if (bodyLine.indent == 0 && bodyTokens.size() >= 4 && bodyTokens[0] == "let") {
                size_t nameIndex = 1;
                auto typedName = parseTypedIdentifier(bodyTokens, nameIndex, bodyLine.number);
                if (fields.count(typedName.name) && nameIndex < bodyTokens.size() &&
                    bodyTokens[nameIndex] == "=") {
                    continue;
                }
            }
            Line copy = bodyLine;
            copy.indent = lines[i].indent + bodyLine.indent;
            copy.text = transformStoreLineText(bodyLine.text, storeName, fields, actions, bodyLine.number);
            expanded.push_back(copy);
        }

        i = end - 1;
    }
    return expanded;
}

std::map<std::string, ComponentDef>
collectComponentDefs(const std::vector<Line>& lines) {
    std::map<std::string, ComponentDef> components;
    for (size_t i = 0; i < lines.size(); ++i) {
        auto tokens = splitTokens(lines[i].text);
        if (tokens.empty() || tokens[0] != "make") continue;
        if (tokens.size() < 2 || !isComponentName(tokens[1])) {
            throw std::runtime_error("Expected uppercase component name after 'make' at line " + std::to_string(lines[i].number));
        }
        size_t end = findBlockEnd(lines, i);
        if (end == i + 1) {
            throw std::runtime_error("Component '" + tokens[1] + "' needs an indented body at line " + std::to_string(lines[i].number));
        }

        ComponentDef def;
        def.name = tokens[1];
        def.number = lines[i].number;
        for (const auto& param : parseTypedIdentifiers(tokens, 2, lines[i].number)) {
            def.params.push_back(param.name);
        }
        def.body = normalizeChildBlock(lines, i + 1, end);
        components[def.name] = std::move(def);
        i = end - 1;
    }
    return components;
}

std::string replaceAll(std::string text, const std::string& needle, const std::string& replacement) {
    if (needle.empty()) return text;
    size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string::npos) {
        text.replace(pos, needle.size(), replacement);
        pos += replacement.size();
    }
    return text;
}

std::string substituteIdentifiers(const std::string& text,
                                  const std::map<std::string, std::string>& bindings) {
    auto tokens = splitTokens(text);
    for (auto& token : tokens) {
        if (isQuoted(token)) {
            for (const auto& [name, replacement] : bindings) {
                token = replaceAll(token, "{" + name + "}", "{" + replacement + "}");
                token = replaceAll(token, "{" + name + ".", "{" + replacement + ".");
                token = replaceAll(token, "{" + name + "[", "{" + replacement + "[");
            }
            continue;
        }
        auto it = bindings.find(token);
        if (it != bindings.end()) {
            token = it->second;
            continue;
        }
        for (const auto& [name, replacement] : bindings) {
            if (startsWith(token, name + ".") ||
                startsWith(token, name + "[") ||
                startsWith(token, name + "(")) {
                token = replacement + token.substr(name.size());
                break;
            }
        }
    }

    std::ostringstream out;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0) out << ' ';
        out << tokens[i];
    }
    return out.str();
}

std::string substituteParams(const std::string& text,
                             const std::map<std::string, std::string>& bindings) {
    return substituteIdentifiers(text, bindings);
}

void collectComponentLocalNames(const std::vector<Line>& body,
                                const ComponentDef& def,
                                std::set<std::string>& names) {
    std::set<std::string> params(def.params.begin(), def.params.end());
    for (const auto& line : body) {
        auto tokens = splitTokens(line.text);
        if (tokens.empty()) continue;
        try {
            if ((tokens[0] == "let" || tokens[0] == "get" || tokens[0] == "const") &&
                tokens.size() >= 2) {
                size_t index = 1;
                auto typedName = parseTypedIdentifier(tokens, index, line.number);
                if (!params.count(typedName.name)) names.insert(typedName.name);
            } else if (tokens[0] == "when" && tokens.size() >= 2) {
                if (!params.count(tokens[1])) names.insert(tokens[1]);
            } else if (tokens[0] == "effect" && tokens.size() == 2) {
                if (!params.count(tokens[1])) names.insert(tokens[1]);
            }
        } catch (const std::exception&) {
            // Let the main translator produce the real syntax error later.
        }
    }
}

std::map<std::string, std::string> componentLocalBindings(const ComponentDef& def,
                                                          const std::vector<Line>& body,
                                                          int& instanceCounter) {
    std::set<std::string> names;
    collectComponentLocalNames(body, def, names);

    std::map<std::string, std::string> bindings;
    if (names.empty()) return bindings;
    ++instanceCounter;
    std::string prefix = "__" + def.name + "_" + std::to_string(instanceCounter) + "_";
    for (const auto& name : names) {
        bindings[name] = prefix + name;
    }
    return bindings;
}

bool isComponentLocalDeclaration(const Line& line,
                                 const std::map<std::string, std::string>& localBindings,
                                 std::string& markerHead) {
    if (line.indent != 0) return false;
    auto tokens = splitTokens(line.text);
    if (tokens.size() < 2) return false;
    if (tokens[0] != "let" && tokens[0] != "const" && tokens[0] != "get") return false;
    size_t index = 1;
    auto typedName = parseTypedIdentifier(tokens, index, line.number);
    if (!localBindings.count(typedName.name)) return false;
    if (tokens[0] == "let") markerHead = "__define";
    else if (tokens[0] == "const") markerHead = "__const";
    else markerHead = "__derive";
    return true;
}

std::string replaceFirstToken(const std::string& text, const std::string& replacement) {
    auto tokens = splitTokens(text);
    if (tokens.empty()) return text;
    tokens[0] = replacement;
    std::ostringstream out;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0) out << ' ';
        out << tokens[i];
    }
    return out.str();
}

std::string componentMetadataValue(std::string value) {
    if (isQuoted(value)) {
        value = value.substr(1, value.size() - 2);
    }
    for (char& ch : value) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (std::isalnum(c) || ch == '_' || ch == '-' || ch == '.' || ch == ':' ||
            ch == '/' || ch == '#') {
            continue;
        }
        ch = '_';
    }
    return value;
}

std::string serializedComponentMap(const std::map<std::string, std::string>& pairs) {
    std::ostringstream out;
    bool first = true;
    for (const auto& [key, value] : pairs) {
        if (!first) out << ";";
        first = false;
        out << componentMetadataValue(key) << "=" << componentMetadataValue(value);
    }
    return out.str();
}

std::string serializedComponentParams(const std::vector<std::string>& params) {
    std::ostringstream out;
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) out << ";";
        out << componentMetadataValue(params[i]);
    }
    return out.str();
}

std::string serializedComponentBodyHex(const ComponentDef& def) {
    std::ostringstream body;
    for (const auto& line : def.body) {
        body << line.indent << ":" << line.text << "\n";
    }
    return hexEncode(body.str());
}

std::string componentDefinitionLine(const ComponentDef& def) {
    std::ostringstream line;
    line << "@template data-jtml-component-def=" << quote(def.name)
         << " data-jtml-component-def-params=" << quote(serializedComponentParams(def.params))
         << " data-jtml-source-line=" << quote(std::to_string(def.number))
         << " data-jtml-component-body-hex=" << quote(serializedComponentBodyHex(def))
         << "\\\\";
    return line.str();
}

std::string componentInstanceLine(const ComponentDef& def,
                                  int instanceId,
                                  const std::map<std::string, std::string>& localBindings,
                                  const std::map<std::string, std::string>& paramBindings,
                                  int sourceLine,
                                  const std::string& role = "",
                                  const std::string& extraAttrs = "") {
    const std::string instanceName = def.name + "_" + std::to_string(instanceId);
    std::ostringstream line;
    line << "div data-jtml-instance=" << quote(instanceName)
         << " data-jtml-component=" << quote(def.name)
         << " data-jtml-instance-id=" << quote(std::to_string(instanceId))
         << " data-jtml-component-locals=" << quote(serializedComponentMap(localBindings))
         << " data-jtml-component-params=" << quote(serializedComponentMap(paramBindings))
         << " data-jtml-source-line=" << quote(std::to_string(sourceLine));
    if (!role.empty()) {
        line << " data-jtml-component-role=" << quote(role);
    }
    if (!extraAttrs.empty()) {
        line << " " << extraAttrs;
    }
    return line.str();
}

std::vector<std::string> routeParamNames(const std::string& quotedPath) {
    std::string path = quotedPath;
    if (isQuoted(path)) path = path.substr(1, path.size() - 2);
    std::vector<std::string> params;
    std::istringstream in(path);
    std::string part;
    while (std::getline(in, part, '/')) {
        if (part.size() > 1 && part.front() == ':') {
            params.push_back(part.substr(1));
        }
    }
    return params;
}

std::string joinParamNames(const std::vector<std::string>& params) {
    std::ostringstream out;
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) out << ",";
        out << params[i];
    }
    return out.str();
}

std::vector<Line> expandComponentLinesImpl(const std::vector<Line>& lines,
                                           const std::map<std::string, ComponentDef>& components,
                                           int& instanceCounter,
                                           int depth = 0) {
    if (depth > 32) {
        throw std::runtime_error("Component expansion exceeded the recursion limit");
    }

    std::vector<Line> expanded;
    for (size_t i = 0; i < lines.size(); ++i) {
        auto tokens = splitTokens(lines[i].text);
        if (tokens.empty()) continue;

        if (tokens[0] == "make") {
            i = findBlockEnd(lines, i) - 1;
            continue;
        }

        if (tokens[0] == "route") {
            if (!(tokens.size() >= 4 && tokens[2] == "as" && isQuoted(tokens[1]) && isComponentName(tokens[3]))) {
                throw std::runtime_error("Expected 'route \"path\" as Component [layout Layout] [load fetchName...]' at line " + std::to_string(lines[i].number));
            }
            std::string layoutName;
            std::vector<std::string> routeLoads;
            size_t routeIndex = 4;
            if (routeIndex < tokens.size() && tokens[routeIndex] == "layout") {
                if (routeIndex + 1 >= tokens.size() || !isComponentName(tokens[routeIndex + 1])) {
                    throw std::runtime_error("Expected layout component after 'layout' at line " + std::to_string(lines[i].number));
                }
                layoutName = tokens[routeIndex + 1];
                routeIndex += 2;
            }
            if (routeIndex < tokens.size() && tokens[routeIndex] == "load") {
                ++routeIndex;
                if (routeIndex >= tokens.size()) {
                    throw std::runtime_error("Expected fetch name after 'load' at line " + std::to_string(lines[i].number));
                }
                for (; routeIndex < tokens.size(); ++routeIndex) {
                    if (tokens[routeIndex] == ",") continue;
                    std::string name = tokens[routeIndex];
                    if (!name.empty() && name.back() == ',') name.pop_back();
                    if (!name.empty()) routeLoads.push_back(name);
                }
            }
            if (routeIndex != tokens.size()) {
                throw std::runtime_error("Expected 'route \"path\" as Component [layout Layout] [load fetchName...]' at line " + std::to_string(lines[i].number));
            }
            const auto routeComponentIt = components.find(tokens[3]);
            if (routeComponentIt == components.end()) {
                throw std::runtime_error("Unknown route component '" + tokens[3] +
                                         "' at line " + std::to_string(lines[i].number));
            }

            const ComponentDef& def = routeComponentIt->second;
            const ComponentDef* layoutDef = nullptr;
            if (!layoutName.empty()) {
                const auto layoutIt = components.find(layoutName);
                if (layoutIt == components.end()) {
                    throw std::runtime_error("Unknown route layout component '" + layoutName +
                                             "' at line " + std::to_string(lines[i].number));
                }
                if (!layoutIt->second.params.empty()) {
                    throw std::runtime_error("Route layout component '" + layoutName +
                                             "' must not declare parameters at line " +
                                             std::to_string(lines[i].number));
                }
                layoutDef = &layoutIt->second;
            }
            const auto routeParams = routeParamNames(tokens[1]);
            if (def.params.size() != routeParams.size()) {
                throw std::runtime_error("Route component '" + def.name +
                                         "' expects " + std::to_string(def.params.size()) +
                                         " route parameter(s), but route path provides " +
                                         std::to_string(routeParams.size()) + " at line " +
                                         std::to_string(lines[i].number));
            }
            for (size_t p = 0; p < def.params.size(); ++p) {
                if (def.params[p] != routeParams[p]) {
                    throw std::runtime_error("Route parameter '" + routeParams[p] +
                                             "' must match component parameter '" + def.params[p] +
                                             "' at line " + std::to_string(lines[i].number));
                }
                expanded.push_back({
                    lines[i].indent,
                    "let " + def.params[p] + " = \"\"",
                    lines[i].number
                });
            }
            std::map<std::string, std::string> routeParamBindings;
            for (const auto& param : def.params) {
                routeParamBindings[param] = param;
            }

            expanded.push_back({
                lines[i].indent,
                "section data-jtml-route " + tokens[1] +
                    " data-jtml-route-name " + quote(tokens[3]) +
                    " data-jtml-route-params " + quote(joinParamNames(routeParams)) +
                    (routeLoads.empty() ? "" : " data-jtml-route-load " + quote(joinParamNames(routeLoads))),
                lines[i].number
            });
            std::vector<Line> body = expandComponentLinesImpl(def.body, components, instanceCounter, depth + 1);
            auto localBindings = componentLocalBindings(def, body, instanceCounter);
            int routeInstanceId = localBindings.empty() ? ++instanceCounter : instanceCounter;
            std::vector<Line> routeBody;
            for (const auto& bodyLine : body) {
                Line copy = bodyLine;
                copy.indent = bodyLine.indent;
                copy.text = substituteIdentifiers(copy.text, localBindings);
                std::string markerHead;
                if (isComponentLocalDeclaration(bodyLine, localBindings, markerHead)) {
                    copy.text = replaceFirstToken(copy.text, markerHead);
                }
                routeBody.push_back(std::move(copy));
            }

            if (!layoutDef) {
                expanded.push_back({
                    lines[i].indent + 2,
                    componentInstanceLine(def, routeInstanceId, localBindings,
                                          routeParamBindings, lines[i].number, "route"),
                    lines[i].number
                });
                for (auto copy : routeBody) {
                    copy.indent = lines[i].indent + 4 + copy.indent;
                    expanded.push_back(std::move(copy));
                }
                continue;
            }

            std::vector<Line> layoutBody = expandComponentLinesImpl(layoutDef->body, components, instanceCounter, depth + 1);
            auto layoutBindings = componentLocalBindings(*layoutDef, layoutBody, instanceCounter);
            int layoutInstanceId = layoutBindings.empty() ? ++instanceCounter : instanceCounter;
            expanded.push_back({
                lines[i].indent + 2,
                componentInstanceLine(*layoutDef, layoutInstanceId, layoutBindings, {},
                                      lines[i].number, "layout",
                                      "data-jtml-layout=" + quote(layoutDef->name)),
                lines[i].number
            });
            for (const auto& layoutLine : layoutBody) {
                auto layoutTokens = splitTokens(layoutLine.text);
                if (!layoutTokens.empty() && layoutTokens[0] == "slot") {
                    const int slotIndent = lines[i].indent + 4 + layoutLine.indent;
                    expanded.push_back({
                        slotIndent,
                        componentInstanceLine(def, routeInstanceId, localBindings,
                                              routeParamBindings, lines[i].number, "route"),
                        lines[i].number
                    });
                    for (auto copy : routeBody) {
                        copy.indent = slotIndent + 2 + copy.indent;
                        expanded.push_back(std::move(copy));
                    }
                    continue;
                }
                Line copy = layoutLine;
                copy.indent = lines[i].indent + 4 + layoutLine.indent;
                copy.text = substituteIdentifiers(copy.text, layoutBindings);
                std::string markerHead;
                if (isComponentLocalDeclaration(layoutLine, layoutBindings, markerHead)) {
                    copy.text = replaceFirstToken(copy.text, markerHead);
                }
                expanded.push_back(std::move(copy));
            }
            continue;
        }

        const auto componentIt = components.find(tokens[0]);
        if (componentIt == components.end()) {
            if (isComponentName(tokens[0])) {
                throw std::runtime_error("Unknown Friendly JTML component '" + tokens[0] +
                                         "' at line " + std::to_string(lines[i].number));
            }
            expanded.push_back(lines[i]);
            continue;
        }

        const ComponentDef& def = componentIt->second;
        if (tokens.size() - 1 != def.params.size()) {
            throw std::runtime_error("Component '" + def.name + "' expects " +
                                     std::to_string(def.params.size()) + " argument(s) at line " +
                                     std::to_string(lines[i].number));
        }

        size_t callEnd = findBlockEnd(lines, i);
        std::vector<Line> slotLines = normalizeChildBlock(lines, i + 1, callEnd);
        slotLines = expandComponentLinesImpl(slotLines, components, instanceCounter, depth + 1);

        std::map<std::string, std::string> bindings;
        for (size_t p = 0; p < def.params.size(); ++p) {
            bindings[def.params[p]] = tokens[p + 1];
        }

        std::vector<Line> body = expandComponentLinesImpl(def.body, components, instanceCounter, depth + 1);
        auto localBindings = componentLocalBindings(def, body, instanceCounter);
        // Claim a unique instance ID (componentLocalBindings may not have incremented).
        int instanceId;
        if (localBindings.empty()) {
            instanceId = ++instanceCounter;
        } else {
            instanceId = instanceCounter;
        }
        const std::string instanceMarker =
            componentInstanceLine(def, instanceId, localBindings, bindings, lines[i].number);
        expanded.push_back({lines[i].indent, instanceMarker, lines[i].number});
        for (const auto& bodyLine : body) {
            auto bodyTokens = splitTokens(bodyLine.text);
            if (!bodyTokens.empty() && bodyTokens[0] == "slot") {
                for (const auto& slotLine : slotLines) {
                    Line injected = slotLine;
                    injected.indent = lines[i].indent + 2 + bodyLine.indent + slotLine.indent;
                    expanded.push_back(injected);
                }
                continue;
            }

            Line copy = bodyLine;
            copy.indent = lines[i].indent + 2 + bodyLine.indent;
            copy.text = substituteParams(copy.text, bindings);
            copy.text = substituteIdentifiers(copy.text, localBindings);
            std::string markerHead;
            if (isComponentLocalDeclaration(bodyLine, localBindings, markerHead)) {
                copy.text = replaceFirstToken(copy.text, markerHead);
            }
            expanded.push_back(copy);
        }
        i = callEnd - 1;
    }
    return expanded;
}

std::vector<Line> expandComponentLines(const std::vector<Line>& lines,
                                       const std::map<std::string, ComponentDef>& components) {
    int instanceCounter = 0;
    return expandComponentLinesImpl(lines, components, instanceCounter);
}

struct ElementResult {
    std::string openLine;
    std::vector<std::string> bodyLines;
    std::vector<std::string> synthesizedSetters;
    bool closesWithHash = false;
    bool isVoid = false;
};

ElementResult translateChartElement(const std::vector<std::string>& tokens, int lineNumber) {
    ElementResult result;
    std::string type = "bar";
    std::string dataExpr;
    std::string byField;
    std::string valueField;
    std::string label = "\"Chart\"";
    std::string width = "\"640\"";
    std::string height = "\"320\"";
    std::string viewBox = "\"0 0 640 320\"";
    std::string color = "\"#0f766e\"";
    std::string axisXLabel;
    std::string axisYLabel;
    std::string legendStr;
    std::string gridStr;
    std::string stackedStr;
    std::vector<std::string> passthroughAttrs;

    static const std::set<std::string> chartKeywords = {
        "data", "by", "value", "label", "aria-label", "width", "height",
        "viewBox", "viewbox", "color", "axis", "legend", "grid", "stacked"
    };

    size_t i = 1;
    if (i < tokens.size() && !chartKeywords.count(tokens[i])) {
        type = tokens[i++];
    }

    while (i < tokens.size()) {
        const std::string key = tokens[i++];
        auto requireValue = [&](const std::string& option) -> std::string {
            if (i >= tokens.size()) {
                throw std::runtime_error("Expected value after chart " + option + " at line " + std::to_string(lineNumber));
            }
            return tokens[i++];
        };

        if (key == "data") { dataExpr = requireValue("data"); continue; }
        if (key == "by")   { byField = requireValue("by"); continue; }
        if (key == "value") { valueField = requireValue("value"); continue; }
        if (key == "label" || key == "aria-label") {
            label = friendlyExpression(requireValue(key)); continue;
        }
        if (key == "width")  { width  = friendlyExpression(requireValue("width")); continue; }
        if (key == "height") { height = friendlyExpression(requireValue("height")); continue; }
        if (key == "viewBox" || key == "viewbox") {
            viewBox = friendlyExpression(requireValue(key)); continue;
        }
        if (key == "color") { color = friendlyExpression(requireValue("color")); continue; }
        if (key == "axis") {
            const std::string axis = (i < tokens.size()) ? tokens[i++] : "";
            if (i < tokens.size() && tokens[i] == "label") {
                ++i;
                const std::string val = requireValue("axis label");
                if (axis == "x") axisXLabel = friendlyExpression(val);
                else if (axis == "y") axisYLabel = friendlyExpression(val);
            }
            continue;
        }
        if (key == "legend") {
            if (i < tokens.size() && (tokens[i] == "true" || tokens[i] == "false")) {
                legendStr = quote(tokens[i++]);
            } else {
                legendStr = "\"true\"";
            }
            continue;
        }
        if (key == "grid") {
            if (i < tokens.size() && (tokens[i] == "true" || tokens[i] == "false")) {
                gridStr = quote(tokens[i++]);
            } else {
                gridStr = "\"true\"";
            }
            continue;
        }
        if (key == "stacked") { stackedStr = "\"true\""; continue; }
        if (i >= tokens.size()) {
            passthroughAttrs.push_back(key);
        } else {
            passthroughAttrs.push_back(key + "=" + friendlyExpression(tokens[i++]));
        }
    }

    if (type != "bar" && type != "line") {
        throw std::runtime_error("Unsupported chart type '" + type + "' at line " + std::to_string(lineNumber) + "; supported: bar, line");
    }
    if (dataExpr.empty() || byField.empty() || valueField.empty()) {
        throw std::runtime_error("Expected 'chart " + type + " data <rows> by <field> value <field>' at line " + std::to_string(lineNumber));
    }

    std::ostringstream open;
    open << "@svg role=\"img\" aria-label=" << label
         << " data-jtml-chart=" << quote(type)
         << " data-jtml-chart-data=" << quote(dataExpr)
         << " data-jtml-chart-by=" << quote(unquote(byField))
         << " data-jtml-chart-value=" << quote(unquote(valueField))
         << " data-jtml-chart-color=" << color
         << " width=" << width
         << " height=" << height
         << " viewBox=" << viewBox;
    if (!axisXLabel.empty()) open << " data-jtml-chart-axis-x=" << axisXLabel;
    if (!axisYLabel.empty()) open << " data-jtml-chart-axis-y=" << axisYLabel;
    if (!legendStr.empty())  open << " data-jtml-chart-legend=" << legendStr;
    if (!gridStr.empty())    open << " data-jtml-chart-grid=" << gridStr;
    if (!stackedStr.empty()) open << " data-jtml-chart-stacked=" << stackedStr;
    for (const auto& attr : passthroughAttrs) open << " " << attr;
    open << "\\\\";
    result.openLine = open.str();
    result.closesWithHash = true;
    result.isVoid = false;
    return result;
}

ElementResult translateElement(const std::vector<std::string>& tokens, int lineNumber) {
    if (tokens.empty()) {
        throw std::runtime_error("Empty element line at line " + std::to_string(lineNumber));
    }

    if (tokens[0] == "chart") {
        return translateChartElement(tokens, lineNumber);
    }

    ElementResult result;
    const std::string friendlyTag = tokens[0];
    const std::string resolvedTag = resolveTagName(friendlyTag, tokens);
    size_t i = 1;
    std::vector<std::string> attrs;

    // Inject default attributes from element aliases (e.g. checkbox → type="checkbox")
    auto aliasIt = elementAliases().find(friendlyTag);
    if (aliasIt != elementAliases().end()) {
        for (const auto& kv : aliasIt->second.defaultAttrs) {
            if (kv.second.empty()) attrs.push_back(kv.first);
            else attrs.push_back(kv.first + "=" + kv.second);
        }
    }

    // For input-like elements, determine which tags accept placeholder text
    const bool isInputLike = (resolvedTag == "input" || resolvedTag == "textarea");
    const bool isFileLike = (friendlyTag == "file" || friendlyTag == "dropzone");
    const bool isScene3d = (friendlyTag == "scene3d");

    if (shouldTreatAsInlineText(resolvedTag, tokens, i)) {
        if (isFileLike || isScene3d) {
            attrs.push_back("aria-label=" + friendlyExpression(tokens[i]));
        } else if (isInputLike) {
            attrs.push_back("placeholder=" + tokens[i]);
        } else if (friendlyTag == "checkbox") {
            // For checkbox, the inline text becomes a label-like text
            // We skip inline text for checkbox (it's handled differently)
            // Actually, for checkbox "Accept Terms" should be adjacent label text
            // For now, emit as a show statement since there's no label element
            result.bodyLines.push_back("show " + friendlyExpression(tokens[i]) + "\\\\");
        } else {
            result.bodyLines.push_back("show " + friendlyExpression(tokens[i]) + "\\\\");
        }
        ++i;
    }

    while (i < tokens.size()) {
        const std::string key = tokens[i++];

        // Skip 'ordered' for list elements — it was already consumed by resolveTagName
        if (key == "ordered" && (friendlyTag == "list")) {
            continue;
        }

        // Internal expansion and shorthand authoring can produce complete
        // `key=value` tokens. Treat them as already-formed attributes so a
        // following attribute is not accidentally consumed as this value.
        if (!isQuoted(key) && key.find('=') != std::string::npos) {
            attrs.push_back(key);
            continue;
        }

        if (key == "to" && resolvedTag == "a") {
            if (i >= tokens.size()) {
                throw std::runtime_error("Expected route target after 'to' at line " + std::to_string(lineNumber));
            }
            attrs.push_back("href=\"javascript:void(0)\"");
            attrs.push_back("data-jtml-href=" + friendlyRouteHref(tokens[i++], lineNumber));
            attrs.push_back("data-jtml-link=\"true\"");
            if (i < tokens.size() && tokens[i] == "active-class") {
                ++i;
                if (i >= tokens.size()) {
                    throw std::runtime_error("Expected class name after 'active-class' at line " + std::to_string(lineNumber));
                }
                attrs.push_back("data-jtml-active-class=" + tokens[i++]);
            }
            continue;
        }

        if (eventNames().count(key)) {
            if (i >= tokens.size()) {
                throw std::runtime_error("Expected action after event '" + key + "' at line " + std::to_string(lineNumber));
            }
            attrs.push_back(classicEventName(key) + "=" + actionCall(tokens[i++]));
            continue;
        }
        if (isScene3d && (key == "scene" || key == "camera" ||
                          key == "controls" || key == "renderer")) {
            if (i >= tokens.size()) {
                throw std::runtime_error("Expected value after scene3d '" + key + "' at line " + std::to_string(lineNumber));
            }
            const std::string dataKey =
                key == "scene" ? "data-jtml-scene" :
                key == "camera" ? "data-jtml-camera" :
                key == "controls" ? "data-jtml-controls" :
                "data-jtml-renderer";
            const std::string value = tokens[i++];
            attrs.push_back(dataKey + "=" + (isQuoted(value) ? friendlyExpression(value) : quote(value)));
            continue;
        }
        if (key == "into") {
            if (i >= tokens.size()) {
                throw std::runtime_error("Expected variable after 'into' at line " + std::to_string(lineNumber));
            }
            const std::string variable = tokens[i++];
            const std::string setter = setterNameFor(variable);
            if (isScene3d) {
                attrs.push_back("data-jtml-scene3d-controller=" + quote(variable));
                result.synthesizedSetters.push_back(
                    "define " + variable + " = " + scene3dControllerInitialState() + "\\\\\n");
                continue;
            }
            if (resolvedTag == "video" || resolvedTag == "audio") {
                attrs.push_back("data-jtml-media-controller=" + quote(variable));
                result.synthesizedSetters.push_back(
                    "define " + variable + " = " + mediaControllerInitialState() + "\\\\\n");
                continue;
            }
            if (!isFileLike) {
                attrs.push_back("value=" + variable);
            }
            // checkbox/file inputs use change; text inputs use input.
            const std::string eventAttr =
                (friendlyTag == "checkbox" || isFileLike) ? "onChange" : "onInput";
            attrs.push_back(eventAttr + "=" + setter + "()");
            result.synthesizedSetters.push_back(
                "function " + setter + "(value)\\\\\n"
                "    " + variable + " = value\\\\\n"
                "\\\\");
            continue;
        }
        if (booleanAttributes().count(key)) {
            attrs.push_back(key);
            continue;
        }
        // A quoted literal that has no following attribute value is inline text,
        // not a standalone attribute. Emit it as a show statement.
        if (isQuoted(key) &&
            (i >= tokens.size() || eventNames().count(tokens[i]) || tokens[i] == "into")) {
            if (!isInputLike) {
                result.bodyLines.push_back("show " + friendlyExpression(key) + "\\\\");
            }
            continue;
        }
        if (i >= tokens.size() || eventNames().count(tokens[i]) || tokens[i] == "into") {
            attrs.push_back(key);
            continue;
        }
        attrs.push_back(key + "=" + friendlyExpression(tokens[i++]));
    }

    std::ostringstream open;
    open << "@" << resolvedTag;
    for (const auto& attr : attrs) open << " " << attr;
    open << "\\\\";
    result.openLine = open.str();
    result.isVoid = isVoidTag(resolvedTag);
    result.closesWithHash = !result.isVoid;
    return result;
}

} // namespace

bool isFriendlySyntax(const std::string& source) {
    std::istringstream in(source);
    std::string raw;
    while (std::getline(in, raw)) {
        std::string text = trim(raw);
        if (text.empty() || startsWith(text, "//") || startsWith(text, "#")) continue;
        return text == "jtml 2";
    }
    return false;
}

bool looksLikeFriendlySyntax(const std::string& source) {
    std::istringstream in(source);
    std::string raw;
    while (std::getline(in, raw)) {
        std::string text = trim(stripFriendlyLineComment(raw));
        if (text.empty() || startsWith(text, "//") || startsWith(text, "#")) continue;
        if (text == "jtml 2") return true;
        auto tokens = splitTokens(text);
        if (tokens.empty()) continue;
        const std::string& head = tokens[0];
        if (head == "let" || head == "get" || head == "const" || head == "when" || head == "page" ||
            head == "make" || head == "use" || head == "show" ||
            head == "for" || head == "if" || head == "while" || head == "style" ||
            head == "route" || head == "effect" || head == "store" || head == "extern" ||
            head == "export") {
            return true;
        }
        return false;
    }
    return false;
}

std::string formatFriendlySource(const std::string& source) {
    std::vector<std::pair<int, std::string>> rows;
    std::istringstream in(source);
    std::string raw;
    bool sawHeader = false;
    int indentUnit = 0;

    while (std::getline(in, raw)) {
        if (!raw.empty() && raw.back() == '\r') raw.pop_back();
        std::string withoutRight = rtrim(raw);
        std::string text = ltrim(withoutRight);
        if (text.empty()) {
            rows.push_back({-1, ""});
            continue;
        }
        int indent = 0;
        for (char ch : withoutRight) {
            if (ch == ' ') {
                ++indent;
            } else if (ch == '\t') {
                throw std::runtime_error("Tabs are not allowed for friendly JTML indentation");
            } else {
                break;
            }
        }
        if (text == "jtml 2") {
            sawHeader = true;
            rows.push_back({0, text});
            continue;
        }
        if (indent > 0) {
            indentUnit = indentUnit == 0 ? indent : std::gcd(indentUnit, indent);
        }
        rows.push_back({indent, text});
    }

    if (indentUnit <= 0 || indentUnit > 8) indentUnit = 2;

    std::ostringstream out;
    if (!sawHeader) {
        out << "jtml 2\n\n";
    }

    bool previousBlank = !sawHeader;
    bool wroteAny = false;
    for (const auto& [indent, text] : rows) {
        if (text.empty()) {
            if (!previousBlank && wroteAny) {
                out << "\n";
                previousBlank = true;
            }
            continue;
        }
        const int level = std::max(0, indent / indentUnit);
        out << std::string(static_cast<size_t>(level * 2), ' ') << text << "\n";
        previousBlank = false;
        wroteAny = true;
    }

    return out.str();
}

FriendlyClassicResult friendlyToClassicWithSourceMap(const std::string& source) {
    FriendlyClassicResult result;
    result.classicLineToFriendlyLine.push_back(0);
    struct SourceMapGuard {
        std::vector<int>* previousMap;
        int previousLine;
        SourceMapGuard(std::vector<int>* map)
            : previousMap(activeFriendlySourceMap),
              previousLine(activeFriendlySourceLine) {
            activeFriendlySourceMap = map;
            activeFriendlySourceLine = 0;
        }
        ~SourceMapGuard() {
            activeFriendlySourceMap = previousMap;
            activeFriendlySourceLine = previousLine;
        }
    } guard(&result.classicLineToFriendlyLine);

    auto lines = stripExportModifiers(collectLines(source));
    std::map<std::string, std::string> storeActions;
    lines = expandStoreLines(lines, storeActions);
    lines = rewriteStoreActionRefs(lines, storeActions);
    const auto componentDefs = collectComponentDefs(lines);
    lines = expandComponentLines(lines, componentDefs);
    std::ostringstream out;
    std::vector<OpenBlock> stack;
    std::set<std::string> synthesizedSetters;
    // Maps action name → redirect path for client-side wiring.
    std::map<std::string, std::string> redirectActions;
    // Maps action name → fetch names refreshed after the action runs.
    std::map<std::string, std::vector<std::string>> invalidateActions;
    // Guard declarations: path → {guardVar, redirectPath}
    struct GuardDecl { std::string var; std::string redirect; };
    std::vector<std::pair<std::string, GuardDecl>> guardDecls;

    auto closeToIndent = [&](int indent) {
        while (!stack.empty() && indent <= stack.back().indent) {
            const int level = static_cast<int>(stack.size()) - 1;
            const auto close = stack.back().close;
            const auto afterClose = stack.back().afterClose;
            emitLine(out, level, close);
            stack.pop_back();
            for (const auto& line : afterClose) {
                emitLine(out, level, line);
            }
        }
    };

    auto activeActionName = [&]() -> std::string {
        for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
            if (!it->actionName.empty()) return it->actionName;
        }
        return "";
    };

    for (const auto& [_, def] : componentDefs) {
        activeFriendlySourceLine = def.number;
        emitLine(out, 0, componentDefinitionLine(def));
        emitLine(out, 0, "#");
    }
    if (!componentDefs.empty()) {
        out << "\n";
        if (activeFriendlySourceMap) activeFriendlySourceMap->push_back(0);
    }

    for (size_t index = 0; index < lines.size(); ++index) {
        const Line& line = lines[index];
        activeFriendlySourceLine = line.number;
        const int nextIndent = (index + 1 < lines.size()) ? lines[index + 1].indent : -1;
        const bool hasChildren = nextIndent > line.indent;
        closeToIndent(line.indent);

        auto tokens = splitTokens(line.text);
        if (tokens.empty()) continue;
        const int level = static_cast<int>(stack.size());
        std::string lineText = line.text;
        if (tokens[0] == "export") {
            if (tokens.size() == 1) {
                throw std::runtime_error("Expected declaration after 'export' at line " +
                                         std::to_string(line.number));
            }
            const auto pos = lineText.find("export");
            lineText = trim(lineText.substr(pos + std::string("export").size()));
            tokens = splitTokens(lineText);
        }
        const std::string head = tokens[0];

        if (head == "use") {
            emitLine(out, level, translateUse(tokens, line.number));
        } else if (head == "extern") {
            if (!(tokens.size() == 2 || (tokens.size() == 4 && tokens[2] == "from"))) {
                throw std::runtime_error("Expected 'extern actionName [from \"window.path\"]' at line " + std::to_string(line.number));
            }
            const std::string action = tokens[1];
            const std::string target = tokens.size() == 4
                ? (isQuoted(tokens[3]) ? tokens[3] : quote(tokens[3]))
                : quote(action);
            emitLine(out, level, "@meta data-jtml-extern-action=" + quote(action) +
                     " data-window=" + target + "\\\\");
        } else if (head == "style") {
            if (!hasChildren) {
                throw std::runtime_error("Expected indented CSS block after 'style' at line " + std::to_string(line.number));
            }
            const size_t end = findBlockEnd(lines, index);
            const std::string css = friendlyStyleBlockToCss(lines, index + 1, end);
            emitLine(out, level, "@style\\\\");
            emitLine(out, level + 1, "show " + cssStringLiteral(css) + "\\\\");
            emitLine(out, level, "#");
            index = end - 1;
        } else if (head == "let") {
            if (tokens.size() < 4) {
                throw std::runtime_error("Expected 'let name = expression' at line " + std::to_string(line.number));
            }
            size_t nameIndex = 1;
            const auto typedName = parseTypedIdentifier(tokens, nameIndex, line.number);
            if (nameIndex >= tokens.size() || tokens[nameIndex] != "=") {
                throw std::runtime_error("Expected 'let name = expression' at line " + std::to_string(line.number));
            }
            const bool insideBlock = !stack.empty();
            const FetchSpec fetch = parseFetchSpec(tokens, nameIndex + 1, line.number);
            if (fetch.valid) {
                if (insideBlock) {
                    throw std::runtime_error("Friendly fetch declarations are currently supported at module/page scope only at line " +
                                             std::to_string(line.number));
                }
                emitLine(out, level, "define " + typedName.name + " = {loading: true, data: [], error: \"\"}\\\\");
                std::string marker = "@template data-jtml-fetch=" + quote(typedName.name) +
                                     " data-url=" + fetch.url +
                                     " data-method=" + fetch.method;
                if (!fetch.bodyExpr.empty()) {
                    marker += " data-body-expr=" + quote(fetch.bodyExpr);
                }
                if (!fetch.refreshAction.empty()) {
                    marker += " data-refresh-action=" + quote(fetch.refreshAction);
                }
                if (!fetch.cache.empty()) {
                    marker += " data-cache=" + fetch.cache;
                }
                if (!fetch.credentials.empty()) {
                    marker += " data-credentials=" + fetch.credentials;
                }
                if (!fetch.timeoutMs.empty()) {
                    marker += " data-timeout-ms=" + quote(fetch.timeoutMs);
                }
                if (!fetch.retryCount.empty()) {
                    marker += " data-retry=" + quote(fetch.retryCount);
                }
                if (!fetch.stalePolicy.empty()) {
                    marker += " data-stale=" + fetch.stalePolicy;
                }
                if (fetch.lazy) {
                    marker += " data-lazy=\"true\"";
                }
                emitLine(out, level, marker + "\\\\");
                emitLine(out, level, "#");
                if (!fetch.refreshAction.empty()) {
                    emitLine(out, level, "function " + fetch.refreshAction + "()\\\\");
                    emitLine(out, level, "\\\\");
                }
                continue;
            }
            // Detect `let name = image src resize W H [fit mode]`
            const size_t exprStart = nameIndex + 1;
            if (!insideBlock && exprStart + 2 < tokens.size() &&
                tokens[exprStart] == "image") {
                const std::string imgSrc = tokens[exprStart + 1];
                const std::string imgOp  = exprStart + 2 < tokens.size() ? tokens[exprStart + 2] : "";
                if (imgOp == "resize" || imgOp == "crop" || imgOp == "filter") {
                    // Parse remaining image-proc args
                    std::string procW, procH, procFit, procX, procY, procFilter, procAmount;
                    size_t pi = exprStart + 3;
                    if (imgOp == "resize") {
                        if (pi < tokens.size()) procW = tokens[pi++];
                        if (pi < tokens.size()) procH = tokens[pi++];
                        if (pi < tokens.size() && tokens[pi] == "fit" && pi + 1 < tokens.size()) {
                            ++pi; procFit = tokens[pi++];
                        }
                    } else if (imgOp == "crop") {
                        if (pi < tokens.size()) procX = tokens[pi++];
                        if (pi < tokens.size()) procY = tokens[pi++];
                        if (pi < tokens.size()) procW = tokens[pi++];
                        if (pi < tokens.size()) procH = tokens[pi++];
                    } else if (imgOp == "filter") {
                        if (pi < tokens.size()) procFilter = tokens[pi++];
                        if (pi < tokens.size() && tokens[pi] == "amount" && pi + 1 < tokens.size()) {
                            ++pi; procAmount = tokens[pi++];
                        }
                    }
                    emitLine(out, level, "define " + typedName.name +
                        " = {preview: \"\", loading: false, error: \"\", width: 0, height: 0}\\\\");
                    std::string imgMarker = "@template data-jtml-image-proc=" + quote(imgOp) +
                        " data-jtml-image-src=" + quote(imgSrc) +
                        " data-jtml-image-into=" + quote(typedName.name);
                    if (!procW.empty())      imgMarker += " data-jtml-image-w=" + quote(procW);
                    if (!procH.empty())      imgMarker += " data-jtml-image-h=" + quote(procH);
                    if (!procFit.empty())    imgMarker += " data-jtml-image-fit=" + quote(procFit);
                    if (!procX.empty())      imgMarker += " data-jtml-image-x=" + quote(procX);
                    if (!procY.empty())      imgMarker += " data-jtml-image-y=" + quote(procY);
                    if (!procFilter.empty()) imgMarker += " data-jtml-image-filter=" + quote(procFilter);
                    if (!procAmount.empty()) imgMarker += " data-jtml-image-amount=" + quote(procAmount);
                    imgMarker += "\\\\";
                    emitLine(out, level, imgMarker);
                    continue;
                }
            }
            const std::string expr = friendlyExpression(joinTokens(tokens, nameIndex + 1));
            const std::string typeSuffix = (!insideBlock && !typedName.type.empty()) ? ": " + typedName.type : "";
            emitLine(out, level, (insideBlock ? "" : "define ") + typedName.name + typeSuffix + " = " + expr + "\\\\");
        } else if (head == "__define" || head == "__const" || head == "__derive") {
            if (tokens.size() < 4) {
                throw std::runtime_error("Expected internal component state declaration at line " + std::to_string(line.number));
            }
            size_t nameIndex = 1;
            const auto typedName = parseTypedIdentifier(tokens, nameIndex, line.number);
            if (nameIndex >= tokens.size() || tokens[nameIndex] != "=") {
                throw std::runtime_error("Expected '=' in internal component state declaration at line " + std::to_string(line.number));
            }
            const std::string expr = friendlyExpression(joinTokens(tokens, nameIndex + 1));
            const std::string classicHead =
                head == "__const" ? "const " : (head == "__derive" ? "derive " : "define ");
            const std::string typeSuffix = typedName.type.empty() ? "" : ": " + typedName.type;
            emitLine(out, level, classicHead + typedName.name + typeSuffix + " = " + expr + "\\\\");
        } else if (tokens.size() >= 3 && isFriendlyAssignmentOperator(tokens[1])) {
            const std::string expr = friendlyExpression(joinTokens(tokens, 2));
            emitLine(out, level, tokens[0] + " " + tokens[1] + " " + expr + "\\\\");
        } else if (head == "get") {
            if (tokens.size() < 4) {
                throw std::runtime_error("Expected 'get name = expression' at line " + std::to_string(line.number));
            }
            size_t nameIndex = 1;
            const auto typedName = parseTypedIdentifier(tokens, nameIndex, line.number);
            if (nameIndex >= tokens.size() || tokens[nameIndex] != "=") {
                throw std::runtime_error("Expected 'get name = expression' at line " + std::to_string(line.number));
            }
            const std::string expr = friendlyExpression(joinTokens(tokens, nameIndex + 1));
            const std::string typeSuffix = typedName.type.empty() ? "" : ": " + typedName.type;
            emitLine(out, level, "derive " + typedName.name + typeSuffix + " = " + expr + "\\\\");
        } else if (head == "const") {
            if (tokens.size() < 4) {
                throw std::runtime_error("Expected 'const name = expression' at line " + std::to_string(line.number));
            }
            size_t nameIndex = 1;
            const auto typedName = parseTypedIdentifier(tokens, nameIndex, line.number);
            if (nameIndex >= tokens.size() || tokens[nameIndex] != "=") {
                throw std::runtime_error("Expected 'const name = expression' at line " + std::to_string(line.number));
            }
            const std::string expr = friendlyExpression(joinTokens(tokens, nameIndex + 1));
            const std::string typeSuffix = typedName.type.empty() ? "" : ": " + typedName.type;
            emitLine(out, level, "const " + typedName.name + typeSuffix + " = " + expr + "\\\\");
        } else if (head == "when") {
            if (tokens.size() < 2) {
                throw std::runtime_error("Expected action name after 'when' at line " + std::to_string(line.number));
            }
            std::ostringstream params;
            const auto parsedParams = parseTypedIdentifiers(tokens, 2, line.number);
            for (size_t i = 0; i < parsedParams.size(); ++i) {
                if (i > 0) params << ", ";
                params << parsedParams[i].name;
            }
            emitLine(out, level, "function " + tokens[1] + "(" + params.str() + ")\\\\");
            stack.push_back({line.indent, "\\\\", {}, line.number, tokens[1]});
        } else if (head == "effect") {
            if (tokens.size() != 2) {
                throw std::runtime_error("Expected 'effect variable' at line " + std::to_string(line.number));
            }
            if (!hasChildren) {
                throw std::runtime_error("Expected indented body after 'effect' at line " + std::to_string(line.number));
            }
            std::string fnName = "__effect_" + tokens[1] + "_" + std::to_string(line.number);
            for (char& ch : fnName) {
                if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') ch = '_';
            }
            emitLine(out, level, "function " + fnName + "()\\\\");
            stack.push_back({line.indent, "\\\\", {"subscribe " + fnName + " to " + tokens[1] + "\\\\"}});
        } else if (head == "page") {
            std::vector<std::string> elementTokens = tokens;
            elementTokens[0] = "main";
            auto element = translateElement(elementTokens, line.number);
            emitLine(out, level, element.openLine);
            for (const auto& body : element.bodyLines) emitLine(out, level + 1, body);
            stack.push_back({line.indent, "#"});
        } else if (head == "show") {
            emitLine(out, level, "show " + friendlyExpression(joinTokens(tokens, 1)) + "\\\\");
        } else if (head == "if") {
            emitLine(out, level, "if (" + joinTokens(tokens, 1) + ")\\\\");
            stack.push_back({line.indent, "\\\\"});
        } else if (head == "else") {
            emitLine(out, level, "else\\\\");
            stack.push_back({line.indent, "\\\\"});
        } else if (head == "for") {
            if (tokens.size() < 4 || tokens[2] != "in") {
                throw std::runtime_error("Expected 'for name in expression' at line " + std::to_string(line.number));
            }
            emitLine(out, level, "for (" + tokens[1] + " in " + joinTokens(tokens, 3) + ")\\\\");
            stack.push_back({line.indent, "\\\\"});
        } else if (head == "while") {
            if (tokens.size() < 2) {
                throw std::runtime_error("Expected 'while expression' at line " + std::to_string(line.number));
            }
            emitLine(out, level, "while (" + joinTokens(tokens, 1) + ")\\\\");
            stack.push_back({line.indent, "\\\\"});
        } else if (head == "try") {
            if (tokens.size() != 1) {
                throw std::runtime_error("Expected bare 'try' at line " + std::to_string(line.number));
            }
            emitLine(out, level, "try\\\\");
            stack.push_back({line.indent, "\\\\"});
        } else if (head == "catch") {
            if (tokens.size() != 2) {
                throw std::runtime_error("Expected 'catch name' at line " + std::to_string(line.number));
            }
            emitLine(out, level, "except(" + tokens[1] + ")\\\\");
            stack.push_back({line.indent, "\\\\"});
        } else if (head == "finally") {
            if (tokens.size() != 1) {
                throw std::runtime_error("Expected bare 'finally' at line " + std::to_string(line.number));
            }
            emitLine(out, level, "then\\\\");
            stack.push_back({line.indent, "\\\\"});
        } else if (head == "return") {
            std::string expr = joinTokens(tokens, 1);
            emitLine(out, level, expr.empty() ? "return\\\\" : "return " + expr + "\\\\");
        } else if (head == "throw") {
            std::string expr = joinTokens(tokens, 1);
            emitLine(out, level, expr.empty() ? "throw\\\\" : "throw " + expr + "\\\\");
        } else if (head == "redirect") {
            if (tokens.size() != 2 || !isQuoted(tokens[1])) {
                throw std::runtime_error("Expected 'redirect \"path\"' at line " + std::to_string(line.number));
            }
            // Record this action → path mapping. The browser runtime reads
            // @meta data-jtml-redirect-action elements on load and intercepts
            // clicks before they reach the server.
            const std::string actionName = activeActionName();
            if (actionName.empty()) {
                throw std::runtime_error("'redirect' can only be used inside a 'when' action at line " + std::to_string(line.number));
            }
            redirectActions[actionName] = tokens[1];
        } else if (head == "invalidate") {
            if (tokens.size() < 2) {
                throw std::runtime_error("Expected 'invalidate fetchName [fetchName...]' at line " + std::to_string(line.number));
            }
            const std::string actionName = activeActionName();
            if (actionName.empty()) {
                throw std::runtime_error("'invalidate' can only be used inside a 'when' action at line " + std::to_string(line.number));
            }
            auto& names = invalidateActions[actionName];
            for (size_t i = 1; i < tokens.size(); ++i) {
                if (tokens[i] == ",") continue;
                std::string name = tokens[i];
                if (!name.empty() && name.back() == ',') name.pop_back();
                if (!name.empty()) names.push_back(name);
            }
        } else if (head == "timeline") {
            // timeline name [duration N] [easing E] [autoplay] [repeat]
            //   animate var from A to B
            if (tokens.size() < 2) {
                throw std::runtime_error("Expected 'timeline name [duration N] [easing E]' at line " + std::to_string(line.number));
            }
            const std::string tlName = tokens[1];
            std::string tlDuration = "400";
            std::string tlEasing = "linear";
            bool tlAutoplay = false;
            bool tlRepeat = false;
            for (size_t ti = 2; ti < tokens.size(); ++ti) {
                if (tokens[ti] == "duration" && ti + 1 < tokens.size()) {
                    tlDuration = tokens[++ti];
                } else if (tokens[ti] == "easing" && ti + 1 < tokens.size()) {
                    tlEasing = tokens[++ti];
                } else if (tokens[ti] == "autoplay") {
                    tlAutoplay = true;
                } else if (tokens[ti] == "repeat") {
                    tlRepeat = true;
                }
            }
            std::string tlAnimates;
            if (hasChildren) {
                const size_t end = findBlockEnd(lines, index);
                for (size_t ci = index + 1; ci < end; ++ci) {
                    auto ctoks = splitTokens(lines[ci].text);
                    if (ctoks.size() >= 6 && ctoks[0] == "animate" &&
                        ctoks[2] == "from" && ctoks[4] == "to") {
                        if (!tlAnimates.empty()) tlAnimates += ";";
                        tlAnimates += ctoks[1] + ":" + ctoks[3] + ":" + ctoks[5];
                    }
                }
                index = end - 1;
            }
            emitLine(out, level, "define " + tlName + " = {playing: false, paused: false, progress: 0.0, elapsed: 0}\\\\");
            std::string tlMarker = "@template data-jtml-timeline=" + quote(tlName) +
                " data-jtml-timeline-duration=" + quote(tlDuration) +
                " data-jtml-timeline-easing=" + quote(tlEasing);
            if (!tlAnimates.empty()) tlMarker += " data-jtml-timeline-animates=" + quote(tlAnimates);
            if (tlAutoplay)          tlMarker += " data-jtml-timeline-autoplay=\"true\"";
            if (tlRepeat)            tlMarker += " data-jtml-timeline-repeat=\"true\"";
            tlMarker += "\\\\";
            emitLine(out, level, tlMarker);
        } else if (head == "guard") {
            // guard "/path" require varName [else "/fallback"]
            // Emits a meta element for browser-side route guard checking.
            if (tokens.size() < 4 || !isQuoted(tokens[1]) || tokens[2] != "require") {
                throw std::runtime_error("Expected 'guard \"/path\" require varName [else \"/fallback\"]' at line " + std::to_string(line.number));
            }
            GuardDecl decl;
            decl.var = tokens[3];
            if (tokens.size() >= 6 && tokens[4] == "else" && isQuoted(tokens[5])) {
                decl.redirect = tokens[5];
            }
            guardDecls.push_back({tokens[1], decl});
        } else if (head == "break" || head == "continue") {
            emitLine(out, level, head + "\\\\");
        } else if (tokens.size() == 1 &&
                   (head.find('(') != std::string::npos || head.find('.') != std::string::npos)) {
            emitLine(out, level, head + "\\\\");
        } else if (head == "slot") {
            throw std::runtime_error("'slot' can only be used inside a Friendly JTML component.");
        } else if (head == "make") {
            throw std::runtime_error("'make' can only be handled by the Friendly JTML component expander.");
        } else if (isKnownLineKeyword(head)) {
            throw std::runtime_error("Unsupported Friendly JTML statement '" + head + "' at line " + std::to_string(line.number));
        } else {
            auto element = translateElement(tokens, line.number);
            emitLine(out, level, element.openLine);
            for (const auto& body : element.bodyLines) emitLine(out, level + 1, body);
            for (const auto& setter : element.synthesizedSetters) {
                synthesizedSetters.insert(setter);
            }
            if (hasChildren && element.closesWithHash) {
                stack.push_back({line.indent, "#"});
            } else if (element.closesWithHash) {
                emitLine(out, level, "#");
            } else if (element.isVoid && !stack.empty() && nextIndent <= line.indent) {
                emitLine(out, level, "@span hidden\\\\");
                emitLine(out, level, "#");
            }
        }
    }

    closeToIndent(-1);

    for (const auto& setter : synthesizedSetters) {
        out << "\n" << setter << "\n";
    }

    // Emit meta elements for redirect actions so the browser runtime can
    // intercept clicks before they go to the server.
    for (const auto& [actionName, path] : redirectActions) {
        out << "@meta data-jtml-redirect-action=" << quote(actionName)
            << " data-jtml-redirect-to=" << path << "\\\\\n";
    }

    // Emit meta elements for action-driven fetch invalidation. Unlike
    // `refresh action`, these run after the action has dispatched so mutation
    // handlers can update state/server data before the fetch is retried.
    for (const auto& [actionName, fetchNames] : invalidateActions) {
        if (fetchNames.empty()) continue;
        std::ostringstream joined;
        for (size_t i = 0; i < fetchNames.size(); ++i) {
            if (i > 0) joined << ",";
            joined << fetchNames[i];
        }
        out << "@meta data-jtml-invalidate-action=" << quote(actionName)
            << " data-jtml-invalidate-fetches=" << quote(joined.str()) << "\\\\\n";
    }

    // Emit meta elements for route guards so the browser runtime can
    // block or redirect navigation when a guard condition fails.
    for (const auto& [routePath, decl] : guardDecls) {
        out << "@meta data-jtml-route-guard=" << routePath
            << " data-jtml-guard-var=" << quote(decl.var);
        if (!decl.redirect.empty()) {
            out << " data-jtml-guard-redirect=" << decl.redirect;
        }
        out << "\\\\\n";
    }

    result.classicSource = out.str();
    return result;
}

std::string friendlyToClassic(const std::string& source) {
    return friendlyToClassicWithSourceMap(source).classicSource;
}

std::string normalizeSourceSyntax(const std::string& source) {
    return normalizeSourceSyntax(source, SyntaxMode::Auto);
}

std::string normalizeSourceSyntax(const std::string& source, SyntaxMode mode) {
    if (mode == SyntaxMode::Classic) return source;
    if (mode == SyntaxMode::Friendly) return friendlyToClassic(source);
    if (!isFriendlySyntax(source)) return source;
    return friendlyToClassic(source);
}

} // namespace jtml
