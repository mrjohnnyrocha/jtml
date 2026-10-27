# JTML AI coding evaluation — public calibration v1

This is benchmark infrastructure, **not evidence of an AI-native advantage or
external adoption**. There are zero measured agent trials. Do not count the
reference applications, seeded defects, or this implementation session as trials.

## Smallest credible architecture

A versioned task specification → isolated agent workspace → frozen submission →
independent browser oracle → immutable evidence → adjudicated trial record.
React 19.2.0 + Vite 7.2.2 (plain JSX) is the mainstream comparator. Friendly JTML 2
uses the published 0.3.0-rc.2 clean browser build. Pin the exact binary hash; newer
compiler results are a different cohort. Dependencies are locked in
`package-lock.json`. Node 22.12+ and Playwright 1.63.0 Chromium are required.

`catalog.json` defines 20 task concepts across forms, reactivity, components,
lists, routing, modification and repair. Only 01 and 02 have executable
specifications and acceptance tests. This deliberate review boundary avoids
scaling an unvalidated protocol. All 20 concepts are public; future held-out work must use newly authored private
variants. The remaining 18 need complete specifications,
matched starters, capability probes, independent oracles and negative controls.
They are **not** claimed to be implemented or held out.

The two pilot tasks are public calibration, permanently ineligible as held-out
results. `tasks/` is agent-facing; `evaluator/` contains reference solutions and
oracles. `prepare` exports only one starter and its specification/stack note.
Directory names do not provide isolation: run agents in a separate container/VM
with only that export mounted, no repository, evaluator, other-stack solutions,
shared conversation, network, parent-directory access or evaluator results.
Future held-out task details/tests belong in a separately access-controlled
repository and must never enter agent-facing guidance or public training examples.
Freeze their hashes before runs and publish them after the cohort concludes.

## Reproduce the vertical slice

From this directory:

```sh
npm ci --ignore-scripts
npx playwright install chromium
export JTML_BIN=/absolute/path/to/rc2/bin/jtml
npm test
node pilot.mjs runs/pilot-001
```

Optional `PLAYWRIGHT_BROWSERS_PATH` selects a preprovisioned browser cache;
`BENCH_BROWSER` selects a browser executable. Record browser version and use the
same one for both stacks. Missing tools/browser/ports are failures, never skips.
Each run directory must be new. The pilot builds and tests both reference apps
for both tasks, then requires the equivalent reset defects to fail. RC.2 currently fails the
input-reset contract in both references, so the recorded pilot exits 1; see
REPORT.md. This result is deliberately not masked. Exit 0 means
harness calibration succeeded; it does not mean an agent succeeded. Assertions
use accessible controls and visible text, never runtime internals or source shape.
Traces and screenshots are captured for each scenario; source, outputs, compiler
hash, build logs, oracle hashes, checks and environment accompany each result.

```sh
node harness.mjs prepare 02 jtml /tmp/reservation-jtml
node harness.mjs prepare 02 react /tmp/reservation-react
# Provision identical isolated agent environments, run externally, freeze source.
node harness.mjs evaluate 02 jtml /tmp/submission-jtml runs/submission-001
node records.mjs import /tmp/trial.json runs/trial-001
node records.mjs summary runs/trial-001/trial.json
```

The evaluator executes candidate builds. Run it in a disposable sandbox with no
credentials and bounded CPU/memory, even though it does not execute candidate npm
scripts. It is **not a security sandbox or an agent launcher**. Resource isolation,
agent token/cost/time enforcement and raw provider telemetry are the operator's
responsibility in this first slice. Never run untrusted submissions on a credentialed
workstation. The isolated agent receives preinstalled dependencies/toolchain and
an identical shell/local-browser permission set; installation time is excluded
from agent latency but separately recorded as provisioning cost.

## Freeze a fair cohort before measuring

Fill the null model, agentVersion, reasoningEffort and pricingSnapshot in
`protocol.json`; commit and hash it before any trial. Archive the evaluator source and lockfile at that hash, not
only their digests. Use one exact model snapshot,
agent build/system prompt, reasoning setting, temperature/seed where supported,
and permission profile for both stacks. Freeze a versioned, public-doc-only
stack guide for each, recording hashes and size; neither includes task solutions
or held-out hints. No JTML-specific coaching during trials. Report the inevitable
pretraining familiarity difference; do not suggest identical documentation makes
prior knowledge equal. Mainstream choice and pinned versions are controls, not a
claim to represent every frontend stack.

Default proposal: 900 seconds, 30,000 total input+output tokens (cached input also
counts), USD 5 and at most three independently logged agent sessions per task and
stack. Stop at the first exhausted limit, including unsuccessful sessions. Charge
all sessions, tool/model calls and repair work. An attempt is a fresh/resumed agent
session ending in a frozen submission, timeout, cancellation or crash, not a single
edit/build. Include empty submissions after timeouts. Aggregate budgets span all
attempts. Acceptance is run only after submission; it supplies no repair feedback.
Agents may build, browse their local app and write their own tests within budget.

Use five paired replicates per task, alternate stack ordering, fresh independent
sessions, and record run order and seed. Task 02 always starts from the reviewed
seeded baseline, not a variable-quality output of task 01. Retained baseline
checks identify compatibility failures; compare them with the supplied baseline
(the known reset failure is a repair target, not a newly introduced regression).
Human reviewers inspect whether app logic uses the assigned stack and whether a
reusable editor component exists; browser behavior alone cannot prove that.
An integrity violation invalidates the trial and remains in the evidence ledger.

## Measurements and adjudication

`records.mjs` imports externally generated records, copies every attempt and raw
transcript, and reports descriptive totals. Missing tokens, price or latency are
`null`, never free or zero. `evaluatorLatencyMs` measures building/testing only;
it is **not** agent latency. Provider telemetry must include input, cached input,
output, billed USD and wall-clock time (`inputTokens` includes the cached subset,
so cached tokens are not added twice); preserve invoices/rate snapshots alongside
transcripts, including tool calls and failures. No fabricated token estimates.
See `trial.example.json` for the import shape. IDs must identify task, stack,
replicate and cohort. Audit matching pair IDs against exact protocol/model/agent
and guidance hashes before comparing; summaries alone do not prove comparability.

Classify failures separately: agent failure, independently reproduced unsupported
capability, toolchain defect in a claimed-supported shape, infrastructure error,
or unresolved. An agent's assertion or a compiler error is never sufficient to
label a task unsupported. Preserve a minimal reproducer, version, reviewer and
reason. Do not repair the compiler during a frozen cohort. Report all-task success
(including unsupported tasks) and supported-task success side by side. Do not drop
losses or retroactively change tests. Toolchain defects count as supported failures;
infrastructure/unresolved counts remain visible with a sensitivity analysis.

Report per-task first-attempt and budget-bounded success, attempts, total tokens,
USD, median/p95 wall latency (failures/timeouts included), and **new** baseline
regressions. Use paired task/replicate differences and uncertainty intervals; 2
calibration tasks cannot establish a language-wide advantage. Publish all attempts
and exclusion reasons, JTML losses, and raw anonymized evidence. Audit secrets in
transcripts before publication and preserve private originals plus redaction logs.
Record genuine external users separately from evaluation agents. No synthetic
activity is adoption evidence.

## Expansion gate

Review the two task contracts, negative controls, evidence and fairness protocol
first. Then implement the other 18 with independent tests and capability probes,
add a provider-specific agent adapter enforcing budgets and isolation, and freeze
a genuinely held-out cohort. Do not run a paid study until model/pricing/permissions
are fixed. See `REPORT.md` for the actual measured scope and limitations.
