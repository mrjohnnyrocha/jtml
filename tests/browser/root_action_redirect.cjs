// Real Chromium regression: one existing Runtime Plan action transaction precedes navigation.
const assert = require('node:assert/strict');
const fs = require('node:fs'), path = require('node:path'), http = require('node:http');
const { spawnSync } = require('node:child_process'), { createRequire } = require('node:module');
assert(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT && process.env.JTML_BENCH_BROWSER, 'real Chromium required; no skip');
const { chromium } = createRequire(path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT, 'package.json'))('playwright');
const repo = path.resolve(__dirname, '../..'), out = path.join(repo, 'dist/root-action-redirect');
const checks = [], errors = [], navigations = []; let browser;
const built = spawnSync(process.env.JTML_BIN || path.join(repo, 'build/jtml'), ['build', 'tests/fixtures/production/root-action-redirect/index.jtml', '--target', 'browser', '--out', out], { cwd: repo, encoding: 'utf8' });
assert.equal(built.status, 0, built.stdout + built.stderr);
const server = http.createServer((req, res) => {
  const file = path.join(out, new URL(req.url, 'http://localhost').pathname);
  if (!file.startsWith(out + path.sep) || !fs.existsSync(file) || !fs.statSync(file).isFile()) { res.writeHead(404).end(); return; }
  res.setHeader('Content-Security-Policy', "script-src 'self'");
  res.setHeader('Content-Type', file.endsWith('.js') ? 'text/javascript' : 'text/html');
  res.end(fs.readFileSync(file));
});
(async () => {
  await new Promise(r => server.listen(0, '127.0.0.1', r));
  browser = await chromium.launch({ executablePath: process.env.JTML_BENCH_BROWSER, headless: true });
  const page = await browser.newPage(); page.setDefaultTimeout(3000);
  page.on('pageerror', e => errors.push(e.message));
  const base = `http://127.0.0.1:${server.address().port}/index.html`;
  const state = () => page.evaluate(() => ({ ...jtml.state }));
  const check = async (name, fn) => { try { await fn(); checks.push({ name, pass: true }); } catch (e) { checks.push({ name, pass: false, error: e.message }); } };
  for (const owner of ['Root', 'Component']) {
    await page.goto(base); await page.waitForFunction(() => window.jtml?.getCurrentRoute());
    await page.evaluate(() => {
      window.redirectSnapshots = [];
      const redirect = window.__jtml_redirect;
      window.__jtml_redirect = path => { redirectSnapshots.push({ path, state: { ...jtml.state } }); return redirect(path); };
    });
    const click = label => page.getByRole('button', { name: `${owner} ${label}`, exact: true }).click();
    const route = expected => page.waitForFunction(p => jtml.getCurrentRoute()?.path === p, expected);
    const amount = owner === 'Root' ? 2 : 3, token = owner.toLowerCase();
    await check(`${owner}: assignments, effects and parameter restoration precede redirect`, async () => {
      await click('enter'); await route('/private');
      const s = await state();
      assert.equal(s.token, token); assert.equal(s.executions, 1); assert.equal(s.total, amount); assert.equal(s.tail, 1);
      assert.equal(s.observed, `${token}:1:${amount}:1`); assert.equal(s.effectHits, 1); assert.equal(s.value, 'outer'); assert.equal(s.restored, 'outer'); assert(!Object.hasOwn(s, 'transient')); assert(!Object.hasOwn(s, 'amount'));
      const snapshots = await page.evaluate(() => redirectSnapshots); navigations.push({ owner, snapshots });
      assert.equal(snapshots.length, 1); assert.equal(snapshots[0].state.observed, s.observed); assert.equal(snapshots[0].state.value, 'outer'); assert(!Object.hasOwn(snapshots[0].state, 'transient'));
      await page.goBack(); await route('/'); assert.equal((await state()).executions, 1);
      await page.goForward(); await route('/private'); assert.equal((await state()).executions, 1);
    });
    await check(`${owner}: repeated clicks execute once each`, async () => {
      for (let i = 0; i < 3; ++i) await click('enter');
      const s = await state(); assert.equal(s.executions, 4); assert.equal(s.total, amount * 4); assert.equal(s.tail, 4); assert.equal(s.effectHits, 4);
    });
    await check(`${owner}: sign-out commits before navigation and guard denies re-entry`, async () => {
      await click('leave'); await route('/login'); const s = await state();
      assert.equal(s.token, ''); assert.equal(s.executions, 5); assert.equal(s.effectHits, 5);
      const snapshots = await page.evaluate(() => redirectSnapshots); assert.equal(snapshots.at(-1).state.token, ''); assert.equal(snapshots.at(-1).state.effectHits, 5);
      await page.getByRole('link', { name: 'Private link', exact: true }).click();
      await page.waitForTimeout(150); await route('/login'); assert.equal(await page.evaluate(() => location.hash), '#/login');
    });
    await check(`${owner}: redirect-only navigates without state writes`, async () => {
      await page.goto(base); await page.waitForFunction(() => window.jtml?.getCurrentRoute());
      const before = await state(); await click('only'); await route('/login'); const after = await state();
      for (const key of ['token', 'executions', 'total', 'tail', 'effectHits']) assert.equal(after[key], before[key]);
    });
  }
  await check('strict CSP, no page exceptions or compatibility asset', async () => {
    assert.deepEqual(errors, []); assert.equal(await page.evaluate(() => jtml.browserBuild.mode), 'prod'); assert(!fs.existsSync(path.join(out, 'jtml-update-plans.js')));
  });
})().catch(e => { errors.push(e.stack); process.exitCode = 1; }).finally(async () => {
  if (browser) await browser.close(); await new Promise(r => server.close(r));
  if (checks.some(c => !c.pass)) process.exitCode = 1;
  const result = { checks, errors, navigations, browser: browser && browser.version(), exit: process.exitCode || 0 };
  fs.writeFileSync(path.join(out, 'results.json'), JSON.stringify(result, null, 2) + '\n'); console.log(JSON.stringify(result, null, 2));
});
