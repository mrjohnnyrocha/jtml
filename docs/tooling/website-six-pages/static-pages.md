# Four static public pages, two reactive browser pages

Base: main after PR #73, `69a2cb7a60bd12887cc434e47d22cce0e8a0a8c3`.
All six source files, shared JTML components, CSS, favicon and public page URLs are
unchanged. The compiler pin advances from `325bae128aaaa729d4876159099e63fce9fe59c4`
to the reviewed static compiler merge above. No compiler, language, runtime or
release implementation is changed by this migration.

## Registry, packaging and provenance

`site/pages.json` schema 3 gives every page a closed `target` field. Only Homepage
and Reference may use `browser`; they retain root/Reference component and bootstrap
bundles, sharing one root runtime. Examples, Tools, Security and Deploy use `static`
and have `assetDirectory: null`. Their compiler output must contain exactly
`index.html`, without scripts, manifests or runtime hooks. They never fall back to
browser compilation. The independent original Counter retains its complete browser
bundle under `demo/counter/`.

`site-build.json` schema 6 records targets, original compiler file hashes, deployed
HTML/asset hashes, source inputs, registry hash and compiler provenance. Every
original compiler file remains in `<output>.compiler-output/`, outside deployment.
Static application bodies are byte-identical to those originals; only head/language
metadata is packaged. Browser JavaScript remains byte-identical. Equality of the
two original browser runtimes is mandatory before sharing; a missing or differing
runtime fails the build. Static pages retain their compiler-issued strict CSP,
including `script-src 'none'`; browser pages retain the existing `script-src 'self'`
policy. There is exactly one CSP per document.

The complete website and original-output directories are prepared under a temporary
sibling directory before publication. Existing accepted directories are backed up
and restored together on publication failure. A failed compiler run leaves the
accepted pair intact and preserves partial compiler originals separately under
`<output>.failed-compiler-output/` for diagnosis. This is exception-safe publication,
not a crash/fsync durability guarantee. Tests inject runtime mismatch, late static
compiler rejection and failure between publishing the site and provenance.

The default helper builds the exact git-archive compiler snapshot with a checked
version/hash sidecar. Overrides without a sidecar remain explicitly unverified.
No compiler source revision is inferred from its version string. Repeated site
builds using identical inputs, compiler binary, tools and base URL must have
identical public files and identical original compiler files.

## Published baseline and real browser proof

The previous live deployment identifies source
`5f970b44e2c4f98595ef156050f093328c632686`, compiler
`325bae128aaaa729d4876159099e63fce9fe59c4`. All 303 deployed files were downloaded
read-only and SHA256-verified against its deployed manifest. The compact
`tests/fixtures/site/published-baseline.tar.gz` contains its rendering resources and
provenance, not handwritten website source. Its recorded archive SHA and each
selected file's deployed SHA are checked by `scripts/materialize_site_baseline.py`.
The full deployed mirror is used separately for complete-inventory measurement.
The rendering-only fixture intentionally cannot stand in for that full inventory.

Chromium compares all six current pages against these exact published bytes under
`/` and `/jtml/`, at 1440×1000, 900×1100, 390×844 and 320×720. Every configuration
requires independent content/code/link fixtures, authored/presentation/accessibility
attributes, accessible trees and pixel-identical screenshots. Existing fragment,
metadata, asset/MIME, overflow, skip-link, Counter and Reference interactions remain.
The four static pages repeat DOM/accessibility/pixel and keyboard-focus comparisons
with JavaScript disabled. They make zero JavaScript/API/WebSocket requests, also
when JavaScript is enabled. Independent inline-script and inline-handler attacks
must be blocked under both browser and static policies.

The two browser pages and original Counter have exactly the same original compiler
asset hashes as the published baseline, despite the compiler-pin update. No
reactive manifest or runtime optimization is hidden in this packaging change.

## Actual payloads

Values are **raw / gzip bytes**. Python gzip level 9, mtime 0, each file independently,
zlib 1.2.12. These are actual packaged resources, not DOM snapshots, timings or
hosted transfer headers. Cold totals include HTML, required JavaScript, CSS and
favicon. [Detailed measurements](../website-static/payloads.json) keep cold, warm
navigation and unique resource accounting separate.

