#include "jtml/diagnostic.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

namespace jtml {

namespace {

std::string trimCopy(std::string value) {
    auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string lowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

void extractLocation(const std::string& message, Diagnostic& diagnostic) {
    std::smatch match;
    static const std::regex lineColumn(
        R"((?:line|Line)\s+([0-9]+)(?:\s*,?\s*(?:column|col|Column)\s+([0-9]+))?)");
    if (std::regex_search(message, match, lineColumn)) {
        diagnostic.line = std::stoi(match[1].str());
        if (match.size() > 2 && match[2].matched) {
            diagnostic.column = std::stoi(match[2].str());
        }
    }
}

void classify(Diagnostic& diagnostic) {
    const std::string text = lowerCopy(diagnostic.message);

    if (text.find("lexer error") != std::string::npos ||
        text.find("unterminated string") != std::string::npos) {
        diagnostic.code = "JTML_LEXER";
        diagnostic.hint = "Check quotes, escapes, and line endings before this location.";
        diagnostic.example = "text \"Hello\"";
        return;
    }
    if (text.find("expected action after event") != std::string::npos ||
        text.find("event handler") != std::string::npos ||
        text.find(" click") != std::string::npos) {
        diagnostic.code = "JTML_EVENT_ACTION";
        diagnostic.hint = "Add a named action after the event, then define it with `when`.";
        diagnostic.example = "button \"Save\" click save\n\nwhen save\n  show \"Saved\"";
        return;
    }
    if (text.find("unsupported fetch option") != std::string::npos ||
        text.find("fetch") != std::string::npos) {
        diagnostic.code = "JTML_FETCH";
        diagnostic.hint = "Use supported fetch options such as `method`, `body`, `cache`, `credentials`, and `refresh`.";
        diagnostic.example = "let users = fetch \"/api/users\" method \"GET\"";
        return;
    }
    if (text.find("type mismatch") != std::string::npos ||
        text.find("declared ") != std::string::npos) {
        diagnostic.code = "JTML_TYPE_MISMATCH";
        diagnostic.hint = "Make the annotation match the expression, or change the expression to produce the declared type.";
        diagnostic.example = "let count: number = 0\nlet name: string = \"JTML\"";
        return;
    }
    if (text.find("argument(s)") != std::string::npos ||
        (text.find("function '") != std::string::npos &&
         text.find("expects") != std::string::npos &&
         text.find("got") != std::string::npos)) {
        diagnostic.code = "JTML_ARITY";
        diagnostic.hint = "Pass the number of values declared by the action/function parameters, or update the declaration.";
        diagnostic.example = "when save id\n  show id\n\nbutton \"Save\" click save(42)";
        return;
    }
    if (text.find("media accessibility") != std::string::npos ||
        text.find("media 3d") != std::string::npos ||
        text.find("media input") != std::string::npos ||
        text.find("alt text") != std::string::npos ||
        text.find("dropzone") != std::string::npos) {
        diagnostic.code = "JTML_MEDIA_ACCESSIBILITY";
        diagnostic.hint = "Add accessible names and bounded media attributes so generated interfaces are usable and production-safe.";
        diagnostic.example = "image src \"/photo.jpg\" alt \"Team photo\"\nvideo src \"/demo.mp4\" controls\nfile \"Choose image\" accept \"image/*\" into selected";
        return;
    }
    if (text.find("route") != std::string::npos) {
        diagnostic.code = "JTML_ROUTE";
        diagnostic.hint = "Routes should map a path to a named component, and route params become component arguments.";
        diagnostic.example = "route \"/user/:id\" as UserProfile\n\nmake UserProfile id\n  page\n    h1 id";
        return;
    }
    if (text.find("undefined friendly jtml component") != std::string::npos ||
        text.find("unknown friendly jtml component") != std::string::npos ||
        text.find("component") != std::string::npos) {
        diagnostic.code = "JTML_COMPONENT";
        diagnostic.hint = "Define uppercase components with `make`, then call them by name.";
        diagnostic.example = "make Card title\n  box\n    h2 title\n\npage\n  Card \"Welcome\"";
        return;
    }
    if (text.find("tab") != std::string::npos ||
        text.find("indent") != std::string::npos) {
        diagnostic.code = "JTML_INDENTATION";
        diagnostic.hint = "Use spaces for Friendly JTML indentation and keep child lines deeper than their parent.";
        diagnostic.example = "page\n  h1 \"Title\"\n  text \"Body\"";
        return;
    }
    if (text.find("undefined variable") != std::string::npos ||
        text.find("undefined reference") != std::string::npos) {
        diagnostic.code = "JTML_UNDEFINED_NAME";
        diagnostic.hint = "Declare the value with `let` before using it, import it, or pass it as a component parameter.";
        diagnostic.example = "let name = \"JTML\"\n\npage\n  h1 name";
        return;
    }
    if (text.find("unreachable") != std::string::npos) {
        diagnostic.code = "JTML_UNREACHABLE_CODE";
        diagnostic.hint = "Move this statement before the flow-ending statement or remove it.";
        diagnostic.example = "when submit\n  show \"done\"\n  return true";
        return;
    }
    if (text.find("parser error") != std::string::npos ||
        text.find("expected") != std::string::npos ||
        text.find("unexpected") != std::string::npos) {
        diagnostic.code = "JTML_PARSE";
        diagnostic.hint = "Check the surrounding JTML syntax and indentation.";
        diagnostic.example = "page\n  h1 \"Hello\"\n  button \"Save\" click save";
        return;
    }

    diagnostic.code = diagnostic.severity == DiagnosticSeverity::Warning
        ? "JTML_WARNING"
        : "JTML_ERROR";
}

} // namespace

const char* diagnosticSeverityName(DiagnosticSeverity severity) {
    return severity == DiagnosticSeverity::Error ? "error" : "warning";
}

Diagnostic diagnosticFromMessage(const std::string& message, DiagnosticSeverity severity) {
    Diagnostic diagnostic;
    diagnostic.severity = severity;
    diagnostic.message = trimCopy(message);
    extractLocation(diagnostic.message, diagnostic);
    classify(diagnostic);
    return diagnostic;
}

std::vector<Diagnostic> diagnosticsFromMessageBlock(
    const std::string& messages,
    DiagnosticSeverity severity) {
    std::vector<Diagnostic> diagnostics;
    std::istringstream input(messages);
    std::string line;
    while (std::getline(input, line)) {
        line = trimCopy(line);
        if (line.empty()) continue;
        diagnostics.push_back(diagnosticFromMessage(line, severity));
    }
    if (diagnostics.empty() && !trimCopy(messages).empty()) {
        diagnostics.push_back(diagnosticFromMessage(messages, severity));
    }
    return diagnostics;
}

void remapDiagnosticLines(std::vector<Diagnostic>& diagnostics,
                          const std::vector<int>& oneBasedLineMap) {
    for (auto& diagnostic : diagnostics) {
        if (diagnostic.line <= 0 ||
            diagnostic.line >= static_cast<int>(oneBasedLineMap.size()) ||
            oneBasedLineMap[static_cast<size_t>(diagnostic.line)] <= 0) {
            continue;
        }
        const int oldLine = diagnostic.line;
        const int newLine = oneBasedLineMap[static_cast<size_t>(diagnostic.line)];
        diagnostic.line = newLine;

        const std::regex pattern(
            "((?:line|Line)\\s+)(" + std::to_string(oldLine) + ")\\b");
        std::smatch match;
        if (std::regex_search(diagnostic.message, match, pattern)) {
            diagnostic.message.replace(
                static_cast<size_t>(match.position(2)),
                static_cast<size_t>(match.length(2)),
                std::to_string(newLine));
        }
    }
}

} // namespace jtml
