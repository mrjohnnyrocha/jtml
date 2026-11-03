# Minimal independent scalar reference model

Implementation baseline: main `27d05e9497911f89f3d463dd0fb68ea67fe65b33`, after
PR #77. This first executable slice selects **exactly V01, V03, V04 and V05** from
[the authored vectors](../../docs/spec/reactive-core-v1/vectors.json). It implements
only their successful scalar episodes, not the complete proposed Reactive Core.

## Independence and profile assumptions

`model.py` takes only a semantic program AST and external action inputs. It has
no source parser, file IO, vector IDs, oracle input, browser adapter or production
imports. It was authored from the scalar operational rules and these source
examples, without reading/translating production evaluation or scheduler code for
this implementation. All imports are Python standard library. `audit.py` checks
fixture provenance; `compare.py` reads expectations and old browser observations
**outside** the evaluator and only after running it.

The explicit `s1-minimal-success-v1` profile assumes finite binary64 numeric,
Boolean and Unicode string literals, numeric `+`/`*`, scalar-to-string `+`, resolved
read IDs, mutable `let`, pure `get`, zero-argument synchronous actions containing
sequential assignments, and ordered virtual text segments. S1 is an explicit
model experiment authorized for this slice, **not language-wide A6a ratification**.
Value/action namespaces are separate; all declarations share one lexical scope.
The AST carries opaque resolved IDs; source spellings never provide a fallback.

Derived reads recursively substitute a formula against the current immutable
source-state map, every time. There is no cached derived state, dirty scheduler,
Runtime Plan, topological execution queue or fixed-pass limit. A DFS recursion
stack rejects cycles before initialization. Mutable initializers retain their
list/source order and may read earlier lets only. Derived declaration order is
irrelevant. Every assignment produces a new map; each successful external action
produces one immutable `Commit` after the initial commit 0. Snapshot values and
virtual text are deterministic; changing a returned JSON view cannot mutate them.
Host recursion exhaustion is explicitly `MODEL_LIMIT`, never a truncated value.

Only already-separated literal/read segments are needed for the four texts. Their
normal source `{e}` sites were manually lowered to segments with scalar text
conversion, consistent with the B1 preferred direction. **No interpolation/escape
parser is implemented**, and this does not select general A6b rules. Numeric text
uses standard-library float representation with integral `.0` removed and signed
zero rendered `0`; cross-backend formatting edge cases beyond these exact small
integer vectors are not certified here.

A6c remains undecided. A dynamic S1 operand/non-finite-result error raises
`PendingPolicy(PENDING_POLICY)` and yields **no failure-state oracle**. There is
no model session, rollback, resumption, recovery or instance-termination API.
Immutable maps used in successful computation do not ratify persistent-state
rollback in a failed JTML episode. Effects, parameters, conditionals, imports,
components/lifetimes, arbitrary operators, calls, general DOM and host callbacks
are explicitly unsupported, not ignored. Unknown JSON fields/shapes reject;
there is no eval, source-string interpretation or implicit legacy namespace.

## Source-to-AST audit

The four fixtures were independently authored and manually checked against the
original source in PR #77's catalog; no production AST/IR, compiler output or
expected result was used to construct them. “Independent” here means independent
of the production implementation and its oracles; it does **not** claim a separate
human reviewer has approved this PR. This crosswalk and the full fixture source
are provided for that review.

Each fixture contains the exact source bytes, SHA-256 digest, separately authored
AST and canonical AST digest (UTF-8 JSON with sorted keys and compact separators).
Its audit accounts for **every original source line**, including static UI and
entry transport, and ties executable lines to JSON pointers and original positions.
`audit.py` compares the source copy/digest with the original catalog, checks the
AST digest, line coverage, JSON pointers, source positions and action input sequence,
and invokes the closed model schema validator. Hashes/checks detect drift; they
are not an automatic proof of semantic equivalence or a substitute for manual
source-to-AST review. There is deliberately no general JTML parser.

