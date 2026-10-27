# JTML vs React: two-task feasibility pilot

The product hypothesis is **a compiler-first language for reliable AI-generated
web applications**. It remains unproven. This benchmark currently has zero real
agent trials. References, deterministic stubs and the Codex development session
are never agent performance or adoption evidence.

Two public tasks run: build a reservation form; then modify/repair a supplied
form. Both stacks use the same specifications and browser oracle. The 20-entry
catalog is design-only beyond those two tasks. Public tasks are calibration and
feasibility material, not held-out evaluation.

Main `afa7ee9754add76df6fa0f80e4a0402d327ddd67` fixes input synchronization.
Corrected JTML and React references pass both contracts; both seeded negative
controls still fail. [Current report](POST_MERGE_REPORT.md) records source/binary
hashes and verification. [Historical RC.2 report](REPORT.md) and all original
RC.2 failures remain unchanged.

## Local reference calibration

Node 22.12+ and a browser are required. From this directory:

```sh
npm ci --ignore-scripts
npx playwright install chromium
npm test
JTML_BIN=/absolute/path/to/jtml \
JTML_COMPILER_SHA=afa7ee9754add76df6fa0f80e4a0402d327ddd67 \
node pilot.mjs runs/new-calibration
```

Every output directory must be new. The pilot expects four passing references
and two detected defects. Published RC.2 still exits 1 because its references
fail; corrected main is a separate compiler cohort.

## Isolated pipeline and planned real trials

The evaluated agent will be one exact model snapshot plus the frozen shell-tool
controller, not this Codex chat. Each of the four task/stack trials gets a fresh
session and candidate directory, its task, and one versioned stack guide of
comparable scope. The agent cannot access evaluator files or reference sources.
The host controls model requests; the worker has no credentials, external network,
Docker socket or repository mount. Submission building and browser evaluation
happen after the agent stops, in separate containers.

Limits per trial: 900 seconds, 30,000 cumulative input+output tokens, USD 5;
the first pilot uses one session (within the original three-attempt maximum).
Input includes cached input; output includes reasoning. Shell output is capped
at 1 MiB with a truncation flag. Missing provider usage stops requests. Price-based
reservations are conservative bounds; `actualCostUSD` stays null until a reviewed
provider receipt is reconciled. Missing billing/isolation/integrity evidence
prevents a comparison. Failures and timeouts remain in the evidence ledger.

Run [the deterministic smoke and operator procedure](agent/README.md) before any
paid run. It must record one successful and one failed submission through the
actual isolated controller/build/browser path. Stub tokens are synthetic and
cannot be scored. [Runner review](agent/AUDIT.md) explains the retained boundaries
and removed complexity.

**Real execution requires explicit operator authorization for the frozen seal.**
No paid API calls are made by installation, unit tests, calibration, readiness
checks or deterministic smoke. Four real observations establish feasibility only;
they cannot establish statistical significance, a language-wide advantage or
external adoption. Stop for methodology review before five paired replicates.
