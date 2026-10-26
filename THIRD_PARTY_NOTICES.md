# Third-party notices and licensing scope

JTML-owned material is offered under Apache-2.0; this does **not** relicense the
components below. Their existing source notices remain intact. Full upstream
texts are in `third_party/licenses/` and are included in CLI archives and CMake
installations. Version numbers identify the reviewed sources, not a blanket
license clearance of future dependencies.

| Component | Reviewed source | Terms / retained text | Use |
| --- | --- | --- | --- |
| WebSocket++ | 0.8.2, CMake SHA256-pinned archive | BSD-3-Clause main library; bundled base64/MD5 zlib-style, SHA1 BSD and UTF8 MIT notices; complete `websocketpp-COPYING` | Compiled live backend |
| Standalone Asio | 1.30.2, CMake SHA256-pinned archive | Boost Software License 1.0; `asio-LICENSE_1_0.txt`, `asio-COPYING` | Compiled live backend |
| cpp-httplib | vendored header identifies 0.18.3 | MIT; `cpp-httplib-LICENSE`; header's 2024 Yuji Hirose attribution retained | HTTP adapters |
| JSON for Modern C++ | upstream snapshot `bf8ccc20e9d3bf17083e9e9ab8703c11f3eaf82a` (2025-01-22), retained 3.11.3 banner | MIT; `nlohmann-json-LICENSE.MIT`; additional attributions below | Compiler/runtime/tooling |
| GoogleTest | v1.14.0 | BSD-3-Clause; `googletest-LICENSE` | Test builds; test binaries are not part of CLI archives |

The nlohmann header carries these additional original notices verbatim:

```
SPDX-FileCopyrightText: 2013 - 2025 Niels Lohmann <https://nlohmann.me>
SPDX-FileCopyrightText: 2008 - 2009 Björn Hoehrmann <bjoern@hoehrmann.de>
SPDX-FileCopyrightText: 2009 Florian Loitsch <https://florian.loitsch.com/>
SPDX-FileCopyrightText: 2016 - 2021 Evan Nemerson <evan@nemerson.com>
SPDX-FileCopyrightText: 2018 The Abseil Authors
```

It also identifies Apache-2.0 code from Abseil commit
`10cb35e459f5ecca5b2ff107635da0bfa41011b4` in its pre-C++14 compatibility
branch. The Apache-2.0 text is supplied in root `LICENSE`; this attribution and
the source comments remain intact. The vendored header matches the identified
2025 snapshot except for its omitted final LF; it is not the stable v3.11.3 header.
The retained MIT text now matches that snapshot's 2013–2025 attribution, replacing
the older tag's 2013–2022 text without changing license terms. Exact source/hash
comparison and the date explanation are recorded in the
[provenance record](docs/releases/licensing-provenance.md#nlohmannjson-import).

## Source locations for the retained license texts

- [Apache-2.0 official text](https://www.apache.org/licenses/LICENSE-2.0.txt)
- [WebSocket++ 0.8.2 COPYING](https://github.com/zaphoyd/websocketpp/blob/0.8.2/COPYING)
- [Asio 1.30.2 license](https://github.com/chriskohlhoff/asio/blob/asio-1-30-2/asio/LICENSE_1_0.txt)
- [cpp-httplib 0.18.3 license](https://github.com/yhirose/cpp-httplib/blob/v0.18.3/LICENSE)
- [nlohmann/json snapshot MIT license](https://github.com/nlohmann/json/blob/bf8ccc20e9d3bf17083e9e9ab8703c11f3eaf82a/LICENSE.MIT)
- [GoogleTest 1.14.0 license](https://github.com/google/googletest/blob/v1.14.0/LICENSE)

## Optional tools, assets and owner review

The editor source declares `vscode-languageclient ^9.0.1`; downloaded npm modules
are not included by the CLI packaging script. A published VSIX would need its
own resolved dependency/license inventory. Optional pybind11/Python builds,
OS libraries and the externally installed Playwright/Chromium test toolchain
are not redistributed in these CLI artifacts and are not relicensed here.

The repository SVG (`demos/opspulse/assets/ops-workspace.svg`), site CSS/JS,
examples, documentation and editor resources show no separate third-party
license marker in the reviewed source tree. This is an inventory observation,
not proof of authorship. External URLs in examples confer no rights over remote
content. Git authorship is not copyright assignment. The owner explicitly
confirmed the necessary rights and authority on 2026-10-26 for project-owned code,
contributions, documentation, examples, website, editor assets and graphics, while
preserving third-party terms and previous licenses. The exact
[owner statement](docs/releases/licensing-provenance.md#owner-confirmation) is
recorded separately; it does not revoke the editor's earlier MIT grants or approve
publication of an unverified artifact.

No root NOTICE is added: the inspected dependency distributions do not supply an
applicable Apache NOTICE requiring propagation. Required MIT/BSD/zlib-style
copyright and permission notices are instead retained above and in their full
license texts. JTML is not an Apache Software Foundation project.
