# JTML Friendly Syntax Grammar And Implementation Plan

Status: **implementation note** — Friendly JTML 2 is the default authoring
syntax. This document records the historical grammar plan and compatibility
strategy; the user-facing reference is `language-reference.md`, and the active
product roadmap is `jtml-competitive-features-roadmap.md`.

Phases 1–6 are complete as a source-normalization and runtime-bridge path.
Later phases remain open where they require deeper architecture, especially
direct non-expanded `ComponentInstance` execution.
Scope: preserve Classic JTML v1, add Friendly JTML v2 as the preferred syntax

Implementation note: the current implementation lives in `src/friendly.cpp` and
normalizes Friendly JTML to Classic JTML before lexing/parsing. This keeps both
syntaxes on the same AST today. Component syntax (`make`, uppercase component
calls, and `slot`) still expands for compatibility, but the runtime now records
component definitions, component instances, per-instance locals/actions, and
component action dispatch metadata. The remaining architectural step is direct
non-expanded component execution.

The element dictionary aliases (`text`, `box`, `list`/`item`, `link`, `image`,
`video`, `audio`, `embed`, `checkbox`, and related form controls) are
implemented. `const`, `while`, destructured `use`, explicit `export`, package
imports, routing, fetch, stores, effects, scoped styles, and extern actions
also have first slices or hardened slices depending on the feature.

## Design Goal

JTML should be comfortable for programmers and readable for non-programmers. The language should avoid framework jargon where possible, but keep a small, stable keyword set that is easy to remember and easy for tools and AI systems to generate.

The core rule:

> The shortest readable form should also be the canonical form.

JTML v2 should feel like HTML without angle brackets, plus simple app behavior:

```jtml
use Button from "./button.jtml"

let count = 0
let label = "Count: {count}"

when add
  count += 1

page class "app"
  h1 "Counter"
  show label
  Button "Add" click add
```

## Compatibility Model

JTML will support two source syntaxes that compile to the same AST.

- Classic syntax: existing `@tag`, `define`, `derive`, `function`, `\`, and `#` grammar.
- Friendly syntax: indentation-based `let`, `when`, `make`, `page`, `show`, `use`.

The compiler should normalize both syntaxes into the same internal nodes:

| Concept | Classic JTML | Friendly JTML | AST Meaning |
| --- | --- | --- | --- |
| import | `import "x.jtml"\` | `use "./x.jtml"` | Import module |
| mutable value | `define count = 0\` | `let count = 0` | Define or assign value |
| update value | `count += 1\` | `count += 1` | Compound assignment |
| computed value | `derive label = expr\` | `let label = expr` or later `get label = expr` | Reactive expression |
| action | `function save()\ ... \` | `when save ...` | Function declaration |
| page root | `@main\ ... #` | `page ...` | Root element |
| element | `@button class="x"\ ... #` | `button class "x" ...` | Element |
| text | `show expr\` | `show expr` | Text binding |
| reusable UI | not complete | `make Card title ...` | Component declaration |
| slot | not complete | `slot` | Component children insertion |

For v2, `let` is intentionally broad: at top level it creates a value; inside an action it updates an existing value if one exists, otherwise creates a local value. This is friendly and memorable, while still deterministic.

## Core Friendly Keywords

The v2 reserved core should stay small:

```text
use export let const get when make page show if else for while in slot return break continue throw try catch finally as from
```

Recommended user-facing teaching names:

- `use`: bring in another file or component.
- `let`: name data or update data.
- `when`: define an action.
- `make`: define reusable UI.
- `page`: the main screen.
- `show`: show text or a value.
- `slot`: where child content goes.

The language should avoid requiring users to learn terms like state, props, hydrate, render, reactive, and component for the common path. Documentation can still explain those concepts in advanced sections.

## Lexical Grammar

This section uses EBNF-like notation. Single quotes are literal tokens. `NL` means newline. `INDENT` and `DEDENT` are indentation tokens produced by the v2 lexer mode.

```ebnf
upperLetter     = "A".."Z" ;
lowerLetter     = "a".."z" ;
letter          = upperLetter | lowerLetter | "_" ;
digit           = "0".."9" ;
identifier      = letter { letter | digit | "_" | "-" | ":" } ;
componentName   = upperLetter { letter | digit | "_" } ;
number          = digit { digit } [ "." digit { digit } ] ;
string          = '"' { char | interpolation } '"' | "'" { char | interpolation } "'" ;
interpolation   = "{" expression "}" ;
boolean         = "true" | "false" ;
comment         = "//" { notNL } | "#" { notNL } ;  // v2 line comments only outside classic element close position
path            = string ;
```

Classic syntax keeps `#` as an element close marker. Friendly syntax should treat `#` as a comment only when the lexer is in indentation mode and the `#` appears after optional whitespace on a line.

