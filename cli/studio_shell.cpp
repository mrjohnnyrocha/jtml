// cli/studio_shell.cpp — unified browser shell for `jtml studio` / `jtml demo`.
#include "studio_shell.h"

namespace jtml::cli {

const char* kStudioShellHTML = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>JTML Studio</title>
<link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/codemirror@5.65.16/lib/codemirror.min.css">
<style>
/* ── Tokens ─────────────────────────────────────────────────── */
:root {
  --bg:     #f4f2ec;
  --panel:  #fdfcf8;
  --ink:    #192027;
  --muted:  #64707a;
  --border: #d8d4c8;
  --dark:   #101820;
  --dark-2: #0d151b;
  --line:   #26333d;
  --accent: #0f766e;
  --danger: #b42318;
  --warn:   #92400e;
  --ok:     #065f46;
}
*, *::before, *::after { box-sizing: border-box; }
html, body { height: 100%; margin: 0; overflow: hidden; }
body {
  background: var(--bg);
  color: var(--ink);
  font-family: Inter, ui-sans-serif, system-ui, -apple-system, "Segoe UI", sans-serif;
  font-size: 14px;
}
button { font: inherit; cursor: pointer; transition: border-color .1s, background .1s; }
button:disabled { opacity: .5; cursor: default; }

/* ── App ─────────────────────────────────────────────────────── */
.app {
  height: 100vh;
  display: flex;
  flex-direction: column;
  padding: 10px;
  gap: 8px;
  overflow: hidden;
}

/* ── Header ──────────────────────────────────────────────────── */
header {
  background: var(--dark);
  color: #fff;
  border-radius: 8px;
  padding: 10px 16px;
  display: flex;
  align-items: center;
  gap: 10px;
  flex-shrink: 0;
}
.logo { font-size: 20px; font-weight: 800; letter-spacing: -0.5px; white-space: nowrap; }
.logo sub { font-size: 11px; font-weight: 600; color: #7dd3c8; vertical-align: baseline; margin-left: 3px; }
.tagline { color: #6a8898; font-size: 12px; flex: 1; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.h-actions { display: flex; gap: 6px; align-items: center; flex-shrink: 0; }
.kb { color: #5c7a8a; font-size: 11px; white-space: nowrap; }
.kb kbd {
  font-family: "SF Mono", Menlo, Consolas, monospace; font-size: 10px;
  background: #1e2e38; border: 1px solid #3a4d58; border-bottom-width: 2px;
  border-radius: 3px; padding: 1px 5px; color: #a8c0cc;
}
.btn {
  border: 1px solid #a9b0b6; background: var(--panel); color: var(--ink);
  border-radius: 6px; padding: 6px 11px; font-weight: 700; font-size: 13px;
}
.btn:hover:not(:disabled) { border-color: #6e7b85; }
.btn.primary { background: var(--accent); border-color: var(--accent); color: #fff; }
.btn.dark {
  background: rgba(255,255,255,.07); border-color: rgba(255,255,255,.15);
  color: #9ab4c4; font-size: 14px; padding: 5px 10px;
}
.btn.dark:hover:not(:disabled) { background: rgba(255,255,255,.13); color: #d4e4ec; }
.btn.dark.active { background: rgba(125,211,200,.15); border-color: #7dd3c8; color: #7dd3c8; }

/* ── Main layout ─────────────────────────────────────────────── */
.main-layout {
  display: flex;
  flex-direction: row;
  flex: 1;
  min-height: 0;
  overflow: hidden;
}

/* ── Resize handles ──────────────────────────────────────────── */
.rh {
  flex-shrink: 0;
  position: relative;
  z-index: 20;
  background: transparent;
  transition: background .15s;
}
.rh.col { width: 6px; cursor: col-resize; }
.rh.row { height: 6px; cursor: row-resize; }
/* Visual line */
.rh.col::before {
  content: '';
  position: absolute;
  top: 0; bottom: 0;
  left: 50%; transform: translateX(-50%);
  width: 1px;
  background: var(--border);
  transition: width .12s, background .12s;
}
.rh.row::before {
  content: '';
  position: absolute;
  left: 0; right: 0;
  top: 50%; transform: translateY(-50%);
  height: 1px;
  background: var(--border);
  transition: height .12s, background .12s;
}
/* Grip dots */
.rh.col::after {
  content: '·\A·\A·';
  white-space: pre;
  line-height: 1.2;
  position: absolute;
  top: 50%; left: 50%;
  transform: translate(-50%, -50%);
  font-size: 12px;
  color: transparent;
  transition: color .12s;
  pointer-events: none;
}
.rh.row::after {
  content: '· · ·';
  position: absolute;
  top: 50%; left: 50%;
  transform: translate(-50%, -50%);
  font-size: 12px;
  letter-spacing: 2px;
  color: transparent;
  transition: color .12s;
  pointer-events: none;
}
.rh:hover::before, .rh.dragging::before { background: var(--accent); }
.rh.col:hover::before, .rh.col.dragging::before { width: 3px; }
.rh.row:hover::before, .rh.row.dragging::before { height: 3px; }
.rh:hover::after, .rh.dragging::after { color: var(--accent); }
/* Title tooltip on hover */
.rh[title] { pointer-events: auto; }

/* ── Sidebar ─────────────────────────────────────────────────── */
.sidebar {
  flex-shrink: 0;
  background: var(--panel);
  border: 1px solid var(--border);
  border-radius: 8px;
  display: flex;
  flex-direction: column;
  overflow: hidden;
  will-change: width;
}
.sidebar.collapsing { transition: width .2s ease; }
.sidebar-head {
  padding: 9px 10px 7px;
  border-bottom: 1px solid var(--border);
  flex-shrink: 0;
  display: flex;
  align-items: center;
  justify-content: space-between;
}
.sidebar-title { font-size: 12px; font-weight: 800; margin: 0; white-space: nowrap; overflow: hidden; }
.sidebar-body { flex: 1; overflow-y: auto; padding: 6px; display: flex; flex-direction: column; gap: 10px; }
.sb-section { display: flex; flex-direction: column; gap: 2px; }
.sb-category { margin: 8px 4px 2px; font-size: 10px; font-weight: 700; text-transform: uppercase; letter-spacing: .06em; color: var(--muted); }
.sb-category:first-child { margin-top: 2px; }
.sb-label {
  font-size: 10px; font-weight: 800; text-transform: uppercase;
  letter-spacing: .08em; color: var(--muted); padding: 2px 6px 4px;
}
.sb-item {
  width: 100%; text-align: left; background: transparent; border: 1px solid transparent;
  border-radius: 5px; padding: 6px 8px; font-size: 12px; font-weight: 500;
  display: flex; align-items: center; justify-content: space-between; gap: 4px;
  color: var(--ink); white-space: nowrap; overflow: hidden;
}
.sb-item span { overflow: hidden; text-overflow: ellipsis; }
.sb-item:hover:not(:disabled) { background: #edeae2; }
.sb-item.active-file { background: #e7f1ef; border-color: #b7d6d1; color: #0d5d57; }
.sb-item.active-lesson { background: #eff6ff; border-color: #bfdbfe; color: #1e40af; }
.v-badge { font-size: 10px; background: #d4e8e5; color: #0d5d57; border-radius: 999px; padding: 1px 5px; font-weight: 700; flex-shrink: 0; }
.sb-item:not(.active-file) .v-badge { background: #eceae2; color: var(--muted); }
.check-ok { color: #0f766e; font-size: 11px; flex-shrink: 0; }
.no-items { font-size: 12px; color: var(--muted); padding: 4px 8px; font-style: italic; }

/* ── Content column ──────────────────────────────────────────── */
.content {
  flex: 1;
  min-width: 0;
  display: flex;
  flex-direction: column;
  overflow: hidden;
}

/* ── Prose panel ─────────────────────────────────────────────── */
.prose-panel {
  flex-shrink: 0;
  background: var(--panel);
  border: 1px solid var(--border);
  border-radius: 8px;
  overflow: hidden;
  display: flex;
  flex-direction: column;
  will-change: height;
}
.prose-panel.collapsing { transition: height .2s ease; }
.prose-body {
  flex: 1; overflow-y: auto; padding: 12px 16px 6px;
  line-height: 1.6; font-size: 13px;
}
.prose-body h1 { font-size: 17px; margin: 0 0 8px; }
.prose-body h2 { font-size: 14px; margin: 10px 0 5px; }
.prose-body p  { margin: 5px 0; }
.prose-body code {
  font-family: "SF Mono", Menlo, Consolas, monospace; font-size: 12px;
  background: #f0ede5; padding: 1px 4px; border-radius: 3px;
}
.prose-body pre { background: #f0ede5; padding: 8px 10px; border-radius: 5px; overflow-x: auto; font-size: 12px; }
.prose-nav {
  display: flex; justify-content: space-between; align-items: center;
  padding: 6px 12px; border-top: 1px solid var(--border); flex-shrink: 0;
}
.prose-nav button {
  font: inherit; font-size: 12px; padding: 3px 9px;
  border: 1px solid var(--border); background: var(--panel); border-radius: 4px; color: inherit;
}
.prose-nav button:disabled { opacity: .35; }
.lesson-ctr { font-size: 12px; color: var(--muted); }

/* ── Work row (editor + preview) ─────────────────────────────── */
.work-row {
  flex: 1;
  min-height: 0;
  display: flex;
  flex-direction: row;
  overflow: hidden;
}

/* ── Editor panel ────────────────────────────────────────────── */
.editor-panel {
  flex-shrink: 0;
  display: flex; flex-direction: column;
  background: var(--dark-2); border: 1px solid var(--line); border-radius: 8px;
  overflow: hidden; min-width: 160px;
  will-change: flex-basis;
}
.editor-head {
  padding: 8px 11px;
  border-bottom: 1px solid var(--line);
  display: flex; align-items: center; justify-content: space-between; gap: 8px;
  flex-shrink: 0;
}
.editor-name { margin: 0; font-size: 12px; font-weight: 700; color: #b8d0da; }
.editor-right { display: flex; align-items: center; gap: 7px; }
.editor-meta { font: 10px/1 "SF Mono", Menlo, Consolas, monospace; color: #3d5970; }
.history-btn {
  font-size: 11px; padding: 2px 7px; font-weight: 600;
  background: rgba(255,255,255,.06); border: 1px solid rgba(255,255,255,.12);
  color: #6a8898; border-radius: 4px;
}
.history-btn:hover:not(:disabled) { background: rgba(255,255,255,.1); color: #b8c8d4; border-color: rgba(255,255,255,.22); }
.editor-body { flex: 1; min-height: 0; overflow: hidden; position: relative; }
.editor-foot {
  flex-shrink: 0; padding: 5px 11px;
  border-top: 1px solid var(--line);
  color: #3d5970;
  display: flex; align-items: center; justify-content: space-between;
  font: 10px/1 "SF Mono", Menlo, Consolas, monospace; gap: 8px;
}
#status        { transition: color .15s; }
#status.ok     { color: #4ade80; }
#status.err    { color: #f87171; }
#status.run    { color: #fbbf24; }

/* ── CodeMirror: jtml-dark theme ─────────────────────────────── */
.cm-s-jtml-dark.CodeMirror {
  height: 100%;
  font-family: "SF Mono", SFMono-Regular, Consolas, "Liberation Mono", monospace;
  font-size: 13px; line-height: 1.65;
  background: #0d151b; color: #cdd8e0;
}
.cm-s-jtml-dark .CodeMirror-scroll  { min-height: 80px; }
.cm-s-jtml-dark .CodeMirror-gutters { background: #0a1117; border-right: 1px solid #1e2d38; }
.cm-s-jtml-dark .CodeMirror-linenumber { color: #293f52; padding: 0 10px; font-size: 11px; }
.cm-s-jtml-dark .CodeMirror-cursor  { border-left: 2px solid #7dd3c8; }
.cm-s-jtml-dark .CodeMirror-activeline-background { background: rgba(125,211,200,.04); }
.cm-s-jtml-dark .CodeMirror-activeline .CodeMirror-gutter-elt { color: #5a8099; }
.cm-s-jtml-dark .CodeMirror-selected { background: rgba(125,211,200,.13); }
.cm-s-jtml-dark .CodeMirror-matchingbracket { color: #7dd3c8 !important; background: rgba(125,211,200,.16); outline: 1px solid #7dd3c8; }
/* Token palette */
.cm-s-jtml-dark .cm-string       { color: #f59e0b; }          /* amber  — string literals */
.cm-s-jtml-dark .cm-comment      { color: #3a5568; font-style: italic; }
.cm-s-jtml-dark .cm-jtml-kw      { color: #7dd3c8; font-weight: 600; } /* teal   — define/derive/show/… */
.cm-s-jtml-dark .cm-jtml-el      { color: #60a5fa; font-weight: 600; } /* blue   — element / @tag */
.cm-s-jtml-dark .cm-jtml-tag     { color: #93c5fd; font-weight: 600; } /* light-blue — tag name */
.cm-s-jtml-dark .cm-jtml-attr    { color: #a78bfa; }           /* lavender — HTML attrs  */
.cm-s-jtml-dark .cm-jtml-event   { color: #fb923c; font-weight: 600; } /* orange  — onClick/onInput/… */
.cm-s-jtml-dark .cm-jtml-term    { color: #243545; }           /* dim     — \\ */
.cm-s-jtml-dark .cm-jtml-close   { color: #243545; font-weight: 600; } /* dim     — # */
.cm-s-jtml-dark .cm-def          { color: #86efac; }           /* green   — function names */
.cm-s-jtml-dark .cm-number       { color: #c084fc; }           /* purple  — numbers */
.cm-s-jtml-dark .cm-atom         { color: #f472b6; }           /* pink    — true/false/null */
.cm-s-jtml-dark .cm-operator     { color: #3a5568; }
.cm-s-jtml-dark .cm-variable     { color: #cdd8e0; }
.cm-s-jtml-dark span.cm-jtml-kw,
.cm-s-jtml-dark span.cm-keyword  { color: #7dd3c8 !important; font-weight: 650; }
.cm-s-jtml-dark span.cm-jtml-el,
.cm-s-jtml-dark span.cm-tag      { color: #60a5fa !important; font-weight: 650; }
.cm-s-jtml-dark span.cm-jtml-tag { color: #93c5fd !important; font-weight: 650; }
.cm-s-jtml-dark span.cm-jtml-attr,
.cm-s-jtml-dark span.cm-attribute,
.cm-s-jtml-dark span.cm-property { color: #a78bfa !important; }
.cm-s-jtml-dark span.cm-jtml-event { color: #fb923c !important; font-weight: 650; }
.cm-s-jtml-dark span.cm-string   { color: #f59e0b !important; }
.cm-s-jtml-dark span.cm-number   { color: #c084fc !important; }
.cm-s-jtml-dark span.cm-comment  { color: #4b6678 !important; font-style: italic; }
.cm-s-jtml-dark span.cm-error    { color: #fecaca !important; background: rgba(185,28,28,.24); }

/* ── Preview panel ───────────────────────────────────────────── */
.preview-panel {
  flex: 1;
  min-width: 160px;
  display: flex; flex-direction: column;
  background: var(--panel); border: 1px solid var(--border); border-radius: 8px;
  overflow: hidden;
}
.panel-head {
  padding: 8px 11px;
  border-bottom: 1px solid var(--border);
  display: flex; align-items: center; justify-content: space-between; gap: 8px;
  flex-shrink: 0;
}
.panel-title { margin: 0; font-size: 12px; font-weight: 800; }
.hint { color: var(--muted); font-size: 11px; }
iframe { flex: 1; width: 100%; border: 0; background: #fff; display: block; }

/* ── Bottom row ──────────────────────────────────────────────── */
.bottom-row {
  flex-shrink: 0;
  display: flex;
  flex-direction: row;
  overflow: hidden;
  will-change: height;
}
.bottom-row.collapsing { transition: height .2s ease; }
.diag-panel {
  flex-shrink: 0; min-width: 120px;
  display: flex; flex-direction: column;
  background: var(--panel); border: 1px solid var(--border); border-radius: 8px;
  overflow: hidden;
}
.ref-panel {
  flex: 1; min-width: 120px;
  display: flex; flex-direction: column;
  background: var(--panel); border: 1px solid var(--border); border-radius: 8px;
  overflow: hidden;
}
.panel-body { flex: 1; overflow-y: auto; padding: 8px 11px; }
/* Diagnostics */
.diag-list { list-style: none; margin: 0; padding: 0; display: grid; gap: 3px; }
.diag-list li {
  display: flex; gap: 6px; align-items: baseline;
  font: 11px/1.4 "SF Mono", Menlo, Consolas, monospace;
  padding: 3px 5px; border-radius: 4px;
}
.d-ok   { color: var(--ok); }
.d-warn { color: var(--warn); background: #fffbeb; }
.d-err  { color: var(--danger); background: #fef2f2; }
.d-idle { color: var(--muted); font-style: italic; font-size: 12px; padding: 4px 0; }
.hint.ok  { color: var(--ok); }
.hint.err { color: var(--danger); }
.hint.wrn { color: var(--warn); }
/* Reference */
.ref-sec { margin-bottom: 12px; }
.ref-sec:last-child { margin-bottom: 0; }
.ref-sec-label { font-size: 10px; font-weight: 800; text-transform: uppercase; letter-spacing: .07em; color: var(--muted); margin: 0 0 4px; }
.ref-table { border-collapse: collapse; width: 100%; font-size: 11px; }
.ref-table th { text-align: left; padding: 3px 6px; border-bottom: 1px solid var(--border); font-size: 10px; font-weight: 800; text-transform: uppercase; letter-spacing: .05em; color: var(--muted); }
.ref-table td { padding: 3px 6px; border-bottom: 1px solid #ebe8df; vertical-align: top; }
.ref-table tr:last-child td { border-bottom: 0; }
code { font-family: "SF Mono", Menlo, Consolas, monospace; font-size: 11px; background: #f0ede5; padding: 1px 4px; border-radius: 3px; }

/* ── History modal ───────────────────────────────────────────── */
.modal { position: fixed; inset: 0; z-index: 300; display: flex; align-items: center; justify-content: center; }
.modal[hidden] { display: none; }
.modal-bg  { position: absolute; inset: 0; background: rgba(0,0,0,.5); backdrop-filter: blur(2px); }
.modal-box {
  position: relative; z-index: 1;
  background: var(--panel); border: 1px solid var(--border); border-radius: 10px;
  width: min(600px, 90vw); max-height: 72vh;
  display: flex; flex-direction: column;
  box-shadow: 0 24px 64px rgba(0,0,0,.22);
}
.modal-head {
  padding: 13px 15px; border-bottom: 1px solid var(--border);
  display: flex; align-items: flex-start; justify-content: space-between; gap: 8px;
}
.modal-head h3 { margin: 0; font-size: 14px; font-weight: 800; }
.modal-head .hint { font-size: 11px; margin-top: 2px; }
.modal-body { overflow-y: auto; flex: 1; padding: 8px; }
.x-btn { background: transparent; border: 1px solid transparent; padding: 2px 7px; font-size: 16px; line-height: 1; color: var(--muted); border-radius: 4px; flex-shrink: 0; }
.x-btn:hover { background: #f0ede5; color: var(--ink); }
.hist-list { list-style: none; margin: 0; padding: 0; display: grid; gap: 6px; }
.hist-item { border: 1px solid var(--border); border-radius: 6px; padding: 9px 11px; display: grid; gap: 4px; }
.hist-top { display: flex; justify-content: space-between; align-items: center; gap: 8px; }
.hist-ts { font-size: 12px; font-weight: 700; }
.hist-latest { font-size: 10px; background: #e7f1ef; color: #0d5d57; padding: 1px 6px; border-radius: 999px; font-weight: 700; }
.hist-stats { font-size: 11px; color: var(--muted); }
.hist-pre { font: 11px/1.5 "SF Mono", Menlo, Consolas, monospace; color: #4a6070; background: #f5f2ea; padding: 5px 7px; border-radius: 4px; white-space: pre-wrap; word-break: break-all; max-height: 48px; overflow: hidden; }
.hist-empty { text-align: center; color: var(--muted); padding: 24px; font-size: 13px; }

/* ── Responsive ──────────────────────────────────────────────── */
@media (max-width: 860px) {
  .work-row { flex-direction: column; }
  .editor-panel { flex-basis: auto !important; flex-shrink: 0; height: 55%; }
  .preview-panel { flex: 1; }
  #rh-editor.col { display: none; }
}
</style>
</head>
<body>
<div class="app">

  <!-- ── Header ─────────────────────────────────────────────── -->
  <header>
    <button class="btn dark" id="sidebar-toggle" title="Toggle sidebar (S)">&#9776;</button>
    <span class="logo">JTML<sub>studio</sub></span>
    <span class="tagline">Edit · Run · Lint · Format · Export · Preview · Tutorial</span>
    <div class="h-actions">
      <span class="kb"><kbd>Cmd</kbd>/<kbd>Ctrl</kbd>+<kbd>Enter</kbd> run</span>
      <button class="btn primary" id="run">Run</button>
      <button class="btn dark" id="lint">Lint</button>
      <button class="btn dark" id="format">Format</button>
      <button class="btn dark" id="export">Export</button>
      <button class="btn dark" id="save">Save</button>
      <button class="btn dark" id="reset">Reset</button>
      <button class="btn dark" id="bottom-toggle" title="Toggle diagnostics panel (D)">&#x229F;</button>
    </div>
  </header>

  <!-- ── Main layout ────────────────────────────────────────── -->
  <div class="main-layout" id="main-layout">

    <!-- Sidebar -->
    <aside class="sidebar" id="sidebar">
      <div class="sidebar-head">
        <h2 class="sidebar-title">Explorer</h2>
      </div>
      <div class="sidebar-body">
        <div class="sb-section">
          <span class="sb-label">Files</span>
          <div id="file-list"></div>
        </div>
        <div class="sb-section">
          <span class="sb-label">Tutorial</span>
          <div id="lesson-list"><p class="no-items">Loading…</p></div>
        </div>
      </div>
    </aside>

    <!-- Sidebar resize handle -->
    <div class="rh col" id="rh-sidebar" title="Drag to resize · Double-click to reset"></div>

    <!-- Content column -->
    <div class="content" id="content">

      <!-- Prose panel (lesson mode) -->
      <div class="prose-panel" id="prose-panel" style="height:0;overflow:hidden">
        <div class="prose-body" id="prose-body"></div>
        <div class="prose-nav">
          <button id="prev" disabled>&#8592; Prev</button>
          <span class="lesson-ctr" id="lesson-ctr"></span>
          <button id="next">Next &#8594;</button>
        </div>
      </div>

      <!-- Prose resize handle (hidden until lesson shown) -->
      <div class="rh row" id="rh-prose" style="display:none" title="Drag to resize prose"></div>

      <!-- Editor + Preview -->
      <div class="work-row" id="work-row">

        <section class="editor-panel" id="editor-panel">
          <div class="editor-head">
            <h2 class="editor-name" id="active-name">counter.jtml</h2>
            <div class="editor-right">
              <span class="editor-meta" id="editor-meta"></span>
              <button class="history-btn" id="history-btn">History</button>
            </div>
          </div>
          <div class="editor-body" id="editor-body"></div>
          <div class="editor-foot">
            <span id="status">ready</span>
            <span id="cursor-pos">Ln 1, Col 1</span>
            <span id="issue-count"></span>
          </div>
        </section>

        <!-- Editor/Preview resize handle -->
        <div class="rh col" id="rh-editor" title="Drag to resize · Double-click to reset"></div>

        <section class="preview-panel">
          <div class="panel-head">
            <h2 class="panel-title">Preview</h2>
            <span class="hint">live</span>
          </div>
          <iframe id="preview" title="JTML preview"
            sandbox="allow-scripts allow-same-origin allow-forms"></iframe>
        </section>

      </div><!-- .work-row -->

      <!-- Bottom resize handle -->
      <div class="rh row" id="rh-bottom" title="Drag to resize · Double-click to reset"></div>

      <!-- Bottom row -->
      <div class="bottom-row" id="bottom-row">

        <section class="diag-panel" id="diag-panel">
          <div class="panel-head">
            <h2 class="panel-title">Diagnostics</h2>
            <span class="hint" id="diag-sum">parser + linter</span>
          </div>
          <div class="panel-body">
            <ul class="diag-list" id="diag-list">
              <li class="d-idle">No diagnostics yet. Press Run.</li>
            </ul>
          </div>
        </section>

        <!-- Diag/Ref resize handle -->
        <div class="rh col" id="rh-diag" title="Drag to resize · Double-click to reset"></div>

        <section class="ref-panel">
          <div class="panel-head"><h2 class="panel-title">Reference</h2></div>
          <div class="panel-body">
            <div class="ref-sec">
              <p class="ref-sec-label">State &amp; values</p>
              <table class="ref-table"><thead><tr><th>Keyword</th><th>Syntax</th><th>Notes</th></tr></thead><tbody>
                <tr><td><code>define</code></td><td><code>define x = expr</code></td><td>Reactive mutable var</td></tr>
                <tr><td><code>const</code></td><td><code>const x = expr</code></td><td>Immutable</td></tr>
                <tr><td><code>derive</code></td><td><code>derive x = expr</code></td><td>Auto-computed</td></tr>
                <tr><td><code>show</code></td><td><code>show expr</code></td><td>Render text</td></tr>
                <tr><td><code>?:</code></td><td><code>ok ? a : b</code></td><td>Conditional value</td></tr>
                <tr><td><code>+=</code></td><td><code>count += 1</code></td><td>Compound update</td></tr>
                <tr><td><code>object</code></td><td><code>{ "key": value }</code></td><td>Dictionary literal</td></tr>
                <tr><td><code>array</code></td><td><code>[a, b, c]</code></td><td>List literal</td></tr>
              </tbody></table>
            </div>
            <div class="ref-sec">
              <p class="ref-sec-label">Elements</p>
              <table class="ref-table"><tbody>
                <tr><td><code>element tag [attrs] … #</code></td><td>Classic block</td></tr>
                <tr><td><code>@tag [attrs] … #</code></td><td>Short form</td></tr>
                <tr><td><code>\\</code></td><td>Statement end</td></tr>
                <tr><td><code>#</code></td><td>Block close</td></tr>
              </tbody></table>
            </div>
            <div class="ref-sec">
              <p class="ref-sec-label">Flow</p>
              <table class="ref-table"><tbody>
                <tr><td><code>if (expr)\\</code></td><td>Conditional block</td></tr>
                <tr><td><code>else \\</code></td><td>Fallback block after <code>if</code></td></tr>
                <tr><td><code>for (x in list)\\</code></td><td>Iterate values</td></tr>
                <tr><td><code>while (expr)\\</code></td><td>Loop while true</td></tr>
                <tr><td><code>break</code> / <code>continue</code></td><td>Loop control</td></tr>
              </tbody></table>
            </div>
            <div class="ref-sec">
              <p class="ref-sec-label">Functions &amp; modules</p>
              <table class="ref-table"><tbody>
                <tr><td><code>function save(value)\\</code></td><td>Define event handler or helper</td></tr>
                <tr><td><code>return expr</code></td><td>Return from function</td></tr>
                <tr><td><code>throw expr</code></td><td>Raise an error</td></tr>
                <tr><td><code>try \\ ... except(error)\\</code></td><td>Handle errors</td></tr>
                <tr><td><code>import name from "file"</code></td><td>Classic import form</td></tr>
              </tbody></table>
            </div>
            <div class="ref-sec">
              <p class="ref-sec-label">Common attrs</p>
              <table class="ref-table"><tbody>
                <tr><td><code>style="…css…"</code></td><td>Inline CSS styles</td></tr>
                <tr><td><code>class="name"</code></td><td>CSS class</td></tr>
                <tr><td><code>id="name"</code></td><td>Element id</td></tr>
                <tr><td><code>type placeholder value</code></td><td>Input attributes</td></tr>
                <tr><td><code>disabled required</code></td><td>Boolean attributes</td></tr>
              </tbody></table>
            </div>
            <div class="ref-sec">
              <p class="ref-sec-label">Events</p>
              <table class="ref-table"><tbody>
                <tr><td><code>onClick=fn()</code></td><td>—</td></tr>
                <tr><td><code>onInput=fn()</code></td><td><code>value</code> (string)</td></tr>
                <tr><td><code>onSubmit=fn()</code></td><td>Form submit</td></tr>
                <tr><td><code>onChange=fn()</code></td><td>Inputs/selects</td></tr>
                <tr><td><code>onMouseOver=fn()</code></td><td>—</td></tr>
                <tr><td><code>onScroll=fn()</code></td><td><code>scrollTop</code></td></tr>
              </tbody></table>
            </div>
            <div class="ref-sec">
              <p class="ref-sec-label">Friendly basics (jtml 2)</p>
              <table class="ref-table"><tbody>
                <tr><td><code>jtml 2</code></td><td>Enable friendly (indentation-based) syntax</td></tr>
                <tr><td><code>let x = expr</code></td><td>Reactive mutable variable</td></tr>
                <tr><td><code>get x = expr</code></td><td>Derived (computed) value — updates automatically</td></tr>
                <tr><td><code>const x = expr</code></td><td>Immutable constant</td></tr>
                <tr><td><code>when actionName</code></td><td>Named action handler (function body below)</td></tr>
                <tr><td><code>page</code></td><td>Root <code>&lt;main&gt;</code> element</td></tr>
                <tr><td><code>show "text {expr}"</code></td><td>Output text with interpolation</td></tr>
                <tr><td><code>h1 "Title"</code></td><td>Inline-text element shorthand</td></tr>
                <tr><td><code>button "Save" click save</code></td><td>Button wired to action</td></tr>
                <tr><td><code>input placeholder "Email" into email</code></td><td>Two-way bound input</td></tr>
              </tbody></table>
            </div>
            <div class="ref-sec">
              <p class="ref-sec-label">Friendly elements</p>
              <table class="ref-table"><tbody>
                <tr><td><code>box</code></td><td><code>&lt;div&gt;</code></td></tr>
                <tr><td><code>text</code></td><td><code>&lt;p&gt;</code></td></tr>
                <tr><td><code>link "L" to "/path"</code></td><td><code>&lt;a href="#/path"&gt;</code></td></tr>
                <tr><td><code>image src "url" alt "A"</code></td><td><code>&lt;img&gt;</code></td></tr>
                <tr><td><code>checkbox "Label" into flag</code></td><td><code>&lt;input type=checkbox&gt;</code></td></tr>
                <tr><td><code>list</code> / <code>list ordered</code></td><td><code>&lt;ul&gt;</code> / <code>&lt;ol&gt;</code></td></tr>
                <tr><td><code>item</code></td><td><code>&lt;li&gt;</code></td></tr>
              </tbody></table>
            </div>
            <div class="ref-sec">
              <p class="ref-sec-label">Async fetch</p>
              <table class="ref-table"><tbody>
                <tr><td><code>let data = fetch "/api/url"</code></td><td>Browser-side GET; creates <code>data.loading</code>, <code>data.data</code>, <code>data.error</code></td></tr>
                <tr><td><code>fetch … method "POST" body {k:v}</code></td><td>POST with JSON body</td></tr>
                <tr><td><code>fetch … refresh reload</code></td><td>Wire a <code>reload</code> action to re-trigger the fetch</td></tr>
                <tr><td><code>if data.loading</code></td><td>Show loading state</td></tr>
                <tr><td><code>for item in data.data</code></td><td>Iterate fetched array</td></tr>
                <tr><td><code>if data.error</code></td><td>Show error message</td></tr>
              </tbody></table>
            </div>
            <div class="ref-sec">
              <p class="ref-sec-label">Store (shared state)</p>
              <table class="ref-table"><tbody>
                <tr><td><code>store name</code></td><td>Declare a shared state namespace</td></tr>
                <tr><td><code>&nbsp;&nbsp;let field = value</code></td><td>Store field</td></tr>
                <tr><td><code>&nbsp;&nbsp;when action</code></td><td>Store action handler</td></tr>
                <tr><td><code>show store.field</code></td><td>Read a store field</td></tr>
                <tr><td><code>button "X" click store.action</code></td><td>Call a store action</td></tr>
              </tbody></table>
            </div>
            <div class="ref-sec">
              <p class="ref-sec-label">Effects</p>
              <table class="ref-table"><tbody>
                <tr><td><code>effect variable</code></td><td>Run body when <code>variable</code> changes</td></tr>
                <tr><td><code>&nbsp;&nbsp;let msg = "Updated: {variable}"</code></td><td>Update state in response</td></tr>
              </tbody></table>
            </div>
            <div class="ref-sec">
              <p class="ref-sec-label">Scoped styles</p>
              <table class="ref-table"><tbody>
                <tr><td><code>style</code></td><td>Open a scoped CSS block (indented selectors and declarations)</td></tr>
                <tr><td><code>&nbsp;&nbsp;h1</code></td><td>Selector block — scoped to <code>[data-jtml-app]</code></td></tr>
                <tr><td><code>&nbsp;&nbsp;&nbsp;&nbsp;color: teal</code></td><td>CSS declaration (semicolons optional)</td></tr>
                <tr><td><code>&nbsp;&nbsp;body</code> / <code>:root</code></td><td>Maps to <code>[data-jtml-app]</code></td></tr>
              </tbody></table>
            </div>
            <div class="ref-sec">
              <p class="ref-sec-label">Routes (SPA)</p>
              <table class="ref-table"><tbody>
                <tr><td><code>route "/path" as Component</code></td><td>Hash-based route; shows <code>Component</code> at <code>#/path</code></td></tr>
                <tr><td><code>route "/user/:id" as Profile</code></td><td>Route with <code>:param</code> captured as variable</td></tr>
                <tr><td><code>route "*" as NotFound</code></td><td>Wildcard fallback route</td></tr>
                <tr><td><code>link "Label" to "/path"</code></td><td>Navigation link — lowers to <code>href="#/path"</code></td></tr>
                <tr><td><code>redirect "/path"</code></td><td>Programmatic navigation from an action</td></tr>
              </tbody></table>
            </div>
            <div class="ref-sec">
              <p class="ref-sec-label">Components</p>
              <table class="ref-table"><tbody>
                <tr><td><code>make Name param1 param2</code></td><td>Define a reusable component</td></tr>
                <tr><td><code>&nbsp;&nbsp;slot</code></td><td>Inject caller content here</td></tr>
                <tr><td><code>Name "arg1" "arg2"</code></td><td>Call a component — each instance gets isolated state</td></tr>
                <tr><td><code>Name "arg"</code><br>&nbsp;&nbsp;<code>p "child"</code></td><td>Call with slot content (indented below)</td></tr>
              </tbody></table>
            </div>
            <div class="ref-sec">
              <p class="ref-sec-label">Studio tools</p>
              <table class="ref-table"><tbody>
                <tr><td><code>Run</code> <kbd>⌘↩</kbd></td><td>Compile, lint, and preview in the right pane</td></tr>
                <tr><td><code>Lint</code></td><td>Parser + linter diagnostics only (no preview)</td></tr>
                <tr><td><code>Format</code></td><td>Rewrite source with canonical formatter</td></tr>
                <tr><td><code>Export</code></td><td>Download the compiled HTML file</td></tr>
                <tr><td><code>Save</code></td><td>Versioned local snapshot (up to 50 per file)</td></tr>
                <tr><td><kbd>⌘B</kbd></td><td>Toggle sidebar</td></tr>
                <tr><td><kbd>⌘J</kbd></td><td>Toggle diagnostics panel</td></tr>
              </tbody></table>
            </div>
          </div>
        </section>

      </div><!-- .bottom-row -->
    </div><!-- .content -->
  </div><!-- .main-layout -->
</div><!-- .app -->

<!-- History modal -->
<div id="history-modal" class="modal" hidden>
  <div class="modal-bg" id="hist-bg"></div>
  <div class="modal-box">
    <div class="modal-head">
      <div>
        <h3 id="hist-title">Version history</h3>
        <span class="hint" id="hist-count"></span>
      </div>
      <button class="x-btn" id="hist-close">&#x2715;</button>
    </div>
    <div class="modal-body">
      <ol class="hist-list" id="hist-list"></ol>
    </div>
  </div>
</div>

<script src="https://cdn.jsdelivr.net/npm/marked/marked.min.js"></script>
<script src="https://cdn.jsdelivr.net/npm/codemirror@5.65.16/lib/codemirror.min.js"></script>
<script src="https://cdn.jsdelivr.net/npm/codemirror@5.65.16/addon/mode/simple.min.js"></script>
<script src="https://cdn.jsdelivr.net/npm/codemirror@5.65.16/addon/edit/matchbrackets.min.js"></script>
<script src="https://cdn.jsdelivr.net/npm/codemirror@5.65.16/addon/selection/active-line.min.js"></script>
<script>
/* ═══════════════════════════════════════════════════════════
   JTML CodeMirror mode
   State machine for accurate @ + element tag highlighting
═══════════════════════════════════════════════════════════ */
CodeMirror.defineSimpleMode("jtml", {
  start: [
    /* Strings */
    { regex: /"(?:[^\\"]|\\.)*"/, token: "string" },
    { regex: /'(?:[^\\']|\\.)*'/, token: "string" },
    /* Comments */
    { regex: /\/\/.*$/, token: "comment" },
    /* Statement terminator \\ and block-close # */
    { regex: /\\\\/, token: "jtml-term" },
    { regex: /(?:^|(?<=\s))(#)(?=\s|$)/, token: "jtml-close" },
    /* @tag — push state to highlight the tag name */
    { regex: /@/, token: "jtml-el", push: "atag" },
    /* element keyword — push state to highlight the tag name */
    { regex: /\belement\b/, token: "jtml-el", push: "etag" },
    /* function keyword — push state to highlight the fn name */
    { regex: /\bfunction\b/, token: "jtml-kw", push: "fname" },
    /* Event attributes */
    { regex: /\b(?:onClick|onInput|onMouseOver|onScroll|onChange|onFocus|onBlur|onKeyDown|onKeyUp|onKeyPress|onSubmit|onDblClick)\b/, token: "jtml-event" },
    /* HTML-style attributes */
    { regex: /\b(?:style|class|id|type|href|src|placeholder|value|disabled|required|readonly|checked|selected|name|method|action|target|rel|alt|title|role|width|height|min|max|step|pattern|tabindex|autocomplete|autofocus|multiple|accept|enctype|for|data-jtml-fetch|data-url|data-method)\b/, token: "jtml-attr" },
    /* JTML keywords */
    { regex: /\b(?:define|const|derive|show|if|else|while|for|in|break|continue|try|except|then|return|throw|async|subscribe|unsubscribe|to|from|store|unbind|object|derives|import|main|jtml|let|get|when|make|page|route|slot|fetch|catch|finally|use|effect|redirect|refresh|into|link|text|box|image|item|list|ordered)\b/, token: "jtml-kw" },
    /* Literals */
    { regex: /\b(?:true|false|null)\b/, token: "atom" },
    { regex: /\b\d+(?:\.\d+)?\b/, token: "number" },
    /* Operators */
    { regex: /[+\-*/%^=<>!&|?:]+/, token: "operator" },
    /* Variables */
    { regex: /[A-Za-z_][A-Za-z0-9_]*/, token: "variable" },
  ],
  /* After @, highlight one word as tag name then pop */
  atag: [
    { regex: /[A-Za-z_][A-Za-z0-9_\-]*/, token: "jtml-tag", pop: true },
    { regex: /.*/, token: null, pop: true },
  ],
  /* After element, skip whitespace then highlight tag name */
  etag: [
    { regex: /\s+/, token: null },
    { regex: /[A-Za-z_][A-Za-z0-9_\-]*/, token: "jtml-tag", pop: true },
    { regex: /.*/, token: null, pop: true },
  ],
  /* After function, skip whitespace then highlight fn name */
  fname: [
    { regex: /\s+/, token: null },
    { regex: /[A-Za-z_][A-Za-z0-9_]*/, token: "def", pop: true },
    { regex: /.*/, token: null, pop: true },
  ],
  meta: { lineComment: "//" }
});

/* ═══════════════════════════════════════════════════════════
   Built-in file samples
═══════════════════════════════════════════════════════════ */
const SAMPLES = [
  {
    name: "counter.jtml",
    label: "Counter",
    category: "basics",
    code: `jtml 2

let count = 0
get doubled = count * 2

when increment
  count += 1

when decrement
  if count > 0
    count -= 1

when reset
  count = 0

style
  main
    font-family: system-ui
    max-width: 560px
    margin: 48px auto
    padding: 0 20px
    display: grid
    gap: 16px
  h1
    margin: 0
    font-size: 28px
  .stats
    color: #64707a
    font-size: 15px
  .controls
    display: flex
    gap: 8px
  button
    padding: 10px 18px
    border-radius: 6px
    font-size: 15px
    cursor: pointer
    border: 1px solid #d8d4c8
  .primary
    background: #0f766e
    color: white
    border-color: transparent

page
  h1 "Counter"
  p class "stats" "Count: {count} — doubled: {doubled}"
  div class "controls"
    button "+" class "primary" click increment
    button "−" click decrement
    button "Reset" click reset`
  },
  {
    name: "form.jtml",
    label: "Form",
    category: "basics",
    code: `jtml 2

let email = ""
let submitted = false
get label = submitted ? "Subscribed! Check {email}." : "Enter your email to subscribe."

when submit
  if email != ""
    let submitted = true

style
  main
    font-family: system-ui
    max-width: 420px
    margin: 48px auto
    padding: 32px
    background: white
    border-radius: 12px
    box-shadow: 0 4px 24px rgba(0,0,0,.08)
    display: grid
    gap: 14px
  h1
    margin: 0
    font-size: 22px
  p
    margin: 0
    color: #64707a
    font-size: 14px
  input
    padding: 10px 12px
    border: 1px solid #d8d4c8
    border-radius: 6px
    font-size: 14px
    width: 100%
    box-sizing: border-box
  button
    padding: 11px 20px
    background: #0f766e
    color: white
    border: none
    border-radius: 6px
    font-size: 14px
    cursor: pointer
    font-weight: 600

page
  h1 "Newsletter"
  p label
  input type "email" placeholder "you@example.com" into email
  button "Subscribe" click submit`
  },
  {
    name: "dashboard.jtml",
    label: "Dashboard",
    category: "basics",
    code: `jtml 2

let revenue = 12400
let customers = 320
get avg = revenue / customers

when addCustomer
  let customers = customers + 1
  let revenue = revenue + avg

style
  main
    font-family: system-ui
    max-width: 800px
    margin: 40px auto
    padding: 0 16px
    display: grid
    gap: 20px
  h1
    margin: 0
  .cards
    display: grid
    grid-template-columns: repeat(3, 1fr)
    gap: 14px
  article
    background: white
    border: 1px solid #e8e4dc
    border-radius: 10px
    padding: 18px 20px
    display: grid
    gap: 6px
  .label
    font-size: 12px
    color: #64707a
    text-transform: uppercase
    letter-spacing: .05em
    margin: 0
  .value
    font-size: 28px
    font-weight: 700
    margin: 0
  button
    justify-self: start
    padding: 10px 18px
    background: #0f766e
    color: white
    border: none
    border-radius: 7px
    font-size: 14px
    cursor: pointer
    font-weight: 600

page
  h1 "Dashboard"
  div class "cards"
    article
      p class "label" "Revenue"
      p class "value" "{revenue}"
    article
      p class "label" "Customers"
      p class "value" "{customers}"
    article
      p class "label" "Avg / customer"
      p class "value" "{avg}"
  button "+ Add customer" click addCustomer`
  },
  {
    name: "fetch.jtml",
    label: "Fetch",
    category: "data",
    code: `jtml 2

// Fetches /api/users (or any JSON endpoint).
// Swap the URL for a real endpoint in your project.
let users = fetch "/api/users" refresh reload

style
  main
    font-family: system-ui
    max-width: 560px
    margin: 48px auto
    padding: 0 20px
    display: grid
    gap: 12px
  h1
    margin: 0
  .user-card
    padding: 14px 16px
    background: white
    border: 1px solid #e8e4dc
    border-radius: 8px
  .loading
    color: #64707a
  .error
    color: #b42318
  button
    justify-self: start
    padding: 8px 14px
    border-radius: 6px
    font-size: 13px
    cursor: pointer

page
  h1 "Users"
  button "Reload" click reload
  if users.loading
    p class "loading" "Loading…"
  else
    for user in users.data
      div class "user-card"
        strong "{user.name}"
  if users.error
    p class "error" "Error: {users.error}"`
  },
  {
    name: "store.jtml",
    label: "Store",
    category: "data",
    code: `jtml 2

store auth
  let user = "Ada"
  let loggedIn = true

  when login
    let user = "Ada"
    let loggedIn = true

  when logout
    let user = ""
    let loggedIn = false

style
  main
    font-family: system-ui
    max-width: 420px
    margin: 48px auto
    padding: 0 20px
    display: grid
    gap: 14px
  h1
    margin: 0
  .badge
    padding: 8px 12px
    background: #ecfdf5
    border: 1px solid #6ee7b7
    border-radius: 6px
    font-size: 14px
    color: #065f46
  button
    justify-self: start
    padding: 9px 16px
    border-radius: 6px
    cursor: pointer
    font-size: 14px

page
  h1 "Auth Store"
  if auth.loggedIn
    p class "badge" "Signed in as {auth.user}"
    button "Logout" click auth.logout
  else
    p "You are signed out."
    button "Login" click auth.login`
  },
  {
    name: "effects.jtml",
    label: "Effects",
    category: "data",
    code: `jtml 2

let count = 0
let last = "No changes yet."
let history = []

effect count
  let last = "Count changed to {count}"

when increment
  count += 1

when decrement
  count -= 1

style
  main
    font-family: system-ui
    max-width: 480px
    margin: 48px auto
    padding: 0 20px
    display: grid
    gap: 12px
  h1
    margin: 0
  .log
    padding: 12px 16px
    background: #f0fdf4
    border: 1px solid #86efac
    border-radius: 8px
    font-size: 14px
    color: #166534
  .controls
    display: flex
    gap: 8px
  button
    padding: 10px 16px
    border-radius: 6px
    cursor: pointer
    font-size: 14px

page
  h1 "Effects"
  p "Count: {count}"
  p class "log" "Last: {last}"
  div class "controls"
    button "+ Increment" click increment
    button "− Decrement" click decrement`
  },
  {
    name: "routes.jtml",
    label: "Routes",
    category: "navigation",
    code: `jtml 2

route "/" as Home
route "/about" as About
route "/user/:id" as UserProfile
route "*" as NotFound

make Home
  style
    main
      font-family: system-ui
      max-width: 600px
      margin: 48px auto
      padding: 0 20px
      display: grid
      gap: 12px
    nav
      display: flex
      gap: 10px
    a
      color: #0f766e
  page
    h1 "Home"
    nav
      link "About" to "/about"
      link "Ada's profile" to "/user/ada"
    p "Choose a page above."

make About
  page style "font-family: system-ui; max-width: 600px; margin: 48px auto; padding: 0 20px; display: grid; gap: 12px"
    h1 "About"
    p "JTML is a local-first reactive HTML language."
    link "← Home" to "/"

make UserProfile id
  page style "font-family: system-ui; max-width: 600px; margin: 48px auto; padding: 0 20px; display: grid; gap: 12px"
    h1 "User Profile"
    p "Viewing: {id}"
    link "← Home" to "/"

make NotFound
  page style "font-family: system-ui; max-width: 600px; margin: 48px auto; padding: 0 20px; display: grid; gap: 12px"
    h1 "404 — Not Found"
    link "← Home" to "/"`
  },
  {
    name: "components.jtml",
    label: "Components",
    category: "composition",
    code: `jtml 2

make Button label action
  button label click action

make Card title
  div style "border: 1px solid #e8e4dc; border-radius: 10px; padding: 18px; display: grid; gap: 10px; background: white"
    h3 style "margin: 0; font-size: 16px" title
    slot

let count = 0

when inc
  count += 1

when dec
  if count > 0
    count -= 1

style
  main
    font-family: system-ui
    max-width: 480px
    margin: 48px auto
    padding: 0 20px
    display: grid
    gap: 16px
  h1
    margin: 0
  .row
    display: flex
    gap: 8px
  button
    padding: 9px 16px
    border-radius: 6px
    font-size: 14px
    cursor: pointer

page
  h1 "Component Isolation"
  Card "Counter A"
    p "Count: {count}"
    div class "row"
      Button "+ Increment" inc
      Button "− Decrement" dec`
  }
];

/* ═══════════════════════════════════════════════════════════
   App state
═══════════════════════════════════════════════════════════ */
const $ = id => document.getElementById(id);
let mode            = "file";
let activeFileIdx   = 0;
let activeLessonIdx = -1;
let lessons         = [];
let completedSlugs  = new Set(JSON.parse(localStorage.getItem("jtml:completed") || "[]"));
let dirty           = false;
let loading         = false;
let proseOpen       = false;
let sidebarOpen     = true;
let bottomOpen      = true;

/* ═══════════════════════════════════════════════════════════
   Layout system
═══════════════════════════════════════════════════════════ */
const PORT = location.port || "80";
const LAYOUT_KEY = "jtml:layout:" + PORT;
const DEF_LAYOUT = { sidebarW: 210, editorW: null, bottomH: 185, diagW: null, proseH: 200 };
let L = Object.assign({}, DEF_LAYOUT, JSON.parse(localStorage.getItem(LAYOUT_KEY) || "{}"));

function clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)); }

function applyLayout() {
  /* Sidebar */
  const sidebar = $("sidebar");
  sidebar.style.width = sidebarOpen ? L.sidebarW + "px" : "0";
  sidebar.style.minWidth = sidebarOpen ? "0" : "0";
  $("rh-sidebar").style.visibility = sidebarOpen ? "" : "hidden";
  $("sidebar-toggle").classList.toggle("active", !sidebarOpen);

  /* Editor width inside work-row */
  const workRow = $("work-row");
  if (workRow) {
    const totalW = workRow.getBoundingClientRect().width;
    if (totalW > 0) {
      const rhW   = 6;
      const avail = totalW - rhW;
      const eW    = L.editorW != null ? clamp(L.editorW, 160, avail - 160) : Math.round(avail * 0.56);
      $("editor-panel").style.flexBasis = eW + "px";
    }
  }

  /* Bottom row height */
  const bottomRow = $("bottom-row");
  bottomRow.style.height = bottomOpen ? L.bottomH + "px" : "0";
  $("rh-bottom").style.visibility = bottomOpen ? "" : "hidden";
  $("bottom-toggle").classList.toggle("active", !bottomOpen);
  $("bottom-toggle").innerHTML = bottomOpen ? "&#x229F;" : "&#x229E;";

  /* Diag width inside bottom-row */
  if (bottomOpen) {
    const bRect = bottomRow.getBoundingClientRect();
    if (bRect.width > 0) {
      const avail = bRect.width - 6;
      const dW = L.diagW != null ? clamp(L.diagW, 120, avail - 120) : Math.round(avail * 0.56);
      $("diag-panel").style.flexBasis = dW + "px";
    }
  }

  /* Prose height */
  $("prose-panel").style.height = proseOpen ? L.proseH + "px" : "0";
  $("rh-prose").style.display   = proseOpen ? "" : "none";

  /* Refresh CodeMirror */
  if (typeof editor !== "undefined") requestAnimationFrame(() => editor.refresh());
}

function saveLayout() { localStorage.setItem(LAYOUT_KEY, JSON.stringify(L)); }

/* Make a resize handle draggable */
function makeResizable(id, type, onDrag, onDblClick) {
  const handle = $(id);
  if (!handle) return;

  handle.addEventListener("mousedown", e => {
    if (e.button !== 0) return;
    e.preventDefault();
    handle.classList.add("dragging");
    document.body.style.cursor     = type === "col" ? "col-resize" : "row-resize";
    document.body.style.userSelect = "none";
    /* Disable pointer events on iframes so they don't steal the drag */
    document.querySelectorAll("iframe").forEach(f => f.style.pointerEvents = "none");

    const onMove = ev => { onDrag(ev.clientX, ev.clientY); applyLayout(); };
    const onUp   = ()  => {
      handle.classList.remove("dragging");
      document.body.style.cursor = document.body.style.userSelect = "";
      document.querySelectorAll("iframe").forEach(f => f.style.pointerEvents = "");
      document.removeEventListener("mousemove", onMove);
      document.removeEventListener("mouseup",   onUp);
      saveLayout();
    };
    document.addEventListener("mousemove", onMove);
    document.addEventListener("mouseup",   onUp);
  });

  if (onDblClick) handle.addEventListener("dblclick", () => { onDblClick(); applyLayout(); saveLayout(); });
}

/* Sidebar resize */
makeResizable("rh-sidebar", "col", (x) => {
  L.sidebarW = clamp(x - $("main-layout").getBoundingClientRect().left, 150, 400);
}, () => { L.sidebarW = DEF_LAYOUT.sidebarW; });

/* Editor/Preview resize */
makeResizable("rh-editor", "col", (x) => {
  L.editorW = clamp(x - $("work-row").getBoundingClientRect().left, 160, $("work-row").getBoundingClientRect().width - 160);
}, () => { L.editorW = null; });

/* Prose panel resize */
makeResizable("rh-prose", "row", (x, y) => {
  L.proseH = clamp(y - $("content").getBoundingClientRect().top, 80, 400);
  $("prose-panel").style.height = L.proseH + "px";
}, () => { L.proseH = DEF_LAYOUT.proseH; });

/* Bottom row resize */
makeResizable("rh-bottom", "row", (x, y) => {
  const cRect = $("content").getBoundingClientRect();
  L.bottomH = clamp(cRect.bottom - y, 80, cRect.height * 0.45 | 0);
}, () => { L.bottomH = DEF_LAYOUT.bottomH; });

/* Diag/Reference resize */
makeResizable("rh-diag", "col", (x) => {
  L.diagW = clamp(x - $("bottom-row").getBoundingClientRect().left, 120, $("bottom-row").getBoundingClientRect().width - 120);
}, () => { L.diagW = null; });

/* Sidebar toggle */
$("sidebar-toggle").onclick = () => {
  sidebarOpen = !sidebarOpen;
  $("sidebar").classList.add("collapsing");
  applyLayout();
  setTimeout(() => $("sidebar").classList.remove("collapsing"), 220);
};

/* Bottom toggle */
$("bottom-toggle").onclick = () => {
  bottomOpen = !bottomOpen;
  $("bottom-row").classList.add("collapsing");
  applyLayout();
  setTimeout(() => $("bottom-row").classList.remove("collapsing"), 220);
};

/* Keyboard shortcuts for panel toggles */
document.addEventListener("keydown", e => {
  if ((e.metaKey || e.ctrlKey) && !e.shiftKey && !e.altKey) {
    if (e.key === "b") { e.preventDefault(); $("sidebar-toggle").click(); }
    if (e.key === "j") { e.preventDefault(); $("bottom-toggle").click(); }
  }
});

window.addEventListener("resize", applyLayout);

/* ═══════════════════════════════════════════════════════════
   CodeMirror init
═══════════════════════════════════════════════════════════ */
const editor = CodeMirror($("editor-body"), {
  mode: "jtml",
  theme: "jtml-dark",
  lineNumbers: true,
  lineWrapping: false,
  indentUnit: 4,
  tabSize: 4,
  matchBrackets: true,
  styleActiveLine: true,
  extraKeys: { "Cmd-Enter": () => run(), "Ctrl-Enter": () => run() },
});
editor.setSize("100%", "100%");
editor.on("change",         () => { if (!loading) { dirty = true; updateMeta(); } });
editor.on("cursorActivity", updateCursor);

/* ═══════════════════════════════════════════════════════════
   Storage helpers
═══════════════════════════════════════════════════════════ */
function latestKey(name)   { return "jtml:latest:"   + PORT + ":" + name; }
function versionsKey(name) { return "jtml:versions:" + PORT + ":" + name; }
function lessonKey(slug)   { return "jtml:lesson:"   + PORT + ":" + slug; }

function legacyLessonLatestKey(slug) { return lessonKey(slug); }
function hashText(text) {
  let h = 2166136261;
  for (let i = 0; i < text.length; i++) {
    h ^= text.charCodeAt(i);
    h = Math.imul(h, 16777619);
  }
  return (h >>> 0).toString(16).padStart(8, "0");
}
function lineCount(text) { return text ? text.split("\n").length : 0; }
function normalizeVersion(v, i) {
  const code = String(v.code || "");
  return {
    version: Number(v.version || 0) || i + 1,
    code,
    savedAt: v.savedAt || new Date().toISOString(),
    hash: v.hash || hashText(code),
    chars: Number(v.chars || code.length),
    lines: Number(v.lines || lineCount(code)),
  };
}
function getVersionsByName(name) {
  try {
    return JSON.parse(localStorage.getItem(versionsKey(name)) || "[]")
      .map(normalizeVersion);
  } catch {
    return [];
  }
}
function getVersions(s) { return getVersionsByName(s.name); }
function pushVersionByName(name, code) {
  let vs = getVersionsByName(name);
  const hash = hashText(code);
  localStorage.setItem(latestKey(name), code);
  if (vs.length && vs[0].hash === hash) {
    return { saved: false, version: vs[0].version, hash };
  }
  const maxVersion = vs.reduce((m, v) => Math.max(m, Number(v.version) || 0), 0);
  vs.unshift({
    version: maxVersion + 1,
    code,
    savedAt: new Date().toISOString(),
    hash,
    chars: code.length,
    lines: lineCount(code),
  });
  if (vs.length > 50) vs = vs.slice(0, 50);
  localStorage.setItem(versionsKey(name), JSON.stringify(vs));
  return { saved: true, version: vs[0].version, hash };
}
function activeDocName() {
  if (mode === "file") return SAMPLES[activeFileIdx].name;
  if (mode === "lesson" && activeLessonIdx >= 0) return "lesson:" + lessons[activeLessonIdx].slug;
  return "untitled";
}
function activeDisplayName() {
  if (mode === "file") return SAMPLES[activeFileIdx].name;
  if (mode === "lesson" && activeLessonIdx >= 0) return lessons[activeLessonIdx].slug + "/code.jtml";
  return "untitled";
}
function loadFileCode(s)  { return localStorage.getItem(latestKey(s.name)) || s.code; }
function pushVersion(s, code) {
  return pushVersionByName(s.name, code);
}
function loadLessonCode(l) {
  return localStorage.getItem(latestKey("lesson:" + l.slug)) ||
         localStorage.getItem(legacyLessonLatestKey(l.slug)) ||
         l.code;
}
function saveLessonCode(l, code) {
  return pushVersionByName("lesson:" + l.slug, code);
}
function markComplete(slug) {
  completedSlugs.add(slug);
  localStorage.setItem("jtml:completed", JSON.stringify([...completedSlugs]));
}

/* ═══════════════════════════════════════════════════════════
   UI helpers
═══════════════════════════════════════════════════════════ */
function esc(t) {
  return String(t).replace(/[&<>"']/g, c => ({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"})[c]);
}
function updateCursor() {
  const c = editor.getCursor();
  $("cursor-pos").textContent = "Ln " + (c.line + 1) + ", Col " + (c.ch + 1);
}
function updateMeta() {
  const text  = editor.getValue();
  const lines = text ? text.split("\n").length : 0;
  $("editor-meta").textContent = lines + "L";
  /* Dirty dot in name */
  const base = mode === "file"
    ? SAMPLES[activeFileIdx].name
    : (lessons[activeLessonIdx] ? lessons[activeLessonIdx].slug + "/code.jtml" : "");
  $("active-name").textContent = base + (dirty ? " ●" : "");
}

function renderSidebar() {
  /* Files grouped by category */
  const fl = $("file-list");
  fl.innerHTML = "";
  const categoryLabels = { basics: "Basics", data: "Data & State", navigation: "Navigation", composition: "Composition" };
  const seen = {};
  SAMPLES.forEach((s, i) => {
    const cat = s.category || "basics";
    if (!seen[cat]) {
      seen[cat] = true;
      const hdr = document.createElement("p");
      hdr.className = "sb-category";
      hdr.textContent = categoryLabels[cat] || cat;
      fl.appendChild(hdr);
    }
    const vs  = getVersions(s);
    const btn = document.createElement("button");
    btn.className = "sb-item" + (mode === "file" && i === activeFileIdx ? " active-file" : "");
    const label = s.label || s.name;
    btn.innerHTML = `<span>${esc(label)}</span>${vs.length ? `<span class="v-badge">v${vs.length}</span>` : ""}`;
    btn.onclick = () => selectFile(i);
    fl.appendChild(btn);
  });
  /* Lessons */
  const ll = $("lesson-list");
  if (!lessons.length) {
    ll.innerHTML = '<p class="no-items">No tutorial/ found.<br>Run from repo root.</p>';
    return;
  }
  ll.innerHTML = "";
  lessons.forEach((l, i) => {
    const vs = getVersionsByName("lesson:" + l.slug);
    const btn = document.createElement("button");
    btn.className = "sb-item" + (mode === "lesson" && i === activeLessonIdx ? " active-lesson" : "");
    const chk = completedSlugs.has(l.slug) ? '<span class="check-ok">&#x2713;</span>' : "";
    const badge = vs.length ? `<span class="v-badge">v${vs[0].version}</span>` : "";
    btn.innerHTML = `<span>${esc((i + 1) + ". " + l.title)}</span>${badge}${chk}`;
    btn.onclick = () => selectLesson(i);
    ll.appendChild(btn);
  });
}

/* ═══════════════════════════════════════════════════════════
   Navigation
═══════════════════════════════════════════════════════════ */
function setCode(code) {
  loading = true;
  editor.setValue(code);
  editor.clearHistory();
  loading = false;
}

function openProse() {
  proseOpen = true;
  $("prose-panel").classList.add("collapsing");
  applyLayout();
  setTimeout(() => $("prose-panel").classList.remove("collapsing"), 220);
}
function closeProse() {
  proseOpen = false;
  $("prose-panel").classList.add("collapsing");
  applyLayout();
  setTimeout(() => $("prose-panel").classList.remove("collapsing"), 220);
}

function selectFile(i) {
  mode = "file";
  activeFileIdx = i;
  $("history-btn").style.display = "";
  closeProse();
  setCode(loadFileCode(SAMPLES[i]));
  dirty = false;
  updateMeta();
  renderSidebar();
  run();
}

async function selectLesson(i) {
  if (i < 0 || i >= lessons.length) return;
  mode = "lesson";
  activeLessonIdx = i;
  $("history-btn").style.display = "";

  const l = lessons[i];
  try {
    const res  = await fetch("/api/lesson/" + encodeURIComponent(l.slug));
    const data = await res.json();
    l.prose = data.prose || "";
    l.code  = data.code  || "";
  } catch {}

  $("prose-body").innerHTML = typeof marked !== "undefined"
    ? marked.parse(l.prose || "")
    : "<p>" + esc(l.prose || "") + "</p>";
  $("prev").disabled = i === 0;
  $("next").disabled = i === lessons.length - 1;
  $("lesson-ctr").textContent = "Lesson " + (i + 1) + " of " + lessons.length;
  openProse();

  setCode(loadLessonCode(l));
  dirty = false;
  updateMeta();
  renderSidebar();
  run();
}

$("prev").onclick = () => selectLesson(activeLessonIdx - 1);
$("next").onclick = () => {
  markComplete(lessons[activeLessonIdx].slug);
  renderSidebar();
  selectLesson(activeLessonIdx + 1);
};

/* ═══════════════════════════════════════════════════════════
   API
═══════════════════════════════════════════════════════════ */
async function api(path, body) {
  const res = await fetch(path, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  });
  return res.json();
}

/* ═══════════════════════════════════════════════════════════
   Diagnostics
═══════════════════════════════════════════════════════════ */
function showDiagnostics(data) {
  const ds      = data.diagnostics || [];
  const list    = $("diag-list");
  const summary = $("diag-sum");
  if (!ds.length) {
    list.innerHTML = '<li class="d-ok">&#x2713; Clean</li>';
    summary.textContent = "clean"; summary.className = "hint ok";
    $("issue-count").textContent = ""; return;
  }
  const errs  = ds.filter(d => d.severity === "error");
  const warns = ds.filter(d => d.severity !== "error");
  const parts = [];
  if (errs.length)  parts.push(errs.length  + " error"   + (errs.length  > 1 ? "s" : ""));
  if (warns.length) parts.push(warns.length + " warning" + (warns.length > 1 ? "s" : ""));
  const txt = parts.join(", ");
  summary.textContent = txt;
  summary.className = "hint " + (errs.length ? "err" : "wrn");
  $("issue-count").textContent = txt;
  list.innerHTML = ds.map(d => {
    const isE = d.severity === "error";
    return `<li class="${isE ? "d-err" : "d-warn"}">` +
      `<span>${isE ? "&#x2715;" : "&#x26A0;"}</span>` +
      `<span>${esc(d.message)}</span></li>`;
  }).join("");
}

/* ═══════════════════════════════════════════════════════════
   Actions
═══════════════════════════════════════════════════════════ */
async function run() {
  const st = $("status");
  st.textContent = "running…"; st.className = "run";
  $("run").disabled = true;
  try {
    const data = await api("/api/run", { code: editor.getValue() });
    if (data.ok) {
      $("preview").srcdoc = data.html;
      st.textContent = "ready"; st.className = "ok";
      showDiagnostics(data);
    } else {
      $("preview").srcdoc =
        `<pre style="padding:16px;color:#b42318;white-space:pre-wrap;font:13px/1.5 monospace">${esc(data.error || "error")}</pre>`;
      st.textContent = "error"; st.className = "err";
      $("diag-sum").textContent = "parse error"; $("diag-sum").className = "hint err";
      $("diag-list").innerHTML =
        `<li class="d-err"><span>&#x2715;</span><span>${esc(data.error || "")}</span></li>`;
      $("issue-count").textContent = "1 error";
    }
  } catch (e) { st.textContent = String(e); st.className = "err"; }
  finally     { $("run").disabled = false; }
}

async function formatSource() {
  const st = $("status");
  st.textContent = "formatting…"; st.className = "run";
  const data = await api("/api/format", { code: editor.getValue() });
  if (data.ok) {
    loading = true; editor.setValue(data.code); loading = false;
    dirty = true; updateMeta();
    st.textContent = "formatted"; st.className = "ok";
    showDiagnostics({ diagnostics: [] });
  } else {
    st.textContent = "format error"; st.className = "err";
    $("diag-list").innerHTML =
      `<li class="d-err"><span>&#x2715;</span><span>${esc(data.error || "")}</span></li>`;
  }
}

async function lintSource() {
  const st = $("status");
  st.textContent = "linting…"; st.className = "run";
  $("lint").disabled = true;
  try {
    const data = await api("/api/lint", { code: editor.getValue() });
    if (data.ok) {
      showDiagnostics(data);
      const ds = data.diagnostics || [];
      st.textContent = ds.length ? "lint found issues" : "lint clean";
      st.className = ds.length ? "run" : "ok";
    } else {
      st.textContent = "lint error"; st.className = "err";
      $("diag-sum").textContent = "lint error"; $("diag-sum").className = "hint err";
      $("diag-list").innerHTML =
        `<li class="d-err"><span>&#x2715;</span><span>${esc(data.error || "")}</span></li>`;
      $("issue-count").textContent = "1 error";
    }
  } catch (e) {
    st.textContent = String(e); st.className = "err";
  } finally {
    $("lint").disabled = false;
  }
}

async function exportHtml() {
  const st = $("status");
  st.textContent = "exporting…"; st.className = "run";
  $("export").disabled = true;
  try {
    const data = await api("/api/export", { code: editor.getValue() });
    if (data.ok) {
      showDiagnostics(data);
      const blob = new Blob([data.html], { type: "text/html" });
      const url = URL.createObjectURL(blob);
      const a = document.createElement("a");
      a.href = url;
      a.download = activeDisplayName().replace(/[/:]/g, "-").replace(/\.jtml$/, ".html");
      document.body.appendChild(a);
      a.click();
      a.remove();
      URL.revokeObjectURL(url);
      st.textContent = "exported html"; st.className = "ok";
    } else {
      st.textContent = "export error"; st.className = "err";
      $("diag-sum").textContent = "export error"; $("diag-sum").className = "hint err";
      $("diag-list").innerHTML =
        `<li class="d-err"><span>&#x2715;</span><span>${esc(data.error || "")}</span></li>`;
      $("issue-count").textContent = "1 error";
    }
  } catch (e) {
    st.textContent = String(e); st.className = "err";
  } finally {
    $("export").disabled = false;
  }
}

function saveSource() {
  const code = editor.getValue();
  const st   = $("status");
  if (mode === "file") {
    const s = SAMPLES[activeFileIdx];
    const result = pushVersion(s, code);
    dirty = false; updateMeta(); renderSidebar();
    st.textContent = result.saved ? "saved v" + result.version : "already saved v" + result.version;
    st.className = "ok";
  } else if (mode === "lesson" && activeLessonIdx >= 0) {
    const result = saveLessonCode(lessons[activeLessonIdx], code);
    dirty = false; updateMeta(); renderSidebar();
    st.textContent = result.saved ? "saved v" + result.version : "already saved v" + result.version;
    st.className = "ok";
  }
}

function resetSource() {
  const st = $("status");
  if (mode === "file") {
    loading = true; editor.setValue(SAMPLES[activeFileIdx].code); editor.clearHistory(); loading = false;
    dirty = false; updateMeta();
    st.textContent = "reset"; st.className = "";
  } else if (mode === "lesson" && activeLessonIdx >= 0) {
    const l = lessons[activeLessonIdx];
    loading = true; editor.setValue(l.code || ""); editor.clearHistory(); loading = false;
    dirty = false; updateMeta();
    st.textContent = "reset to original"; st.className = "";
  }
  run();
}

$("run").onclick    = run;
$("lint").onclick   = lintSource;
$("format").onclick = formatSource;
$("export").onclick = exportHtml;
$("save").onclick   = saveSource;
$("reset").onclick  = resetSource;

/* ═══════════════════════════════════════════════════════════
   History modal
═══════════════════════════════════════════════════════════ */
function openHistory() {
  const name = activeDocName();
  const displayName = activeDisplayName();
  const vs = getVersionsByName(name);
  $("hist-title").textContent = displayName + " — history";
  $("hist-count").textContent = vs.length
    ? vs.length + " snapshot" + (vs.length > 1 ? "s" : "") + " · up to 50 kept"
    : "No snapshots yet";
  const list = $("hist-list");
  if (!vs.length) {
    list.innerHTML = '<li class="hist-empty">No versions saved. Press <strong>Save</strong> to create the first snapshot.</li>';
  } else {
    list.innerHTML = vs.map((v, i) => {
      const dt    = new Date(v.savedAt).toLocaleString(undefined, { month: "short", day: "numeric", hour: "2-digit", minute: "2-digit" });
      const lines = v.lines || v.code.split("\n").length;
      const version = v.version || (vs.length - i);
      const hash = v.hash || hashText(v.code);
      return `<li class="hist-item">
        <div class="hist-top">
          <div><span class="hist-ts">v${version} · ${esc(dt)}</span>${i === 0 ? ' <span class="hist-latest">latest</span>' : ""}</div>
          <button class="btn" style="font-size:12px;padding:3px 9px" data-idx="${i}">Restore</button>
        </div>
        <div class="hist-stats">${lines} line${lines !== 1 ? "s" : ""} · ${v.chars || v.code.length} chars · ${hash}</div>
        <div class="hist-pre">${esc(v.code.slice(0, 180))}${v.code.length > 180 ? "…" : ""}</div>
      </li>`;
    }).join("");
    list.querySelectorAll("button[data-idx]").forEach(btn => {
      btn.onclick = () => {
        const v = vs[Number(btn.dataset.idx)];
        loading = true; editor.setValue(v.code); editor.clearHistory(); loading = false;
        dirty = true; updateMeta(); closeHistory();
        $("status").textContent = "restored"; $("status").className = "ok";
        run();
      };
    });
  }
  $("history-modal").hidden = false;
}
function closeHistory() { $("history-modal").hidden = true; }

$("history-btn").onclick = openHistory;
$("hist-close").onclick  = closeHistory;
$("hist-bg").onclick     = closeHistory;
document.addEventListener("keydown", e => { if (e.key === "Escape") closeHistory(); });

/* ═══════════════════════════════════════════════════════════
   Boot
═══════════════════════════════════════════════════════════ */
async function boot() {
  try {
    const res = await fetch("/api/lessons");
    if (res.ok) lessons = await res.json();
  } catch {}
  renderSidebar();
  /* Apply layout after first paint so getBoundingClientRect returns real values */
  requestAnimationFrame(() => {
    applyLayout();
    selectFile(0);
  });
}
boot();
</script>
</body>
</html>
)HTML";

} // namespace jtml::cli
