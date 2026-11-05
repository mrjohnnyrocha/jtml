# ReleaseDesk — first issue workflow

ReleaseDesk is a local release-blocker/regression notebook, written in Friendly
JTML 2. It lists real SQLite issues, validates and saves a title, description and
severity, and recovers them after browser reload or server restart. Search and
severity filters operate on the already loaded list. There are no seed records,
accounts, dashboard metrics or cloud services. JTML owns fetch/create/list
rendering. The small external `public/issue-filters.js` owns filter controls, row
visibility and result messages, using ordinary browser APIs and no dependencies.
It reads app-owned DOM fields and makes no API requests or runtime-state writes.
`public/issue-create.js` retains creation keys/drafts and synchronizes declared
form bindings through native input events. It never sends requests or replaces
JTML request handling. `test_browser.cjs` is a test driver, not a shipped asset.

## Start

From the repository root, build the CLI if needed (see the root README), then:

```sh
./build/jtml build apps/releasedesk --target browser --out dist/releasedesk
python3 apps/releasedesk/server.py --dist dist/releasedesk \
  --db dist/releasedesk-data/issues.sqlite3 --port 8765
```

Open **http://127.0.0.1:8765**. Use that address rather than `localhost`: write
origin checks deliberately match the bound IPv4 loopback address. Python 3 and
its bundled SQLite are the only backend dependencies. The compiled app and API
share the same origin. No JTML live/WebSocket server or Node process is needed to
use the application. Stop with Ctrl+C; the database remains on disk.

Enter the three fields and choose **Save issue** (or Enter from Title). Success
means a *Last confirmed save* notice and a row in Saved issues. Reload the whole
page: the row must return. **Clear text for next issue** resets the title/description and retains your last
severity choice;
it does not delete records. Once submitted, the draft and request key survive
failure, same-tab reload and backend restart in browser session storage. Drafts
that have never been submitted are not retained across reload. **Reload issues** explicitly reads the API again.

Keep the SQLite file outside the static directory; the server rejects an unsafe
location. Back it up while the server is stopped. To deliberately start with an
empty list, choose a new `--db` file. Rebuilding static output does not reset data.
The server binds only 127.0.0.1 and is a trusted local development tool, not a
public deployment, authentication boundary or multi-user service.

## Find saved issues

Type in **Search issues** to match a literal substring in the title or
description, ignoring case and surrounding search whitespace. Choose
**Filter by severity** to narrow the same list. Both filters apply together.
The status reports how many of the loaded issues are shown. An empty database
keeps its first-issue prompt. A nonempty list with no matches shows a distinct
message and a **Clear filters** recovery path. Clear resets both controls and
returns keyboard focus to search.

The URL retains `q` and `severity`, so browser reload, local bookmarks and
Back/Forward navigation restore the visible filters. Typing replaces the current
history entry. It does not create a history entry per keystroke or submit the
creation form. Unknown severity URL values return to All severities.

**Reload issues** preserves filters. A confirmed save also updates the filtered
list immediately. A newly saved issue may be hidden by your filters, so its
confirmation explicitly points to Clear filters. Failed refreshes retain and
filter the last confirmed list alongside the existing recovery message.
Filtering never changes records in SQLite. All matching is over the records
already returned by the unchanged API.

Filters become visible when the external helper initializes. Its script is
loaded from the same origin under strict CSP. The reviewed, literal `html raw`
script reference produces the expected lint advisory. The core create/load UI
and unfiltered list remain usable if that helper cannot load.


## API and state ownership

- `GET /api/issues` returns `{issues: [...], revision: number}` from SQLite,
  newest first. `revision` is the highest persisted issue ID for this append-only
  slice.
- `POST /api/issues` accepts JSON `{title, description, severity, request_key?}`. Both text
  fields are trimmed and required; maximum lengths are 120 and 4000. Severity
  must be `low`, `medium`, `high` or `critical`. Unknown fields/types are rejected.
