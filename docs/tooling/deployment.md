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

## Production Notes

- Put a reverse proxy such as nginx or Caddy in front of `jtml serve` for TLS.
- Proxy both HTTP and WebSocket traffic.
- Use `--watch` for local development, not production.
- Use `jtml doctor --json` in CI or image builds to verify the local toolkit shape.
- For static pages, prefer `jtml build <input.jtml|app/> --out dist`; it writes
  `dist/index.html`. Directory builds also copy non-`.jtml` assets such as
  images, CSS, JSON, fonts, and downloads into the same relative paths.
  `jtml transpile <file> -o <output.html>` remains useful for one-off files.
