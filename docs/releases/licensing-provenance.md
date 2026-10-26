# RC.2 licensing authority and dependency provenance

Recorded 2026-10-26, following the audit of main
`15df41d54e48ea7115afa1b9b80735bd64fb8e6d` (merged PR #47).

## Owner confirmation

The repository owner supplied this confirmation directly in the release-readiness
conversation on 2026-10-26:

> I confirm that I have the necessary rights and authority to distribute the JTML project-owned code, contributions, documentation, examples, website, editor assets and graphics under Apache-2.0, while preserving all applicable third-party terms and previous licenses.

This records the owner's statement of authority for the listed material, including
pre-Apache contributions and project assets. It does not invent copyright-holder
names/years or assignments, relicense third-party code, or revoke previous grants.
The editor's historical MIT declaration remains part of the repository history;
the current Apache-2.0 declaration is scoped to project-owned material. The root
Apache text is unchanged. No new licensing terms are introduced by this record.

The confirmation resolves the owner-authority approval item recorded in the prior
readiness audit. It does not approve an unbuilt RC.2 artifact, establish a platform
result, or authorize automatic merge, tagging or publication. Exact source/artifact
review and the separate publication decision remain required.

## nlohmann/json import

`third_party/json.hpp` was imported in `b0b3c65` and is unchanged by this correction.
The banner says 3.11.3, but the source is a later upstream development snapshot,
not the stable `v3.11.3` release header:

| Source | Bytes | SHA256 |
| --- | ---: | --- |
| Vendored `third_party/json.hpp` | 952699 | `5f49960a57b014bc38a100f0c119dba49a349452ab263481375bf7d18fcceaf7` |
| Upstream `bf8ccc20e9d3bf17083e9e9ab8703c11f3eaf82a`, 2025-01-22 | 952700 | `e7ca7e6c43eadf5c68ae5417149ab83ba185f0344c2f1d1beb16fe710ea11a3c` |
| Stable `v3.11.3` header | 919975 | `9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6` |

Byte comparison proves `upstream_bytes[:-1] == vendored_bytes`: the only difference
from the identified upstream snapshot is the omitted final LF. The stable tag is
different code, including absence of `JSON_DIAGNOSTIC_POSITIONS`; substituting it
would be a code change and is not part of this licensing correction.

The snapshot's header and MIT license carry 2013–2025 attribution. The older stable
tag has 2013–2023 in its header and 2013–2022 in LICENSE.MIT. This explains the date
discrepancy: the previously retained license text came from the older tag, while
the header came from the 2025 snapshot. The 2025 attribution was already retained
in THIRD_PARTY_NOTICES; the date mismatch alone did not demonstrate missing
attribution. The retained MIT file now matches the identified snapshot verbatim:
SHA256 `46a65cffd1ea955132d95a8dd921640714a8d6b537d2e4e482d31145ae95b603`.

Pinned upstream sources:

- [Identified header](https://github.com/nlohmann/json/blob/bf8ccc20e9d3bf17083e9e9ab8703c11f3eaf82a/single_include/nlohmann/json.hpp)
- [Matching MIT license](https://github.com/nlohmann/json/blob/bf8ccc20e9d3bf17083e9e9ab8703c11f3eaf82a/LICENSE.MIT)
- [Stable tag header](https://github.com/nlohmann/json/blob/v3.11.3/single_include/nlohmann/json.hpp)
- [Stable tag MIT license](https://github.com/nlohmann/json/blob/v3.11.3/LICENSE.MIT)

Björn Hoehrmann, Florian Loitsch, Evan Nemerson and Abseil notices remain intact.
The referenced Abseil revision `10cb35e459f5ecca5b2ff107635da0bfa41011b4` has
AUTHORS/LICENSE and no root NOTICE. Its Apache text and attribution are retained.
WebSocket++, Asio, cpp-httplib and GoogleTest notices are unchanged. The existing
packagers copy root/dependency licenses into the bundle and CMake/CPack layouts;
source editor packaging excludes node_modules. Python/VSIX redistribution remains
outside this candidate's scope and would require its own resolved inventory.

## Readiness verification boundary

Focused local checks pass: seven RC guards (including rejection of RC.1 metadata),
six macOS artifact guards, archive reproducibility/changed-content controls,
dev/RC.1/RC.2/final CMake version mapping, README consistency, YAML parsing and
embedded Bash syntax. Workflow comparison permits only the RC identity changes;
its manual trigger, read-only permissions, platform matrix and checks are intact.
The source/header/license hashes and literal owner statement were independently
checked. No compiler, runtime, build implementation or vendored header changed.

The full compiler gate is not repeated for this documentation/license/RC-identity
correction. The inherited source baseline is PR #47 head
`af65a4cc35a68722603cfd166e32989176dd8f4c`: 476 CTests, full local gate with nine
mandatory clean-exit benchmarks, and [Ubuntu/macOS CI](https://github.com/mrjohnnyrocha/jtml/actions/runs/40324000920).
Fresh PR checks are separate evidence. Actual RC.2 workflow artifacts and their
physical macOS 14.4.1 download acceptance remain pending; the RC workflow is not
triggered by this readiness change.
