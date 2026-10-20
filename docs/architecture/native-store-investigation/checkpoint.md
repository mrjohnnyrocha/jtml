# Native store investigation checkpoint — 2026-10-20

Current branch `codex/native-store-investigation`, main base `094e530`.
Worktree `/Users/joaorocha/.codex/worktrees/native-store-safety/jtml-v1`.
Historical comparison worktree remains detached and clean at `08fd449`:
`/Users/joaorocha/.codex/worktrees/native-store-investigation/jtml-v1`.
No source fix or root-cause attribution is established.

Recovered original macOS reports under
`~/Library/Logs/DiagnosticReports/Retired/`:
- `jtml-2026-10-18-154102.ips`: SIGABRT, malloc free-list checksum botch,
  detected while FunctionDeclarationNode::toString allocates/formats output.
- `jtml-pr12-a74d88c-2026-10-18-154231.ips`: SIGSEGV at address 0x20 in
  ObjectPropertyAccessExpressionNode::toString, during cloned function body
  logging in interpretFunctionDeclaration.
Both show a second thread in the WebSocket Asio loop. Neither identifies the
corrupting operation. Old temporary failure logs/binaries are gone.

The store fixture has no async function call. PR #19's C API replacement UAF
is therefore not an established explanation of this startup path.
A surviving candidate is historical unsynchronized stdout stream-buffer
redirection: serveOnce creates SilenceStdout while the WS startup thread logs.
Its sink is std::ostringstream, whose concurrent mutation is not guaranteed safe.
PR #15 now serializes the CLI construction/interpretation guard and WS startup
logging. This source observation is a candidate mechanism, not proof it caused
these crashes; current successful starts cannot establish a historical repair.

Next: instrument the preserved historical startup path, retain all process
outcomes, use hard timeouts, then reduce any sanitizer finding with barriers.
Compare current main without modifying provenance worktrees. No repair PR unless
the corrupting operation and before/after negative control are demonstrated.

## Confirmed findings and implementation

Historical `08fd449` is byte-identical to saved failing revision `a74d88c` in
interpreter, AST/parser/lexer/Friendly, adapter, CLI stream guard and legacy
runtime sources. Its fully instrumented ASan/UBSan startup experiment was ready
12/12 without a sanitizer finding; this does not establish safety. Separate TSan
startup failed 12/12, confirming concurrent interpreter/WS writes to redirected
stdout. Deterministic two-writer reduction using the real `SilenceStdout` guard
produces ASan **double-free** and a TSan race; mutex-guarded controls exit 0.

Current main `094e530` also fails TSan startup 12/12: the WS log holds the adapter
mutex, but the CLI startup banner accesses stdout without it. This surviving
race is directly demonstrated, not inferred from stress success. The fix wraps
serve/watch/Studio banner output and final flush in the existing adapter lock.
The lock is released before listen/join/drain. Interpreter/security behavior and
Host/Origin policies are unchanged. Fixed actual serve TSan startup: 12/12 ready,
no sanitizer finding. A deterministic audit of real command banner/flush lock
ownership fails the original serve/watch/Studio adapters with exit 86 each and
passes the fix. HTTP/WS readiness and shutdown remain separate full-gate tests.

One early regression invocation was BAD_COMMAND because Python discovery was
missing in tests CMake scope; explicit required test-only Python discovery fixes
that harness error. Earlier failed logs remain in `/tmp/jtml-native-banner-*`.
Release focused test PASS. Full gate running `/tmp/jtml-native-fix-final-gate.log`.
Full ASan/UBSan and TSan banner probes running; not final until results complete.

The logging race and its ability to corrupt the shared sink are proven. Exclusive
attribution of the original malloc checksum/SIGSEGV reports remains UNVERIFIED;
this report does not claim all historical heap corruption is fixed. Original
reports detect corruption, not necessarily its first invalid operation.

## Final local validation

Full native-fix local gate **PASS**: 376/376 CTests, nine real browser benchmarks
with clean exits, production references, C01–C14, lifecycle, literal fidelity,
security/async adapters, package/editor/LSP/site checks and live smoke 8/8.
This branch starts from main independently of the provenance PRs, so its inherited
N04 is still BLOCKED; the normal gate's zero exit is not T0 acceptance here.
PR #22 separately establishes strict N04 and T0 on the provenance stack.

Deterministic banner audit passes serve/watch/Studio under Release, full
ASan/UBSan and full TSan. Fixed real ASan/UBSan serve startup is ready 12/12;
fixed TSan serve 12/12, watch 3/3, Studio 3/3. No sanitizer suppression, retries
to mask a runtime failure, raised timeout or budget changes. Initial standalone
Studio readiness used nonexistent `/api/health`, producing 3 false readiness
results with live processes; corrected `/` oracle passes and both sets are kept.
Signal termination in startup experiments proves process/port teardown, not
C++ destructor draining. Existing lifecycle and async probes retain that separate
coverage; no general external host/stream-replacement concurrency is claimed.

The confirmed current-main race is repaired. Historical redirected-output
concurrency can cause double-free, but exclusive attribution of all original
heap-corruption reports remains UNVERIFIED. Do not relabel the general historical
native corruption as proven fixed from successful startups.

Evidence JSON, representative original failures, reduction and guarded controls
are committed alongside this record. Full logs remain at paths recorded there.
Next: commit/push focused native repair, obtain exact-head Ubuntu/macOS CI, then
optionally validate the combined unmerged native/provenance tree locally.
