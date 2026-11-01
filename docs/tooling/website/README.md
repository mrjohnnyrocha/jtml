# Homepage visual review

Real Chromium 151.0.7922.34 screenshots from the default source-pinned site build,
served locally under `/jtml/`. These show the complete page at 1440×1000 and
390×844 viewports. The same test also covers 320×720 and the domain-root mount.

- [Desktop, full page](desktop.png)
- [Mobile, full page](mobile.png)

Homepage source SHA256:
`b6ece66b34d2a91a58a6869fc426a60a6e013b943760d19ba92dd691005758d7`.
Compiler source: `325bae128aaaa729d4876159099e63fce9fe59c4`, `jtml 0.3.0-dev`.
Regenerate with the commands in [website-launch.md](../website-launch.md).
The browser suite emits all viewport/path screenshots into `dist/`; CI retains
those as review artifacts. These images demonstrate local rendering, not a live
Pages deployment or production readiness.
