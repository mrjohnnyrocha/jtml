// Real Chromium required. Authored inline input text must survive direct creation/update.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const http = require('node:http');
const {spawnSync} = require('node:child_process');
const {createRequire} = require('node:module');
const repo = path.resolve(__dirname, '../..');
assert(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT && process.env.JTML_BENCH_BROWSER, 'real Chromium required; no skip');
const {chromium} = createRequire(path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT, 'package.json'))('playwright');
const out = path.join(repo, 'dist/component-input-name');
const checks = [], errors = []; let browser;
for (const mode of ['plain', 'layout']) {
  const dest = path.join(out, mode); fs.mkdirSync(dest, {recursive: true});
  const source = fs.readFileSync(path.join(repo, 'tests/fixtures/production/component-input-name/index.jtml'), 'utf8');
  const fixture = path.join(dest, 'source.jtml'); fs.writeFileSync(fixture, mode === 'plain' ? source.replace(' layout Layout', '') : source);
  const r = spawnSync(process.env.JTML_BIN || path.join(repo, 'build/jtml'), ['build', fixture, '--target', 'browser', '--out', dest], {encoding: 'utf8'});
  assert.equal(r.status, 0, r.stdout + r.stderr);
}
const server = http.createServer((req, res) => {
  const file = path.join(out, new URL(req.url, 'http://localhost').pathname);
  if (!file.startsWith(out + path.sep) || !fs.existsSync(file) || !fs.statSync(file).isFile()) { res.writeHead(404).end(); return; }
  res.setHeader('Content-Security-Policy', "script-src 'self'");
  res.setHeader('Content-Type', file.endsWith('.js') ? 'text/javascript' : 'text/html');
  res.end(fs.readFileSync(file));
});
(async () => {
  await new Promise(r => server.listen(0, '127.0.0.1', r));
  browser = await chromium.launch({executablePath: process.env.JTML_BENCH_BROWSER, headless: true});
  for (const mode of ['plain', 'layout']) {
    const page = await browser.newPage(); page.setDefaultTimeout(1500);
    page.on('pageerror', e => errors.push(e.message));
    await page.goto(`http://127.0.0.1:${server.address().port}/${mode}/index.html#/edit`);
    await page.waitForFunction(() => window.jtml?.getCurrentRoute()?.path === '/edit');
    const editors = page.locator('.editor:visible'), first = editors.first();
    const check = async (name, fn) => { try { await fn(); checks.push({mode, name, pass: true}); } catch (e) { checks.push({mode, name, pass: false, error: e.message}); } };
    await check('inline input text becomes placeholder and accessible name', async () => {
      assert.equal(await editors.count(), 2);
      const email = first.getByRole('textbox', {name: 'Email', exact: true});
      await email.waitFor(); assert.equal(await email.getAttribute('placeholder'), 'Email');
      assert.equal(await email.getAttribute('aria-label'), null);
      assert.equal(await email.getAttribute('type'), 'email'); assert.equal(await email.getAttribute('required'), '');
      for (const value of ['first@example.test', 'second@example.test']) {
        await email.fill(value); await first.getByText('Mirror: ' + value, {exact: true}).waitFor();
        assert(await email.evaluate(e => document.activeElement === e));
      }
      await email.fill('seed');
    });
    await check('parameterized hints, textarea and explicit attributes retain semantics', async () => {
      await first.getByRole('textbox', {name: 'Parameterized hint', exact: true}).waitFor();
      await editors.last().getByRole('textbox', {name: 'Sibling hint', exact: true}).waitFor();
      const explicit = first.getByRole('textbox', {name: 'Explicit name', exact: true});
      assert.equal(await explicit.getAttribute('placeholder'), 'Explicit hint');
      await first.getByRole('textbox', {name: 'Notes', exact: true}).waitFor();
      await first.getByRole('textbox', {name: 'Owned', exact: true}).waitFor();
      assert.equal(await first.getByLabel('Choose file', {exact: true}).getAttribute('type'), 'file');
      assert.equal(await first.getByLabel('Drop files', {exact: true}).getAttribute('type'), 'file');
    });
    await check('editing and unrelated local updates retain ownership, DOM, focus and cursor', async () => {
      // Explicit aria-label is the independent control, even on the failing baseline.
      const input = first.getByRole('textbox', {name: 'Owned', exact: true});
      await input.fill('changed'); await first.getByText('Mirror: changed', {exact: true}).waitFor();
      await editors.last().getByText('Mirror: seed', {exact: true}).waitFor();
      await input.evaluate(e => { window.retainedInput = e; e.setSelectionRange(1, 3); });
      await first.getByRole('button', {name: 'Tick', exact: true}).dispatchEvent('click');
      await first.getByText('Ticks: 1', {exact: true}).waitFor();
      assert.deepEqual(await input.evaluate(e => [e === retainedInput, document.activeElement === e, e.selectionStart, e.selectionEnd]), [true, true, 1, 3]);
      assert.equal(await first.locator('input[type=email]').inputValue(), 'changed');
      assert.equal(await page.getByRole('textbox', {name: 'Root email', exact: true}).inputValue(), 'root');
    });
    await page.close();
  }
  assert.deepEqual(errors, []);
  if (checks.some(c => !c.pass)) process.exitCode = 1;
})().catch(e => { errors.push(e.stack); process.exitCode = 1; }).finally(async () => {
  if (browser) await browser.close(); await new Promise(r => server.close(r));
  const result = {checks, errors, browser: browser && browser.version(), exit: process.exitCode || 0};
  fs.writeFileSync(path.join(out, 'results.json'), JSON.stringify(result, null, 2) + '\n'); console.log(JSON.stringify(result, null, 2));
});
