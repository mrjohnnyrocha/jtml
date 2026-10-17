# Taskboard adversarial acceptance contract

Status: **proposed acceptance specification; owner approval pending**, 2026-10-18.
This defines desired application behavior independently of the compiler. It is
not an implementation report or a claim that the current language can express
all requirements. The companion [support matrix](v0.3-support-candidates.md)
records actual evidence separately.

The read-only draft at
`/Users/joaorocha/.codex/visualizations/2026/10/14/01a13b12-9a73-7db2-8311-abd954688870/taskboard-t0-contract-draft.md`
was inspected. Its product assumptions are made explicit below; its pre-PR #10
conditional defect is historical. This version-controlled contract is self-contained
and does not require that local draft to remain available.

Requirements must not be weakened because JTML fails them. A failing scenario
is a recorded blocker, with its earliest incorrect compiler/runtime layer and
remediation. Any future product change must retain the original failing evidence
and receive explicit owner approval; it cannot retroactively count as a pass.
No language/runtime feature, application, backend, license or release is added here.

## Milestones and proposed product decisions

| Milestone | Scope | What success does not establish |
| --- | --- | --- |
| T0 Core | One browser-local board, in-memory single-user data, semantic stress test on clean production assets. Reload deterministically restores the seed. | Persistent services, authentication, deployment or complete application capability. |
| T1 Application (v0.4) | Independent HTTP API, persistent CRUD, routing/direct links, validation and async recovery. | Authentication/deployment/maintenance acceptance by itself. |
| T2 Application (v0.4) | Conventional backend authentication boundary, production deployment, end-to-end/accessibility and external developer maintenance acceptance. | Enterprise readiness or a new JTML backend language. |

The concrete choices below (trimming, monotonic IDs, fresh local state on remount,
focus after removal, HTTP conflict behavior) are proposals awaiting owner approval.
They deliberately settle ambiguous outcomes before implementation. They are not
permissions to lower T0's required stress coverage.

## T0 data, ownership and UI

The board owns a collection of objects `{id, title, completed, note}` and explicit
order. IDs are immutable, unique opaque strings, never titles or positions.
An ID→object map plus ordered IDs is acceptable, as is an ordered object array;
all visible rows must be backed by those records, not disconnected demonstration
cards. Title, completion, saved note and ordering survive unmount within the
current session because they belong to board state.

Each keyed TaskRow has local `draftTitle`, `draftNote`, edit mode and a local
interaction counter. Each row nests a header/status component, including a second
nested level receiving board title and task status through parameters. The board
is the sole writer of canonical records. Rows request parent-owned updates using
an explicit supported communication path; they must not mutate borrowed objects.
A parallel second board with separate object instances and the same seed IDs
provides an isolation control. Key identity is scoped to its owning board/loop.

Seed order and values (saved notes initially empty):

| ID | Title | Completed |
| --- | --- | --- |
| t-101 | Audit runtime | false |
| t-102 | Audit runtime | false |
| t-103 | Ship documentation | true |
| t-104 | `<release> & CSP` | false |

Board title starts `Before`. Newly created IDs are `t-105`, `t-106`, …, assigned
only on successful creation and never reused in the session, including after
deletion. Identity must be testable by ID; assertions may not select a row by its
duplicate title or by its current ordinal position.

Required controls: native create form, per-row edit/save/cancel and note-submit
forms, completion toggle, move up/down, delete, All/Open/Complete filter, board
rename, help toggle and a deterministic last-open-task completion scenario.
Counts total/open/complete use all canonical tasks; visible count uses the filter.
Render distinct “No tasks” and “No tasks match this filter” states. All labels,
counts, status text/attributes and nested headings must agree after every action.

Board diagnostic UI makes conditional expectations independently observable:
`fallback` initially equals `Fallback A`, `help` initially false; show a branch
label (`title` when `title == "After"`, otherwise `fallback`), a logical flag
`title == "After" && help`, and a ternary label `help ? title : fallback`.
Also show derived `remaining = total - complete` and
`remaining == 0 ? "All complete" : "Work remains"`; remaining must equal open
count through all task actions. These probes are application requirements, not
prescribed compiler lowering.

Titles are trimmed and nonempty. Empty/whitespace saves fail with a visible,
associated error; they neither mutate state nor consume an ID. Duplicate titles
are valid. Cancel restores committed title, exits edit mode, and clears its error.
Notes preserve their text exactly, including whitespace and markup-looking text.
Native form submit must dispatch once, prevent browser navigation and not depend
on an extra click handler. Enter uses the real keyboard submit path in browser
acceptance, not only a synthetic handler call. Clicking a submit button also
executes once. Mutation completion may be observed after settled microtasks/frames;
no arbitrary fixed sleeps are an oracle.

