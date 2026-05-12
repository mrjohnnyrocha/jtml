const vscode = require("vscode");
const cp = require("child_process");
const fs = require("fs");
const os = require("os");
const path = require("path");

const CHECK_DELAY_MS = 350;
const MAX_BUFFER = 1024 * 1024 * 4;

let diagnostics;
let timers = new Map();
let languageClient = null;
let languageClientStartFailed = false;

// vscode-languageclient is loaded lazily so the extension still installs and
// runs without it. When the dependency is present we launch `jtml lsp`, which
// provides diagnostics, formatting, completion, hover, definition, references,
// rename, code actions, signature help, and document highlights natively.
// When it's missing (or the user opts out), we fall back to the legacy
// CLI-shell path that calls `jtml check`/`jtml lint`/`jtml fmt` directly.
function tryLoadLanguageClient() {
  try {
    return require("vscode-languageclient/node");
  } catch (_) {
    return null;
  }
}

function useLanguageServer() {
  return config().get("languageServer.enabled", true);
}

function config() {
  return vscode.workspace.getConfiguration("jtml");
}

function candidateCliPaths(context, document) {
  const configured = config().get("executablePath", "");
  const roots = vscode.workspace.workspaceFolders || [];
  const candidates = [];

  if (configured) candidates.push(configured);
  if (document && document.uri.scheme === "file") {
    candidates.push(path.join(findAncestor(document.uri.fsPath, "build") || "", "jtml"));
  }
  for (const root of roots) {
    candidates.push(path.join(root.uri.fsPath, "build", "jtml"));
  }
  candidates.push(path.join(context.extensionPath, "..", "..", "build", "jtml"));
  candidates.push("jtml");

  return candidates.filter(Boolean);
}

function findAncestor(filePath, childName) {
  let current = path.dirname(filePath);
  while (current && current !== path.dirname(current)) {
    const candidate = path.join(current, childName);
    if (fs.existsSync(candidate)) return candidate;
    current = path.dirname(current);
  }
  return "";
}

function commandExists(command) {
  if (command.includes(path.sep)) {
    return fs.existsSync(command);
  }
  return true;
}

function findCli(context, document) {
  const cli = candidateCliPaths(context, document).find(commandExists);
  return cli || "jtml";
}

function execJtml(context, document, args) {
  const cli = findCli(context, document);
  return new Promise((resolve) => {
    cp.execFile(cli, args, { maxBuffer: MAX_BUFFER }, (error, stdout, stderr) => {
      resolve({ error, stdout, stderr, cli });
    });
  });
}

function jsonFromOutput(result) {
  if (!(result.stdout || "").trim()) {
    return {
      ok: false,
      diagnostics: [{
        severity: "error",
        code: "JTML_EXTENSION_CLI",
        message: `Could not run JTML CLI: ${result.error ? result.error.message : "no output"}`,
        hint: `Set jtml.executablePath or ensure ${result.cli} is available.`
      }]
    };
  }

  try {
    return JSON.parse(result.stdout);
  } catch (error) {
    return {
      ok: false,
      diagnostics: [{
        severity: "error",
        code: "JTML_EXTENSION_JSON",
        message: `Could not parse JTML CLI JSON output from ${result.cli}: ${error.message}`,
        hint: result.stderr || result.stdout || "Check jtml.executablePath."
      }]
    };
  }
}

function tempPathFor(document) {
  if (document.uri.scheme === "file") {
    const dir = path.dirname(document.uri.fsPath);
    const base = path.basename(document.uri.fsPath, ".jtml");
    return path.join(dir, `.${base}.vscode-check.${process.pid}.jtml`);
  }
  return path.join(os.tmpdir(), `jtml-vscode-${process.pid}-${Date.now()}.jtml`);
}

async function withTempDocument(document, fn) {
  const tmp = tempPathFor(document);
  fs.writeFileSync(tmp, document.getText(), "utf8");
  try {
    return await fn(tmp);
  } finally {
    try {
      fs.unlinkSync(tmp);
    } catch (_) {
      // Best effort cleanup only.
    }
  }
}

