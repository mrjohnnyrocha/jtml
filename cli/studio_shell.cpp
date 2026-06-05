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
  --bg:     #eef1f4;
  --panel:  #ffffff;
  --panel-2:#f8fafc;
  --ink:    #17212b;
  --muted:  #64748b;
  --border: #d8dee7;
  --dark:   #111820;
  --dark-2: #0b1219;
  --line:   #203040;
  --accent: #0f766e;
  --accent-2: #2563eb;
  --gold:   #b7791f;
  --danger: #b42318;
  --warn:   #92400e;
  --ok:     #065f46;
  --shadow: 0 18px 50px rgba(15, 23, 42, .10);
}
*, *::before, *::after { box-sizing: border-box; }
html, body { height: 100%; margin: 0; overflow: hidden; }
body {
  background: var(--bg);
  color: var(--ink);
  font-family: Inter, ui-sans-serif, system-ui, -apple-system, "Segoe UI", sans-serif;
  font-size: 14px;
}
body::before {
  content: "";
  position: fixed;
  inset: 0;
  pointer-events: none;
  background:
    linear-gradient(90deg, rgba(15, 118, 110, .08), transparent 34%),
    linear-gradient(180deg, rgba(37, 99, 235, .06), transparent 42%);
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
  background: #111820;
  color: #fff;
  border: 1px solid rgba(255,255,255,.08);
  border-radius: 12px;
  padding: 10px 14px;
  display: flex;
  align-items: center;
  gap: 10px;
  flex-shrink: 0;
  overflow: hidden;
  min-width: 0;
  box-shadow: 0 12px 34px rgba(15, 23, 42, .20);
}
.logo { font-size: 20px; font-weight: 800; letter-spacing: -0.5px; white-space: nowrap; }
.logo sub { font-size: 11px; font-weight: 600; color: #7dd3c8; vertical-align: baseline; margin-left: 3px; }
.brand-btn { background: transparent; border: 0; color: inherit; padding: 0; display: flex; align-items: baseline; }
.tagline { color: #9eb3c4; font-size: 12px; flex: 1; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.h-actions {
  display: flex;
  gap: 6px;
  align-items: center;
  justify-content: flex-end;
  flex: 1 1 auto;
  min-width: 0;
  overflow-x: auto;
  scrollbar-width: thin;
}
.mode-cluster {
  display: flex;
  align-items: center;
  gap: 6px;
  flex-shrink: 0;
}
.hub-chip {
  border: 1px solid rgba(125,211,200,.25);
  background: rgba(125,211,200,.10);
  color: #c8f3ed;
  border-radius: 999px;
  padding: 4px 9px;
  font-size: 11px;
  font-weight: 750;
  white-space: nowrap;
}
.kb { color: #5c7a8a; font-size: 11px; white-space: nowrap; }
.kb kbd {
  font-family: "SF Mono", Menlo, Consolas, monospace; font-size: 10px;
  background: #1e2e38; border: 1px solid #3a4d58; border-bottom-width: 2px;
  border-radius: 3px; padding: 1px 5px; color: #a8c0cc;
}
.btn {
  border: 1px solid #a9b0b6; background: var(--panel); color: var(--ink);
  border-radius: 6px; padding: 6px 11px; font-weight: 700; font-size: 13px;
  flex-shrink: 0;
}
.btn:hover:not(:disabled) { border-color: #6e7b85; }
.btn.primary { background: var(--accent); border-color: var(--accent); color: #fff; }
.btn.secondary { background: #eaf2ff; border-color: #bcd3ff; color: #174ea6; }
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
  border-radius: 10px;
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
.sidebar-search {
  width: 100%;
  border: 1px solid var(--border);
  border-radius: 7px;
  padding: 7px 9px;
  background: var(--panel-2);
  color: var(--ink);
  font: inherit;
  font-size: 12px;
  outline: none;
}
.sidebar-search:focus { border-color: #9ab6e8; box-shadow: 0 0 0 3px rgba(37,99,235,.10); }
.workspace-actions {
  display: grid;
  grid-template-columns: repeat(3, minmax(0, 1fr));
  gap: 5px;
}
.workspace-create {
  display: grid;
  gap: 6px;
  padding: 7px;
  border: 1px solid var(--border);
  border-radius: 7px;
  background: var(--panel-2);
}
.workspace-create[hidden] { display: none; }
.create-label {
  font-size: 10px;
  font-weight: 850;
  color: var(--muted);
  text-transform: uppercase;
  letter-spacing: .06em;
}
.create-input {
  width: 100%;
  min-width: 0;
  border: 1px solid var(--border);
  border-radius: 5px;
  padding: 6px 7px;
  background: #fff;
  color: var(--ink);
  font: inherit;
  font-size: 11px;
}
.create-actions { display: flex; gap: 5px; }
.create-actions .mini-btn { flex: 1; }
.create-error {
  min-height: 14px;
  margin: 0;
  color: #b91c1c;
  font-size: 10px;
  line-height: 1.35;
}
.mini-btn {
  border: 1px solid var(--border);
  background: var(--panel-2);
  color: var(--ink);
  border-radius: 5px;
  padding: 5px 6px;
  font: inherit;
  font-size: 10px;
  font-weight: 800;
}
.mini-btn:hover { border-color: #9ab6e8; color: #174ea6; }
.tree-group { margin: 2px 0 6px; }
.tree-folder {
  font-size: 11px;
  font-weight: 850;
  color: #596b7f;
  padding: 5px 8px 4px;
  text-transform: uppercase;
  letter-spacing: .06em;
}
.tree-children {
  display: grid;
  gap: 2px;
  margin-left: 8px;
  padding-left: 7px;
  border-left: 1px solid rgba(103,116,135,.18);
}
.sb-empty-note {
  color: var(--muted);
  font-size: 11px;
  line-height: 1.45;
  padding: 6px 8px;
}
.sb-section { display: flex; flex-direction: column; gap: 2px; }
.sb-section[hidden] { display: none; }
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
  color: var(--ink); overflow: hidden;
}
.sb-item span { overflow: hidden; text-overflow: ellipsis; }
.sb-text { min-width: 0; display: grid; gap: 1px; }
.sb-main { white-space: nowrap; font-weight: 700; }
.sb-sub { white-space: nowrap; font-size: 10px; color: var(--muted); font-weight: 600; }
.sb-item.active-file .sb-sub,
.sb-item.active-lesson .sb-sub,
.sb-item.active-doc .sb-sub { color: currentColor; opacity: .75; }
.sb-item:hover:not(:disabled) { background: #edeae2; }
.sb-item.active-file { background: #e7f1ef; border-color: #b7d6d1; color: #0d5d57; }
.sb-item.active-lesson { background: #eff6ff; border-color: #bfdbfe; color: #1e40af; }
.sb-item.active-hub { background: #111820; border-color: #111820; color: #fff; }
.sb-item.active-doc { background: #fff7ed; border-color: #fed7aa; color: #9a3412; }
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

/* ── Studio hub ─────────────────────────────────────────────── */
.hub-panel {
  flex: 1;
  min-height: 0;
  overflow: auto;
  background: var(--panel);
  border: 1px solid var(--border);
  border-radius: 12px;
  box-shadow: var(--shadow);
}
.hub-inner {
  max-width: 1180px;
  margin: 0 auto;
  padding: 22px;
  display: grid;
  gap: 18px;
}
.hub-hero {
  display: grid;
  grid-template-columns: minmax(0, 1.25fr) minmax(320px, .75fr);
  gap: 18px;
  align-items: stretch;
}
.hub-title { margin: 0; font-size: 34px; line-height: 1.04; letter-spacing: 0; }
.hub-copy { margin: 10px 0 0; color: #4b5f74; font-size: 15px; line-height: 1.6; max-width: 760px; }
.hub-actions { display: flex; flex-wrap: wrap; gap: 8px; margin-top: 16px; }
.hub-card, .hub-hero-card {
  border: 1px solid var(--border);
  border-radius: 10px;
  background: #fff;
  padding: 16px;
}
.hub-hero-card {
  background: #111820;
  color: #edf6fb;
  border-color: #243241;
  display: grid;
  gap: 12px;
}
.hub-hero-card h2,
.hub-card h3 { margin: 0; font-size: 15px; }
.hub-hero-card p,
.hub-card p { margin: 6px 0 0; color: #627386; line-height: 1.45; }
.hub-hero-card p { color: #adc0ce; }
.hub-kpis { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; }
.hub-kpi {
  border: 1px solid rgba(255,255,255,.10);
  border-radius: 8px;
  padding: 10px;
  background: rgba(255,255,255,.05);
}
.hub-kpi strong { display: block; font-size: 22px; line-height: 1; }
.hub-kpi span { color: #9eb3c4; font-size: 11px; }
.hub-grid { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 12px; }
.hub-card { display: grid; gap: 10px; align-content: start; }
.hub-card .btn { justify-self: start; }
.hub-list { display: grid; gap: 6px; margin: 0; padding: 0; list-style: none; }
.hub-list li {
  display: flex;
  justify-content: space-between;
  gap: 10px;
  border-top: 1px solid #edf0f4;
  padding-top: 7px;
  color: #3f5164;
  font-size: 12px;
}
.hub-list li:first-child { border-top: 0; padding-top: 0; }
.hub-tag {
  display: inline-flex;
  align-items: center;
  border-radius: 999px;
  padding: 3px 8px;
  font-size: 11px;
  font-weight: 750;
  background: #ecfeff;
  color: #0e7490;
}
.hub-track { display: flex; flex-wrap: wrap; gap: 6px; }
.hub-track button {
  border: 1px solid var(--border);
  background: var(--panel-2);
  border-radius: 999px;
  padding: 5px 9px;
  font-size: 12px;
  font-weight: 700;
  color: #304256;
}
.hub-track button:hover { border-color: #9ab6e8; color: #174ea6; }

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
.lesson-after-panel {
  flex-shrink: 0;
  background: var(--panel);
  border: 1px solid var(--border);
  border-radius: 8px;
  overflow: auto;
  max-height: 170px;
}
.lesson-after-panel[hidden] { display: none; }
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
.editor-tools {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 7px 10px;
  border-bottom: 1px solid var(--line);
  background: rgba(15, 23, 32, .72);
  overflow-x: auto;
  scrollbar-width: thin;
  flex-shrink: 0;
}
.editor-tools .btn { padding: 5px 9px; font-size: 12px; }
.tool-caption {
  color: #88a0b5;
  font: 10px/1 "SF Mono", Menlo, Consolas, monospace;
  text-transform: uppercase;
  letter-spacing: .08em;
  margin-right: 2px;
  white-space: nowrap;
}
.editor-name { margin: 0; font-size: 12px; font-weight: 700; color: #b8d0da; }
.editor-right { display: flex; align-items: center; gap: 7px; }
.editor-meta { font: 10px/1 "SF Mono", Menlo, Consolas, monospace; color: #3d5970; }
.dialect-badge {
  border: 1px solid #315f99;
  background: rgba(37, 99, 235, .18);
  color: #b7d3ff;
  border-radius: 999px;
  padding: 2px 7px;
  font-size: 10px;
  font-weight: 800;
  white-space: nowrap;
}
.dialect-badge.classic {
  border-color: #925d22;
  background: rgba(183, 121, 31, .18);
  color: #ffd69b;
}
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
.cm-s-jtml-dark .cm-atom         { color: #f472b6; }           /* pink    — true/false */
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
  flex-direction: column;
  overflow: hidden;
  will-change: height;
  background: var(--panel);
  border: 1px solid var(--border);
  border-radius: 8px;
}
.bottom-row.collapsing { transition: height .2s ease; }
.inspector-head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 10px;
  padding: 7px 10px;
  border-bottom: 1px solid var(--border);
  flex-shrink: 0;
}
.inspector-tabs { display: flex; align-items: center; gap: 5px; }
.inspector-tab {
  border: 1px solid var(--border);
  background: transparent;
  color: var(--muted);
  border-radius: 5px;
  padding: 3px 8px;
  font: inherit;
  font-size: 11px;
  font-weight: 800;
}
.inspector-tab.active { background: #e7f1ef; border-color: #b7d6d1; color: #0d5d57; }
.bottom-row > .rh { display: none; }
.diag-panel {
  flex: 1; min-width: 0;
  display: flex; flex-direction: column;
  overflow: hidden;
}
.artifact-panel {
  flex: 1;
  min-width: 0;
  display: flex; flex-direction: column;
  overflow: hidden;
}
.ref-panel {
  flex: 1; min-width: 0;
  display: flex; flex-direction: column;
  overflow: hidden;
}
.bottom-row[data-mode="diagnostics"] .artifact-panel,
.bottom-row[data-mode="diagnostics"] .ref-panel,
.bottom-row[data-mode="artifacts"] .diag-panel,
.bottom-row[data-mode="artifacts"] .ref-panel,
.bottom-row[data-mode="reference"] .diag-panel,
.bottom-row[data-mode="reference"] .artifact-panel { display: none; }
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
.diag-main { min-width: 0; display: grid; gap: 2px; }
.diag-head { display: flex; gap: 6px; flex-wrap: wrap; align-items: baseline; }
.diag-code { font-weight: 800; opacity: .78; }
.diag-loc { opacity: .72; }
.diag-hint { font-family: Inter, system-ui, sans-serif; font-size: 11px; line-height: 1.35; color: #5f6e76; }
.diag-example { margin: 2px 0 0; padding: 4px 6px; border-radius: 4px; background: rgba(255,255,255,.7); color: #334155; white-space: pre-wrap; }
.hint.ok  { color: var(--ok); }
.hint.err { color: var(--danger); }
.hint.wrn { color: var(--warn); }
/* Artifacts */
.artifact-tabs { display: flex; gap: 4px; align-items: center; }
.artifact-tab {
  border: 1px solid var(--border);
  background: transparent;
  color: var(--muted);
  border-radius: 4px;
  padding: 2px 7px;
  font-size: 11px;
  font-weight: 700;
}
.artifact-tab.active { background: #e7f1ef; border-color: #b7d6d1; color: #0d5d57; }
.artifact-code {
  margin: 0;
  min-height: 100%;
  font: 11px/1.55 "SF Mono", Menlo, Consolas, monospace;
  white-space: pre;
  overflow: auto;
  color: #334155;
  background: #f8f6ef;
}
.artifact-empty { color: var(--muted); font-size: 12px; line-height: 1.45; }
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
.confirm-box { width: min(520px, 92vw); }
.confirm-body { padding: 14px 15px; display: grid; gap: 10px; color: var(--ink); }
.confirm-copy { margin: 0; font-size: 13px; line-height: 1.45; color: #34444f; }
.confirm-summary { border: 1px solid var(--border); background: #f8f6ef; border-radius: 8px; padding: 10px 12px; font-size: 12px; line-height: 1.45; color: #586873; }
.confirm-actions { padding: 12px 15px; border-top: 1px solid var(--border); display: flex; justify-content: flex-end; gap: 8px; }
.btn.danger { background: var(--danger); border-color: var(--danger); color: white; }
.btn.danger:hover { background: #8f1b12; border-color: #8f1b12; }
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

/* ── Command palette ───────────────────────────────────────── */
.palette-box { width: min(720px, 92vw); max-height: 76vh; }
.palette-search {
  width: 100%;
  border: 1px solid var(--border);
  border-radius: 8px;
  padding: 10px 12px;
  font: 14px/1.2 Inter, system-ui, sans-serif;
  outline: none;
  background: #fbfcfe;
}
.palette-search:focus { border-color: #87aee8; box-shadow: 0 0 0 3px rgba(37,99,235,.10); }
.palette-list { list-style: none; margin: 10px 0 0; padding: 0; display: grid; gap: 4px; }
.palette-item {
  width: 100%;
  border: 1px solid transparent;
  background: transparent;
  color: var(--ink);
  text-align: left;
  border-radius: 7px;
  padding: 9px 10px;
  display: grid;
  grid-template-columns: minmax(0, 1fr) auto;
  gap: 8px;
}
.palette-item:hover,
.palette-item.active { background: #eef7f5; border-color: #bfded9; }
.palette-title { font-size: 13px; font-weight: 800; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.palette-sub { grid-column: 1 / -1; color: var(--muted); font-size: 11px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.palette-kind { color: #0d5d57; background: #dff3ef; border-radius: 999px; padding: 1px 7px; font-size: 10px; font-weight: 800; text-transform: uppercase; letter-spacing: .04em; }
.palette-empty { padding: 22px; color: var(--muted); text-align: center; font-size: 13px; }

/* ── Responsive ──────────────────────────────────────────────── */
@media (max-width: 860px) {
  .app { padding: 6px; }
  header { flex-wrap: wrap; }
  .tagline, .kb, .hub-chip { display: none; }
  .h-actions { flex-wrap: wrap; }
  .hub-inner { padding: 14px; }
  .hub-hero, .hub-grid { grid-template-columns: 1fr; }
  .hub-title { font-size: 27px; }
  .work-row { flex-direction: column; }
  .editor-panel { flex-basis: auto !important; flex-shrink: 0; height: 55%; }
  .preview-panel { flex: 1; }
  #rh-editor.col { display: none; }
}
@media (max-width: 700px) {
  .btn.dark { padding: 5px 8px; font-size: 12px; }
  #export { display: none; }
  #format { display: none; }
}
@media (max-width: 520px) {
  #lint { display: none; }
  header { padding: 8px 10px; gap: 6px; }
}
@media (max-width: 1280px) {
  .tagline, .kb { display: none; }
  .h-actions { justify-content: flex-start; }
}
</style>
</head>
<body>
<div class="app">

  <!-- ── Header ─────────────────────────────────────────────── -->
  <header>
    <button class="btn dark" id="sidebar-toggle" title="Toggle sidebar (S)">&#9776;</button>
    <button class="brand-btn" id="brand-home" title="Open Studio home">
      <span class="logo">JTML<sub>studio</sub></span>
    </button>
    <span class="hub-chip" id="workspace-chip">Local workspace</span>
    <span class="tagline">Home · Learn · Build · Debug · Ship</span>
    <div class="h-actions">
      <div class="mode-cluster" aria-label="Studio sections">
        <button class="btn dark" id="home-btn">Home</button>
        <button class="btn dark" id="learn-btn">Learn</button>
        <button class="btn dark" id="docs-btn">Docs</button>
        <button class="btn dark" id="reference-btn">Reference</button>
        <button class="btn dark" id="playground-btn">IDE</button>
      </div>
    </div>
  </header>

  <!-- ── Main layout ────────────────────────────────────────── -->
  <div class="main-layout" id="main-layout">

    <!-- Sidebar -->
    <aside class="sidebar" id="sidebar">
      <div class="sidebar-head">
        <h2 class="sidebar-title">Workspace</h2>
      </div>
      <div class="sidebar-body">
        <button class="sb-item active-hub" id="hub-nav">
          <span class="sb-text">
            <span class="sb-main">Studio home</span>
            <span class="sb-sub">overview, paths, and workflow</span>
          </span>
        </button>
        <input class="sidebar-search" id="sidebar-search" type="search" placeholder="Search examples, lessons, docs">
        <div class="sb-section">
          <span class="sb-label">Projects</span>
          <div class="workspace-actions">
            <button class="mini-btn" id="new-project">Project</button>
            <button class="mini-btn" id="new-folder">Folder</button>
            <button class="mini-btn" id="new-file">File</button>
          </div>
          <div class="workspace-create" id="workspace-create" hidden>
            <div class="create-label" id="workspace-create-title">New file</div>
            <input class="create-input" id="workspace-create-path" type="text" autocomplete="off" spellcheck="false">
            <div class="create-actions">
              <button class="mini-btn" id="workspace-create-confirm">Create</button>
              <button class="mini-btn" id="workspace-create-cancel">Cancel</button>
            </div>
            <p class="create-error" id="workspace-create-error"></p>
          </div>
          <div id="project-tree"></div>
        </div>
        <div class="sb-section">
          <span class="sb-label">Templates</span>
          <div id="file-list"></div>
        </div>
        <div class="sb-section" hidden>
          <span class="sb-label">Tutorial</span>
          <div id="lesson-list"><p class="no-items">Loading…</p></div>
        </div>
        <div class="sb-section" hidden>
          <span class="sb-label">Docs &amp; guides</span>
          <div id="doc-list"><p class="no-items">Loading…</p></div>
        </div>
      </div>
    </aside>

    <!-- Sidebar resize handle -->
    <div class="rh col" id="rh-sidebar" title="Drag to resize · Double-click to reset"></div>

    <!-- Content column -->
    <div class="content" id="content">

      <section class="hub-panel" id="hub-panel">
        <div class="hub-inner">
          <div class="hub-hero">
            <div class="hub-card">
              <span class="hub-tag">JTML command center</span>
              <h1 class="hub-title">Build, learn, test, and ship JTML from one place.</h1>
              <p class="hub-copy">Studio is the home hub for the language: Learn gives you the narrative tutorial, Docs gives guides, Reference gives exact syntax, and IDE opens the editor, compiler inspector, formatter, linter, fixer, previewer, and export tools.</p>
              <div class="hub-actions">
                <button class="btn primary" data-open-sample="counter.jtml">Open playground</button>
                <button class="btn secondary" data-open-lesson="0">Start tutorial</button>
                <button class="btn" data-open-doc="language-reference">Language reference</button>
                <button class="btn" data-open-sample="routes.jtml">SPA routes</button>
              </div>
            </div>
            <div class="hub-hero-card">
              <h2>Workspace readiness</h2>
              <div class="hub-kpis">
                <div class="hub-kpi"><strong id="hub-example-count">0</strong><span>examples</span></div>
                <div class="hub-kpi"><strong id="hub-lesson-count">0</strong><span>lessons</span></div>
                <div class="hub-kpi"><strong id="hub-doc-count">0</strong><span>guides</span></div>
              </div>
              <ul class="hub-list">
                <li><span>Authoring contract</span><strong>Friendly JTML 2</strong></li>
                <li><span>Runtime loop</span><strong>Run + Preview</strong></li>
                <li><span>Quality gate</span><strong>Lint + Fix + Format</strong></li>
                <li><span>Delivery</span><strong>Export HTML</strong></li>
              </ul>
            </div>
          </div>
          <div class="hub-grid">
            <article class="hub-card">
              <h3>Beginner path</h3>
              <p>Learn the grammar from documents to state, events, inputs, loops, and complete apps.</p>
              <div class="hub-track">
                <button data-open-lesson="0">Welcome</button>
                <button data-open-lesson="1">Hello</button>
                <button data-open-lesson="2">State</button>
                <button data-open-lesson="3">Events</button>
              </div>
            </article>
            <article class="hub-card">
              <h3>Application patterns</h3>
              <p>Jump into production-style examples and plans for data, stores, routes, components, effects, media, graphics, and interop.</p>
              <div class="hub-track">
                <button data-open-sample="fetch.jtml">Fetch</button>
                <button data-open-sample="store.jtml">Store</button>
                <button data-open-sample="routes.jtml">Routes</button>
                <button data-open-sample="media.jtml">Media</button>
                <button data-open-sample="charts.jtml">Charts</button>
                <button data-open-sample="animation.jtml">Animation</button>
                <button data-open-sample="components.jtml">Components</button>
              </div>
            </article>
            <article class="hub-card">
              <h3>Advanced docs</h3>
              <p>Use Studio as the living reference for AI-native authoring, runtime APIs, editor tooling, and deployment.</p>
              <div class="hub-track">
                <button data-open-doc="ai-authoring-contract">AI contract</button>
                <button data-open-doc="media-graphics-roadmap">Media/graphics</button>
                <button data-open-doc="runtime-http-contract">Runtime API</button>
                <button data-open-doc="language-server">LSP</button>
                <button data-open-doc="deployment">Deploy</button>
              </div>
            </article>
          </div>
          <div class="hub-grid">
            <article class="hub-card">
              <h3>Studio workflow</h3>
              <ul class="hub-list">
                <li><span>Write Friendly JTML</span><strong>jtml 2</strong></li>
                <li><span>Compile and preview</span><strong>Run</strong></li>
                <li><span>Inspect lowered code</span><strong>Artifacts</strong></li>
                <li><span>Clean source</span><strong>Fix + Format</strong></li>
              </ul>
            </article>
            <article class="hub-card">
              <h3>Friendly vs Classic</h3>
              <p>Use Friendly JTML 2 for all new code. The compatibility backend exists for older files, generated artifacts, migration, and embedding.</p>
              <div class="hub-track">
                <button data-open-lesson="12">Compatibility lesson</button>
                <button data-open-doc="language-reference">Syntax reference</button>
              </div>
            </article>
            <article class="hub-card">
              <h3>Production language</h3>
              <ul class="hub-list">
                <li><span>Async data</span><strong>fetch / lazy / invalidate</strong></li>
                <li><span>SPA navigation</span><strong>route / guard / layout</strong></li>
                <li><span>Composition</span><strong>make / slot / use</strong></li>
                <li><span>Media today</span><strong>image / video / audio / extern</strong></li>
                <li><span>State model</span><strong>let / get / store / effect</strong></li>
              </ul>
            </article>
            <article class="hub-card">
              <h3>Developer tooling</h3>
              <ul class="hub-list">
                <li><span>Diagnostics</span><strong>structured JSON</strong></li>
                <li><span>Versioning</span><strong>local snapshots</strong></li>
                <li><span>Editor support</span><strong>LSP + syntax color</strong></li>
                <li><span>Embedding</span><strong>HTTP + C ABI</strong></li>
              </ul>
            </article>
          </div>
        </div>
      </section>

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
              <span class="dialect-badge" id="dialect-badge">Friendly JTML 2</span>
              <span class="editor-meta" id="editor-meta"></span>
              <button class="history-btn" id="history-btn">History</button>
            </div>
          </div>
          <div class="editor-tools" id="editor-tools" aria-label="Current document tools">
            <span class="tool-caption">Tools</span>
            <span class="kb"><kbd>Cmd</kbd>/<kbd>Ctrl</kbd>+<kbd>Enter</kbd> run</span>
            <button class="btn primary" id="run">Run</button>
            <button class="btn dark" id="lint">Lint</button>
            <button class="btn dark" id="fix">Fix</button>
            <button class="btn dark" id="format">Format</button>
            <button class="btn dark" id="export">Export</button>
            <button class="btn dark" id="save">Save</button>
            <button class="btn dark" id="reset">Reset</button>
            <button class="btn dark" id="bottom-toggle" title="Toggle inspector panel (D)">&#x229F;</button>
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

      <!-- Follow-up prose appears below the live lesson so code + preview sit in the middle. -->
      <div class="lesson-after-panel" id="lesson-after-panel" hidden>
        <div class="prose-body" id="lesson-after-body"></div>
      </div>

      <!-- Bottom resize handle -->
      <div class="rh row" id="rh-bottom" title="Drag to resize · Double-click to reset"></div>

      <!-- Bottom row -->
      <div class="bottom-row" id="bottom-row" data-mode="diagnostics">
        <div class="inspector-head">
          <h2 class="panel-title">Inspector</h2>
          <div class="inspector-tabs" aria-label="Inspector sections">
            <button class="inspector-tab active" id="inspector-diagnostics">Diagnostics</button>
            <button class="inspector-tab" id="inspector-artifacts">Artifacts</button>
            <button class="inspector-tab" id="inspector-reference">Reference</button>
          </div>
        </div>

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

        <section class="artifact-panel">
          <div class="panel-head">
            <h2 class="panel-title">Artifacts</h2>
            <div class="artifact-tabs">
              <button class="artifact-tab active" id="artifact-html-tab">HTML</button>
              <button class="artifact-tab" id="artifact-classic-tab">Compatibility IR</button>
            </div>
          </div>
          <div class="panel-body" style="padding:0">
            <pre class="artifact-code" id="artifact-code"><span class="artifact-empty">Run the current file to inspect generated HTML and compatibility IR.</span></pre>
          </div>
        </section>

        <!-- Artifact/Reference spacer handle -->
        <div class="rh col" style="cursor:default" title="Generated compiler artifacts"></div>

        <section class="ref-panel">
          <div class="panel-head"><h2 class="panel-title">Reference</h2></div>
          <div class="panel-body">
            <div class="ref-sec">
              <p class="ref-sec-label">Which syntax should I choose?</p>
              <table class="ref-table"><tbody>
                <tr><td><code>jtml 2</code></td><td><strong>Default for web/app code.</strong> Indentation-based Friendly syntax for humans, AI agents, Studio, docs, examples, and tutorials.</td></tr>
                <tr><td><code>jtl 1</code></td><td>Experimental core-language header for logic-first examples. Currently lowers through the same Friendly pipeline.</td></tr>
                <tr><td><code>Compatibility backend</code></td><td>Support layer for older files, compiler artifacts, generated output, migration targets, and low-level embedding.</td></tr>
                <tr><td><code>jtml migrate old.jtml -o new.jtml</code></td><td>Convert most Classic files into Friendly JTML 2.</td></tr>
                <tr><td><code>Artifacts → Compatibility IR</code></td><td>Shows the lowered backend form. Inspect it for debugging, but do not copy it for new apps.</td></tr>
              </tbody></table>
            </div>
            <div class="ref-sec">
              <p class="ref-sec-label">Friendly state &amp; values</p>
              <table class="ref-table"><thead><tr><th>Keyword</th><th>Syntax</th><th>Notes</th></tr></thead><tbody>
                <tr><td><code>let</code></td><td><code>let x = expr</code></td><td>Reactive mutable var</td></tr>
                <tr><td><code>const</code></td><td><code>const x = expr</code></td><td>Immutable</td></tr>
                <tr><td><code>get</code></td><td><code>get x = expr</code></td><td>Auto-computed derived value</td></tr>
                <tr><td><code>show</code></td><td><code>show expr</code></td><td>Render text</td></tr>
                <tr><td><code>?:</code></td><td><code>ok ? a : b</code></td><td>Conditional value</td></tr>
                <tr><td><code>+=</code></td><td><code>count += 1</code></td><td>Compound update</td></tr>
                <tr><td><code>object</code></td><td><code>{ "key": value }</code></td><td>Dictionary literal</td></tr>
                <tr><td><code>array</code></td><td><code>[a, b, c]</code></td><td>List literal</td></tr>
              </tbody></table>
            </div>
            <div class="ref-sec">
              <p class="ref-sec-label">Compatibility backend: elements</p>
              <table class="ref-table"><tbody>
                <tr><td><code>element tag [attrs] … #</code></td><td>Classic block</td></tr>
                <tr><td><code>@tag [attrs] … #</code></td><td>Short form</td></tr>
                <tr><td><code>\\</code></td><td>Statement end</td></tr>
                <tr><td><code>#</code></td><td>Block close</td></tr>
              </tbody></table>
            </div>
            <div class="ref-sec">
              <p class="ref-sec-label">Compatibility backend: flow</p>
              <table class="ref-table"><tbody>
                <tr><td><code>if (expr)\\</code></td><td>Conditional block</td></tr>
                <tr><td><code>else \\</code></td><td>Fallback block after <code>if</code></td></tr>
                <tr><td><code>for (x in list)\\</code></td><td>Iterate values</td></tr>
                <tr><td><code>while (expr)\\</code></td><td>Loop while true</td></tr>
                <tr><td><code>break</code> / <code>continue</code></td><td>Loop control</td></tr>
              </tbody></table>
            </div>
            <div class="ref-sec">
              <p class="ref-sec-label">Compatibility backend: functions &amp; modules</p>
              <table class="ref-table"><tbody>
                <tr><td><code>function save(value)\\</code></td><td>Define event handler or helper</td></tr>
                <tr><td><code>return expr</code></td><td>Return from function</td></tr>
                <tr><td><code>throw expr</code></td><td>Raise an error</td></tr>
                <tr><td><code>try \\ ... except(error)\\</code></td><td>Handle errors</td></tr>
                <tr><td><code>import name from "file"</code></td><td>Classic import form</td></tr>
                <tr><td><code>export make Card</code></td><td>Friendly module public component</td></tr>
                <tr><td><code>use { Card } from "./card.jtml"</code></td><td>Named import of exported declarations</td></tr>
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
                <tr><td><code>jtml 2</code></td><td>Enable Friendly web/app syntax</td></tr>
                <tr><td><code>jtl 1</code></td><td>Enable experimental JTL core syntax through the Friendly pipeline</td></tr>
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
                <tr><td><code>link "L" to "/path"</code></td><td><code>&lt;a data-jtml-href="#/path"&gt;</code></td></tr>
                <tr><td><code>image src "url" alt "A"</code></td><td><code>&lt;img&gt;</code></td></tr>
                <tr><td><code>video src "demo.mp4" controls into player</code></td><td>Video plus <code>player.paused</code>, <code>player.currentTime</code>, <code>player.play</code></td></tr>
                <tr><td><code>audio src "intro.mp3" controls into playback</code></td><td>Audio plus reactive playback state/actions</td></tr>
                <tr><td><code>file "Choose image" accept "image/*" into selected</code></td><td>File metadata + preview URL</td></tr>
                <tr><td><code>dropzone "Drop media" accept "image/*" into assets</code></td><td>Multiple file input with drag/drop dispatch</td></tr>
                <tr><td><code>graphic aria-label "Chart"</code></td><td><code>&lt;svg role="img"&gt;</code> for declarative graphics</td></tr>
                <tr><td><code>bar</code> / <code>dot</code> / <code>group</code></td><td><code>&lt;rect&gt;</code> / <code>&lt;circle&gt;</code> / <code>&lt;g&gt;</code></td></tr>
                <tr><td><code>chart bar data rows by label value total</code></td><td>Accessible SVG bar chart rendered from JTML state/fetch data</td></tr>
                <tr><td><code>chart line data rows by day value visits</code></td><td>Accessible SVG line chart for trends</td></tr>
                <tr><td><code>values sales expenses</code></td><td>Render multiple fields as grouped bars or multiple lines</td></tr>
                <tr><td><code>series "Sales,Expenses" colors "#0f766e,#b42318"</code></td><td>Label and colour multi-series charts</td></tr>
                <tr><td><code>stacked</code></td><td>Stack multi-series bar segments per row</td></tr>
                <tr><td><code>export svg png csv</code></td><td>Add browser-local chart export buttons</td></tr>
                <tr><td><code>scene3d "Product" scene model camera orbit into sceneState</code></td><td>Canvas 3D mount with fallback, state binding, and <code>window.jtml3d.render</code> host hook</td></tr>
                <tr><td><code>canvas id "chart" width "800"</code></td><td>Raw drawing surface; use <code>extern</code> today</td></tr>
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
                <tr><td><code>fetch … cache "no-store"</code></td><td>Set browser cache policy</td></tr>
                <tr><td><code>fetch … credentials "include"</code></td><td>Send cookies/credentials when needed</td></tr>
                <tr><td><code>fetch … timeout 2500 retry 2</code></td><td>Abort slow requests and retry failures</td></tr>
                <tr><td><code>fetch … stale keep</code></td><td>Keep previous data during refresh/error</td></tr>
                <tr><td><code>fetch … group people</code></td><td>Register related fetches for grouped invalidation</td></tr>
                <tr><td><code>fetch … key id dedupe</code></td><td>Give the request a logical identity and skip duplicate loaded requests</td></tr>
                <tr><td><code>fetch … every 30000</code></td><td>Timed revalidation while the page is visible</td></tr>
                <tr><td><code>fetch … background</code></td><td>Keep timed revalidation active in hidden tabs</td></tr>
                <tr><td><code>fetch … lazy</code></td><td>Register a fetch without starting it immediately</td></tr>
                <tr><td><code>fetch … refresh reload</code></td><td>Wire a <code>reload</code> action to re-trigger the fetch</td></tr>
                <tr><td><code>invalidate data</code></td><td>Inside an action, refresh a named fetch after the action runs</td></tr>
                <tr><td><code>invalidate group people</code></td><td>Refresh all fetches in a named group after an action runs</td></tr>
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
              <p class="ref-sec-label">Interop</p>
              <table class="ref-table"><tbody>
                <tr><td><code>extern notify from "host.notify"</code></td><td>Declare a browser-hosted action</td></tr>
                <tr><td><code>button "Notify" click notify("Saved")</code></td><td>Calls <code>window.host.notify("Saved")</code> client-side</td></tr>
                <tr><td><code>html raw "&lt;host-widget&gt;&lt;/host-widget&gt;"</code></td><td>Trusted raw HTML escape hatch for custom elements/host widgets</td></tr>
                <tr><td><code>css raw</code></td><td>Unscoped CSS escape hatch for host-owned surfaces</td></tr>
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
              <p class="ref-sec-label">Semantic UI</p>
              <table class="ref-table"><tbody>
                <tr><td><code>theme</code></td><td>Define color, spacing, radius, and shadow tokens</td></tr>
                <tr><td><code>shell / sidebar / content</code></td><td>App chrome primitives that lower to ordinary HTML/classes</td></tr>
                <tr><td><code>panel title "Usage" pad lg shadow md</code></td><td>Framed surface with semantic utility modifiers</td></tr>
                <tr><td><code>grid cols 4 gap md</code></td><td>Responsive grid intent, visible to semantic analysis</td></tr>
                <tr><td><code>metric "Users" total "Active" tone good</code></td><td>Dashboard metric primitive</td></tr>
              </tbody></table>
            </div>
            <div class="ref-sec">
              <p class="ref-sec-label">Routes (SPA)</p>
              <table class="ref-table"><tbody>
                <tr><td><code>route "/path" as Component</code></td><td>Hash-based route; shows <code>Component</code> at <code>#/path</code></td></tr>
                <tr><td><code>route "/path" as Page layout Shell</code></td><td>Wrap route content in a layout component's <code>slot</code></td></tr>
                <tr><td><code>route "/path" as Page load data</code></td><td>Start lazy fetches only when the route matches</td></tr>
                <tr><td><code>route "/user/:id" as Profile</code></td><td>Route with <code>:param</code> captured as variable</td></tr>
                <tr><td><code>route "*" as NotFound</code></td><td>Wildcard fallback route</td></tr>
                <tr><td><code>link "Label" to "/path"</code></td><td>Navigation link — lowers to a router-owned <code>data-jtml-href="#/path"</code></td></tr>
                <tr><td><code>link "Home" to "/" active-class "active"</code></td><td>Adds CSS class when path matches current route</td></tr>
                <tr><td><code>redirect "/path"</code></td><td>Programmatic navigation from an action</td></tr>
                <tr><td><code>guard "/path" require var</code></td><td>Block route when <code>var</code> is falsy</td></tr>
                <tr><td><code>guard "/path" require var else "/login"</code></td><td>Redirect to fallback when guard fails</td></tr>
                <tr><td><code>activeRoute</code></td><td>Built-in reactive var — current hash path (e.g. <code>"/"</code>)</td></tr>
                <tr><td><code>activeRouteName</code></td><td>Built-in reactive var — matched route component name</td></tr>
              </tbody></table>
            </div>
            <div class="ref-sec">
              <p class="ref-sec-label">Components</p>
              <table class="ref-table"><tbody>
                <tr><td><code>make Name param1 param2</code></td><td>Define a reusable component</td></tr>
                <tr><td><code>&nbsp;&nbsp;slot</code></td><td>Inject caller content here</td></tr>
                <tr><td><code>Name "arg1" "arg2"</code></td><td>Call a component — each instance gets isolated state and a <code>data-jtml-instance</code> marker</td></tr>
                <tr><td><code>Name "arg"</code><br>&nbsp;&nbsp;<code>p "child"</code></td><td>Call with slot content (indented below)</td></tr>
              </tbody></table>
            </div>
            <div class="ref-sec">
              <p class="ref-sec-label">Studio tools</p>
              <table class="ref-table"><tbody>
                <tr><td><code>Run</code> <kbd>⌘↩</kbd></td><td>Compile, lint, and preview in the right pane</td></tr>
                <tr><td><code>Lint</code></td><td>Parser + linter diagnostics only (no preview)</td></tr>
                <tr><td><code>Fix</code></td><td>Apply safe source repairs: Friendly header, tabs, whitespace, final newline</td></tr>
                <tr><td><code>Format</code></td><td>Rewrite source with canonical formatter</td></tr>
                <tr><td><code>Export</code></td><td>Download the compiled HTML file</td></tr>
                <tr><td><code>Save</code></td><td>Versioned local snapshot (up to 50 per file)</td></tr>
                <tr><td><kbd>⌘B</kbd></td><td>Toggle sidebar</td></tr>
                <tr><td><kbd>⌘J</kbd></td><td>Toggle inspector panel</td></tr>
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

<!-- Confirm action modal -->
<div id="confirm-modal" class="modal" hidden>
  <div class="modal-bg" id="confirm-bg"></div>
  <div class="modal-box confirm-box">
    <div class="modal-head">
      <div>
        <h3 id="confirm-title">Are you sure?</h3>
        <span class="hint" id="confirm-hint"></span>
      </div>
      <button class="x-btn" id="confirm-close">&#x2715;</button>
    </div>
    <div class="modal-body confirm-body">
      <p class="confirm-copy" id="confirm-message"></p>
      <div class="confirm-summary" id="confirm-summary"></div>
    </div>
    <div class="confirm-actions">
      <button class="btn dark" id="confirm-cancel">Cancel</button>
      <button class="btn primary" id="confirm-accept">Continue</button>
    </div>
  </div>
</div>

<!-- Command palette -->
<div id="palette-modal" class="modal" hidden>
  <div class="modal-bg" id="palette-bg"></div>
  <div class="modal-box palette-box">
    <div class="modal-head">
      <div>
        <h3>Command palette</h3>
        <span class="hint">Search actions, examples, lessons, and docs</span>
      </div>
      <button class="x-btn" id="palette-close">&#x2715;</button>
    </div>
    <div class="modal-body">
      <input class="palette-search" id="palette-search" type="search" placeholder="Run, format, routes, reference...">
      <ol class="palette-list" id="palette-list"></ol>
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
    /* Event attributes and Friendly event sugar */
    { regex: /\b(?:onClick|onInput|onMouseOver|onScroll|onChange|onFocus|onBlur|onKeyDown|onKeyUp|onKeyPress|onSubmit|onDblClick|click|input|change|submit|focus|blur|keydown|keyup|key-down|key-up|keypress|dblclick|double-click|dragover|drop|mouseover|scroll)\b/, token: "jtml-event" },
    /* HTML-style attributes */
    { regex: /\b(?:style|class|id|type|href|src|placeholder|value|disabled|required|readonly|checked|selected|name|method|body|cache|credentials|action|target|rel|alt|title|role|width|height|viewBox|viewbox|fill|stroke|x|y|cx|cy|r|x1|y1|x2|y2|d|points|stroke-width|stroke-linecap|stroke-linejoin|stroke-dasharray|opacity|fill-opacity|stroke-opacity|rx|ry|text-anchor|dominant-baseline|font-size|font-weight|font-family|scene|camera|renderer|min|max|step|pattern|tabindex|autocomplete|autofocus|multiple|accept|capture|enctype|for|poster|controls|autoplay|muted|loop|preload|playsinline|loading|decoding|active-class|aria-label|aria-describedby|aria-hidden|data-jtml-dropzone|data-jtml-media-controller|data-jtml-chart|data-jtml-chart-data|data-jtml-chart-by|data-jtml-chart-value|data-jtml-chart-values|data-jtml-chart-series|data-jtml-chart-color|data-jtml-chart-colors|data-jtml-chart-axis-x|data-jtml-chart-axis-y|data-jtml-chart-legend|data-jtml-chart-grid|data-jtml-chart-stacked|data-jtml-chart-min|data-jtml-chart-max|data-jtml-chart-ticks|data-jtml-chart-annotations|data-jtml-chart-export|data-jtml-timeline|data-jtml-timeline-duration|data-jtml-timeline-easing|data-jtml-timeline-animates|data-jtml-image-proc|data-jtml-image-src|data-jtml-image-into|data-jtml-scene3d|data-jtml-scene|data-jtml-camera|data-jtml-controls|data-jtml-renderer|data-jtml-scene3d-controller|data-jtml-fetch|data-url|data-method|data-timeout-ms|data-retry|data-stale|data-group|data-cache-key-expr|data-revalidate-ms|data-dedupe|data-background|data-lazy|data-jtml-route-load|data-jtml-route-guard|data-jtml-guard-var|data-jtml-guard-redirect|data-jtml-invalidate-action|data-jtml-invalidate-fetches|data-jtml-invalidate-groups|data-jtml-invalidate-all)\b/, token: "jtml-attr" },
    /* JTML keywords */
    { regex: /\b(?:define|const|derive|show|if|else|while|for|in|break|continue|try|except|then|return|throw|async|subscribe|unsubscribe|to|from|store|unbind|object|derives|import|main|jtml|jtl|let|get|when|make|page|route|layout|load|guard|require|slot|fetch|catch|finally|use|export|effect|redirect|refresh|invalidate|timeout|retry|stale|lazy|keep|group|key|cache-key|cacheKey|dedupe|every|revalidate|background|extern|html|css|raw|into|link|navlink|text|box|image|video|audio|embed|file|dropzone|canvas|svg|graphic|bar|dot|line|path|polyline|polygon|svgtext|chart|scene3d|item|list|ordered|theme|app|shell|topbar|sidebar|content|panel|card|metric|stack|cluster|split|toolbar|tabs|tab|alert|badge|modal|drawer|toast|loading|error|empty|field|spacer|cols|gap|pad|radius|shadow|tone|align|justify|width|surface|timeline|animate|resize|crop|filter|axis|legend|grid|stacked|values|series|colors|min|max|ticks|annotate|duration|easing|autoplay|repeat|activeRoute|activeRouteName)\b/, token: "jtml-kw" },
    /* Literals */
    { regex: /\b(?:true|false)\b/, token: "atom" },
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

theme
  color primary "#0f766e"
  color background "#f6f7fb"
  radius md 12
  shadow md "0 12px 32px rgba(15,23,42,.10)"

style
  main
    font-family: system-ui
    max-width: 620px
    margin: 48px auto
    padding: 0 20px
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
  panel title "Counter" pad lg shadow md
    stack gap md
      grid cols 2 gap md
        metric "Count" count "Current value" tone primary
        metric "Doubled" doubled "Derived from count" tone good
      toolbar
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
let plan = "Pro"
let accepted = false
let submitted = false
let status = "Ready."
let subscribers = 128
get label = submitted ? "Subscribed to {plan}. Check {email}." : "Enter your email to subscribe."
get canSubmit = email != "" && accepted
get audience = subscribers + 1

when submit
  if canSubmit
    submitted = true
    status = "Subscription saved locally."
    subscribers += 1
  else
    status = "Add an email and accept the terms first."

when resetForm
  email = ""
  plan = "Pro"
  accepted = false
  submitted = false
  status = "Ready."

theme
  color primary "#0f766e"
  color background "#f6f7fb"
  radius md 12
  shadow md "0 12px 32px rgba(15,23,42,.10)"

style
  main
    font-family: system-ui
    max-width: 760px
    margin: 48px auto
    padding: 0 20px
  input, select
    padding: 10px 12px
    border: 1px solid #d8d4c8
    border-radius: 6px
    font-size: 14px
    width: 100%
    box-sizing: border-box
  .terms
    display: flex
    align-items: center
    gap: 8px
  .terms input
    width: auto
  button
    padding: 11px 20px
    background: white
    color: #111827
    border: 1px solid #d8d4c8
    border-radius: 6px
    font-size: 14px
    cursor: pointer
    font-weight: 600
  .primary
    background: #0f766e
    color: white
    border-color: #0f766e

page
  panel title "Newsletter" pad lg shadow md
    stack gap md
      text label
      grid cols 3 gap md
        metric "Subscribers" subscribers "current" tone primary
        metric "Plan" plan "selected" tone good
        metric "Audience" audience "after save" tone warn
      field
        text "Email"
        input type "text" placeholder "you@example.com" into email required
      field
        text "Plan"
        select into plan
          option "Free" value "Free"
          option "Pro" value "Pro" selected
          option "Enterprise" value "Enterprise"
      label class "terms"
        checkbox into accepted
        text "I accept product updates."
      toolbar
        button "Subscribe" class "primary" click submit
        button "Reset" type "button" click resetForm
      alert "{status}" tone good
      if submitted
        alert "Saved subscriber: {email} on {plan}." tone primary`
  },
  {
    name: "dashboard.jtml",
    label: "Dashboard",
    category: "basics",
    code: `jtml 2

let revenue = 124
let customers = 320
let trials = 42
let health = 87
let incidents = 2
let report = [{ "month": "Jan", "mrr": 92 }, { "month": "Feb", "mrr": 106 }, { "month": "Mar", "mrr": 118 }, { "month": "Apr", "mrr": 124 }]
get avg = revenue * 1000 / customers
get conversion = customers / trials
get score = health > 90 ? "Excellent" : "Needs attention"
get runway = revenue - incidents * 12

when addTrial
  trials += 5
  health -= 1

when closeDeal
  customers += 3
  revenue += 8
  health += 2

when reduceChurn
  health += 4
  incidents -= 1

when resetQuarter
  revenue = 124
  customers = 320
  trials = 42
  health = 87
  incidents = 2

theme
  color primary "#0f766e"
  color background "#f6f7fb"
  radius md 12
  shadow md "0 12px 32px rgba(15,23,42,.10)"

style
  main
    font-family: system-ui
    max-width: 920px
    margin: 40px auto
    padding: 0 16px
  button
    padding: 10px 18px
    background: white
    color: #111827
    border: 1px solid #d8d4c8
    border-radius: 7px
    font-size: 14px
    cursor: pointer
    font-weight: 600
  .primary
    background: #0f766e
    color: white
    border-color: #0f766e

page
  stack gap lg
    panel title "Dashboard" pad lg shadow md
      grid cols 4 gap md
        metric "Revenue" "USD {revenue}k" "MRR" tone primary
        metric "Customers" customers "Active" tone good
        metric "Avg / customer" avg "USD" tone warn
        metric "Open trials" trials "Pipeline"
        metric "Health" "{health}%" score tone good
        metric "Incidents" incidents "Open" tone danger
        metric "Conversion" conversion "Customers / trial"
        metric "Runway" runway "Score"
      alert "Close deals to grow revenue, reduce churn to improve health, and reset to replay the quarter." tone primary
      chart bar data report by month value mrr label "MRR" color "#0f766e" axis x label "Month" axis y label "MRR ($k)" grid legend width "760" height "260"
      toolbar
        button "Add trials" click addTrial
        button "Close deal" class "primary" click closeDeal
        button "Reduce churn" click reduceChurn
        button "Reset quarter" click resetQuarter`
  },
  {
    name: "fetch.jtml",
    label: "Fetch",
    category: "data",
    code: `jtml 2

// Fetches /api/users (or any JSON endpoint).
// Swap the URL for a real endpoint in your project.
let saved = false
let users = fetch "/api/users" timeout 2500 retry 2 stale keep group people key "users" dedupe every 30000 refresh reloadUsers
let teams = fetch "/api/teams" group people lazy

when saveLocal
  saved = true
  invalidate group people

when reloadEverything
  invalidate all

when clearSaved
  saved = false

theme
  color primary "#0f766e"
  color background "#f6f7fb"
  radius md 12
  shadow md "0 12px 32px rgba(15,23,42,.10)"

style
  main
    font-family: system-ui
    max-width: 680px
    margin: 48px auto
    padding: 0 20px
  button
    padding: 8px 14px
    background: white
    border: 1px solid #d8d4c8
    border-radius: 6px
    font-size: 13px
    cursor: pointer
    font-weight: 700
  .primary
    background: #0f766e
    color: white
    border-color: #0f766e
  .meta
    color: #64707a
    font-size: 13px

page
  panel title "Users" pad lg shadow md
    stack gap md
      p class "meta" "Endpoint: /api/users · stale keep · timeout 2500ms · retry 2"
      toolbar
        button "Reload users" class "primary" click reloadUsers
        button "Save + invalidate group" click saveLocal
        button "Invalidate all" click reloadEverything
        button "Clear saved flag" click clearSaved
      metric "Saved flag" saved "Invalidates users" tone primary
      if users.loading
        loading "Loading users..."
      if saved
        alert "Saved locally; users fetch was invalidated." tone good
      grid cols 2 gap md
        for user in users.data
          card
            strong "{user.name}"
            p "{user.email}"
      if users.error
        error "Error: {users.error}"`
  },
  {
    name: "fetch-post.jtml",
    label: "POST fetch",
    category: "data",
    code: `jtml 2

let email = "ada@example.com"
let note = "Ready to send."
let login = fetch "/api/login" method "POST" body { email: email } cache "no-store" credentials "include" timeout 2500 retry 2 stale keep refresh retryLogin

when useValid
  email = "ada@example.com"
  note = "Valid request prepared."

when useInvalid
  email = "not-an-email"
  note = "Invalid request prepared; retry to see server validation."

when clearRequest
  email = ""
  note = "Empty request prepared."

style
  main
    font-family: system-ui
    max-width: 640px
    margin: 48px auto
    padding: 0 20px
    display: grid
    gap: 14px
  h1
    margin: 0
  .toolbar
    display: flex
    flex-wrap: wrap
    gap: 10px
  input
    padding: 10px 12px
    border: 1px solid #d8d4c8
    border-radius: 6px
    min-width: 280px
    box-sizing: border-box
  button
    padding: 9px 16px
    background: white
    border: 1px solid #d8d4c8
    border-radius: 6px
    cursor: pointer
    font-weight: 700
  .primary
    background: #0f766e
    color: white
    border-color: #0f766e
  .notice
    padding: 10px 12px
    background: #f8fafc
    border: 1px solid #e2e8f0
    border-radius: 8px
  .error
    color: #b42318
    background: #fef2f2
    border: 1px solid #fecaca
    padding: 10px 12px
    border-radius: 8px

page
  h1 "Login request"
  p class "notice" note
  input type "text" placeholder "ada@example.com" into email
  box class "toolbar"
    button "POST login" class "primary" click retryLogin
    button "Valid example" click useValid
    button "Invalid email" click useInvalid
    button "Empty body value" click clearRequest
  if login.loading
    p "Sending request…"
  else
    p "Request finished."
  if login.data.user
    p "Signed in"
    p "{login.data.user.name}"
    p "{login.data.user.email}"
  if login.error
    p class "error" "Server rejected request: {login.error}"`
  },
  {
    name: "store.jtml",
    label: "Store",
    category: "data",
    code: `jtml 2

let name = ""

store auth
  let user = ""
  let role = "viewer"
  let sessions = 0
  let loggedIn = false

  when login
    let user = name
    let role = "admin"
    let sessions = sessions + 1
    let loggedIn = true

  when logout
    let user = ""
    let role = "viewer"
    let loggedIn = false

theme
  color primary "#0f766e"
  color background "#f6f7fb"
  radius md 12
  shadow md "0 12px 32px rgba(15,23,42,.10)"

style
  main
    font-family: system-ui
    max-width: 680px
    margin: 48px auto
    padding: 0 20px
  input
    padding: 10px 14px
    border: 1px solid #d1cfc9
    border-radius: 6px
    font-size: 14px
    width: 100%
    box-sizing: border-box
  button
    justify-self: start
    padding: 9px 16px
    border-radius: 6px
    cursor: pointer
    font-size: 14px

page
  panel title "Auth Store" pad lg shadow md
    stack gap md
      alert "Store state is shared and keeps its own actions." tone primary
      grid cols 3 gap md
        metric "User" auth.user "shared field" tone primary
        metric "Role" auth.role "store-owned" tone good
        metric "Sessions" auth.sessions "opened" tone warn
      if auth.loggedIn
        card tone good
          alert "Signed in as {auth.user}" tone good
          text "Role: {auth.role}"
          text "Sessions opened: {auth.sessions}"
          toolbar
            button "Logout" click auth.logout
      else
        card
          field
            text "Name"
            input type "text" placeholder "Your name" into name
          text "Next role: admin"
          toolbar
            button "Login" click auth.login`
  },
  {
    name: "effects.jtml",
    label: "Effects",
    category: "data",
    code: `jtml 2

let count = 0
let last = "No changes yet."
let direction = "idle"
let saves = 0

effect count
  last = "Count changed to {count}"
  if count > 0
    direction = "positive"
  else
    direction = "zero or negative"

when increment
  count += 1

when decrement
  count -= 1

when saveSnapshot
  saves += 1
  last = "Snapshot {saves}: count {count}"

theme
  color primary "#0f766e"
  color background "#f6f7fb"
  radius md 12
  shadow md "0 12px 32px rgba(15,23,42,.10)"

style
  main
    font-family: system-ui
    max-width: 680px
    margin: 48px auto
    padding: 0 20px
  button
    padding: 10px 16px
    border-radius: 6px
    cursor: pointer
    font-size: 14px

page
  panel title "Effects" pad lg shadow md
    stack gap md
      grid cols 3 gap md
        metric "Count" count "watched by effect" tone primary
        metric "Direction" direction "derived by effect" tone good
        metric "Snapshots" saves "manual saves" tone warn
      alert "Last effect: {last}" tone good
      toolbar
        button "+ Increment" click increment
        button "- Decrement" click decrement
        button "Save snapshot" click saveSnapshot`
  },
  {
    name: "routes.jtml",
    label: "Routes",
    category: "navigation",
    code: `jtml 2

// active-class highlights the current nav link
// guard blocks protected routes when token is empty

let token = "demo-token"
let users = fetch "/api/users" lazy stale keep
let routeMode = "hash router"

guard "/dashboard" require token else "/login"

theme
  color primary "#0f766e"
  color background "#f6f7fb"
  radius md 12
  shadow md "0 12px 32px rgba(15,23,42,.10)"

make Nav
  style
    .shell
      font-family: system-ui
      max-width: 720px
      margin: 44px auto
      padding: 0 20px
    nav
      display: flex
      gap: 12px
      flex-wrap: wrap
    nav a
      color: #0f766e
      text-decoration: none
      padding: 8px 12px
      border: 1px solid #99f6e4
      border-radius: 999px
    nav a.active
      background: #ccfbf1
      font-weight: 600
  nav
    link "Home" to "/" active-class "active"
    link "Dashboard" to "/dashboard" active-class "active"
    link "Login" to "/login" active-class "active"

make Home
  page class "shell"
    stack gap md
      Nav
      panel title "Home" pad lg shadow md
        stack gap md
          text "Routes render components without a page refresh."
          alert "Mode: {routeMode}. Token is prefilled so the protected route opens." tone primary
          link "Open dashboard" to "/dashboard"

make Dashboard
  page class "shell"
    stack gap md
      Nav
      panel title "Dashboard" pad lg shadow md
        stack gap md
          alert "Protected route: token is present." tone good
          if users.loading
            loading "Loading dashboard data..."
          grid cols 2 gap md
            for user in users.data
              card
                text "{user.name}"
          link "Back home" to "/"

make Login
  page class "shell"
    stack gap md
      Nav
      panel title "Login" pad lg shadow md
        stack gap md
          alert "If token were empty, the guard would redirect /dashboard here." tone warn
          link "Back home" to "/"

make NotFound
  page class "shell"
    stack gap md
      Nav
      panel title "Not found" pad lg shadow md
        empty "No route matched the current path."

route "/" as Home
route "/dashboard" as Dashboard load users
route "/login" as Login
route "*" as NotFound`
  },
  {
    name: "redirect.jtml",
    label: "Redirect",
    category: "navigation",
    code: `jtml 2

when openDashboard
  redirect "/dashboard"

theme
  color primary "#0f766e"
  color background "#f6f7fb"
  radius md 12
  shadow md "0 12px 32px rgba(15,23,42,.10)"

make Home
  page style "font-family: system-ui; max-width: 560px; margin: 48px auto; padding: 0 20px"
    panel title "Welcome" pad lg shadow md
      stack gap md
        text "This action navigates without a full page load."
        toolbar
          button "Open dashboard" click openDashboard

make Dashboard
  page style "font-family: system-ui; max-width: 560px; margin: 48px auto; padding: 0 20px"
    panel title "Dashboard" pad lg shadow md
      stack gap md
        alert "Redirect landed here through route state." tone good
        link "Back home" to "/"

make NotFound
  page style "font-family: system-ui; max-width: 560px; margin: 48px auto; padding: 0 20px"
    panel title "Not found" pad lg shadow md
      stack gap md
        empty "No route matched the current path."
        link "Home" to "/"

route "/" as Home
route "/dashboard" as Dashboard
route "*" as NotFound`
  },
  {
    name: "media.jtml",
    label: "Media",
    category: "media",
    code: `jtml 2

let selectedImage = ""
let assets = []
let revenue = [{ "month": "Jan", "total": 12 }, { "month": "Feb", "total": 18 }, { "month": "Mar", "total": 9 }]
let sampleImage = "data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 640 360'%3E%3Crect width='640' height='360' fill='%23ecfeff'/%3E%3Ccircle cx='180' cy='180' r='96' fill='%230f766e'/%3E%3Crect x='300' y='110' width='220' height='140' rx='24' fill='%232563eb'/%3E%3Ctext x='320' y='190' font-family='system-ui' font-size='42' fill='white'%3EJTML%3C/text%3E%3C/svg%3E"

when clearMedia
  selectedImage = ""
  assets = []

theme
  color primary "#0f766e"
  color background "#f6f7fb"
  radius md 12
  shadow md "0 12px 32px rgba(15,23,42,.10)"

style
  main
    font-family: system-ui
    max-width: 920px
    margin: 40px auto
    padding: 0 20px
  .hero
    grid-column: 1 / -1
    display: grid
    grid-template-columns: minmax(220px, 1fr) minmax(220px, 1fr)
    gap: 16px
    align-items: center
  input
    max-width: 100%
    min-width: 0
    box-sizing: border-box
    padding: 12px
    border: 1px dashed #7a8b8b
    border-radius: 8px
  input[data-jtml-drag="over"]
    border-color: #0f766e
    background: #ecfdf5
  img
    max-width: 100%
    width: 100%
    border-radius: 8px
    border: 1px solid #e8e4dc
  button
    justify-self: start
    padding: 9px 14px
    border: 1px solid #d8d4c8
    border-radius: 6px
    background: white
    cursor: pointer
    font-weight: 700

page
  panel title "Media" pad lg shadow md
    stack gap md
      alert "Files become previewable JTML state; images, video, audio, canvas, and SVG stay standards-based." tone primary
      grid cols 2 gap md
        card class "hero"
          stack gap md
            h2 "Image pipeline"
            text "Upload an image to turn the file into state with name and preview URL."
            file "Choose image" accept "image/*" into selectedImage
            toolbar
              button "Clear media state" click clearMedia
          stack gap md
            if selectedImage
              alert "Selected: {selectedImage.name}" tone good
              image src selectedImage.preview alt selectedImage.name
            else
              text "Fallback preview"
              image src sampleImage alt "Generated JTML preview"
        card
          h2 "Dropzone"
          dropzone "Drop media files" accept "image/*,video/*,audio/*" into assets
          metric "Assets" assets.length "selected" tone primary
          for asset in assets
            if asset.preview
              image src asset.preview alt asset.name
            else
              text asset.name
        card
          h2 "Media controllers"
          text "Video and audio elements expose controller state with into."
          text "Use video src mediaUrl controls into player, then call player.play, player.pause, or player.seek."
          alert "Studio avoids bundling heavy demo media, so uploads are the live path here." tone warn
        card
          h2 "Graphic"
          graphic aria-label "Simple bars" width "320" height "120" viewBox "0 0 320 120"
            bar x "20" y "40" width "70" height "60" fill "#0f766e"
            bar x "120" y "20" width "70" height "80" fill "#2563eb"
            bar x "220" y "60" width "70" height "40" fill "#9333ea"
            dot cx "255" cy "48" r "10" fill "#111827"
            line x1 "20" y1 "104" x2 "300" y2 "104" stroke "#475569" stroke-width "2"
            path d "M20 92 C90 20 180 120 300 40" fill "none" stroke "#9333ea" stroke-width "3"
            svgtext x "20" y "24" fill "#334155" font-size "14" "Revenue"
        card
          h2 "Chart"
          chart bar data revenue by month value total label "Revenue by month" color "#2563eb" axis x label "Month" axis y label "Revenue ($k)" grid legend
        card
          h2 "3D scene"
          scene3d "Interactive product scene" scene productScene camera orbit controls orbit renderer "three" width "640" height "360"
          text "3D hosts can attach Three.js/WebGPU through window.jtml3d.render(canvas, spec)."`
  },
  {
    name: "charts.jtml",
    label: "Charts",
    category: "media",
    code: `jtml 2

let revenue = [{ "month": "Jan", "sales": 12, "expenses": 8 }, { "month": "Feb", "sales": 18, "expenses": 10 }, { "month": "Mar", "sales": 9, "expenses": 7 }, { "month": "Apr", "sales": 24, "expenses": 14 }, { "month": "May", "sales": 30, "expenses": 16 }, { "month": "Jun", "sales": 22, "expenses": 11 }]

let traffic = [{ "day": "Mon", "visits": 420, "signups": 18 }, { "day": "Tue", "visits": 680, "signups": 28 }, { "day": "Wed", "visits": 540, "signups": 21 }, { "day": "Thu", "visits": 790, "signups": 34 }, { "day": "Fri", "visits": 610, "signups": 25 }, { "day": "Sat", "visits": 320, "signups": 12 }, { "day": "Sun", "visits": 210, "signups": 8 }]

theme
  color primary "#0f766e"
  color background "#f6f7fb"
  radius md 12
  shadow md "0 12px 32px rgba(15,23,42,.10)"

style
  main
    font-family: system-ui
    max-width: 860px
    margin: 40px auto
    padding: 0 24px
  h2
    margin: 0 0 12px
    font-size: 15px
    color: #334155
  svg
    overflow: visible

page
  panel title "Charts" pad lg shadow md
    stack gap md
      alert "JTML charts render to accessible SVG. Bar and line charts support axes, labels, legends, grid lines, grouped series, and stacked bars." tone primary
      grid cols 2 gap md
        card
          h2 "Grouped revenue"
          chart bar data revenue by month values sales expenses series "Sales,Expenses" colors "#0f766e,#b42318" label "Revenue split" axis x label "Month" axis y label "$k" grid legend max 40 ticks 5 annotate "Launch" at "Apr" value sales color "#9333ea" export svg png csv width "380" height "240"
        card
          h2 "Multi-series trend"
          chart line data traffic by day values visits signups series "Visits,Signups" colors "#2563eb,#9333ea" label "Traffic and signups" axis x label "Day" axis y label "Count" grid legend annotate "Peak" at "Thu" value visits export svg csv width "380" height "240"
        card
          h2 "Stacked revenue"
          chart bar data revenue by month values sales expenses series "Sales,Expenses" colors "#0f766e,#b42318" label "Stacked revenue" axis x label "Month" axis y label "$k" grid legend stacked max 50 ticks 6 width "380" height "240"
        card
          h2 "Single-series baseline"
          chart line data traffic by day value visits label "Visit trend" color "#475569" grid axis x label "Day of week" width "380" height "240"`
  },
  {
    name: "animation.jtml",
    label: "Animation",
    category: "media",
    code: `jtml 2

let progress = 0
let opacity = 0
let scale = 0
let playing = false

theme
  color primary "#0f766e"
  color background "#f6f7fb"
  radius md 12
  shadow md "0 12px 32px rgba(15,23,42,.10)"

when advance
  if progress < 100
    progress += 25
    opacity += 25
    scale += 25
    playing = true

when resetMotion
  progress = 0
  opacity = 0
  scale = 0
  playing = false

when completeMotion
  progress = 100
  opacity = 100
  scale = 100
  playing = false

style
  main
    font-family: system-ui
    max-width: 640px
    margin: 40px auto
    padding: 0 24px
  button
    padding: 8px 18px
    border: 1px solid #d1d5db
    border-radius: 6px
    background: white
    font: inherit
    cursor: pointer
    font-weight: 600
  button:hover
    background: #f1f5f9
  .bar-track
    height: 20px
    background: #f1f5f9
    border-radius: 10px
    overflow: hidden
  .bar-fill
    height: 100%
    background: #0f766e
    border-radius: 10px
    transition: width .2s ease-out
  .orb
    width: 96px
    height: 96px
    border-radius: 999px
    background: #0f766e
    transition: all .2s ease-out

page
  panel title "Animation" pad lg shadow md
    stack gap md
      alert "JTML drives visible state. CSS owns the transition details." tone primary
      grid cols 3 gap md
        metric "Progress" progress "percent" tone primary
        metric "Opacity" opacity "percent" tone neutral
        metric "Scale" scale "pixels" tone good
      card
        stack gap md
          h2 "Motion preview"
          box class "bar-track"
            box class "bar-fill" style "width: {progress}%"
          box class "orb" aria-label "Animated preview" style "opacity: {opacity}%; width: {scale}px; height: {scale}px"
          toolbar
            button "Advance" click advance
            button "Complete" click completeMotion
            button "Reset" click resetMotion
      card
        h2 "State-driven CSS"
        if playing
          alert "Motion is in progress." tone good
        else
          alert "Motion is idle." tone neutral`
  },
  {
    name: "components.jtml",
    label: "Components",
    category: "composition",
    code: `jtml 2

make Counter label
  let count = 0

  when add
    count += 1

  when reset
    count = 0

  card title label
    stack gap md
      metric "Count" count "local state" tone primary
      toolbar
        button "+ Increment" click add
        button "Reset" click reset

theme
  color primary "#0f766e"
  color background "#f6f7fb"
  radius md 12
  shadow md "0 12px 32px rgba(15,23,42,.10)"

style
  main
    font-family: system-ui
    max-width: 620px
    margin: 48px auto
    padding: 0 20px
  button
    padding: 9px 16px
    border-radius: 6px
    font-size: 14px
    cursor: pointer

page
  panel title "Component Isolation" pad lg shadow md
    stack gap md
      alert "Each Counter call receives its own local state and actions." tone primary
      grid cols 2 gap md
        Counter "Counter A"
        Counter "Counter B"`
  }
];

/* ═══════════════════════════════════════════════════════════
   App state
═══════════════════════════════════════════════════════════ */
const $ = id => document.getElementById(id);
let mode            = "file";
let activeFileIdx   = 0;
let activeLessonIdx = -1;
let activeDocIdx    = -1;
let activeWorkspacePath = "";
let lessons         = [];
let docs            = [];
let completedSlugs  = new Set(JSON.parse(localStorage.getItem("jtml:completed") || "[]"));
let dirty           = false;
let loading         = false;
let proseOpen       = false;
let sidebarOpen     = true;
let bottomOpen      = true;
let inspectorMode   = "diagnostics";
let artifactMode    = "html";
let lastArtifacts   = { classic: "", html: "" };
let sidebarQuery    = "";
let workspaceCreateKind = "file";
let draftTimer      = 0;
let paletteItems    = [];
let paletteActive   = 0;

/* ═══════════════════════════════════════════════════════════
   Layout system
═══════════════════════════════════════════════════════════ */
const PORT = location.port || "80";
const LAYOUT_KEY = "jtml:layout:" + PORT;
const DEF_LAYOUT = { sidebarW: 210, editorW: null, bottomH: 185, proseH: 200 };
let L = Object.assign({}, DEF_LAYOUT, JSON.parse(localStorage.getItem(LAYOUT_KEY) || "{}"));

function clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)); }

function applyLayout() {
  const isHub = mode === "home";
  $("hub-panel").hidden = !isHub;
  $("work-row").style.display = isHub ? "none" : "flex";
  $("rh-editor").style.display = isHub ? "none" : "";
  $("bottom-row").style.display = isHub ? "none" : "flex";
  $("rh-bottom").style.display = isHub ? "none" : "";
  if (isHub) {
    $("prose-panel").style.height = "0";
    $("rh-prose").style.display = "none";
    $("lesson-after-panel").hidden = true;
  }

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
  bottomRow.style.height = (!isHub && bottomOpen) ? L.bottomH + "px" : "0";
  $("rh-bottom").style.visibility = bottomOpen ? "" : "hidden";
  $("bottom-toggle").classList.toggle("active", !bottomOpen);
  $("bottom-toggle").innerHTML = bottomOpen ? "&#x229F;" : "&#x229E;";

  /* Prose height */
  if (!isHub) {
    $("prose-panel").style.height = proseOpen ? L.proseH + "px" : "0";
    $("rh-prose").style.display   = proseOpen ? "" : "none";
  }

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

/* Keyboard shortcuts for panel toggles and command palette */
document.addEventListener("keydown", e => {
  if ((e.metaKey || e.ctrlKey) && !e.shiftKey && !e.altKey) {
    if (e.key === "b") { e.preventDefault(); $("sidebar-toggle").click(); }
    if (e.key === "j") { e.preventDefault(); $("bottom-toggle").click(); }
    if (e.key.toLowerCase() === "k") { e.preventDefault(); openPalette(); }
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
  extraKeys: {
    "Cmd-Enter": () => run(),
    "Ctrl-Enter": () => run(),
    "Cmd-K": () => openPalette(),
    "Ctrl-K": () => openPalette(),
  },
});
editor.setSize("100%", "100%");
editor.on("change", () => {
  if (!loading) {
    dirty = true;
    updateMeta();
    scheduleDraftSave();
  }
});
editor.on("cursorActivity", updateCursor);

/* ═══════════════════════════════════════════════════════════
   Storage helpers
═══════════════════════════════════════════════════════════ */
function latestKey(name)   { return "jtml:latest:"   + PORT + ":" + name; }
function versionsKey(name) { return "jtml:versions:" + PORT + ":" + name; }
function draftKey(name)    { return "jtml:draft:"    + PORT + ":" + name; }
function lessonKey(slug)   { return "jtml:lesson:"   + PORT + ":" + slug; }
const WORKSPACE_KEY = "jtml:workspace:" + PORT;

function defaultWorkspace() {
  return {
    projects: [{
      type: "project",
      name: "starter-app",
      children: [
        { type: "file", name: "index.jtml", code: SAMPLES[0].code },
        { type: "folder", name: "app", children: [
          { type: "file", name: "dashboard.jtml", code: SAMPLES.find(s => s.name === "dashboard.jtml").code },
          { type: "file", name: "routes.jtml", code: SAMPLES.find(s => s.name === "routes.jtml").code },
        ]},
        { type: "folder", name: "docs", children: [
          { type: "doc", name: "Language reference", slug: "language-reference" },
          { type: "doc", name: "AI authoring contract", slug: "ai-authoring-contract" },
        ]},
      ],
    }]
  };
}
function loadWorkspace() {
  try {
    const parsed = JSON.parse(localStorage.getItem(WORKSPACE_KEY) || "null");
    if (parsed && Array.isArray(parsed.projects)) return parsed;
  } catch {}
  const fresh = defaultWorkspace();
  localStorage.setItem(WORKSPACE_KEY, JSON.stringify(fresh));
  return fresh;
}
let workspace = loadWorkspace();
function saveWorkspace() {
  localStorage.setItem(WORKSPACE_KEY, JSON.stringify(workspace));
}

function pathParts(path) {
  return String(path || "").split("/").map(p => p.trim()).filter(Boolean);
}
function workspacePathFor(projectName, parts) {
  return [projectName].concat(parts || []).join("/");
}
function findWorkspaceNode(path) {
  const parts = pathParts(path);
  if (!parts.length) return null;
  let node = (workspace.projects || []).find(p => p.name === parts[0]);
  for (let i = 1; node && i < parts.length; i++) {
    node = (node.children || []).find(child => child.name === parts[i]);
  }
  return node || null;
}
function ensureWorkspaceFolder(project, parts) {
  let node = project;
  for (const part of parts) {
    node.children = node.children || [];
    let next = node.children.find(child => child.type === "folder" && child.name === part);
    if (!next) {
      next = { type: "folder", name: part, children: [] };
      node.children.push(next);
    }
    node = next;
  }
  return node;
}
function setWorkspaceCreateError(message) {
  $("workspace-create-error").textContent = message || "";
}
function openWorkspaceCreate(kind) {
  workspaceCreateKind = kind;
  const defaults = {
    project: ["New project", "my-jtml-app"],
    folder: ["New folder", "starter-app/app/components"],
    file: ["New file", "starter-app/app/page.jtml"],
  };
  const [title, value] = defaults[kind] || defaults.file;
  $("workspace-create-title").textContent = title;
  $("workspace-create-path").value = value;
  setWorkspaceCreateError("");
  $("workspace-create").hidden = false;
  $("workspace-create-path").focus();
  $("workspace-create-path").select();
}
function closeWorkspaceCreate() {
  $("workspace-create").hidden = true;
  setWorkspaceCreateError("");
}
function submitWorkspaceCreate() {
  const raw = ($("workspace-create-path").value || "").trim();
  if (workspaceCreateKind === "project") createWorkspaceProject(raw);
  else if (workspaceCreateKind === "folder") createWorkspaceFolder(raw);
  else createWorkspaceFile(raw);
}
function createWorkspaceProject(name) {
  name = (name || "").trim();
  if (!name) return;
  if ((workspace.projects || []).some(p => p.name === name)) {
    setWorkspaceCreateError("A project with that name already exists.");
    return;
  }
  workspace.projects.push({
    type: "project",
    name,
    children: [{ type: "file", name: "index.jtml", code: SAMPLES[0].code }],
  });
  saveWorkspace();
  closeWorkspaceCreate();
  renderSidebar();
}
function createWorkspaceFolder(raw) {
  raw = (raw || "").trim();
  const parts = pathParts(raw);
  if (parts.length < 2) return;
  const project = (workspace.projects || []).find(p => p.name === parts[0]);
  if (!project) {
    setWorkspaceCreateError("Start the path with an existing project name.");
    return;
  }
  ensureWorkspaceFolder(project, parts.slice(1));
  saveWorkspace();
  closeWorkspaceCreate();
  renderSidebar();
}
function createWorkspaceFile(raw) {
  raw = (raw || "").trim();
  const parts = pathParts(raw);
  if (parts.length < 2) return;
  const project = (workspace.projects || []).find(p => p.name === parts[0]);
  if (!project) {
    setWorkspaceCreateError("Start the path with an existing project name.");
    return;
  }
  const fileName = parts[parts.length - 1];
  const folder = ensureWorkspaceFolder(project, parts.slice(1, -1));
  folder.children = folder.children || [];
  if (folder.children.some(child => child.name === fileName)) {
    setWorkspaceCreateError("A file or folder already exists at that path.");
    return;
  }
  folder.children.push({
    type: "file",
    name: fileName,
    code: fileName.endsWith(".jtml") ? SAMPLES[0].code : "# Notes\n\nUse .jtml files for runnable Studio documents.\n",
  });
  saveWorkspace();
  closeWorkspaceCreate();
  renderSidebar();
  selectWorkspaceFile(raw);
}

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
function getDraftByName(name) {
  try {
    const raw = localStorage.getItem(draftKey(name));
    if (!raw) return null;
    const draft = JSON.parse(raw);
    return draft && typeof draft.code === "string" ? draft : null;
  } catch {
    return null;
  }
}
function hasDraftByName(name) { return !!getDraftByName(name); }
function clearDraftByName(name) { localStorage.removeItem(draftKey(name)); }
function isFriendlySource(code) {
  return /^\s*jtml\s+2\b/.test(String(code || ""));
}
function isClassicSource(code) {
  const text = String(code || "");
  return !isFriendlySource(text) && /(^|\n)\s*(define|function|@|element)\b/.test(text);
}
function saveDraftNow() {
  if (mode === "home") return;
  const name = activeDocName();
  const code = editor.getValue();
  localStorage.setItem(draftKey(name), JSON.stringify({
    code,
    savedAt: new Date().toISOString(),
    hash: hashText(code),
    chars: code.length,
    lines: lineCount(code),
  }));
  const st = $("status");
  if (st && st.className !== "err") {
    st.textContent = "draft saved";
    st.className = "run";
  }
  updateMeta();
}
function scheduleDraftSave() {
  clearTimeout(draftTimer);
  draftTimer = setTimeout(saveDraftNow, 600);
}
function flushPendingDraft() {
  if (!draftTimer) return;
  clearTimeout(draftTimer);
  draftTimer = 0;
  saveDraftNow();
}
function pushVersionByName(name, code) {
  let vs = getVersionsByName(name);
  const hash = hashText(code);
  localStorage.setItem(latestKey(name), code);
  clearDraftByName(name);
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
  if (mode === "home") return "studio:home";
  if (mode === "file") return SAMPLES[activeFileIdx].name;
  if (mode === "workspace") return "workspace:" + activeWorkspacePath;
  if (mode === "lesson" && activeLessonIdx >= 0) return "lesson:" + lessons[activeLessonIdx].slug;
  return "untitled";
}
function activeDisplayName() {
  if (mode === "home") return "Studio home";
  if (mode === "file") return SAMPLES[activeFileIdx].name;
  if (mode === "workspace") return activeWorkspacePath || "workspace file";
  if (mode === "lesson" && activeLessonIdx >= 0) return lessons[activeLessonIdx].slug + "/code.jtml";
  return "untitled";
}
function loadFileCode(s)  {
  const draft = getDraftByName(s.name);
  if (draft && (!isClassicSource(draft.code) || !isFriendlySource(s.code))) return draft.code;
  if (draft && isClassicSource(draft.code) && isFriendlySource(s.code)) clearDraftByName(s.name);
  const latest = localStorage.getItem(latestKey(s.name));
  if (latest && (!isClassicSource(latest) || !isFriendlySource(s.code))) return latest;
  if (latest && isClassicSource(latest) && isFriendlySource(s.code)) localStorage.removeItem(latestKey(s.name));
  return s.code;
}
function pushVersion(s, code) {
  return pushVersionByName(s.name, code);
}
function loadLessonCode(l) {
  const name = "lesson:" + l.slug;
  const draft = getDraftByName(name);
  if (draft) return draft.code;
  return localStorage.getItem(latestKey("lesson:" + l.slug)) ||
         localStorage.getItem(legacyLessonLatestKey(l.slug)) ||
         l.code;
}
function saveLessonCode(l, code) {
  return pushVersionByName("lesson:" + l.slug, code);
}
function loadWorkspaceFileCode(path) {
  const name = "workspace:" + path;
  const node = findWorkspaceNode(path);
  const draft = getDraftByName(name);
  if (draft) return draft.code;
  return localStorage.getItem(latestKey(name)) || (node && node.code) || "";
}
function saveWorkspaceFileCode(path, code) {
  const node = findWorkspaceNode(path);
  if (node && node.type === "file") {
    node.code = code;
    saveWorkspace();
  }
  return pushVersionByName("workspace:" + path, code);
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

let pendingConfirm = null;
function closeConfirm(value) {
  $("confirm-modal").hidden = true;
  const done = pendingConfirm;
  pendingConfirm = null;
  if (done) done(Boolean(value));
}
function confirmAction(opts) {
  return new Promise(resolve => {
    pendingConfirm = resolve;
    $("confirm-title").textContent = opts.title || "Are you sure?";
    $("confirm-hint").textContent = opts.hint || activeDisplayName();
    $("confirm-message").textContent = opts.message || "This action will change the current Studio state.";
    $("confirm-summary").textContent = opts.summary || "";
    $("confirm-accept").textContent = opts.confirmText || "Continue";
    $("confirm-accept").className = "btn " + (opts.danger ? "danger" : "primary");
    $("confirm-modal").hidden = false;
    requestAnimationFrame(() => $("confirm-accept").focus());
  });
}
async function guardedAction(opts, action) {
  const ok = await confirmAction(opts);
  if (ok) return action();
}

function renderArtifacts() {
  $("artifact-classic-tab").classList.toggle("active", artifactMode === "classic");
  $("artifact-html-tab").classList.toggle("active", artifactMode === "html");
  const value = artifactMode === "classic" ? lastArtifacts.classic : lastArtifacts.html;
  if (value) {
    $("artifact-code").textContent = value;
  } else {
    $("artifact-code").innerHTML =
      '<span class="artifact-empty">Run the current file to inspect generated HTML and compatibility IR.</span>';
  }
}
function setInspectorMode(modeName) {
  inspectorMode = modeName;
  $("bottom-row").dataset.mode = modeName;
  $("inspector-diagnostics").classList.toggle("active", modeName === "diagnostics");
  $("inspector-artifacts").classList.toggle("active", modeName === "artifacts");
  $("inspector-reference").classList.toggle("active", modeName === "reference");
}
function setArtifacts(data) {
  lastArtifacts = {
    classic: data && data.classic ? data.classic : "",
    html: data && data.html ? data.html : "",
  };
  renderArtifacts();
}
function updateCursor() {
  const c = editor.getCursor();
  $("cursor-pos").textContent = "Ln " + (c.line + 1) + ", Col " + (c.ch + 1);
}
function updateMeta() {
  const text  = editor.getValue();
  const lines = text ? text.split("\n").length : 0;
  const draft = mode !== "home" && hasDraftByName(activeDocName());
  $("editor-meta").textContent = lines + "L" + (draft ? " · draft" : "");
  const dialect = $("dialect-badge");
  if (dialect) {
    const friendly = isFriendlySource(text);
    dialect.textContent = friendly ? "Friendly JTML 2" : "Compatibility backend";
    dialect.className = "dialect-badge" + (friendly ? "" : " classic");
  }
  /* Dirty dot in name */
  const base = mode === "home"
    ? "Studio home"
    : mode === "file"
    ? SAMPLES[activeFileIdx].name
    : mode === "workspace"
    ? activeWorkspacePath
    : (lessons[activeLessonIdx] ? lessons[activeLessonIdx].slug + "/code.jtml" : "");
  $("active-name").textContent = base + (dirty ? " ●" : "");
}

function renderSidebar() {
  const query = sidebarQuery.trim().toLowerCase();
  const matches = text => !query || String(text || "").toLowerCase().includes(query);
  $("hub-nav").className = "sb-item" + (mode === "home" ? " active-hub" : "");
  renderProjectTree(query);
  /* Files grouped by category */
  const fl = $("file-list");
  fl.innerHTML = "";
  const categoryLabels = { basics: "Basics", data: "Data & State", navigation: "Navigation", media: "Media & Graphics", composition: "Composition" };
  const seen = {};
  const pinnedTemplates = new Set(["counter.jtml", "form.jtml", "dashboard.jtml", "fetch.jtml", "routes.jtml", "media.jtml", "components.jtml"]);
  SAMPLES.forEach((s, i) => {
    if (!query && !pinnedTemplates.has(s.name)) return;
    if (!matches((s.label || "") + " " + s.name + " " + (s.category || ""))) return;
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
    btn.innerHTML = `<span class="sb-text"><span class="sb-main">${esc(label)}</span><span class="sb-sub">${esc(s.name)}</span></span>${vs.length ? `<span class="v-badge">v${vs.length}</span>` : ""}`;
    btn.onclick = () => selectFile(i);
    fl.appendChild(btn);
  });
  if (!fl.children.length) fl.innerHTML = '<p class="no-items">No examples match.</p>';
  /* Lessons */
  const ll = $("lesson-list");
  if (!lessons.length) {
    ll.innerHTML = '<p class="no-items">No tutorial/ found.<br>Run from repo root.</p>';
    return;
  }
  ll.innerHTML = "";
  lessons.forEach((l, i) => {
    if (!matches((i + 1) + " " + l.title + " " + l.slug)) return;
    const vs = getVersionsByName("lesson:" + l.slug);
    const btn = document.createElement("button");
    btn.className = "sb-item" + (mode === "lesson" && i === activeLessonIdx ? " active-lesson" : "");
    const chk = completedSlugs.has(l.slug) ? '<span class="check-ok">&#x2713;</span>' : "";
    const badge = vs.length ? `<span class="v-badge">v${vs[0].version}</span>` : "";
    btn.innerHTML = `<span class="sb-text"><span class="sb-main">${esc((i + 1) + ". " + l.title)}</span><span class="sb-sub">${esc(l.slug + "/code.jtml")}</span></span>${badge}${chk}`;
    btn.onclick = () => selectLesson(i);
    ll.appendChild(btn);
  });
  if (!ll.children.length) ll.innerHTML = '<p class="no-items">No lessons match.</p>';

  const dl = $("doc-list");
  dl.innerHTML = "";
  const docSeen = {};
  docs.forEach((d, i) => {
    if (!matches(d.title + " " + d.slug + " " + (d.category || ""))) return;
    const cat = d.category || "Guide";
    if (!docSeen[cat]) {
      docSeen[cat] = true;
      const hdr = document.createElement("p");
      hdr.className = "sb-category";
      hdr.textContent = cat;
      dl.appendChild(hdr);
    }
    const btn = document.createElement("button");
    btn.className = "sb-item" + (i === activeDocIdx ? " active-doc" : "");
    btn.innerHTML = `<span class="sb-text"><span class="sb-main">${esc(d.title)}</span><span class="sb-sub">${esc(d.slug)}</span></span>`;
    btn.onclick = () => openDoc(i);
    dl.appendChild(btn);
  });
  if (!dl.children.length) dl.innerHTML = '<p class="no-items">No docs match.</p>';
}

function renderProjectTree(query) {
  const root = $("project-tree");
  root.innerHTML = "";
  const matchesTree = text => !query || String(text || "").toLowerCase().includes(query);
  function renderNode(node, projectName, parts, parent) {
    const fullPath = workspacePathFor(projectName, parts.concat(node.name));
    if (node.type === "folder") {
      const group = document.createElement("div");
      group.className = "tree-group";
      const label = document.createElement("div");
      label.className = "tree-folder";
      label.textContent = node.name;
      const children = document.createElement("div");
      children.className = "tree-children";
      (node.children || []).forEach(child => renderNode(child, projectName, parts.concat(node.name), children));
      if (children.children.length || matchesTree(fullPath)) {
        group.appendChild(label);
        group.appendChild(children);
        parent.appendChild(group);
      }
      return;
    }
    if (node.type === "doc") {
      if (!matchesTree(fullPath + " " + (node.slug || ""))) return;
      const btn = document.createElement("button");
      btn.className = "sb-item";
      btn.innerHTML = `<span class="sb-text"><span class="sb-main">${esc(node.name)}</span><span class="sb-sub">docs/${esc(node.slug || "")}</span></span>`;
      btn.onclick = () => {
        const i = docIndexBySlug(node.slug);
        if (i >= 0) {
          if (mode === "home") selectWorkspaceFile(findFirstWorkspaceFile() || "");
          openDoc(i);
        }
      };
      parent.appendChild(btn);
      return;
    }
    if (!matchesTree(fullPath)) return;
    const vs = getVersionsByName("workspace:" + fullPath);
    const btn = document.createElement("button");
    btn.className = "sb-item" + (mode === "workspace" && fullPath === activeWorkspacePath ? " active-file" : "");
    btn.innerHTML = `<span class="sb-text"><span class="sb-main">${esc(node.name)}</span><span class="sb-sub">${esc(fullPath)}</span></span>${vs.length ? `<span class="v-badge">v${vs[0].version}</span>` : ""}`;
    btn.onclick = () => selectWorkspaceFile(fullPath);
    parent.appendChild(btn);
  }
  (workspace.projects || []).forEach(project => {
    const group = document.createElement("div");
    group.className = "tree-group";
    const label = document.createElement("div");
    label.className = "tree-folder";
    label.textContent = project.name;
    const children = document.createElement("div");
    children.className = "tree-children";
    (project.children || []).forEach(child => renderNode(child, project.name, [], children));
    if (children.children.length || matchesTree(project.name)) {
      group.appendChild(label);
      group.appendChild(children);
      root.appendChild(group);
    }
  });
  if (!root.children.length) {
    root.innerHTML = '<p class="sb-empty-note">No local project files match. Create a project, folder, or file above.</p>';
  }
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

function setToolButtonsEnabled(enabled) {
  ["run", "lint", "fix", "format", "export", "save", "reset", "history-btn"].forEach(id => {
    const el = $(id);
    if (el) el.disabled = !enabled;
  });
}
function currentSourceIsJtml() {
  return mode !== "workspace" || activeWorkspacePath.endsWith(".jtml");
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
  $("lesson-after-panel").hidden = true;
  applyLayout();
  setTimeout(() => $("prose-panel").classList.remove("collapsing"), 220);
}

function renderMarkdown(md) {
  return typeof marked !== "undefined" ? marked.parse(md || "") : "<p>" + esc(md || "") + "</p>";
}

function splitLessonProse(md) {
  const marker = "<!-- studio:playground -->";
  const at = (md || "").indexOf(marker);
  if (at < 0) return { intro: md || "", after: "" };
  return {
    intro: md.slice(0, at).trim(),
    after: md.slice(at + marker.length).trim()
  };
}

function setLessonFollowup(markdown) {
  $("lesson-after-body").innerHTML = renderMarkdown(markdown);
  $("lesson-after-panel").hidden = !(markdown && markdown.trim());
}

function showHub() {
  flushPendingDraft();
  mode = "home";
  activeDocIdx = -1;
  proseOpen = false;
  dirty = false;
  setToolButtonsEnabled(false);
  $("status").textContent = "home";
  $("status").className = "";
  $("issue-count").textContent = "";
  $("diag-sum").textContent = "ready";
  updateMeta();
  updateHubStats();
  renderSidebar();
  applyLayout();
}

function openWorkMode() {
  $("hub-panel").hidden = true;
  setToolButtonsEnabled(true);
}

function findFirstWorkspaceFile() {
  for (const project of workspace.projects || []) {
    const found = findFirstFileInNode(project, project.name, []);
    if (found) return found;
  }
  return "";
}
function findFirstFileInNode(node, projectName, parts) {
  for (const child of node.children || []) {
    const nextParts = parts.concat(child.name);
    if (child.type === "file") return workspacePathFor(projectName, nextParts);
    if (child.type === "folder") {
      const found = findFirstFileInNode(child, projectName, nextParts);
      if (found) return found;
    }
  }
  return "";
}

function selectWorkspaceFile(path) {
  const node = findWorkspaceNode(path);
  if (!node || node.type !== "file") return;
  flushPendingDraft();
  mode = "workspace";
  activeWorkspacePath = path;
  activeDocIdx = -1;
  openWorkMode();
  $("history-btn").style.display = "";
  closeProse();
  setCode(loadWorkspaceFileCode(path));
  setArtifacts({});
  dirty = hasDraftByName("workspace:" + path);
  if (dirty) { $("status").textContent = "draft restored"; $("status").className = "run"; }
  updateMeta();
  renderSidebar();
  if (path.endsWith(".jtml")) run();
  else {
    $("status").textContent = "notes file";
    $("status").className = "";
    $("preview").srcdoc = "<!doctype html><body style='font:16px system-ui;padding:24px;white-space:pre-wrap'>" + esc(editor.getValue()) + "</body>";
  }
}

function selectFile(i) {
  flushPendingDraft();
  mode = "file";
  activeFileIdx = i;
  const docName = SAMPLES[i].name;
  const draft = getDraftByName(docName);
  const restoredDraft = !!(draft && (!isClassicSource(draft.code) || !isFriendlySource(SAMPLES[i].code)));
  activeDocIdx = -1;
  openWorkMode();
  $("history-btn").style.display = "";
  closeProse();
  setCode(loadFileCode(SAMPLES[i]));
  setArtifacts({});
  dirty = restoredDraft;
  if (restoredDraft) { $("status").textContent = "draft restored"; $("status").className = "run"; }
  updateMeta();
  renderSidebar();
  run();
}

async function selectLesson(i) {
  if (i < 0 || i >= lessons.length) return;
  flushPendingDraft();
  mode = "lesson";
  activeLessonIdx = i;
  activeDocIdx = -1;
  openWorkMode();
  $("history-btn").style.display = "";

  const l = lessons[i];
  try {
    const res  = await fetch("/api/lesson/" + encodeURIComponent(l.slug));
    const data = await res.json();
    l.prose = data.prose || "";
    l.code  = data.code  || "";
  } catch {}

  const restoredDraft = hasDraftByName("lesson:" + l.slug);
  const lessonProse = splitLessonProse(l.prose || "");
  $("prose-body").innerHTML = renderMarkdown(lessonProse.intro);
  setLessonFollowup(lessonProse.after);
  $("prev").disabled = i === 0;
  $("next").disabled = i === lessons.length - 1;
  $("lesson-ctr").textContent = "Lesson " + (i + 1) + " of " + lessons.length;
  openProse();

  setCode(loadLessonCode(l));
  setArtifacts({});
  dirty = restoredDraft;
  if (restoredDraft) { $("status").textContent = "draft restored"; $("status").className = "run"; }
  updateMeta();
  renderSidebar();
  run();
}

async function openDoc(i) {
  if (i < 0 || i >= docs.length) return;
  if (mode === "home") selectFile(0);
  activeDocIdx = i;
  const d = docs[i];
  try {
    const res = await fetch("/api/doc/" + encodeURIComponent(d.slug));
    const data = await res.json();
    d.prose = data.prose || "";
  } catch {}
  $("prose-body").innerHTML = renderMarkdown(d.prose || "");
  setLessonFollowup("");
  $("prev").disabled = true;
  $("next").disabled = true;
  $("lesson-ctr").textContent = d.category || "Guide";
  openProse();
  renderSidebar();
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
  setInspectorMode("diagnostics");
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
    const loc = d.line ? `<span class="diag-loc">:${esc(d.line)}${d.column ? ":" + esc(d.column) : ""}</span>` : "";
    const code = d.code ? `<span class="diag-code">${esc(d.code)}</span>` : "";
    const hint = d.hint ? `<div class="diag-hint">${esc(d.hint)}</div>` : "";
    const example = d.example ? `<pre class="diag-example">${esc(d.example)}</pre>` : "";
    return `<li class="${isE ? "d-err" : "d-warn"}">` +
      `<span>${isE ? "&#x2715;" : "&#x26A0;"}</span>` +
      `<span class="diag-main"><span class="diag-head">${code}${loc}<span>${esc(d.message)}</span></span>${hint}${example}</span></li>`;
  }).join("");
}

function showFixChanges(changes) {
  const cs = changes || [];
  if (!cs.length) {
    showDiagnostics({ diagnostics: [] });
    return;
  }
  $("diag-sum").textContent = cs.length + " safe fix" + (cs.length > 1 ? "es" : "");
  $("diag-sum").className = "hint ok";
  $("issue-count").textContent = "";
  $("diag-list").innerHTML = cs.map(c => {
    const loc = c.line ? `<span class="diag-loc">:${esc(c.line)}</span>` : "";
    return `<li class="d-ok"><span>&#x2713;</span><span class="diag-main"><span class="diag-head"><span class="diag-code">${esc(c.code || "JTML_FIX")}</span>${loc}<span>${esc(c.message || "Applied safe fix.")}</span></span></span></li>`;
  }).join("");
}

function showRuntimeStatus(message, state) {
  const text = String(message || "").trim();
  if (!text) return;
  const st = $("status");
  const isError = state === "error";
  st.textContent = isError ? "runtime error" : "runtime " + state;
  st.className = isError ? "err" : "run";
  if (isError) {
    showDiagnostics({
      diagnostics: [{
        severity: "error",
        code: "JTML_RUNTIME",
        message: text,
        hint: "This came from the live preview runtime. Re-run after fixing the event/action path."
      }]
    });
  }
}

window.addEventListener("message", (event) => {
  const data = event && event.data;
  if (!data || data.type !== "jtml:runtime-status") return;
  showRuntimeStatus(data.message, data.state);
});

/* ═══════════════════════════════════════════════════════════
   Actions
═══════════════════════════════════════════════════════════ */
async function run() {
  const st = $("status");
  if (!currentSourceIsJtml()) {
    $("preview").srcdoc = "<!doctype html><body style='font:16px system-ui;padding:24px;white-space:pre-wrap'>" + esc(editor.getValue()) + "</body>";
    st.textContent = "notes preview";
    st.className = "";
    return;
  }
  st.textContent = "running…"; st.className = "run";
  $("run").disabled = true;
  try {
    const data = await api("/api/run", { code: editor.getValue() });
    if (data.ok) {
      $("preview").srcdoc = data.html;
      setArtifacts(data);
      st.textContent = "ready"; st.className = "ok";
      showDiagnostics(data);
    } else {
      $("preview").srcdoc =
        `<pre style="padding:16px;color:#b42318;white-space:pre-wrap;font:13px/1.5 monospace">${esc(data.error || "error")}</pre>`;
      st.textContent = "error"; st.className = "err";
      showDiagnostics(data.diagnostics ? data : { diagnostics: [{ severity: "error", message: data.error || "" }] });
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
    saveDraftNow();
    st.textContent = "formatted"; st.className = "ok";
    showDiagnostics({ diagnostics: [] });
  } else {
    st.textContent = "format error"; st.className = "err";
    showDiagnostics(data.diagnostics ? data : { diagnostics: [{ severity: "error", message: data.error || "" }] });
  }
}

async function fixSource() {
  const st = $("status");
  st.textContent = "fixing…"; st.className = "run";
  $("fix").disabled = true;
  try {
    const data = await api("/api/fix", { code: editor.getValue() });
    if (data.ok) {
      if (data.changed) {
        loading = true; editor.setValue(data.code); loading = false;
        dirty = true; updateMeta();
        saveDraftNow();
      }
      st.textContent = data.changed ? "fixed" : "no fixes needed";
      st.className = "ok";
      showFixChanges(data.changes || []);
    } else {
      st.textContent = "fix error"; st.className = "err";
      showDiagnostics(data.diagnostics ? data : { diagnostics: [{ severity: "error", message: data.error || "" }] });
    }
  } catch (e) {
    st.textContent = String(e); st.className = "err";
  } finally {
    $("fix").disabled = false;
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
      showDiagnostics(data.diagnostics ? data : { diagnostics: [{ severity: "error", message: data.error || "" }] });
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
      setArtifacts(data);
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
      showDiagnostics(data.diagnostics ? data : { diagnostics: [{ severity: "error", message: data.error || "" }] });
    }
  } catch (e) {
    st.textContent = String(e); st.className = "err";
  } finally {
    $("export").disabled = false;
  }
}

function saveSource() {
  clearTimeout(draftTimer);
  draftTimer = 0;
  const code = editor.getValue();
  const st   = $("status");
  if (mode === "file") {
    const s = SAMPLES[activeFileIdx];
    const result = pushVersion(s, code);
    dirty = false; updateMeta(); renderSidebar();
    st.textContent = result.saved ? "saved v" + result.version : "already saved v" + result.version;
    st.className = "ok";
  } else if (mode === "workspace") {
    const result = saveWorkspaceFileCode(activeWorkspacePath, code);
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
  clearDraftByName(activeDocName());
  const st = $("status");
  if (mode === "file") {
    loading = true; editor.setValue(SAMPLES[activeFileIdx].code); editor.clearHistory(); loading = false;
    dirty = false; updateMeta();
    st.textContent = "reset"; st.className = "";
  } else if (mode === "workspace") {
    const node = findWorkspaceNode(activeWorkspacePath);
    loading = true; editor.setValue((node && node.code) || ""); editor.clearHistory(); loading = false;
    dirty = false; updateMeta();
    st.textContent = "reset to saved file"; st.className = "";
  } else if (mode === "lesson" && activeLessonIdx >= 0) {
    const l = lessons[activeLessonIdx];
    // Clear saved versions too so re-loading this lesson shows the original code
    localStorage.removeItem(latestKey("lesson:" + l.slug));
    localStorage.removeItem(legacyLessonLatestKey(l.slug));
    loading = true; editor.setValue(l.code || ""); editor.clearHistory(); loading = false;
    dirty = false; updateMeta();
    st.textContent = "reset to original"; st.className = "";
  }
  if (mode === "workspace" && !activeWorkspacePath.endsWith(".jtml")) {
    $("preview").srcdoc = "<!doctype html><body style='font:16px system-ui;padding:24px;white-space:pre-wrap'>" + esc(editor.getValue()) + "</body>";
  } else {
    run();
  }
}

$("run").onclick    = run;
$("lint").onclick   = lintSource;
$("fix").onclick    = () => guardedAction({
  title: "Apply safe fixes?",
  hint: activeDisplayName(),
  message: "Studio will run the JTML fixer and may rewrite the current editor contents.",
  summary: "A draft is saved automatically after fixes are applied. Use version history if you need to compare snapshots.",
  confirmText: "Apply fixes",
}, fixSource);
$("format").onclick = formatSource;
$("export").onclick = () => guardedAction({
  title: "Export compiled HTML?",
  hint: activeDisplayName(),
  message: "Studio will compile the current JTML source and download the generated HTML artifact.",
  summary: "Run Lint first if you want a clean diagnostics pass before handing the file to someone else.",
  confirmText: "Export HTML",
}, exportHtml);
$("save").onclick   = () => guardedAction({
  title: "Save this version?",
  hint: activeDisplayName(),
  message: "Studio will store a local version snapshot for the current file or lesson.",
  summary: "Snapshots are kept in this browser's local storage, with the newest version first.",
  confirmText: "Save version",
}, saveSource);
$("reset").onclick  = () => guardedAction({
  title: "Reset current source?",
  hint: activeDisplayName(),
  message: "Studio will discard the current local draft and restore the bundled source.",
  summary: "Saved versions remain in history, but unsaved editor changes will be replaced.",
  confirmText: "Reset source",
  danger: true,
}, resetSource);
$("home-btn").onclick = showHub;
$("learn-btn").onclick = () => selectLesson(0);
$("docs-btn").onclick = () => {
  if (docs.length) {
    if (mode === "home") selectWorkspaceFile(findFirstWorkspaceFile() || "");
    openDoc(0);
  }
};
$("reference-btn").onclick = () => {
  const i = docIndexBySlug("language-reference");
  if (i >= 0) {
    if (mode === "home") selectWorkspaceFile(findFirstWorkspaceFile() || "");
    openDoc(i);
  }
};
$("playground-btn").onclick = () => selectWorkspaceFile(findFirstWorkspaceFile() || "");
$("brand-home").onclick = showHub;
$("hub-nav").onclick = showHub;
$("new-project").onclick = () => openWorkspaceCreate("project");
$("new-folder").onclick = () => openWorkspaceCreate("folder");
$("new-file").onclick = () => openWorkspaceCreate("file");
$("workspace-create-confirm").onclick = submitWorkspaceCreate;
$("workspace-create-cancel").onclick = closeWorkspaceCreate;
$("workspace-create-path").onkeydown = e => {
  if (e.key === "Enter") submitWorkspaceCreate();
  if (e.key === "Escape") closeWorkspaceCreate();
};
$("sidebar-search").oninput = e => {
  sidebarQuery = e.target.value || "";
  renderSidebar();
};
$("artifact-classic-tab").onclick = () => { artifactMode = "classic"; setInspectorMode("artifacts"); renderArtifacts(); };
$("artifact-html-tab").onclick    = () => { artifactMode = "html"; setInspectorMode("artifacts"); renderArtifacts(); };
$("inspector-diagnostics").onclick = () => setInspectorMode("diagnostics");
$("inspector-artifacts").onclick = () => setInspectorMode("artifacts");
$("inspector-reference").onclick = () => setInspectorMode("reference");

function sampleIndexByName(name) {
  return SAMPLES.findIndex(s => s.name === name || (s.label || "").toLowerCase() === String(name).toLowerCase());
}
function docIndexBySlug(slug) {
  return docs.findIndex(d => d.slug === slug);
}
function updateHubStats() {
  $("hub-example-count").textContent = String(SAMPLES.length);
  $("hub-lesson-count").textContent = String(lessons.length);
  $("hub-doc-count").textContent = String(docs.length);
}
document.addEventListener("click", e => {
  const sampleBtn = e.target.closest("[data-open-sample]");
  if (sampleBtn) {
    const i = sampleIndexByName(sampleBtn.dataset.openSample);
    if (i >= 0) selectFile(i);
    return;
  }
  const lessonBtn = e.target.closest("[data-open-lesson]");
  if (lessonBtn) {
    const i = Number(lessonBtn.dataset.openLesson);
    if (Number.isFinite(i)) selectLesson(i);
    return;
  }
  const docBtn = e.target.closest("[data-open-doc]");
  if (docBtn) {
    const i = docIndexBySlug(docBtn.dataset.openDoc);
    if (i >= 0) {
      if (mode === "home") selectWorkspaceFile(findFirstWorkspaceFile() || "");
      openDoc(i);
    }
  }
});

/* ═══════════════════════════════════════════════════════════
   Command palette
═══════════════════════════════════════════════════════════ */
function commandItems() {
  const items = [
    { kind: "command", title: "Studio home", sub: "Return to the main hub", run: showHub },
    { kind: "command", title: "Run current file", sub: "Compile, lint, and refresh preview", run: run },
    { kind: "command", title: "Lint current file", sub: "Show parser and linter diagnostics", run: lintSource },
    { kind: "command", title: "Fix current file", sub: "Apply safe source repairs", run: fixSource },
    { kind: "command", title: "Format current file", sub: "Rewrite with canonical JTML formatter", run: formatSource },
    { kind: "command", title: "Save snapshot", sub: "Create a versioned local snapshot", run: saveSource },
    { kind: "command", title: "Export HTML", sub: "Download generated HTML", run: exportHtml },
    { kind: "command", title: "Toggle sidebar", sub: "Show or hide Explorer", run: () => $("sidebar-toggle").click() },
    { kind: "command", title: "Toggle inspector", sub: "Show or hide Diagnostics, Artifacts, Reference", run: () => $("bottom-toggle").click() },
  ];
  SAMPLES.forEach((s, i) => items.push({
    kind: "example",
    title: s.label || s.name,
    sub: s.name + " · " + (s.category || "example"),
    run: () => selectFile(i)
  }));
  lessons.forEach((l, i) => items.push({
    kind: "lesson",
    title: (i + 1) + ". " + l.title,
    sub: l.slug + "/code.jtml",
    run: () => selectLesson(i)
  }));
  docs.forEach((d, i) => items.push({
    kind: "doc",
    title: d.title,
    sub: (d.category || "Guide") + " · " + d.slug,
    run: () => { if (mode === "home") selectWorkspaceFile(findFirstWorkspaceFile() || ""); openDoc(i); }
  }));
  return items;
}

function renderPalette() {
  const q = $("palette-search").value.trim().toLowerCase();
  paletteItems = commandItems().filter(item => {
    const hay = (item.kind + " " + item.title + " " + item.sub).toLowerCase();
    return !q || hay.includes(q);
  }).slice(0, 80);
  if (paletteActive >= paletteItems.length) paletteActive = 0;
  const list = $("palette-list");
  if (!paletteItems.length) {
    list.innerHTML = '<li class="palette-empty">No command, example, lesson, or doc matched.</li>';
    return;
  }
  list.innerHTML = paletteItems.map((item, i) => `
    <li>
      <button class="palette-item ${i === paletteActive ? "active" : ""}" data-palette-index="${i}">
        <span class="palette-title">${esc(item.title)}</span>
        <span class="palette-kind">${esc(item.kind)}</span>
        <span class="palette-sub">${esc(item.sub || "")}</span>
      </button>
    </li>`).join("");
  list.querySelectorAll("[data-palette-index]").forEach(btn => {
    btn.onclick = () => runPaletteItem(Number(btn.dataset.paletteIndex));
  });
}

function runPaletteItem(i) {
  const item = paletteItems[i];
  if (!item) return;
  closePalette();
  item.run();
}

function openPalette() {
  $("palette-modal").hidden = false;
  paletteActive = 0;
  $("palette-search").value = "";
  renderPalette();
  requestAnimationFrame(() => $("palette-search").focus());
}

function closePalette() { $("palette-modal").hidden = true; }

$("palette-search").oninput = () => { paletteActive = 0; renderPalette(); };
$("palette-close").onclick = closePalette;
$("palette-bg").onclick = closePalette;
$("confirm-close").onclick = () => closeConfirm(false);
$("confirm-cancel").onclick = () => closeConfirm(false);
$("confirm-bg").onclick = () => closeConfirm(false);
$("confirm-accept").onclick = () => closeConfirm(true);
document.addEventListener("keydown", e => {
  if (!$("confirm-modal").hidden) {
    if (e.key === "Escape") {
      e.preventDefault();
      closeConfirm(false);
    } else if (e.key === "Enter") {
      e.preventDefault();
      closeConfirm(true);
    }
    return;
  }
  if ($("palette-modal").hidden) return;
  if (e.key === "ArrowDown") {
    e.preventDefault();
    paletteActive = paletteItems.length ? (paletteActive + 1) % paletteItems.length : 0;
    renderPalette();
  } else if (e.key === "ArrowUp") {
    e.preventDefault();
    paletteActive = paletteItems.length ? (paletteActive - 1 + paletteItems.length) % paletteItems.length : 0;
    renderPalette();
  } else if (e.key === "Enter") {
    e.preventDefault();
    runPaletteItem(paletteActive);
  }
});

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
document.addEventListener("keydown", e => {
  if (e.key === "Escape") {
    closeHistory();
    closePalette();
  }
});

/* ═══════════════════════════════════════════════════════════
   Boot
═══════════════════════════════════════════════════════════ */
async function boot() {
  try {
    const res = await fetch("/api/lessons");
    if (res.ok) lessons = await res.json();
  } catch {}
  try {
    const res = await fetch("/api/docs");
    if (res.ok) docs = await res.json();
  } catch {}
  updateHubStats();
  renderSidebar();
  /* Apply layout after first paint so getBoundingClientRect returns real values */
  requestAnimationFrame(() => {
    applyLayout();
    if (lessons.length) selectLesson(0);
    else selectWorkspaceFile(findFirstWorkspaceFile() || "");
  });
}
boot();
</script>
</body>
</html>
)HTML";

} // namespace jtml::cli
