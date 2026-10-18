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
// Independent values; never read expected data from emitted plans or fixture source.
const expected = [
  "Tom & Jerry",
  "<tag> > &",
  "Quote: \"yes\" and 'ok'",
  "C:\\tmp\\new",
  "line\nnext\r\ttab\b\f",
  "literal \\n \\u0026",
  "Unicode café 雪 😀",
  "&<>😀",
  '</script><img src=x onerror="window.literalXss=1">',
  "\0\x1f",
  "tail\\",
  '--><img src=x onerror="window.literalXss=1">',
  "single ' quote",
  "Done",
];
(async () => {
  const browser = await chromium.launch({
    headless: true,
    executablePath: process.env.JTML_BENCH_BROWSER || undefined,
  });
  try {
    for (const mode of ["clean", "compat"]) {
      const page = await browser.newPage(),
        errors = [],
        requests = [];
      page.on("pageerror", (e) => errors.push(e.message));
      page.on("console", (m) => {
        if (m.type() === "error") errors.push(m.text());
      });
      const root = path.resolve(process.argv[2], mode);
      const allowed = [
        "index.html",
        "jtml-runtime.js",
        "components/index.js",
        "app.js",
        ...(mode === "compat" ? ["jtml-update-plans.js"] : []),
      ].map((f) => pathToFileURL(path.join(root, f)).href);
      page.on("request", (r) => {
        if (!allowed.includes(r.url())) requests.push(r.url());
      });
      await page.addInitScript(() => (window.literalXss = 0));
      await page.goto(allowed[0]);
      for (let index = 0; index < expected.length; index++) {
        if (index) {
          await page
            .getByRole("button", { name: "Next top", exact: true })
            .click();
          await page
            .getByRole("button", { name: "Next local", exact: true })
            .click();
        }
        await page.evaluate(() => new Promise((r) => requestAnimationFrame(r)));
        assert.deepEqual(errors, []);
        for (const name of ["top", "local"]) {
          assert.equal(
            await page
              .locator("." + name + (name === "top" ? " [data-jtml-expr]" : ""))
              .textContent(),
            expected[index],
            mode + " " + name + " case " + index,
          );
          assert.equal(
            await page
              .locator(
                "." +
                  name +
                  "-derived" +
                  (name === "top" ? " [data-jtml-expr]" : ""),
              )
              .textContent(),
            "<&> " + expected[index] + "!",
          );
        }
        assert.equal(
          await page.locator(".local").getAttribute("title"),
          expected[index],
        );
        assert.equal(await page.evaluate(() => window.literalXss), 0);
        assert.equal(await page.locator("img,tag").count(), 0);
        assert.deepEqual(errors, []);
        assert.deepEqual(requests, []);
      }
      console.log(mode + ": literal fidelity and XSS negatives PASS");
      await page.close();
    }
  } finally {
    await browser.close();
  }
})().catch((e) => {
  console.error(e);
  process.exit(1);
});