function rangeForDiagnostic(document, diagnostic) {
  const line = Math.max(0, (diagnostic.line || 1) - 1);
  const column = Math.max(0, (diagnostic.column || 1) - 1);
  const textLine = line < document.lineCount ? document.lineAt(line) : document.lineAt(document.lineCount - 1);
  const start = Math.min(column, textLine.range.end.character);
  const end = Math.min(Math.max(start + 1, textLine.firstNonWhitespaceCharacterIndex + 1), textLine.range.end.character);
  return new vscode.Range(line, start, line, end);
}

function severityOf(value) {
  if (value === "warning") return vscode.DiagnosticSeverity.Warning;
  if (value === "info") return vscode.DiagnosticSeverity.Information;
  if (value === "hint") return vscode.DiagnosticSeverity.Hint;
  return vscode.DiagnosticSeverity.Error;
}

function toVscodeDiagnostic(document, diagnostic) {
  const messageParts = [diagnostic.message || "JTML diagnostic"];
  if (diagnostic.hint) messageParts.push(`Hint: ${diagnostic.hint}`);
  if (diagnostic.example) messageParts.push(`Example:\n${diagnostic.example}`);

  const item = new vscode.Diagnostic(
    rangeForDiagnostic(document, diagnostic),
    messageParts.join("\n\n"),
    severityOf(diagnostic.severity)
  );
  item.source = "jtml";
  item.code = diagnostic.code || undefined;
  return item;
}

async function collectDiagnostics(context, document) {
  if (!config().get("diagnostics.enabled", true) || document.languageId !== "jtml") {
    return;
  }

  const result = await withTempDocument(document, async (tmp) => {
    const check = jsonFromOutput(await execJtml(context, document, ["check", tmp, "--json"]));
    if (!check.ok) return check;
    return jsonFromOutput(await execJtml(context, document, ["lint", tmp, "--json"]));
  });

  const items = (result.diagnostics || []).map((diagnostic) => toVscodeDiagnostic(document, diagnostic));
  diagnostics.set(document.uri, items);
}

function scheduleDiagnostics(context, document, immediate = false) {
  if (document.languageId !== "jtml") return;
  const key = document.uri.toString();
  if (timers.has(key)) clearTimeout(timers.get(key));
  const delay = immediate ? 0 : CHECK_DELAY_MS;
  timers.set(key, setTimeout(() => {
    timers.delete(key);
    collectDiagnostics(context, document);
  }, delay));
}

async function formatDocument(context, document) {
  return withTempDocument(document, async (tmp) => {
    const result = await execJtml(context, document, ["fmt", tmp]);
    if (result.error) {
      const detail = result.stderr || result.stdout || result.error.message;
      throw new Error(detail.trim());
    }
    const fullRange = new vscode.Range(
      document.positionAt(0),
      document.positionAt(document.getText().length)
    );
    return [vscode.TextEdit.replace(fullRange, result.stdout)];
  });
}

async function fixDocument(context) {
  const editor = vscode.window.activeTextEditor;
  if (!editor || editor.document.languageId !== "jtml") return;

  await withTempDocument(editor.document, async (tmp) => {
    const result = await execJtml(context, editor.document, ["fix", tmp, "-w", "--json"]);
    const payload = jsonFromOutput(result);
    if (!payload.ok && !payload.changed) {
      vscode.window.showWarningMessage(payload.error || "JTML safe fix could not repair this file.");
      return;
    }
    const fixed = fs.readFileSync(tmp, "utf8");
    const fullRange = new vscode.Range(
      editor.document.positionAt(0),
      editor.document.positionAt(editor.document.getText().length)
    );
    await editor.edit((builder) => builder.replace(fullRange, fixed));
    scheduleDiagnostics(context, editor.document, true);
    const count = (payload.changes || []).length;
    vscode.window.showInformationMessage(`JTML applied ${count} safe fix${count === 1 ? "" : "es"}.`);
  });
}

