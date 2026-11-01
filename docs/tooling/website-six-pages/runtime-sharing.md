# Shared website runtime packaging

Base: clean main `3370f63`, after PR #71. Compiler pin remains
`325bae128aaaa729d4876159099e63fce9fe59c4`; releases, compiler sources,
runtime implementation, browserBuild metadata and compatibility contracts are
unchanged. This changes website packaging only.

`site/pages.json` schema 2 declares `sharedRuntime: jtml-runtime.js`. All six
public HTML documents reference `./jtml-runtime.js`, preserving both `/` and
`/jtml/` mounting. Each retains its isolated `components/index.js` and `app.js`.
The standalone `demo/counter/` retains its complete original independent bundle.
No cache headers, service worker, runtime rewriting or framework are introduced.

Before publishing assets the packager compares the actual bytes of all six
compiled runtimes. A mismatch raises an error before replacing the public site.
Every original compiler-generated HTML/JS file is preserved under the sibling
`<output>.compiler-output/<page-id>/`; the Counter is preserved under `counter/`.
This review directory is not deployed. The Pages workflow uploads it separately.
`site-build.json` schema 5 keeps the original generated hashes and adds the shared
runtime path/hash and six verified pages. Chromium verifies preserved originals,
byte-identical deployed JS, registry/source hashes and manifest integrity.
Packaging still changes only document metadata and external script URLs.

## Measurements

Exact baseline runtime equality was checked before deduplication: six copies,
SHA-256 `8a7d54918027ab04e58781ed1a980605504fdbf26c26209b37a70de9d5ad81a1`,
272,370 raw / 50,835 gzip bytes each. Five removed copies save **1,361,850 raw /
254,175 gzip bytes** across deployed runtime paths. The root runtime is copied
verbatim from the homepage compiler output. Original files consume review storage;
the storage saving applies to the public deployment, not those retained originals.

Cold page HTML + JS + CSS, bytes **raw / gzip**, same pinned compiler:

| Page | Before | After |
| --- | ---: | ---: |
| index.html | 881,717 / 99,033 | 881,717 / 99,033 |
| reference.html | 931,836 / 102,331 | 931,819 / 102,334 |
| examples.html | 622,775 / 79,923 | 622,759 / 79,926 |
| tools.html | 741,338 / 87,092 | 741,325 / 87,095 |
| security.html | 698,983 / 84,468 | 698,967 / 84,471 |
| deploy.html | 706,319 / 87,431 | 706,305 / 87,433 |

A shared cacheable runtime **does not reduce a cold page's runtime requirement**:
each page still needs all 272,370 raw / 50,835 gzip runtime bytes and all three
script resources. Shorter runtime URLs remove 76 raw HTML bytes across the five
non-home pages; gzip HTML grows by 14 bytes in aggregate. With a reusable HTTP
cache entry, subsequent pages can avoid transferring the same runtime. Chromium
checks six navigations and at least five runtime cache hits under each mount,
using an explicit cache header on the test server. Actual hosted cache policy,
transfer encoding and cache eviction are not guaranteed by that test.

Compression is Python stdlib gzip level 9, mtime 0, zlib 1.2.12, each file
independently; these are estimates, not observed hosted response headers.
`site-payloads.json` schema 3 counts the runtime once per cold page and once in
the unique deployed inventory. The latter includes sources, docs, screenshots,
legal assets and the independent Counter; it explicitly excludes the three
self-referential metadata reports. The PR reports separately measured complete
deployment totals including those reports, so metadata/doc changes remain visible.

## Verification and limits

Run `python3 tests/test_site_registry.py` for registry guards, actual-byte mismatch
rejection and a full packaging failure-injection check preserving the previous
public output and six mismatched originals. Run the website Chromium suite with
`JTML_WEBSITE_BASELINE` pointing to the unchanged PR #71 site; all existing content,
CSP, links, focus, Counter/Reference, mounting and viewport checks remain required,
as do exact shared-runtime/isolated-bundle requests and cache reuse. Homepage and
Reference screenshots must remain byte-identical to that baseline. Repeated builds
must have identical public manifests and original compiler files.

The complete `scripts/verify_all.sh` gate must run on the committed final candidate
with real Chromium and `JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1`. Exact final SHA,
results, full deployment accounting and hosted CI are reported in the focused PR.
No merge or deployment is performed. This packaging change does not reduce inline
manifest/component payloads or resolve known native renderer, source-span or
reference HTML-size limitations, and does not establish production readiness.
