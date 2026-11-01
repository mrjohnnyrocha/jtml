# Public website: JTML source, static output, manual publication

The homepage is authored in `site/index.jtml`, canonical Friendly JTML 2.
Reference is now authored in `site/reference.jtml`; see the
[Reference migration, payload measurements and acceptance](website-reference/README.md).
Its `Homepage`, `SiteNav`, `Principle` and `Counter` components own all visible
structure and composition. Counter owns local `let count`, `get doubled`, and
`when add`/`when reset` actions. Native links provide navigation; native buttons
support pointer, Enter and Space. There is no handwritten application JavaScript,
HTML frontend, framework, raw HTML escape hatch or compatibility fallback.

`site/home.css`, `site/reference.css` and `site/favicon.svg` are handwritten presentation assets. The four remaining HTML pages and their
`site/styles.css` remain inherited HTML/CSS. Their content, documentation links,
RC.3 download recommendation, and the complete unchanged
`examples/friendly_counter.jtml` standalone demo are preserved. The hero's compact
source is an illustrative working program, not an exact listing of the styled
homepage component. Its complete source is linked on the page.

## Reproducible build and provenance

```sh
# Builds the exact compiler snapshot, then compiles both homepage and Counter.
scripts/build_site.sh dist/site
# Local preview only:
python3 -m http.server 8000 --bind 127.0.0.1 --directory dist/site
```

`site/compiler.json` pins compiler source to
`325bae128aaaa729d4876159099e63fce9fe59c4` in `mrjohnnyrocha/jtml`, reporting
`jtml 0.3.0-dev`. This is a development compiler snapshot, separate from the
published experimental RC.3 download. The old RC.2 Pages binary is no longer
used to build the website. No release artifact is changed or republished.

`scripts/build_site_compiler.sh` uses `git archive` of that full commit, never
working-tree compiler sources, then CMake/C++17 with Python/tests off and the
`dev` suffix. It writes a sidecar containing source repository/revision, actual
version, binary SHA256 and build method. Dependency archives retain the hash pins
in that compiler snapshot's CMake configuration. A C++ toolchain, CMake, Python 3,
and network access or populated dependency caches are required.

`JTML_BIN=/absolute/path/to/jtml scripts/build_site.sh dist/site` remains an
explicit local override, including for the repository gate. An override without
the checked sidecar is labeled **source revision unverified**, with its actual
version and binary hash; it is never attributed to the pinned source implicitly.

`scripts/build_public_site.py` compiles a byte-for-byte temporary copy of the
homepage, Reference and original Counter with `build --target browser`. Each build must
produce exactly `index.html`, `jtml-runtime.js`, `components/index.js`, and `app.js`.
The homepage assets occupy the output root; the independent original Counter
occupies `demo/counter/`. The old handwritten `site/app.js` has been removed, so it
cannot collide with the compiler-owned root `app.js`. Generated JavaScript and
application markup remain unchanged. Reference uses `reference-assets/` with only
its external script URLs rebased in `reference.html`. Packaging adds document head/language metadata:
CSP, canonical URL, title, description, viewport, local favicon and the homepage stylesheet.

`site-build.json` schema 3 records per-page entry/shared inputs and isolated assets, in addition to source revision/dirty status, homepage and
Counter source hashes, original generated-asset hashes and compiler provenance.
`manifest.txt` hashes all deployed files. No build time or absolute build path is
added to public provenance. Repeated builds from identical source/compiler/base
URL must be byte-identical. Compiler reproducibility here means pinned inputs and
recorded binary identity, not a cross-platform bit-identical C++ binary claim.
The CLI package continues to include the website **source**, now `index.jtml`.

## Browser and security acceptance

```sh
export JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT=/absolute/path/to/playwright-prefix
export JTML_BENCH_BROWSER=/absolute/path/to/chrome-headless-shell
node tests/browser/site.cjs dist/site
scripts/verify_all.sh
```

Real Chromium is mandatory. The suite visits all six HTML pages at `/` and
`/jtml/`, at 1440×1000, 390×844 and 320×720. It checks local navigation, fragment
targets and assets, JavaScript MIME, mobile overflow and visible navigation,
keyboard skip navigation, both genuine Counters, derived updates, reset/reload,
and the formal clean-production mode. Unexpected console/CSP errors, external
runtime requests, API calls and WebSockets fail. Independent inline-script and
inline-handler attacks must remain blocked. Screenshots are saved beside the
output as `website-*.png`; CI uploads both the site and screenshots for review.
External release/GitHub links retain their verified published targets; the suite
does not depend on external network availability to validate local navigation.

All HTML documents retain meta CSP before scripts: `script-src 'self'`, no eval,
no executable inline handlers, `connect-src 'none'`. Inline styles are permitted
separately, as required by the unchanged original Counter. No remote fonts,
analytics or framework are loaded. GitHub Pages does not supply configurable
response headers here; meta CSP does not establish header-only protections such
as `frame-ancestors`. Actual hosted HTTPS/MIME still needs post-publication review.

## Compiler boundaries encountered

- Root explicit IDs are emitted beside generated IDs, breaking fragment targets.
  The homepage uses the supported static component path, which retains authored
  IDs. No compiler or runtime code is changed.
- Static component attributes omit `tabindex`. Skip navigation targets the first
  native, focusable content link instead. Tests require actual keyboard focus.
- Friendly event modifiers consume the rest of a button line. Presentation
  modifiers precede `click`, so button styles remain authored and generated.
- The linter interpreted quoted prose containing `into generated` as an into
  binding advisory. The copy was rephrased; no diagnostic was suppressed.
- Document metadata has no site-level authoring contract in this slice. The
  existing packager adds head metadata; it does not construct body markup.

Clean mode also retains the compiler’s redundant client-manifest payload in
HTML: this homepage is approximately 282 KB of HTML plus 271 KB of component
JavaScript and a 273 KB shared runtime, before transfer compression. Asset
minimization belongs in a separately demonstrated compiler change; this site
ships compiler output intact. The existing reference HTML microfixture overage
remains a known warning in the full gate.

These are bounded site authoring constraints, not evidence of general frontend
or production readiness. A separate compiler issue can address authored root IDs
and component attribute completeness with independent regressions.

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

## Next migration

Migrate Tools or Examples using the shared JTML navigation/footer and
this same compiler/assets pipeline. Reference is already migrated. Preserve documentation destinations and
keyboard/mobile acceptance; keep examples, tools, security and deployment pages
for subsequent focused migrations. Avoid a second frontend or deployment system.

[Desktop/mobile screenshots and source identity](website/README.md) are committed
for review. CI also retains every tested viewport/mount screenshot as an artifact.
