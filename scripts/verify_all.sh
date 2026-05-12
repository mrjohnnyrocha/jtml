#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${BUILD_DIR:-"$repo_root/build"}"

cmake -S "$repo_root" -B "$build_dir" \
  -DJTML_BUILD_TESTS=ON \
  -DJTML_BUILD_PYTHON=OFF
cmake --build "$build_dir" --parallel

mkdir -p "$repo_root/dist"
ctest --test-dir "$build_dir" --output-on-failure
"$build_dir/jtml" doctor --json > "$repo_root/dist/doctor.json"
"$build_dir/jtml" test
"$build_dir/jtml" check "$repo_root/examples/friendly_components.jtml"
"$build_dir/jtml" check "$repo_root/examples/friendly_import_page.jtml"
"$build_dir/jtml" build "$repo_root/examples/friendly_import_page.jtml" --out "$repo_root/dist/verify-app"
"$build_dir/jtml" check "$repo_root/demos/index.jtml"
"$build_dir/jtml" build "$repo_root/demos" --out "$repo_root/dist/demos"

tmp_pkg_root="$(mktemp -d "${TMPDIR:-/tmp}/jtml-package.XXXXXX")"
mkdir -p "$tmp_pkg_root/shared" "$tmp_pkg_root/app"
printf 'jtml 2\n\nmake Card title\n  box\n    h2 title\n    slot\n' > "$tmp_pkg_root/shared/card.jtml"
(
  cd "$tmp_pkg_root/app"
  "$build_dir/jtml" add ../shared/card.jtml --json > "$repo_root/dist/package-add.json"
  printf 'jtml 2\n\nuse Card from "card"\n\npage\n  Card "Package card"\n    text "Works"\n' > index.jtml
  "$build_dir/jtml" check index.jtml --json > "$repo_root/dist/package-check.json"
  "$build_dir/jtml" build index.jtml --out dist > "$repo_root/dist/package-build.txt"
  rm -rf jtml_modules
  "$build_dir/jtml" install --json > "$repo_root/dist/package-install.json"
  "$build_dir/jtml" check index.jtml --json > "$repo_root/dist/package-check-after-install.json"
)
if command -v python3 >/dev/null 2>&1; then
  python3 - "$repo_root/dist/package-add.json" "$repo_root/dist/package-check.json" "$repo_root/dist/package-install.json" "$repo_root/dist/package-check-after-install.json" "$tmp_pkg_root/app" <<'PY'
import json
import pathlib
import sys

add = json.loads(pathlib.Path(sys.argv[1]).read_text())
check = json.loads(pathlib.Path(sys.argv[2]).read_text())
install = json.loads(pathlib.Path(sys.argv[3]).read_text())
check_after_install = json.loads(pathlib.Path(sys.argv[4]).read_text())
app = pathlib.Path(sys.argv[5])
assert add["ok"] is True, add
assert add["package"] == "card", add
assert check["ok"] is True, check
assert install["ok"] is True, install
assert install["restored"] == ["card"], install
assert check_after_install["ok"] is True, check_after_install
assert (app / "jtml_modules/card/index.jtml").exists()
assert (app / "jtml.packages.json").exists()
assert (app / "jtml.lock.json").exists()
assert (app / "dist/index.html").exists()
PY
fi
rm -rf "$tmp_pkg_root"

tmp_bad_jtml="$(mktemp "${TMPDIR:-/tmp}/jtml-source-map.XXXXXX.jtml")"
printf 'jtml 2\n\n// source map check\n\npage\n  show @\n' > "$tmp_bad_jtml"
if "$build_dir/jtml" check "$tmp_bad_jtml" --json > "$repo_root/dist/source-map-check.json"; then
  echo "Expected source-map check fixture to fail" >&2
  exit 1
fi
if command -v python3 >/dev/null 2>&1; then
  python3 - "$repo_root/dist/source-map-check.json" <<'PY'
import json
import pathlib
import sys

payload = json.loads(pathlib.Path(sys.argv[1]).read_text())
first = payload["diagnostics"][0]
assert first["line"] == 6
assert "line 6" in first["message"]
assert "line 6" in payload["error"]
PY
fi
rm -f "$tmp_bad_jtml"

# ---------------------------------------------------------------------------
# `jtml refactor rename`: round-trip a Friendly file through the new CLI and
# confirm string-/comment-aware behaviour matches the LSP rename path.
# ---------------------------------------------------------------------------
tmp_refactor="$(mktemp "${TMPDIR:-/tmp}/jtml-refactor.XXXXXX.jtml")"
printf 'jtml 2\nlet count = 0\n// count comment\nshow "count string"\nshow count\n' > "$tmp_refactor"
"$build_dir/jtml" refactor rename "$tmp_refactor" --from count --to total --json \
  > "$repo_root/dist/refactor-rename.json"
if command -v python3 >/dev/null 2>&1; then
  python3 - "$repo_root/dist/refactor-rename.json" "$tmp_refactor" <<'PY'
import json
import pathlib
import sys

payload = json.loads(pathlib.Path(sys.argv[1]).read_text())
src = pathlib.Path(sys.argv[2]).read_text()
assert payload["ok"] is True, payload
assert payload["refactor"] == "rename"
assert payload["from"] == "count" and payload["to"] == "total"
assert payload["changed"] is True, payload
# Two occurrences only — the comment word and the string-literal occurrences
# must be skipped, leaving `let count` and the trailing `show count`.
assert len(payload["edits"]) == 2, payload["edits"]
assert payload["written"] is False  # --json without -w must not mutate the file
# File on disk is unchanged.
assert "let count = 0" in src and "show count\n" in src, src
PY
fi
"$build_dir/jtml" refactor rename "$tmp_refactor" --from count --to total -w
if command -v python3 >/dev/null 2>&1; then
  python3 - "$tmp_refactor" <<'PY'
import pathlib
import sys

