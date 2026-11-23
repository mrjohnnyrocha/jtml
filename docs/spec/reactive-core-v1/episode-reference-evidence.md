# Independent strict scalar episode comparison

This test-only slice uses the private scalar compiler from PR #103. The
four-vector Python model now also traces synchronous root episodes, including
numeric parameter frames, same-value writes, acyclic effects and an explicit
experimental P3 transition. The 24 original vectors, public compiler, runtimes
and 273,000-byte cap are unchanged. Browser Core v1 remains **NO-GO**.

## Model and observations

The [Python evaluator](../../../tests/reference_model/model.py) shares the
original model's resolved AST, scalar evaluator and immutable commit snapshots.
It substitutes getter formulas over the current source cells, uses a separate
parameter frame, records executed writes even when values are equal, and
enumerates complete effect permutations in source order to find the first order
satisfying every potential write-to-watch edge. It admits at most six root
effects per fixture. Its trace records notifications, effect entry and exit,
typed show values, complete successful commits and failed partial source state.
No production AST/evaluator, generated script, scheduler, Runtime Plan, browser
result or catalog expectation is used to compute that trace.

The old four [reviewed AST fixtures](../../../tests/reference_model/fixtures)
are reused. The [extended fixture crosswalk](../../../tests/reference_model/episode_fixtures.cjs)
hand-transcribes the remaining ASTs and identifies executable source lines,
resolved bindings, explicit text segments and button entries. SHA-256 locks
each file to the original source bytes. The driver checks the original action
and argument sequence, with one labeled post-fault click added to V19.
Static page/box/button lines identify the observation envelope and dispatch
transport. They are not interpreted as a second UI frontend.

The [Chromium adapter](../../../tests/browser/core_reference.cjs) compiles those
same bytes using the private compiler and serves its assets unchanged with
`script-src 'self'; object-src 'none'; base-uri 'none'`. It drives real button
clicks. It samples typed source and derived values by the reviewed single-module
binding names. For effect entry, a read-through accessor records the body selection
and watched scalar. The manifest is used only to map that physical action address
to the independently reviewed effect identity. The adapter also captures
console show text, rendered text at show/effect entry, and passive
`MutationObserver` snapshots. It trims static HTML envelope whitespace for this
text-only observation. Non-finite browser numbers and signed zero cannot be
silently turned into JSON null or equated with a Boolean or string.

## Demonstrated matches

| Fixtures | Compared evidence |
| --- | --- |
| V01, V03–V05, V08, V11 | Typed state/getters and settled DOM for repeated dispatch, fresh reads, chain and diamond formulas, conditional reads and source-ordered initialization. Passive samples of changed DOM show the final projection. |
| V02, V06, V07, V17, V20 | Coalesced and same-value writes, invocation-local shadowing, effect entry order/typed watch values, and ordered show text. In V20, the model and browser both show `2, 4`, observe the effect with `x=2`, and settle the DOM at `2\|4\|4`. |
| V12–V15, V18 | Matching rejection category and original location, with no runnable assets. V18 maps the compiler's source-located cycle wording to the model's `E_EFFECT_CYCLE` category. |
| `typed-order` | Boolean/string/number state, guarded arithmetic, branch execution, and a downstream effect written in source before its producer. The observed entry order is E2, E1, E3, including the source-order choice between E1 and E3 once E2 is done. |
| V19, `failure-prefix` | **Experimental P3 only.** Private FAILED status and arithmetic error, unchanged last-successful DOM and publication revision, cleared pending writes, and inert subsequent clicks. In the latter case the action and first effect retain `x=1,y=2` as failed partial state. Show text remains `1, 4, failing`. The failing write leaves `stopped=0`; the already-eligible second watcher of y and the recovery action do not execute. |

## Confirmed divergence under the experimental transition rules

[The repairing effect witness](../../../tests/fixtures/core_reference/derived-repair.jtml)
boots with `trigger=1, divisor=1, quotient=1`. The action writes `trigger=2`
and then `divisor=0`. The model evaluates getters only when read or at final
publication. The eligible effect shows `repair`, sets `divisor=2` and the
model commits `0.5|2`. The browser instead raises
`E_NON_FINITE_ARITHMETIC`, remains at DOM `1|1`, retains failed partial source
`trigger=2, divisor=0`, and never enters the effect. The runtime calls
`applyClientDerived()` before `flushRootSubscriptions()` in
[`applyClientState`](../../../src/browser_runtime_assets.cpp).
This is a reproduced mismatch in fault timing; A6a/A6c and the required
stabilization point remain provisional. The research comparator reports
`DIVERGENCE` and returns nonzero. The constructive comparison gate and all
existing mandatory tests run separately.

## Missing observability and exclusions

- **UNVERIFIED** for logical commit identity/count, all observer or paint
  boundaries, and generalized glitch freedom. Passive callbacks provide
  positive evidence at sampled boundaries. No callback yields no passive
  sample. The private publication revision is a diagnostic and is not compared
  to the model's commit sequence.
- **UNVERIFIED** for notification delivery distinct from effect entry, effect
  exit, show scalar kind/statement identity, coherent derived state after
  failure and a direct UI dispatch return. Settled values, body reads, rendered
  text and failed-source diagnostics establish the bounded observations above.
- The model supports exact safe decimal-integer literals and finite arithmetic,
  Booleans, plain ASCII strings, resolved reads, Boolean conditions,
  short-circuiting, homogeneous operators and literal/read render segments
  for reviewed single-root-module fixtures. Text conversion is exercised
  within those fixtures; general numeric formatting and interpolation escapes
  remain outside the comparison. It excludes imports and name collisions,
  components and generations, collections, computed click arguments, boot
  faults, asynchronous or host behavior, and P1/P2.
- A6a scalar policy, A6b general interpolation and A6c publication, recovery
  and termination remain **unratified**. The model requires explicit
  experimental selection and otherwise returns `PENDING_POLICY`.
  V19's original expectation still has no selected policy.

## Reproduce

Run `scripts/test_browser_local_runtime.sh` with a built test toolchain, real
Chromium and the existing Playwright environment. It runs the model tests,
historical fixture audit, comparator tests and the current browser comparison.
The browser trace and both sides of each check are written to
`dist/core-reference/results.json`. Ubuntu and macOS CI retain this file.

With the same browser environment configured, the separately failing
counterexample is reproducible using the emitted private runtime:

```sh
JTML_CORE_PRIVATE_COMPILER="$PWD/build/jtml_browser_private_compiler" \
  node tests/browser/core_reference.cjs --research
```

Expect `derived-repair DIVERGENCE`, exit 1 and
`dist/core-reference/research.json`. The command makes no runtime changes.
