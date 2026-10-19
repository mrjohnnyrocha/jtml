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
      errors = [];
    page.on("pageerror", (e) => errors.push(e.message));
    page.on("console", (m) => {
      if (m.type() === "error") errors.push(m.text());
    });
    await page.goto(
      pathToFileURL(path.resolve(process.argv[2], "index.html")).href,
    );
    const probe = page.locator('[data-probe="A"]');
    await page.evaluate(() => {
      window.other = document.querySelector('[data-probe="B"]');
      window.otherHTML = other.outerHTML;
      window.otherRefs = [other, ...other.querySelectorAll("*")];
    });
    const settle = () =>
      page.evaluate(
        () =>
          new Promise((r) =>
            requestAnimationFrame(() => requestAnimationFrame(r)),
          ),
      );
    const click = async (name) => {
      await probe
        .getByRole("button", { name, exact: true })
        .evaluate((e) => e.click());
      await settle();
    };
    let scores = [2, 5],
      index = 0,
      offset = 1,
      limit = 4,
      enabled = false,
      fallback = "First";
    const checkpoint = async () => {
      const selected = scores[index] + offset,
        label = enabled ? "Selected " + selected : fallback;
      assert.equal(
        await probe.locator(".selected").textContent(),
        "" + selected,
      );
      assert.equal(
        await probe.locator(".logical").textContent(),
        "" + (selected > limit && enabled),
      );
      assert.equal(await probe.locator(".ternary").textContent(), label);
      assert.equal(
        await probe.locator(".dependency-child h2").textContent(),
        label,
      );
      assert.equal(
        await probe.locator(".dependency-child").getAttribute("title"),
        label,
      );
      for (let n = 0; n < 2; n++)
        assert.equal(
          await probe
            .locator('[data-record="r-' + n + '"] .score')
            .textContent(),
          "" + scores[n],
        );
      assert.equal(
        await page.evaluate(
          () =>
            otherRefs.every((e) => e.isConnected) &&
            other.outerHTML === otherHTML,
        ),
        true,
      );
      assert.deepEqual(errors, []);
      assert.deepEqual(
        await page.evaluate(() =>
          [
            "directComponentCreateFallback",
            "directComponentPatchFallback",
            "directComponentBodyPlanFallback",
            "directComponentStaticUpdateError",
          ].filter((k) => jtml[k]),
        ),
        [],
      );
    };
    await checkpoint();
    // Change the inactive branch operand first, then activate its guard.
    await click("Fallback");
    fallback = "Changed";
    await checkpoint();
    await click("Offset");
    offset++;
    await checkpoint();
    await click("Index");
    index = 1;
    await checkpoint();
    await click("Enable");
    enabled = true;
    await checkpoint();
    await probe.locator(".dependency-child button").click();
    await page.evaluate(() => {
      window.child = document
        .querySelector('[data-probe="A"] .dependency-child')
        .closest("[data-jtml-direct-instance]");
      window.childGeneration =
        jtml.getRuntimeRegistrySnapshot().dynamicGenerations[
          child.dataset.jtmlDirectInstance
        ];
    });
    for (const [name, mutate] of [
      ["Property", () => scores[0]++],
      ["Indexed", () => scores[index]++],
      ["Alias", () => (scores[index] += 2)],
      ["Limit", () => limit++],
      ["Offset", () => offset++],
      ["Index", () => (index = 0)],
    ]) {
      await click(name);
      mutate();
      await checkpoint();
      assert.equal(
        await page.evaluate(
          () =>
            child.isConnected &&
            jtml.getRuntimeRegistrySnapshot().dynamicGenerations[
              child.dataset.jtmlDirectInstance
            ] === childGeneration,
        ),
        true,
        "unchanged conditional branch keeps component",
      );
      assert.equal(
        await probe.locator(".dependency-child .counter").textContent(),
        "1",
      );
    }
    const input = probe.locator('[data-record="r-0"] input');
    await input.fill("unsaved local draft");
    await input.focus();
    await input.evaluate((e) => e.setSelectionRange(2, 11, "backward"));
    await page.evaluate(() => {
      window.rowRefs = [
        ...document.querySelectorAll('[data-probe="A"] .dependency-row'),
      ].flatMap((e) => [e, ...e.querySelectorAll("*")]);
      window.generations = jtml.getRuntimeRegistrySnapshot().dynamicGenerations;
    });
    await click("Equivalent replacement");
    await checkpoint();
    assert.equal(
      await page.evaluate(
        () =>
          rowRefs.every((e) => e.isConnected) &&
          Object.entries(generations).every(
            ([id, g]) =>
              jtml.getRuntimeRegistrySnapshot().dynamicGenerations[id] === g,
          ),
      ),
      true,
    );
    assert.deepEqual(
      await input.evaluate((e) => [
        e === document.activeElement,
        e.value,
        e.selectionStart,
        e.selectionEnd,
        e.selectionDirection,
      ]),
      [true, "unsaved local draft", 2, 11, "backward"],
    );
    // Alias still resolves to the current owner after equivalent root replacement.
    await click("Alias");
    scores[index] += 2;
    await checkpoint();
    console.log(
      JSON.stringify({
        N03: "PASS",
        expectedRoots: [
          "records",
          "index",
          "offset",
          "limit",
          "enabled",
          "fallback",
        ],
        browser: browser.version(),
      }),
    );
  } finally {
    await browser.close();
  }
})().catch((e) => {
  console.error(e);
  process.exitCode = 1;
});
