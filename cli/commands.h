// cli/commands.h
//
// Entry points for every CLI subcommand.  main.cpp dispatches to exactly one
// of these; each owns its own argument validation, execution, and exit codes.
// All live in `jtml::cli`.
#pragma once

#include "util.h"

namespace jtml::cli {

// ── Developer workflow ──────────────────────────────────────────────────────
int cmdNew      (const Options& o);   // scaffold a new .jtml page
int cmdDev      (const Options& o);   // zero-config app dev server
int cmdAdd      (const Options& o);   // install a local JTML package
int cmdInstall  (const Options& o);   // restore/verify packages from manifest + lock
int cmdServe    (const Options& o);   // live HTTP + WebSocket server
int cmdStudio   (const Options& o);   // full IDE + tutorial (merged demo+tutorial)

// ── Compiler pipeline ───────────────────────────────────────────────────────
int cmdCheck    (const Options& o);   // parse and report errors
int cmdFix      (const Options& o);   // conservative source repair
int cmdFmt      (const Options& o);   // format source (AST-driven, idempotent)
int cmdLint     (const Options& o);   // lint for common mistakes
int cmdTranspile(const Options& o);   // compile to static HTML
int cmdBuild    (const Options& o);   // build directory → dist/
int cmdExport   (const Options& o);   // export as HTML/React/Vue/custom-element wrapper
int cmdMigrate  (const Options& o);   // classic → friendly syntax migration
int cmdRefactor (const Options& o);   // semantic refactors (rename, ...)

// ── Tooling / discovery ─────────────────────────────────────────────────────
int cmdInterpret(const Options& o);   // run in the interpreter
int cmdTest     (const Options& o);   // smoke-test bundled examples
int cmdExamples (const Options& o);   // list bundled examples
int cmdDoctor   (const Options& o);   // verify local toolkit layout
int cmdGenerate (const Options& o);   // print AI-native generation prompt + starter
int cmdExplain  (const Options& o);   // explain a JTML source file
int cmdSuggest  (const Options& o);   // suggest refactors for a JTML source file
int cmdLsp      (const Options& o);   // Language Server Protocol over stdio

// ── Backward-compatible aliases ─────────────────────────────────────────────
int cmdDemo     (const Options& o);   // → cmdStudio
int cmdTutorial (const Options& o);   // → cmdStudio

} // namespace jtml::cli