// ---------------------------------------------------------------------------
// Language client lifecycle
// ---------------------------------------------------------------------------
function startLanguageClient(context) {
  const lc = tryLoadLanguageClient();
  if (!lc) {
    languageClientStartFailed = true;
    return null;
  }
  const cli = findCli(context, vscode.window.activeTextEditor && vscode.window.activeTextEditor.document);
  if (!cli) {
    languageClientStartFailed = true;
    return null;
  }
  // We pass an empty document because the LSP server itself decides workspace
  // root via `initialize` params. The CLI binary `jtml lsp` speaks JSON-RPC
  // over stdio.
  const serverOptions = {
    run: { command: cli, args: ["lsp"], transport: lc.TransportKind.stdio },
    debug: { command: cli, args: ["lsp"], transport: lc.TransportKind.stdio },
  };
  const clientOptions = {
    documentSelector: [{ scheme: "file", language: "jtml" }],
    synchronize: {
      fileEvents: vscode.workspace.createFileSystemWatcher("**/*.jtml"),
      configurationSection: "jtml",
    },
    diagnosticCollectionName: "jtml",
  };
  try {
    const client = new lc.LanguageClient("jtml", "JTML Language Server", serverOptions, clientOptions);
    client.start();
    return client;
  } catch (error) {
    vscode.window.showWarningMessage(
      `JTML LSP failed to start (${error && error.message ? error.message : error}); falling back to CLI diagnostics.`
    );
    languageClientStartFailed = true;
    return null;
  }
}

async function stopLanguageClient() {
  if (!languageClient) return;
  try {
    await languageClient.stop();
  } catch (_) {
    // Best-effort shutdown.
  } finally {
    languageClient = null;
  }
}

function registerCliFallback(context) {
  // The legacy CLI-shell path. Only registered when the LSP client is not
  // running, so we don't double-publish diagnostics or compete on formatting.
  diagnostics = vscode.languages.createDiagnosticCollection("jtml");
  context.subscriptions.push(diagnostics);

  for (const document of vscode.workspace.textDocuments) {
    scheduleDiagnostics(context, document, true);
  }

  context.subscriptions.push(vscode.workspace.onDidOpenTextDocument((document) => {
    scheduleDiagnostics(context, document, true);
  }));
  context.subscriptions.push(vscode.workspace.onDidSaveTextDocument((document) => {
    scheduleDiagnostics(context, document, true);
  }));
  context.subscriptions.push(vscode.workspace.onDidChangeTextDocument((event) => {
    if (config().get("diagnostics.onType", true)) {
      scheduleDiagnostics(context, event.document);
    }
  }));
  context.subscriptions.push(vscode.workspace.onDidCloseTextDocument((document) => {
    diagnostics.delete(document.uri);
  }));

  context.subscriptions.push(vscode.languages.registerDocumentFormattingEditProvider("jtml", {
    provideDocumentFormattingEdits(document) {
      return formatDocument(context, document);
    }
  }));
}

function activate(context) {
  // Prefer the native LSP. When unavailable (missing dep, missing CLI, or
  // user-disabled), fall back to the CLI-shell diagnostics/format path.
  if (useLanguageServer()) {
    languageClient = startLanguageClient(context);
  }

  if (!languageClient) {
    registerCliFallback(context);
  }

  context.subscriptions.push(vscode.commands.registerCommand("jtml.restartLanguageServer", async () => {
    await stopLanguageClient();
    if (useLanguageServer()) {
      languageClient = startLanguageClient(context);
      if (languageClient) {
        vscode.window.showInformationMessage("JTML language server restarted.");
      }
    }
  }));

  // The Apply Safe Fixes command stays available regardless of transport so
  // users with a keybinding keep their muscle memory; LSP code actions cover
  // the same ground but are scoped to the diagnostic at the cursor.
  context.subscriptions.push(vscode.commands.registerCommand("jtml.fixDocument", () => {
    fixDocument(context);
  }));

  // Hot-swap on configuration change so toggling the setting takes effect
  // without forcing a window reload.
  context.subscriptions.push(vscode.workspace.onDidChangeConfiguration(async (event) => {
    if (!event.affectsConfiguration("jtml.languageServer.enabled")) return;
    if (useLanguageServer() && !languageClient) {
      languageClient = startLanguageClient(context);
    } else if (!useLanguageServer() && languageClient) {
      await stopLanguageClient();
      vscode.window.showInformationMessage("JTML language server disabled.");
    }
  }));
}

async function deactivate() {
  for (const timer of timers.values()) clearTimeout(timer);
  timers.clear();
  if (diagnostics) diagnostics.dispose();
  await stopLanguageClient();
}

module.exports = { activate, deactivate };