src = pathlib.Path(sys.argv[1]).read_text()
assert "let total = 0" in src, src
assert "show total\n" in src, src
# Comment and string preserved verbatim.
assert "// count comment" in src, src
assert '"count string"' in src, src
assert "let count" not in src, src
PY
fi
rm -f "$tmp_refactor"

# ---------------------------------------------------------------------------
# `jtml refactor rename <dir>`: workspace-mode rename. Walks every .jtml file
# under the directory, applies the same string-/comment-aware scanner, and
# skips `dist/` (and other build dirs) the same way the LSP workspace scan
# does.
# ---------------------------------------------------------------------------
tmp_ws="$(mktemp -d "${TMPDIR:-/tmp}/jtml-refactor-ws.XXXXXX")"
printf 'jtml 2\nlet count = 0\nshow count\n' > "$tmp_ws/a.jtml"
printf 'jtml 2\n// count comment\nshow "count str"\nshow count\n' > "$tmp_ws/b.jtml"
mkdir -p "$tmp_ws/dist"
printf 'let count = 999\n' > "$tmp_ws/dist/skipme.jtml"

# Preview (no -w): files unchanged on disk, JSON reports edits per file.
"$build_dir/jtml" refactor rename "$tmp_ws" --from count --to total --json \
  > "$repo_root/dist/refactor-rename-ws-preview.json"
if command -v python3 >/dev/null 2>&1; then
  python3 - "$repo_root/dist/refactor-rename-ws-preview.json" "$tmp_ws" <<'PY'
import json
import pathlib
import sys

payload = json.loads(pathlib.Path(sys.argv[1]).read_text())
ws = pathlib.Path(sys.argv[2]).resolve()
assert payload["scope"] == "workspace", payload
assert payload["filesChanged"] == 2, payload
assert payload["totalEdits"] == 3, payload
assert all(not f["written"] for f in payload["files"]), payload
files = sorted(pathlib.Path(f["file"]).resolve() for f in payload["files"])
assert files == [(ws / "a.jtml").resolve(), (ws / "b.jtml").resolve()], files
# dist/ must be skipped — its file must NOT appear in the report.
assert all("dist" not in str(f) for f in files), files
# Files on disk are unchanged in preview mode.
assert "let count = 0" in (ws / "a.jtml").read_text()
assert "let count = 999" in (ws / "dist/skipme.jtml").read_text()
PY
fi

# Apply with -w: files are rewritten on disk, dist/ remains untouched.
"$build_dir/jtml" refactor rename "$tmp_ws" --from count --to total -w \
  > "$repo_root/dist/refactor-rename-ws-apply.txt"
if command -v python3 >/dev/null 2>&1; then
  python3 - "$tmp_ws" <<'PY'
import pathlib
import sys

ws = pathlib.Path(sys.argv[1])
a = (ws / "a.jtml").read_text()
b = (ws / "b.jtml").read_text()
skip = (ws / "dist/skipme.jtml").read_text()
assert "let total = 0" in a and "show total\n" in a, a
assert "let count" not in a, a
assert "show total\n" in b, b
# Comment + string preserved across workspace mode too.
assert "// count comment" in b, b
assert '"count str"' in b, b
# dist/ untouched.
assert skip == "let count = 999\n", skip
PY
fi
rm -rf "$tmp_ws"

if command -v node >/dev/null 2>&1; then
  node --check "$repo_root/editors/vscode/extension.js"
  node -e '
const fs = require("fs");
for (const file of [
  "editors/vscode/package.json",
  "editors/vscode/language-configuration.json",
  "editors/vscode/snippets/jtml.code-snippets",
  "editors/vscode/syntaxes/jtml.tmLanguage.json",
]) {
  JSON.parse(fs.readFileSync(process.argv[1] + "/" + file, "utf8"));
}
' "$repo_root"
elif command -v python3 >/dev/null 2>&1; then
  python3 - "$repo_root" <<'PY'
import json
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
for file in [
    "editors/vscode/package.json",
    "editors/vscode/language-configuration.json",
    "editors/vscode/snippets/jtml.code-snippets",
    "editors/vscode/syntaxes/jtml.tmLanguage.json",
]:
    json.loads((root / file).read_text())
PY
fi

if command -v python3 >/dev/null 2>&1; then
  python3 - "$repo_root" "$build_dir/jtml" <<'PY'
import json
import pathlib
import select
import subprocess
import sys
import tempfile

root = pathlib.Path(sys.argv[1])
jtml = pathlib.Path(sys.argv[2])
source = 'jtml 2\n\npage\n  button "Save" click\n'
doc = pathlib.Path(tempfile.gettempdir()) / "jtml-lsp-verify.jtml"
doc.write_text(source)
uri = "file://" + str(doc)

proc = subprocess.Popen([str(jtml), "lsp"], stdin=subprocess.PIPE, stdout=subprocess.PIPE)

def send(message):
    body = json.dumps(message, separators=(",", ":")).encode()
    proc.stdin.write(b"Content-Length: " + str(len(body)).encode() + b"\r\n\r\n" + body)
    proc.stdin.flush()

def read_one(timeout=5):
    ready, _, _ = select.select([proc.stdout.fileno()], [], [], timeout)
    if not ready:
        raise RuntimeError("Timed out waiting for LSP response")
    headers = b""
    while b"\r\n\r\n" not in headers:
        chunk = proc.stdout.read(1)
        if not chunk:
            raise RuntimeError("LSP server closed stdout")
        headers += chunk
    length = 0
    for line in headers.decode().split("\r\n"):
        if line.lower().startswith("content-length:"):
            length = int(line.split(":", 1)[1].strip())
    return json.loads(proc.stdout.read(length))

