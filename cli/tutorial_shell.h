// cli/tutorial_shell.h
//
// HTML shell for `jtml tutorial`. Extracted into its own translation unit
// so cmd_tutorial.cpp stays focused on routing and lesson plumbing.
#pragma once

namespace jtml::cli {

// Zero-dependency (modulo CDN'd CodeMirror + marked.js) HTML shell that
// renders the split-view tutorial IDE. Served at `/` by cmdTutorial.
extern const char* kTutorialShellHTML;

} // namespace jtml::cli
