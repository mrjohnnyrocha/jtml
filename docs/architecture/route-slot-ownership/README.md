# Routed slot instance ownership

Base: merged main `6ece4e746dd8e126626f403168a6890eef74b48a` (PR #66).
The original audit commit and preserved worktree are excluded from this branch.

## Reproduction and focused evidence

Run with `JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT` pointing to Playwright and
`JTML_BENCH_BROWSER` pointing to real Chromium:

```sh
node tests/browser/route_slot_ownership.cjs
node tests/browser/enterprise_audit.cjs
ctest --test-dir build -R 'RouteSlot|RouteLayoutsRetain|RouteAndLayout' --output-on-failure
```

Chromium 151.0.7922.34, strict script CSP, browser closed normally.
The unchanged-main-equivalent renderer compiler (identical source tree to base)
produced **6/13, exit 1**: the six plain-route checks pass; all seven layout checks
fail (`before.json`). The candidate produces **13/13, exit 0** (`after.json`).
All three focused CTests pass, including seven rejected manifest mutations.
Existing route/layout, component redirects, root effects, explicit inputs,
Media rendering and Enterprise supported-value action suites also exit zero.

The fixture exercises local input/mirror/focus, exactly-once no-argument actions,
cursor/DOM retention, isolated nested counters, dormant declarations, conditional
removal of counters and the supplied route, stale detached handlers, fresh
remounting, retained route state, redirects and guard denial. It does not rely on
dynamic event arguments.

## Compiler and lifecycle contract

Friendly route lowering records the already-issued route invocation identity on
its layout. Semantic IR retains `slotInstance`; Runtime Plan and the clean client
manifest carry that exact identity. The closed schema accepts only a layout's
unique route child in the same caller module with its matching slot invocation.
Unknown child, role/module/call mismatch, duplicate ownership/identity and unknown
authority fields fail validation.

The generated default-slot creator registers that exact child through the existing
dynamic-instance lifecycle, with the layout as `parentId`, then uses the existing
component scope and static renderer. No global manifest registration, global name
binding fallback, interpreter, router replacement or writable route binding is
introduced. The root ancestry filter remains. Teardown/pruning follows the explicit
parent edge as well as existing nested generated-ID ancestry, so removed caller-owned
children lose their registry, scope, handlers and dispatch authority. No raw internal
slot marker is emitted in production HTML. Existing schema/privacy/CSP checks remain.

## Enterprise: still NO-GO

The unchanged detailed audit is **8/11 before, exit 1** and **7/11 after, exit 1**;
both have no page exceptions. PR #66 removed the previous scene3d startup blocker.
The original demo and audit assertions are unchanged.

The additional failed assertion is Email accessibility: activating Login uses the
existing direct renderer, which emits its implicit module-targeted input without
an accessible name. An independent no-layout control built with the unchanged-main
compiler produces the same DOM (`plain-main-dom.json`); candidate DOM is recorded
in `enterprise-dom.json`. This pre-existing direct-renderer discrepancy is now exposed
in the layout route, not silently counted as a pass. Input ownership remains
unauthorized, its value is empty, and module loginEmail remains ada@example.com.
The dependent probe@example.test token/audit assertions still fail. Sign-in with
the existing supported value, sign-out, token clearing and guard denial pass.
There is no authentication service or API mock.

Enterprise's actual LocalCounter buttons now change the first mirror to 1 while
the sibling stays 0 (`enterprise-counter.txt`). Compiler-issued Command/Login route
instances and nested counters have active environments. Unsupported implicit module
writes, component free module reads, direct input accessibility and stale dynamic
event arguments remain separate work; no ownership contract is expanded here.

## Size and delivery acceptance

Runtime: **272,370 raw / 50,835 gzip bytes**, compared with base **272,989 / 50,903**.
The 273,000-byte ceiling is unchanged (630 bytes headroom). Equivalent object-property
shorthand in the existing generated-helper table funds the lifecycle helper exposure;
no runtime code stripping or budget adjustment is used.

Focused ownership acceptance: GO. Enterprise application readiness: NO-GO.
Delivery additionally requires the complete local gate on the committed candidate,
all nine benchmarks with clean browser exits, and Ubuntu/macOS CI on that exact SHA.
Those external results belong in the PR so this evidence commit stays immutable.