For retention assertions, invoke the action without intentionally moving focus
(native Enter or documented keyboard reorder shortcuts). Set the input selection
immediately before the action under test. Normal pointer focus transfer to a
clicked button is not a compiler focus failure; separately test that button
activation still dispatches once.

## Identity, lifetime, focus and failure policy

- Reordering retained IDs preserves row/nested component instances, local drafts,
  counter values and exact input DOM. Save/toggle/unrelated mutations retain the
  still-mounted form/input, active element and selection start/end/direction when
  focus was in that input. After each such operation, another edit/submit must
  work once on the correct row.
- Filtering a row out **destroys** that row and its nested instances, handlers,
  subscriptions and registry ownership. Filtering back in creates fresh instances:
  draft fields initialize from canonical title/note, counter resets to zero,
  edit mode is closed. Unsaved drafts are discarded; committed data survives.
  Rows remaining visible retain identity and local state. Merely hiding all rows
  with CSS does not satisfy this unmount contract.
- Deletion has the same cleanup obligation and removes canonical data. Captured
  detached controls cannot dispatch mutations after removal. Repeated filter,
  branch and deletion cycles must return registry/handler counts to the expected
  live set, with no growth from prior generations.
- A removed focused row sends focus to the filter control if filtered out, or the
  next remaining row's primary edit control on deletion (previous row if last;
  create title input if empty). Remount does not steal focus. Moving a task that
  is already at the boundary is a no-op preserving identity/state/focus.
- Same active conditional branch retains interactive descendants on both relevant
  and unrelated writes. Genuine transitions destroy old children before registering
  replacements. Help off/on recreates its local counter at zero without changing
  retained task state.
- A sibling task/board must not change because another owner writes an object,
  index or alias. Unsupported ownership/assignment shapes must reject before
  runnable production output with original file/line/column/component context
  and remediation; accepting a no-op or stale DOM fails acceptance.
- No implicit compatibility or runtime patch fallback, `eval`/`new Function`,
  unexpected fetch/XHR/WebSocket, uncaught errors, console errors, duplicate
  submission or silent runtime failure. Explicit invalid-input feedback is expected.

## Deterministic T0 acceptance sequence

Run this sequence on board A; board B remains exactly at the seed throughout.
At every checkpoint assert canonical values, rendered values, counts, order,
filtered IDs, nested text/attributes, identity/lifecycle and zero errors/network.
Counts below are total/open/complete; visible order is listed explicitly.

