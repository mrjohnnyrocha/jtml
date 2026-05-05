# JTML Language Reference

Version: 0.1.0-dev

JTML is a compact syntax for building reactive HTML pages. It keeps HTML's element and attribute model, then adds state, derived values, functions, events, objects, imports, and subscriptions.

## Files

JTML source files use `.jtml`.

JTML supports the classic syntax documented below and an early friendly syntax
when a file starts with `jtml 2`:

```jtml
jtml 2

let count = 0

when add
  let count = count + 1

page
  h1 "Counter"
  show "Count: {count}"
  button "Add" click add
```

Friendly syntax is indentation-based and currently lowers to classic JTML before
parsing. The implemented compatibility slice supports `use`, `let`, `when`,
`page`, `show`, HTML-like elements, `if`/`else`, `for`, `try`/`catch`/`finally`,
`return`, `throw`, `break`, `continue`, common DOM event shorthands, and
`input ... into name` bindings. Expressions support `condition ? valueA :
valueB` for conditional values. `make`, uppercase component calls, and `slot`
are supported as a source-level expansion to classic elements; component scope
is not yet isolated at runtime. Files without a `jtml 2` header can be parsed
as friendly syntax with `--syntax friendly`; `--syntax classic` disables
friendly normalization.

Friendly syntax also includes an early async data primitive:

```jtml
let users = fetch "/api/users"

page
  if users.loading
    text "Loading..."
  else
    for user in users.data
      text "{user.name}"
  if users.error
    text "Could not load users: {users.error}"
```

`fetch` creates a reactive browser-side value shaped as `{ loading, data,
error }`. It supports simple GET fetches, JSON request bodies for non-GET
calls, and a `refresh` action that re-triggers the fetch client-side without
a server round-trip:

```jtml
let login = fetch "/api/login" method "POST" body { email: email }

let posts = fetch "/api/posts" refresh reloadPosts

page
  button "Reload" click reloadPosts
  for post in posts.data
    text "{post.title}"
```

The browser runtime handles simple dotted reads such as `users.loading`,
`users.data`, and `user.name` inside loop templates.

Friendly routes map hash paths to components:

```jtml
route "/" as Home
route "/user/:id" as UserProfile
route "*" as NotFound

make Home
  page
    h1 "Home"
    link "Open user 42" to "/user/42"

make UserProfile id
  page
    h1 "User"
    show id

make NotFound
  page
    h1 "Not found"
```

Route parameters must match component parameters in order. `link ... to "/path"`
lowers to hash navigation such as `href="#/path"` so local single-page routes
work without extra JavaScript. Use `route "*" as Component` as the last route
for a not-found fallback.

`redirect "/path"` navigates to a route programmatically from inside a `when`
block without a server round-trip. The compiler emits a `<meta>` marker that
the browser runtime reads on load; clicking the triggering button is
intercepted client-side before the server is involved:

```jtml
when goHome
  redirect "/"

page
  button "Back to home" click goHome
```

Friendly effects run a block when a variable changes:

```jtml
let count = 0
let message = "Idle"

effect count
  let message = "Count changed"
```

The first implementation lowers `effect variable` to classic `subscribe`
machinery. Effect bodies should update existing state or call existing actions.

Friendly stores group shared state into a dictionary-like value:

```jtml
store auth
  let user: string = "Ada"
  let token = "abc"

  when logout
    let user = ""
    let token = ""

page
  show auth.user
  button "Logout" click auth.logout
```

The first store slice lowers store fields to one shared dictionary, so
`auth.user` and `auth.token` are ordinary reactive reads. Actions declared
inside the store compile to collision-proof generated functions, and UI code
can call them with the store-qualified form such as `auth.logout`.

Friendly type annotations are optional and erased at compile time:

```jtml
let count: number = 0
const name: string = "Ada"
get label: string = "Hello {name}"

when save email: string age: number
  let count: number = count + 1

make Badge title: string
  text title
```

This first slice keeps types as authoring documentation and prepares the syntax
for future linter and editor checks.

Friendly component calls isolate local state and actions at source-expansion
time:

```jtml
make Counter label
  let count = 0
  when add
    count += 1
  box
    h2 label
    show count
    button "+" click add

page
  Counter "First"
  Counter "Second"
```

