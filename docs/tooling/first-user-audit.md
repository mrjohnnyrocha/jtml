# First-user onboarding audit — 2026-10-27/28

## Scope and verdict

Original baseline: main after PR #51,
`d2b31c2e33b0a2f613a3b354d9b503897dc981b6`. Resumed baseline: main
`1e65fb9425eb36474fd885afd6732e34141134ed`, the verified merge of
[PR #53](https://github.com/mrjohnnyrocha/jtml/pull/53), whose implementation head
is `85b64cef73235523d54e14a4faabdcce53c751fd`.

The published Counter works on both cohorts. Current main also runs the generated
modular starter and the complete single-file CoffeeOrder guide. RC.2 still leaves
the old customer name visible after Clear and cannot clean-build the modular
starter, even after its missing export is corrected. Build success alone does
not establish correct browser behavior.

The bounded first-app flow is technically completable without author help on
current main. This is an experienced tool user's audit, not evidence that three
independent external developers have completed it. The
[first-app guide](../getting-started.md) provides exact commands, complete source,
seven browser checks and the independent-developer handoff protocol.

**GO for merging this focused onboarding PR**, subject to its own hosted CI.
Both first-app flows pass locally; no remaining blocker was found within those
flows. Independent external-user success remains unproven.
This branch changes documentation, website source and onboarding regression
coverage. The generated export and production compiler correction already belong
to main; this PR changes no compiler semantics or runtime.

## Environment and preserved evidence

Physical macOS 14.4.1 arm64, Apple Clang 15.0.0, CMake 3.31.3, Python 3.12.3;
browser verification uses Chromium 153.0.8010.12 and Playwright 1.63.0. The browser
tools are audit dependencies, not application build prerequisites. Other-platform
claims are not inferred from this host. Curl downloads do not reproduce Finder
quarantine or prove acceptance by every macOS security policy.

All original evidence remains under `/tmp/jtml-first-user.d6M2fw`:

- `evidence/`: initial source/RC.2 builds, browser traces/screenshots, website checks.
- `resume-2026-10-28/`: the previous failed gate and corrected installation/PATH tests.
- `post-pr53-resume/`: this integration, rebuild, browser tests and complete gate.

Before integration, all 12 uncommitted onboarding files were copied byte-for-byte
and hashed under `post-pr53-resume/preserved/`; `onboarding.patch` and the retained
Git stash provide additional recovery copies. Eleven files restored byte-identically.
The verification-script conflict was resolved by retaining both imported-component
and first-user browser suites. The Card template correction was identical to main.
No historical evidence was removed; large logs, binaries and traces stay outside Git.

Both clean-start runs use `env -i`, an explicit tools PATH without a preinstalled
JTML, disabled user Git configuration and new clone-local build/dependency
folders. This is a clean checkout/environment on a machine with prerequisites
installed, not a newly provisioned operating system. The resumed source run
records its exact HEAD and downloads its own pinned dependencies.

Published RC.2 identities remain separate:

- Source: `13c53ab475c9e82ac4be2b232f744d664e926dea`.
- macOS archive SHA256: `225b0f5e76251e1b70ef9d2fd450a028e2854edbfc3a5546891f5d4332dd94f9`.
- CLI SHA256: `b96cadca3a0ff010ddcebf931eba60822a89ee40da7537be2024ac59417f17b9`.

## Confirmed friction, ranked by impact

| Impact | Original observation | Resolution / boundary |
| --- | --- | --- |
| High | RC.2 CoffeeOrder compiles/lints, but Clear leaves `Ada` in the textbox. | Documented unreleased input synchronization fix on main; RC.2 is not represented as repaired. |
| High | `jtml new app modular` generates a private Card, so named import checking fails. | PR #53 exports Card on main. RC.2 instructions distinguish the manual export correction from production support. |
| High | After adding the export, clean browser build rejects imported Card; the existing module example also fails. | PR #53 fixes production module planning on main. The unchanged strict onboarding regression now requires a real browser pass, without compatibility output. |
| Medium | First language-reference example uses undeclared `doubled`. | Adds `get doubled = count * 2`; regression extracts and checks/lints the actual snippet. |
| Medium | README assumes `jtml` remains discoverable after `cd my-app`, although source build installs no global command. | Explicit absolute build-directory PATH, prefix-install PATH and consistent `./build/jtml` guide commands. |
| Medium | Binary instructions describe pending publication and workflow-review downloads. | Public release URL, anonymous asset commands and clear historical-preparation context. |
| Medium | Checking the entire checksum manifest after downloading one archive reports three missing archives. | Select and check the downloaded archive's checksum. |
| Low | Single-file generator emits Classic while Friendly is canonical. | Labels that generator as compatibility tooling; guide supplies complete Friendly source. |
| Low | New-user documentation leads with contributor architecture. | README and docs index lead to the first-app guide. |

## Reproduce the walkthroughs

Follow [the source build, CoffeeOrder source and browser checklist](../getting-started.md)
in a new directory. For the generated starter, use a compiler from current main:

```sh
# From the repository root after building:
export PATH="$PWD/build:$PATH"
./build/jtml new app my-app
cd my-app
jtml check index.jtml
jtml lint index.jtml
jtml build . --target browser --out dist
python3 -m http.server 8000 --bind 127.0.0.1 --directory dist
```

Open localhost:8000. Both Card headings must render; Add one changes Count: 0 to
Count: 1. Keep all four clean assets together. The optional
[prefix installation](deployment.md#install-locally-from-source) uses a temporary
prefix in the audit rather than replacing the user's installation.

For published binaries, follow [RC.2 installation](../releases/v0.3.0-rc.2.md#install-and-five-minute-first-app-smoke).
The exact downloaded compiler is used unchanged. Counter Add one/Reset/reload
passes; CoffeeOrder's retained-text failure is recorded separately. This audit
does not claim an interactive Studio walkthrough beyond distribution checks.

The initial public-site audit tested Home, Tools, Examples, Reference, Security
and Deploy at `https://mrjohnnyrocha.github.io/jtml/`: all six pages and tested
same-site navigation/assets returned 200, and the embedded Counter passed Add one
and Reset. This is a dated bounded audit, not a crawl of every URL. The revised
site is built/tested locally; no deployment is performed.

## Automated verification

```sh
# Set these to a Playwright installation and actual Chromium executable:
export JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT=/absolute/path/to/playwright-prefix
export JTML_BENCH_BROWSER=/absolute/path/to/chrome-headless-shell
JTML_BIN="$PWD/build/jtml" node tests/browser/first_user.cjs
JTML_BROWSER_BENCH_REQUIRE_CLEAN_EXIT=1 bash scripts/verify_all.sh
```

The first-user suite extracts the actual guide/reference source; requires check,
lint and clean production compilation; asserts the generated Card export/import;
and drives the modular starter and CoffeeOrder through real Chromium actions
under `script-src 'self'`. It checks validation, focus, Enter without navigation,
derived totals, saved receipt, visible input reset and reload. Missing browser
tools fail instead of skipping. No compatibility assets, eval or new Function are
accepted. Screenshots, browser trace and command/result JSON are preserved for
each unique attempt. The suite is required in both the full local gate and
Ubuntu/macOS PR CI; CI uploads first-user browser evidence.

Historical failures remain evidence: the original baseline regression fails at
missing-export checking. The prior patched regression passes check/lint then
fails production import planning. That previous full gate exited 1 after
477/477 CTests, nine clean-exit benchmarks and the initial browser suites; later
sections were not reached. Those logs remain in `resume-2026-10-28/`. PR #53
addresses the compiler blocker; this branch retains the strict requirements.

### Resumed final results

The complete gate **passes**, with **480/480 CTests**, all **nine performance
benchmarks with clean browser exits**, the clean-exit production reference,
imported-component and first-user suites, CSP/input/lifecycle/security/conformance
checks, editor/site/LSP/package relocation/install checks and live smoke **8/8**.
No verification check was skipped and no requirement or budget was reduced.
The five known native-renderer discrepancies and reference HTML overage remain
explicit observations, not newly classified passes. Python bindings are disabled;
no new sanitizer run is claimed.

The first-user suite passes against both the rebuilt worktree compiler and the
new minimal-environment public clone of the resumed baseline. Its two production
apps render and interact in Chromium. The documented reference example checks
and lints. The fresh source/PATH commands and anonymous RC.2 download/checksum
commands pass. The unchanged RC.2 compiler separately reproduces the expected
CoffeeOrder reset and modular-production failures; its Counter passes.

[The compact verification record](first-user-audit-verification.json) records
source/evidence hashes, executed compiler identities and exact commands. The
raw full-gate log is `post-pr53-resume/full-gate.log`, and fresh-build/browser
records are alongside it. Checksums identify preserved bytes; real build and
browser executions corroborate them. Hosted Ubuntu/macOS CI runs separately on
the PR commit; consult the PR checks rather than inferring CI success from this
local run. The new first-user suite and its browser artifacts are required there.

## Remaining boundaries

General slot behavior is incomplete. Conditional slots can lose supplied content;
slotted child components can remain uninitialized; caller input setters generated
only inside slots can be absent. A root input for the same state can incidentally
provide the missing setter, which is not a supported workaround. The first two
failures also reproduce on the pre-PR #53 baseline. Reproductions/traces from the
independent review remain in `/tmp/jtml-pr53-review.1PX9GM`, particularly
`adversarial-results.json`, `baseline/adversarial-results.json` and
`input-comparison-results.json`. These limitations are recorded, not fixed or
claimed as universally fail-closed. CoffeeOrder uses component-local inputs;
the generated starter uses the verified simple text/button slots.

No published release includes main's production import or input synchronization
fixes. RC.2 remains suitable for the demonstrated Counter, not the full form reset
or modular production contract. Classic single-file scaffolding, generic diagnostic
hints, platform coverage, unsigned downloads, native-renderer discrepancies,
reference HTML-size overage and incomplete type/source-span coverage remain.
No separate compiler change is needed for the two bounded first-app flows on main.
Broader application parity and unaided external-user success remain unproven.
