# ReleaseDesk — first issue workflow

ReleaseDesk is a local release-blocker/regression notebook, written in Friendly
JTML 2. It lists real SQLite issues, validates and saves a title, description and
severity, and recovers them after browser reload or server restart. There are no
seed records, accounts, dashboard metrics, cloud services or handwritten frontend
JavaScript. `test_browser.cjs` is a test driver, not a shipped asset.

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
it does not delete records. Unsaved drafts are kept on request failure, but do
not survive a browser reload. **Reload issues** explicitly reads the API again.

Keep the SQLite file outside the static directory; the server rejects an unsafe
location. Back it up while the server is stopped. To deliberately start with an
empty list, choose a new `--db` file. Rebuilding static output does not reset data.
The server binds only 127.0.0.1 and is a trusted local development tool, not a
public deployment, authentication boundary or multi-user service.

## API and state ownership

- `GET /api/issues` returns `{issues: [...], revision: number}` from SQLite,
  newest first. `revision` is the highest persisted issue ID for this append-only
  slice.
- `POST /api/issues` accepts JSON `{title, description, severity}`. Both text
  fields are trimmed and required; maximum lengths are 120 and 4000. Severity
  must be `low`, `medium`, `high` or `critical`. Unknown fields/types are rejected.
- HTTP 201 is sent **after commit**, with `{created, issues, revision}` from the
  same transaction. Invalid input returns 400, database failure 503; no fake
  success is reported. Cross-origin browser writes and oversized bodies fail.

The app uses an eager GET, a lazy POST, native required/length validation and
JTML fetch `loading`, `error`, `ok` and `data`. The request body reads the draft
when submitted, not while typing. The newer confirmed revision wins between the
GET snapshot and POST snapshot; no optimistic rows are inserted. This is a small
append-only API contract, not a general mutation cache. The UI retains the last
confirmed snapshots during reload/failure with `stale keep`. List refresh never repeats
a POST. Mutation retries are manual: after a network error, reload issues first
because a lost response can leave an uncertain commit. There is no idempotency
key yet, so blindly repeating an uncertain save can create a duplicate.

A failed load has an explicit Reload issues path. A failed save preserves the
draft and shows recovery instructions; repair/restart the server, reload to check
for a completed save, then retry if needed. The runtime currently surfaces HTTP
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
errors, and the four-asset production contract. Intentional HTTP 400/503 and
connection-refused resource messages are expected only in the failure phases.
API responses are never mocked and JTML/DOM state is never injected. Delayed
request delivery is used only to observe loading and disabled-submit states.

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
   flow before allowing updates; add idempotency for uncertain create retries.
2. Routing: issue detail and an explicit not-found route, with reload tests.
3. Filtering: severity/text filters, retaining a clear unfiltered recovery path.

No authentication, teams, GitHub synchronization, public hosting or release
publication is part of this slice. Existing native renderer differences, HTML
size limits and experimental tooling remain unchanged.
