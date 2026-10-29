# OpsPulse showcase

**Canonical source: `demos/opspulse/index.jtml`.** This is the existing single-file
showcase, not a modular application. It declares `/`, `/usage`, `/people`,
`/incidents`, `/settings` and `*`. It imports none of the sibling modules.
All metrics, user names and operational records are illustrative literals.
There is no API, database or persistence. Refresh and Save settings are inert
buttons; filters do not query real data. Do not use this as a functional product
or production acceptance reference.

From the repository root:

```sh
./build/jtml check demos/opspulse/index.jtml
./build/jtml lint demos/opspulse/index.jtml
./build/jtml build demos/opspulse --target browser --out dist/opspulse
```

Build success does not establish working routes or store-bound inputs. The
[inventory and actual Chromium results](../../docs/application-inventory.md)
record the current failures and reproduction commands. `serve` is a separate
live backend and is not covered by the clean-production acceptance probe.

## Preserved modular exploration

`pages/`, `components/` and `stores/` are an unfinished alternative, **not another
entrypoint**. The directory build compiles `index.jtml`; it does not prove these
modules are reachable or valid. Basic relative imports and explicit exports are
repaired and tested against the real files by `tests/test_application_paths.py`.
Named imports and export filtering exist; the former README claim otherwise was
stale.

The dashboard module now resolves its transitive imports. Seven other pages
still import absent `PageHero`; Usage and Releases also import absent
`ProgressBar`. Settings references absent `appState.workspace/settingsSaved`;
Incidents references absent `appState.selectedIncident/draftNote`. Seven pages
call the two-argument `Shell` with only one argument. Store-member `into` targets
are outside the supported component-local writable-let contract. No missing
state, components, backend or success behavior is fabricated by this cleanup.

Correct path examples:

```jtml
// From pages/dashboard.jtml
use "../stores/app-state.jtml"
use "../components/layouts/shell.jtml"

// From components/layouts/shell.jtml
use "../../stores/app-state.jtml"
use "../ui/app-header.jtml"
```

Releases and Media have no route in the canonical source. They and the SVG asset
are retained because they are distinct explorations, not proven redundant
implementations. `demos/example.jtml` is a separate, denser historical OpsPulse
reference with API-dependent pages; it remains at its existing path.

No source files are deleted: the full gate still consumes the single-file demos
and checks directory asset copying. A future consolidation must first make the
chosen routes, state and slots pass real-browser acceptance without weakening
compiler contracts. Build a functional increment in ReleaseDesk first.
