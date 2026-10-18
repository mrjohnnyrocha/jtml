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
  const b = await chromium.launch({
    headless: true,
    executablePath: process.env.JTML_BENCH_BROWSER,
  });
  try {
    const p = await b.newPage(),
      errors = [];
    p.on("pageerror", (e) => errors.push(e.message));
    await p.goto(
      pathToFileURL(path.resolve(process.argv[2], "index.html")).href,
    );
    const input = p.getByLabel("Draft");
    await input.fill("Unsaved draft");
    await p.getByRole("button", { name: "Increment", exact: true }).click();
    await input.focus();
    await input.evaluate((e) => e.setSelectionRange(1, 5, "backward"));
    await p.evaluate(() => {
      window.retained = [...document.querySelectorAll(".editor,input,button")];
      window.registry = jtml.getRuntimeRegistrySnapshot();
    });
    await input.press("Enter");
    assert.equal(await p.locator(".saved").textContent(), "Unsaved draft");
    assert.equal(await p.locator(".saves").textContent(), "1");
    assert.equal(await p.locator(".counter").textContent(), "1");
    assert.equal(
      await p.evaluate(
        () =>
          retained.every((e) => e.isConnected) &&
          JSON.stringify(registry) ===
            JSON.stringify(jtml.getRuntimeRegistrySnapshot()),
      ),
      true,
    );
    assert.deepEqual(
      await input.evaluate((e) => [
        document.activeElement === e,
        e.selectionStart,
        e.selectionEnd,
        e.selectionDirection,
      ]),
      [true, 1, 5, "backward"],
    );
    await input.press("Alt+F8");
    assert.equal(await p.locator(".counter").textContent(), "2");
    assert.deepEqual(await p.locator(".items p").allTextContents(), [
      "right",
      "left",
    ]);
    assert.deepEqual(
      await input.evaluate((e) => [
        document.activeElement === e,
        e.selectionStart,
        e.selectionEnd,
        e.selectionDirection,
      ]),
      [true, 1, 5, "backward"],
    );
    await input.press("Enter");
    assert.equal(await p.locator(".saves").textContent(), "2");
    await p.getByRole("button", { name: "Save", exact: true }).click();
    assert.equal(await p.locator(".saves").textContent(), "3");
    await input.focus();
    await p
      .getByRole("button", { name: "Hide editor", exact: true })
      .evaluate((e) => e.click());
    assert.equal(await p.locator(".editor").count(), 0);
    assert.equal(
      await p
        .getByRole("button", { name: "Hide editor", exact: true })
        .evaluate((e) => document.activeElement === e),
      true,
    );
    assert.deepEqual(errors, []);
    console.log("emit-only action retains owner DOM/state/focus: PASS");
  } finally {
    await b.close();
  }
})().catch((e) => {
  console.error(e);
  process.exit(1);
});
