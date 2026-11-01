# Complete JTML public website

The subsequent [mixed static/browser packaging](static-pages.md) supersedes the
per-page runtime layout and payload totals below. This remains the PR #71
source/content migration record.

This is the PR #71 migration record. The subsequent packaging-only
[runtime sharing change](runtime-sharing.md) supersedes the per-page runtime
layout and schema versions below; source, screenshots and historical measurements
remain unchanged.

Baseline: clean main `70bb3a2c639cd3b9daeab582514037f8e352b5ee` after PR #70.
All six public pages are now canonical Friendly JTML 2. No `.html` or application
JavaScript source remains under `site/`; there is no raw HTML escape hatch,
framework, compatibility fallback or invented interaction on the four static pages.

## Registry and source inventory

`site/pages.json` is the declarative authority for the six entrypoints, public
URLs, isolated asset directories, CSS, titles, descriptions and optional theme
color. Compilation, document metadata, source/asset provenance, payload reporting
and browser expectations consume it. The builder has no per-page metadata or
compilation branches. Validation rejects colliding outputs, escaped paths, missing
metadata/assets, changed public URL inventory and handwritten page HTML inputs.

| Source | Public URL | JavaScript directory | Desktop / mobile |
| --- | --- | --- | --- |
| `site/index.jtml` | `index.html` | `./` | [Desktop](desktop/index.png) / [mobile](mobile/index.png) |
| `site/reference.jtml` | `reference.html` | `reference-assets/` | [Desktop](desktop/reference.png) / [mobile](mobile/reference.png) |
| `site/examples.jtml` | `examples.html` | `examples-assets/` | [Desktop](desktop/examples.png) / [mobile](mobile/examples.png) |
| `site/tools.jtml` | `tools.html` | `tools-assets/` | [Desktop](desktop/tools.png) / [mobile](mobile/tools.png) |
| `site/security.jtml` | `security.html` | `security-assets/` | [Desktop](desktop/security.png) / [mobile](mobile/security.png) |
| `site/deploy.jtml` | `deploy.html` | `deploy-assets/` | [Desktop](desktop/deploy.png) / [mobile](mobile/deploy.png) |

Each bundle contains compiler-owned `jtml-runtime.js`, `components/index.js`
and `app.js`. The emitted `index.html` becomes the registered public HTML file.
Packaging changes document metadata and those three script URLs only. Generated
application markup and JavaScript stay intact; emitted and deployed hashes are
auditable. The original compiled Counter stays under `demo/counter/`.

`SiteNav` and `SiteFooter` are reused unchanged. `SitePages` is a small shared
native navigation component for the four migrated pages, preserving their old
page destinations without changing the existing homepage/Reference header.
Original CTA and legal/footer material are retained as well.

## Preservation and acceptance

An independent fixture extracted from the original HTML records every main
paragraph, code listing, heading/tag, link destination/accessible name and footer
note. Chromium requires exact preserved content, including RC.3 downloads,
checksums, installation/platform instructions and security warnings. Native links
are the only interactions on the four migrated pages; no synthetic controls were
added. The existing Counter and Reference disclosure retain their own JTML state.

The suite covers 1440×1000, 900×1100, 390×844 and 320×720 under both `/` and
`/jtml/` (eight configurations, six pages each). It checks generated native
headings, accessible live DOM, titles/descriptions/viewport/canonicals/CSS,
source/registry/generated-JS hashes, exact page-bundle requests, local links and
cross-page fragment targets, keyboard skip links, no horizontal overflow, CSP
attack negatives, both Counters and Reference click/Enter/Space/reload behavior.
Unexpected browser/console/network errors, API calls and WebSockets fail.

With a clean-main baseline supplied, full-page homepage and Reference PNG buffers
must also match pixel-for-pixel in all eight configurations. Source entry files
and their CSS are unchanged; the new shared definition adds generated metadata
but does not alter their visible content or behavior.

```sh
scripts/build_site.sh dist/site
python3 tests/test_site_registry.py
# Set the installed real Playwright/Chromium paths:
export JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT=/absolute/playwright-prefix
export JTML_BENCH_BROWSER=/absolute/chrome-headless-shell
node tests/browser/site.cjs dist/site /absolute/clean-main-site
# Run once after committing the final candidate:
JTML_WEBSITE_BASELINE=/absolute/clean-main-site \
  JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1 scripts/verify_all.sh
```

