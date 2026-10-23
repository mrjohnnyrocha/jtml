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
| JSON for Modern C++ | vendored header identifies 3.11.3 | MIT; `nlohmann-json-LICENSE.MIT`; additional attributions below | Compiler/runtime/tooling |
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
the source comments remain intact. The vendored header's dates differ from the
upstream v3.11.3 LICENSE.MIT (2013–2022); both original notices are preserved.
Do not infer byte-for-byte upstream provenance from the version banner alone.

## Source locations for the retained license texts

- [Apache-2.0 official text](https://www.apache.org/licenses/LICENSE-2.0.txt)
- [WebSocket++ 0.8.2 COPYING](https://github.com/zaphoyd/websocketpp/blob/0.8.2/COPYING)
- [Asio 1.30.2 license](https://github.com/chriskohlhoff/asio/blob/asio-1-30-2/asio/LICENSE_1_0.txt)
- [cpp-httplib 0.18.3 license](https://github.com/yhirose/cpp-httplib/blob/v0.18.3/LICENSE)
- [nlohmann/json 3.11.3 MIT license](https://github.com/nlohmann/json/blob/v3.11.3/LICENSE.MIT)
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
content. Git authorship is not copyright assignment: the owner must confirm
contribution/asset provenance and authority to license project-owned material,
including the editor's earlier MIT metadata. Earlier grants are not revoked.

No root NOTICE is added: the inspected dependency distributions do not supply an
applicable Apache NOTICE requiring propagation. Required MIT/BSD/zlib-style
copyright and permission notices are instead retained above and in their full
license texts. JTML is not an Apache Software Foundation project.
