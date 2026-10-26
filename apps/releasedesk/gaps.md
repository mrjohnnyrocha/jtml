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

## Retained: strict CSP for root event attributes

Root controls emit `oninput`, `onclick` and `onsubmit` attributes. Serve the probe
with `Content-Security-Policy: script-src 'self'`: clicking Increment is blocked,
Clicks remains 0 and the browser reports an inline-handler CSP violation.
Classification: root browser event transport/HTML emission. A complete fix needs
static event delegation and independent root/native/live coverage; it is not a
small app-only change. The app's local-only CSP explicitly permits generated
inline handlers, but not eval/new Function. No custom JS installs listeners or
patches state. Clean browser mode is tested; strict script CSP is **not** claimed.

To build the remaining-gap probe (a diagnostic, not another product frontend):

```sh
./build/jtml build apps/releasedesk/probes/root-controls.jtml --target browser --out dist/releasedesk-root-probe
python3 apps/releasedesk/server.py --dist dist/releasedesk-root-probe --db dist/releasedesk-probe-data/issues.sqlite3 --port 8766
```

The normal server permits inline handlers. To reproduce strict CSP, serve the
same four assets with the restrictive header described above. The main app does
not load this probe or ship it as an extra browser asset.

## Application/API boundary, not a compiler promise

Async `when`/await and a success callback that invalidates other fetches are not
assumed. The app consumes the newest authoritative append-only snapshot returned
by GET or committed POST. HTTP errors currently expose status text, not the JSON
error body. Drafts survive a failed request, not a page reload. Network retries
cannot promise exactly-once writes without an idempotency key, so recovery asks
the user to read the persisted list before retrying an uncertain save.

Native rendering parity, TypeEnvironment, precise spans, global HTML payload size
and enterprise/public-server readiness remain outside this application claim.
