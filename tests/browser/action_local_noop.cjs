const assert = require("node:assert/strict");
const path = require("node:path");
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
    await page.goto(
      pathToFileURL(path.resolve(process.argv[2], "index.html")).href,
    );
    const input = page.getByLabel("Draft");
    await input.fill("Unsaved local draft");
    await page
      .getByRole("button", { name: "Increment counter", exact: true })
      .click();
    await input.focus();
    await input.evaluate((e) => e.setSelectionRange(2, 7, "backward"));
    await page.evaluate(() => {
      window.retained = [
        ...document.querySelectorAll(
          ".owner,.counter,input,button,[data-jtml-direct-instance]",
        ),
      ];
      window.registry = jtml.getRuntimeRegistrySnapshot();
    });
    for (let i = 0; i < 3; i++) {
      await page
        .getByRole("button", { name: "Walk locals", exact: true })
        .evaluate((e) => e.click());
      await page.evaluate(() => new Promise((r) => requestAnimationFrame(r)));
      assert.equal(
        await page.evaluate(() => !!jtml.directComponentPatchFallback),
        false,
        "action-local loop writes never trigger patch fallback",
      );
      assert.equal(
        await page.evaluate(
          () =>
            retained.every((e) => e.isConnected) &&
            JSON.stringify(registry) ===
              JSON.stringify(jtml.getRuntimeRegistrySnapshot()),
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
        [true, "Unsaved local draft", 2, 7, "backward"],
      );
      assert.equal(await page.locator(".count").textContent(), "1");
    }
    await page
      .getByRole("button", { name: "Increment counter", exact: true })
      .click();
    assert.equal(await page.locator(".count").textContent(), "2");
    assert.deepEqual(errors, []);
    console.log("action-local no-op loop: PASS");
  } finally {
    await browser.close();
  }
})().catch((e) => {
  console.error(e);
  process.exit(1);
});
