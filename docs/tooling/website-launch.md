# Public website: six JTML pages, static output, manual publication

All six public pages are authored in canonical Friendly JTML 2. The source/output
inventory and page metadata live in `site/pages.json`: `index`, `reference`,
`examples`, `tools`, `security` and `deploy` each map a `.jtml` entry to the existing
`.html` URL. No handwritten page HTML or application JavaScript remains in
`site/`. CSS and the favicon are handwritten presentation assets.

The [complete migration record](website-six-pages/README.md) contains exact
payload measurements, compiler boundaries, source preservation checks and desktop/
mobile screenshots. Earlier [homepage](website/README.md) and
[Reference](website-reference/README.md) records retain their historical scope.

## Build and provenance

```sh
scripts/build_site.sh dist/site
python3 -m http.server 8000 --bind 127.0.0.1 --directory dist/site
```

The builder consumes the validated registry for compilation, page URLs, CSS,
titles/descriptions/theme metadata, isolated asset directories and payloads.
Shared JTML navigation/footer components live in `site/shared/layout.jtml`.
Original documentation/install/release links, warnings, prose and source listings
remain available. Four migrated static pages use native navigation only; the
Counter and Reference disclosure remain genuine JTML interactions.

The registry (schema 3) declares a target for every page. Homepage and Reference
compile with `build --target browser`; they retain independent component/bootstrap
bundles and reference one byte-equal root `jtml-runtime.js`. Examples, Tools,
Security and Deploy compile with `build --target static`, emit only `index.html`,
and publish it at their unchanged `.html` URLs. Their asset directories are null:
no runtime, manifest, component module or bootstrap is deployed for these pages.
The original Counter remains independently browser-compiled under `demo/counter/`.
Packaging changes only document metadata and external script URLs. Source content,
CSS and generated application bodies/JavaScript remain compiler-owned.

`site/compiler.json` pins the reviewed static compiler merge
`69a2cb7a60bd12887cc434e47d22cce0e8a0a8c3` (`jtml 0.3.0-dev`), independently of
the recommended published RC.3 download. The existing helper builds an exact
git-archive snapshot and records actual version, source revision and binary hash.
`JTML_BIN` remains an explicit local override; a binary without a checked sidecar
is labeled source-revision-unverified and is never attributed to the pin.

`site-build.json` schema 6 records targets, registry/source hashes, original
compiler inventories, deployed page asset hashes and compiler identity. The
runtime equality gate compares actual bytes from exactly the two browser pages.
A mismatch or static rejection fails with no fallback. Original output is retained
under `<output>.compiler-output/<page-id>/` (one HTML file per static page; four
files per browser page/Counter) and uploaded separately. The public site and
originals are staged together, backed up during publication and restored on
publication failure. Failed compiler outputs are retained separately for diagnosis;
previously accepted originals remain intact.

`site-payloads.json` schema 4 distinguishes cold pages including favicon, unique
page resources, modeled navigation after a cached homepage, and unique deployed
assets. Its deployed subtotal excludes the three self-referential reports and
labels that exclusion. `scripts/measure_site_migration.py` measures complete
before/after deployments including every file and report. Both record deterministic
gzip parameters and zlib version. Estimates are not hosted transfer headers or
cache guarantees. See [static migration measurements and proof](website-six-pages/static-pages.md).
Repeated builds from identical source/compiler/tool inputs and base URL must be
byte-identical; cross-platform identical C++ binaries are not claimed.

## Browser and security acceptance

```sh
python3 tests/test_site_registry.py
export JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT=/absolute/playwright-prefix
export JTML_BENCH_BROWSER=/absolute/chrome-headless-shell
node tests/browser/site.cjs dist/site
# Commit the candidate before the complete final local gate:
JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1 scripts/verify_all.sh
```

The actual published baseline was retrieved from
`https://mrjohnnyrocha.github.io/jtml/`, source `5f970b44e2c4f98595ef156050f093328c632686`,
compiler `325bae128aaaa729d4876159099e63fce9fe59c4`. All 303 deployed files were
checked against its SHA256 manifest. A compact rendering-only fixture stores the
six HTML pages, required scripts/styles/favicon and provenance. Archive and file
hashes are verified before materialization, making all-page visual/DOM acceptance
mandatory and offline in CI. An explicit `JTML_WEBSITE_BASELINE` override must
identify that same published source revision.

Real Chromium visits all six pages at 1440×1000, 900×1100, 390×844 and 320×720
under `/` and `/jtml/`. Every page must retain content, code listings, authored
DOM/accessibility attributes, accessible tree, links and pixel-identical screenshots
against the published baseline. Existing metadata, fragment, MIME, keyboard,
Counter and Reference assertions remain. The four static pages repeat this proof
with JavaScript disabled, including skip-link/subsequent focus and zero JS/API/WS
requests. Cache tests prove only two runtime consumers, one reusable runtime cache
entry and exact script/CSS wire requests during six-page navigation.

Each document has one meta CSP. Static pages retain the compiler's stricter
`script-src 'none'`, `connect-src 'none'`, `object-src 'none'` and `base-uri 'none'`;
reactive pages retain `script-src 'self'` and the existing policy. Independent
inline-script and inline-handler attack controls must be blocked under both.
Unexpected console/CSP/network errors fail. No remote fonts, analytics or framework
is introduced. GitHub Pages meta CSP does not establish header-only protections;
actual hosted HTTPS/MIME/cache behavior still needs separately authorized review.

## Manual publication controls — unchanged and not executed

`.github/workflows/site-pages.yml` creates a read-only PR preview and review
artifacts. It has no push, tag or release deployment trigger. Only explicit
`workflow_dispatch` from main can enter deployment, using a full reviewed source
SHA, clean checkout and main-ancestry checks. Only the deployment job has
`pages: write` and `id-token: write`. Pages configuration uses `enablement: false`
and must already match `https://mrjohnnyrocha.github.io/jtml`. No CNAME is shipped.
No Pages settings, DNS, custom domain, release or publication action is performed
by this website migration. Merge does not publish it.

For a future separately authorized launch:

1. Review/merge the PR and record the full reviewed main SHA.
2. The owner configures Pages to use GitHub Actions, leaves Custom domain empty,
   and sets the `github-pages` environment's main-only/required-reviewer policy.
3. Dispatch `site-pages.yml` from main with `source_revision` set to that SHA.
4. Review the site/browser artifacts and approve the deployment environment.
5. Verify the returned Pages URL on desktop/mobile, release/install links,
   both Counters, HTTPS, MIME, CSP, console and network behavior.

Ownership of `jtml.org` is not assumed. Public availability and production
readiness must not be inferred from a successful local or CI build.
