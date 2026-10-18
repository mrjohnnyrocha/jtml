// Independent continuation of the approved sequence. No expected values are
// read from emitted plans, dependency metadata, or application fixture source.
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
const seed = [
  { id: "t-101", title: "Audit runtime", completed: false, note: "" },
  { id: "t-102", title: "Audit runtime", completed: false, note: "" },
  { id: "t-103", title: "Ship documentation", completed: true, note: "" },
  { id: "t-104", title: "<release> & CSP", completed: false, note: "" },
];
(async () => {
  const browser = await chromium.launch({
    headless: true,
    executablePath: process.env.JTML_BENCH_BROWSER,
  });
  const results = Object.fromEntries(
    ["C09", "C10", "C11", "C12", "C13", "C14"].map((id) => [id, "UNVERIFIED"]),
  );
  let scenario = "C09";
  try {
    const page = await browser.newPage(),
      errors = [],
      requests = [];
    page.setDefaultTimeout(7000);
    const allowed = [
      "index.html",
      "jtml-runtime.js",
      "components/index.js",
      "app.js",
    ].map((f) => pathToFileURL(path.resolve(process.argv[2], f)).href);
    page.on("pageerror", (e) => errors.push(e.message));
    page.on("console", (m) => {
      if (m.type() === "error") errors.push(m.text());
    });
    page.on("request", (r) => {
      if (!allowed.includes(r.url())) requests.push(r.url());
    });
    await page.addInitScript(() => {
      for (const name of ["fetch", "XMLHttpRequest", "WebSocket"])
        window[name] = function () {
          throw Error("unexpected " + name);
        };
    });
    await page.goto(allowed[0]);
    await page.waitForFunction(() => window.jtml?.getRuntimeRegistrySnapshot);
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
    const activate = async (locator) => {
      await locator.evaluate((e) => e.click());
      await settle();
    };
    const action = (id, name) =>
      activate(row(id).getByRole("button", { name, exact: true }));
    let model = structuredClone(seed),
      order = model.map((t) => t.id),
      nextId = 105,
      title = "Before",
      fallback = "Fallback A",
      help = false,
      filter = "All";
    await page.evaluate(() => {
      const b = document.querySelector('.taskboard[data-board="B"]');
      window.siblingRefs = [b, ...b.querySelectorAll("*")];
      window.siblingHTML = b.outerHTML;
    });
    const state = () =>
      page.evaluate(() => ({
        boards: jtml
          .getComponentInstances()
          .filter((i) => i.scope?.records)
          .map((i) => ({
            id: i.scope.boardId,
            records: i.scope.records,
            order: i.scope.order,
            nextId: i.scope.nextId,
            title: i.scope.title,
            fallback: i.scope.fallback,
            help: i.scope.help,
          })),
        dom: [...document.querySelectorAll("[data-jtml-direct-instance]")]
          .map((e) => e.getAttribute("data-jtml-direct-instance"))
          .sort(),
        registry: jtml.getRuntimeRegistrySnapshot(),
        fallback: [
          "directComponentCreateFallback",
          "directComponentPatchFallback",
          "directComponentBodyPlanFallback",
          "directComponentStaticUpdateError",
        ].filter((k) => jtml[k]),
        mode: jtml.browserBuild.mode,
        isolated:
          siblingRefs.every((e) => e.isConnected) &&
          siblingRefs[0].outerHTML === siblingHTML,
      }));
    const checkpoint = async () => {
      await settle();
      const s = await state(),
        a = s.boards.find((b) => b.id === "A"),
        b = s.boards.find((b) => b.id === "B");
      assert.deepEqual(
        a.records,
        Object.fromEntries(model.map((t) => [t.id, t])),
        "canonical records",
      );
      assert.deepEqual(a.order, order);
      assert.equal(a.nextId, nextId);
      assert.deepEqual(
        b.records,
        Object.fromEntries(seed.map((t) => [t.id, t])),
      );
      assert.deepEqual(
        b.order,
        seed.map((t) => t.id),
      );
      assert.equal(b.nextId, 105);
      assert.equal(b.title, "Before");
      assert.equal(s.isolated, true, "Board B DOM and identity");
      const visible = order.filter(
        (id) =>
          filter === "All" ||
          model.find((t) => t.id === id).completed === (filter === "Complete"),
      );
      assert.deepEqual(
        await board
          .locator(".task-row")
          .evaluateAll((es) => es.map((e) => e.dataset.taskId)),
        visible,
      );
      const complete = model.filter((t) => t.completed).length;
      assert.equal(
        Number(await board.locator(".remaining").textContent()),
        model.length - complete,
      );
      assert.equal(
        await board.locator(".completion-label").textContent(),
        model.length === complete ? "All complete" : "Work remains",
      );
      for (const [key, value] of Object.entries({
        total: model.length,
        open: model.length - complete,
        complete,
        visible: visible.length,
      }))
        assert.equal(
          Number(await board.locator(".count-" + key).textContent()),
          value,
          key,
        );
      for (const id of visible) {
        const t = model.find((t) => t.id === id);
        assert.equal(await row(id).locator("h2").textContent(), t.title);
        assert.equal(
          await row(id).locator(".task-status").textContent(),
          t.completed ? "Complete" : "Open",
        );
        assert.equal(
          await row(id).locator(".saved-note").textContent(),
          t.note,
        );
      }
      assert.deepEqual(
        s.dom,
        Object.keys(s.registry.dynamicGenerations).sort(),
        "DOM/registry bijection",
      );
      assert.equal(new Set(s.dom).size, s.dom.length);
      if (s.fallback.length)
        console.error(
          "fallback detail",
          await page.evaluate(() =>
            Object.fromEntries(
              [
                "directComponentPatchFallback",
                "directComponentStaticUpdateError",
              ].map((k) => [k, jtml[k]]),
            ),
          ),
        );
      assert.deepEqual(s.fallback, []);
      assert.equal(s.mode, "prod");
      assert.deepEqual(errors, []);
      assert.deepEqual(requests, []);
      assert.equal(page.url(), allowed[0]);
      return s;
    };
    const capture = async (name, selector = ".task-row") =>
      page.evaluate(
        ({ name, selector }) => {
          const b = document.querySelector('.taskboard[data-board="A"]');
          window[name] = [...b.querySelectorAll(selector)].flatMap((e) => [
            e,
            ...e.querySelectorAll("*"),
          ]);
          const generations =
            jtml.getRuntimeRegistrySnapshot().dynamicGenerations;
          const ids = new Set(
            window[name]
              .map((e) =>
                e
                  .closest("[data-jtml-direct-instance]")
                  ?.getAttribute("data-jtml-direct-instance"),
              )
              .filter(Boolean),
          );
          window[name + "Generations"] = Object.fromEntries(
            [...ids].map((id) => [id, generations[id]]),
          );
        },
        { name, selector },
      );
    const retained = async (name) =>
      assert.equal(
        await page.evaluate(
          (name) =>
            window[name].every((e) => e.isConnected) &&
            Object.entries(window[name + "Generations"]).every(
              ([id, g]) =>
                jtml.getRuntimeRegistrySnapshot().dynamicGenerations[id] === g,
            ),
          name,
        ),
        true,
        "retained controls and generations",
      );
    // Reach the C08 checkpoint entirely through native application controls. The
    // original C01-C08 runner remains mandatory and independently checks details.
    await board.locator(".create-title").fill("  Audit runtime  ");
    await board.locator(".create-title").press("Enter");
    model.push({
      id: "t-105",
      title: "Audit runtime",
      completed: false,
      note: "",
    });
    order.push("t-105");
    nextId = 106;
    await action("t-101", "Edit title");
    await row("t-101").locator(".row-draft").fill("Audit first");
    await row("t-101").locator(".row-draft").press("Enter");
    model[0].title = "Audit first";
    await row("t-101").locator(".row-note").fill("Review <release> & CSP");
    await action("t-101", "Increment");
    await action("t-101", "Increment");
    await row("t-101").locator(".row-note").press("Enter");
    model[0].note = "Review <release> & CSP";
    await row("t-102").locator(".row-note").fill("unsaved second");
    await action("t-102", "Increment");
    for (let i = 0; i < 3; i++) {
      await row("t-101").locator(".row-note").press("Alt+F8");
      const n = order.indexOf("t-101");
      [order[n], order[n + 1]] = [order[n + 1], order[n]];
    }
    await row("t-102").locator(".row-note").press("Enter");
    model[1].note = "unsaved second";
    await row("t-102").locator(".row-note").fill("another unsaved");
    await action("t-101", "Toggle complete");
    model[0].completed = true;
    await activate(board.getByRole("button", { name: "Open", exact: true }));
    await activate(board.getByRole("button", { name: "All", exact: true }));
    await checkpoint();
    await capture("rowsC09");
    const diagnostics = async (branch, logical, ternary) => {
      await checkpoint();
      assert.equal(
        await board.locator(".branch-probe").getAttribute("title"),
        branch,
      );
      assert.equal(await board.locator(".branch-label").textContent(), branch);
      assert.equal(
        await board.locator(".logical-flag").textContent(),
        String(logical),
      );
      assert.equal(
        await board.locator(".ternary-label").textContent(),
        ternary,
      );
      assert.equal(
        Number(await board.locator(".remaining").textContent()),
        model.filter((t) => !t.completed).length,
      );
      assert.equal(
        await board.locator(".completion-label").textContent(),
        "Work remains",
      );
      for (const id of order) {
        assert.equal(
          await row(id).locator(".board-heading").textContent(),
          title,
        );
        assert.equal(
          await row(id).locator(".task-header").getAttribute("title"),
          title,
        );
        assert.equal(
          await row(id)
            .locator(".task-status-detail")
            .getAttribute("data-board-title"),
          title,
        );
        assert.equal(
          await row(id).locator(".board-heading").getAttribute("title"),
          title,
        );
      }
      assert.equal(
        await sibling.locator(".branch-label").textContent(),
        "Fallback A",
      );
      await retained("rowsC09");
    };
    const rename = async (value) => {
      await board.getByLabel("Board title", { exact: true }).fill(value);
      await board.getByLabel("Board title", { exact: true }).press("Enter");
      title = value;
    };
    const toggleHelp = async () => {
      await activate(
        board.getByRole("button", { name: "Toggle help", exact: true }),
      );
      help = !help;
    };
    // C09 exact six-step Before/After/Final sequence and inactive fallback write.
    await rename("After");
    await diagnostics("After", false, "Fallback A");
    await activate(
      board
        .locator(".branch-probe")
        .getByRole("button", { name: "Increment branch", exact: true }),
    );
    await page.evaluate(
      () =>
        (window.afterBranch = document.querySelector(
          '.taskboard[data-board="A"] .branch-probe',
        )),
    );
    await activate(
      board.getByRole("button", { name: "Change fallback", exact: true }),
    );
    fallback = "Fallback B";
    await diagnostics("After", false, "Fallback B");
    assert.equal(await board.locator(".branch-count").textContent(), "1");
    await toggleHelp();
    await diagnostics("After", true, "After");
    assert.equal(await page.evaluate(() => afterBranch.isConnected), true);
    await rename("Before");
    await diagnostics("Fallback B", false, "Before");
    assert.equal(await page.evaluate(() => afterBranch.isConnected), false);
    await activate(
      board
        .locator(".branch-probe")
        .getByRole("button", { name: "Increment branch", exact: true }),
    );
    await page.evaluate(
      () =>
        (window.fallbackBranch = document.querySelector(
          '.taskboard[data-board="A"] .branch-probe',
        )),
    );
    await rename("Final");
    await diagnostics("Fallback B", false, "Final");
    assert.equal(await page.evaluate(() => fallbackBranch.isConnected), true);
    assert.equal(await board.locator(".branch-count").textContent(), "1");
    await toggleHelp();
    await diagnostics("Fallback B", false, "Fallback B");
    assert.equal(await page.evaluate(() => fallbackBranch.isConnected), true);
    assert.equal(await board.locator(".branch-count").textContent(), "1");
    results.C09 = "PASS";
    scenario = "C10";
    await toggleHelp();
    await checkpoint();
    await activate(
      board
        .locator(".help")
        .getByRole("button", { name: "Increment help", exact: true }),
    );
    await capture("activeHelp", ".help");
    await capture("tasksC10");
    assert.equal(await board.locator(".help-count").textContent(), "1");
    await row("t-104").locator(".row-note").fill("");
    await row("t-104").locator(".row-note").press("Enter");
    await checkpoint();
    await retained("activeHelp");
    assert.equal(await board.locator(".help-count").textContent(), "1");
    for (let cycle = 0; cycle < 2; cycle++) {
      await page.evaluate(() => {
        const h = document.querySelector('.taskboard[data-board="A"] .help');
        window.oldHelp = h;
        window.oldHelpOwner = h
          .closest("[data-jtml-direct-instance]")
          .getAttribute("data-jtml-direct-instance");
        window.oldHelpGeneration =
          jtml.getRuntimeRegistrySnapshot().dynamicGenerations[oldHelpOwner];
      });
      await toggleHelp();
      await checkpoint();
      assert.equal(await board.locator(".help").count(), 0);
      assert.equal(
        await page.evaluate(
          () =>
            !oldHelp.isConnected &&
            !jtml.getRuntimeRegistrySnapshot().dynamicGenerations[
              oldHelpOwner
            ] &&
            !jtml
              .getRuntimeRegistrySnapshot()
              .handlerOwnerIds.includes(oldHelpOwner),
        ),
        true,
        "Help cleanup",
      );
      assert.equal(
        await page.evaluate(
          () =>
            (jtml.directComponentDestroyLifecycle.entries || []).filter(
              (e) =>
                e.id === oldHelpOwner &&
                e.g === oldHelpGeneration &&
                e.r &&
                e.s &&
                e.e &&
                e.d,
            ).length,
        ),
        1,
      );
      await toggleHelp();
      await checkpoint();
      assert.equal(await board.locator(".help-count").textContent(), "0");
      assert.equal(
        await page.evaluate(
          () =>
            jtml.getRuntimeRegistrySnapshot().dynamicGenerations[oldHelpOwner] >
            oldHelpGeneration,
        ),
        true,
        "fresh Help generation",
      );
      await retained("tasksC10");
      await activate(
        board
          .locator(".help")
          .getByRole("button", { name: "Increment help", exact: true }),
      );
    }
    results.C10 = "PASS";
    scenario = "C11";
    await page.evaluate(() => {
      const r = document.querySelector(
        '.taskboard[data-board="A"] .task-row[data-task-id="t-103"]',
      );
      window.deletedRow = r;
      window.detachedControls = [...r.querySelectorAll("button,input")];
      const wrapper = r.closest("[data-jtml-direct-instance]");
      window.deletedGenerations = Object.fromEntries(
        [
          wrapper,
          ...wrapper.querySelectorAll("[data-jtml-direct-instance]"),
        ].map((e) => {
          const id = e.getAttribute("data-jtml-direct-instance");
          return [id, jtml.getRuntimeRegistrySnapshot().dynamicGenerations[id]];
        }),
      );
      window.survivors = [
        ...document.querySelectorAll('.taskboard[data-board="A"] .task-row'),
      ]
        .filter((e) => e !== r)
        .flatMap((e) => [e, ...e.querySelectorAll("*")]);
    });
    await capture("survivingC11", '.task-row:not([data-task-id="t-103"])');
    await row("t-103").locator(".row-note").focus();
    await action("t-103", "Delete task");
    model = model.filter((t) => t.id !== "t-103");
    order = order.filter((id) => id !== "t-103");
    await checkpoint();
    await retained("survivingC11");
    assert.deepEqual(order, ["t-102", "t-104", "t-101", "t-105"]);
    assert.equal(
      await row("t-104")
        .getByRole("button", { name: "Edit title", exact: true })
        .evaluate((e) => document.activeElement === e),
      true,
      "deletion focuses next primary edit control",
    );
    assert.equal(
      await page.evaluate(
        () =>
          !deletedRow.isConnected &&
          survivors.every((e) => e.isConnected) &&
          Object.keys(deletedGenerations).every(
            (id) =>
              !jtml.getRuntimeRegistrySnapshot().dynamicGenerations[id] &&
              !jtml.getRuntimeRegistrySnapshot().handlerOwnerIds.includes(id),
          ),
      ),
      true,
    );
    assert.equal(
      await page.evaluate(() =>
        Object.entries(deletedGenerations).every(
          ([id, g]) =>
            (jtml.directComponentDestroyLifecycle.entries || []).filter(
              (e) => e.id === id && e.g === g && e.r && e.s && e.e && e.d,
            ).length === 1,
        ),
      ),
      true,
    );
    const afterDelete = await state();
    await page.evaluate(() => detachedControls.forEach((e) => e.click()));
    await checkpoint();
    assert.deepEqual(
      await state(),
      afterDelete,
      "detached controls cannot dispatch",
    );
    assert.equal(
      await row("t-102").locator(".row-note").inputValue(),
      "another unsaved",
    );
    assert.equal(
      await row("t-102").locator(".interaction-count").textContent(),
      "1",
    );
    results.C11 = "PASS";
    scenario = "C12";
    await capture("beforeCreate");
    await board.locator(".create-title").fill("Final task");
    await board.locator(".create-title").press("Enter");
    model.push({
      id: "t-106",
      title: "Final task",
      completed: false,
      note: "",
    });
    order.push("t-106");
    nextId = 107;
    await checkpoint();
    await retained("beforeCreate");
    assert.deepEqual(order, ["t-102", "t-104", "t-101", "t-105", "t-106"]);
    await capture("boundaryRows");
    for (const [id, key] of [
      ["t-102", "Alt+F7"],
      ["t-106", "Alt+F8"],
    ]) {
      const input = row(id).locator(".row-note");
      if (id === "t-106") await input.fill("unsaved boundary");
      await input.focus();
      await input.evaluate((e) => e.setSelectionRange(1, 5, "backward"));
      const before = await state();
      await input.press(key);
      await checkpoint();
      await retained("boundaryRows");
      assert.deepEqual(
        await state(),
        before,
        "boundary move is a strict no-op",
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
    }
    assert.equal(
      await row("t-102").locator(".row-note").inputValue(),
      "another unsaved",
    );
    assert.equal(
      await row("t-102").locator(".interaction-count").textContent(),
      "1",
    );
    results.C12 = "PASS";
    scenario = "C13";
    for (const id of ["t-102", "t-104", "t-105", "t-106"]) {
      await action(id, "Toggle complete");
      model.find((t) => t.id === id).completed = true;
      await checkpoint();
      await retained("boundaryRows");
    }
    assert.equal(
      await board.locator(".completion-label").textContent(),
      "All complete",
    );
    await page.evaluate(() => {
      window.beforeCompleteFilter = [
        ...document.querySelectorAll('.taskboard[data-board="A"] .task-row'),
      ];
      window.beforeCompleteRegistry =
        jtml.getRuntimeRegistrySnapshot().dynamicGenerations;
    });
    await activate(board.getByRole("button", { name: "Open", exact: true }));
    filter = "Open";
    await checkpoint();
    assert.equal(
      await board.locator(".empty-filter").textContent(),
      "No tasks match this filter",
    );
    assert.equal(
      await board.locator(".empty-board").count(),
      0,
      "empty filter is not empty board",
    );
    assert.equal(
      await page.evaluate(() =>
        beforeCompleteFilter.every((e) => !e.isConnected),
      ),
      true,
    );
    const removedByFilter = await page.evaluate(() =>
      Object.keys(beforeCompleteRegistry).filter(
        (id) => !(id in jtml.getRuntimeRegistrySnapshot().dynamicGenerations),
      ),
    );
    assert.equal(removedByFilter.length, 15);
    await activate(
      board.getByRole("button", { name: "Complete", exact: true }),
    );
    filter = "Complete";
    await checkpoint();
    assert.equal(await board.locator(".empty-filter").count(), 0);
    assert.equal(
      await page.evaluate(
        (ids) =>
          ids.every(
            (id) =>
              jtml.getRuntimeRegistrySnapshot().dynamicGenerations[id] >
              beforeCompleteRegistry[id],
          ),
        removedByFilter,
      ),
      true,
      "Complete remounts fresh generations",
    );
    for (const id of order) {
      assert.equal(
        await row(id).locator(".interaction-count").textContent(),
        "0",
      );
      assert.equal(
        await row(id).locator(".row-note").inputValue(),
        model.find((t) => t.id === id).note,
      );
    }
    results.C13 = "PASS";
    scenario = "C14";
    await activate(board.getByRole("button", { name: "All", exact: true }));
    filter = "All";
    await checkpoint();
    await page.evaluate(() => {
      window.allOldRows = [
        ...document.querySelectorAll('.taskboard[data-board="A"] .task-row'),
      ];
      window.oldTaskOwners = allOldRows.flatMap((r) => {
        const w = r.closest("[data-jtml-direct-instance]");
        return [w, ...w.querySelectorAll("[data-jtml-direct-instance]")].map(
          (e) => e.getAttribute("data-jtml-direct-instance"),
        );
      });
      window.allDetachedButtons = allOldRows.flatMap((r) => [
        ...r.querySelectorAll("button"),
      ]);
    });
    // Delete last first to exercise the previous-row focus boundary as well as
    // next-row focus and the empty-board create-input destination.
    for (const id of ["t-106", "t-102", "t-104", "t-101", "t-105"]) {
      const index = order.indexOf(id),
        remainingOrder = order.filter((t) => t !== id),
        expectedFocus =
          remainingOrder[Math.min(index, remainingOrder.length - 1)];
      await capture(
        "deletionSurvivors",
        '.task-row:not([data-task-id="' + id + '"])',
      );
      await row(id).locator(".row-note").focus();
      await action(id, "Delete task");
      model = model.filter((t) => t.id !== id);
      order = remainingOrder;
      await checkpoint();
      await retained("deletionSurvivors");
      const focused = expectedFocus
        ? row(expectedFocus).getByRole("button", {
            name: "Edit title",
            exact: true,
          })
        : board.locator(".create-title");
      assert.equal(
        await focused.evaluate((e) => e === document.activeElement),
        true,
        "deletion focus destination",
      );
    }
    assert.equal(await board.locator(".empty-board").textContent(), "No tasks");
    assert.equal(await board.locator(".empty-filter").count(), 0);
    assert.equal(
      await page.evaluate(
        () =>
          allOldRows.every((e) => !e.isConnected) &&
          oldTaskOwners.every(
            (id) =>
              !(id in jtml.getRuntimeRegistrySnapshot().dynamicGenerations) &&
              !jtml.getRuntimeRegistrySnapshot().handlerOwnerIds.includes(id),
          ),
      ),
      true,
      "all task registrations and handlers released",
    );
    const emptyState = await state();
    await page.evaluate(() => allDetachedButtons.forEach((e) => e.click()));
    await checkpoint();
    assert.deepEqual(
      await state(),
      emptyState,
      "empty board cannot be mutated by detached controls",
    );
    await board.locator(".create-title").fill("Restarted task");
    await board.locator(".create-title").press("Enter");
    model = [
      { id: "t-107", title: "Restarted task", completed: false, note: "" },
    ];
    order = ["t-107"];
    nextId = 108;
    await checkpoint();
    assert.equal(await board.locator(".empty-board").count(), 0);
    assert.equal(
      await row("t-107").locator(".interaction-count").textContent(),
      "0",
    );
    assert.equal(
      await page.evaluate(() => {
        const r = document.querySelector(
          '.taskboard[data-board="A"] .task-row',
        );
        return !oldTaskOwners.includes(
          r
            .closest("[data-jtml-direct-instance]")
            .getAttribute("data-jtml-direct-instance"),
        );
      }),
      true,
      "new identity is never reused",
    );
    await page.reload();
    await settle();
    const reloaded = await page.evaluate(() => ({
      boards: jtml
        .getComponentInstances()
        .filter((i) => i.scope?.records)
        .map((i) => ({
          id: i.scope.boardId,
          records: i.scope.records,
          order: i.scope.order,
          nextId: i.scope.nextId,
          title: i.scope.title,
          fallback: i.scope.fallback,
          help: i.scope.help,
        })),
      dom: [...document.querySelectorAll("[data-jtml-direct-instance]")]
        .map((e) => e.getAttribute("data-jtml-direct-instance"))
        .sort(),
      registry: Object.keys(
        jtml.getRuntimeRegistrySnapshot().dynamicGenerations,
      ).sort(),
    }));
    for (const b of reloaded.boards) {
      assert.deepEqual(
        b.records,
        Object.fromEntries(seed.map((t) => [t.id, t])),
      );
      assert.deepEqual(
        b.order,
        seed.map((t) => t.id),
      );
      assert.equal(b.nextId, 105);
      assert.equal(b.title, "Before");
      assert.equal(b.fallback, "Fallback A");
      assert.equal(b.help, false);
    }
    assert.equal(reloaded.boards.length, 2);
    assert.deepEqual(reloaded.dom, reloaded.registry);
    for (const b of [board, sibling]) {
      assert.deepEqual(
        await b
          .locator(".task-row")
          .evaluateAll((es) => es.map((e) => e.dataset.taskId)),
        seed.map((t) => t.id),
      );
      for (const [name, value] of Object.entries({
        total: 4,
        open: 3,
        complete: 1,
        visible: 4,
      }))
        assert.equal(
          Number(await b.locator(".count-" + name).textContent()),
          value,
        );
    }
    assert.deepEqual(errors, []);
    assert.deepEqual(requests, []);
    results.C14 = "PASS";
  } catch (error) {
    results[scenario] = "FAIL";
    console.error(error.stack);
    process.exitCode = 1;
  } finally {
    console.log(
      JSON.stringify({ results, excluded: "N01–N08,T1/T2 UNVERIFIED" }),
    );
    await browser.close();
  }
})().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