- A supplied `request_key` must be exactly 32 lowercase hexadecimal characters.
  First creation returns 201 **after commit**. Replaying the same key with the
  same normalized fields returns 200 and the original `created` issue. Both
  responses include `{created, issues, revision, request_key, replayed}`; replay
  sets `replayed: true`. Each response contains the **current** transaction's
  list/revision, not a stored stale copy of the original response. The original
  issue's ID, fields and creation timestamp remain unchanged.
- Reusing a key with different normalized fields returns 409 without mutation.
  Different keys can create separate issues with identical content. Invalid
  fields/keys return 400 before any transaction and do not consume the key.
  Database failures return 503 and roll back issue and key together.
- Compatibility: omitting the key retains the legacy non-idempotent contract:
  every valid POST returns 201 with `{created, issues, revision}` and creates a
  new row. An empty or invalid supplied key is rejected. The shipped UI always
  supplies a key; its Save control starts disabled until protection initializes.
  Cross-origin browser writes and oversized bodies still fail.

`issue_requests.request_key` is a SQLite primary key, with a unique foreign key
link to the created issue. `BEGIN IMMEDIATE` serializes lookup, payload comparison,
issue insertion, key insertion and snapshot reading in **one transaction**. The
connection commits before sending the response. There is no in-memory key cache,
time-based deduplication or content hash. Every request uses a fresh connection;
the mapping survives process restart. Initialization adds the mapping table to
existing databases without changing legacy rows. Payload comparison uses the
immutable issue fields in this append-only API; adding issue edits would require
retaining the original request payload separately. Keys are retained indefinitely
with their issues in this local application.

The app uses an eager GET, a lazy POST, native required/length validation and
JTML fetch `loading`, `error`, `ok` and `data`. The request body reads the draft
when submitted, not while typing. The newer confirmed revision wins between the
GET snapshot and POST snapshot; no optimistic rows are inserted. This is a small
append-only API contract, not a general mutation cache. The UI retains the last
confirmed snapshots during reload/failure with `stale keep`. List refresh never repeats
a POST. Mutation retries are manual; the POST declaration has no automatic retry.

The existing JTML fetch body transmits the per-request JSON key. Custom headers
and UUID generation are not assumed. The app-owned external helper uses
`crypto.getRandomValues` to generate 128 random bits and session storage to retain
one logical creation intent per tab. Before JTML handles submit it stores the
key/draft, sets the hidden `into requestKey` input and dispatches its normal input
event. JTML continues to own fetch dispatch, loading, errors, success and snapshots.
The helper never reads runtime state or patches global fetch.

An uncertain/failed submission keeps its key, including across reload. Save issue
retries that intent without automatically sending anything on reload/recovery.
Editing an uncertain draft keeps the key: if the first request committed, changed
fields receive 409, protecting the original. Restore the original fields to replay,
or inspect Saved issues and deliberately choose **Start a separate issue**.
Validation failures can be corrected using the same unconsumed key. Repeating a
confirmed unchanged submission also reuses its key. Editing confirmed fields
starts a new intent on the next submit. **Start a separate issue** explicitly
starts a new intent even with identical fields; **Clear text for next issue**
also clears the old intent.

Secure randomness and working session storage are required for the UI Save path.
A storage failure prevents dispatch rather than sending an unretained key. Closing
the tab, clearing storage or using another browser/tab can lose the retry intent;
inspect saved records before deliberately starting another. This is per-tab retry
protection, not deduplication of independent user submissions.

A failed load has an explicit Reload issues path. A failed save preserves the
draft and shows recovery instructions; repair/restart the server, reload to check
for a completed save if desired, then manually retry the retained intent. The runtime currently surfaces HTTP
status text rather than the backend JSON validation message, so the form always
shows its field limits and required values.

## Verification

API tests need only Python; browser tests require Node, Playwright and a real
Chrome/Chromium executable, following the repository's existing test convention:

```sh
python3 apps/releasedesk/test_api.py
# Use the same pinned test driver as CI if one is not already installed:
PLAYWRIGHT_SKIP_BROWSER_DOWNLOAD=1 npm install --prefix /tmp/releasedesk-driver --no-save playwright@1.63.0
export PLAYWRIGHT_BROWSERS_PATH=/tmp/releasedesk-browsers
/tmp/releasedesk-driver/node_modules/.bin/playwright install chromium-headless-shell
export JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT=/tmp/releasedesk-driver
export JTML_BENCH_BROWSER="$(find "$PLAYWRIGHT_BROWSERS_PATH" -type f -name chrome-headless-shell -perm -111 -print -quit 2>/dev/null)"
# Or set JTML_BENCH_BROWSER explicitly to your installed test browser.
scripts/test_releasedesk.sh
```

