# Literal fidelity and interpreter function calls

Base: main `b164ab7`, merged PR #13. This repair changes no Taskboard acceptance
requirements, C01–C08 oracles, keyed lifecycle/commit recovery, or asset budgets.
C09–C14 remain unverified and are outside this PR.

## Browser source/value boundaries

`jsonString()` correctly emits JSON-safe `\u0026`, `\u003c` and `\u003e`.
The lexer used to discard the backslash instead of decoding it. Friendly lowering
also re-quoted still-escaped contents, and browser source-expression evaluators
removed delimiters without decoding escapes. These boundaries must agree:

- `decodeStringLiteral()` normalizes JTML single/double quotes and raw multiline
  whitespace to JSON, then validates/decodes with the existing JSON library.
  JSON escapes, UTF-8 and surrogate pairs retain their values. JTML's escaped
  apostrophe remains accepted; unknown/dangling escapes and invalid Unicode fail.
- Friendly lowering decodes literal fragments before safe re-serialization. It
  distinguishes individual quoted terms from concatenation and preserves quoted
  component arguments inside interpolation. Raw payload decoding shares the
  helper. No JSON-safe escaping was disabled.
- Browser source plans, client expressions and quoted bracket keys use the same
  small JSON-based normalization/decoding rule. No eval, Function constructor or
  markup-based decoding is used. Compiled typed literals continue to carry values.
- A hostile `-->` literal exposed unescaped diagnostic comments in the transpiler.
  Their source text now passes through the existing HTML escaper. This prevents
  comment termination from creating markup before runtime initialization.

The [independent Chromium oracle](../../tests/browser/literal_fidelity.cjs) tests
14 fixed values in clean production and explicit compatibility builds: top-level
and component-local state, both derived expressions, reactive updates and a
reactive attribute. This is 140 exact value comparisons, including ampersands,
angle brackets, both quotes, trailing backslashes, escaped versus literal control
sequences, NUL/control bytes and Unicode. Script/comment-closing image payloads
must stay text: no injected nodes, executable flag, unexpected requests or errors.
The [runner](../../scripts/test_literal_fidelity.sh) also rejects four malformed
browser-source cases with diagnostics and a subprocess timeout, scans assets for
runtime eval/Function, and enforces the unchanged shared-runtime budget. It is
mandatory in Ubuntu/macOS CI and the full local gate. Existing production-reference
CSP tests remain mandatory.

## Interpreter call-stack ownership

`executeFunction()` held a process-wide non-recursive mutex throughout the body.
Any nested call deadlocked, including recursion and calls in another interpreter.
The process-wide depth counter also lacked exception-safe restoration.

A per-interpreter frame now owns depth, calling thread, previous environment and
function context. A brief mutex protects entry/exit bookkeeping; no lock is held
across user execution. The frame restores all saved state on return or exception,
including allocation/parameter binding failures before the body handler. The
maximum active depth is **128** to bound native-stack use; it counts all function
frames, not only self-recursion. Ownership/depth failures propagate past internal
statement error handlers instead of being logged and converted into success.
Public interpreter/frontend/async reporting boundaries retain their diagnostics.

This is **not general interpreter thread safety**. Callers must externally
serialize public operations and keep the interpreter alive through callbacks.
Overlapping function entry from another thread is rejected explicitly. Detached
async scheduling/lifetime and arbitrary concurrent reset/environment access were
not redesigned or certified safe.

Six [CTest subprocess probes](../../tests/interpreter_call_probe.cpp), each with
a hard 10-second timeout, cover simple/nested calls, bounded recursion, repeated
excessive recursion with depth/environment/context recovery, independent
interpreters and rejected overlapping function entry. The concurrency gate pauses
a function only after frame entry, not before the ownership check. No live port or
browser is required for these CI regressions.

## Negative controls and verification

On unmodified base main, clean-production Chromium rendered `Tom u0026 Jerry`
for `Tom & Jerry`. The saved main binary also fails the final browser oracle.
The original four subprocess probes on pre-fix interpreter code passed only the
simple call; nested, recursive and excessive cases each timed out after 10 seconds.
Actual `jtml serve` became healthy for the simple fixture but timed out after five
seconds for `outer()` calling `inner()`; both become healthy after repair.
A separate intermediate negative control inserted executable image markup by
closing a diagnostic comment; the final oracle rejects that behavior.

Two early full-gate attempts stopped on lowering regressions exposed by the
stricter decoder: a tutorial quoted concatenation and demo component interpolation.
Both were corrected at Friendly lowering, retaining the existing examples/tests.
Final validation results and hosted CI are recorded below and in the PR.

The known native `friendly_store` malloc/SIGABRT/SIGSEGV failure remains a separate,
pre-existing issue. A passing startup does not establish its repair; these scoped
literal/call-stack changes make no claim to fix native heap corruption.

Local macOS arm64 Release validation (Node 22.23.3, Playwright 1.63.0,
Chrome Headless Shell 153.0.8010.12, Python bindings off):

- Final focused tests **14/14**; final full CTests **357/357**.
- `scripts/verify_all.sh` **PASS**, including complete production reference,
  C01–C08, both original P1 lifecycle regressions, four commit-recovery cases,
  all nine required browser benchmarks with clean exits, and live smoke **8/8**.
  No check was skipped. Store smoke passed; the intermittent native bug remains
  unresolved. An earlier successful full gate had 356 tests before adding the
  standalone lowering regression; the settled final gate includes all 357.
- Clean/compat literal matrix and XSS controls **PASS**; four malformed sources
  rejected. Existing CSP/static-generation tests and asset budgets **PASS**.
- One manual CTest attempt overlapped test-binary linking and reported killed /
  not-run processes. It is invalid evidence; the settled final full gate above
  rebuilt first and passed all tests. No test was disabled or weakened.
- Node/shell syntax, Markdown/local links and `git diff --check` checked.
  Original acceptance contract and Taskboard/lifecycle browser oracles unchanged.

| Asset, same source on main and candidate | Main bytes | Final bytes | Delta |
| ---------------------------------------- | ---------: | ----------: | ----: |
| Shared runtime                           |     271114 |      271629 |  +515 |
| Production-reference HTML                |     135374 |      135374 |     0 |
| Production-reference component module    |     112418 |      112418 |     0 |
| Taskboard HTML                           |     712195 |      712771 |  +576 |
| Taskboard component module               |     244488 |      244488 |     0 |
| Bootstrap, both applications             |        619 |         619 |     0 |

The shared runtime has 371 bytes of headroom under **272000**. Taskboard HTML
increases because diagnostic comment quotes are escaped. The reference HTML's
pre-existing excess over the 70000-byte microfixture budget remains explicitly
reported by the reference runner; it is not reclassified or repaired here.

Ephemeral logs: `/tmp/jtml-pr14-verify-final.log`,
`/tmp/jtml-pr14-focused-final.log`, `/tmp/jtml-pr14-literal-negative.log`,
`/tmp/jtml-pr14-literal-negative-final.log`,
`/tmp/jtml-pr14-interpreter-negative.log`, and
`/tmp/jtml-pr14-live-{negative,positive}.log`. Hosted Ubuntu/macOS CI is separate
final-head evidence, recorded in PR #14. No automatic merge.
