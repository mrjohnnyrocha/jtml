# Reactive Core commit and effect observations

The browser adapter executes the unchanged V02, V16, V17, and V20 sources and
reads their expectations directly from the [authored catalog](../../spec/reactive-core-v1/vectors.json).
The comparison baseline is merged main at
`1aca51f9f9b860b816c425e68f22dca6cccc40e2`. The available PR #83 binary came from
`5cff90a1a80cabae67704054c627428abd71bf75`, whose tree is identical to that merge.
The local browser was Chromium 156.0.8078.80. The HTTP response enforces
`script-src 'self'` and Playwright drives the actual page controls.

| Vector and phases | State, derived and settled DOM | Effect entries and watched value | `show` | Synchronous writes to observed DOM | Lifecycle |
| --- | --- | --- | --- | --- | --- |
| V02 load, Step | PASS `0\|0 → 2\|1` | PASS empty, then E1 at x=2 once | PASS empty, then 2 | PASS final projection at each sampled Step write | — |
| V16 load, Local, Toggle, Bump, Toggle | PASS `0\|0 → 0\|1 → [] → [] → 2\|0` | PASS empty throughout | PASS empty throughout | Local writes `0\|1`. Remove clears the observer. Bump makes no sampled write. Creation inserts a placeholder before patching it | PASS initial create, destruction/cleanup, no generated update on Bump, fresh remount |
| V17 load, Step | PASS `0\|0\|0 → 1\|2\|3` | PASS empty, then E1 at x=1 and E2 at y=2 | PASS empty throughout | PASS final projection at each sampled Step write | — |
| V20 load, Step | PASS `0\|0\|0 → 2\|4\|4` | PASS empty, then E1 at x=2 with cached twice=4 | PASS empty, then 2 and 4 while DOM is still initial | PASS final projection and coherent root storage at every sampled Step write | — |
| V07 load, Step, Step | PASS `1\|0 → 1\|1 → 1\|2` | PASS E1 at x=1 once per same-value assignment | PASS empty throughout | PASS final projection at each sampled Step write | — |
| Same-watch source-order control | PASS `0\|0 → 1\|2` | PASS E1 then E2 at x=1, independently identified by declaration line | PASS empty | PASS final projection at each sampled Step write | — |

## Observation mechanisms and limits

- Root values come from `jtml.state`. Forwarding property accessors log scalar
  storage writes, including same-value writes. Getter storage that is missing
  during bootstrap is observed only after the initial snapshot. Storage is the
  working cache. Logical commit count and commit-observer delivery are
  **UNVERIFIED**.
- The root action-table body getter returns the original statement list and
  records when the subscription dispatcher enters it. Effect identity is
  resolved against the source declaration line. The watched value is sampled at
  entry. A separate notification-delivery trace is **UNVERIFIED** for subscribed
  vectors. V16's manifest subscription inventory is empty.
- A forwarding console hook captures each `[SHOW]` string in sequence. The
  scalar output channel carries rendered text. State checks independently
  verify numbers and booleans as typed values.
- Forwarding native `textContent` and `innerHTML` setters record the actual
  synchronous writes used by these paths, together with root storage and the
  observer projection. V16 load/remount temporarily inserts the lowered
  interpolation placeholder inside the same synchronous episode. R8 allows
  multiple physical patches. Paint, reentrant JTML observer delivery and
  passive-observer atomicity are **UNVERIFIED**.
- Forwarding generated component functions record create/update/destroy and
  expose the existing scope passed to create/update. The runtime's active
  instance enumeration and destruction ledger check cleanup. A weak map of
  instance objects distinguishes the fresh mount even though `Card_1` is reused.
  Any attempted generated update for the dead instance during Bump would be
  recorded before invoking the original function. Arbitrary dead-subscription
  delivery outside the observed entry points is **UNVERIFIED**. Existing
  root-conditional and keyed lifecycle suites cover sibling isolation, detached
  controls and retained keyed DOM identity separately.

## Baseline counterexample and correction

The V20 callback reads a getter after the action writes. The additional
`effect-freshness` control isolates a different R6/R7 obligation: the callback
itself writes `y=2` and then executes `show twice; seen=twice`, where
`twice=y*2`. Merged main shows 0 and commits `y=2, twice=4, seen=0` with DOM
`2|4|0`. The authored control requires show 4 and DOM `2|4|4`. The first
three read observations in the original V20 still give show 2, show 4, and
effect-entry twice=4.

The clean-browser dispatcher now passes the existing action read scope into
the root effect's existing statement interpreter. The compiler already
certifies derived reads for these callback bodies as root actions. Subscription
scheduling and publication stay on their existing paths. The same control is
also run in explicit compatibility and debug builds, where it retains show 0
and DOM `2|4|0`.

The full gate writes the per-phase comparisons and raw trace to
`dist/reactive-core-observations.json`. To reproduce the strict-CSP suite, run
`node tests/browser/reactive_core_observation.cjs` with the existing
`JTML_BENCH_BROWSER` and `JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT` settings.
`JTML_BIN` selects the comparison compiler. A mismatch exits nonzero after
printing the trace. The historical reference-model comparison remains pinned
to its own browser evidence and covers a different four-vector subset.
