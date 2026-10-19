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
    assert.equal(
      await p.evaluate(() => jtml.directComponentStaticCreate?.mode),
      "static-production-create-function",
      "deferred bootstrap uses generated creation",
    );
    await p.getByRole("button", { name: "Send structured arguments" }).click();
    assert.equal(
      await p.locator(".result").textContent(),
      "quoted ' & <雪> done",
    );
    assert.equal(await p.locator(".calls").textContent(), "1");
    assert.deepEqual(errors, []);
    console.log(
      "PASS: structured action arguments preserve whitespace, nesting, quotes and one dispatch",
    );
  } finally {
    await b.close();
  }
})().catch((e) => {
  console.error(e);
  process.exitCode = 1;
});