| ID | Action and setup | Expected outcome |
| --- | --- | --- |
| C01 | Fresh clean production load, All. Capture row/nested/input references, instance IDs and registry. | Order 101,102,103,104 (all IDs carry `t-`); counts 4/3/1, visible 4. Duplicate titles represent distinct objects and instances. `<release>` is literal text with no created element. |
| C02 | Submit empty then whitespace create titles using Enter and submit button. | Associated errors; seed/order/counts unchanged, no ID consumed, no navigation or task created. |
| C03 | Enter-submit `  Audit runtime  `. | Exactly t-105 created once, trimmed title, empty note, open; order 101,102,103,104,105; counts 5/4/1. Clear create input after success; next edit works. |
| C04 | Edit t-101 to `Audit first`; save by Enter. Edit t-102 to whitespace then cancel. | Only 101 changes; 102 stays `Audit runtime`, validation clears on cancel. Counts 5/4/1. Retained inputs/instances and sibling B unchanged. |
| C05 | Set t-101 local draft note to `Review <release> & CSP`, increment its local counter twice, then focus/select [7,16] backward and save note with native Enter submit. | Exactly one save; canonical note equals exact text, no injected element; same input/selection/focus and counter 2. Save count instrumentation is one; handler is not duplicated. |
| C06 | Leave t-102 draft note `unsaved second`, counter 1. Reorder t-101 below t-104 using three keyboard move-down operations while its input is focused/selected. | Order 102,103,104,101,105; all row/nested/input references retained, t-101 focus/selection/counter 2 and saved note retained; t-102 draft/counter 1 retained. Further submit in 102 updates only 102, then set another unsaved draft. |
| C07 | Toggle t-101 complete, then open, then complete. | Counts 5/3/2 → 5/4/1 → 5/3/2; nested status/attributes update each time. IDs/order/local state stay fixed. |
| C08 | Select Open; then All. | Open order 102,104,105, visible 3, counts 5/3/2. Removed 101/103 are destroyed. Back to All: original order restored, 101/103 fresh local state/counter 0 with saved data; 102/104/105 retain references, unsaved drafts/counters. Focus follows removal rule. |
| C09 | Rename Before→After; change inactive branch fallback to `Fallback B`; set help true; rename After→Before→Final; set help false. | Branch label: After→After→After→Fallback B→Fallback B→Fallback B. Logical flag: false→false→true→false→false→false. Ternary label: Fallback A→Fallback B→After→Before→Final→Fallback B. Both nested levels and attributes track title; same-branch children retain state/identity. Board B stays Before with Fallback A. Counts remain 5/3/2. |
| C10 | Help on, increment help-local counter, write t-104 note to its existing empty value, help off/on twice. | Same branch retains counter/identity through unrelated write; genuine transitions release old generation and recreate counter 0. Task state stays intact. |
| C11 | All: delete t-103; repeat attempted delete via captured detached control. | Order 102,104,101,105, counts 4/3/1. Deleted generation/state/handlers absent. Detached event has no effect; surviving rows retain identity. |
| C12 | Create `Final task`, then try moving first up and last down. | t-106 allocated; order 102,104,101,105,106; counts 5/4/1. Boundary moves leave values/identity/focus unchanged. |
| C13 | Complete 102,104,105,106; select Open, then Complete. | Counts 5/0/5, Open visible 0 and “No tasks match this filter”; Complete order 102,104,101,105,106. This is not “No tasks”. |
| C14 | All: delete every task. Create one task after empty state. | Counts 0/0/0, “No tasks”, empty task registry/handlers. New task is t-107, counts 1/1/0, no reused task identity. Reload resets both boards to C01 (in-memory T0). |

IDs abbreviated in this table do not authorize numeric/index-derived keys.
Focus assertions use real browser active-element/selection readings. Record exact
DOM references and instance generations, not only equal HTML snapshots.

## Additional negative and edge scenarios

Run each from an isolated seed with deterministic expected outcomes:

| ID | Stimulus | Required outcome |
| --- | --- | --- |
| N01 | Supply duplicate IDs or missing IDs via the test data boundary. | Deterministic visible data error before rendering conflicting rows; no corruption/identity reuse. Duplicate titles remain valid. |
| N02 | Attempt to change an existing task ID or reuse a deleted ID. | Rejected by the application/model boundary; original data/order/registry unchanged. |
| N03 | No-op root replacement with equivalent records; owned property/index/alias updates; independently change dynamic index then each operand. | Values follow intended ID/owner, sound root/path invalidation, no stale nested DOM or sibling leakage. Extra scheduling may occur but cannot remount an unchanged branch. |
| N04 | Child mutates a borrowed parameter through nested aliases/loop iterators; unknown call in assignment RHS or computed target; nested local name collides with another binding. | Clean production compilation fails before runnable assets; useful source/component remediation. Preserve the exact [scope-leak regression](../../tests/fixtures/production/reference-app/regressions/scope-leak-rejected.jtml). |
| N05 | Title/note includes quotes, `&`, `<`, `>`, `</script>`, U+2028/U+2029 and non-ASCII text; repeated create/update/remount. | Exact safe visible data round-trip; parseable serialized assets, no executable injection or unexpected request. Invalid/dangerous URLs must follow the approved release URL policy if exposed. |
| N06 | Repeat Open/All and completion branch cycles 20 times; submit after each return. | Exact live instance/handler count, one action per submit, no stale-generation actions or accumulating subscriptions. |
| N07 | Edit while an unrelated row changes; use multi-character selection; filter/delete currently focused row. | Retained input/caret range/direction remain stable; removal follows focus policy and remount never steals focus. |
| N08 | Run under CSP without dynamic compilation; inspect mode/assets and instrument fallback/error/network paths. | Four clean assets, formal production mode, no legacy plan references/globals or generic fallback, zero unexpected runtime activity. |

For each reactive expression, specify expected lexical/component observable
bindings independently, then require **required dependencies ⊆ compiled
dependencies** across Expression IR, Runtime Plan and generated update selection.
Compare actual browser outcomes after changing inactive operands before guards.
Use negative controls against the known defective compiler when validating a
repair; record the exact command, failing assertion and restored pass. Do not
copy emitted read sets into the expected oracle.

## T1/T2 application acceptance (v0.4; specification only)

