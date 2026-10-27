# Four-trial preparation report — 2026-10-27

**NO-GO for real-agent execution.** Reference calibration, Docker isolation and
the deterministic end-to-end pipeline pass. No paid API requests or real agent
trials were executed. Model/account/pricing selection, genuine provider usage and
billing verification, and explicit operator authorization remain outstanding.
There is no measured JTML agent-performance advantage or statistical inference.

## Preserved baseline and corrected references

PR #51 includes main `afa7ee9754add76df6fa0f80e4a0402d327ddd67` via a merge.
[Post-merge main CI](https://github.com/mrjohnnyrocha/jtml/actions/runs/40499324034)
is successful on Ubuntu and macOS. The compiler source diff against that commit
is empty. No compiler semantics, release or website deployment changed here.

All original RC.2 evidence files, including the approximately 7 MB archive,
failed JTML references and historical report, remain byte-identical to commit
`7a2cb04c270adc1ec8fe04e8b9f544b84991d706`. The specifications, starters, reference
source and browser acceptance criteria are also unchanged. The corrected compiler
is a separately recorded cohort, not a replacement for RC.2.

Both corrected cohorts use compiler source SHA
`afa7ee9754add76df6fa0f80e4a0402d327ddd67` and identify as `jtml 0.3.0-dev`:

| Cohort | Compiler binary SHA-256 |
| --- | --- |
| Local macOS arm64 | `aed6dd26d4c83c3a52554d10719c6d41cfab4332d3ab311b3d86c00c38d70112` |
| Docker Linux arm64, Release | `f1ec136a180b3fcec1227fbe025d81c33139d1a7b4dfea27b3205d4bd119f276` |

Worker image ID:
`sha256:bff47eabffba34021986fa29be3318b9a48307db8354d785077ecd9bfab4a84d`.
React/React DOM 19.2.0, Vite 7.2.2 and Playwright 1.63.0 are locked. Chromium is
153.0.8010.12. The independent oracle SHA-256 remains
`5faabdc8a064e3c64ab8da01d9067208e3dd89613f5f4794babeec8470adff72`.

| Case, on both macOS and worker image | JTML | React |
| --- | --- | --- |
| Task 01 reference | PASS | PASS |
| Task 02 reference | PASS | PASS |
| Broken starter: confirmation-reset negative | Detected functional failure | Detected functional failure |

Raw local results, evaluator-source snapshots, builds, screenshots/traces and CI
receipt are in [post-merge evidence](evidence/post-merge-afa7ee9/). The separate
worker calibration is in [worker-calibration.json](evidence/infrastructure-smoke/worker-calibration.json).
These are authored reference applications, never agent trials.

## Runner review and deterministic end-to-end evidence

The unfinished implementation was preserved before simplification. The
[focused audit](agent/AUDIT.md) retains the worker/controller/evaluator boundaries,
one freeze manifest and budget ledger. Mandatory five-project/key provisioning
and the separate paid API smoke command were removed. Only one provider, two
public tasks, four fixed trials and a deterministic test provider are supported.
There is no scheduler, registry or generalized benchmark service.

The actual Docker smoke exercised:

`frozen packet → worker → scripted shell tool call → submitted source → worker
teardown → isolated build → independent Chromium acceptance → evidence/report`.

| Deterministic submission | Actual outcome | Recorded correctly |
| --- | --- | --- |
| Correct Task 01 app | Browser acceptance PASS | Yes |
| Broken confirmation reset | Browser functional failure; validation still passes | Yes |
| Intentionally unavailable image | Infrastructure blocked; no submissions attempted | Yes |

The final success/failure smoke report is
[success-and-failure.json](evidence/infrastructure-smoke/success-and-failure.json).
It records `agentTrials: 0`, `paidApiCalls: 0`, and `provider: deterministic-stub`.
Synthetic ledger usage is explicitly labelled and excluded from provider totals.
Actual cost remains null. The deliberately blocked run is retained separately,
not represented as an application failure or an end-to-end pass.

Isolation checks passed for compiler hash, read-only root, absence of evaluator,
host repository and Docker socket, absence of credentials, disabled external
network, and a working local browser. Container inspection confirms only the
candidate mount. Workers have 2 CPU, 2 GiB memory and 128 PID limits. Build and
browser evaluation run after the agent worker is destroyed; no acceptance feedback
is returned to a session. The archive preserves the initial successful smoke,
final smoke, blocked-image control and complete worker calibration, including
packets, source, journals, builds, browser traces and result JSON.

## Verification and remaining blockers

Nineteen automated tests pass, with zero skips. They cover budget reservation,
missing usage, scoped packets, seal mutation, no paid execution without an exact
operator-authorized seal, stub exclusion from real scoring, billing reconciliation,
output symlink rejection and controller reporting. CI runs these without model calls.
The full local repository gate also passed: 477 CTests, nine clean-exit browser
benchmarks, package relocation checks and live smoke 8/8, no verification skips.
Known five native renderer discrepancies and the 99,953-byte reference HTML
against the unchanged 70,000-byte microfixture budget remain disclosed.

[Current readiness](evidence/infrastructure-smoke/readiness.json) confirms the
worker, reference calibration, end-to-end smoke and main CI. Exact blockers are:

1. No accessible immutable model snapshot or reasoning effort has been selected.
2. `JTML_EVAL_API_KEY` is not provided to the controller.
3. Reviewed rate evidence and conservative bounds, including token-count endpoint
   charges, are unconfigured. Bounds are not actual billed cost.
4. Genuine provider token-count/usage round-trip and settled actual-cost attribution
   have not been verified. Unit/stub usage cannot establish these.
5. The operator has not authorized any paid run for a reviewed frozen seal.

The real path refuses missing readiness, model identity mismatch, unknown usage,
budget exhaustion and absent authorization. It preserves raw responses and stops
on infrastructure/telemetry failure. Reported provider tokens must be genuine;
actual USD requires a reviewed, hashed provider export. A pending bill is unknown,
never zero. The real-provider integration path remains unverified until authorized
provider evidence exists; Docker/stub success does not make it operational.

[Operator commands and configuration](agent/README.md) cover image pinning,
reproduction, readiness, freezing, authorization, the four independent sessions,
receipt reconciliation and review. First run only Task 01 JTML/React and Task 02
React/JTML, one fresh session each, with shared 900-second/30,000-token/USD-5 caps.
Do not run the paid command until the blockers are resolved and authorization is
explicit. Do not count this development session, references or stubs as trials.

**NO-GO for five paired replicates as well.** Finish and review the first four
real observations, their failures and complete telemetry before any expansion.
PR #51 remains in draft. This bounded slice stops here.
