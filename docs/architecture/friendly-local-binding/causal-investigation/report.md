# PR25 final-gate causal investigation

Verdict: FIX FIRST. The original reset mechanism remains UNVERIFIED. No compiler,
interpreter, adapter, timeout, concurrency assertion or budget change is made by
this investigation. Passing later runs do not replace the original failed gate.

## Exact recovered failure

Original /tmp/jtml-pr25-final-gate.log: 152552 bytes; SHA256
5a74a52efe93df02894b6863e8237fc8eb426570e9e83547b9f4c4089a1a3cfc.
The Python stack is test_async_runtime.py:35 -> check_concurrency at
 test_local_server_security.py:245 -> post:36 -> request:29 ->
HTTPConnection.getresponse -> response.begin -> readline -> recv_into.
It is one of twenty HTTP POST /api/event response-header reads, concurrent with
twenty submitted WebSocket events. It is not a state snapshot, an intentional
watch reload or teardown operation. The code reaches snapshots only after all
HTTP futures complete, and this async fixture never rewrites its source file.

The test's except handler prints only the last 16000 bytes of server output.
Its temporary directory is deleted. The original record retains no precise
per-request timestamps, port number, failed request ordinal, live process poll,
listener probe or post-failure state. Count 35 in buffered log output is not
proof of final state or lost mutation. No unexpected exit assertion was raised,
but the lifecycle accepts exit 0 as well as requested signal termination; the
previous assertion that the process was certainly alive was too strong.
No retained crash report establishes a memory failure. Original lifetime,
listener and state at the reset remain UNVERIFIED.

## Implementation boundary

serve --watch calls parseProgramFromFile(inputFile, Auto), loadCompilationSource
and normalizeCompilationSource. The Classic fixture has no Friendly header or
component; its selected Classic branch appends the mapped source unchanged.
friendlyToClassicWithSourceMap (and substituteIdentifiers) runs only for Friendly.
The watcher uses collectSourceFiles and file timestamps; all handlers/WS callbacks
and the live worker acquire the shared execution mutex. Lifecycle waits are
outside worker execution locks. POST /api/event dispatches the existing binding
then snapshots state; exceptions become JSON errors, not a deliberate reset.
HTTP admission/stream close paths remain in vendored httplib. No finding locates
an original socket close in one of those paths, so no such hypothesis is claimed
as the cause. Interpreter and server sources are byte-unchanged from main.

## Matched native A/B

Main 100c618 and PR c7b3d99 were built in separate worktrees/build directories,
Release, Python off, same pinned dependency sources and compiler. Binary hashes
are in summary.json. Python3.14 and the same existing HTTP/WS helpers and Classic
fixture were used. Browser versions do not affect these native-only cases; the
full gate retains Playwright1.63/Chromium153 and unchanged browser requirements.
Port allocation uses the same ephemeral paired-port helper for both binaries.

Predetermined ABBA x4: main 8/8 PASS; PR 8/8 PASS. Each run retained all HTTP
request start/end monotonic timestamps, method/path/status/error, state 40,
server logs, port and accepted shutdown status. No failed request is retried.
No failure occurred to trigger the added process-poll/failure-snapshot recorder.
These results do not reproduce or explain the original failure on either tree.
Artifacts: /tmp/jtml-pr25-causal/{results.json,ab.log,NN-label/record.json,
NN-label/watch.log}. Recorder: /tmp/jtml-pr25-causal/ab.py.

## Deterministic negative transport control

For each binary, a one-shot proxy forwards one POST to the real watch server,
then signals an Event after the backend response. The parent reads state at this
barrier, releases the proxy, and the proxy closes with SO_LINGER reset before
sending response headers to the client. No timing sleep triggers this control.
Both clients receive ConnectionResetError(54), while state is 1 before/after the
reset and subsequent direct HTTP returns 200. This demonstrates why the exception
alone cannot prove backend death or lost mutation. It is deliberate fault
injection, NOT a reproduction or attribution of the original transport reset.
Artifacts: rst_control.py, rst-results.json, rst-control.log in the same directory.

## Validation interpretation

Seven semantic resolutions and nine reviewed defects remain unchanged. No native
runtime repair is attempted to obtain a green gate. The requested fresh complete
local gate is recorded separately as full-gate-investigation.log, retaining the
original failed gate as unresolved regardless of its outcome. Hosted CI on
c7b3d99 passed both platforms (run39791323953). Further causal evidence, such as a
captured close/packet trace during an actual un-injected failure, is needed before
attributing or clearing the original boundary. Sanitizers were not used as a
substitute for missing native transport evidence.
