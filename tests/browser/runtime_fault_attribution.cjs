const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const os = require('node:os');
const path = require('node:path');
const {spawnSync} = require('node:child_process');
const {createRequire} = require('node:module');

const repo = path.resolve(__dirname, '../..');
const fixture = path.join(repo, 'tests/fixtures/production/runtime-fault-attribution/index.jtml');
const binary = process.env.JTML_BIN || path.join(repo, 'build/jtml');
assert(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT && process.env.JTML_BENCH_BROWSER,
  'Real Chromium and Playwright are required');
const {chromium} = createRequire(path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT, 'package.json'))('playwright');
const work = fs.mkdtempSync(path.join(os.tmpdir(), 'jtml-runtime-fault-'));
let target, browser;
const server = http.createServer((request, response) => {
  if (request.url === '/favicon.ico') return response.writeHead(204).end();
  const file = path.resolve(target, '.' + new URL(request.url, 'http://localhost').pathname);
  if (!file.startsWith(target + path.sep) || !fs.existsSync(file)) return response.writeHead(404).end();
  response.writeHead(200, {'Content-Security-Policy': "script-src 'self'; object-src 'none'; base-uri 'none'",
    'Content-Type': file.endsWith('.js') ? 'text/javascript' : 'text/html'}).end(fs.readFileSync(file));
});

async function observe(flags) {
  const mode = flags[0] || 'prod';
  target = path.join(work, mode);
  const result = spawnSync(binary, ['build', fixture, '--target', 'browser', '--out', target, ...flags],
    {encoding: 'utf8', timeout: 30000});
  assert.equal(result.status, 0, result.stdout + result.stderr);
  if (!flags.length) assert.deepEqual(fs.readdirSync(target).sort(), ['app.js', 'components', 'index.html', 'jtml-runtime.js']);
  const page = await browser.newPage();
  try {
    await page.addInitScript(() => {
      window.faults = [];
      window.cspViolations = [];
      function observe(error) {
        faults.push({message: error.message, code: error.code || null});
      }
      window.addEventListener('error', event => {if (event.error) observe(event.error);});
      window.addEventListener('unhandledrejection', event => observe(event.reason));
      document.addEventListener('securitypolicyviolation', event => cspViolations.push(event.effectiveDirective));
    });
    await page.goto(`http://127.0.0.1:${server.address().port}/index.html`);
    const click = name => page.getByRole('button', {name, exact: true}).click();
    const rows = page.locator('.rows');
    const effect = page.locator('.effect-out');
    const items = () => page.evaluate(() => jtml.getComponentInstances().find(i => i.component === 'Board').scope.items);
    assert.equal(await rows.textContent(), 'ab');
    await click('Duplicate');
    await page.waitForFunction(() => faults.length === 1);
    assert.equal(await rows.textContent(), 'ab');
    assert.deepEqual(await items(), ['a', 'a', 'c']);
    await click('Repair');
    assert.equal(await rows.textContent(), 'ba');
    assert.deepEqual(await items(), ['b', 'a']);

    assert.equal((await effect.textContent()).trim(), '0|0');
    await click('Invalid write');
    await page.waitForFunction(() => faults.length === 2);
    assert.equal((await effect.textContent()).trim(), '0|0');
    assert.deepEqual(await page.evaluate(() => [jtml.state.watched, jtml.state.hits]), [[1, 2], 0]);
    await click('Valid write');
    assert.equal((await effect.textContent()).trim(), '7|1');
    assert.deepEqual(await page.evaluate(() => [jtml.state.watched, jtml.state.hits]), [7, 1]);
    assert.deepEqual(await page.evaluate(() => cspViolations), []);
    const faults = await page.evaluate(() => window.faults);
    console.log(mode, JSON.stringify(faults));
    return faults;
  } finally {
    await page.close();
  }
}

(async () => {
  await new Promise((resolve, reject) => server.once('error', reject).listen(0, '127.0.0.1', resolve));
  browser = await chromium.launch({headless: true, executablePath: process.env.JTML_BENCH_BROWSER});
  const observations = [];
  for (const flags of [[], ['--compat'], ['--debug']]) observations.push(await observe(flags));
  for (const faults of observations) {
    assert.deepEqual(faults, [
      {code: 'E_DUPLICATE_KEYED_ROW', message: 'owners.jtml:20:7: E_DUPLICATE_KEYED_ROW: Keyed reconciliation in Board: Duplicate keyed row keys'},
      {code: 'E_EFFECT_SCALAR_WRITE', message: 'owners.jtml:6:1: E_EFFECT_SCALAR_WRITE: Root effect dependency watched requires scalar writes; inspect its writers with jtml explain'},
    ]);
  }
})().catch(error => {
  console.error(error);
  process.exitCode = 1;
}).finally(async () => {
  if (browser) await browser.close();
  if (server.listening) await new Promise(resolve => server.close(resolve));
  fs.rmSync(work, {recursive: true, force: true});
});
