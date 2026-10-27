# Calibration report — 2026-10-27

**No agent comparison has been measured. No AI-native advantage or external
adoption is established.** This is a two-task harness pilot with authored
reference applications, not a model study. There are zero agent trials; attempts,
tokens, agent latency, cost and agent regression rates are not available.

## Observed results

Published JTML `0.3.0-rc.2` macOS arm64 binary SHA-256:
`b96cadca3a0ff010ddcebf931eba60822a89ee40da7537be2024ac59417f17b9`.
React 19.2.0 / React DOM 19.2.0 / Vite 7.2.2, Node v22.23.3,
Playwright 1.63.0, Chromium 153.0.8010.12, macOS arm64.

| Public calibration case | JTML RC.2 reference | React reference |
| --- | --- | --- |
| 01: build reservation form | FAIL: displayed input does not clear on Reset | PASS |
| 02: modify party size + repair Reset | FAIL: displayed input does not clear on Reset | PASS |
| Seeded stale-confirmation defect | Detected by confirmation-reset check | Detected by confirmation-reset check |

JTML passes empty-name validation, literal text handling, focus after entry,
Enter submission without navigation, reservation snapshot semantics and
confirmation reset. In task 02, the extension scenario reaches and passes size
bounds, totals, confirmation snapshots and size reset, then fails the final
empty-input assertion. These are observations of references, **not** agent success
rates. React wins functional reference acceptance on these two cases. A future
agent may find a valid implementation; this pilot does not establish impossibility.

The standalone `evaluator/probe-reset.mjs` removes reservation logic: type a draft,
click Clear, inspect the visible echo and input. RC.2 updates the echo to
`Draft: []` while the textbox retains `Before reset`. Its nonzero exit is retained.
This is evidence of a potential toolchain defect in an expected supported shape;
classification remains **unresolved pending independent review**, not unsupported
and not agent failure. No compiler/runtime changes or alternate acceptance
criteria were introduced to make this pass.

The pilot command intentionally exits **1** because JTML references fail. The
negative controls are required to fail; they are not included as successful apps.
The oracle separately checks confirmation reset so the seeded stale-confirmation
bug is detected even when an input-reset defect also exists. Do not turn the
observed JTML failure into an expected success in CI.

## Unsuccessful calibration attempts

All pilot directories are retained in the evidence archive:

- `pilot-1`: builds completed, but sandbox prevented browser launch; infrastructure
  errors for all six cases. No functional results inferred.
- `pilot-2`: React references passed; JTML reference inputs lacked an accessible
  name using `field label`. This is reference authoring/calibration evidence.
- `pilot-3`: native `label` alone still did not satisfy the accessible-name oracle.
- `pilot-4`: explicit `aria-label` satisfied the unchanged name contract; input-reset
  failures remained. No compiler changes.
- `pilot-final`: adds a distinct confirmation-reset scenario to distinguish the
  intended seeded defect from the input-reset issue. All prior behavioral
  assertions remain. Final results are in the table above.
- `reset-probe`: independent minimal state-versus-input reset reproduction.
- `pilot-verified`: repeats the final oracle with automatic evaluator-source
  snapshots in every attempt; this is the archived final matrix.

Earlier failed snapshots preserve submitted source, generated assets, logs,
screenshots/traces where the browser launched, and result hashes. Final evaluator
source is bundled for reproduction. Earlier harness source revisions were not
separately archived; their hashes and outputs remain, so the earlier records are
forensic history rather than fully versioned evaluator releases. The harness now automatically copies its evaluator source, task, protocol and
lockfile into every new attempt. Future cohorts must also freeze these before
the first attempt.

## Limits and review decision

Seven Node harness tests pass, including export separation, symlink rejection,
exclusive evidence creation, loss/unknown billing reporting, pair checks and
failed-attempt import with generated outputs. Browser functional acceptance is
**not fully green**, as reported above. The full repository gate passed: 476 CTests, nine clean-exit browser benchmarks,
package relocation checks and 8/8 live smoke checks, with no verification skips.
Its five known native attribute-renderer discrepancies and reference HTML size
overage remain disclosed; gate success is not production readiness or benchmark
acceptance. See `evidence/verification.json` and compressed full logs, including
failed initial dependency provisioning. Hosted CI is separate and was not measured
for this report.

This implementation contains two public tasks, one browser, one host and authored
references. The other 18 catalog entries have acceptance intent only, no executable
oracle yet. There is no provider adapter or automatic agent sandbox/budget
enforcement, and model/pricing remain deliberately unselected. Imports trust the
operator's transcript and classification; a reviewer must audit telemetry, stack
use, pairing, budget compliance, capability classification and integrity before
making any comparative claim. Raw success and budget-qualified success are
reported separately; missing costs/tokens cannot qualify a run within budget.

Review this architecture and investigate the reset observation independently
before expanding. Keep all 20 public catalog concepts out of any future claimed
held-out set. Use newly authored private variants with precommitted tests and no
cross-stack solution leakage. A measured report must retain JTML losses, aborted
runs and unsupported-capability counts rather than selecting only successful tasks.
