// Independent N01/N02/N05/N06/N07/N08 oracle. Expectations never come from plans.
const assert = require("node:assert/strict");
const fs = require("node:fs"),
  path = require("node:path"),
  http = require("node:http");
const { chromium } = require(
  process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT
    ? path.join(
        process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT,
        "node_modules/playwright",
      )
    : "playwright",
);
const seed = [
  { id: "t-101", title: "Audit runtime", completed: false, note: "" },
  { id: "t-102", title: "Audit runtime", completed: false, note: "" },
  { id: "t-103", title: "Ship documentation", completed: true, note: "" },
  { id: "t-104", title: "<release> & CSP", completed: false, note: "" },
];
const results = Object.fromEntries(
  ["N01", "N02", "N03", "N05", "N06", "N07", "N08"].map((n) => [
    n,
    "UNVERIFIED",
  ]),
);
(async () => {
  const root = path.resolve(process.argv[2]);
  const assets = [
    "index.html",
    "jtml-runtime.js",
    "components/index.js",
    "app.js",
  ];
  assert.deepEqual(fs.readdirSync(root).sort(), [
    "app.js",
    "components",
    "index.html",
    "jtml-runtime.js",
  ]);
  for (const file of assets) {
    const source = fs.readFileSync(path.join(root, file), "utf8");
    assert(!/\beval\s*\(|\bnew\s+Function\b/.test(source), file);
    assert(!source.includes("jtml-update-plans.js"), file);
  }
  assert(fs.statSync(path.join(root, "jtml-runtime.js")).size <= 272000);
  assert.deepEqual(fs.readdirSync(path.join(root, "components")), ["index.js"]);
  const manifestText = fs
    .readFileSync(path.join(root, "index.html"), "utf8")
    .match(
      /<script type="application\/json" id="__jtml_client_manifest">([\s\S]*?)<\/script>/,
    )[1];
  assert(JSON.parse(manifestText).project);
  for (const forbidden of [
    "__jtml_static_update_plans",
    '\"bodyPlan\"',
    '\"bodySource\"',
    '\"bodyHex\"',
  ])
    assert(
      !fs
        .readFileSync(path.join(root, "components/index.js"), "utf8")
        .includes(forbidden),
      forbidden,
    );
  const policy =
    "default-src 'none'; script-src 'self'; style-src 'unsafe-inline'; connect-src 'none'; img-src 'none'; base-uri 'none'; form-action 'none'";
  // Real HTTP CSP, not a meta tag or mocked compilation. Only four assets exist.
  const server = http.createServer((req, res) => {
    const file = decodeURIComponent(req.url.slice(1) || "index.html");
    if (!assets.includes(file)) {
      res.writeHead(404);
      res.end();
      return;
    }
    res.writeHead(200, {
      "Content-Type": file.endsWith(".js") ? "text/javascript" : "text/html",
      "Content-Security-Policy": policy,
    });
    res.end(fs.readFileSync(path.join(root, file)));
  });
  await new Promise((r) => server.listen(0, "127.0.0.1", r));
  const origin = "http://127.0.0.1:" + server.address().port;
  const browser = await chromium.launch({
    headless: true,
    executablePath: process.env.JTML_BENCH_BROWSER,
  });
  let scenario = "N01";
  try {
    const page = await browser.newPage(),
      errors = [],
      requests = [];
    page.setDefaultTimeout(6000);
    page.on("pageerror", (e) => errors.push(e.message));
    page.on("console", (m) => {
      if (m.type() === "error") errors.push(m.text());
    });
    page.on("request", (r) => {
      if (!assets.map((f) => origin + "/" + f).includes(r.url()))
        requests.push(r.url());
    });
    await page.addInitScript(() => {
      window.negativeXss = 0;
      window.forbiddenActivity = [];
      window.cspViolations = [];
      window.listenerCounts = {};
      const originalAdd = document.addEventListener.bind(document);
      document.addEventListener = function (type, ...args) {
        listenerCounts[type] = (listenerCounts[type] || 0) + 1;
        return originalAdd(type, ...args);
      };
      document.addEventListener("securitypolicyviolation", (e) =>
        cspViolations.push(e.violatedDirective),
      );
      for (const name of ["fetch", "XMLHttpRequest", "WebSocket"])
        window[name] = function () {
          forbiddenActivity.push(name);
          throw Error("Unexpected " + name);
        };
    });
    const board = page.locator('.taskboard[data-board="A"]'),
      sibling = page.locator('.taskboard[data-board="B"]');
    const row = (id) => board.locator('.task-row[data-task-id="' + id + '"]');
    const settle = () =>
      page.evaluate(
        () =>
          new Promise((r) =>
            requestAnimationFrame(() => requestAnimationFrame(r)),
          ),
      );
    const activate = async (el) => {
      await el.evaluate((e) => e.click());
      await settle();
    };
    const button = (name) => board.getByRole("button", { name, exact: true });
    const state = () =>
      page.evaluate(() => {
        const s = jtml
          .getComponentInstances()
          .find((i) => i.scope?.boardId === "A" && i.scope?.records).scope;
        return {
          records: s.records,
          order: s.order,
          nextId: s.nextId,
          notes: s.noteSaveCount,
          submits: s.submitCount,
        };
      });
    const load = async () => {
      await page.goto(origin + "/index.html");
      await settle();
      await page.evaluate(() => {
        window.bRefs = [
          ...document.querySelectorAll('[data-board="B"], [data-board="B"] *'),
        ];
        window.bHTML = bRefs[0].outerHTML;
      });
    };
    const health = async () => {
      const s = await page.evaluate(() => ({
        registry: jtml.getRuntimeRegistrySnapshot(),
        dom: [...document.querySelectorAll("[data-jtml-direct-instance]")]
          .map((e) => e.dataset.jtmlDirectInstance)
          .sort(),
        fallback: [
          "directComponentCreateFallback",
          "directComponentPatchFallback",
          "directComponentBodyPlanFallback",
          "directComponentStaticUpdateError",
        ].filter((k) => jtml[k]),
        mode: jtml.browserBuild,
        staticCreate: jtml.directComponentStaticCreate?.mode,
        legacy: typeof window.__jtml_static_update_plans,
        isolation:
          bRefs.every((e) => e.isConnected) && bRefs[0].outerHTML === bHTML,
        forbidden: forbiddenActivity,
        violations: cspViolations,
        xss: negativeXss,
      }));
      assert.deepEqual(
        s.dom,
        Object.keys(s.registry.dynamicGenerations).sort(),
        "DOM/registry bijection",
      );
      assert.equal(new Set(s.dom).size, s.dom.length, "unique instance IDs");
      assert.equal(
        new Set(s.registry.handlerOwnerIds).size,
        s.registry.handlerOwnerIds.length,
      );
      assert(
        s.registry.handlerOwnerIds.every((id) => s.dom.includes(id)),
        "handlers only for live owners",
      );
      assert.deepEqual(s.fallback, []);
      assert.equal(s.mode.mode, "prod");
      assert.equal(s.staticCreate, "static-production-create-function");
      assert.equal(s.mode.cleanProduction, true);
      assert.equal(s.mode.compatibility, false);
      assert.equal(s.legacy, "undefined");
      assert.equal(s.isolation, true);
      assert.deepEqual(s.forbidden, []);
      assert.deepEqual(s.violations, []);
      assert.equal(s.xss, 0);
      assert.equal(
        await page.locator("[onclick],[onsubmit],[onkeydown]").count(),
        0,
        "live DOM has no inline event handlers",
      );
      assert.deepEqual(errors, []);
      assert.deepEqual(requests, []);
      return s.registry;
    };
    const capture = async () =>
      page.evaluate(() => {
        window.retained = [
          ...document.querySelectorAll('[data-board="A"] .task-row'),
        ].map((e) => ({
          id: e.dataset.taskId,
          e,
          nodes: [...e.querySelectorAll("*")],
        }));
        window.recordOwner = jtml
          .getComponentInstances()
          .find((i) => i.scope?.boardId === "A" && i.scope?.records).scope;
        window.recordMap = recordOwner.records;
        window.recordRefs = Object.entries(recordMap);
        window.retainedGenerations =
          jtml.getRuntimeRegistrySnapshot().dynamicGenerations;
      });
    const retained = async (keepRecords = true) =>
      assert.equal(
        await page.evaluate(
          (keepRecords) =>
            (!keepRecords ||
              (recordOwner.records === recordMap &&
                recordRefs.every(
                  ([id, record]) => recordOwner.records[id] === record,
                ))) &&
            retained.every(
              (x) => x.e.isConnected && x.nodes.every((e) => e.isConnected),
            ) &&
            Object.entries(retainedGenerations).every(
              ([id, g]) =>
                jtml.getRuntimeRegistrySnapshot().dynamicGenerations[id] === g,
            ),
          keepRecords,
        ),
        true,
        "retained DOM and generations",
      );
    await load();
    const initial = await state();
    await capture();
    // The actual JTML model boundary validates the complete batch before commit.
    const supply = async (records) => {
      const control = button("Validate supplied data");
      await control.evaluate(
        (e, data) =>
          e.setAttribute(
            "data-jtml-direct-component-args",
            JSON.stringify([data]),
          ),
        records,
      );
      await activate(control);
    };
    const invalidCases = [
      [
        "Duplicate IDs",
        [
          { ...seed[0], id: "t-201" },
          { ...seed[1], id: "t-201" },
        ],
      ],
      [
        "Missing ID",
        [
          { ...seed[0], id: "t-201" },
          { title: "Missing", completed: false, note: "" },
        ],
      ],
      ...[null, 201, "", "  ", ["t-201"], { value: "t-201" }].map((id) => [
        "Invalid ID " + JSON.stringify(id),
        [{ ...seed[0], id }],
      ]),
    ];
    for (const [name, data] of invalidCases) {
      await supply(data);
      assert.equal(
        await board.locator(".data-error").textContent(),
        "Invalid task IDs",
        name,
      );
      assert.deepEqual(
        await state(),
        initial,
        name + " must not commit or allocate",
      );
      await retained();
      await health();
    }
    // A positive control prevents an always-rejecting validator from passing.
    await supply(seed);
    assert.equal(await board.locator(".data-error").textContent(), "");
    assert.deepEqual(await state(), initial);
    await retained(false);
    await capture();
    await health();
    assert.deepEqual(
      await row("t-101").locator("h2").textContent(),
      await row("t-102").locator("h2").textContent(),
      "duplicate titles valid",
    );
    results.N01 = "PASS";
    scenario = "N02";
    await activate(button("Mutate task ID"));
    assert.equal(
      await board.locator(".data-error").textContent(),
      "Task IDs are immutable",
    );
    assert.deepEqual(await state(), initial);
    await retained();
    await health();
    await activate(
      row("t-103").getByRole("button", { name: "Delete task", exact: true }),
    );
    const deleted = await state();
    await capture();
    await supply([seed[2]]);
    assert.equal(
      await board.locator(".data-error").textContent(),
      "Task ID was already used",
    );
    assert.deepEqual(await state(), deleted);
    await retained();
    await health();
    results.N02 = "PASS";
    scenario = "N03";
    await load();
    const expected = Object.fromEntries(seed.map((r) => [r.id, { ...r }]));
    await activate(button("Owned property title"));
    expected["t-101"].title = "First owned";
    assert.equal(
      await board.locator(".selected-title").textContent(),
      "First owned!",
    );
    await board
      .getByLabel("Selected fallback", { exact: true })
      .fill("Inactive changed");
    await settle();
    assert.equal(
      await board.locator(".selected-label").textContent(),
      "Inactive changed",
    );
    await board.getByLabel("Selected task ID", { exact: true }).fill("t-102");
    await settle();
    assert.equal(
      await board.locator(".selected-title").textContent(),
      "Audit runtime!",
    );
    await board.getByLabel("Selected suffix", { exact: true }).fill("?");
    await settle();
    await activate(button("Enable selected label"));
    assert.equal(
      await board.locator(".selected-label").textContent(),
      "Audit runtime?",
    );
    await activate(button("Owned indexed title"));
    expected["t-102"].title = "Indexed owned";
    assert.equal(
      await board.locator(".selected-label").textContent(),
      "Indexed owned?",
    );
    await activate(button("Owned alias title"));
    expected["t-102"].title += " alias";
    assert.deepEqual((await state()).records, expected);
    assert.equal(
      await board.locator(".selected-label").textContent(),
      "Indexed owned alias?",
    );
    assert.equal(
      await row("t-102").locator("h2").textContent(),
      "Indexed owned alias",
    );
    assert.equal(await row("t-101").locator("h2").textContent(), "First owned");
    await row("t-102").locator(".row-note").fill("unsaved selection");
    await activate(
      row("t-102").getByRole("button", { name: "Increment", exact: true }),
    );
    const selectedInput = row("t-102").locator(".row-note");
    await selectedInput.focus();
    await selectedInput.evaluate((e) => e.setSelectionRange(2, 10, "backward"));
    await capture();
    const equivalentBefore = await state();
    await activate(button("Equivalent task records"));
    assert.deepEqual(await state(), equivalentBefore);
    await retained(false);
    assert.deepEqual(
      await selectedInput.evaluate((e) => [
        e === document.activeElement,
        e.value,
        e.selectionStart,
        e.selectionEnd,
        e.selectionDirection,
      ]),
      [true, "unsaved selection", 2, 10, "backward"],
    );
    assert.equal(
      await row("t-102").locator(".interaction-count").textContent(),
      "1",
    );
    await selectedInput.press("Enter");
    await settle();
    expected["t-102"].note = "unsaved selection";
    assert.deepEqual((await state()).records, expected);
    assert.equal((await state()).notes, 1);
    await health();
    results.N03 = "PASS";
    scenario = "N05";
    await load();

    const value = `Quotes " ' & < > </script><img src=x onerror="window.negativeXss=1">\u2028\u2029 café 雪 😀 \\n \\u0026`;
    assert(
      value.includes(String.fromCodePoint(0x2028)) &&
        value.includes(String.fromCodePoint(0x2029)),
      "actual Unicode separators, not source escape text",
    );
    await board.locator(".create-title").fill(value);
    await board.locator(".create-title").press("Enter");
    await settle();
    assert.equal((await state()).records["t-105"].title, value);
    assert.equal(await row("t-105").locator("h2").textContent(), value);
    for (let i = 0; i < 3; i++) {
      const title = value + " title " + i;
      await row("t-105").locator(".row-draft").fill(title);
      await row("t-105").locator(".row-draft").press("Enter");
      await settle();
      const note = `  ${value} iteration ${i}  `;
      await row("t-105").locator(".row-note").fill(note);
      await row("t-105").locator(".row-note").press("Enter");
      await settle();
      assert.equal((await state()).records["t-105"].note, note);
      assert.equal(
        await row("t-105").locator(".saved-note").textContent(),
        note,
      );
      await activate(
        row("t-105").getByRole("button", {
          name: "Toggle complete",
          exact: true,
        }),
      );
      await activate(button("Open"));
      await activate(button("All"));
      assert.equal(await row("t-105").locator(".row-note").inputValue(), note);
      assert.equal(await row("t-105").locator("h2").textContent(), title);
      await activate(
        row("t-105").getByRole("button", {
          name: "Toggle complete",
          exact: true,
        }),
      );
      await health();
    }
    assert.equal(await page.locator('img,script[src="x"]').count(), 0);
    results.N05 = "PASS";
    scenario = "N06";
    await load();
    let saved = 0;
    const base = await health();
    assert.equal(
      Object.keys(base.dynamicGenerations).length,
      26,
      "two boards each own four three-component rows and a branch",
    );
    assert.equal(
      base.handlerOwnerIds.length,
      8,
      "one emitted-event owner per live task row",
    );
    const listeners = await page.evaluate(() => ({ ...listenerCounts }));
    for (let cycle = 0; cycle < 20; cycle++) {
      await page.evaluate(() => {
        const e = document
          .querySelector('[data-board="A"] [data-task-id="t-103"]')
          .closest("[data-jtml-direct-instance]");
        window.staleControls = [...e.querySelectorAll("button")];
        window.oldOwners = [
          e,
          ...e.querySelectorAll("[data-jtml-direct-instance]"),
        ]
          .filter((e) => e.hasAttribute("data-jtml-direct-instance"))
          .map((e) => e.dataset.jtmlDirectInstance);
      });
      await activate(button("Open"));
      const filtered = await health();
      assert.equal(
        Object.keys(filtered.dynamicGenerations).length,
        Object.keys(base.dynamicGenerations).length - 3,
      );
      assert.equal(
        await page.evaluate(() =>
          oldOwners.every(
            (id) =>
              !(id in jtml.getRuntimeRegistrySnapshot().dynamicGenerations) &&
              !jtml.getRuntimeRegistrySnapshot().handlerOwnerIds.includes(id),
          ),
        ),
        true,
      );
      const before = await state();
      await page.evaluate(() => staleControls.forEach((e) => e.click()));
      await settle();
      assert.deepEqual(await state(), before);
      await activate(button("All"));
      const restored = await health();
      const afterRemount = await state();
      await page.evaluate(() => staleControls.forEach((e) => e.click()));
      await settle();
      assert.deepEqual(
        await state(),
        afterRemount,
        "detached old generation cannot dispatch into remounted ID",
      );
      assert.equal(
        Object.keys(restored.dynamicGenerations).length,
        Object.keys(base.dynamicGenerations).length,
      );
      assert.equal(
        restored.handlerOwnerIds.length,
        base.handlerOwnerIds.length,
      );
      for (const id of await page.evaluate(() => oldOwners))
        assert(
          restored.dynamicGenerations[id] > filtered.dynamicGenerations[id] ||
            restored.dynamicGenerations[id] > base.dynamicGenerations[id],
        );
      await row("t-103")
        .locator(".row-note")
        .fill("cycle " + cycle);
      await row("t-103").locator(".row-note").press("Enter");
      await settle();
      assert.equal((await state()).notes, ++saved);
      await activate(
        row("t-101").getByRole("button", {
          name: "Toggle complete",
          exact: true,
        }),
      );
      await activate(button("Open"));
      assert.equal(await row("t-101").count(), 0);
      await activate(button("All"));
      assert.equal(
        await row("t-101").locator(".interaction-count").textContent(),
        "0",
      );
      await row("t-101")
        .locator(".row-note")
        .fill("second return " + cycle);
      await row("t-101")
        .getByRole("button", { name: "Save note", exact: true })
        .click();
      await settle();
      assert.equal((await state()).notes, ++saved);
      await activate(
        row("t-101").getByRole("button", {
          name: "Toggle complete",
          exact: true,
        }),
      );
      await activate(button("Toggle help"));
      await activate(button("Increment help"));
      assert.equal(await board.locator(".help-count").textContent(), "1");
      await activate(button("Toggle help"));
      assert.equal(await board.locator(".help").count(), 0);
      const current = await health();
      assert.equal(Object.keys(current.dynamicGenerations).length, 26);
      assert.equal(current.handlerOwnerIds.length, 8);
      assert.deepEqual(
        await page.evaluate(() => listenerCounts),
        listeners,
        "delegated listeners do not accumulate",
      );
    }
    results.N06 = "PASS";
    scenario = "N07";
    await load();
    const input = row("t-101").locator(".row-note");
    await input.fill("retained multi-character draft");
    await input.focus();
    await input.evaluate((e) => e.setSelectionRange(3, 17, "backward"));
    await capture();
    for (const el of [
      row("t-102").getByRole("button", {
        name: "Toggle complete",
        exact: true,
      }),
      button("Change fallback"),
    ]) {
      await activate(el);
      await retained();
      assert.deepEqual(
        await input.evaluate((e) => [
          e === document.activeElement,
          e.selectionStart,
          e.selectionEnd,
          e.selectionDirection,
          e.value,
        ]),
        [true, 3, 17, "backward", "retained multi-character draft"],
      );
    }
    await activate(
      row("t-101").getByRole("button", {
        name: "Toggle complete",
        exact: true,
      }),
    );
    await activate(button("Open"));
    assert.equal(
      await button("Open").evaluate((e) => e === document.activeElement),
      true,
    );
    await activate(button("All"));
    assert.equal(
      await button("Open").evaluate((e) => e === document.activeElement),
      true,
      "remount must not steal focus",
    );
    assert.equal(
      await input.inputValue(),
      "",
      "remount starts with committed draft",
    );
    for (const [id, target] of [
      ["t-101", "#edit-title-A-t-102"],
      ["t-104", "#edit-title-A-t-103"],
    ]) {
      await row(id).locator(".row-note").focus();
      await activate(
        row(id).getByRole("button", { name: "Delete task", exact: true }),
      );
      assert.equal(
        await board
          .locator(target)
          .evaluate((e) => e === document.activeElement),
        true,
      );
    }
    await health();
    results.N07 = "PASS";
    results.N08 = "PASS";
    console.log(
      JSON.stringify({
        results,
        csp: policy,
        cycles: 20,
        browser: browser.version(),
        unexpectedRequests: requests,
      }),
    );
  } catch (e) {
    console.error(JSON.stringify({ results, failed: scenario }));
    throw e;
  } finally {
    await browser.close();
    await new Promise((r) => server.close(r));
  }
})().catch((e) => {
  console.error(e);
  process.exitCode = 1;
});
