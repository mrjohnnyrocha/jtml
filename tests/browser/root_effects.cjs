// Root effects use real UI dispatch and production assets; no injected state/callbacks.
const assert = require('node:assert/strict'), fs = require('fs'), path = require('path'), http = require('http');
const {spawnSync} = require('child_process'), {createRequire} = require('module');
assert(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT && process.env.JTML_BENCH_BROWSER, 'real Chromium required; no skip');
const {chromium} = createRequire(path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT, 'package.json'))('playwright');
const repo = path.resolve(__dirname, '../..'), out = path.join(repo, 'dist/root-effects');
const sources = ['examples/friendly_effect.jtml', 'studio/samples/effects.jtml', 'tests/fixtures/production/root-effects/index.jtml'];
const rows = []; let browser;
const server = http.createServer((req, res) => {
  const file = path.resolve(out, '.' + new URL(req.url, 'http://localhost').pathname);
  res.setHeader('Content-Security-Policy', "script-src 'self'");
  if (!file.startsWith(out + path.sep) || !fs.existsSync(file)) { res.writeHead(404).end(); return; }
  res.setHeader('Content-Type', file.endsWith('.js') ? 'text/javascript' : 'text/html'); res.end(fs.readFileSync(file));
});
(async () => {
  fs.mkdirSync(out, {recursive: true}); await new Promise(r => server.listen(0, '127.0.0.1', r));
  browser = await chromium.launch({headless: true, executablePath: process.env.JTML_BENCH_BROWSER});
  for (let i = 0; i < sources.length; ++i) {
    const command = [process.env.JTML_BIN || path.join(repo, 'build/jtml'), 'build', sources[i], '--target', 'browser', '--out', path.join(out, '' + i)];
    const built = spawnSync(command[0], command.slice(1), {cwd: repo, encoding: 'utf8'});
    const row = {source: sources[i], command, exit: built.status, stderr: built.stderr, checks: []}; rows.push(row);
    assert.equal(built.status, 0, built.stdout + built.stderr);
    const page = await browser.newPage(), errors = [], violations = [], shown = [];
    page.setDefaultTimeout(3000); page.on('pageerror', e => errors.push(e.message));
    page.on('console', message => { if (message.type() === 'error') violations.push(message.text()); if (message.text().startsWith('[SHOW] ')) shown.push(message.text()); });
    await page.goto(`http://127.0.0.1:${server.address().port}/${i}/index.html`);
    await page.waitForFunction(() => window.jtml?.browserBuild?.mode === 'prod');
    const text = value => page.getByText(value, {exact: true}).waitFor();
    const click = label => page.getByRole('button', {name: label, exact: true}).click();
    const check = async (name, fn) => { await fn(); row.checks.push(name); };
    if (i < 2) {
      const last = value => text((i === 0 ? 'Last: ' : 'Last effect: ') + value);
      await check('non-eager initial state', () => last('No changes yet.'));
      for (const count of [1, 2]) await check('increment ' + count, async () => { await click('+ Increment'); await last('Count changed to ' + count); });
      for (const count of [1, 0, -1]) await check('decrement ' + count, async () => { await click('- Decrement'); await last('Count changed to ' + count); });
      if (i === 1) {
        await check('conditional effect body', () => text('zero or negative'));
        await check('unrelated snapshot write survives', async () => { await click('Save snapshot'); await last('Snapshot 1: count -1'); });
        await check('subsequent effect after unrelated write', async () => { await click('+ Increment'); await last('Count changed to 0'); });
      }
    } else {
      const state = async (count, hits) => { await text('Count: ' + count); await text('Hits: ' + hits); await text('Chain: ' + hits); };
      await check('non-eager module and root registration', async () => { await state(0, 0); await text('Module hits: 0'); await text('Order: seed'); });
      await check('same-value assignment triggers', async () => { await click('Same'); await state(0, 1); await text('Order: seen 1'); });
      await check('multiple writes coalesce with final visibility', async () => { await click('Batch'); await state(2, 2); await text('Summary: count 2 hits 2'); await text('Order: seen 2'); });
      await check('unrelated and parameter-shadow writes do not trigger', async () => { await click('Unrelated'); await click('Shadow'); await click('False branch'); await state(2, 2); });
      await check('conditional write and chain', async () => { await click('True branch'); await state(3, 3); });
      await check('repeated dispatches and reset', async () => { await click('Increment'); await state(4, 4); await click('Zero'); await state(0, 5); await click('Same'); await state(0, 6); });
      await check('module-owned subscription executes once independently', async () => { await click('Module'); await text('Module count: 1'); await text('Module hits: 1'); await click('Module'); await text('Module hits: 2'); await state(0, 6); });
      assert.deepEqual(shown, ['[SHOW] 0', '[SHOW] 2', '[SHOW] 3', '[SHOW] 4', '[SHOW] 0', '[SHOW] 0']);
      row.checks.push('scalar show executes once at the notification boundary');
      row.effectInteractionMs = await page.evaluate(() => {
        const button = [...document.querySelectorAll('button')].find(el => el.textContent.trim() === 'Same');
        const start = performance.now(); for (let i = 0; i < 200; ++i) button.click();
        return performance.now() - start;
      });
      assert(row.effectInteractionMs < 3000, 'effect interaction exceeds existing 3000ms benchmark limit');
      await text('Hits: 206'); await text('Chain: 206');
      row.checks.push('200 additional same-value UI dispatches execute bounded effect chains');
      const subscriptions = await page.evaluate(() => JSON.parse(document.querySelector('#__jtml_client_manifest').textContent).subscriptions);
      assert.equal(subscriptions.length, 4); assert(subscriptions.every(s => Number.isInteger(s.moduleId)));
      assert.equal(new Set(subscriptions.map(s => `${s.moduleId}/${s.action}/${s.dependency}`)).size, 4);
    }
    assert.deepEqual(errors, []); assert.deepEqual(violations, []);
    assert(!fs.existsSync(path.join(out, '' + i, 'jtml-update-plans.js')));
    row.checks.push('strict CSP / clean assets / no errors'); await page.close();
  }
})().catch(e => { console.error(e); process.exitCode = 1; }).finally(async () => {
  if (browser) await browser.close(); await new Promise(r => server.close(r));
  fs.writeFileSync(path.join(out, 'results.json'), JSON.stringify(rows, null, 2) + '\n'); console.log(JSON.stringify(rows, null, 2));
});
