# PR #51 independent review corrections

Scope: the five findings against `4f0e2eeb8df86b9e389a51ca6b6a89b6dc1e4cf0`.
Implementation commit: `283006d411a7fe5e6848e8fb3d1bd8d243adf842`.
No compiler, task, reference source or acceptance criteria changed. No paid API
requests or real agent trials ran. These changes establish experimental
infrastructure safety, not live provider or billing compatibility.

| Finding | Root cause and correction | Regression evidence |
| --- | --- | --- |
| P1: smoke attribution | The outer receipt described source different from the archived run. Preserve all ten historical files unchanged and label the actual hashes. Capture controller hashes at module load, compare current and frozen source, bind all raw files to the receipt, and verify the complete bundle when checking/freezing a campaign. | Provenance mutation/rejection tests; final Docker smoke has 95 hashed raw files and 18 source hashes matching the implementation commit and archived source. |
| P1: worker lifecycle | Detached `sleep infinity` depended on the controller and unchecked removal. Agent/build/evaluator containers now have root-owned finite PID 1 lifetimes, `--rm`, uid 1000 tool execution, prelaunch identity journals and verified daemon absence before reading results. Unconfirmed teardown stops the cohort and prevents candidate inspection. | Real Docker normal completion, timeout/expiry, controller SIGKILL/expiry, and injected removal failure; unit integration proves no evaluation directory is created after failed teardown. |
| P1: special files | Nondirectory files were copied/read without validating type or bounds. Inspect the whole tree first; accept regular files/directories only; use nonblocking/no-follow opens plus descriptor checks; cap entry count, depth, file size and total bytes. | Actual FIFO, Unix socket and `/dev/null` rejection; entry/file/total limit tests; FIFO submission produces preserved infrastructure-error evidence without snapshot reads. |
| P2: deadline and unknown usage | Wall expiry won over unknown usage. Unknown usage now wins, wall expiry is independent, unresolved exposure independently stops the cohort, and interrupted requests retain conservative reservations and available raw response bytes/IDs/errors. | Exact-zero boundary accounting and an interrupted deterministic generation through `runTrial`; no additional generation calls; original interruption preserved. |
| P2: request parity | Counting omitted reasoning and parallel-tool settings. One shared configuration supplies both requests; generation adds only storage/encrypted-output options and reserved output cap. | Compare actual recorded count/generation requests; all output tokens, including reasoning, remain charged. |

## Evidence and reproducibility

The final artifacts are in [evidence/review-fixes](evidence/review-fixes/).
`verification.json` records source identity, Docker image, test results and gate
coverage. `pipeline-evidence.tar.gz` contains the unmodified final smoke receipt,
frozen controller/packets, lifecycle journals, requests/responses, submissions,
build/evaluator records, screenshots and browser traces. Calibration and lifecycle
regressions are separate archives. `regression-source.tar.gz` preserves test code.
Earlier development iterations are explicitly separate and are not final-head proof.

The old archive and receipt remain untouched; see the
[historical attribution correction](evidence/HISTORICAL_SMOKE_PROVENANCE.md).

From `benchmarks/ai-eval`, verify the archive and its extracted raw evidence:

```sh
(cd evidence/review-fixes && shasum -a 256 -c SHA256SUMS)
mkdir /tmp/jtml-review-evidence-new
tar -xzf evidence/review-fixes/pipeline-evidence.tar.gz -C /tmp/jtml-review-evidence-new
node --input-type=module -e 'import {verifySmokeEvidence} from "./agent/core.mjs"; verifySmokeEvidence(process.argv[1]); console.log("verified")' /tmp/jtml-review-evidence-new/smoke-verified/result.json
```

The worker was rebuilt from unchanged compiler source
`afa7ee9754add76df6fa0f80e4a0402d327ddd67`; compiler binary SHA-256 remains
`f1ec136a180b3fcec1227fbe025d81c33139d1a7b4dfea27b3205d4bd119f276`.
Both stacks use rebuilt image
`sha256:1f27eed4cc65bf2518446cfc02deff27e8c79a4af9aba584039b5aa79ecc7d8a`.
All four references pass and both seeded negative controls fail as expected.
The deterministic smoke accepts its good submission and rejects its broken one.
All 25 unit tests pass with zero skips; all four Docker lifecycle cases pass.

## Verification limitations and failed attempts

Initial sandboxed unit runs could not bind the real Unix socket; approved runs
pass without skips. Those failed logs remain archived. The first repository gate
used local Chrome 156 virtual time and failed a nonpositive interaction-time
assertion. A second run used Playwright with that Chrome and passed all nine
benchmarks, then failed strict-CSP on a resource 404. Neither test nor budget was
changed. The final gate uses Playwright 1.63.0 and its pinned Chromium headless
shell 153.0.8010.12, matching the previous passing repository setup. That prior pinned-browser log stopped during packaging and has no completion
marker; it is preserved as `full-gate-pinned-incomplete.log.gz`, not a pass.
The finalization rerun completed successfully; see below. Existing native-renderer
limitations and the reference HTML-size overage are not resolved by this PR.

Paid execution remains NO-GO until genuine provider token-count/usage and billing
attribution evidence is available and the operator explicitly authorizes the
frozen seal. Unit stubs, reference runs and this development session cannot qualify
as real agent measurements. Four eventual observations remain feasibility only.


## Finalization audit — 2026-10-27

The completed local gate passed 477/477 CTests, all nine performance browser
benchmarks with clean exits, the production reference and integration suites,
package relocation/site/editor/LSP checks, and live smoke 8/8. No verification
checks were skipped. The fresh runner unit suite passed 25/25 with zero skips.
The raw successful gate is `full-gate-finalization.log.gz`. A finalization attempt
used the wrong driver environment variable and failed a nonpositive virtual-time
measurement; `full-gate-finalization-virtual-time.log.gz` preserves that failure.
No assertions or budgets changed. Python bindings and new sanitizer runs are
outside this verification.

All 34 evidence files tracked at the reviewed original head remain byte-identical,
including the ten historical smoke files. The 95-file smoke manifest, 18 controller
hashes, five archived regression sources and all six calibration output manifests
were checked against the preserved bytes and implementation source. The original
pending receipt is retained as `verification-before-finalization.json`.
`SHA256SUMS` covers every other file in this evidence directory. Checksums establish
byte consistency, not independent attestation that a run occurred; the planned
independent security and evidence-integrity review remains required.

Implementation CI passed on Ubuntu and macOS in
[run 40544808339](https://github.com/mrjohnnyrocha/jtml/actions/runs/40544808339),
at `283006d411a7fe5e6848e8fb3d1bd8d243adf842`; its job metadata is archived in
`implementation-ci.json`. That result is not a claim about CI on the later
documentation/evidence commit. This finalization changes no executable source.
PR #51 remains draft and unmerged, with paid execution still NO-GO.
