// Independent failure oracle: a one-shot host exception is recoverable only
// while subsequent native DOM operations remain usable. No hostile-host promise.
const assert = require('node:assert/strict');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const { chromium } = require(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT
  ? path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT, 'node_modules/playwright') : 'playwright');
(async () => {
  const browser = await chromium.launch({ headless: true, executablePath: process.env.JTML_BENCH_BROWSER || undefined });
  const results = {};
  try {
    for (const fixture of ['partial', 'nested']) for (const afterMutation of [false, true]) {
      const name = fixture + (afterMutation ? '-after-insert' : '-before-insert');
      const page = await browser.newPage(), errors = [], consoleErrors = [];
      page.on('pageerror', error => errors.push(error.message));
      page.on('console', message => { if (message.type() === 'error') consoleErrors.push(message.text()); });
      try {
        await page.goto(pathToFileURL(path.resolve(process.argv[2], fixture, 'index.html')).href);
        const board = page.locator('.board[data-board="A"]');
        const row = id => board.locator('.row[data-id="' + id + '"]');
        const action = label => board.getByRole('button', { name: label, exact: true }).evaluate(el => el.click());
        await row('a').getByRole('button', { name: 'Increment', exact: true }).click();
        await row('a').locator('input').fill('Unsaved');
        await row('a').locator('input').evaluate(el => el.setSelectionRange(1, 5, 'backward'));
        await page.evaluate(({afterMutation}) => {
          const board = document.querySelector('.board[data-board="A"]');
          const wrapper = board.querySelector('.row').closest('[data-jtml-direct-instance]');
          const region = wrapper.closest('[data-jtml-direct-list-key]').parentNode;
          window.beforeCommit = { html: board.innerHTML, registry: JSON.stringify(jtml.getRuntimeRegistrySnapshot()),
            publicInstances: jtml.getComponentInstances(),
            destroyed: jtml.directComponentDestroyLifecycle?.count || 0,
            lifecycle: JSON.stringify(jtml.directComponentListLifecycle),
            refs: [...board.querySelectorAll('*')],
            input: board.querySelector('.row[data-id="a"] input') };
          const insert = Node.prototype.insertBefore;
          window.commitTrace = [];
          Node.prototype.insertBefore = function (child, reference) {
            if (this !== region) return insert.call(this, child, reference);
            Node.prototype.insertBefore = insert;
            if (afterMutation) insert.call(this, child, reference);
            window.commitTrace.push({ mutated: board.innerHTML !== beforeCommit.html,
              destroyed: jtml.directComponentDestroyLifecycle?.count || 0,
              registry: JSON.stringify(jtml.getRuntimeRegistrySnapshot()) });
            throw new DOMException('one-shot keyed commit fault', 'NotFoundError');
          };
        }, { afterMutation });
        await action(fixture === 'partial' ? 'Insert' : 'Update');
        await page.evaluate(() => new Promise(resolve => requestAnimationFrame(resolve)));
        console.log(JSON.stringify({ case: name, trace: await page.evaluate(() => {
          const ids = [...document.querySelectorAll('[data-jtml-direct-instance]')].map(el => el.getAttribute('data-jtml-direct-instance'));
          return { earlierDomMutation: commitTrace[0]?.mutated,
            destroyedAtThrow: commitTrace[0]?.destroyed, destroyedBefore: beforeCommit.destroyed,
            registrationsAtThrow: Object.keys(JSON.parse(commitTrace[0]?.registry || '{}').dynamicGenerations || {}).length,
            dom: ids.length, uniqueIds: new Set(ids).size,
            registered: Object.keys(jtml.getRuntimeRegistrySnapshot().dynamicGenerations).length };
        }) }));
        assert.equal(await page.evaluate(() => commitTrace.length), 1, 'fault reached the live region commit');
        assert.deepEqual(errors, ['Keyed reconciliation in Board: one-shot keyed commit fault'], 'failure surfaces once, without fallback');
        errors.length = 0;
        const state = await page.evaluate(() => {
          const board = document.querySelector('.board[data-board="A"]'), b = beforeCommit;
          const dom = [...document.querySelectorAll('[data-jtml-direct-instance]')].map(el => el.getAttribute('data-jtml-direct-instance')).sort();
          return { html: board.innerHTML === b.html, refs: b.refs.every(el => el.isConnected && board.contains(el)),
            registry: JSON.stringify(jtml.getRuntimeRegistrySnapshot()) === b.registry,
            dom, registered: Object.keys(jtml.getRuntimeRegistrySnapshot().dynamicGenerations).sort(),
            destroyed: jtml.directComponentDestroyLifecycle?.count || 0, oldDestroyed: b.destroyed,
            lifecycle: JSON.stringify(jtml.directComponentListLifecycle) === b.lifecycle,
            focus: [document.activeElement === b.input, b.input.selectionStart, b.input.selectionEnd, b.input.selectionDirection],
            trace: commitTrace[0],
            fallback: ['directComponentStaticUpdateError','directComponentPatchFallback','directComponentBodyPlanFallback'].filter(k => jtml[k]) };
        });
        const publicRollback = await page.evaluate(() => {
          const current = jtml.getComponentInstances(), before = beforeCommit.publicInstances;
          return {beforeIds:before.map(i=>i.id), currentIds:current.map(i=>i.id),
            sameObjects:current.map((i,n)=>i===before[n]), connected:current.map(i=>!!i.element?.isConnected),
            aliases:[jtml.components,__jtml_components].map(es=>es.map(i=>i.id)),
            lookup:current.map(i=>jtml.findComponentInstance(i.id)===i), metadata:current.map(i=>!!i.runtime)};
        });
        assert.equal(publicRollback.beforeIds.length,10,'two boards, four rows and four counters');
        assert.deepEqual(publicRollback.currentIds,publicRollback.beforeIds,'rollback restores exact public registry order/identities');
        for(const flag of ['sameObjects','connected','lookup','metadata']) assert.deepEqual(publicRollback[flag],Array(10).fill(true),flag);
        assert.deepEqual(publicRollback.aliases,[publicRollback.beforeIds,publicRollback.beforeIds]);
        assert.ok(state.html && state.refs && state.registry && state.lifecycle, 'exact DOM/ownership/metadata rollback');
        assert.deepEqual(state.dom, state.registered, 'DOM and registry bijection');
        assert.equal(new Set(state.dom).size, state.dom.length, 'unique component IDs');
        assert.equal(state.destroyed, state.oldDestroyed, 'no destruction before successful commit');
        assert.equal(state.trace.destroyed, state.oldDestroyed, 'no destruction at throw point');
        assert.ok(state.trace.mutated, 'earlier nested edits really occurred');
        assert.deepEqual(state.focus, [true,1,5,'backward']);
        assert.deepEqual(state.fallback, []);
        assert.equal(await row('a').locator('input').inputValue(), 'Unsaved');
        await row('a').getByRole('button', { name: 'Increment', exact: true }).click();
        assert.equal(await row('a').locator('.counter p').textContent(), '2', 'handler reaches the retained row exactly once');
        assert.equal(await row('b').locator('.counter p').textContent(), '0', 'no handler misrouting');
        // The parent action's state writes survive rejection; retry that same
        // action directly, without a compensating Restore or new generation.
        await action(fixture === 'partial' ? 'Insert' : 'Update');
        if (fixture === 'partial') {
          assert.equal(await row('c').locator('.counter p').textContent(), '0');
          const generations = await page.evaluate(() => jtml.getRuntimeRegistrySnapshot().dynamicGenerations);
          const inserted = Object.entries(generations).filter(([id]) => id.includes('_c'));
          assert.equal(inserted.length, 2);
          assert.ok(inserted.every(([,g]) => g === 1), 'aborted registration consumed no generation');
        } else {
          assert.equal(await board.locator('.counter').count(), 0);
          await action('Restore');
          assert.deepEqual(await board.locator('.counter p').allTextContents(), ['0','0']);
        }
        assert.deepEqual(await page.locator('.board[data-board="B"] .counter p').allTextContents(), ['0','0']);
        const finalOwnership = await page.evaluate(() => ({
          dom: [...document.querySelectorAll('[data-jtml-direct-instance]')].map(el => el.getAttribute('data-jtml-direct-instance')).sort(),
          registered: Object.keys(jtml.getRuntimeRegistrySnapshot().dynamicGenerations).sort() }));
        assert.deepEqual(finalOwnership.dom, finalOwnership.registered);
        assert.equal(new Set(finalOwnership.dom).size, finalOwnership.dom.length);
        assert.deepEqual(errors, []);
        assert.deepEqual(consoleErrors, []);
        results[name] = 'PASS';
      } catch (error) { results[name] = { result: 'FAIL', error: error.stack }; process.exitCode = 1; }
      finally { await page.close(); }
    }
    console.log(JSON.stringify(results));
  } finally { await browser.close(); }
})();
