# Your first JTML application

Build a browser-local coffee-order form: enter a name, add cups, calculate a
total, submit an order and clear the form. It has no backend or persistence;
reloading starts a new order. JTML is an experimental technical preview.

## Choose a compiler

- **Current main (`0.3.0-dev`)** is required for this form's complete reset
  behavior. The component-input synchronization fix is not in a published release.
- **Published `0.3.0-rc.2`** has binaries for Ubuntu 22.04 x86_64 and Apple
  Silicon macOS (tested on 14.4.1). Follow the
  [download/checksum/Counter instructions](releases/v0.3.0-rc.2.md#install-and-five-minute-first-app-smoke)
  if you want to try a binary first. Its Counter works, but this form's Clear
  button leaves the old name visible. Do not interpret a successful build as
  proof that the application behaves correctly.

For source builds, install Git, a C++17 compiler/build tools, and CMake
(validated with 3.31.3). On macOS, Apple's Command Line Tools supply Git and
Clang; CMake is a separate prerequisite. Python 3 serves static output in this
guide. Check `git --version`, `c++ --version`, `cmake --version` and
`python3 --version` before starting. Configure needs internet access to download
the pinned WebSocket++ and Asio sources. Node/npm, Python bindings and tests are
not required to build this application. Other platforms are not verified here.

From a new directory:

```sh
git clone https://github.com/mrjohnnyrocha/jtml.git
cd jtml
git rev-parse HEAD
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJTML_BUILD_PYTHON=OFF -DJTML_BUILD_TESTS=OFF
cmake --build build --target jtml_cli --parallel
./build/jtml --version
```

Record the full commit printed above: main moves, and `0.3.0-dev` alone does
not identify a build. The [audit](tooling/first-user-audit.md) records the exact
baseline tested. The first build may take several minutes. These commands do
not install a global `jtml` command; keep using `./build/jtml` from this repository
root. [Prefix installation](tooling/deployment.md#install-locally-from-source)
is optional.

## Write the application

Create `coffee.jtml` in the repository root and paste this complete file:

```jtml
jtml 2

make CoffeeOrder
  let customer = ""
  let cups = 1
  get total = cups * 3
  let receipt = "No order yet"

  when more
    cups += 1

  when order
    if customer == ""
      receipt = "Enter your name"
    else
      receipt = "Order for {customer}: {cups} cups, EUR {total}"

  when clear
    customer = ""
    cups = 1
    receipt = "No order yet"

  h1 "Coffee order"
  form submit order
    label "Your name"
      input into customer aria-label "Your name"
    p "Cups: {cups}"
    p "Total: EUR {total}"
    button "Add cup" type "button" click more
    button "Place order" type "submit"
    button "Clear" type "button" click clear
  p receipt

page
  CoffeeOrder
```

Use spaces for indentation. `let` holds mutable state; `get` recomputes a derived
value; `when` defines an action. `make` defines a reusable component, instantiated
under `page`. `input into customer` connects the textbox to that component's
local writable state. `{customer}` interpolates text. The `receipt` stores the
last submitted order, so adding another cup does not rewrite that order until
you submit again. The explicit accessible label makes the textbox discoverable
by assistive technology and browser tests.

## Check, compile and run

```sh
./build/jtml check coffee.jtml
./build/jtml lint coffee.jtml
./build/jtml build coffee.jtml --target browser --out dist/coffee
python3 -m http.server 8000 --bind 127.0.0.1 --directory dist/coffee
```

Open [http://localhost:8000](http://localhost:8000). Leave the terminal running.
If port 8000 is occupied, choose 8001 in both the command and URL.

1. Initially see an empty name, `Cups: 1`, `Total: EUR 3`, `No order yet`.
2. Submit an empty name: see `Enter your name`.
3. Type `Ada` and press Enter: see `Order for Ada: 1 cups, EUR 3` without navigation.
4. Add a cup: total becomes EUR 6 while the saved order still describes one cup.
5. Place the order again: the receipt now describes two cups and EUR 6.
6. Clear: the visible textbox empties, cups return to 1, total to EUR 3, and the
   receipt to `No order yet`. This step fails on RC.2; use current main.
7. Reload: initial state returns. Stop the server with Ctrl+C.

Keep `index.html`, `jtml-runtime.js`, `components/index.js` and `app.js` together.
Serve the directory over HTTP; do not open `index.html` with a `file://` URL.
No JTML process is needed while using this compiled application. `jtml serve`
and `jtml dev` are separate live development backends; they are not substitutes
for checking your production build in a browser.

After editing the source, rerun check/lint/build and refresh the browser. A check
or lint error should include the file and a diagnostic; fix it before continuing.
For example, using `{doubled}` without declaring it is an undefined-name error.
For component imports, the imported file must declare `export make Name`.
Current main includes PR #53's production import correction; `jtml new app`
now generates a modular starter that checks, lints, builds and runs in a browser.
RC.2 lacks that correction. This guide keeps CoffeeOrder in one file so its local
input/state ownership is explicit. General slots still have limitations: conditional
slots can lose content, slotted child components can fail to initialize, and an
input setter generated only inside a caller slot can be missing. These are separate
compiler work; avoid those shapes in a first application. See the
[audit boundaries](tooling/first-user-audit.md#remaining-boundaries).

## Independent first-user test

Give this page to three developers independently, with no author assistance.
Use a new checkout/directory for each, record its commit and OS/tool versions,
and allow 30 minutes after prerequisites are installed. Do not share another
participant's solution. Each should follow the commands, complete all seven
browser checks, then change the price from EUR 3 to EUR 4 and verify the new
total after rebuilding. Record elapsed time, help/documentation consulted, every
failed command with full output, and whether intervention was required.

Success requires all browser checks and the price change without author help.
An installation failure, unresolved diagnostic or stale textbox is a failure,
even if the compiler exits zero. Report RC.2 results separately and preserve its
reset failure. This handoff protocol has not yet been run by three external
developers; automated checks are not evidence of independent user success.
