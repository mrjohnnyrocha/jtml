# Clean-browser initializer ownership

On baseline `817b155e46087cfb84bd67067eb7b833e5240d0d`, the compiler skipped
every declaration at a repeated physical slot and replaced the entire project's
initialization certificate with `stateOrder: null`. Serialization retained the
last initializer for each slot. Bootstrap evaluated that projection in object
key order.

The strict-CSP Chromium reproductions use the original Friendly lowering paths.

| Source shape | Baseline | Certified build |
| --- | --- | --- |
| Component-only import; private `seed=7`; `let slot=0` then `let slot=seed` | Null order, effective expression `seed`, state and DOM `7` | `E_INIT_UNRESOLVED` at the second declaration |
| Same source with the two declarations reversed | Null order, effective literal `0`, state and DOM `0` | `E_INIT_UNRESOLVED` at the first declaration |
| `let z=2` then `let a=z+1`, with `scene3d … into slot` and no authored slot | Order `z,a,slot`, DOM `3` | Same order and DOM |
| Same scene with an earlier authored `let slot={}` | Null order, `z=2`, `a` absent, DOM `{{a}}` | Order `slot,z,a`, `a=3`, DOM `3` |

Module initialization certifies every original expression, including inactive
branches and authored expressions overridden by lowering. Reads must resolve
in the defining module or through a retained import. Local and imported
dependencies must precede the consumer. Ownership validation runs before
selecting any effective slot.

Friendly scene and media controllers attach provenance to their generated
defaults. Within one owner, a mutable authored controller slot and a tagged
default for that slot use the existing single effective initializer. The
default retains the previous emitted payload and cannot read state. The
effective operation takes the first declaration position. Multiple authored
declarations and conflicting controller defaults fail closed. This policy
does not generalize duplicate names into declaration identity.

The linked projection must agree with the certified module initializer.
The browser manifest validator checks the effective merged plans and each
projection before writing assets. Nonempty clean state requires a certificate.
Bootstrap continues to use the existing evaluator and stops before exposing
initialized state if the evaluator reports failure. This does not select a
new scalar domain or runtime fault policy. Empty projects retain the existing
null marker. Explicit compatibility and debug builds retain their legacy order.

Reproduce with `node tests/browser/mutable_initialization.cjs` using the
required Chromium and Playwright environment. The test includes source-located
rejections, previous asset preservation, the scene controller, source and
derived state, DOM, and the unchanged authored initialization vectors.