The browser is mandatory; missing tools fail rather than skip. The test builds
the **actual directory** in clean browser mode, launches the real Python backend
with a temporary SQLite file, and interacts through labels and keyboard/buttons.
It asserts loading and empty UI, one UI-created record in SQLite, browser reload,
server restart, required/whitespace rejection, an actual SQLite failure, retained
draft, save recovery, a stopped API/restart/retry, no unexpected console/page
errors, and the four compiler assets plus the two app-owned scripts. Intentional
HTTP 400/409/503, deliberately aborted responses and connection-refused resource
messages are expected only in the failure phases.
Additional acceptance covers title/description substring matching, all four
severities, combined filters, no results, clearing, keyboard/focus retention,
zero filtering requests, active-filter saves/refresh/failure recovery, real
SQLite persistence, URL reload/history and mobile layout. Successful API responses are
never mocked and JTML/DOM state is never injected. Delayed
request delivery is used only to observe loading and disabled-submit states.

The 12 API tests additionally cover normalized replay, payload conflict,
different-key identical content, eight concurrent same-key requests, concurrent
conflicting payloads, a dropped real HTTP response, mapping-insert rollback,
legacy database migration, validation/key rejection and keyless compatibility.
The browser forwards an actual POST to SQLite, observes its committed 201, then
aborts response delivery. It retains the draft/key, rejects a changed uncertain
draft with 409, restarts the backend, reloads the page, and manually replays the
original request with 200 and the original ID. Row counts stay at eight until an
explicit separate identical-content submission creates the ninth issue.

`dist/releasedesk-filtered.png`, `dist/releasedesk-no-results.png` and
`dist/releasedesk-filtered-mobile.png` record filter acceptance.
`dist/releasedesk-e2e.png` is the final browser screenshot. Both CI platforms and
the full local gate call the same test script. Existing compiler fixtures,
acceptance oracles and budgets are unchanged.

## Dogfooding findings

See [the minimal reproductions](gaps.md). Two bounded implementation defects are
fixed here without changing the language contract: lazy fetches started with an
incorrect loading flag, and root form-submit actions allowed native navigation
to race asynchronous work. Independent compiler regressions and real browser
interaction cover both.

The clean browser application now runs with `script-src 'self'`: the compiler
emits declarative root-event metadata and external event delegation. The local
server no longer permits `unsafe-inline` for scripts. Real browser tests cover
create/persist/reload and error/retry under that header and reject inline
executable scripts, event handlers, `eval` and `new Function`. Inline styles remain
permitted separately; this is a script-CSP claim for the tested application,
not a claim that arbitrary raw HTML, extern hosts or live previews are CSP-safe.

Components introduced by root conditionals now initialize and clean up on
hide/remount, with independent strict-CSP lifecycle tests. Direct root loops
of components still lack per-item ownership and reject before browser assets;
put such keyed loops inside a component. See the minimal probes in gaps.md.
Other limitations remain explicit.
Programmatic root select values are not reflected in the native selection, so
the form starts with a required empty choice and retains the user-selected severity
when clearing its text fields. Explicit root IDs are duplicated by the
renderer, so the app uses native wrapping labels and named sections rather than
broken `for`/ID references. No custom JS patches these behaviors.

## Small next steps

1. Edit/status: add a transactional revision and explicit successful mutation
   flow before allowing updates; preserve original request payloads when adding edits.
2. Routing: issue detail and an explicit not-found route, with reload tests.
3. Scale the loaded-list search only after a measured need for pagination. The
   current API and UI intentionally load the full issue list.

No authentication, teams, GitHub synchronization, public hosting or release
publication is part of this slice. Existing native renderer differences, HTML
size limits and experimental tooling remain unchanged.
