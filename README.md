# JTML

JTML is a small C++ runtime and transpiler for a reactive HTML-oriented language.

## Syntax

The canonical source dialect is Friendly JTML 2. Start files with `jtml 2`,
write indentation-based blocks, and use HTML-shaped element names without
closing tags:

```jtml
jtml 2

let count = 0
get doubled = count * 2

when add
  count += 1

page
  h1 "Counter"
  show "Count: {count}"
  text "Doubled: {doubled}"
  button "Add" click add
```

Try it with:

```sh
./build/jtml serve examples/friendly_counter.jtml --port 8000
```

Friendly syntax is the format used by the tutorial, Studio samples, AI authoring
contract, formatter, and most bundled examples. Classic JTML remains supported
as a compatibility/low-level target for existing files and compiler artifacts.

Hyphenated attributes such as `data-page` and `aria-label` are supported.
Keyword-shaped attributes such as `for` are supported in element position, and
boolean attributes can be written naturally:

```jtml
jtml 2

page class "profile" data-page "home"
  label for "email" aria-label "Email"
    show "Email"
  input "Email" id "email" type "email" required
  my-widget data-mode "compact"
    text "Custom element"
```

Browser events use Friendly event sugar in `jtml 2` files: `click`, `input`,
`change`, `submit`, `hover`, `scroll`, `focus`, `blur`, `key-down`, `key-up`,
and `double-click`.

```jtml
let email = ""
let saved = false

when save
  if email != ""
    saved = true

page
  input "Email" into email
  button "Save" click save
  if saved
    text "Saved {email}"
```

Core production features are first-class in Friendly JTML:

```jtml
use Card from "./components/card.jtml"

let users = fetch "/api/users" stale keep retry 2

route "/" as Home
route "/users/:id" as User

make Home
  page
    h1 "Users"
    for user in users.data
      Card user.name

make User id
  page
    h1 "User {id}"
```

Classic syntax is still accepted for compatibility and for generated
compiler artifacts. It uses `define`, `derive`, `function`, `@tag`, `\`, and
`#`:

```jtml
define name = "Alice"\\
derive greeting = "Hello, " + name\\
@main\\
    show greeting\\
#
```

Use Classic only when maintaining old code or inspecting lowered artifacts.

## Reserved Keywords

The language reserves these words:

```text
element show define derive unbind store for if const in break continue throw
else while try except then return function to subscribe from unsubscribe
object derives async import main true false
let get when make page route layout load slot fetch effect use export style
guard redirect refresh invalidate lazy extern into link text box image video
audio embed canvas list item
```

`const` defines immutable variables, `async function` dispatches calls without blocking the caller, `import` evaluates another JTML file in the current runtime, `subscribe` and `unsubscribe` manage variable-change callbacks, and `object ... derives from ...` defines inherited object members.

## Build

```sh
cmake -S . -B build -DJTML_BUILD_PYTHON=OFF
cmake --build build --target jtml_cli
```

The runtime currently expects WebSocket++ and Boost.System to be available. On this machine those headers/libraries are provided by Homebrew under `/opt/homebrew`; see `third_party/` for vendored single-header deps (`nlohmann/json.hpp`, `cpp-httplib`).

## Repository Layout

```
include/jtml/    public headers, one namespace-style include root
src/             implementation files, matching public modules
cli/             `jtml` binary split by command
tests/           CMake/CTest unit tests
third_party/     vendored headers
cmake/           reusable CMake helpers
examples/        runnable `.jtml` programs
tutorial/        lesson content for `jtml tutorial`
editors/vscode/  VS Code extension assets
scripts/         optional integration bindings
docs/            language, deployment, and design documentation
site/            static project website
```

## Run

```sh
./build/jtml transpile examples/user_interactions.jtml -o example.html
./build/jtml serve examples/user_interactions.jtml --port 8000
```

For `jtml serve`, the generated page connects to the runtime WebSocket server on `--port + 80` and falls back to `POST /api/event` if WebSocket is unavailable. The same server exposes `/api/health`, `/api/bindings`, `/api/state`, and `/api/runtime` for IDEs, test runners, and external hosts; see `docs/runtime-http-contract.md`.

## Quick Start For End Users

After building, try the language from the repo root:

```sh
./build/jtml examples
./build/jtml demo --port 8000
```

Then open `http://localhost:8000`. `demo` starts JTML Studio: a real browser editor with Run, Lint, safe Fix, Friendly-preserving Format, Export HTML, diagnostics, versioned local Save/history, compiler artifacts, and live preview.

Check your install:

```sh
./build/jtml --version
```

Create a starter page:

```sh
./build/jtml new my_page.jtml
./build/jtml check my_page.jtml
./build/jtml serve my_page.jtml --port 8000
```

Create a modular app:

```sh
./build/jtml new app my-app
cd my-app
jtml dev . --port 8000
```

Install a local JTML package into an app:

```sh
jtml add ../shared/card.jtml
jtml add ui-kit --from ../packages/ui-kit
```

Packages are copied into `jtml_modules/` and tracked in `jtml.packages.json`.
Friendly imports can use bare package names, for example
`use Card from "card"` or `use Button from "ui-kit"`.

For a live development loop, add `--watch`. The source file is polled and any saved change re-transpiles, hot-swaps the interpreter, and broadcasts a reload to every connected browser — no extension, no script, no config:

```sh
./build/jtml serve my_page.jtml --port 8000 --watch
```