send({"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {"capabilities": {}}})
init = read_one()
capabilities = init["result"]["capabilities"]
assert capabilities["documentFormattingProvider"] is True
assert capabilities["hoverProvider"] is True
assert capabilities["documentSymbolProvider"] is True
assert capabilities["definitionProvider"] is True
assert "completionProvider" in capabilities
send({"jsonrpc": "2.0", "method": "initialized", "params": {}})
send({"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {
    "textDocument": {"uri": uri, "languageId": "jtml", "version": 1, "text": source}
}})
published = read_one()
diagnostics = published["params"]["diagnostics"]
assert diagnostics and diagnostics[0]["code"] == "JTML_EVENT_ACTION"
send({"jsonrpc": "2.0", "id": 150, "method": "textDocument/codeAction", "params": {
    "textDocument": {"uri": uri},
    "range": diagnostics[0]["range"],
    "context": {"diagnostics": [diagnostics[0]]},
}})
event_actions = read_one()
event_titles = [a["title"] for a in event_actions["result"]]
assert any("when save" in title for title in event_titles), event_titles
event_fix = next(a for a in event_actions["result"] if "when save" in a["title"])
event_edits = event_fix["edit"]["changes"][uri]
assert any(" save" in edit["newText"] for edit in event_edits), event_edits
assert any("when save" in edit["newText"] for edit in event_edits), event_edits
fixed = 'jtml 2\n\nlet count = 0\n\nwhen add\n  count += 1\n\npage\n    h1 "Hi"\n    button "Add" click add\n'
send({"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
    "textDocument": {"uri": uri, "version": 2},
    "contentChanges": [{"text": fixed}]
}})
published = read_one()
assert published["params"]["diagnostics"] == []
send({"jsonrpc": "2.0", "id": 2, "method": "textDocument/formatting", "params": {
    "textDocument": {"uri": uri}, "options": {"tabSize": 2, "insertSpaces": True}
}})
formatted = read_one()
assert '  h1 "Hi"' in formatted["result"][0]["newText"]
send({"jsonrpc": "2.0", "id": 3, "method": "textDocument/completion", "params": {
    "textDocument": {"uri": uri}, "position": {"line": 3, "character": 4}
}})
completion = read_one()
labels = [item["label"] for item in completion["result"]["items"]]
assert "fetch" in labels and "route layout" in labels and "count" in labels and "add" in labels
send({"jsonrpc": "2.0", "id": 4, "method": "textDocument/hover", "params": {
    "textDocument": {"uri": uri}, "position": {"line": 0, "character": 1}
}})
hover = read_one()
assert "Friendly JTML" in hover["result"]["contents"]["value"]
send({"jsonrpc": "2.0", "id": 5, "method": "textDocument/hover", "params": {
    "textDocument": {"uri": uri}, "position": {"line": 2, "character": 5}
}})
hover = read_one()
assert "JTML let" in hover["result"]["contents"]["value"]
send({"jsonrpc": "2.0", "id": 6, "method": "textDocument/definition", "params": {
    "textDocument": {"uri": uri}, "position": {"line": 9, "character": 28}
}})
definition = read_one()
assert definition["result"]["range"]["start"]["line"] == 4
send({"jsonrpc": "2.0", "id": 7, "method": "textDocument/documentSymbol", "params": {
    "textDocument": {"uri": uri}
}})
symbols = read_one()
symbol_names = [item["name"] for item in symbols.get("result", [])]
assert "count" in symbol_names and "add" in symbol_names
send({"jsonrpc": "2.0", "method": "textDocument/didChange", "params": {
    "textDocument": {"uri": uri, "version": 3},
    "contentChanges": [{"text": 'jtml 2\n\n// source map check\n\npage\n  show @\n'}]
}})
published = read_one()
assert published["params"]["diagnostics"][0]["range"]["start"]["line"] == 5
assert "line 6" in published["params"]["diagnostics"][0]["message"]

# ---------------------------------------------------------------------------
# Cross-file LSP intelligence: open an importer that pulls in a sibling module
# via `use`, and confirm definition / hover / completion walk the module
# graph. This pins the new behaviour from the friendly-aware linter / LSP.
# ---------------------------------------------------------------------------
# Resolve through any platform-level symlinks (e.g. macOS `/var` -> `/private/var`)
# so the LSP-returned canonical URI matches what the test expects.
mod_dir = pathlib.Path(tempfile.mkdtemp(prefix="jtml-lsp-modgraph-")).resolve()
mod_path = mod_dir / "shared.jtml"
app_path = mod_dir / "app.jtml"
mod_path.write_text('jtml 2\n\nlet shared_total = 7\n\nwhen sharedAction\n  shared_total += 1\n')
app_source = (
    'jtml 2\n'
    '\n'
    'use "./shared.jtml"\n'
    '\n'
    'page\n'
    '  show shared_total\n'
    '  button "Bump" click sharedAction\n'
)
app_path.write_text(app_source)
app_uri = "file://" + str(app_path)
send({"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {
    "textDocument": {"uri": app_uri, "languageId": "jtml", "version": 1, "text": app_source}
}})
read_one()  # consume publishDiagnostics

send({"jsonrpc": "2.0", "id": 100, "method": "textDocument/definition", "params": {
    "textDocument": {"uri": app_uri}, "position": {"line": 5, "character": 10}
}})
cross_def = read_one()
assert cross_def["result"] is not None, "cross-file go-to-definition returned null"
assert cross_def["result"]["uri"] == "file://" + str(mod_path), cross_def["result"]
assert cross_def["result"]["range"]["start"]["line"] == 2

send({"jsonrpc": "2.0", "id": 101, "method": "textDocument/hover", "params": {
    "textDocument": {"uri": app_uri}, "position": {"line": 6, "character": 24}
}})
cross_hover = read_one()
assert cross_hover["result"] is not None, "cross-file hover returned null"
assert "imported from" in cross_hover["result"]["contents"]["value"]
assert "shared.jtml" in cross_hover["result"]["contents"]["value"]