## Classic JTML v1 Grammar

Classic JTML is the current implemented syntax. It is statement-terminated by `\` and closes non-void elements with `#`.

```ebnf
classicProgram       = { classicStatement } EOF ;

classicStatement     = importStmt
                     | defineStmt
                     | constStmt
                     | deriveStmt
                     | assignmentStmt
                     | showStmt
                     | elementStmt
                     | ifStmt
                     | whileStmt
                     | forStmt
                     | tryStmt
                     | functionStmt
                     | asyncFunctionStmt
                     | returnStmt
                     | throwStmt
                     | breakStmt
                     | continueStmt
                     | objectStmt
                     | subscribeStmt
                     | unsubscribeStmt
                     | unbindStmt
                     | storeStmt
                     | expressionStmt ;

importStmt           = "import" path "\" ;
defineStmt           = "define" identifier "=" expression "\" ;
constStmt            = "const" identifier "=" expression "\" ;
deriveStmt           = "derive" identifier [ ":" identifier ] "=" expression "\" ;
assignmentStmt       = reference "=" expression "\" ;
showStmt             = "show" expression "\" ;
expressionStmt       = expression "\" ;
unbindStmt           = "unbind" identifier "\" ;
storeStmt            = "store" "(" scope ")" identifier "\" ;
scope                = "main" | identifier ;

elementStmt          = ("@" tagName | "element" tagName) { classicAttribute } "\" classicElementBody ;
classicElementBody   = voidEnd | { classicStatement } "#" ;
voidEnd              = /* allowed only for void HTML tags with empty body */ ;
tagName              = identifier ;

classicAttribute     = attrName [ "=" expression ] [ "," ] ;
attrName             = identifier ;

ifStmt               = "if" "(" expression ")" "\" block [ "else" "\" block ] ;
whileStmt            = "while" "(" expression ")" "\" block ;
forStmt              = "for" "(" identifier "in" forSource ")" "\" block ;
forSource            = expression [ ".." expression ] ;
block                = { classicStatement } "\" ;

tryStmt              = "try" "\" block [ "except" "(" identifier ")" "\" block ] [ "then" "\" block ] ;

functionStmt         = "function" identifier "(" [ parameters ] ")" [ ":" identifier ] "\" block ;
asyncFunctionStmt    = "async" functionStmt ;
parameters           = parameter { "," parameter } ;
parameter            = identifier [ ":" identifier ] ;
returnStmt           = "return" [ expression ] "\" ;
throwStmt            = "throw" [ expression ] "\" ;
breakStmt            = "break" "\" ;
continueStmt         = "continue" "\" ;

objectStmt           = "object" identifier [ "derives" "from" identifier ] "\" objectBody "\" ;
objectBody           = { defineStmt | constStmt | deriveStmt | functionStmt | asyncFunctionStmt } ;

subscribeStmt        = "subscribe" identifier "to" identifier "\" ;
unsubscribeStmt      = "unsubscribe" identifier "from" identifier "\" ;
```

## Friendly JTML v2 Grammar

Friendly syntax is indentation-based and line-oriented. It compiles to the same AST as classic syntax.