Export for existing apps:

```sh
./build/jtml export my_page.jtml --target html -o dist/index.html
./build/jtml export my_page.jtml --target react -o JtmlApp.jsx
./build/jtml export my_page.jtml --target vue -o JtmlApp.vue
./build/jtml export my_page.jtml --target custom-element -o jtml-app.js
```

Format and lint before committing:

```sh
./build/jtml fix my_page.jtml -w        # safe mechanical repairs
./build/jtml fmt my_page.jtml           # prints canonical source to stdout
./build/jtml fmt my_page.jtml -w        # rewrites the file in place
./build/jtml lint my_page.jtml          # reports undefined vars, unreachable code, etc.
./build/jtml test                       # smoke-tests every bundled example
```

`jtml fix` only applies low-risk source repairs: missing Friendly header, tab indentation, trailing whitespace, and final newline. `jtml fmt` is idempotent. Friendly files (`jtml 2`) are formatted with a source-preserving formatter that keeps high-level constructs such as `fetch`, `store`, and `route`; Classic files use the AST formatter with statements terminated by `\\`. `jtml check --json`, `jtml fix --json`, and `jtml lint --json` emit structured diagnostics for tools and AI agents. `jtml lint` exits non-zero when it finds errors, including obvious primitive type mismatches such as `let count: number = "zero"` and action arity mismatches such as `save(1)` for `when save id label`, so it is safe to use in pre-commit hooks or CI. The current linter is intentionally simple: some dynamic/runtime patterns may still need broader future checks.

Run the editor-neutral language server:

```sh
./build/jtml lsp
```

`jtml lsp` speaks the Language Server Protocol over stdio. It supports diagnostics, formatting, completions, hover, document/workspace symbols, cross-file definition, references, rename, code actions, signature help, document highlights, and selection ranges; see `docs/language-server.md`.

## AI-Native Authoring

JTML is designed to be a small, predictable target for AI-generated and AI-edited web apps. The contract for models and agents lives in `docs/ai-authoring-contract.md`, and the implementation track lives in `docs/ai-native-implementation-roadmap.md`.

Recommended repair loop:

```sh
./build/jtml explain my_page.jtml --json
./build/jtml fix my_page.jtml -w
./build/jtml fmt my_page.jtml -w
./build/jtml check my_page.jtml
./build/jtml lint my_page.jtml
./build/jtml build my_page.jtml --out dist
```

## Interactive Tutorial

The fastest way to learn the language is the built-in tutorial. It serves a split-view IDE in the browser: prose on top, editable code on the left, live preview on the right, and a `Run` button (or `Cmd/Ctrl+Enter`) that re-transpiles the code and hot-swaps the running interpreter.

```sh
./build/jtml tutorial --port 8000
```

Lessons live under `tutorial/<NN-slug>/{lesson.md, code.jtml}`. Adding a new lesson is just adding a new folder — no code changes required. The editor has JTML syntax highlighting, lesson navigation, and a `Cmd/Ctrl+Enter` run shortcut.

## Editor Support

A VS Code extension lives under `editors/vscode/`. It ships Friendly JTML 2 syntax highlighting, CLI-backed diagnostics, formatting, safe fixes, bracket matching, and snippets for app, component, fetch, route, store, effect, style, input, extern, and Classic fallback constructs. The native `jtml lsp` server is now available for editor clients that prefer LSP-powered diagnostics, formatting, completion, hover, and symbols. See `editors/vscode/README.md` for install instructions.

## Media And Graphics

JTML supports standards-based media today with `image`, `video`, `audio`,
`embed`, `file`, `dropzone`, SVG-first `graphic`/`bar`/`dot`/`line`/`path`,
accessible
`chart bar data rows by label value total`, and `scene3d` 3D mount points, plus raw `canvas`/SVG-compatible tags and
explicit `extern` bridges for host-provided graphics or media libraries.
`video/audio ... into name` exposes playback state and client actions such as
`name.play`, `name.pause`, and `name.seek(0)`. `scene3d ... into sceneState`
exposes renderer/fallback state and lets host renderers publish updates through
`spec.update(...)`. Richer charts, 3D adapters, and media processing are tracked
in `docs/media-graphics-roadmap.md` and `docs/3d-custom-interfaces-roadmap.md`.

The roadmap lives in `ROADMAP.md`, and the docs index lives in
`docs/README.md`. The current product lane is production hardening:
Friendly-first app semantics, Studio as the main hub, zero-config tooling,
modular apps, interop, media/graphics, and AI-native authoring.

## Local Toolkit

Everything needed before public deployment is available from the repo:

```sh
./build/jtml serve examples/friendly_counter.jtml --port 8000 --watch
./build/jtml tutorial --port 8000
./build/jtml demo --port 8000
./build/jtml doctor
./build/jtml doctor --json
./build/jtml test
./build/jtml build examples/friendly_import_page.jtml --out dist/app
scripts/verify_all.sh
```

The future public site content is in `site/`, including `site/tools.html` for
the editor, runner, tutorial, studio, tester, and release-tooling overview.

## Install And Package

```sh
cmake --install build --prefix ~/.local
cd build && cpack
```

The documentation set starts at `docs/README.md`. The versioned language
reference is in `docs/language-reference.md`, the deployment guide is in
`docs/deployment.md`, and the predeploy static website for `jtml.org` lives in
`site/`.

Build all local predeploy artifacts:

```sh
scripts/verify_all.sh
```
