# Owned interpreter async execution (issue #16)

## Supported behavior

Classic `async function` calls still return an empty value without waiting for
execution. A lazily started, Interpreter-owned worker executes admitted tasks in
FIFO order. Tasks hold their function, argument and receiver references. Nested
async calls enqueue work; they do not execute inline. There is one worker per
Interpreter, not parallel execution of functions sharing mutable environments.
This preserves PR #14's synchronous nested/recursive frames and its rejection
of overlapping raw function entry from another thread. Raw `evaluateExpression`
entry from a different thread during an active function is explicitly rejected;
adapter operations and copied JSON snapshots wait at their outer execution guard.

Live interpreters use the same process-wide recursive execution lock as PR #15's
HTTP/WS/watch adapters. Headless interpreters use per-instance execution locks;
independent headless instances can progress independently. Public interpretation,
state/binding/component snapshots, events, expression evaluation and lifecycle
replacement serialize with worker execution. Recursive snapshot/event/function
callbacks are allowed on the owning thread. Mutable Environment/VarValue handles
returned to embedders are borrowed; concurrent mutation through those escaped
handles is outside this guarantee. Borrowing does not extend the Interpreter
or renderer lifetime; using escaped handles after destruction is unsupported. External callers must keep the Interpreter
and its referenced transpiler alive for each call. Mixing an embedding host's
unsynchronized global stream-buffer replacement with interpreter execution is
also unsupported. Reentrant callbacks are supported on the same interpreter;
host callbacks spanning multiple headless instances must establish their own
lock order. Arbitrary cross-instance callback cycles and host-held locks are
not a general deadlock-freedom guarantee.

## Public-operation serialization audit

| Public operations | Protection / escaping state |
| --- | --- |
| `interpret` overloads, `reset`, `reload` | Execution lock; reset/reload also reject active function reentry. |
| `dispatchEvent`, `dispatchComponentAction`, `handleFrontendMessage`, `populateBindings` | Execution lock, recursively compatible with live adapters/broadcasts. |
| `getStateJSON`, `getBindingsJSON`, `getComponentsJSON`, `getComponentDefinitionsJSON` | Execution lock; return copied JSON strings. State serialization also updates render-generation metadata. |
| `evaluateExpression` | Execution lock plus PR #14's raw overlapping-entry rejection. The returned shared value may alias mutable environment data. |
| `getCurrentEnvironment` | Execution lock protects pointer access; the returned shared environment is borrowed mutable state. |
| `getNumericValue`, `getStringValue` | Execution lock; borrowed input values must not be mutated outside it. Results are copied scalars/strings. |
| `waitForAsync` | Rejects any held framework execution lock; waits with the queue mutex released. |
| Construction / destruction | Publish only after construction; external callers coordinate object lifetime. Destruction closes admission and joins without execution locks. |

## Reset, reload and shutdown

Reset/reload acquires the execution lock, so an active task completes first.
It increments a program generation before replacing environments. A queued or
already popped task from an older generation is cancelled before entering user
code; it cannot update the replacement program. Cancellation is reported as an
async diagnostic. No worker is joined/drained during reset or reload. A lifecycle
replacement reentered from an active function callback is rejected before any
state changes, because that frame would otherwise restore a stale environment.

Destruction closes external async admission, wakes the worker, and joins it
without holding either execution lock. Already admitted tasks and descendants
of the draining worker finish before renderer, environments or interpreter
storage is destroyed. Then the WebSocket event loop is stopped and joined.
The listener remembers a stop request that arrives before startup completes.

Finite task execution and a finite descendant task graph are required for
draining to finish. There is no unsafe
thread cancellation, preemption of infinite JTML loops, or timeout on blocking
native operations. Deleting the Interpreter from its own callback, or destroying
it while holding an execution lock, is invalid and terminates with an explicit
diagnostic instead of attempting a self-join/deadlock. Arbitrary external API
calls racing object destruction remain invalid C++ object-lifetime usage.

`waitForAsync()` is an embedding/test completion boundary: it waits until the
queue and active task are empty, then throws a combined diagnostic for failures
and cancellations since the previous wait. Worker diagnostic handlers also
report to stderr, so CLI users do not need to call this API to observe failures.
A subsequent wait does not repeat consumed errors. HTTP event success acknowledges
dispatch, not successful completion of deferred user code. Calling the wait API
from an execution callback or while holding an adapter/execution lock is rejected.
Admission concurrent with wait has normal queue linearization: work admitted
after the empty observation belongs to a later wait.

## C API resource ownership

C API replacement drains its current interpreter before compiling/interpreting
another program, and destroys that interpreter before releasing its referenced
transpiler. Worker failures at this completion boundary return through the
existing `jtml_load` error result; the old context is retained and the diagnostic
is consumed once. New-program async calls remain deferred. The native renderer
no longer logs from its destructor: that shared-output write could overlap the
replacement runtime’s worker even after old work had drained.

