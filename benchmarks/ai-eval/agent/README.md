# Operator procedure: bounded four-trial pilot

Run commands from `benchmarks/ai-eval`. Never put credentials in configuration,
packets, images or evidence files. Use absolute output paths below. All commands
require new output directories and preserve unsuccessful attempts.

## 1. Build and pin the worker (no model calls)

```sh
npm ci --ignore-scripts
npm test
docker build --build-arg JTML_SOURCE_SHA=afa7ee9754add76df6fa0f80e4a0402d327ddd67 \
  -t jtml-ai-pilot:afa7ee9 -f agent/Worker.Dockerfile .
docker image inspect jtml-ai-pilot:afa7ee9 --format '{{.Id}}'
docker run --rm --network none --read-only jtml-ai-pilot:afa7ee9 \
  sha256sum /opt/jtml/bin/jtml
cp agent/cohort.example.json /tmp/jtml-cohort.json
```

Set `workerImage` to the immutable `sha256:...` image ID, and
`workerCompilerSha256` to the printed binary hash. Record image architecture and
build log. This locally builds the already merged compiler; it publishes nothing.
A local macOS reference calibration does not substitute for Linux image calibration.

## 2. Free deterministic end-to-end smoke and worker calibration

These commands need only worker/toolchain/resource fields in the config, not
model keys, pricing or billing readiness:

```sh
node agent/smoke.mjs /tmp/jtml-cohort.json /tmp/jtml-stub-smoke-001
node agent/run.mjs calibrate /tmp/jtml-cohort.json /tmp/jtml-worker-calibration-001
```

The smoke freezes two packets, proves isolation, and feeds deterministic shell
tool calls through `runTrial`. It submits a correct and a deliberately broken
Task 01 application, destroys each worker, builds submissions without evaluator
access, then runs the unchanged browser tests. Both application outcomes must
match expectations; merely detecting a build/infrastructure error cannot satisfy
the negative control. Inspect `result.json`, each `trial.json`, `events.jsonl`,
source snapshots, build logs, screenshots and traces. `agentTrials` is zero and
`paidApiCalls` is zero. An unavailable Docker daemon or failed isolation check
produces a blocked report and nonzero exit; it is not an end-to-end pass.

The separate worker calibration must pass both reference tasks for both stacks
and detect both seeded defects. Set `smokeEvidence` and `calibrationEvidence` to
the respective `result.json` paths. Set `mainCiEvidence` to the saved successful
GitHub main CI receipt for the exact compiler source commit. Paths in configuration
are resolved relative to the config file.

## 3. Establish real provider readiness; no automatic paid smoke

Before requesting authorization, pin an accessible immutable dated model ID and
`reasoningEffort`. Both stacks must use exactly these settings. The current
adapter is OpenAI Responses with a single shell function. It verifies the returned
model ID, counts the complete input through `/v1/responses/input_tokens`, reserves
input plus capped output, and compares returned usage to the reservation. Unknown
usage or a failed request stops the cohort without a blind retry. Raw requests,
responses and request IDs are preserved, excluding authorization headers.

Provide reviewed pricing evidence and conservative input/output USD per million
tokens plus any token-count endpoint charge. Cached discounts never increase the
spending bound. Count endpoint requests, failed/unfinished requests and generated
reasoning must be included in actual billing attribution. Obtain provider receipts
with request-level costs, or use exclusive project/time windows with settled
before/after exports. Never label rate multiplication as actual billed cost.
One key/account may serve the four sequential trials when attribution is sound;
independent model sessions do not require five billing projects.

`providerEvidence` must identify `kind: "provider-readiness"`, exact `model`,
`reasoningEffort`, a `reviewer`, `rawEvidence` (path relative to the receipt),
`rawEvidenceSha256`, and verified booleans:
`tokenCountRoundTripVerified`, `usageReportingVerified`, `billingAttributionVerified`.
The readiness check verifies the raw file hash. Preserve that original file with
the receipt. It must be backed by genuine provider evidence, not this stub or self-attestation
without data. If an authorized live integration check is needed, obtain explicit
operator authorization first and record its real usage/billing separately. No
such authorization or live provider evidence is assumed by this repository.

`billingReadiness.evidence` is a JSON receipt with `kind: "billing-readiness"`,
`supportsActualUSD: true`, `exclusiveProjects: true` and the unique project IDs
listed in the four planned trials. Here exclusivity means no unrelated work in
the export windows. Set `billingReadiness.reviewer`; save the raw supporting
provider export. Set each trial's `billingProject`. Credentials are read only
from the named environment variable, default `JTML_EVAL_API_KEY`.

```sh
node agent/run.mjs check /tmp/jtml-cohort.json
node agent/run.mjs freeze /tmp/jtml-cohort.json /tmp/jtml-frozen-001
node -e "const fs=require('fs'),c=require('crypto'); console.log(c.createHash('sha256').update(fs.readFileSync('/tmp/jtml-frozen-001/seal.json')).digest('hex'))"
```