Each `Counter` call receives its own generated names for `count` and `add`.
This prevents accidental shared state between repeated components. Full runtime
component instances are still a larger follow-up.

Imports are resolved relative to the importing file, parsed once per compile
graph, and cycles are reported as errors. `jtml serve --watch` watches imported
files as well as the entry file.

Every statement ends with `\\` unless it is an element close marker:

```jtml
define name = "Ada"\\
show "Hello, " + name\\
```

## Elements

The preferred element syntax is `@tag`:

```jtml
@main class="profile" data-page="home"\\
    @h1\\
        show "Hello"\\
    #
#
```

The explicit form is also supported:

```jtml
element main class="profile"\\
    show "Hello"\\
#
```

`#` closes the current non-void element. Void HTML elements such as `input`, `img`, `br`, `hr`, `meta`, and `link` can be written as one-line elements:

```jtml
@input id="email" type="email" required\\
@img src="/logo.png" alt="Logo"\\
```

Custom and SVG-style names are accepted:

```jtml
@my-widget data-mode="compact"\\
    show "Custom element"\\
#

@svg:path data-vector="ok"\\
    show "SVG-friendly name"\\
#
```

## Attributes

Attribute names can include hyphens and colons. Keyword-shaped names such as `for` are allowed in element and attribute positions.

```jtml
@label for="email" aria-label="Email"\\
    show "Email"\\
#
```

Boolean attributes can omit `=`:

```jtml
@input disabled required\\
```

Event attributes call JTML functions without user JavaScript:

```jtml
function save()\\
    show "Saved"\\
\\

@button onClick=save()\\
    show "Save"\\
#
```

Supported browser event attributes include `onClick`, `onInput`, `onChange`, `onKeyUp`, `onMouseOver`, and `onScroll`.

## State

`define` creates mutable state:

```jtml
define count = 0\\
```

`const` creates immutable state:

```jtml
const limit = 10\\
```

Assignment updates an existing mutable variable:

```jtml
count = count + 1\\
count += 1\\
```

Compound assignment supports `+=`, `-=`, `*=`, `/=`, and `%=`. The parser lowers
these to ordinary assignments, so tooling and runtime behavior stay predictable.

## Derived Values

`derive` creates a reactive value from other values:

```jtml
define count = 0\\
derive doubled = count * 2\\
show "Doubled: " + doubled\\
```

Derived values update when their dependencies update.

Conditional expressions keep production UI copy and attributes compact:

```jtml
derive message = saved ? "Saved" : "Unsaved"\\
@button disabled=saved\\
    show saved ? "Done" : "Save"\\
#
```

## Collections

Arrays and dictionaries are first-class values:

```jtml
define names = ["Ada", "Grace", "Linus"]\\
define user = {"name": "Ada", "role": "developer"}\\

show names[0]\\
show user["name"]\\
```

## Control Flow

Blocks use statement terminators around their bodies:

```jtml
if (count > 0)\\
    show "Positive"\\
\\
else\\
    show "Zero"\\
\\
```

Loops:

```jtml
for (name in names)\\
    show name\\
\\

while (count < 10)\\
    count = count + 1\\
\\
```

## Functions

Functions can take parameters and return values:

```jtml
function greet(name)\\
    return "Hello, " + name\\
\\
```

`async function` dispatches calls without blocking the caller:

```jtml
async function refresh()\\
    show "Refreshing"\\
\\
```

## Objects

Objects group fields and methods. Inheritance uses `derives from`:

```jtml
object Person\\
    define name = "Ada"\\
\\

object Admin derives from Person\\
    define role = "admin"\\
\\
```

## Imports

Imports evaluate another JTML file in the current runtime:

```jtml
import "shared.jtml"\\
```

## Subscriptions

Subscriptions call a function when a variable changes:

```jtml
function changed(value)\\
    show "Changed: " + value\\
\\

subscribe changed to count\\
unsubscribe changed from count\\
```

## Reserved Keywords

```text
element show define derive unbind store for if const in break continue throw
else while try except then return function to subscribe from unsubscribe
object derives async import main true false
```

These words are reserved in statement and expression positions. They can still be used as element or attribute names when the parser is clearly reading HTML-shaped syntax.
