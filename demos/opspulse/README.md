# OpsPulse - Production-Style JTML Demo

OpsPulse is the modular enterprise demo for JTML. It uses one canonical
entrypoint, `index.jtml`, and composes the app from exported stores, route
pages, layout components, reusable UI components, and local assets.

## Project Structure

```
opspulse/
├── index.jtml                    # Routes, theme tokens, global app styles
├── assets/
│   └── ops-workspace.svg         # Local visual asset used by the Media route
├── stores/
│   ├── app-state.jtml            # Exported appState store
│   ├── filter-state.jtml         # Exported filterState store
│   └── ops-data.jtml             # Exported opsData demo data store
├── components/
│   ├── layouts/
│   │   └── shell.jtml            # Exported Shell layout
│   └── ui/
│       ├── app-header.jtml       # Exported AppHeader
│       ├── app-sidebar.jtml      # Exported AppSidebar
│       ├── filter-panel.jtml     # Exported FilterPanel
│       └── common.jtml           # Exported shared UI blocks
└── pages/
    ├── dashboard.jtml
    ├── usage.jtml
    ├── people.jtml
    ├── incidents.jtml
    ├── releases.jtml
    ├── media.jtml
    ├── settings.jtml
    └── not-found.jtml
```

## Run And Build

Use app-folder mode so assets and package metadata are copied with the build:

```bash
jtml check demos/opspulse/index.jtml
jtml explain demos/opspulse/index.jtml --json
jtml serve demos/opspulse/index.jtml --watch
jtml build demos/opspulse --out dist/opspulse
```

## Module Pattern

Providers export the public API:

```jtml
export store appState
  let currentUser = "João"

export make Shell title
  page
    slot
```

Consumers import exactly what they use:

```jtml
use appState from "../stores/app-state.jtml"
use { PageHero, MetricCard } from "../components/ui/common.jtml"
```

The current compatibility module loader enforces named imports against exports,
keeps private state/action helpers needed by exported declarations, and hides
private components from named importers.

Stores are used as observable shared state. Clickable behavior is currently
implemented as module-owned `when` actions that write into those stores; this is
the reliable production pattern until store-method execution moves into the
core semantic runtime.

## What This Demo Exercises

- Nested relative imports from pages, components, stores, and layout modules.
- Named imports and destructured imports.
- Exported stores and exported components.
- Shared state across routes.
- Route table with fallback route.
- Semantic UI-style layout with polished CSS.
- Forms, inputs, file/dropzone state, image assets, actions, derived values,
  and app-folder builds.

## Current Architecture Note

JTML now exposes core `SemanticProject` / `SemanticModule` records for explain
and future tooling, but execution still lowers modules through a compatibility
compilation unit. The next architecture target is moving module loading and
linking fully into core semantic project ownership.