A successful check is required, but does not authorize payment. Review frozen
packets, controller and receipts. The controller is experimental until its live
provider path has been verified with actual telemetry and billing.

## 4. Only after explicit authorization

The operator must intentionally provide the reviewed seal hash:

```sh
node agent/run.mjs pilot /tmp/jtml-frozen-001 /tmp/jtml-real-pilot-001 \
  --authorize-paid APPROVED_SEAL_SHA256
```

No resume, conversation fork or cross-trial state is used. Task 02 starts from the
reviewed broken starter, not Task 01 output. Order is JTML/React for Task 01 and
React/JTML for Task 02. One session may repair its own intermediate shell/build
failures within budget. Independent acceptance is never fed back to it. Worker
startup/cleanup are included in recorded wall latency; provisioning/image build
and independent evaluation latency are outside the agent budget. A timeout or
budget stop still preserves and evaluates the submitted files. Infrastructure or
unknown telemetry aborts the cohort and records the unstarted trials.

## 5. Reconcile, review, report

A provider billing receipt must contain `kind: "provider-billing-receipt"`,
`project`, `currency: "USD"`, `settled: true`, `exclusiveProjectUse: true`,
`actualCostUSD`, every trial `responseIds`, `source`, `reviewer`, `rawEvidence`
(path relative to receipt) and `rawEvidenceSha256`. Preserve raw provider data.

```sh
node agent/run.mjs reconcile /tmp/jtml-real-pilot-001/01-jtml-r1/trial.json /tmp/receipt.json
```

Repeat for all four trials. This creates a separate `.billed.json` and archives
the billing export without overwriting the original trial. Add an independent
`integrity-review.json` beside each trial with `reviewer`, `assignedStack: true`,
`noEvaluatorAccess: true`, and `trialSha256` matching the original `trial.json`.
Inspect source and raw records before signing it; browser success alone does not
prove assigned-stack use. Preserve compiler/unsupported capability disputes as
unresolved with a reproducer, never automatically as an agent failure.

```sh
node agent/summary.mjs /tmp/jtml-frozen-001 /tmp/jtml-real-pilot-001
```

The report withholds a success rate when required records are absent or
incomparable. It keeps application failures, infrastructure errors, timeouts,
token totals, actual USD and wall latency visible. Do not expand to five paired
replicates until the four-trial methodology and failures have been reviewed.

API contract reference: [official input-token counting documentation](https://developers.openai.com/api/docs/guides/token-counting).

## Review fixes: lifecycle, bounded input and evidence verification

Worker PID 1 is a root-owned, finite `sleep` with all capabilities dropped;
Docker `--rm` removes the container on exit. Tool commands explicitly run as
uid/gid 1000 and cannot stop the supervisor. The independent lifetime is at most
900 seconds even if the controller is killed. The controller journals the name,
launch arguments, lifetime, returned container ID and removal/absence checks.
A failed removal is acceptable only when a successful daemon query proves the
container is already absent. Otherwise the cohort stops without inspecting or
evaluating the candidate. The build and evaluator also have container-side timeouts.

After confirmed teardown, the controller removes only its known dependency
symlink. Tree inspection accepts regular files and directories only, including
inside excluded output directories. It rejects symlinks, FIFOs, sockets and
devices before reading; limits are 4,096 entries, depth 32, 64 MiB per file and
256 MiB total per inspected tree. Invalid submissions retain an infrastructure
error, candidate and journal; they are never silently counted as application failures.

Unknown usage takes precedence over wall expiry, which is a separate field.
The full reservation remains charged conservatively, partial response bytes and
available request IDs are retained, and no potentially billed request is retried.
Counting and generation share model, reasoning, parallel-tool policy, instructions,
conversation and tools. This is deterministic contract testing, not proof of
live provider compatibility or actual billing.

A new smoke receipt includes the frozen seal, hashes captured for the running
controller, and a manifest of all raw run files. Keep `result.json` beside the
complete raw evidence tree. `freeze` copies and verifies that bundle. Verification
rejects edited source, raw evidence, receipts or different controller versions:

```sh
node --input-type=module -e 'import {verifySmokeEvidence} from "./agent/core.mjs"; verifySmokeEvidence(process.argv[1]); console.log("verified")' /absolute/smoke/result.json
node agent/lifecycle-smoke.mjs /absolute/cohort.json /absolute/new-lifecycle-results
```

The lifecycle command covers normal completion, command timeout, controller
SIGKILL followed by independent expiry/removal, and an injected removal failure
against a real live container. It makes zero provider requests.
See [historical attribution](../evidence/HISTORICAL_SMOKE_PROVENANCE.md); old
artifacts remain unchanged and cannot qualify as current smoke evidence.