The PR records the exact final commit, full-gate exit/result and Ubuntu/macOS CI
and website-preview runs. No merge, deployment, Pages setting, DNS or domain
change is authorized by this migration. Manual publication controls are retained.

## Payloads and redundancy

Bytes, **raw / gzip**. Local Python stdlib gzip level 9, mtime 0, zlib 1.2.12,
each asset independently. These measure HTML/JS/CSS per cold page, not hosted
transfer headers, documentation downloads or the favicon. Shared CSS contributes
to each page total; no compiler-output optimization or performance improvement
is claimed. [Full per-file/type accounting](payloads.json).

| Page | HTML | JavaScript | CSS | Total |
| --- | ---: | ---: | ---: | ---: |
| index.html | 293,672 / 27,034 | 578,831 / 69,553 | 9,214 / 2,446 | 881,717 / 99,033 |
| reference.html | 295,702 / 24,845 | 624,050 / 74,116 | 12,084 / 3,370 | 931,836 / 102,331 |
| examples.html | 154,240 / 13,045 | 455,885 / 63,462 | 12,650 / 3,416 | 622,775 / 79,923 |
| tools.html | 215,345 / 17,648 | 513,343 / 66,028 | 12,650 / 3,416 | 741,338 / 87,092 |
| security.html | 192,556 / 16,032 | 493,777 / 65,020 | 12,650 / 3,416 | 698,983 / 84,468 |
| deploy.html | 202,742 / 17,984 | 490,927 / 66,031 | 12,650 / 3,416 | 706,319 / 87,431 |

All six bundles contain byte-identical runtimes: 272,370 raw / 50,835 gzip
bytes each. Five additional copies contribute 1,361,850 raw / 254,175 gzip bytes
across the public-page bundle URLs; separate URLs do not share a cache entry.
The pre-existing standalone Counter is outside this six-page total.

| Page | Inline client manifest, raw / gzip | Exact repeated bodyPlan bytes |
| --- | ---: | ---: |
| index.html | 259,635 / 18,832 | 35,319 |
| reference.html | 260,761 / 17,231 | 84,981 |
| examples.html | 136,254 / 9,152 | 20,557 |
| tools.html | 191,228 / 12,226 | 20,557 |
| security.html | 170,764 / 11,132 | 20,557 |
| deploy.html | 176,650 / 12,140 | 20,557 |

Repeated arrays are counted as canonical compact JSON bytes beyond the first
identical occurrence. Initial markup, templates and static component creation
code also overlap in content. These counts are observations, not safely removable
byte budgets or estimates of compressed savings. Runtime/manifest ownership
would need a separate proven compiler change before removing data.

The baseline homepage/Reference totals were 861,347 / 98,242 and 911,492 /
101,588 bytes respectively. Their new shared-navigation definition increases
generated output despite identical pixels. The four handwritten pages were much
smaller than their compiled counterparts; this is a source-authoring migration,
not a payload improvement.

## Compiler provenance and limitations

The unchanged compiler pin is `325bae128aaaa729d4876159099e63fce9fe59c4`,
`jtml 0.3.0-dev`. The existing helper builds an exact git-archive snapshot and
records actual binary identity. `site-build.json` schema 4 additionally records
the registry SHA256, per-page entry/shared inputs and original generated asset
hashes. Caller-supplied unprovenanced compilers stay explicitly unverified.
`manifest.txt` hashes the complete site; repeat builds on identical tool inputs
must be byte-identical. Cross-platform bit-identical compiler binaries are not
claimed.

No unsupported-language blocker was found, and no compiler/runtime code or
budgets changed. Static component composition preserves authored fragment IDs;
native focusable links provide skip destinations within the known component
`tabindex` boundary. All six sources check, compile and lint with exit 0.
Four false `into` lint advisories remain unsuppressed in quoted Reference
examples (two), Examples prose (one), and Tools prose (one). Readable original
material is retained rather than reworded to hide those diagnostics.

Handwritten material is limited to CSS, the favicon SVG, packaging/head metadata
and build/test tooling. The inherited `styles.css` URL remains available; it is
no longer loaded by any public page. There is no handwritten application HTML
or application JavaScript.