| Fixture | Independently reviewed declaration/action translation | Text and UI audit |
| --- | --- | --- |
| V01 | L2: let x=1. L3–4: zero-arg step, assign the resolved x to read(x)+1 | L7: literal `Value ` then read(x). L1/L5–6 are dialect/static wrappers; L8 binds Step to step |
| V03 | L2–3: x=0, saved=0. L4: twice=read(x)*2. L5–7: first assign x=1, then saved=read(twice), in that order | L10: read(saved), literal `\|`, read(twice). L1/L8–9 wrappers; L11 dispatches step |
| V04 | L2: x=1. L3: z=read(x)+1. L4: a=read(z)*2. L5–6: step assigns x=read(x)+1 | L9: read(a), `\|`, read(z). L1/L7–8 wrappers; L10 dispatches step |
| V05 | L2: x=1. L3–5: zleft=x+1, zright=x*2, atotal=read(zleft)+read(zright). L6–7: step assigns x=read(x)+1 | L10: left, `\|`, right, `\|`, total, in source order. L1/L8–9 wrappers; L11 dispatches step |

`main::x` etc. are manually resolved declaration identities in one scope, not a
runtime storage-key guess. The action entry lists come from the vector inputs,
not its expected outputs. Static page/box/button markup is accounted for but not
rendered; `.out` is a virtual observation label. No general CSS/DOM semantics are
inferred. Metamorphic tests operate on synthetic ASTs; they do not claim those
transformed ASTs still correspond to the unmodified original source digest.

## Run the focused checks

From the repository root, with Python 3 standard library only:

```sh
python3 -m unittest discover -s tests/reference_model -v
python3 tests/reference_model/audit.py
python3 tests/reference_model/compare.py --output tests/reference_model/differential.json
```

The first two commands pass. The comparison command deliberately exits **1** when
it records a model or browser snapshot FAIL. On the preserved observations it
returns 1 for the three known browser discrepancies; those remain FAIL. This is
not installed as an expected-failure waiver or substituted for an existing gate.
The report is deterministic and can be regenerated byte-for-byte. No browser or
C++ executable is invoked by any of these commands.

The Python validator is the executable closed schema check (no JSON-schema
package dependency). Tests also check missing/unsupported fields, scalar kinds,
source/AST digest tampering and source positions. The metamorphic checks include
independent let permutations, all diamond-get permutations, generated chains and
layered diamonds with closed-form oracles, opaque identity renaming, and 100
repeated actions. Cyclic and unresolved inputs reject explicitly.

## Differential evidence

[differential.json](differential.json) compares independently authored vector
expectations with this model, and separately with the unchanged
[PR #77 Chromium observations](../../docs/spec/reactive-core-v1/baseline-observations.json).
That browser evidence is pinned to `ae8f984dc3c35163cc702f7e494a5f2b015124fb`, as
recorded in PR #77; it is not a new browser run at the model checkout baseline.
The report records its digest. Only for these four audited single-scope examples,
the comparison adapter labels raw browser values by their independently reviewed
root declaration names; that mapping is not a general module identity resolver
and is never input to the model.

| Vector | Authored expectation / independent model | Pinned Chromium observation | Classification |
| --- | --- | --- | --- |
| V01 | x: 1 → 2 → 3; text Value 1 → Value 2 → Value 3 | Same post-load/post-click state/text | MODEL PASS / BROWSER SNAPSHOT PASS |
| V03 | First action saves 2 and twice=2; text `2\|2` | saved=0, twice=2; text `0\|2` | MODEL PASS / BROWSER FAIL |
| V04 | First action z=3/a=6; second z=4/a=8 | First z=3/a=4; second z=4/a=6 | MODEL PASS / BROWSER FAIL |
| V05 | First left=3/right=4/total=7; second total=10 | First left=3/right=4/total=4; second total=7 | MODEL PASS / BROWSER FAIL |

Model results are computed, not case-dispatched or copied from this table.
Browser phase/commit-count/notification/MutationObserver and lifetime evidence
is **UNVERIFIED**, including for V01. Four model passes do not turn the original
21 Chromium executions into conformance passes. No expected vector or historical
observation is changed to match the runtime.

No production runtime/compiler, language syntax, CI, budget or existing required
gate changes here. This Python-only test tool does not trigger AGENTS.md's runtime/
CLI/build-behavior requirement for verify_all.sh. Focused Python/schema checks and
`git diff --check` are the validation for this slice; unrelated C++ rebuilds and
Chromium reruns are intentionally not performed. The remaining 20 vectors, A6c,
full scalar operator/formatting coverage and full browser atomicity stay outside
this PR. Stop at this slice; production discrepancies are not fixed here.
