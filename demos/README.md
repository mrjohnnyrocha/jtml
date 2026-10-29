# JTML showcases

These are language showcases, not complete production applications. See the
[application inventory](../docs/application-inventory.md) for measured results.
The existing SQLite-backed application is [ReleaseDesk](../apps/releasedesk/README.md).
Taskboard lives under `tests/fixtures/production/` as an adversarial regression
fixture, not a second product.

| Source | Role and data | Current boundary |
| --- | --- | --- |
| `index.jtml` | Enterprise Operations language showcase; literal records, `/api/users`, `/api/posts`, `/api/runtime`, POST `/api/login`, `host.notify` | Live/demo contracts; clean browser build currently rejects module ownership. No database or complete backend is supplied here. |
| `opspulse/index.jtml` | Canonical OpsPulse single-file showcase; literal metrics and transient stores | Clean build succeeds with binding advisories; browser route/state acceptance fails. |
| `example.jtml` | Preserved denser OpsPulse reference; `/api/usage`, `/api/users`, `/api/incidents`, lazy `/api/releases` and `/api/media` | No matching production backend; successful compilation is not functional acceptance. |
| `opspulse/pages`, `components`, `stores` | Unfinished modular exploration | Not imported by the canonical entrypoint; see its [module audit](opspulse/README.md). |

Run source checks from the repository root:

```sh
./build/jtml check demos/index.jtml
./build/jtml lint demos/index.jtml
./build/jtml serve demos/index.jtml --port 8030
```

The last command exercises the live backend. To test the separate clean browser
contract, request it explicitly; currently the following reports a known error:

```sh
./build/jtml build demos --target browser --out dist/demos-browser
```

The full gate's existing `build demos --out dist/demos` is a different build mode;
it must not be cited as clean-production/browser proof. Historical media, routes,
guards, fetch and host-integration examples remain for coverage. No showcase is
deleted merely because it overlaps another.