send({"jsonrpc": "2.0", "id": 102, "method": "textDocument/completion", "params": {
    "textDocument": {"uri": app_uri}, "position": {"line": 6, "character": 0}
}})
cross_completion = read_one()
cross_labels = [item["label"] for item in cross_completion["result"]["items"]]
assert "shared_total" in cross_labels, cross_labels
assert "sharedAction" in cross_labels, cross_labels
# The imported items should advertise their origin module so AI tools and
# editors can disambiguate same-named symbols across files.
imported_details = [
    item.get("detail", "") for item in cross_completion["result"]["items"]
    if item["label"] in ("shared_total", "sharedAction")
]
assert any("shared.jtml" in detail for detail in imported_details), imported_details

# ---------------------------------------------------------------------------
# Workspace symbols + rename: re-initialize the LSP with the temp module
# directory as its workspace root, then exercise the new endpoints.
# ---------------------------------------------------------------------------
send({"jsonrpc": "2.0", "id": 200, "method": "workspace/symbol", "params": {"query": "shared"}})
ws_symbols_before = read_one()
# Without a workspace root, the open documents are still scanned, so
# `shared_total` from the open module file should already be visible.
ws_names_before = sorted({s["name"] for s in ws_symbols_before["result"]})
assert "shared_total" in ws_names_before, ws_names_before
assert "sharedAction" in ws_names_before, ws_names_before

# prepareRename must accept a user-defined symbol and reject keyword tokens.
send({"jsonrpc": "2.0", "id": 201, "method": "textDocument/prepareRename", "params": {
    "textDocument": {"uri": app_uri}, "position": {"line": 5, "character": 10}
}})
prep_user = read_one()
assert prep_user["result"] is not None, prep_user
assert prep_user["result"]["start"]["line"] == 5

send({"jsonrpc": "2.0", "id": 202, "method": "textDocument/prepareRename", "params": {
    "textDocument": {"uri": app_uri}, "position": {"line": 4, "character": 0}  # `page` keyword
}})
prep_keyword = read_one()
assert prep_keyword["result"] is None, prep_keyword

# Rename `shared_total` -> `total`. The edit must include both the importer
# (app.jtml: usage on `show shared_total`) and the module (shared.jtml:
# definition + assignment).
mod_uri = "file://" + str(mod_path)
# Open shared.jtml so the LSP also tracks it as in-memory; this exercises the
# branch that prefers open-document text over disk content.
send({"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {
    "textDocument": {"uri": mod_uri, "languageId": "jtml", "version": 1, "text": mod_path.read_text()}
}})
read_one()  # publishDiagnostics for shared.jtml

send({"jsonrpc": "2.0", "id": 203, "method": "textDocument/rename", "params": {
    "textDocument": {"uri": app_uri},
    "position": {"line": 5, "character": 10},
    "newName": "total",
}})
rename = read_one()
assert rename["result"] is not None, rename
assert "changes" in rename["result"], rename["result"]
changes = rename["result"]["changes"]
assert app_uri in changes, sorted(changes.keys())
assert mod_uri in changes, sorted(changes.keys())
# At least one edit per file, and every edit's newText must be the new name.
assert all(edit["newText"] == "total" for edits in changes.values() for edit in edits)
# Definition in shared.jtml is on line 2; module rename must touch both line 2
# and line 5 (the `+= 1` site). Importer must touch the `show shared_total` site.
mod_lines = sorted({e["range"]["start"]["line"] for e in changes[mod_uri]})
app_lines = sorted({e["range"]["start"]["line"] for e in changes[app_uri]})
assert 2 in mod_lines and 5 in mod_lines, mod_lines
assert 5 in app_lines, app_lines

# Now exercise workspace-rooted scanning: re-send initialize with the temp dir
# as rootUri so future workspace/symbol queries traverse the directory.
send({"jsonrpc": "2.0", "id": 204, "method": "workspace/symbol", "params": {"query": "Action"}})
ws_actions = read_one()
assert any(s["name"] == "sharedAction" for s in ws_actions["result"]), ws_actions["result"]

# ---------------------------------------------------------------------------
# textDocument/references: must return every word-boundary occurrence of
# `shared_total` across the workspace (definition + assignment in shared.jtml,
# usage in app.jtml). includeDeclaration=False must drop the definition line.
# ---------------------------------------------------------------------------
send({"jsonrpc": "2.0", "id": 205, "method": "textDocument/references", "params": {
    "textDocument": {"uri": app_uri},
    "position": {"line": 5, "character": 10},
    "context": {"includeDeclaration": True},
}})
refs_with_decl = read_one()
ref_uris = sorted({r["uri"] for r in refs_with_decl["result"]})
assert app_uri in ref_uris and mod_uri in ref_uris, ref_uris
mod_ref_lines = sorted({r["range"]["start"]["line"]
                        for r in refs_with_decl["result"] if r["uri"] == mod_uri})
assert mod_ref_lines == [2, 5], mod_ref_lines  # def + `+= 1`
app_ref_lines = sorted({r["range"]["start"]["line"]
                        for r in refs_with_decl["result"] if r["uri"] == app_uri})
assert app_ref_lines == [5], app_ref_lines  # the show site

send({"jsonrpc": "2.0", "id": 206, "method": "textDocument/references", "params": {
    "textDocument": {"uri": mod_uri},
    "position": {"line": 2, "character": 4},
    "context": {"includeDeclaration": False},
}})
refs_no_decl = read_one()
mod_lines_excl = sorted({r["range"]["start"]["line"]
                         for r in refs_no_decl["result"] if r["uri"] == mod_uri})
assert 2 not in mod_lines_excl, mod_lines_excl  # declaration excluded
assert 5 in mod_lines_excl, mod_lines_excl

