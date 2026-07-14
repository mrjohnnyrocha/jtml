# JTML Deployment Guide

JTML has two deployment modes.

## Static HTML

Use this when the page does not need server-backed events after load.

```sh
jtml transpile examples/html_syntax.jtml -o dist/index.html
```

The generated file is plain HTML with a tiny runtime script. It can be hosted on GitHub Pages, Netlify, Vercel static hosting, Cloudflare Pages, nginx, or any static file host.

## Live Runtime

Use this when event handlers should call JTML functions and update reactive bindings.

```sh
jtml demo --port 8000
```

`jtml serve` starts:

- an HTTP server on the selected `--port`
- a WebSocket server on `--port + 80`
- an HTTP fallback endpoint at `/api/event`
- JSON inspection endpoints at `/api/health`, `/api/bindings`, `/api/state`,
  and `/api/runtime`

The generated browser runtime tries WebSocket first. If WebSocket is unavailable, event dispatch falls back to `POST /api/event`.
External tools can use the stable runtime contract in
[`runtime-http-contract.md`](runtime-http-contract.md).

## Install Locally

```sh
cmake -S . -B build -DJTML_BUILD_PYTHON=OFF
cmake --build build --target jtml_cli
cmake --install build --prefix ~/.local
```

Make sure `~/.local/bin` is on your `PATH`, then run:

```sh
jtml --version
jtml tutorial --port 8000
```

## Package

The CMake project includes CPack metadata for a `.tar.gz` CLI package:

```sh
cmake -S . -B build -DJTML_BUILD_PYTHON=OFF
cmake --build build --target jtml_cli
cd build
cpack
```

## Predeploy Artifacts

Build the static website artifact locally:

```sh
scripts/build_site.sh dist/site
```

Build a local CLI release bundle with the executable, docs, examples, tutorial,
editor files, and site source:

```sh
scripts/package_cli.sh dist/release
```

Run the full predeploy verification without publishing anything:

```sh
scripts/verify_all.sh
```

The release bundle includes:

- `MANIFEST.txt` with every packaged file
- `SHA256SUMS` for release verification
- docs, examples, tutorial lessons, editor assets, and the predeploy website
- `dist/doctor.json` from `jtml doctor --json`

## Hosting jtml.org

The static site lives in `site/`.

For GitHub Pages:

1. Publish the generated `dist/site/` folder.
2. Keep `site/CNAME` set to `jtml.org`.
3. Point the domain's DNS at the hosting provider.

For another static host, deploy the contents of `site/` and configure the custom domain in that host's dashboard.

## Browser Production Builds

Use this for deployable browser apps:

```sh
jtml build app.jtml --target browser --out dist
```

This is a clean production build by default. It writes:

- `dist/index.html`
- `dist/jtml-runtime.js`
- `dist/components/index.js`
- `dist/app.js`

Clean production output does not emit `jtml-update-plans.js`, does not expose
`compatibilityUpdatePlans`, does not enable synthetic imported-component stubs,
and refuses component shapes that still need the legacy compatibility/update-plan
fallback. If a build fails for that reason, the error should name the component,
source location when known, unsupported shape, and the migration command:

```sh
jtml build app.jtml --target browser --compat --out dist
```

Use `--compat` only as a temporary migration bridge. It emits the legacy
`dist/jtml-update-plans.js` asset for older/debug tooling.

Use `--debug` when you need source-rich compatibility metadata and fallback
telemetry while investigating compiler/runtime behavior:

```sh
jtml build app.jtml --target browser --debug --out dist-debug
```

`dist/app.js` exposes the formal browser build contract at
`window.jtml.browserBuild`:

- `version`
- `mode`: `prod`, `compat`, or `debug`
- `cleanProduction`
- `compatibility`
- `debug`
- `assets`
- `diagnostics.sourceSpans`
- `diagnostics.fallbackTelemetry`

`window.jtml.productionMode` remains as a compatibility alias for older local
tools; new integrations should read `window.jtml.browserBuild.mode`.

Every browser build validates its emitted asset schema before returning
success. Production validates the absence of legacy update-plan references and
source-rich component debug payloads. Compat/debug validate the explicit legacy
asset and mode flags. All modes reject shipped browser assets containing
runtime dynamic compilation (`new Function` / direct `eval`).

## Browser Assets

Directory builds copy a conservative public asset allowlist:

- files under `public/`
- files under `assets/`
- root web metadata files such as `favicon.ico`, `robots.txt`, `sitemap.xml`,
  `manifest.json`, `site.webmanifest`, `browserconfig.xml`, and `humans.txt`

Project-private files are not copied by default. That includes hidden paths,
`.env*`, `.jtml` sources, `node_modules`, `dist`, `build`, `target`,
`jtml_modules`, lockfiles, nested README files, and arbitrary nested folders
outside `public/` or `assets/`.

Symlinks are skipped intentionally. Put deployable assets in `public/` or
`assets/` directly until configurable include/exclude rules ship.

`jtml transpile <file> -o <output.html>` remains useful for one-off inline
browser artifacts and previews, but production asset-mode flags are only
supported by `jtml build --target browser`.

## Browser Runtime Benchmarks

`scripts/benchmark_runtime.sh` always runs static asset and
compatibility-shape checks. It also calls
`scripts/benchmark_browser_runtime.sh` when a Chrome/Chromium-family browser is
discoverable. Headless browser runs can be blocked by local sandboxing, GUI
permissions, or missing browser dependencies; in that case the browser portion
may skip in local developer environments.

For release CI or any environment where Chrome/Chromium is expected to work,
make the browser benchmark mandatory:

```sh
JTML_REQUIRE_BROWSER_BENCH=1 scripts/benchmark_runtime.sh
```

Set `JTML_BENCH_BROWSER=/absolute/path/to/chrome` to choose a specific browser
binary. A v0.2 release candidate should pass with
`JTML_REQUIRE_BROWSER_BENCH=1` in at least one clean CI/browser environment.

## Production Notes

- Put a reverse proxy such as nginx or Caddy in front of `jtml serve` for TLS.
- Proxy both HTTP and WebSocket traffic.
- Use `--watch` for local development, not production.
- Use `jtml doctor --json` in CI or image builds to verify the local toolkit shape.
