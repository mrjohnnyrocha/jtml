# Public website launch: build, verify, then activate manually

The public product is the [v0.3.0-rc.2 experimental prerelease](https://github.com/mrjohnnyrocha/jtml/releases/tag/v0.3.0-rc.2),
not an enterprise/production platform. This website slice changes the static site,
its build/tests and deployment preparation only. No compiler/runtime or release
artifact is changed, and no backend service is introduced.

## Actual demo and reproducibility

`scripts/build_site.sh` invokes `scripts/build_public_site.py`. It copies the
complete unchanged `examples/friendly_counter.jtml` to a temporary `counter.jtml`
and calls the selected CLI's actual `build --target browser`. There is no compat
fallback or handwritten Counter implementation. All three generated JavaScript
assets are copied unchanged; only CSP/canonical metadata is added to generated
index.html. The four-file demo lives in `demo/counter/`; the exact source is
`counter.jtml` at the site root and is also injected as text into the JTML code tab.
HTML/React tabs are explicitly illustrative snippets, never the demo frontend.

The Pages workflow downloads the published Ubuntu RC.2 compiler bundle and checks
its approved SHA256 `6c3e21a53bf7729b824c27e73d1ce35ffe69182f98dd8c95f4969084c53e1d6b`.
It checks the actual CLI version before compiling. No fresh JTML build or new
release is needed. Local source gates may use their own just-built CLI through
`JTML_BIN`; `site-build.json` records the actual compiler version, source revision,
dirty status, source hash and original generated-asset hashes honestly.

Relative URLs work at both a domain root and `/jtml/`. `SITE_BASE_URL` supplies
canonical/robots/sitemap URLs and defaults to `https://mrjohnnyrocha.github.io/jtml/`.
The Actions workflow deploys the prepared static artifact directly, without a
Jekyll build. The output uses real `.html`/`.js`/`.css` extensions and no CNAME.
`manifest.txt` is a sorted SHA256 list of all other site files. No build timestamp,
absolute build path or output-directory path enters the public provenance file.

```sh
# Use an already verified RC.2 CLI or your existing local source build.
JTML_BIN=/absolute/path/to/release/bin/jtml scripts/build_site.sh dist/site
# Serve only for local preview, never as a production backend:
python3 -m http.server 8000 --bind 127.0.0.1 --directory dist/site
```

Configure actual installed Playwright/Chromium paths, then:

```sh
export JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT=/absolute/path/to/playwright-prefix
export JTML_BENCH_BROWSER=/absolute/path/to/chrome-headless-shell
node tests/browser/site.cjs dist/site
```

The mandatory real-browser test checks all six HTML pages and their local links,
fragments/assets, root/project paths, JavaScript MIME, desktop/mobile overflow,
visible mobile navigation, keyboard snippet tabs, Counter click/Enter/Reset/reload,
iframe/standalone behavior and the formal clean-production contract. Independent
inline-script/handler negatives must be blocked. No unexpected console/CSP errors,
external runtime requests, WebSockets or API/backend calls are allowed.

A repeated build from the same source/compiler/base URL is byte-identical. Counter
production output was also compared from independent temporary source paths.
Changing the compiler/source/base URL intentionally changes provenance/output.
Screenshots are emitted beside the site output as `website-*.png` for visual review.
The existing local gate and both CI jobs require this browser check after site build.

## CSP and MIME boundary

GitHub Pages does not provide this project with configurable response headers.
Every HTML document therefore declares its own meta CSP before scripts:
`script-src 'self'`, no eval or executable inline handlers, no API connections.
The Friendly example uses inline styles, so style-src permits them separately;
script-src does not use unsafe-inline. The iframe uses same-origin static assets.
No remote font, analytics or framework is loaded at runtime.

[Meta CSP does not support every header CSP feature](https://developer.mozilla.org/en-US/docs/Web/HTTP/Guides/CSP#csp_overview):
this is not a claim of frame-ancestors/report-only/header-level protection. The
browser test serves normal HTML/JavaScript MIME types and validates actual script
responses; production Pages headers/HTTPS must be checked after manual activation.
No `_headers` file is presented as an effective GitHub Pages header configuration.

## Domain and deployment controls

The removed `site/CNAME` contained `jtml.org`; the old builder also hardcoded that
host in robots/sitemap. Neither file proved ownership. No domain/DNS setting was
changed. The Pages API returned 404 during inspection on 2026-10-26, so a configured
or live Pages site is not assumed. The supported launch path is the github.io
project URL. A custom domain requires a separate ownership/DNS/configuration review.
[Actions deployment uses repository Pages settings, not a CNAME file](https://docs.github.com/en/pages/configuring-a-custom-domain-for-your-github-pages-site/troubleshooting-custom-domains-and-github-pages).

`.github/workflows/site-pages.yml` runs a read-only build/browser preview on PRs
and uploads website-preview and website-browser review artifacts (including
desktop/mobile screenshots). There is no push/release/tag deployment trigger.
Only `workflow_dispatch` from main can enter the deployment job, after a full
40-character source SHA/clean checkout/main ancestry check and a successful build.
Only that job has pages/id-token write permissions and uses the github-pages
environment. It reads existing Pages configuration and rejects any URL other than
the reviewed github.io fallback. No setting is automatically enabled or changed.

## Exact manual activation steps — not executed by this PR

1. Review/merge this PR manually and record its reviewed merge SHA.
2. Open repository **Settings → Pages**. Select **GitHub Actions** as the publishing
   source. Leave **Custom domain empty**; do not add jtml.org. Enable HTTPS when
   available. These settings require a separate explicit owner action.
3. In **Settings → Environments → github-pages**, restrict deployment branches to
   main and configure the owner as required reviewer where available. The workflow
   does not create a reviewer policy on your behalf.
4. Run the existing prepared workflow manually from main, pinned to the reviewed
   website SHA (not the older compiler release SHA):

```sh
gh workflow run site-pages.yml --ref main \
  -f source_revision=<FULL_REVIEWED_WEBSITE_MERGE_SHA>
```

5. Review the preview/browser result and approve the github-pages environment
   deployment when prompted. The deploy job consumes the already tested Pages
   artifact and verifies the configured fallback URL.
6. Open `https://mrjohnnyrocha.github.io/jtml/`. On desktop and phone, follow every
   navigation item; click Add one/Reset in the embedded and standalone Counter;
   verify release/install links. Inspect response MIME for demo JavaScript and
   confirm no console/CSP errors or unexpected network/API/WebSocket calls.
7. Read back Pages status and the deployment's page URL. If path, MIME, policy or
   interaction differs from local/CI evidence, stop promotion and report it rather
   than changing compiler behavior or weakening CSP. Do not announce a working
   public site before these post-deployment checks pass.

The official [custom-workflow requirements](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages)
cover configure/upload/deploy actions and permissions; [publishing-source setup](https://docs.github.com/en/pages/getting-started-with-github-pages/configuring-a-publishing-source-for-your-github-pages-site)
is an owner setting. The website is not automatically deployed by merge.

## Verification and launch verdict

Focused local browser acceptance passed at 1440×1000 and 390×844 for both `/` and
`/jtml/`, including independent CSP negatives, all navigation/assets and genuine
Counter interactions. Desktop/mobile screenshots were inspected; hero contrast
and visible mobile navigation were corrected without introducing a second frontend.
The compiler's Counter runtime remains **271961/272000 bytes**. Known native
renderer/HTML-size/tooling/platform/signing limits remain visible on the site.

The complete local gate passed once with exit 0: 476/476 CTests, all nine mandatory
benchmarks with clean browser exit, unchanged Counter/CSP/ReleaseDesk/Taskboard/
provenance/lifecycle/security checks, the new site test, package/install/LSP/editor
checks and live smoke 8/8. No verification check was skipped. Two complete site
builds (140 files) were byte-identical. YAML/Bash/JavaScript/Python syntax and
manual-only deployment/permissions/no-enablement guards passed. Compiler/runtime,
accepted example, vendor code, budgets and release workflows are unchanged.
Exact-head hosted CI/preview results are recorded separately in the PR; local and
hosted-preview evidence are distinct from live Pages evidence.
**GO for review and manual activation once checks pass.** Actual Pages source,
environment approval, HTTPS/MIME and live browser confirmation remain pending.
This is not a claim that the website is already deployed or that jtml.org is owned.