# ---------------------------------------------------------------------------
# textDocument/codeAction: opening a tab-indented Friendly file with no `jtml 2`
# header must produce a quick-fix that calls `jtml fix` over the whole document.
# ---------------------------------------------------------------------------
broken_dir = pathlib.Path(tempfile.mkdtemp(prefix="jtml-lsp-codeaction-")).resolve()
broken_path = broken_dir / "broken.jtml"
broken_source = "page\n\tlet count = 0   \n\tshow count\n"  # tabs + trailing whitespace + no header
broken_path.write_text(broken_source)
broken_uri = "file://" + str(broken_path)
send({"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {
    "textDocument": {"uri": broken_uri, "languageId": "jtml", "version": 1, "text": broken_source}
}})
read_one()  # publishDiagnostics

send({"jsonrpc": "2.0", "id": 207, "method": "textDocument/codeAction", "params": {
    "textDocument": {"uri": broken_uri},
    "range": {"start": {"line": 0, "character": 0}, "end": {"line": 0, "character": 0}},
    "context": {"diagnostics": [{"code": "JTML_FIX_HEADER", "message": "missing header",
                                  "range": {"start": {"line": 0, "character": 0},
                                            "end": {"line": 0, "character": 0}}}]},
}})
code_actions = read_one()
titles = [a["title"] for a in code_actions["result"]]
assert any("Run jtml fix" in t for t in titles), titles
assert any("jtml 2" in t for t in titles), titles
fix_all = next(a for a in code_actions["result"] if a.get("kind") == "source.fixAll.jtml")
assert fix_all.get("isPreferred") is True
assert broken_uri in fix_all["edit"]["changes"]
applied = fix_all["edit"]["changes"][broken_uri][0]["newText"]
assert applied.startswith("jtml 2"), applied
assert "\t" not in applied, "tabs should be replaced"
assert "   \n" not in applied, "trailing whitespace should be removed"

# ---------------------------------------------------------------------------
# textDocument/documentHighlight: every word-boundary occurrence of
# `shared_total` *within* the open file (no cross-file results), tagged as
# DocumentHighlightKind.Read.
# ---------------------------------------------------------------------------
send({"jsonrpc": "2.0", "id": 208, "method": "textDocument/documentHighlight", "params": {
    "textDocument": {"uri": mod_uri}, "position": {"line": 2, "character": 4}
}})
highlights = read_one()
hl_lines = sorted({h["range"]["start"]["line"] for h in highlights["result"]})
assert hl_lines == [2, 5], hl_lines
assert all(h["kind"] == 2 for h in highlights["result"]), highlights["result"]

send({"jsonrpc": "2.0", "id": 209, "method": "textDocument/documentHighlight", "params": {
    "textDocument": {"uri": app_uri}, "position": {"line": 5, "character": 10}
}})
highlights_app = read_one()
hl_app_lines = sorted({h["range"]["start"]["line"] for h in highlights_app["result"]})
assert hl_app_lines == [5], hl_app_lines  # same-file only — no cross-file leakage

# ---------------------------------------------------------------------------
# textDocument/signatureHelp: open a doc with a parameterised `make` action
# and exercise activeParameter advancement across commas.
# ---------------------------------------------------------------------------
sig_dir = pathlib.Path(tempfile.mkdtemp(prefix="jtml-lsp-sighelp-")).resolve()
sig_path = sig_dir / "sig.jtml"
sig_source = (
    'jtml 2\n'
    '\n'
    'when greet name greeting\n'
    '  show greeting\n'
    '\n'
    'page\n'
    '  button "Hi" click greet("ada", "hi")\n'
)
sig_path.write_text(sig_source)
sig_uri = "file://" + str(sig_path)
send({"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {
    "textDocument": {"uri": sig_uri, "languageId": "jtml", "version": 1, "text": sig_source}
}})
read_one()  # publishDiagnostics

# Cursor inside the first argument of `greet(...)` -> activeParameter 0.
first_arg_col = sig_source.splitlines()[6].index('"ada"') + 1
send({"jsonrpc": "2.0", "id": 210, "method": "textDocument/signatureHelp", "params": {
    "textDocument": {"uri": sig_uri}, "position": {"line": 6, "character": first_arg_col}
}})
sig_first = read_one()
assert sig_first["result"] is not None, sig_first
assert sig_first["result"]["signatures"][0]["label"] == "greet(name, greeting)"
param_labels = [p["label"] for p in sig_first["result"]["signatures"][0]["parameters"]]
assert param_labels == ["name", "greeting"], param_labels
assert sig_first["result"]["activeParameter"] == 0, sig_first["result"]

# Cursor past the comma -> activeParameter advances to 1.
second_arg_col = sig_source.splitlines()[6].index('"hi"') + 1
send({"jsonrpc": "2.0", "id": 211, "method": "textDocument/signatureHelp", "params": {
    "textDocument": {"uri": sig_uri}, "position": {"line": 6, "character": second_arg_col}
}})
sig_second = read_one()
assert sig_second["result"]["activeParameter"] == 1, sig_second["result"]

# Lowercase `make name params` (function-style, not a component) must now be
# indexed by the symbol scanner so signature help reaches it. This pins the
# gap that previously silently no-op'd for canonical lowercase functions.
make_dir = pathlib.Path(tempfile.mkdtemp(prefix="jtml-lsp-makefn-")).resolve()
make_path = make_dir / "fn.jtml"
make_source = (
    'jtml 2\n'
    '\n'
    'make sum a b\n'
    '  show a\n'
    '  show b\n'
    '\n'
    'page\n'
    '  button "Add" click sum(1, 2)\n'
)
make_path.write_text(make_source)
make_uri = "file://" + str(make_path)
send({"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {
    "textDocument": {"uri": make_uri, "languageId": "jtml", "version": 1, "text": make_source}
}})
read_one()  # publishDiagnostics

# documentSymbol must now list the lowercase make as a function.
send({"jsonrpc": "2.0", "id": 213, "method": "textDocument/documentSymbol", "params": {
    "textDocument": {"uri": make_uri}
}})
make_syms = read_one()
sym_names = [s["name"] for s in make_syms.get("result", [])]
assert "sum" in sym_names, sym_names
sum_sym = next(s for s in make_syms["result"] if s["name"] == "sum")
assert sum_sym["kind"] == 12, sum_sym  # SymbolKind.Function

