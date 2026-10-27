# Four-trial runner review

The unfinished runner was preserved in
`../evidence/runner-audit/unfinished-runner.tar.gz` before simplification. Its
source and uncommitted harness edits are retained there; nothing was discarded
without an identified reason.

| Component | Decision for this pilot |
| --- | --- |
| Docker worker | Keep: image/compiler hashes, no network, read-only root, one candidate mount, fixed CPU/memory/PID limits. |
| External shell controller | Keep: fresh input array/session for each task/stack; credentials and evaluator stay outside the worker. |
| Build/evaluation separation | Keep: stop worker, snapshot submission, build without evaluator access, then evaluate static output in a separate browser container. |
| Token/cost reservations | Keep: exact provider input count plus capped output; reconcile reported usage; stop on unknown usage. Rates bound spending but do not establish actual billing. |
| Freeze/seal | Keep one manifest: packets, controller, oracle, lockfile and readiness receipts; no registry, scheduler, plugin system or resumable campaign service. |
| Five billing projects/keys | Remove mandatory uniqueness. One account/key is sufficient if receipts genuinely attribute costs to each trial. Use dedicated projects/windows only when the provider cannot attribute by request. |
| Separate paid API smoke command | Remove from the runner. Deterministic smoke is free; any provider verification requires separate explicit authorization and real billing evidence. |
| Result reporting | Keep four fixed rows. Stub/reference/development sessions cannot become real trials; missing actual billing prevents scoring. |
| Twenty-task execution | Deferred. Only the two public tasks execute. |

The deterministic provider emits a source-writing shell tool call and a final
submission through the same controller as real requests. One correct application
and one seeded broken application must be recorded by the unchanged browser
oracle. Synthetic usage exists only to exercise budget logic and is labelled
`stubBudgetUsage`; provider token totals and actual cost stay unavailable.

Unit tests validate ledger/seal/authorization rules, but cannot prove container
isolation or provider billing. The Docker smoke must demonstrate the former;
reviewed provider evidence and an authorized live integration check must establish
the latter. A successful stub run alone never authorizes or establishes readiness
for paid trials.
