# ReleaseDesk dogfooding findings

These findings come from compiling the directory and operating the real browser
against HTTP/SQLite, rather than assuming an example's source proves behavior.
No accepted compiler fixture, budget or oracle was changed.

## Fixed: lazy fetch starts busy without any request

```jtml
jtml 2
let write = fetch "/api/issues" method "POST" body { title: "Probe", description: "Details", severity: "low" } lazy refresh save
get busy = write.loading
page
  button "Save" click save disabled=busy
  if busy
    text "Saving"
```

At main `fa6d10e`, the browser sends no POST (correct), but `write.loading` starts
true, so the button never enables. Classification: Friendly lowering initialized
all fetches as loading, ignoring `lazy`. The fix initializes only lazy fetches
with loading false; eager fetch behavior remains unchanged. The compiler test
examines actual manifest state, and app E2E verifies idle/enabled → blocked
in-flight → confirmed result, with no mutation during startup or editing.

## Fixed: root form submit reloads before async completion

```jtml
jtml 2
let draft = ""
let write = fetch "/api/issues" method "POST" body { title: draft, description: "Details", severity: "low" } lazy refresh save
page
  form submit save
    input into draft required
    button "Save" type "submit"
```

Before the fix, Enter navigates to `/?` and races/aborts the POST. Classification:
root element HTML event emission, not fetch semantics or SQLite. Root submit
handlers now call `event.preventDefault()` synchronously before dispatch, matching
the existing delegated component-form contract. Forms without a JTML submit
handler retain normal HTML submission. A compiler regression and E2E keyboard
submission verify no navigation, real persistence and native required validation.

## Retained: root explicit IDs lose to generated IDs

The first two controls in [root-controls.jtml](probes/root-controls.jtml) reproduce
this independently: generated HTML contains both `id="elem_…"` and
`id="declared"` on the input. The browser retains the first, so
`document.getElementById("declared")` is null and the explicit label's `.control`
is null. Classification: root HTML emission/DOM identity ownership. Fixing this
must preserve runtime lookup/bindings and is outside this app slice. ReleaseDesk
uses standard implicit wrapping labels, verified by actual label-based keyboard
and input operations; sections use literal accessible names.

## Retained: programmatic root select value does not select an option

The same probe initializes `selection = "high"`; browser state says high but the
native select displays its first option, low. Choose High manually, then click
Choose low: state becomes low but the displayed selection stays high.
Classification: browser `applyAttribute` sets the value attribute, but only
synchronizes the value property for INPUT/TEXTAREA, not SELECT. No semantics need
to change, but this slice avoids another compiler/runtime patch: ReleaseDesk uses
a required empty first choice and preserves the user's severity when clearing
text. Real `selectOption`/input events persist the chosen severity in SQLite.

## Fixed: strict script CSP for supported browser events

At PR #45 main `32bb777`, root controls emitted `oninput`, `onclick` and
`onsubmit`. The unchanged root-controls probe under `script-src 'self'` kept
Clicks at 0 and reported a blocked inline handler. Root events in browser-local
output now carry HTML-escaped JSON metadata. Compiler-owned external `app.js`
listeners call the existing action dispatcher; they do not evaluate attribute
JavaScript. Submit cancellation is synchronous and native required validation
still precedes submit. Input/change values, keyboard payloads, action arguments,
bubbling and dynamic root controls retain real-browser coverage.

`tests/browser/strict_csp.cjs` compiles this existing probe, the unchanged Friendly
Counter and a new independent control fixture, serves the actual assets with the
strict HTTP header, and operates them in Chromium. Separate negative controls
prove inline scripts and event attributes are blocked. ReleaseDesk's complete
HTTP/SQLite E2E now runs with the same strict script policy, without custom
frontend JavaScript or compatibility assets. Inline styles remain allowed by the
separate style policy. Author-supplied raw HTML/JavaScript URLs, extern host
functions and legacy inline live/preview documents are outside this proof.
The inert `application/json` manifest remains an HTML data block, not executable
inline JavaScript.

```sh
./build/jtml build apps/releasedesk/probes/root-controls.jtml --target browser --out dist/releasedesk-root-probe
python3 apps/releasedesk/server.py --dist dist/releasedesk-root-probe --db dist/releasedesk-probe-data/issues.sqlite3 --port 8766
node tests/browser/strict_csp.cjs
```

## Fixed: component introduced by a root conditional is not initialized

The unchanged [root-conditional-component.jtml](probes/root-conditional-component.jtml)
failed on PR #46 main `26eb51c`: Toggle inserted lowered HTML without initializing
Child; its local state stayed as a placeholder and increment reached the missing
`__Child_1_increment` action. Semantic IR and the client Runtime Plan retained
Child's definition/instance. The first loss was after root `innerHTML` insertion,
which never registered/rendered the instance.

Root template replacement now uses existing component registration, generated
static creation and destruction. It tears down old descendants before replacement
and initializes connected component roots from their manifest ownership records.
Hidden/expanded nested declarations are not registered as active root instances.
Remount gets fresh local state; live root arguments update without resetting it.
Strict-CSP browser acceptance covers the unchanged probe, repeated cycles,
independent siblings, nested/keyed children, callback cleanup and detached controls.
See [the lifecycle report](../../docs/architecture/root-conditional-mount.md).

Remaining boundaries: a component directly repeated by a **root** loop has no
per-item component identity in the current root backend. Browser-local compilation
now rejects that shape before assets, at the authored component location, and
instructs the author to put the keyed loop inside a component. Plain root-loop
controls and component-owned keyed children pass. This does not add a new root
keyed-component renderer. Component input retention tests use the existing
explicit `value draft into draft` pattern; the independently observed patch
fallback for component `input into draft` without `value` is outside this fix.

## Application/API boundary, not a compiler promise

Async `when`/await and a success callback that invalidates other fetches are not
assumed. The app consumes the newest authoritative append-only snapshot returned
by GET or committed POST. HTTP errors currently expose status text, not the JSON
error body. Drafts survive a failed request, not a page reload. Network retries
cannot promise exactly-once writes without an idempotency key, so recovery asks
the user to read the persisted list before retrying an uncertain save.

Native rendering parity, TypeEnvironment, precise spans, global HTML payload size
and enterprise/public-server readiness remain outside this application claim.