```ebnf
friendlyProgram      = { friendlyStatement } EOF ;

friendlyStatement    = useStmt
                     | letStmt
                     | constStmt2
                     | getStmt
                     | whenStmt
                     | makeStmt
                     | pageStmt
                     | friendlyElementStmt
                     | showStmt2
                     | ifStmt2
                     | forStmt2
                     | whileStmt2
                     | slotStmt
                     | returnStmt2
                     | throwStmt2
                     | breakStmt2
                     | continueStmt2 ;

useStmt              = "use" useTarget NL ;
useTarget            = path
                     | identifier "from" path
                     | "{" importList "}" "from" path ;
importList           = identifier { "," identifier } ;

letStmt              = "let" reference "=" expression NL ;
constStmt2           = "const" identifier "=" expression NL ;
getStmt              = "get" identifier "=" expression NL ;
showStmt2            = "show" expression NL ;
returnStmt2          = "return" [ expression ] NL ;
throwStmt2           = "throw" [ expression ] NL ;
breakStmt2           = "break" NL ;
continueStmt2        = "continue" NL ;

whenStmt             = "when" identifier [ parameters2 ] NL INDENT { friendlyStatement } DEDENT ;
parameters2          = identifier { identifier } ;

makeStmt             = "make" componentName [ parameters2 ] NL INDENT { friendlyStatement } DEDENT ;
pageStmt             = "page" { inlineAttribute } NL INDENT { friendlyStatement } DEDENT ;

friendlyElementStmt  = elementHead NL [ INDENT { friendlyStatement } DEDENT ] ;
elementHead          = tagOrComponent [ inlineText ] { inlineAttribute | eventBinding | inputBinding } ;
tagOrComponent       = tagName | componentName ;
inlineText           = expression ;

inlineAttribute      = attrName attributeValue ;
attributeValue       = expression | booleanAttr ;
booleanAttr          = /* empty value, e.g. required */ ;

eventBinding         = eventName actionExpr ;
inputBinding         = "into" reference ;
eventName            = "click" | "input" | "change" | "submit" | "hover" | "scroll" | identifier ;
actionExpr           = identifier | callExpression ;
callExpression       = identifier "(" [ args ] ")" ;

ifStmt2              = "if" expression NL INDENT { friendlyStatement } DEDENT [ "else" NL INDENT { friendlyStatement } DEDENT ] ;
forStmt2             = "for" identifier "in" expression NL INDENT { friendlyStatement } DEDENT ;
whileStmt2           = "while" expression NL INDENT { friendlyStatement } DEDENT ;
slotStmt             = "slot" NL ;

(* Element dictionary aliases — resolved before classic parsing *)
(*  text → p, box → div, list → ul|ol, item → li, link → a,    *)
(*  image → img, embed → iframe, checkbox → input type=checkbox *)
```

Friendly shorthand examples:

```jtml
button "Save" click save
```

Desugars to:

```jtml
@button onClick=save()\
  show "Save"\
#
```

```jtml
input "Email" into email
```

Desugars to:

```jtml
@input placeholder="Email" value=email onInput=setEmail()\
```

The compiler must synthesize a setter action when needed:

```jtml
when setEmail value
  let email = value
```

## Expression Grammar

The expression grammar should be shared by both syntaxes.

Current implementation note: classic and friendly expressions support
conditional values with `condition ? valueA : valueB`. This keeps derived UI
copy, attributes, and small display decisions out of statement-level branches
without introducing a second friendly-only expression grammar.

```ebnf
expression           = orExpr ;
orExpr               = andExpr { "or" andExpr } ;
andExpr              = equalityExpr { "and" equalityExpr } ;
equalityExpr         = comparisonExpr { ( "==" | "!=" ) comparisonExpr } ;
comparisonExpr       = additiveExpr { ( "<" | "<=" | ">" | ">=" ) additiveExpr } ;
additiveExpr         = multiplicativeExpr { ( "+" | "-" ) multiplicativeExpr } ;
multiplicativeExpr   = powerExpr { ( "*" | "/" | "%" ) powerExpr } ;
powerExpr            = unaryExpr { "^" unaryExpr } ;
unaryExpr            = ( "!" | "-" ) unaryExpr | postfixExpr ;
postfixExpr          = primaryExpr { postfixOp } ;
postfixOp            = "." identifier [ "(" [ args ] ")" ]
                     | "[" expression "]"
                     | "(" [ args ] ")" ;
primaryExpr          = identifier
                     | number
                     | string
                     | boolean
                     | arrayLiteral
                     | dictLiteral
                     | "(" expression ")" ;
args                 = expression { "," expression } ;
arrayLiteral         = "[" [ expression { "," expression } ] "]" ;
dictLiteral          = "{" [ dictPair { "," dictPair } ] "}" ;
dictPair             = ( string | identifier ) ":" expression ;
reference            = identifier { "." identifier | "[" expression "]" } ;
```

