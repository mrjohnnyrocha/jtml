# 0. Welcome to JTML

JTML is a small reactive language for HTML pages. You write what the page is, JTML keeps it in sync as state changes.

This is the language's home page. Edit the code on the right and press **Run** to see the page rebuild. The same code is canonical Friendly JTML — what `jtml fmt` produces.

The lessons below walk through the language piece by piece:

- **0. Welcome** — you are here.
- **1. Hello** — `let`, `show`, `page`.
- **2. State** — multiple `let`s and reactive `show`.
- **3. Events** — `when`, `click`, action functions.
- **4. Input** — `into` for two-way binding.
- **5. Collections** — `for`, lists, derived `get` values.
- **6. Effects** — reactive side effects with `effect`.
- **7. Putting it together** — a small interactive page.
- **8. Data fetch** — async GET data and loading/error states.
- **9. Stores** — shared state with namespaced actions.
- **10. Routes** — local SPA navigation, params, and links.
- **11. Components** — reusable UI with isolated instance state.
- **12. Compatibility and tooling** — when to use Friendly JTML 2, when Classic matters, and how Studio/CLI fit together.

When you finish the lessons:

- Read the **AI authoring contract** in `docs/ai-authoring-contract.md`.
- Browse the **example gallery** with `jtml examples`.
- See the **language reference** in `docs/language-reference.md`.
- Format any file with `jtml fmt file.jtml -w`.
- Lint any file with `jtml lint file.jtml`.
- Build a static page with `jtml build file.jtml --out dist`.

The code on the right is itself a working JTML page describing the language. Modify any string and press **Run** to confirm the live runtime is reactive.