Use a separately deployable HTTP service and durable database with a documented
request/response contract, not compiler-owned mock persistence. A deterministic
service test adapter controls response order/failures and logs mutation IDs.
The backend remains a conventional service; no JTL backend/auth syntax is required.

Proposed HTTP model: server-issued immutable IDs and record versions, idempotency
keys for creates/mutations, validation errors, 401/403/404/409/5xx responses and
an authenticated current-user boundary. Decide exact API representation before
implementation; the observable outcomes below are fixed requirements.

| ID / milestone | Deterministic scenario | Expected outcome |
| --- | --- | --- |
| A01 / T1 | Independent API starts with seed; create/edit/toggle/reorder/delete, then reload and open a second client. | Server data and IDs persist; order/counts/notes agree across reloads. Server failure cannot be shown as success; documented reconciliation strategy preserves or rolls back pending UI. |
| A02 / T1 | Delay initial GET, return 500, retry with success; test offline and timeout separately. | Accessible loading indicator, distinct recoverable error with retry, then correct data. No misleading empty-board state during loading/error; retry does not duplicate records. |
| A03 / T1 | Request task/filter A, then B; complete B before A. Delay an old edit response until a newer edit has committed; delete while a request remains pending. | Latest request/record version wins; stale responses cannot overwrite B/new data or resurrect deletion. Cancellation alone is not an oracle: deliberately deliver stale responses. |
| A04 / T1 | Submit invalid title; double-click Save and press Enter while pending. Server returns validation failure, then retry same logical create after a lost response. | Field-associated errors, preserved draft, one logical mutation with idempotency; no duplicate tasks. Controls/status communicate pending state and recover on error. |
| A05 / T1 | Load a task direct link from a fresh session, reload it, navigate back/forward; unknown ID and encoded/malformed route. | Correct task after loading/auth, stable history, deterministic not-found/invalid-route UI. Deployment serves entry assets on supported direct URLs; hash-only routing is explicit if chosen. |
| A06 / T1 | Server returns 404 on edit/delete and 409 version conflict from another client. | No silent overwrite; visible conflict/not-found recovery, ability to reload current version and preserve/copy draft. Counts/order reconcile to authoritative data. |
| A07 / T2 | Unauthenticated direct link, expired session during save, authenticated user A requests user B's task. | Conventional backend enforces 401/403 and authorization on every operation; UI shows sign-in/access state, preserves recoverable draft safely and never leaks B's data. Client route guards alone do not count. No secrets embedded in generated assets. |
| A08 / T2 | Build clean assets and deploy to a fresh documented environment with the independent API; reload and test cache/version changes. | Reproducible production build, correct asset/API base paths, direct navigation works, no compat fallback, clear server/network error handling. Source/package install instructions match deployed versions. |
| A09 / T2 | Run end-to-end C01–C14 and A01–A08 using real browser keyboard and HTTP; inspect accessibility. | Automated persistent CRUD, race, error, auth-boundary and deployment assertions; labeled controls, associated errors, visible focus, keyboard operation, announced loading/results, sensible heading order and focus restoration. Automated accessibility checks plus manual keyboard review recorded. |
| A10 / T2 | External developer follows clean setup with no private caches/context, changes one field/validation rule, rebuilds and redeploys using public docs. | Developer can install, run tests, locate ownership, diagnose a deliberate validation failure, make the change and maintain it without undocumented manual asset edits. Record OS/tools, steps, friction and test evidence. This is a v0.4 application gate, not an adoption prerequisite for the v0.3 preview. |

## Evidence and completion record

A future implementation report must map every C/N/A ID to test file, exact commit,
command/run artifact and observed outcome. Record unmet scenarios as failures or
unverified, including inability to express the required shape. A scenario cannot
pass by excluding its hard cases or routing them through compatibility.

T0 completion requires all C01–C14/N01–N08 plus existing production reference,
all nine mandatory real-browser/asset benchmarks, relevant CTests and separate
full local `scripts/verify_all.sh`. Capture versions, counts, skips and browser
process exit status. Preserve the reference HTML regression: 135374 bytes in the
last PR #10 record versus a 70000-byte **microfixture comparison**, not an approved
real-application budget. Neither T0 nor a green doctor declares a release ready.

This specification PR only inspects existing evidence and runs documentation
checks; it does not execute or pass T0/T1/T2. The
[preview release gates](v0.3-support-candidates.md#minimum-evidence-for-an-experimental-v03-preview)
remain a separate owner-approved decision.
