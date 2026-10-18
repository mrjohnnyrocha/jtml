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
const root = path.resolve(process.argv[2]);
const seed = [
  { id: "t-101", title: "Audit runtime", completed: false, note: "" },
  { id: "t-102", title: "Audit runtime", completed: false, note: "" },
  { id: "t-103", title: "Ship documentation", completed: true, note: "" },
  { id: "t-104", title: "<release> & CSP", completed: false, note: "" },
];
(async () => {
  const browser = await chromium.launch({
    executablePath: process.env.JTML_BENCH_BROWSER,
    headless: true,
  });
  const results = Object.fromEntries(
    ["C04", "C05", "C06", "C07", "C08"].map((id) => [id, "UNVERIFIED"]),
  );
  let scenario = "C04";
  try {
    const page = await browser.newPage(),
      errors = [],
      unexpected = [];
    page.on("pageerror", (e) => errors.push(e.message));
    page.on("console", (m) => {
      if (m.type() === "error") errors.push(m.text());
    });
    const allowed = [
      "index.html",
      "jtml-runtime.js",
      "components/index.js",
      "app.js",
    ].map((f) => pathToFileURL(path.join(root, f)).href);
    page.on("request", (r) => {
      if (!allowed.includes(r.url())) unexpected.push(r.url());
    });
    await page.addInitScript(() => {
      window.submits = [];
      document.addEventListener(
        "submit",
        (e) => {
          const row = e.target.closest(".task-row");
          window.submits.push({
            id: row?.dataset.taskId || "create",
            kind: e.target.className,
          });
        },
        true,
      );
      for (const name of ["fetch", "XMLHttpRequest", "WebSocket"])
        window[name] = function () {
          throw Error("unexpected " + name);
        };
    });
    await page.goto(allowed[0]);
    await page.waitForFunction(() => window.jtml?.getRuntimeRegistrySnapshot);
    const url = page.url();
    const board = page.locator('.taskboard[data-board="A"]'),
      row = (id) => board.locator('.task-row[data-task-id="' + id + '"]');
    const settle = () =>
      page.evaluate(
        () =>
          new Promise((r) =>
            requestAnimationFrame(() => requestAnimationFrame(r)),
          ),
      );
    const activate = async (locator) => {
      await locator.evaluate((e) => e.click());
      await settle();
    };
    const action = (id, name) =>
      activate(row(id).getByRole("button", { name, exact: true }));
    const model = structuredClone(seed);
    model.push({
      id: "t-105",
      title: "Audit runtime",
      completed: false,
      note: "",
    });
    let order = model.map((t) => t.id);
    const checkpoint = async (visible = order) => {
      await settle();
      const state = await page.evaluate(() => ({
        boards: jtml
          .getComponentInstances()
          .filter((i) => i.scope?.records)
          .map((i) => ({
            id: i.scope.boardId,
            records: i.scope.records,
            order: i.scope.order,
            completeIds: i.scope.completeIds,
            nextId: i.scope.nextId,
            noteSaveCount: i.scope.noteSaveCount,
            titleSaveCount: i.scope.titleSaveCount,
          })),
        dom: [...document.querySelectorAll("[data-jtml-direct-instance]")]
          .map((e) => e.getAttribute("data-jtml-direct-instance"))
          .sort(),
        registry: Object.keys(
          jtml.getRuntimeRegistrySnapshot().dynamicGenerations,
        ).sort(),
        fallback: [
          "directComponentCreateFallback",
          "directComponentPatchFallback",
          "directComponentBodyPlanFallback",
          "directComponentStaticUpdateError",
        ].filter((k) => jtml[k]),
        mode: jtml.browserBuild.mode,
      }));
      assert.deepEqual(
        state.boards.find((b) => b.id === "A").records,
        Object.fromEntries(model.map((t) => [t.id, t])),
        "canonical records independent oracle",
      );
      assert.deepEqual(state.boards.find((b) => b.id === "A").order, order);
      assert.equal(state.boards.find((b) => b.id === "A").nextId, 106);
      assert.deepEqual(
        state.boards.find((b) => b.id === "A").completeIds,
        order.filter((id) => model.find((t) => t.id === id).completed),
      );
      assert.deepEqual(
        state.boards.find((b) => b.id === "B").records,
        Object.fromEntries(seed.map((t) => [t.id, t])),
      );
      assert.deepEqual(
        state.boards.find((b) => b.id === "B").order,
        seed.map((t) => t.id),
      );
      assert.deepEqual(
        await board
          .locator(".task-row")
          .evaluateAll((es) => es.map((e) => e.dataset.taskId)),
        visible,
      );
      const complete = model.filter((t) => t.completed).length;
      for (const [n, v] of Object.entries({
        total: 5,
        open: 5 - complete,
        complete,
        visible: visible.length,
      }))
        assert.equal(
          Number(await board.locator(".count-" + n).textContent()),
          v,
        );
      for (const id of visible) {
        const t = model.find((t) => t.id === id);
        assert.equal(await row(id).locator("h2").textContent(), t.title);
        assert.equal(
          await row(id).locator(".task-status").textContent(),
          t.completed ? "Complete" : "Open",
        );
      }
      assert.deepEqual(state.dom, state.registry);
      assert.equal(new Set(state.dom).size, state.dom.length);
      assert.deepEqual(state.fallback, []);
      assert.equal(state.mode, "prod");
      assert.equal(await page.locator("release").count(), 0);
      assert.equal(page.url(), url);
      assert.deepEqual(errors, []);
      assert.deepEqual(unexpected, []);
      return state;
    };
    const retain = async (name) =>
      page.evaluate((name) => {
        const b = document.querySelector('.taskboard[data-board="A"]');
        window[name] = [
          ...b.querySelectorAll(
            ".task-row,.task-header,.task-status-detail,.task-status,.row-draft,.row-note,[data-jtml-direct-instance]",
          ),
        ];
        window[name + "Registry"] = jtml.getRuntimeRegistrySnapshot();
      }, name);
    const retained = async (name) =>
      assert.equal(
        await page.evaluate(
          (name) =>
            window[name].every((e) => e.isConnected) &&
            JSON.stringify(jtml.getRuntimeRegistrySnapshot()) ===
              JSON.stringify(window[name + "Registry"]),
          name,
        ),
        true,
        "retained DOM and generations",
      );
    await board.locator(".create-title").fill("  Audit runtime  ");
    await board.locator(".create-title").press("Enter");
    await checkpoint();
    await retain("initialRows");
    // C04: actual native Enter, invalid save, cancel; never mutate a borrowed record.
    await action("t-101", "Edit title");
    await row("t-101").locator(".row-draft").fill("Audit first");
    await row("t-101").locator(".row-draft").press("Enter");
    model[0].title = "Audit first";
    await checkpoint();
    await retained("initialRows");
    await action("t-102", "Edit title");
    await row("t-102").locator(".row-draft").fill("   ");
    await row("t-102").locator(".row-draft").press("Enter");
    await checkpoint();
    assert.equal(
      await row("t-102").locator(".row-draft").getAttribute("aria-invalid"),
      "true",
    );
    const errorId = await row("t-102")
      .locator(".row-draft")
      .getAttribute("aria-describedby");
    assert.ok(errorId);
    assert.equal(
      await page.locator('[id="' + errorId + '"]').textContent(),
      "Enter a task title",
    );
    await action("t-102", "Cancel title");
    assert.equal(
      await row("t-102").locator(".row-draft").inputValue(),
      "Audit runtime",
    );
    assert.equal(await row("t-102").getAttribute("data-editing"), "false");
    assert.equal(await row("t-102").locator(".title-error").textContent(), "");
    await checkpoint();
    await retained("initialRows");
    assert.equal(
      (await checkpoint()).boards.find((b) => b.id === "A").titleSaveCount,
      1,
    );
    results.C04 = "PASS";
    scenario = "C05";
    assert.equal(
      await row("t-101")
        .getByRole("button", { name: "Increment", exact: true })
        .count(),
      1,
    );
    const note = row("t-101").locator(".row-note");
    await note.fill("Review <release> & CSP");
    await retained("initialRows");
    await action("t-101", "Increment");
    await retained("initialRows");
    await action("t-101", "Increment");
    await retained("initialRows");
    await note.focus();
    await note.evaluate((e) => e.setSelectionRange(7, 16, "backward"));
    const before = await page.evaluate(() => submits.length);
    await note.press("Enter");
    model[0].note = "Review <release> & CSP";
    let s = await checkpoint();
    assert.equal(s.boards.find((b) => b.id === "A").noteSaveCount, 1);
    assert.equal(await page.evaluate(() => submits.length), before + 1);
    assert.deepEqual(
      await note.evaluate((e) => [
        document.activeElement === e,
        e.selectionStart,
        e.selectionEnd,
        e.selectionDirection,
      ]),
      [true, 7, 16, "backward"],
    );
    assert.equal(
      await row("t-101").locator(".interaction-count").textContent(),
      "2",
    );
    assert.equal(
      await row("t-101").locator(".saved-note").textContent(),
      model[0].note,
    );
    await retained("initialRows");
    results.C05 = "PASS";
    scenario = "C06";
    await row("t-102").locator(".row-note").fill("unsaved second");
    await action("t-102", "Increment");
    await note.focus();
    await note.evaluate((e) => e.setSelectionRange(7, 16, "backward"));
    for (let i = 0; i < 3; i++) {
      await note.press("Alt+F8");
      const n = order.indexOf("t-101");
      [order[n], order[n + 1]] = [order[n + 1], order[n]];
      await checkpoint();
      await retained("initialRows");
      assert.deepEqual(
        await note.evaluate((e) => [
          document.activeElement === e,
          e.selectionStart,
          e.selectionEnd,
          e.selectionDirection,
        ]),
        [true, 7, 16, "backward"],
      );
    }
    assert.deepEqual(order, ["t-102", "t-103", "t-104", "t-101", "t-105"]);
    assert.equal(
      await row("t-102").locator(".row-note").inputValue(),
      "unsaved second",
    );
    assert.equal(
      await row("t-102").locator(".interaction-count").textContent(),
      "1",
    );
    assert.equal(
      await row("t-101").locator(".interaction-count").textContent(),
      "2",
    );
    await row("t-102").locator(".row-note").press("Enter");
    model[1].note = "unsaved second";
    s = await checkpoint();
    assert.equal(s.boards.find((b) => b.id === "A").noteSaveCount, 2);
    await row("t-102").locator(".row-note").fill("another unsaved");
    await retained("initialRows");
    results.C06 = "PASS";
    scenario = "C07";
    assert.equal(
      await row("t-101")
        .getByRole("button", { name: "Toggle complete", exact: true })
        .count(),
      1,
    );
    await note.focus();
    await note.evaluate((e) => e.setSelectionRange(7, 16, "backward"));
    for (const completed of [true, false, true]) {
      await action("t-101", "Toggle complete");
      model[0].completed = completed;
      await checkpoint();
      await retained("initialRows");
      assert.equal(
        await row("t-101")
          .locator(".task-status-detail")
          .getAttribute("data-completed"),
        String(completed),
      );
      assert.equal(
        await row("t-101").locator(".task-status-detail").getAttribute("title"),
        completed ? "Complete" : "Open",
      );
      assert.deepEqual(
        await note.evaluate((e) => [
          document.activeElement === e,
          e.selectionStart,
          e.selectionEnd,
          e.selectionDirection,
        ]),
        [true, 7, 16, "backward"],
      );
    }
    results.C07 = "PASS";
    scenario = "C08";
    await row("t-101").locator(".row-note").fill("discard unsaved note");
    await action("t-103", "Edit title");
    await row("t-103").locator(".row-draft").fill("discard unsaved title");
    assert.equal(
      await board.getByRole("button", { name: "Open", exact: true }).count(),
      1,
    );
    await page.evaluate(() => {
      const b = document.querySelector('.taskboard[data-board="A"]');
      window.filterRows = Object.fromEntries(
        [...b.querySelectorAll(".task-row")].map((e) => [
          e.dataset.taskId,
          {
            refs: [
              e,
              ...e.querySelectorAll(
                ".row-draft,.row-note,.task-header,.task-status-detail,[data-jtml-direct-instance]",
              ),
            ],
            button: e.querySelector(".increment"),
            owner: e
              .closest("[data-jtml-direct-instance]")
              .getAttribute("data-jtml-direct-instance"),
          },
        ]),
      );
      window.beforeFilterRegistry =
        jtml.getRuntimeRegistrySnapshot().dynamicGenerations;
    });
    await note.focus();
    await activate(board.getByRole("button", { name: "Open", exact: true }));
    await checkpoint(["t-102", "t-104", "t-105"]);
    assert.equal(
      await board
        .getByRole("button", { name: "Open", exact: true })
        .evaluate((e) => document.activeElement === e),
      true,
      "focused row removal focuses filter control",
    );
    assert.equal(
      await page.evaluate(() =>
        ["t-101", "t-103"].every((id) =>
          filterRows[id].refs.every((e) => !e.isConnected),
        ),
      ),
      true,
    );
    assert.equal(
      await page.evaluate(() =>
        ["t-102", "t-104", "t-105"].every((id) =>
          filterRows[id].refs.every((e) => e.isConnected),
        ),
      ),
      true,
    );
    const removed = await page.evaluate(() => {
      const now = jtml.getRuntimeRegistrySnapshot().dynamicGenerations;
      return Object.keys(beforeFilterRegistry).filter((id) => !(id in now));
    });
    assert.equal(
      removed.length,
      6,
      "two rows and two nested components each removed",
    );
    for (const id of removed) {
      assert.equal(
        await page.evaluate(
          (id) =>
            (jtml.directComponentDestroyLifecycle.entries || []).filter(
              (e) =>
                e.id === id &&
                e.g === beforeFilterRegistry[id] &&
                e.r &&
                e.s &&
                e.e &&
                e.d,
            ).length,
          id,
        ),
        1,
      );
    }
    await page.evaluate(() => {
      filterRows["t-101"].button.click();
      filterRows["t-103"].button.click();
    });
    await checkpoint(["t-102", "t-104", "t-105"]);
    await activate(board.getByRole("button", { name: "All", exact: true }));
    await checkpoint();
    assert.equal(
      await board
        .getByRole("button", { name: "Open", exact: true })
        .evaluate((e) => document.activeElement === e),
      true,
      "remount does not steal focus",
    );
    for (const id of ["t-101", "t-103"]) {
      assert.equal(
        await row(id).locator(".interaction-count").textContent(),
        "0",
      );
      assert.equal(await row(id).getAttribute("data-editing"), "false");
      const t = model.find((t) => t.id === id);
      assert.equal(await row(id).locator(".row-draft").inputValue(), t.title);
      assert.equal(await row(id).locator(".row-note").inputValue(), t.note);
      assert.equal(
        await page.evaluate(
          (id) => filterRows[id].refs.every((e) => !e.isConnected),
          id,
        ),
        true,
      );
    }
    assert.equal(
      await row("t-102").locator(".row-note").inputValue(),
      "another unsaved",
    );
    assert.equal(
      await row("t-102").locator(".interaction-count").textContent(),
      "1",
    );
    assert.equal(
      await page.evaluate(() =>
        ["t-102", "t-104", "t-105"].every((id) =>
          filterRows[id].refs.every((e) => e.isConnected),
        ),
      ),
      true,
    );
    const fresh = await page.evaluate(() => {
      const now = jtml.getRuntimeRegistrySnapshot().dynamicGenerations;
      return Object.keys(beforeFilterRegistry).filter(
        (id) => now[id] !== beforeFilterRegistry[id],
      );
    });
    assert.deepEqual(fresh.sort(), removed.sort());
    assert.equal(
      await page.evaluate(() => {
        const now = jtml.getRuntimeRegistrySnapshot().dynamicGenerations;
        return (
          Object.keys(beforeFilterRegistry).every(
            (id) => now[id] >= beforeFilterRegistry[id],
          ) &&
          Object.keys(beforeFilterRegistry).filter(
            (id) => now[id] > beforeFilterRegistry[id],
          ).length === 6
        );
      }),
      true,
    );
    await row("t-102").locator(".row-note").press("Enter");
    model[1].note = "another unsaved";
    await checkpoint();
    assert.equal(
      (await checkpoint()).boards.find((b) => b.id === "A").noteSaveCount,
      3,
    );
    results.C08 = "PASS";
  } catch (error) {
    results[scenario] = "FAIL";
    console.error(error.stack);
    process.exitCode = 1;
  } finally {
    console.log(
      JSON.stringify({ results, excluded: "C09–C14,N01–N08,T1/T2 UNVERIFIED" }),
    );
    await browser.close();
  }
})();