## Semantics

### Values And Assignment

`let name = expr` creates or updates a value depending on scope.

- At module/page top level: creates a module value if missing; updates if already defined in the same module.
- Inside `when`: updates the nearest existing value named `name`; if no value exists, creates a local action value.
- Inside `make`: creates local component state unless explicitly marked as shared later.

Classic `define` and `const` keep their current semantics:

- `define`: mutable value.
- `const`: immutable value.
- assignment: updates an existing mutable value.

### Derived Values

Classic `derive` remains explicit. Friendly v2 initially uses `let` for both stored and computed values. The compiler can infer dependencies for expressions and decide whether to create a derived binding when the value is used in UI.

Later, if ambiguity becomes a problem, add an advanced keyword:

```jtml
get label = "Count: {count}"
```

But v2 should not lead with this keyword.

### Elements

Friendly element rules:

- Lowercase names are HTML/custom tags.
- Uppercase names are components.
- First unlabeled expression after an element name is text content.
- Attributes are pairs: `class "card"`, `value email`, `id userId`.
- Boolean attributes can stand alone: `required`, `disabled`.
- Events use lowercase names: `click save`, `input setName`.

### Components

`make Name args...` defines reusable UI.

```jtml
make Card title
  section class "card"
    h2 title
    slot
```

Component calls:

```jtml
Card "Revenue"
  show "$42k"
```

Component semantics:

- Each component instance has an isolated scope.
- Parameters are local immutable inputs by default.
- `slot` renders child statements from the call site.
- Events passed into components are action references.

### Modules

`use` is the friendly import keyword.

Supported forms:

```jtml
use "./logic.jtml"
use Button from "./button.jtml"
use { formatMoney, parseDate } from "./money.jtml"
```

Modules can mark public top-level declarations with `export`:

```jtml
export let currency = "EUR"
export when refresh
  let currency = "USD"
export make Card title
  h2 title
```

Module semantics:

- A file is parsed once per compilation graph.
- Side-effect imports evaluate top-level `let`, `when`, and `make` into the current module scope during v1 compatibility.
- Named imports include only matching exported top-level declarations when the imported module uses `export`.
- Modules without `export` keep compatibility behavior for now.
- Import cycles are errors unless the cycle contains only type/component declarations that can be safely hoisted.

### Events

Friendly event names map to DOM event attributes:

| Friendly | Classic |
| --- | --- |
| `click` | `onClick` |
| `input` | `onInput` |
| `change` | `onChange` |
| `keyup` | `onKeyUp` |
| `hover` | `onMouseOver` |
| `scroll` | `onScroll` |
| `submit` | `onSubmit` |

Input binding:

```jtml
input "Email" into email
```

Means:

- placeholder text is `"Email"`.
- value is bound to `email`.
- input changes update `email`.

### Error Handling

Classic keeps `try / except / then`.

Friendly should use more familiar naming:

```jtml
try
  riskyAction
catch error
  show error
finally
  cleanup
```

Implementation can map `catch` to current `except` and `finally` to current `then`.

## Implementation Plan

### Phase 1: Formalize Existing Classic Grammar ✅

- Add grammar tests for every classic statement.
- Document current limitations around imports, linter, and transpiler.
- Keep all existing examples passing.

Deliverables:

- `docs/language-reference.md` updated.
- Parser fixture tests for statements, expressions, and elements.

### Phase 2: Add Friendly Lexer Mode ✅

- Add indentation tokenization: `INDENT`, `DEDENT`, `NL`.
- Detect friendly mode by file header or first non-comment token.
- Recommended header during transition:

```jtml
jtml 2
```

- Without a header, keep classic parsing for compatibility.
- Auto-detection (`looksLikeFriendlySyntax`) now triggers on `let`, `const`,
  `when`, `page`, `make`, `use`, `show`, `for`, `if`, and `while`.

Deliverables:

- Lexer tests for indentation, comments, strings, interpolation.
- Clear diagnostics for mixed tabs/spaces and bad indentation.

### Phase 3: Friendly Parser To Existing AST ✅