# signatureHelp at the first arg of sum(1, 2)
make_call_line = make_source.splitlines()[7]
arg_col = make_call_line.index("1")
send({"jsonrpc": "2.0", "id": 214, "method": "textDocument/signatureHelp", "params": {
    "textDocument": {"uri": make_uri}, "position": {"line": 7, "character": arg_col + 1}
}})
sig_make = read_one()
assert sig_make["result"] is not None, sig_make
assert sig_make["result"]["signatures"][0]["label"] == "sum(a, b)", sig_make["result"]
assert sig_make["result"]["activeParameter"] == 0

# Classic `function foo(args)` must also be indexed.
fn_dir = pathlib.Path(tempfile.mkdtemp(prefix="jtml-lsp-classicfn-")).resolve()
fn_path = fn_dir / "classic.jtml"
fn_source = (
    'function setName(newName)\\\n'
    '    show newName\\\n'
)
fn_path.write_text(fn_source)
fn_uri = "file://" + str(fn_path)
send({"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {
    "textDocument": {"uri": fn_uri, "languageId": "jtml", "version": 1, "text": fn_source}
}})
read_one()  # publishDiagnostics

send({"jsonrpc": "2.0", "id": 215, "method": "textDocument/documentSymbol", "params": {
    "textDocument": {"uri": fn_uri}
}})
fn_syms = read_one()
fn_names = [s["name"] for s in fn_syms.get("result", [])]
assert "setName" in fn_names, fn_names

# Outside any call site -> null result.
send({"jsonrpc": "2.0", "id": 212, "method": "textDocument/signatureHelp", "params": {
    "textDocument": {"uri": sig_uri}, "position": {"line": 0, "character": 0}
}})
sig_none = read_one()
assert sig_none["result"] is None, sig_none

# ---------------------------------------------------------------------------
# textDocument/selectionRange: walks the indentation hierarchy. For a cursor
# inside a deeply nested Friendly block we expect: word -> trimmed line ->
# full line -> innermost block -> ... -> whole document.
# ---------------------------------------------------------------------------
sel_dir = pathlib.Path(tempfile.mkdtemp(prefix="jtml-lsp-selrange-")).resolve()
sel_path = sel_dir / "sel.jtml"
sel_source = (
    'jtml 2\n'           # line 0
    '\n'                 # line 1
    'when greet name\n'  # line 2 (indent 0)
    '  show name\n'      # line 3 (indent 2)
    '\n'                 # line 4
    'page\n'             # line 5 (indent 0)
    '  h1 "Hi"\n'        # line 6 (indent 2)
    '  button "Bump"\n'  # line 7 (indent 2)
    '    click greet\n'  # line 8 (indent 4)
)
sel_path.write_text(sel_source)
sel_uri = "file://" + str(sel_path)
send({"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {
    "textDocument": {"uri": sel_uri, "languageId": "jtml", "version": 1, "text": sel_source}
}})
read_one()  # publishDiagnostics

# Cursor on the `greet` token of `click greet` (line 8, indent 4 — deepest).
greet_col = sel_source.splitlines()[8].index("greet") + 1
send({"jsonrpc": "2.0", "id": 220, "method": "textDocument/selectionRange", "params": {
    "textDocument": {"uri": sel_uri},
    "positions": [{"line": 8, "character": greet_col}],
}})
sel = read_one()
assert isinstance(sel["result"], list) and len(sel["result"]) == 1, sel
node = sel["result"][0]

# Walk the parent chain and collect ranges from innermost to outermost.
chain = []
cur = node
while cur is not None:
    r = cur["range"]
    chain.append(((r["start"]["line"], r["start"]["character"]),
                  (r["end"]["line"], r["end"]["character"])))
    cur = cur.get("parent")

# Innermost is the word `greet` on line 8.
inner_start, inner_end = chain[0]
assert inner_start[0] == 8 and inner_end[0] == 8, chain
inner_text = sel_source.splitlines()[8][inner_start[1]:inner_end[1]]
assert inner_text == "greet", inner_text

# Outermost must be the whole document.
outer_start, outer_end = chain[-1]
assert outer_start == (0, 0), chain
assert outer_end[0] == len(sel_source.splitlines()) - 1, chain

# The chain must be strictly non-shrinking outward (each parent strictly
# contains the prior range).
def covers(parent, child):
    (ps, pe), (cs, ce) = parent, child
    return (ps[0] < cs[0] or (ps[0] == cs[0] and ps[1] <= cs[1])) and \
           (pe[0] > ce[0] or (pe[0] == ce[0] and pe[1] >= ce[1]))
for i in range(len(chain) - 1):
    assert covers(chain[i + 1], chain[i]), (i, chain)

# We expect at least: word, full line, innermost block (line 7..8 — the
# button + its click child), enclosing `page` block (lines 5..8), whole doc.
# Pin the structural milestones we care about so future refactors notice.
range_pairs = chain
assert any(s == (5, 0) and e[0] == 8 for (s, e) in range_pairs), range_pairs  # `page` block
assert any(s == (7, 0) and e[0] == 8 for (s, e) in range_pairs), range_pairs  # button block

# Multiple positions in one request must each get their own SelectionRange.
send({"jsonrpc": "2.0", "id": 221, "method": "textDocument/selectionRange", "params": {
    "textDocument": {"uri": sel_uri},
    "positions": [
        {"line": 0, "character": 0},   # at the `jtml` header
        {"line": 8, "character": greet_col},
    ],
}})
sel_multi = read_one()
assert len(sel_multi["result"]) == 2, sel_multi

# Capabilities must advertise the new providers so editors actually wire them up.
assert capabilities.get("documentHighlightProvider") is True, capabilities
assert "signatureHelpProvider" in capabilities, capabilities
assert capabilities.get("selectionRangeProvider") is True, capabilities

