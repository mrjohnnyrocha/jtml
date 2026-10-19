// Oracle for generated two-level keyed creation. Expected identity comes from
// logical board/group/row keys, never emitted IDs or dependency metadata.
const assert = require("node:assert/strict"),
  path = require("node:path");
const { pathToFileURL } = require("node:url");
const { chromium } = require(
  process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT
    ? path.join(
        process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT,
        "node_modules/playwright",
      )
    : "playwright",
);
(async () => {
  const browser = await chromium.launch({
    headless: true,
    executablePath: process.env.JTML_BENCH_BROWSER,
  });
  try {
    const page = await browser.newPage(),
      errors = [],
      unexpected = [];
    const root = path.resolve(process.argv[2]),
      assets = [
        "index.html",
        "jtml-runtime.js",
        "components/index.js",
        "app.js",
      ].map((f) => pathToFileURL(path.join(root, f)).href);
    page.on("pageerror", (e) => errors.push(e.message));
    page.on("console", (m) => {
      if (m.type() === "error") errors.push(m.text());
    });
    page.on("request", (r) => {
      if (!assets.includes(r.url())) unexpected.push(r.url());
    });
    await page.goto(assets[0]);
    const a = page.locator('.path-board[data-board="A"]');
    const row = (group, id) =>
      a.locator('.path-row[data-group="' + group + '"][data-row="' + id + '"]');
    const settle = () =>
      page.evaluate(
        () =>
          new Promise((r) =>
            requestAnimationFrame(() => requestAnimationFrame(r)),
          ),
      );
    const action = async (name) => {
      await a
        .getByRole("button", { name, exact: true })
        .evaluate((e) => e.click());
      await settle();
    };
    await page.evaluate(() => {
      const b = document.querySelector('.path-board[data-board="B"]');
      window.otherRefs = [b, ...b.querySelectorAll("*")];
      window.otherHTML = b.outerHTML;
    });
    let order = ["g-1/r-1", "g-1/r-2", "g-2/r-1", "g-2/r-2"];
    const check = async () => {
      await settle();
      assert.deepEqual(errors, []);
      assert.deepEqual(unexpected, []);
      assert.deepEqual(
        await a
          .locator(".path-row")
          .evaluateAll((es) =>
            es.map((e) => e.dataset.group + "/" + e.dataset.row),
          ),
        order,
      );
      const snapshot = await page.evaluate(() => ({
        registry: jtml.getRuntimeRegistrySnapshot(),
        dom: [...document.querySelectorAll("[data-jtml-direct-instance]")]
          .map((e) => e.dataset.jtmlDirectInstance)
          .sort(),
        isolation:
          otherRefs.every((e) => e.isConnected) &&
          otherRefs[0].outerHTML === otherHTML,
        create: jtml.directComponentStaticCreate?.mode,
        fallback: [
          "directComponentCreateFallback",
          "directComponentPatchFallback",
          "directComponentBodyPlanFallback",
          "directComponentStaticUpdateError",
        ].filter((k) => jtml[k]),
      }));
      assert.equal(snapshot.create, "static-production-create-function");
      assert.deepEqual(snapshot.fallback, []);
      assert.equal(snapshot.isolation, true, "board B remains isolated");
      assert.equal(
        snapshot.dom.length,
        (order.length + 4) * 2,
        "one row and counter per logical key",
      );
      assert.equal(
        new Set(snapshot.dom).size,
        snapshot.dom.length,
        "distinct nested keys must not reuse instance paths",
      );
      assert.deepEqual(
        Object.keys(snapshot.registry.dynamicGenerations).sort(),
        snapshot.dom,
        "exact DOM/registry bijection",
      );
      assert.deepEqual(
        snapshot.registry.handlerOwnerIds,
        [],
        "fixture has no emitted-event subscriptions",
      );
      return snapshot.registry.dynamicGenerations;
    };
    await check();
    const input = row("g-1", "r-1").locator("input");
    await input.fill("Unsaved unique draft");
    await row("g-1", "r-1")
      .getByRole("button", { name: "Increment path counter" })
      .click();
    await input.focus();
    await input.evaluate((e) => e.setSelectionRange(1, 8, "backward"));
    await page.evaluate(() => {
      window.kept = [
        ...document.querySelectorAll('[data-board="A"] .path-row'),
      ].map((e) => ({
        key: e.dataset.group + "/" + e.dataset.row,
        e,
        nodes: [...e.querySelectorAll("*")],
      }));
      window.generations = jtml.getRuntimeRegistrySnapshot().dynamicGenerations;
    });
    const retained = async (keys) =>
      assert.equal(
        await page.evaluate(
          (keys) =>
            kept
              .filter((x) => keys.includes(x.key))
              .every(
                (x) => x.e.isConnected && x.nodes.every((e) => e.isConnected),
              ),
          keys,
        ),
        true,
        "retained exact DOM",
      );
    for (let cycle = 0; cycle < 3; cycle++) {
      await action("Reorder groups");
      order = [...order.slice(2), ...order.slice(0, 2)];
      assert.deepEqual(await check(), await page.evaluate(() => generations));
      await retained(order);
      await action("Reorder rows");
      [order[0], order[1]] = [order[1], order[0]];
      assert.deepEqual(await check(), await page.evaluate(() => generations));
      await retained(order);
      assert.deepEqual(
        await input.evaluate((e) => [
          e === document.activeElement,
          e.value,
          e.selectionStart,
          e.selectionEnd,
          e.selectionDirection,
        ]),
        [true, "Unsaved unique draft", 1, 8, "backward"],
      );
      assert.equal(
        await row("g-1", "r-1").locator(".counter-value").textContent(),
        "1",
      );
    }
    // Remove the currently focused group, then remount. Keep one group untouched.
    if (order[0].startsWith("g-2/")) {
      await action("Reorder groups");
      order = [...order.slice(2), ...order.slice(0, 2)];
    }
    const before = await check(),
      fullOrder = [...order],
      survivors = order.slice(2);
    await page.evaluate(() => {
      const rows = [
        ...document.querySelectorAll(
          '[data-board="A"] .path-row[data-group="g-1"]',
        ),
      ];
      window.removedControls = rows.flatMap((e) => [
        ...e.querySelectorAll("button"),
      ]);
    });
    await action("Remove first group");
    order = survivors;
    const filtered = await check();
    await retained(survivors);
    const removed = Object.keys(before).filter((id) => !(id in filtered));
    assert.equal(removed.length, 4);
    for (const id of removed) {
      const entries = await page.evaluate(
        ({ id, g }) =>
          (jtml.directComponentDestroyLifecycle?.entries || []).filter(
            (e) => e.id === id && e.g === g,
          ),
        { id, g: before[id] },
      );
      assert.equal(entries.length, 1, "exactly one destroyed generation");
      assert(entries[0].r && entries[0].s && entries[0].e && entries[0].d);
    }
    assert.equal(
      await a
        .getByRole("button", { name: "Remove first group" })
        .evaluate((e) => e === document.activeElement),
      true,
    );
    await action("Restore groups");
    order = fullOrder;
    const remounted = await check();
    await retained(survivors);
    for (const id of removed)
      assert(remounted[id] > before[id], "fresh removed generation");
    for (const id of Object.keys(filtered))
      assert.equal(remounted[id], filtered[id], "retained sibling generation");
    assert.equal(await input.inputValue(), "Seed");
    assert.equal(
      await row("g-1", "r-1").locator(".counter-value").textContent(),
      "0",
    );
    assert.equal(
      await a
        .getByRole("button", { name: "Remove first group" })
        .evaluate((e) => e === document.activeElement),
      true,
      "remount does not steal focus",
    );
    await page.evaluate(() => removedControls.forEach((e) => e.click()));
    await settle();
    assert.equal(
      await row("g-1", "r-1").locator(".counter-value").textContent(),
      "0",
      "detached old generation cannot reach remounted component",
    );
    await row("g-1", "r-1")
      .getByRole("button", { name: "Increment path counter" })
      .click();
    assert.equal(
      await row("g-1", "r-1").locator(".counter-value").textContent(),
      "1",
    );
    await check();
    console.log(
      "PASS: static nested keyed paths, overlapping boards/groups, reorder/removal/remount, exact ownership, focus and stale controls",
    );
  } finally {
    await browser.close();
  }
})().catch((e) => {
  console.error(e);
  process.exitCode = 1;
});
