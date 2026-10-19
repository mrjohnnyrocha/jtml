# Local serve and Studio security contract

`jtml serve`, `serve --watch`, `dev`, `studio`, `tutorial` and `demo` use
loopback development servers. Run trusted projects only. Studio `/api/run`
and `/api/export` execute the submitted JTML with the local user's privileges.
This is not an execution sandbox or a production hosting service.

## Network and browser boundary

Both HTTP and WebSocket listeners bind to IPv4 `127.0.0.1`, never all interfaces.
HTTP port P must be 1–65455; WebSocket uses P + 80. The documented `localhost`
and `127.0.0.1` workflows work without new flags. There is no remote-access mode.
IPv6-only connections are not served.

Every HTTP route, including static assets, demo APIs and OPTIONS, validates
Host and browser request metadata before dispatch:

- Host must be `localhost`, `127.0.0.1` or `[::1]` with the exact listener port
  (port 80 can be implicit). Suffixes, trailing dots, userinfo and duplicate
  authority/origin headers are rejected.
- A present Origin must exactly match the HTTP Host's `http://` origin.
  Empty, `null`, foreign schemes, different ports and paths are rejected.
- Sec-Fetch-Site, when present, must be `same-origin` or `none` for HTTP.
  Consequently a cross-site request with no Origin is also rejected.
- No wildcard CORS headers are emitted. Rejection is enforced server-side,
  independently of CORS, including simple `text/plain` POSTs.

WebSocket handshakes validate Host against their own listener port and Origin
against the associated HTTP port. The three loopback aliases are allowed for
that HTTP origin: Studio srcdoc previews may inherit `127.0.0.1` while opening
`localhost` sockets. Cross-site/same-site Fetch Metadata is accepted here only
with a validated associated HTTP Origin. Foreign ports and opaque origins are
rejected before callbacks or state snapshots run.

Native HTTP and WebSocket clients may omit Origin and Fetch Metadata, while
still supplying an allowed Host. **Missing Origin is not authentication.**
Local processes can forge these headers. Trusted same-origin scripts and
project assets can operate the APIs; these checks do not defend against a
compromised project, malicious local process, browser extension, filesystem
access, resource exhaustion or arbitrary malicious JTML. No TLS, credentials,
per-tab isolation or general Interpreter thread-safety guarantee is provided.

## Synchronous execution and lock ordering

A process-wide recursive adapter mutex serializes interpreter construction,
HTTP runtime snapshots/actions, Studio POST handlers, watch transpilation and
reload, WebSocket open/message/close callbacks and broadcasts. Studio has one
interpreter per server; the most recent run/export replaces its shared program.
The demo login endpoint has no interpreter or shared mutable login state, but
receives the same request validation.

HTTP handler silencing of the global stdout stream is also inside that mutex.
WebSocket event dispatch can synchronously broadcast while holding it; recursive
acquisition on the same thread permits this. Watch takes the execution mutex
before the HTML mutex; root HTTP GET takes only the HTML mutex. No callback
waits for the WebSocket thread to join while holding the execution mutex in
these supported synchronous adapter paths. Server destruction stops the socket
loop and joins its thread after request handling ends.

PR #14's per-interpreter function-call mutex is retained unchanged. It protects
only entry/exit bookkeeping, releases before user code and rejects overlapping
function execution by another thread. The adapter lock precedes this short
bookkeeping lock; no user callback or broadcast runs while that bookkeeping
lock is held. This does not turn PR #14's ownership check into a scheduler.

Async functions now use Interpreter-owned queued execution under the same live
adapter lock. Reset/reload waits for active execution and invalidates pending
old-program tasks; destruction drains without holding execution locks. This
supersedes PR #15's detached-worker limitation. See the
[async lifetime contract](../architecture/async-worker-lifetime.md) for lock
ordering, cancellation/failure reporting, finite-drain and embedding boundaries.
It does not establish arbitrary concurrent access through escaped mutable handles.

SIGINT/SIGTERM termination and immediate reuse of both listener ports are
integration-tested. The test accepts the OS signal exit status; it does not
prove graceful draining of active requests or async work on process termination.

## Reproducible evidence

```sh
ctest --test-dir build -R LocalServerSecurity --output-on-failure
scripts/test_local_server_security.sh
scripts/verify_all.sh
```

The integration test requires Node, Playwright/Chromium and an available local
network interface; missing coverage fails rather than skips. It uses real HTTP,
raw masked WebSocket frames and a browser. Existing browser selection variables
`JTML_BENCH_BROWSER` and `JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT` are supported.
It covers both host aliases, native requests, denied authority/origin/duplicate
headers, external-interface refusal, Studio srcdoc preview, HTTP clicks,
WebSocket events, concurrent HTTP/WS increments, snapshots during reload,
watch/start/stop and immediate port reuse. The Studio browser oracle uses the
real server origin and returned preview HTML independently of optional Monaco
CDN startup; full editor/CDN availability is UNVERIFIED by this test.

Negative control on the previous server implementation: foreign Host/Origin
POST `/api/run` returned 200, emitted `Access-Control-Allow-Origin: *` and set
`count` to 987; an untrusted WebSocket upgraded with 101; both listeners accepted
connections through the machine's non-loopback address. With this implementation
the same requests returned 403, no wildcard header, no execution, and both
non-loopback TCP connections failed. To repeat against an older binary, set
`JTML_BIN=/path/to/old/jtml scripts/test_local_server_security.sh`; security
assertions must fail. This is a negative control, not a passing old implementation.

The full local gate and Ubuntu/macOS hosted CI are separate evidence. Known
intermittent `friendly_store` native crashes must remain visible if encountered;
they must not be converted into success with retries or skipped checks.
