const assert = require('node:assert/strict');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const { chromium } = require(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT
  ? path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT, 'node_modules/playwright') : 'playwright');
(async () => {
  const browser = await chromium.launch({ headless: true, executablePath: process.env.JTML_BENCH_BROWSER || undefined });
  const results = {};
  try {
    for (const name of ['partial', 'nested']) {
      const page = await browser.newPage();
      const errors = [], unexpected = [];
      const assetRoot = path.resolve(process.argv[2], name);
      const allowed = ['index.html','jtml-runtime.js','components/index.js','app.js'].map(file => pathToFileURL(path.join(assetRoot,file)).href);
      page.on('request', request => { if (!allowed.includes(request.url())) unexpected.push(request.url()); });
      page.on('pageerror', error => errors.push(error.message));
      page.on('console', message => { if (message.type() === 'error') errors.push(message.text()); });
      try {
        await page.addInitScript(() => {
          window.keyedNetwork = [];
          for (const name of ['fetch', 'XMLHttpRequest', 'WebSocket']) window[name] = function () { window.keyedNetwork.push(name); throw Error('unexpected ' + name); };
        });
        await page.goto(pathToFileURL(path.resolve(process.argv[2], name, 'index.html')).href);
        await page.waitForFunction(() => window.jtml && document.querySelectorAll('.row').length === 4);
        const board = page.locator('.board[data-board="A"]');
        const row = id => board.locator('.row[data-id="' + id + '"]');
        const counter = id => row(id).locator('.counter p');
        const action = async label => {
          // Programmatic activation preserves the focused draft during reconciliation.
          await board.getByRole('button', { name: label, exact: true }).evaluate(button => button.click());
          await page.evaluate(() => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve))));
        };
        const check = async () => {
          assert.deepEqual(errors, [], 'no silent runtime errors');
          assert.deepEqual(unexpected, []);
          assert.deepEqual(await page.evaluate(() => window.keyedNetwork), []);
          const state = await page.evaluate(() => {
            const j = window.jtml, registry = j.getRuntimeRegistrySnapshot();
            return { registry, dom: [...document.querySelectorAll('[data-jtml-direct-instance]')]
              .map(el => el.getAttribute('data-jtml-direct-instance')).sort(),
              fallback: ['directComponentPatchFallback','directComponentCreateFallback','directComponentBodyPlanFallback',
                'directComponentStaticUpdateError','directComponentStaticDestroyError'].filter(k => j[k]),
              mode: j.browserBuild.mode };
          });
          assert.deepEqual(Object.keys(state.registry.dynamicGenerations).sort(), state.dom,
            'every live component is registered, every registered component is live');
          assert.equal(new Set(state.dom).size, state.dom.length, 'no duplicate identity');
          assert.deepEqual(state.fallback, []);
          assert.equal(state.mode, 'prod');
          assert.equal(await page.locator('.board[data-board="B"] .counter').count(), 2);
          assert.deepEqual(await page.locator('.board[data-board="B"] .counter p').allTextContents(), ['0','0']);
          assert.deepEqual(await page.locator('.board[data-board="B"] input').evaluateAll(els => els.map(el => el.value)), ['Seed','Seed']);
          return state.registry.dynamicGenerations;
        };
        const initial = await check();
        await row('a').getByRole('button', { name: 'Increment', exact: true }).click();
        assert.equal(await counter('a').textContent(), '1');
        await row('a').locator('input').fill('Unsaved');
        await row('a').locator('input').evaluate(el => el.setSelectionRange(1, 5, 'backward'));
        await page.evaluate(() => {
          const board = document.querySelector('.board[data-board="A"]');
          window.keptRows = [...board.querySelectorAll('.row, input')];
          window.oldCounters = [...board.querySelectorAll('.counter')].map(el => ({ el,
            wrapper: el.closest('[data-jtml-direct-instance]'), button: el.querySelector('button') }));
        });
        for (let cycle = 0; cycle < 3; cycle++) {
          const before = await check();
          await action('Update');
          assert.deepEqual(await board.locator('.row').evaluateAll(els => els.map(el => el.dataset.id)), ['b','a']);
          assert.equal(await page.evaluate(() => window.keptRows.every(el => el.isConnected)), true, 'rows and drafts retained');
          assert.equal(await row('a').locator('input').inputValue(), 'Unsaved');
          if (name === 'partial') {
            assert.deepEqual(await check(), initial, 'retained registrations and generations unchanged');
            assert.equal(await page.evaluate(() => window.oldCounters.every(({el,wrapper}) => el.isConnected && wrapper.isConnected)), true);
            assert.deepEqual(await row('a').locator('input').evaluate(el => [document.activeElement === el, el.selectionStart, el.selectionEnd, el.selectionDirection]),
              [true,1,5,'backward'], 'focus and selection retained');
            await row('a').getByRole('button', { name: 'Increment', exact: true }).click();
            assert.equal(await counter('a').textContent(), String(cycle + 2), 'subsequent action dispatches exactly once');
          } else {
            assert.equal(await board.locator('.counter').count(), 0);
            const hidden = await check();
            assert.equal(Object.keys(hidden).length, 6, 'only two retained A rows plus untouched B rows/counters');
            const removed = Object.keys(before).filter(id => !Object.hasOwn(hidden, id));
            assert.equal(removed.length, 2);
            for (const id of removed) {
              const entries = await page.evaluate(({id,g}) => (window.jtml.directComponentDestroyLifecycle.entries || [])
                .filter(entry => entry.id === id && entry.g === g), { id, g: before[id] });
              assert.equal(entries.length, 1, 'removed child generation destroyed exactly once');
              assert.ok(entries[0].r && entries[0].s && entries[0].e && entries[0].d);
            }
          }
          if (name === 'nested') {
            assert.deepEqual(await row('a').locator('input').evaluate(el => [document.activeElement === el, el.selectionStart, el.selectionEnd, el.selectionDirection]),
              [true,1,5,'backward'], 'focus and selection retained');
          }
          await action('Restore');
          const restored = await check();
          assert.equal(await row('a').locator('input').inputValue(), 'Unsaved');
          if (name === 'nested') {
            assert.deepEqual(await board.locator('.counter p').allTextContents(), ['0','0'], 'fresh counters after conditional remount');
            const fresh = Object.keys(before).filter(id => restored[id] !== before[id]);
            assert.equal(fresh.length, 2, 'only the removed A counters change generation');
            for (const id of fresh) assert.ok(restored[id] > before[id]);
            await page.evaluate(() => window.oldCounters.forEach(({button}) => button.click()));
            assert.deepEqual(await board.locator('.counter p').allTextContents(), ['0','0'], 'detached stale buttons are inert');
            await row('a').getByRole('button', { name: 'Increment', exact: true }).click();
            assert.equal(await counter('a').textContent(), '1', 'fresh handler dispatches once');
          }
          await row('a').locator('input').focus();
          await row('a').locator('input').evaluate(el => el.setSelectionRange(1, 5, 'backward'));
        }
        const beforeRemoval = await check();
        await action('Remove B');
        const afterRemoval = await check();
        assert.equal(Object.keys(beforeRemoval).length - Object.keys(afterRemoval).length, 2, 'genuine keyed removal destroys row and counter');
        assert.equal(await row('a').locator('input').inputValue(), 'Unsaved');
        await action('Restore');
        await check();
        if (name === 'partial') {
          // Force an unsupported retained shape in the SECOND next row. The first
          // row can be fully planned, while rendering also tentatively registers c.
          await page.evaluate(() => {
            const board = document.querySelector('.board[data-board="A"]');
            const root = board.querySelector('.row[data-id="a"]').closest('[data-jtml-direct-instance]');
            const section = document.createElement('section');
            for (const attr of root.attributes) section.setAttribute(attr.name, attr.value);
            section.append(...root.childNodes); root.replaceWith(section);
            window.beforeRejected = { html: board.innerHTML, registry: window.jtml.getRuntimeRegistrySnapshot(),
              destroyed: window.jtml.directComponentDestroyLifecycle.count,
              lifecycle: JSON.stringify(window.jtml.directComponentListLifecycle) };
          });
          const failure = page.waitForEvent('pageerror');
          await action('Insert');
          assert.match((await failure).message, /Keyed reconciliation in Board: Unsupported retained component shape change/);
          assert.equal(errors.length, 1, 'exactly one surfaced rejection');
          errors.length = 0;
          assert.equal(await page.evaluate(() => {
            const board = document.querySelector('.board[data-board="A"]');
            return board.innerHTML === window.beforeRejected.html &&
              JSON.stringify(window.jtml.getRuntimeRegistrySnapshot()) === JSON.stringify(window.beforeRejected.registry) &&
              window.jtml.directComponentDestroyLifecycle.count === window.beforeRejected.destroyed &&
              JSON.stringify(window.jtml.directComponentListLifecycle) === window.beforeRejected.lifecycle;
          }), true, 'rejected preflight commits no DOM, destruction or tentative registrations');
          await check();
          const previous = Number(await counter('a').textContent());
          await row('a').getByRole('button', { name: 'Increment', exact: true }).click();
          assert.equal(await counter('a').textContent(), String(previous + 1), 'handlers work after rejection');
          assert.equal(await row('a').locator('input').inputValue(), 'Unsaved');
          await page.evaluate(() => {
            const old = document.querySelector('.board[data-board="A"] .row[data-id="a"]').closest('[data-jtml-direct-instance]');
            const restored = document.createElement('div');
            for (const attr of old.attributes) restored.setAttribute(attr.name, attr.value);
            restored.append(...old.childNodes); old.replaceWith(restored);
          });
          await action('Restore');
          await check();
          assert.equal(await counter('a').textContent(), String(previous + 1));
          const beforeInsert = await check();
          await action('Insert');
          const inserted = await check();
          const newIds = Object.keys(inserted).filter(id => !Object.hasOwn(beforeInsert, id));
          assert.equal(newIds.length, 2);
          assert.ok(newIds.every(id => inserted[id] === 1), 'aborted render did not consume lifecycle generations');
          assert.equal(await counter('c').textContent(), '0');
          await row('c').getByRole('button', { name: 'Increment', exact: true }).click();
          assert.equal(await counter('c').textContent(), '1');
          await action('Restore');
          assert.deepEqual(await check(), beforeInsert);
        }
        results[name] = 'PASS';
      } catch (error) {
        results[name] = { result: 'FAIL', error: error.stack };
        process.exitCode = 1;
      } finally { await page.close(); }
    }
    console.log(JSON.stringify(results));
  } finally { await browser.close(); }
})();