The added C API subprocess exercises queued native rendering, repeated load,
destruction and failure propagation. A saved ASan-instrumented owned-worker
candidate retaining the old C API assignment order aborted with heap-use-after-
free in `JtmlTranspiler::getNodeBinding`; the corrected probe passes ASan/UBSan.
The extended TSan probe also exposed the old/new-runtime diagnostic overlap,
which the replacement fence and quiet native teardown remove. No sanitizer
suppression hides either failure. These C API checks extend the original 18
probes to 19.

Hosts must externally serialize opaque-context API calls and keep a context alive
for the complete call. Concurrent pointer replacement/destruction versus context
access is not covered by per-interpreter state locking. Unrestricted parallel
embedding of unrelated headless runtimes (including shared diagnostic output)
remains outside this verified matrix; independence of function frames is not a
claim of unrestricted process-wide concurrency safety.

## Lock ordering

1. Execution lock, then short function-frame bookkeeping or async queue lock.
2. Worker pops a task under the queue lock, releases it, then acquires execution.
3. Completion publishes idle state under the queue lock after releasing execution.
4. Wait releases the queue lock while blocked on its condition variable.
5. Destruction closes admission under the queue lock, releases it, then joins.

Function-frame bookkeeping is never held across user code or broadcasts.
A thread-local acquisition count tracks all framework execution locks held by
the caller, including another headless interpreter’s lock. Wait/destruction
reject while that count is nonzero, covering cross-interpreter callback waits
without relying on a possibly spurious `try_lock()` result.
Recursive broadcasts retain PR #15's connection-registry protection. HTTP Host,
Origin, Fetch Metadata, CORS and loopback policies are unchanged.

## Regression evidence and scope

`InterpreterAsync.boundary` is also compilable against pre-fix main. A condition
variable pins a real async call inside its frame; another thread can acquire the
adapter lock on old code, proving reset/snapshots can race that frame. It exits 1
with that diagnostic. The fixed worker excludes adapter access and passes. The
negative-control process exits before freeing state under its pinned old worker;
this proves the unsafe boundary, not a sanitizer-confirmed old use-after-free.

Other subprocess modes cover deferred/nested execution, cancellation, active
reset/reload, concurrent snapshots, recursive broadcast callbacks, failure and
arity diagnostics, destruction while work is pinned, nested admission during
drain, and forty create/execute/destroy cycles. Separate five-second subprocess tests
prove unsupported self-destruction and destruction under an adapter lock report
the diagnostic and exit through the terminate handler (86), rather than hanging.
These are explicit rejection boundaries, not supported destruction patterns. Ordering uses condition-variable
barriers or execution locks; CTest hard timeouts turn deadlocks into failures.
`scripts/test_async_runtime.sh` exercises real HTTP/WS async dispatch in serve,
Studio and watch with concurrent updates/snapshots and PR #15's security negatives.

ASan/UBSan, separate TSan, full local gate and final-head Ubuntu/macOS CI results
are recorded in the PR report. No browser rendering or Taskboard source/contracts
are changed. Browser benchmarks and existing budgets remain mandatory. The known
intermittent native `friendly_store` crash remains separate until evidence links
it to this defect; failures must not be hidden by retries or skips.

## Validation record (macOS, Apple Clang 15)

The final ASan/UBSan and separate TSan runs each passed all 19 async/function
probes with `halt_on_error=1`; UBSan also enabled stack traces. No sanitizer
suppression, timeout increase or retry loop was added. Sanitizer results cover
these isolated probes, not arbitrary embedding code or a complete network-stack
certification. LeakSanitizer and sanitizer execution on other platforms remain
unverified by this local record.

An earlier refreshed TSan run correctly found a fixture borrowing violation:
the portable probe mutated Environment outside the execution lock while WS
startup logged. Fixture mutation and logging now use the required lock. The
same run timed out after the synchronous nested-call probe printed PASS and
renderer destruction; an isolated ten-second subprocess check exited 0 in
0.46 seconds. The earlier finalization-timeout cause remains unestablished;
it is not counted as a passing run or proof of a native interpreter deadlock.

The first full-gate attempt stopped at a missing cached Playwright package.
Playwright 1.63.0 was restored, and an actual Chromium 153.0.8010.12 launch and
clean close were verified before the complete gate. One development HTTP run
also reported a broken pipe during watch concurrency while headers/builds were
changing; no native crash diagnostic was captured. Frozen-source HTTP/WS runs
subsequently passed all three modes. Earlier failures remain recorded separately
from final passing evidence; they are not silently converted to passes.
