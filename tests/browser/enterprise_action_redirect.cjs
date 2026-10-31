// Supported-value acceptance on the original Enterprise source. No API mocks or input ownership expansion.
const assert = require('node:assert/strict'), fs = require('node:fs'), path = require('node:path'), http = require('node:http');
const { spawnSync } = require('node:child_process'), { createRequire } = require('node:module');
assert(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT && process.env.JTML_BENCH_BROWSER, 'real Chromium required; no skip');
const { chromium } = createRequire(path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT, 'package.json'))('playwright');
const repo = path.resolve(__dirname, '../..'), out = path.join(repo, 'dist/enterprise-action-redirect'), checks = [], errors = [], requests = [], observations = []; let browser;
const built = spawnSync(process.env.JTML_BIN || path.join(repo, 'build/jtml'), ['build', 'demos/index.jtml', '--target', 'browser', '--out', out], { cwd: repo, encoding: 'utf8' });
assert.equal(built.status, 0, built.stdout + built.stderr);
const server = http.createServer((req, res) => {
  const file = path.join(out, new URL(req.url, 'http://localhost').pathname); requests.push({ method: req.method, path: req.url });
  if (!file.startsWith(out + path.sep) || !fs.existsSync(file) || !fs.statSync(file).isFile()) { res.writeHead(404).end(); return; }
  res.setHeader('Content-Security-Policy', "script-src 'self'"); res.setHeader('Content-Type', file.endsWith('.js') ? 'text/javascript' : 'text/html'); res.end(fs.readFileSync(file));
});
(async () => {
  await new Promise(r => server.listen(0, '127.0.0.1', r)); browser = await chromium.launch({ executablePath: process.env.JTML_BENCH_BROWSER, headless: true });
  const page = await browser.newPage(); page.setDefaultTimeout(3000); page.on('pageerror', e => errors.push(e.message));
  await page.goto(`http://127.0.0.1:${server.address().port}/index.html#/command`); await page.waitForFunction(() => window.jtml?.getCurrentRoute()?.path === '/command');
  const state = () => page.evaluate(() => ({ token: jtml.state.sessionToken, email: jtml.state.loginEmail, audit: jtml.state.auditTrail, route: jtml.getCurrentRoute().path }));
  const click = name => page.getByRole('button', { name, exact: true }).filter({ visible: true }).click();
  await page.evaluate(() => {
    window.redirectStates = []; const redirect = window.__jtml_redirect;
    window.__jtml_redirect = path => { redirectStates.push({ path, token: jtml.state.sessionToken, audit: jtml.state.auditTrail }); return redirect(path); };
  });
  observations.push({ stage: 'initial', ...await state() });
  await click('Sign out'); await page.waitForFunction(() => jtml.getCurrentRoute()?.path === '/login');
  observations.push({ stage: 'signed-out', ...await state(), redirects: await page.evaluate(() => redirectStates) });
  assert.equal((await state()).token, ''); assert.equal((await state()).audit, 'Signed out.');
  assert.equal(await page.evaluate(() => redirectStates[0].token), ''); checks.push('sign-out clears illustrative token and commits audit before navigation');
  await page.getByRole('link', { name: 'Command', exact: true }).filter({ visible: true }).click(); await page.waitForTimeout(150);
  observations.push({ stage: 'guarded', ...await state() });
  assert.equal((await state()).route, '/login'); assert.equal(await page.evaluate(() => location.hash), '#/login'); checks.push('guard denies Command after sign-out');
  const email = (await state()).email; assert.equal(email, 'ada@example.com');
  await click('Sign in'); await page.waitForFunction(() => jtml.getCurrentRoute()?.path === '/command');
  observations.push({ stage: 'signed-in', ...await state(), redirects: await page.evaluate(() => redirectStates) });
  assert.equal((await state()).token, email); assert.equal((await state()).audit, `Signed in as ${email}.`); checks.push('sign-in uses existing valid module value and commits both assignments');
  assert.deepEqual(errors, []); checks.push('strict script CSP without page exceptions; external APIs remain real missing resources');
})().catch(e => { errors.push(e.stack); process.exitCode = 1; }).finally(async () => {
  if (browser) await browser.close(); await new Promise(r => server.close(r));
  const result = { checks, errors, requests, observations, browser: browser && browser.version(), exit: process.exitCode || 0 }; fs.writeFileSync(path.join(out, 'results.json'), JSON.stringify(result, null, 2) + '\n'); console.log(JSON.stringify(result, null, 2));
});
