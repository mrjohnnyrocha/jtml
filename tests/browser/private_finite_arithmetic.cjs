const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const os = require('node:os');
const path = require('node:path');
const {execFileSync} = require('node:child_process');
const {createRequire} = require('node:module');

const {chromium} = createRequire(path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT, 'package.json'))('playwright');
const repo = path.resolve(__dirname, '../..');
const fixture = path.join(repo, 'tests/fixtures/production/private-finite-arithmetic');
const work = fs.mkdtempSync(path.join(os.tmpdir(), 'jtml-finite-'));
const vectors = require(path.join(repo, 'docs/spec/reactive-core-v1/vectors.json'));
const v19 = vectors.vectors.find(vector => vector.id === 'V19');
const source = path.join(work, 'v19');
fs.mkdirSync(source);
for (const [name, contents] of Object.entries(v19.files)) fs.writeFileSync(path.join(source, name), contents);

const server = http.createServer((request, response) => {
  const file = path.resolve(work, '.' + new URL(request.url, 'http://localhost').pathname);
  if (!file.startsWith(work + path.sep) || !fs.existsSync(file)) return response.writeHead(404).end();
  response.writeHead(200, {'Content-Security-Policy': "script-src 'self'; object-src 'none'",
    'Content-Type': file.endsWith('.js') ? 'text/javascript' : 'text/html'}).end(fs.readFileSync(file));
});

const click = (page, name) => page.getByRole('button', {name, exact: true}).click();
const view = async (page, selector) => (await page.locator(selector).textContent()).trim();
const life = page => page.evaluate(() => window.__jtml_test_failure?.());
const frames = page => page.evaluate(() => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve))));
async function open(browser, mode, app, test) {
  const page = await browser.newPage();
  const errors = [], shows = [];
  page.on('pageerror', error => errors.push(error.message));
  page.on('console', message => { if (message.text().startsWith('[SHOW]')) shows.push(message.text()); });
  await page.addInitScript(() => {
    window.violations = [];
    document.addEventListener('securitypolicyviolation', event => violations.push(event.effectiveDirective));
  });
  try {
    await page.goto(`http://127.0.0.1:${server.address().port}/${mode}-${app}/index.html`);
    await test(page, errors, shows);
    assert.deepEqual(await page.evaluate(() => violations), []);
  } finally { await page.close(); }
}

const cases = [
  {button: 'Divide', action: 'divide', op: '/', state: {x: 2}, shows: ['[SHOW] 2']},
  {button: 'Negative', action: 'negative', op: '/'},
  {button: 'Remainder', action: 'remainder', op: '%'},
  {button: 'Overflow', action: 'overflow', op: '*'},
  {button: 'Derived', action: 'deriveFault', op: '/', state: {divisor: 0}},
  {button: 'Effect', action: 'effectFault', op: '/', state: {trigger: 0}, site: 'index.jtml:13:1'},
  {button: 'Component', action: 'step', op: '/=', component: {count: 3, div: 2}},
  {button: 'Component derived', action: 'getFault', op: '/', component: {count: 2, div: 0}}
];

(async () => {
  for (const [mode, compiler] of [['full', process.env.JTML_REAL_BIN], ['private', process.env.JTML_BIN]]) {
    for (const [app, cwd] of [['v19', source], ['matrix', fixture]]) {
      execFileSync(compiler, ['build', 'index.jtml', '--target', 'browser', '--out', path.join(work, mode + '-' + app)],
        {cwd, timeout: 30000});
    }
  }
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const browser = await chromium.launch({executablePath: process.env.JTML_BENCH_BROWSER});
  try {
    for (const [button, expected] of [['Negative', -Infinity], ['Remainder', NaN], ['Overflow', Infinity]]) {
      await open(browser, 'full', 'matrix', async (page, errors) => {
        await click(page, button);
        assert.equal(await page.evaluate(() => jtml.state.y), expected);
        assert.deepEqual(errors, []);
      });
    }
    await open(browser, 'full', 'v19', async (page, errors) => {
      assert.equal(await view(page, '.out'), 'Value 1');
      await click(page, 'Step');
      assert.equal(await view(page, '.out'), 'Value Infinity');
      assert.equal(await life(page), undefined);
      assert.deepEqual(errors, []);
    });
    for (const mode of ['full', 'private']) {
      await open(browser, mode, 'matrix', async (page, errors, shows) => {
        await click(page, 'Healthy');
        assert.equal(await view(page, '.out'), '2|0|0.5|1|9|0');
        assert.equal(await page.evaluate(() => Object.is(jtml.state.y, -0)), true);
        assert.deepEqual(shows, ['[SHOW] false', '[SHOW] true', '[SHOW] 7']);
        assert.deepEqual(errors, []);
        assert.equal((await life(page))?.revision, mode === 'private' ? 1 : undefined);
      });
    }

    const failures = [];
    for (const c of [...cases, {button: 'Step', action: 'step', op: '/', v19: true}]) {
      try {
        await open(browser, 'private', c.v19 ? 'v19' : 'matrix', async (page, errors, shows) => {
          const rootDOM = await view(page, '.out');
          const componentDOM = c.v19 ? null : await view(page, '.component');
          if (c.component) assert.ok(await page.getByRole('button', {name: c.button, exact: true})
            .getAttribute('data-jtml-direct-component-action'));
          await click(page, c.button);
          await frames(page);
          const failed = await life(page);
          console.log(c.button, JSON.stringify({status: failed.status, revision: failed.revision,
            dom: await view(page, '.out'), diagnostic: errors[0]}));
          assert.equal(failed.status, 'FAILED');
          if (!errors.length) await page.waitForEvent('pageerror', {timeout: 10000});
          assert.equal(failed.revision, 0);
          assert.equal(failed.fault.invocation.action, c.action);
          assert.equal(failed.fault.code, 'E_NON_FINITE_ARITHMETIC');
          assert.equal(failed.fault.site, c.site ?? null);
          assert.equal(failed.fault.publicationEntered, false);
          assert.deepEqual(failed.pendingWrites, []);
          assert.equal(errors.length, 1);
          assert.equal(errors[0], (c.site ? c.site + ': E_NON_FINITE_ARITHMETIC: ' : '') +
            'Non-finite arithmetic result for ' + c.op);
          assert.deepEqual(failed.partial.root, c.v19 ? {x: 1} :
            {x: 1, y: 1, trigger: 2, divisor: 2, hits: 0, effectValue: 9, tail: 0, base: 1e20, ...c.state});
          if (c.component) {
            assert.deepEqual(failed.partial.components.find(instance => instance.component === 'Counter').state, c.component);
          }
          assert.equal(await view(page, '.out'), rootDOM);
          if (componentDOM !== null) assert.equal(await view(page, '.component'), componentDOM);
          await click(page, c.v19 ? 'Step' : 'Healthy');
          if (!c.v19) await click(page, 'Component');
          await frames(page);
          assert.deepEqual(await life(page), failed);
          assert.equal(await view(page, '.out'), rootDOM);
          if (componentDOM !== null) assert.equal(await view(page, '.component'), componentDOM);
          assert.deepEqual(shows, c.shows || []);
          assert.equal(errors.length, 1);
        });
      } catch (error) { failures.push(error); }
    }
    if (failures.length) throw new AggregateError(failures, 'Finite arithmetic acceptance');
    console.log('PASS finite arithmetic, private P3 containment, and default V19 behavior');
  } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; }).finally(async () => {
  if (server.listening) await new Promise(resolve => server.close(resolve));
  fs.rmSync(work, {recursive: true, force: true});
});
