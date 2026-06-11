# OpsPulse - Modularized JTML Demo App

A medium-sized JTML application demonstrating modularization patterns for larger apps.

## Project Structure

```
opspulse/
├── index.jtml                    # Main entry point: routing, pages, global styles
├── stores/                       # Shared application state
│   ├── app-state.jtml           # Global UI state (theme, sidebar, user, alerts)
│   └── filter-state.jtml        # Filter state (regions, platforms, tools, periods)
├── components/
│   ├── layouts/                 # Layout components with slots
│   │   └── shell.jtml           # Main app shell (header + sidebar + content)
│   └── ui/                      # Reusable UI components
│       ├── app-header.jtml      # Top navigation bar
│       ├── app-sidebar.jtml     # Sidebar with nav + filters
│       ├── filter-panel.jtml    # Filter controls
│       └── common.jtml          # Alert, MetricCard, Loading, Error, Empty blocks
└── pages/                        # Page/route components
    └── dashboard.jtml           # Dashboard page
```

## Architecture

### State Management (Stores Over Imports)

Since JTML doesn't have named import filtering yet, **state is managed through stores**:

- `appState`: Global UI state (theme, sidebar, current user, alerts)
- `filterState`: Filter state (regions, platforms, tools)

Components subscribe to stores and re-render on changes:

```jtml
use "../stores/app-state.jtml"

make Dashboard
  if appState.darkMode
    text "Dark mode enabled"
  button "Toggle" click appState.toggleTheme
```

### Component Organization

**Layouts** (`components/layouts/`):
- `Shell`: Main page wrapper with header, sidebar, and content area
- Uses slots for page-specific content

**UI Components** (`components/ui/`):
- `AppHeader`: Top bar with search, theme toggle, user info
- `AppSidebar`: Navigation and filters
- `FilterPanel`: Region/Platform/Tool/Priority filters
- `Common`: Metric cards, loading/error states, alert bars

**Pages** (`pages/`):
- Page components that compose layouts and UI components
- Each page knows how to structure its content

**Stores** (`stores/`):
- `app-state.jtml`: When, get, and derived state for global UI
- `filter-state.jtml`: When, get, and derived state for filtering

### Import Pattern

```jtml
// Side-effect import (loads entire module)
use "../components/ui/common.jtml"

// Use components from the imported module
MetricCard "Label" "Value" "Detail" "primary"
```

Note: Even though you import the whole file, you only use what you need. Naming conventions prevent collisions:
- `UiCard`, `UiButton` → UI primitives
- `FilterPanel`, `FilterBar` → Filter controls
- `MetricCard` → Metric display

## Development

### Run the app

```bash
# Dev with hot-reload
jtml serve opspulse/index.jtml --watch

# Build for browser-local runtime
jtml build opspulse/index.jtml --target browser --out dist
```

### Add a new page

1. Create `pages/new-page.jtml`:
```jtml
use "../stores/app-state.jtml"
use "../components/layouts/shell.jtml"

make NewPage
  Shell "Page title" "OpsPulse"
    fieldset class "panel"
      h2 "Content here"
```

2. Import in `index.jtml` and add route:
```jtml
use "./pages/new-page.jtml"

route "/newpage" as NewPage
```

### Add a new component

1. Create `components/ui/my-component.jtml`
2. Import in the pages that need it:
```jtml
use "../components/ui/my-component.jtml"
```

### Add shared state

1. Create `stores/my-state.jtml`:
```jtml
store myState
  let value = 0
  when increment
    value += 1
```

2. Use in any component:
```jtml
use "../stores/my-state.jtml"

make MyComponent
  text "Value: {myState.value}"
  button "+" click myState.increment
```

## Key Patterns

### 1. File-based Modularity

Organize code by feature/responsibility. Each file has a clear purpose:
- `app-state.jtml` → Global UI state only
- `filter-state.jtml` → Filter state only
- `shell.jtml` → Main layout only
- `dashboard.jtml` → Dashboard page only

### 2. Naming Conventions

Use prefixes to avoid collisions from side-effect imports:
- `AppHeader`, `AppSidebar` → App shell components
- `FilterPanel`, `FilterBar` → Filter components
- `MetricCard`, `EmptyBlock` → Common UI blocks

### 3. Relative Imports

Always use relative paths:
```jtml
use "../stores/app-state.jtml"          # From pages/
use "../../stores/app-state.jtml"       # From components/ui/
use "../ui/common.jtml"                 # From components/layouts/
```

### 4. Stores for Cross-Component State

Don't pass state through imports. Use stores:
```jtml
// Good
use "../stores/app-state.jtml"
button "Logout" click appState.logout

// Avoid
// Can't import "logout" action without loading entire module
```

### 5. Slots for Composition

Use slots in layout components:
```jtml
make Shell title
  page
    header
    slot         # Each page fills this
    footer
```

## Next Steps

1. **Add data fetching**: Update stores to `fetch "/api/data"` and bind in pages
2. **Expand pages**: Implement Usage, People, Incidents pages with real components
3. **Extract UI kit**: Move reusable components to a separate `jtml_modules/ui-kit` package
4. **Add local package**: `jtml add ../my-ui-library` to reuse components across projects
5. **Build for production**: `jtml build . --target browser --out dist` and deploy

## Limitations (First-Slice JTML)

- Named imports not enforced (entire module loads)
- No export filtering yet
- Use naming conventions and file organization to manage namespaces
- All modules execute on import (use `get` and `when` at module level, not inside functions)

See JTML roadmap for planned improvements: [ROADMAP.md](../../ROADMAP.md)
