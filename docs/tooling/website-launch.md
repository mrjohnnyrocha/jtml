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

Every entry compiles with `build --target browser` to exactly `index.html`,
`jtml-runtime.js`, `components/index.js` and `app.js`. The generated HTML is
published at its registered `.html` URL. All six pages load `./jtml-runtime.js`; homepage components/bootstrap stay at the
root and other pages retain their own `<name>-assets/` components/bootstrap. The original unchanged
Counter remains under `demo/counter/`. Generated application markup and JavaScript
are unchanged; packaging adds head/language metadata and rebases external script
URLs. There is no redirect shell, compatibility fallback or second frontend.

`site/compiler.json` retains compiler source pin
`325bae128aaaa729d4876159099e63fce9fe59c4` (`jtml 0.3.0-dev`), independently of
the recommended published RC.3 experimental download. The existing helper builds
a git-archive snapshot with CMake/C++17 and records the actual binary SHA256.
Dependencies retain their source/hash pins. `JTML_BIN` remains a local override;
without a checked sidecar its source revision is explicitly unverified.

`site-build.json` schema 5 records the registry hash, source revision/dirty status,
per-page entry/shared hashes, original generated assets and compiler provenance.
`manifest.txt` hashes deployed files. `site-payloads.json` schema 3 measures all six
pages, with raw/gzip HTML/JS/CSS sizes and redundant runtime/manifest observations.
Compression parameters and zlib version are recorded. The registry (schema 2)
explicitly declares the shared root runtime. All six compiled runtime files must
compare byte-for-byte equal before any public output is replaced. A mismatch
fails the build; no fallback or partial sharing is accepted. Original compiler
HTML and JavaScript are retained beside the site in `<output>.compiler-output/`,
including the independent Counter, and uploaded as a separate review artifact.
These originals are outside the deployed site. Original per-page hashes remain
in provenance; the shared runtime records its hash and all six checked pages.
See [runtime sharing measurements and limits](website-six-pages/runtime-sharing.md). No compiler optimization,
transfer-header guarantee or performance improvement is claimed. Repeated builds
from identical source/compiler/tool inputs and base URL must be byte-identical;
cross-platform bit-identical compiler binaries are not claimed.

## Browser and security acceptance

```sh
python3 tests/test_site_registry.py
export JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT=/absolute/playwright-prefix
export JTML_BENCH_BROWSER=/absolute/chrome-headless-shell
node tests/browser/site.cjs dist/site /absolute/clean-main-site
# Commit the final candidate before the one complete final gate:
JTML_WEBSITE_BASELINE=/absolute/clean-main-site \
  JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1 scripts/verify_all.sh
```

Real Chromium is required at 1440×1000, 900×1100, 390×844 and 320×720 under `/`
and `/jtml/`. All six pages are checked for preserved content/listings/links,
headings and metadata, exact bundle requests and provenance, keyboard navigation,
existing Counter/Reference behavior, fragments/assets/MIME, CSP enforcement and
no page-wide overflow. Unexpected console/CSP/network/API/WebSocket errors fail.
With the clean-main baseline supplied, homepage and Reference screenshots must
remain pixel-identical. CI uploads site and all desktop/tablet/mobile captures.

All documents retain meta CSP before scripts: `script-src 'self'`, no eval,
no executable inline handlers, `connect-src 'none'`. Inline styles are permitted
separately for the unchanged Counter. No remote fonts, analytics or framework
loads at runtime. GitHub Pages does not provide configurable response headers
here; meta CSP is not a claim of header-only protections such as frame-ancestors.
Actual hosted HTTPS/MIME still needs separately authorized post-publication review.

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