- Parse `use`, `let`, `const`, `get`, `when`, `page`, `show`, `if`, `else`, `for`, `while`.
- Desugar friendly nodes to existing AST nodes.
- Element dictionary aliases (`text`, `box`, `list`/`item`, `link`, `image`, `embed`, `checkbox`) resolve before classic parsing.
- Destructured imports (`use { A, B } from "path"`) lower to side-effect imports.

Deliverables:

- `examples/friendly_counter.jtml`.
- `jtml check --syntax friendly`.
- Formatter can preserve friendly syntax.

### Phase 4: Event And Input Sugar ✅

- Map friendly event names to current event attributes.
- Add `submit` support to transpiler/interpreter.
- Implement `into` by generating value + input binding + setter action.
- Checkbox `into` binds via `onChange`; text inputs via `onInput`.
- Ensure inputs/textareas update both attributes and live DOM properties.

Deliverables:

- `examples/friendly_form.jtml`, `examples/friendly_advanced_form.jtml`.
- Browser tests for typing and buttons.

### Phase 5: Module Graph

- Resolve imports relative to the importing file, not only the current working directory.
- Build an import graph.
- Parse each module once.
- Detect cycles.
- Watch imported files in `jtml serve --watch`.

Current status: implemented in the CLI compilation path. Imports are resolved
relative to the importing file, folded into a module graph with cycle checks,
and lowered before the shared Classic AST pipeline. Friendly `export` marks a
module's public top-level declarations, named imports respect that exported
surface, and bare imports resolve through the nearest `jtml_modules` package.

Deliverables:

- `ImportResolver`.
- Module graph tests.
- Watch-mode test editing an imported file.

### Phase 6: Components ✅ (runtime bridge)

- Add `ComponentDeclarationNode`.
- Add `ComponentCallNode`.
- Implement parameter binding and component-local scope.
- Implement `slot`.
- Support component imports.

Current status: `make`, uppercase calls, positional parameters, and `slot` still
lower through compatibility expansion, but each component call now carries
runtime instance metadata and executes inside an instance environment. Element
bindings, event handlers, local state/actions, dirty recalculation, `/api/state`,
`/api/components`, `/api/component-definitions`, and `/api/component-action`
resolve through that instance bridge. The remaining architectural step is direct
non-expanded `ComponentInstance` AST execution.

Deliverables:

- `make Card title ...`
- `Card "Revenue" ...`
- `use Card from "./card.jtml"`

### Phase 7: Friendly Formatter And Migration ✅

- Add `jtml fmt --syntax friendly`.
- Add `jtml migrate classic-to-friendly`.
- Keep classic syntax as stable compatibility layer.

Current status: `jtml fmt` preserves Friendly source and `jtml migrate` converts
most Classic files to Friendly `jtml 2`; bundled examples have been migrated
except for the intentional Classic keyword compatibility fixture.

Deliverables:

- Migration tests for classic examples.
- Rebuild Studio in friendly syntax.

### Phase 8: Production Tooling

- `jtml dev app/`
- `jtml build app/ --out dist`
- `jtml serve app/`
- Better diagnostics with line excerpts and suggested fixes.

Current status: `jtml dev <input.jtml|app/>` serves the app with hot reload and
static assets from the app directory. `jtml build <input.jtml|app/> --out dist`
writes `dist/index.html`; directory builds also copy non-`.jtml` assets into
the output directory. `jtml check --json`, `jtml lint --json`, Studio, and LSP
share structured diagnostics with repair metadata. `jtml serve app/` remains
open.

Deliverables:

- App folder convention.
- Static build output.
- Error snapshots in tests.

## Open Design Decisions

1. Should friendly files require `jtml 2` at the top, or should syntax be auto-detected?
2. Should `let label = "Count: {count}"` be reactive by default, or should computed values use `get`?
3. Should component names be required to start uppercase?
4. Should `input "Email" into email` set placeholder, label, or both?
5. Should `use "./logic.jtml"` leak all names, or should v2 require explicit imports?

Recommended answers:

1. Require `jtml 2` during transition; allow auto-detection later.
2. Make UI bindings reactive by dependency tracking; avoid `get` until needed.
3. Yes, uppercase component names keep parsing simple.
4. Placeholder initially; later pair with `label`.
5. Side-effect imports may leak in compatibility mode; named imports should be preferred for v2.
