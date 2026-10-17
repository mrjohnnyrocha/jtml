// Independent C01–C03 oracle. Expected data comes from the approved contract,
// never from the generated manifest, read sets, or the application seed source.
const assert = require('node:assert/strict');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const { chromium } = require(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT
  ? path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT, 'node_modules/playwright')
  : 'playwright');
const root = path.resolve(process.argv[2]);
const seed = [
  { id: 't-101', title: 'Audit runtime', completed: false, note: '' },
  { id: 't-102', title: 'Audit runtime', completed: false, note: '' },
  { id: 't-103', title: 'Ship documentation', completed: true, note: '' },
  { id: 't-104', title: '<release> & CSP', completed: false, note: '' }
];
async function retainedControlRegression(browser) {
  const page = await browser.newPage();
  const errors = [];
  page.on('pageerror', error => errors.push(error.message));
  try {
    await page.goto(pathToFileURL(path.join(root, 'retained-control/index.html')).href);
    const input = page.locator('.editor').first().getByLabel('Draft');
    const form = page.locator('.editor').first();
    await page.waitForFunction(() => window.jtml && window.jtml.getRuntimeRegistrySnapshot);
    await page.evaluate(() => { window.retainedControls = [...document.querySelectorAll('input,textarea,select')]; });
    for (const [index, title] of ['Alpha', 'Beta'].entries()) {
      await input.fill(title);
      await input.evaluate(el => {
        el.setSelectionRange(1, 3, 'backward');
        el.dispatchEvent(new Event('input', { bubbles: true }));
      });
      assert.deepEqual(await input.evaluate(el => [el.selectionStart, el.selectionEnd, el.selectionDirection]),
        [1, 3, 'backward'], 'unchanged value patches preserve selection');
      await form.getByLabel('Note').fill('Edited note');
      await form.getByLabel('Kind').selectOption('one');
      await input.press('Enter');
      await page.waitForFunction(count => document.querySelector('.attempts').textContent === String(count), index + 1);
      assert.equal(await form.locator('.saved').textContent(), title);
      assert.equal(await input.inputValue(), '', 'dirty input live value clears');
      assert.equal(await form.getByLabel('Note').inputValue(), '', 'dirty textarea live value clears');
      assert.equal(await form.getByLabel('Kind').inputValue(), 'two', 'select property follows state');
      assert.equal(await input.getAttribute('aria-invalid'), 'true', 'ARIA boolean is a string value');
      assert.equal(await page.locator('.editor').nth(1).getByLabel('Draft').inputValue(), 'Seed');
      assert.equal(await page.evaluate(() => window.retainedControls.every(el => el.isConnected &&
        el.getAttribute('data-jtml-direct-component-id') && el.getAttribute('data-jtml-direct-body-node'))), true,
        'controls retain DOM, owner and patch markers across repeated edits');
    }
    assert.deepEqual(errors, []);
    assert.equal(await page.evaluate(() => !!(window.jtml.directComponentCreateFallback ||
      window.jtml.directComponentPatchFallback || window.jtml.directComponentBodyPlanFallback ||
      window.jtml.directComponentStaticUpdateError)), false);
  } finally { await page.close(); }
}
(async () => {
  let browser;
  const results = { controls: 'UNVERIFIED', C01: 'UNVERIFIED', C02: 'UNVERIFIED', C03: 'UNVERIFIED' };
  let scenario = 'controls';
  try {
    browser = await chromium.launch({ headless: true,
      executablePath: process.env.JTML_BENCH_BROWSER || undefined,
      args: ['--disable-gpu', '--no-sandbox', '--disable-background-networking'] });
    await retainedControlRegression(browser);
    results.controls = 'PASS'; scenario = 'C01';
    const page = await browser.newPage();
    const errors = [], unexpected = [];
    page.on('pageerror', error => errors.push(error.message));
    page.on('console', message => { if (message.type() === 'error') errors.push(message.text()); });
    page.on('request', request => {
      const allowed = ['index.html', 'jtml-runtime.js', 'components/index.js', 'app.js']
        .map(name => pathToFileURL(path.join(root, name)).href);
      if (!allowed.includes(request.url())) unexpected.push(request.url());
    });
    await page.addInitScript(() => {
      window.taskboardObserved = { submits: [], activity: [] };
      for (const name of ['fetch', 'XMLHttpRequest', 'WebSocket']) {
        window[name] = function () {
          window.taskboardObserved.activity.push(name);
          throw new Error('unexpected application ' + name);
        };
      }
      document.addEventListener('submit', event => {
        const board = event.target.closest('.taskboard');
        window.taskboardObserved.submits.push(board && board.getAttribute('data-board'));
      }, true);
    });
    await page.goto(pathToFileURL(path.join(root, 'index.html')).href);
    await page.waitForFunction(() => document.querySelectorAll('.taskboard').length === 2 &&
      window.jtml && window.jtml.getRuntimeRegistrySnapshot);
    const initialUrl = page.url();
    const settle = () => page.evaluate(() => new Promise(resolve =>
      requestAnimationFrame(() => requestAnimationFrame(resolve))));
    const checkpoint = async (expectedA, nextId, submits, draft = '') => {
      const state = await page.evaluate(() => {
        const boards = [...document.querySelectorAll('.taskboard')].map(board => {
          const wrapper = board.closest('[data-jtml-instance], [data-jtml-direct-instance]');
          const id = wrapper.getAttribute('data-jtml-instance') || wrapper.getAttribute('data-jtml-direct-instance');
          const instance = window.jtml.findComponentInstance(id);
          const scope = instance.scope;
          return { board: board.getAttribute('data-board'), records: scope.records,
            order: scope.order, completeIds: scope.completeIds, nextId: scope.nextId,
            submitCount: scope.submitCount, draft: scope.draft,
            rows: [...board.querySelectorAll('.task-row')].map(row => ({
              id: row.getAttribute('data-task-id'), title: row.querySelector('h2').textContent,
              status: row.querySelector('.task-status').textContent,
              heading: row.querySelector('.board-heading').textContent,
              draft: row.querySelector('.row-draft').value,
              key: row.closest('[data-jtml-direct-list-key]').getAttribute('data-jtml-direct-list-key')
            })), counts: ['total', 'open', 'complete', 'visible'].map(name =>
              Number(board.querySelector('.count-' + name).textContent)) };
        });
        const a = window.jtml.getComponentInstances().find(i => i.scope && i.scope.boardId === 'A').scope;
        const b = window.jtml.getComponentInstances().find(i => i.scope && i.scope.boardId === 'B').scope;
        return { boards, isolated: a.records !== b.records && a.order !== b.order &&
          Object.keys(b.records).every(id => a.records[id] !== b.records[id]),
          activity: window.taskboardObserved.activity,
          fallbacks: ['directComponentCreateFallback', 'directComponentPatchFallback',
            'directComponentBodyPlanFallback', 'directComponentStaticUpdateError'].filter(k => window.jtml[k]),
          mode: window.jtml.browserBuild.mode,
          legacy: !!window.__jtml_static_update_plans,
          injected: document.querySelectorAll('release').length };
      });
      assert.equal(state.isolated, true, 'boards own separate collections and objects');
      assert.equal(state.mode, 'prod');
      assert.equal(state.legacy, false);
      assert.deepEqual(state.activity, []);
      assert.deepEqual(state.fallbacks, [], 'no production fallback');
      assert.equal(state.injected, 0);
      assert.equal(await page.evaluate(() => {
        if (!window.taskboardInitialRegistry) return true;
        const current = window.jtml.getRuntimeRegistrySnapshot().dynamicGenerations;
        const initial = window.taskboardInitialRegistry.dynamicGenerations;
        return Object.keys(initial).every(id => current[id] === initial[id]) &&
          window.taskboardRetained.every(element => element.isConnected);
      }), true, 'retained C01 rows, controls and registry generations survive creation');
      for (const [index, expected] of [expectedA, seed].entries()) {
        const board = state.boards[index];
        assert.equal(board.board, index === 0 ? 'A' : 'B');
        assert.deepEqual(board.order, expected.map(task => task.id));
        assert.deepEqual(board.records, Object.fromEntries(expected.map(task => [task.id, task])));
        assert.deepEqual(board.completeIds, expected.filter(task => task.completed).map(task => task.id));
        assert.equal(board.nextId, index === 0 ? nextId : 105);
        assert.equal(board.submitCount, index === 0 ? submits : 0);
        assert.equal(board.draft, index === 0 ? draft : '');
        const complete = expected.filter(task => task.completed).length;
        assert.deepEqual(board.counts, [expected.length, expected.length - complete, complete, expected.length]);
        assert.deepEqual(board.rows.map(({id,title,status,heading,draft}) => ({id,title,status,heading,draft})),
          expected.map(task => ({ id: task.id, title: task.title,
            status: task.completed ? 'Complete' : 'Open', heading: 'Before', draft: task.title })));
        assert.equal(new Set(board.rows.map(row => row.key)).size, expected.length, 'distinct keyed rows');
      }
      assert.equal(page.url(), initialUrl, 'native submission never navigates');
      assert.deepEqual(errors, []);
      assert.deepEqual(unexpected, []);
    };
    await checkpoint(seed, 105, 0);
    await page.evaluate(() => {
      window.taskboardRetained = [...document.querySelectorAll('.task-row, .task-header, .task-status, .row-draft, .create-task, .create-title')];
      window.taskboardInitialRegistry = window.jtml.getRuntimeRegistrySnapshot();
      window.taskboardInitialIds = [...document.querySelectorAll('.task-row')].map(row =>
        row.closest('[data-jtml-direct-instance]').getAttribute('data-jtml-direct-instance'));
    });
    assert.equal(await page.evaluate(() => new Set(window.taskboardInitialIds).size), 8,
      'same seed keys on sibling boards have separate component identities');
    results.C01 = 'PASS'; scenario = 'C02';
    const board = page.locator('.taskboard[data-board="A"]');
    const input = board.locator('.create-title');
    for (const [value, enter] of [['', true], ['   ', false], ['\t\u00a0 ', true]]) {
      await input.fill(value);
      assert.equal(await page.evaluate(() => window.jtml.getComponentInstances()
        .find(i => i.scope && i.scope.boardId === 'A').scope.draft), value,
        'each new edit reaches the canonical form draft');
      if (enter) await input.press('Enter');
      else await board.getByRole('button', { name: 'Create task', exact: true }).click();
      await settle();
      const count = await page.evaluate(() => window.taskboardObserved.submits.length);
      await checkpoint(seed, 105, count, value);
      assert.equal(count, ['','   ','\t\u00a0 '].indexOf(value) + 1);
      assert.equal(await input.getAttribute('aria-invalid'), 'true');
      const errorId = await input.getAttribute('aria-describedby');
      assert.ok(errorId);
      assert.equal(await board.locator('[id="' + errorId + '"]').textContent(), 'Enter a task title');
    }
    results.C02 = 'PASS'; scenario = 'C03';
    await input.fill('  Audit runtime  ');
    await input.press('Enter');
    await settle();
    await checkpoint([...seed, { id: 't-105', title: 'Audit runtime', completed: false, note: '' }], 106, 4);
    assert.equal(await input.inputValue(), '');
    assert.equal(await input.getAttribute('aria-invalid'), 'false');
    assert.equal(await board.locator('.create-error').textContent(), '');
    assert.deepEqual(await page.evaluate(() => window.taskboardObserved.submits), ['A','A','A','A']);
    assert.equal(await page.evaluate(() => window.taskboardRetained.every(element => element.isConnected)), true);
    await input.fill('Next draft');
    await settle();
    assert.equal(await input.inputValue(), 'Next draft', 'next edit remains functional');
    assert.equal(await page.evaluate(() => window.jtml.getComponentInstances()
      .find(i => i.scope && i.scope.boardId === 'A').scope.draft), 'Next draft');
    results.C03 = 'PASS';
    console.log(JSON.stringify({ results, browser: browser.version(),
      playwright: require(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT
        ? path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT, 'node_modules/playwright/package.json')
        : 'playwright/package.json').version, excluded: 'C04–C14, N01–N08, T1/T2 UNVERIFIED' }));
  } catch (error) {
    results[scenario] = 'FAIL';
    console.error(JSON.stringify({ results, error: error.stack }));
    process.exitCode = 1;
  } finally {
    if (browser) await browser.close();
  }
})();
