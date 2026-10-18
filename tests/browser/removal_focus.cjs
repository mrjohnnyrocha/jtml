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
    for (const [removedFocus, outside] of [
      [false, false],
      [true, false],
      [true, true],
    ]) {
      const page = await browser.newPage(),
        errors = [];
      page.on("pageerror", (e) => errors.push(e.message));
      page.on("console", (m) => {
        if (m.type() === "error") errors.push(m.text());
      });
      await page.goto(
        pathToFileURL(path.resolve(process.argv[2], "index.html")).href,
      );
      assert.equal(
        await page.evaluate(
          () =>
            jtml
              .getComponentInstances()
              .filter((i) => i.component === "RemovalExample").length,
        ),
        2,
        "one runtime instance per rendered component",
      );
      const group = page.locator('[data-example="A"]'),
        sibling = page.locator('[data-example="B"]');
      const siblingHTML = await sibling.innerHTML();
      const input = removedFocus
        ? group.getByLabel("Removed draft")
        : group.getByLabel("Unrelated draft");
      await input.fill("Retained draft");
      await input.focus();
      await input.evaluate((e) => e.setSelectionRange(2, 7, "backward"));
      await group
        .getByRole("button", {
          name: outside ? "Remove from outside" : "Remove control",
          exact: true,
        })
        .evaluate((e) => e.click());
      await page.evaluate(() => new Promise((r) => requestAnimationFrame(r)));
      assert.equal(await group.locator(".removed").count(), 0);
      if (removedFocus)
        assert.equal(
          await group
            .getByRole("button", {
              name: outside ? "Remove from outside" : "After removal",
              exact: true,
            })
            .evaluate((e) => e === document.activeElement),
          true,
          "removed focused control uses declared surviving target",
        );
      else
        assert.deepEqual(
          await input.evaluate((e) => [
            e === document.activeElement,
            e.value,
            e.selectionStart,
            e.selectionEnd,
            e.selectionDirection,
          ]),
          [true, "Retained draft", 2, 7, "backward"],
          "connected focus remains untouched",
        );
      assert.equal(await sibling.innerHTML(), siblingHTML);
      assert.deepEqual(errors, []);
      await page.close();
    }
    console.log(
      "generic removal focus: PASS (removed and retained active controls)",
    );
  } finally {
    await browser.close();
  }
})().catch((e) => {
  console.error(e);
  process.exit(1);
});
