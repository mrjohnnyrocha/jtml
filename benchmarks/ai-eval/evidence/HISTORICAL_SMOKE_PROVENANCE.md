# Historical smoke attribution correction

The files under `infrastructure-smoke/` are preserved byte-for-byte from
PR #51 head `4f0e2eeb8df86b9e389a51ca6b6a89b6dc1e4cf0`. They are historical
pipeline evidence, **not proof that the controller at that head passed smoke**.

Inside `pipeline-evidence.tar.gz`, `stub-smoke-001/result.json` and the
`stub-smoke-001/frozen/evaluator/` source agree on these executed hashes:

| File | Archived executed SHA-256 | Incorrectly attributed outer receipt SHA-256 |
| --- | --- | --- |
| `agent/run.mjs` | `df1a97f7a28cf087f34cb21324d11f1f13277b84f4c5a1bce95d8bfa6c23c26f` | `6f80b84e027061359d49eb45204f818eab0857c42f3a95cb7841f09d60ff180f` |
| `agent/core.mjs` | `0d55ecbf3986cabfc49c69db36f2487d90acfc0e14d7bc64e12fb8a262362051` | `f36d37142d150abab149d52021e964f5c85aff7451caf69928192f869251c694` |
| `agent/summary.mjs` | `40ab03621c2886bd0ee456a613bdf100017a8f7057925d85dc4364a9f88d3b1d` | `f55788be15bac7239018dd39eb0492879d7c13901a79cbdc7b39ec31204a8fdd` |

The outer `success-and-failure.json` has final-source hashes that differ from
its archived execution. The archive is the evidence of what actually ran; no
Git commit identity is inferred for that earlier worktree. Its functional
observations remain historical, and its receipt is rejected by the corrected
verifier. No historical receipt, archive, checksum or result has been repaired
in place. New verification lives separately in `review-fixes/`.