| Page | Before cold | After cold | Target |
| --- | ---: | ---: | --- |
| index.html | 881,944 / 99,228 | 881,944 / 99,228 | browser |
| reference.html | 932,046 / 102,529 | 932,046 / 102,529 | browser |
| examples.html | 622,986 / 80,121 | 16,649 / 5,084 | static |
| tools.html | 741,552 / 87,290 | 17,938 / 5,520 | static |
| security.html | 699,194 / 84,666 | 17,548 / 5,512 | static |
| deploy.html | 706,532 / 87,628 | 18,432 / 5,964 | static |

Each static page retains 12,650 raw / 3,416 gzip CSS bytes plus a 227 raw / 195 gzip
favicon. The shared runtime remains 272,370 raw / 50,835 gzip bytes. It is required
by each cold browser page, and is not required by a cold static page. The physical
root runtime remains deployed; this migration does not claim its bytes disappeared
from the unique site inventory. The standalone Counter's runtime is independent.

| Navigate from cached Homepage | Before | After |
| --- | ---: | ---: |
| reference.html | 650,235 / 49,053 | 650,235 / 49,053 |
| examples.html | 341,175 / 26,645 | 7,208 / 2,443 |
| tools.html | 459,741 / 33,814 | 8,497 / 2,879 |
| security.html | 417,383 / 31,190 | 8,107 / 2,871 |
| deploy.html | 424,721 / 34,152 | 8,991 / 3,323 |

Warm figures assume fresh target HTML and reusable homepage resources already in
cache. If all three stylesheets are already cached, subsequent Tools, Security and
Deploy navigation requires only their HTML: 1,909, 1,901 and 2,353 gzip bytes.
Chromium's cacheable test server verifies exact script/CSS wire requests: Homepage
loads runtime/components/bootstrap/home.css; Reference reuses that runtime and
loads its components/bootstrap/reference.css; Examples first loads content.css;
Tools/Security/Deploy then need no new script or CSS requests. Hosted cache headers,
compression and eviction are not guaranteed by this local test.

The union of required six-page resources, each URL counted once including favicon,
changes from **3,164,891 / 271,172** to **1,554,674 / 156,887** bytes: a reduction of
**1,610,217 raw / 114,285 gzip** bytes. This differs from summing six cold pages,
which counts shared runtime/CSS per cold visit. Original compiler review files and
the baseline test fixture consume review/test storage, outside the deployed site.

`site-payloads.json` schema 4 records each cold page, modeled warm navigation, unique
page resources and unique deployed assets. Its deployed subtotal explicitly omits
`site-build.json`, `site-payloads.json` and `manifest.txt` to avoid self-reference.
The complete final-commit deployment totals, including every report, documentation
file, image, source and demo, are recorded in the PR and by the following command:

```sh
python3 scripts/measure_site_migration.py /absolute/full-published-mirror dist/site \
  --output dist/site-migration-measurements.json
```

The utility verifies full manifest coverage and refuses an incomplete rendering
fixture for complete-deployment accounting. Documentation/provenance overhead must
remain visible; it is not subtracted to inflate the reported net saving.

## Verification and boundaries

```sh
python3 tests/test_site_registry.py
scripts/build_site.sh dist/site
node tests/browser/site.cjs dist/site
# Reuse the exact checked pinned compiler for repeated builds:
JTML_BIN=/absolute/pinned/jtml scripts/build_site.sh dist/site-repeat
diff -qr dist/site dist/site-repeat
diff -qr dist/site.compiler-output dist/site-repeat.compiler-output
# Commit before the complete final gate; real Chromium is mandatory:
JTML_SITE_BIN=/absolute/pinned/jtml \
  JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1 scripts/verify_all.sh
```

The PR records exact final SHA, numeric gate exit, checks and any known warnings;
CI and preview results remain separate evidence. All actual compiler/runtime code,
budgets and manual Pages controls are unchanged. No merge, deployment, Pages
setting, DNS or release mutation is performed. Remaining native renderer and
reference HTML-size limitations are not resolved by these payload improvements.
