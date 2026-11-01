# Reference page migration

Baseline: clean main `4dfbcd83304ac380911a23662fc8c8dd72f5d31e`, containing
merged PRs #68 and #69. The existing website compiler pin remains
`325bae128aaaa729d4876159099e63fce9fe59c4` (`jtml 0.3.0-dev`); this page does not
need a newer syntax feature. No compiler/runtime changes or optimizations.

## Authoring and preserved behavior

`site/reference.jtml` owns the complete page: all nine original topics and source
examples, heading/contents/documentation links, release information, and a
component-local `examplesVisible` state with a derived button label and
`toggleExamples` action. The button supports pointer, Enter and Space, announces
expanded state and retains focus; reload restores visible examples. Contents
links use native fragments. CSS supplies presentation only.

`site/shared/layout.jtml` exports the shared navigation and footer used by both
homepage and Reference. The homepage copy, CSS and Counter behavior are preserved.
Local Chromium acceptance additionally compares full-page homepage PNG buffers
against the clean-main build at all six viewport/mount combinations. The other
four HTML pages and inherited stylesheet are byte-identical to baseline output.

The old `reference.html` URL, `#reference-title`, `language-reference.md`,
`docs/reference/language-reference.md` and all existing documentation URLs remain
available. Both `/` and `/jtml/` work. RC.3 remains the recommended experimental
release; no production-readiness claim is added.

## Packaging and provenance

The existing builder compiles both JTML entries independently. Homepage keeps
its root assets. `reference.html` contains the compiled Reference application
markup and loads `reference-assets/jtml-runtime.js`, `components/index.js` and
`app.js` beneath that directory. Packaging changes only document metadata and
those three external script URLs; it does not rewrite application markup or
JavaScript. There is no redirect page, handwritten HTML body, new frontend or
compatibility fallback. The original standalone Counter remains isolated under
`demo/counter/`.

`site-build.json` schema 3 records each page's source, entry/shared-source hashes,
original generated-asset hashes and output asset directory, alongside the existing
compiler binary/source provenance and Counter fields. `manifest.txt` hashes all
deployed files. `site-payloads.json` is generated with every build. The existing
manual-only Pages controls remain unchanged; this PR does not merge or deploy.

## Payload measurement

Bytes, with **raw / gzip** columns. Gzip is Python stdlib, zlib 1.2.12, level 9,
mtime 0, each asset compressed independently. These are reproducible local
measurements, not assertions about Pages response compression or transfer size.
HTML/JS/CSS only; favicon and documentation downloads are excluded.

| Page | HTML | JavaScript | CSS | Total |
| --- | ---: | ---: | ---: | ---: |
| Homepage | 285,599 / 26,634 | 566,534 / 69,162 | 9,214 / 2,446 | 861,347 / 98,242 |
| Reference | 287,629 / 24,473 | 611,779 / 73,745 | 12,084 / 3,370 | 911,492 / 101,588 |

Full per-file measurements and accounting are in [payloads.json](payloads.json).
The clean-main homepage was 836,123 / 97,169 bytes in total. Extracting the shared
layout adds 25,224 raw / 1,073 gzip bytes despite identical rendered pixels. The
old handwritten Reference was 17,565 / 5,133 bytes (HTML and inherited CSS, no JS).
The JTML migration therefore has a substantial generated-payload cost; it is not
represented as a performance improvement.

Redundant generated data is measured, not stripped:

- Homepage's inline client manifest: 251,566 raw / 18,426 gzip bytes; 10 bodyPlan
  array occurrences, with 35,319 canonical JSON bytes of exact repeats after
  their first occurrences.
- Reference's inline client manifest: 252,692 raw / 16,859 gzip bytes; 8 bodyPlan
  array occurrences, with 84,981 canonical JSON bytes of exact repeats.
- The two page bundles carry identical runtime files: each is 272,370 raw /
  50,835 gzip bytes. Independent URLs preserve isolation but do not share the
  browser cache entry. The pre-existing standalone Counter is outside this
  two-page measurement.
- Initial markup, compiler templates, client definitions and static component
  creation code also encode overlapping UI information. Only the exact repeated
  bodyPlan arrays are counted above; this is not an estimate of safely removable
  bytes or compressed savings. Runtime ownership/use must be demonstrated before
  any separate compiler optimization.

## Acceptance and reproduction

```sh
scripts/build_site.sh dist/site
export JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT=/absolute/playwright-prefix
export JTML_BENCH_BROWSER=/absolute/chrome-headless-shell
node tests/browser/site.cjs dist/site
# Optional clean-main site built before editing; verifies pixel/unchanged-page preservation:
node tests/browser/site.cjs dist/site /absolute/clean-main-site
# Include that preservation check in the required complete final gate:
JTML_WEBSITE_BASELINE=/absolute/clean-main-site \
  JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1 scripts/verify_all.sh
```

Real Chromium is required at 1440×1000, 390×844 and 320×720 under `/` and `/jtml/`.
The suite checks all six public pages and their navigation/assets, isolated page
script requests, generated-JS hashes and source provenance, nine literal source
snippets, topic navigation, keyboard skip/focus, show/hide/reload behavior, both
Counters, no overflow, strict script CSP with independent attack controls, and no
unexpected console/network/API/WebSocket requests. Original snippets are an
independent fixture extracted from clean main's handwritten page.

The PR records exact final focused/full-gate and hosted results separately; CI
and a successful local gate are distinct evidence. Screenshots are emitted as
`dist/website-reference-*.png` and included in the existing CI screenshot artifact.
[Desktop](desktop.png) and [mobile](mobile.png) captures are committed for review.

## Bounded language/tooling limits

No language limitation prevents clean compilation. Existing root-ID limitations
are avoided with static component composition, and skip navigation targets a
native focusable link because component `tabindex` remains incomplete. Unicode
brace escapes keep the CSS example literal rather than an interpolation.

The pinned linter reports two false `into` advisories inside the inert quoted
media code example (`selected` / `sceneState`); source check and clean compilation
pass. The readable original snippet is retained, no warning is suppressed, and
browser tests require its exact text. No fetch, interop or media operation runs:
those are reference examples, not application declarations.

The next page migration should move Tools or Examples to JTML using the shared
layout and the same isolated assets, provenance and browser checks.

Screenshot source SHA256:

- `site/reference.jtml`: `9ef00d29149a43282661548bef767c9862969b1c627994ad836167db11797ce5`
- `site/shared/layout.jtml`: `b5b0c8374c67d80f7f572c0f4609fa226f64d4be4649f0a99ae2d06a90db4f45`