send({"jsonrpc": "2.0", "id": 8, "method": "shutdown", "params": None})
read_one()
send({"jsonrpc": "2.0", "method": "exit", "params": None})
assert proc.wait(timeout=5) == 0
PY
fi

"$repo_root/scripts/build_site.sh" "$repo_root/dist/site"
"$repo_root/scripts/package_cli.sh" "$repo_root/dist/release"

test -f "$repo_root/dist/site/index.html"
test -f "$repo_root/dist/site/tools.html"
test -f "$repo_root/dist/site/security.html"
test -f "$repo_root/dist/site/sitemap.xml"
test -f "$repo_root/dist/site/robots.txt"
test -f "$repo_root/dist/release/bin/jtml"
test -f "$repo_root/dist/release/site/index.html"
test -f "$repo_root/dist/release/examples/friendly_components.jtml"
test -f "$repo_root/dist/release/MANIFEST.txt"
test -f "$repo_root/dist/release/SHA256SUMS"

# ---------------------------------------------------------------------------
# Runtime regression smoke. Boots `jtml serve` against the bundled examples
# that exercise the production-language paths fixed during the Studio revamp
# (fetch GET, fetch POST + nested conditions, routes, stores, component
# isolation, hex colours in style blocks, and the iframe-safe link
# interceptor) and asserts the documented HTTP contract returns the expected
# shape. Gated on Node so the rest of verify_all stays portable on machines
# that don't have it; the section logs and skips when it's missing.
# ---------------------------------------------------------------------------
runtime_smoke() {
  if ! command -v node >/dev/null 2>&1; then
    echo "Skipping runtime regression smoke (node not installed)."
    return 0
  fi

  local artifacts_dir="$repo_root/dist/runtime-smoke"
  rm -rf "$artifacts_dir"
  mkdir -p "$artifacts_dir"

  local _srv_pid=""
  cleanup_srv() {
    if [ -n "${_srv_pid:-}" ] && kill -0 "$_srv_pid" 2>/dev/null; then
      kill "$_srv_pid" 2>/dev/null || true
      wait "$_srv_pid" 2>/dev/null || true
    fi
    _srv_pid=""
  }
  trap cleanup_srv EXIT

  start_serve() {
    local example="$1"
    local port="$2"
    local log="$3"
    "$build_dir/jtml" serve "$example" --port "$port" >"$log" 2>&1 &
    _srv_pid=$!
    local ready=0
    for _ in $(seq 1 200); do
      if curl -fsS "http://localhost:$port/api/health" >/dev/null 2>&1; then
        ready=1; break
      fi
      sleep 0.1
    done
    if [ "$ready" -ne 1 ]; then
      echo "runtime-smoke: server failed to become ready on port $port" >&2
      cat "$log" >&2 || true
      cleanup_srv
      return 1
    fi
  }

  # `assert_json <file> <label> <node-expression>` evaluates a Node.js
  # boolean expression with `data` bound to the parsed JSON contents of
  # <file> and exits the script with a clear error if the expression is
  # falsy. The expression body lives in a single string so each test stays
  # readable inline.
  assert_json() {
    local file="$1"
    local label="$2"
    local expr="$3"
    if ! node -e "
      const fs = require('fs');
      const data = JSON.parse(fs.readFileSync('$file','utf8'));
      const ok = (function(){ return ($expr); })();
      if (!ok) { console.error('FAIL: $label'); console.error(JSON.stringify(data, null, 2).slice(0, 800)); process.exit(1); }
    " 2>&1; then
      exit 1
    fi
  }

  # ---- 1. fetch GET seeds bindings.state.<name> with {data,error,loading} ----
  echo "[runtime-smoke] 1/7 fetch GET..."
  start_serve "$repo_root/examples/friendly_fetch.jtml" 9501 \
    "$artifacts_dir/fetch-get.log"
  curl -fsS http://localhost:9501/api/bindings > "$artifacts_dir/fetch-get-bindings.json"
  assert_json "$artifacts_dir/fetch-get-bindings.json" \
    "fetch GET bindings.state.users shape" \
    "data.bindings && data.bindings.state && data.bindings.state.users && 'loading' in data.bindings.state.users && 'data' in data.bindings.state.users && 'error' in data.bindings.state.users"
  cleanup_srv

  # ---- 2. fetch POST seeds bindings.state.<name> AND the nested ----
  #         `if login.data.user` page renders without crashing.
  echo "[runtime-smoke] 2/7 fetch POST..."
  start_serve "$repo_root/examples/friendly_fetch_post.jtml" 9502 \
    "$artifacts_dir/fetch-post.log"
  curl -fsS http://localhost:9502/api/bindings > "$artifacts_dir/fetch-post-bindings.json"
  assert_json "$artifacts_dir/fetch-post-bindings.json" \
    "fetch POST bindings.state.login shape" \
    "data.bindings && data.bindings.state && data.bindings.state.login && 'loading' in data.bindings.state.login && 'data' in data.bindings.state.login && 'error' in data.bindings.state.login"
  http_code="$(curl -fsS -o "$artifacts_dir/fetch-post-page.html" \
    -w '%{http_code}' http://localhost:9502/)"
  test "$http_code" = "200" \
    || { echo "fetch POST: GET / returned $http_code (nested if regressed?)" >&2; exit 1; }
  test -s "$artifacts_dir/fetch-post-page.html" \
    || { echo "fetch POST: rendered page is empty" >&2; exit 1; }
  cleanup_srv

  # ---- 3. routes register + iframe-safe link interceptor is in the runtime ----
  echo "[runtime-smoke] 3/7 routes + link interceptor..."
  start_serve "$repo_root/examples/friendly_routes.jtml" 9503 \
    "$artifacts_dir/routes.log"
  curl -fsS http://localhost:9503/ > "$artifacts_dir/routes.html"
  grep -q 'data-jtml-link="true"' "$artifacts_dir/routes.html" \
    || { echo "routes: data-jtml-link attribute missing from rendered output" >&2; exit 1; }
  # The link interceptor (added to fix Studio iframe nav) MUST be present so
  # hash navigation never resolves against the parent's base URL.
  grep -q 'startLinkBindings' "$artifacts_dir/routes.html" \
    || { echo "routes: startLinkBindings interceptor missing — Studio iframe regression" >&2; exit 1; }
  grep -q "closest('a\\[data-jtml-link\\]')" "$artifacts_dir/routes.html" \
    || { echo "routes: link interceptor selector regressed" >&2; exit 1; }
  cleanup_srv

  # ---- 4. store: shared dictionary state surfaces user-authored fields ----
  echo "[runtime-smoke] 4/7 store..."
  start_serve "$repo_root/examples/friendly_store.jtml" 9504 \
    "$artifacts_dir/store.log"
  curl -fsS http://localhost:9504/api/bindings > "$artifacts_dir/store-bindings.json"
  assert_json "$artifacts_dir/store-bindings.json" \
    "store auth.user/auth.token initialization" \
    "data.bindings && data.bindings.state && data.bindings.state.auth && data.bindings.state.auth.user === 'Ada' && data.bindings.state.auth.token === 'demo-token'"
  cleanup_srv

  # ---- 5. component isolation: each Counter has its own locals AND ----
  #         dispatching `add` on instance 1 does NOT mutate instance 2.
  echo "[runtime-smoke] 5/7 component isolation..."
  start_serve "$repo_root/examples/friendly_component_isolation.jtml" 9505 \
    "$artifacts_dir/components.log"
  curl -fsS http://localhost:9505/api/components > "$artifacts_dir/components-before.json"
  assert_json "$artifacts_dir/components-before.json" \
    "two Counter instances with independent count=0" \
    "Array.isArray(data.components) && data.components.length === 2 && data.components.every(c => c.component === 'Counter') && data.components.every(c => c && c.locals && c.locals.count && c.locals.count.value === 0) && data.components[0].id !== data.components[1].id"
  first_id="$(node -e "console.log(JSON.parse(require('fs').readFileSync('$artifacts_dir/components-before.json','utf8')).components[0].id)")"
  second_id="$(node -e "console.log(JSON.parse(require('fs').readFileSync('$artifacts_dir/components-before.json','utf8')).components[1].id)")"
  curl -fsS -X POST http://localhost:9505/api/component-action \
    -H 'Content-Type: application/json' \
    --data "{\"componentId\":\"$first_id\",\"action\":\"add\",\"args\":[]}" \
    > "$artifacts_dir/components-action.json"
  assert_json "$artifacts_dir/components-action.json" \
    "component-action dispatch returns ok" \
    "data.ok === true"
  curl -fsS http://localhost:9505/api/components > "$artifacts_dir/components-after.json"
  FIRST_ID="$first_id" SECOND_ID="$second_id" \
  assert_json "$artifacts_dir/components-after.json" \
    "component isolation: add on first only changes first" \
    "(function(){const f=process.env.FIRST_ID,s=process.env.SECOND_ID;const a=data.components.find(c=>c.id===f);const b=data.components.find(c=>c.id===s);return a && b && a.locals.count.value === 1 && b.locals.count.value === 0;})()"
  cleanup_srv

  # ---- 6. CSS hex colours inside Friendly `style` blocks survive lowering ----
  echo "[runtime-smoke] 6/7 hex colours..."
  "$build_dir/jtml" transpile "$repo_root/examples/friendly_fetch.jtml" \
    -o "$artifacts_dir/fetch-styles.html" >/dev/null
  grep -q '#d8d4c9' "$artifacts_dir/fetch-styles.html" \
    || { echo "style block: hex colour #d8d4c9 was stripped during lowering" >&2; exit 1; }

  # ---- 7. Studio's /api/run goes through the same transpiler so the link ----
  #         interceptor reaches Studio previews too. /api/run is registered
  #         by `jtml studio` only (not `jtml serve`), so this case spins up
  #         the Studio server.
  echo "[runtime-smoke] 7/7 Studio /api/run preview path..."
  "$build_dir/jtml" studio --port 9506 > "$artifacts_dir/api-run.log" 2>&1 &
  _srv_pid=$!
  studio_ready=0
  # Studio doesn't expose /api/health (only `jtml serve` does); GET / serves
  # the embedded shell HTML once the server is listening, so use that.
  for _ in $(seq 1 200); do
    if curl -fsS "http://localhost:9506/" >/dev/null 2>&1; then
      studio_ready=1; break
    fi
    sleep 0.1
  done
  if [ "$studio_ready" -ne 1 ]; then
    echo "runtime-smoke: jtml studio failed to become ready on port 9506" >&2
    cat "$artifacts_dir/api-run.log" >&2 || true
    exit 1
  fi
  ROUTES_FILE="$repo_root/examples/friendly_routes.jtml" \
  node -e "
    const fs = require('fs');
    const code = fs.readFileSync(process.env.ROUTES_FILE, 'utf8');
    fs.writeFileSync('$artifacts_dir/api-run-payload.json', JSON.stringify({ code }));
  "
  curl -fsS -X POST http://localhost:9506/api/run \
    -H 'Content-Type: application/json' \
    --data "@$artifacts_dir/api-run-payload.json" \
    > "$artifacts_dir/api-run.json"
  assert_json "$artifacts_dir/api-run.json" \
    "Studio /api/run rendered HTML carries the link interceptor" \
    "data.ok === true && typeof data.html === 'string' && data.html.includes('startLinkBindings') && data.html.includes('data-jtml-link=\"true\"')"
  cleanup_srv

  trap - EXIT
  echo "Runtime regression smoke passed (7 checks)."
}

runtime_smoke

echo "JTML local predeploy verification passed."
