// Real generated DOM under an HTTP CSP header. No action/state injection.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const http = require('node:http');
const {spawnSync} = require('node:child_process');
const {createRequire} = require('node:module');
const repo = path.resolve(__dirname, '../..');
const out = path.join(repo, 'dist/csp-events');
const driver = process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT;
assert(driver && process.env.JTML_BENCH_BROWSER, 'real browser required; no skip');
const {chromium} = createRequire(path.resolve(driver, 'package.json'))('playwright');
const cases = {
  probe: 'apps/releasedesk/probes/root-controls.jtml',
  controls: 'tests/fixtures/csp/root-events.jtml',
  counter: 'examples/friendly_counter.jtml',
};
for (const [name, source] of Object.entries(cases)) {
  const result = spawnSync(process.env.JTML_BIN || path.join(repo, 'build/jtml'),
    ['build', path.join(repo, source), '--target', 'browser', '--out', path.join(out, name)], {encoding:'utf8'});
  assert.equal(result.status, 0, result.stdout + result.stderr);
  for (const asset of ['index.html','app.js','jtml-runtime.js','components/index.js']) {
    const text = fs.readFileSync(path.join(out, name, asset), 'utf8');
    assert(!/\bnew\s+Function\b|\beval\s*\(/.test(text), `${name}/${asset}: dynamic compilation`);
    assert(!/\son[a-z]+\s*=/i.test(text), `${name}/${asset}: inline event attribute`);
    if (asset === 'index.html') {
      // The inert JSON manifest is data, not an executable inline script.
      assert(!/<script\b(?![^>]*\bsrc=)(?![^>]*\btype="application\/json")/i.test(text), `${name}: inline executable script`);
    }
  }
  assert(fs.statSync(path.join(out,name,'jtml-runtime.js')).size <= 272000);
  assert(fs.statSync(path.join(out,name,'app.js')).size <= 20000);
}
const server = http.createServer((req, res) => {
  res.setHeader('Content-Security-Policy', "script-src 'self'");
  if (req.url.startsWith('/native')) {res.setHeader('Content-Type','text/html');res.end('Native navigation');return;}
  if (req.url === '/negative') {
    res.setHeader('Content-Type','text/html');
    // Independent negative: both inline script and inline event must be blocked.
    res.end('<script>window.inlineRan=true</script><button onclick="this.textContent=\'FAILED\'">Blocked</button>');
    return;
  }
  const relative = decodeURIComponent(new URL(req.url, 'http://localhost').pathname).replace(/^\//,'');
  const file = path.resolve(out, relative.endsWith('/') ? relative + 'index.html' : relative);
  if (!file.startsWith(out + path.sep) || !fs.existsSync(file)) {res.writeHead(404);res.end();return;}
  res.setHeader('Content-Type',file.endsWith('.js')?'text/javascript':'text/html');
  res.end(fs.readFileSync(file));
});
let browser;
(async () => {
  await new Promise(resolve => server.listen(0,'127.0.0.1',resolve));
  const origin = `http://127.0.0.1:${server.address().port}`;
  browser = await chromium.launch({executablePath:process.env.JTML_BENCH_BROWSER,headless:true});
  const page = await browser.newPage();
  page.setDefaultTimeout(7000);
  const errors = [], violations = [];
  let negative = false;
  await page.exposeFunction('observeCSP', directive => violations.push(directive));
  // Observation only; never installs application handlers or writes app state.
  await page.addInitScript(() => document.addEventListener('securitypolicyviolation', e => window.observeCSP(e.effectiveDirective)));
  page.on('pageerror',e => errors.push(e.message));
  page.on('console',m => {if(m.type()==='error' && !negative)errors.push(m.text());});
  const text = value => page.getByText(value,{exact:true}).waitFor();
  const click = name => page.getByRole('button',{name,exact:true}).click();
  const load = async name => {
    const response = await page.goto(`${origin}/${name}/`);
    assert.equal(response.headers()['content-security-policy'], "script-src 'self'");
    await page.waitForFunction(() => window.jtml?.browserBuild?.cleanProduction);
  };
  await load('probe');
  await click('Increment'); await text('Clicks: 1');
  await page.getByLabel('Selection').selectOption('high'); await text('Selection state: high');
  await page.getByLabel('Selection').selectOption('low'); await text('Selection state: low');
  await load('counter');
  await text('Count: 0'); await click('Add one'); await text('Count: 1'); await click('Reset'); await text('Count: 0');
  await load('controls');
  await click('Root increment'); await text('Count: 2');
  await click('Save'); await text('Submissions: 0');
  const url = page.url();
  const draft = page.getByLabel('Draft',{exact:true});
  await draft.fill('Saved under strict CSP'); await text('Draft: Saved under strict CSP');
  await draft.press('Shift+ArrowLeft'); await text('Key: ArrowLeft'); await text('Shifted: true'); await text('Keyup: Saved under strict CSP');
  await draft.press('Enter'); await text('Submissions: 1'); await text('Saved: Saved under strict CSP');
  assert.equal(page.url(),url,'handled submit must not navigate');
  await page.getByLabel('Choice',{exact:true}).selectOption('high'); await text('Choice: high');
  await page.getByLabel('Agreed',{exact:true}).check(); await text('Agreed: true');
  await page.getByLabel('Agreed',{exact:true}).uncheck(); await text('Agreed: false');
  await click('Literal argument'); await text("Saved: Quotes: ' & < > \\");
  await page.getByRole('button',{name:'Double',exact:true}).dblclick(); await text('Doubles: 1');
  assert(Number((await page.getByText(/^Focus: \d+$/).textContent()).split(': ')[1]) > 0);
  assert(Number((await page.getByText(/^Blur: \d+$/).textContent()).split(': ')[1]) > 0);
  const stable = page.locator('.owned').last();
  await stable.getByRole('button',{name:'Component increment'}).click(); await stable.getByText('Local: 1',{exact:true}).waitFor();
  for (let cycle=0; cycle<2; cycle++) {
    await click('Toggle controls');
    await click('Dynamic increment'); await text(`Count: ${5+cycle*3}`);
    await page.getByLabel('Dynamic draft').fill('Dynamic input'); await text('Dynamic: Dynamic input');
    await click('Toggle controls');
    assert.equal(await page.getByRole('button',{name:'Dynamic increment'}).count(),0);
  }
  await page.locator('.owned').first().getByRole('button',{name:'Component increment'}).click();
  await page.locator('.owned').first().getByText('Local: 1',{exact:true}).waitFor();
  await stable.getByText('Local: 1',{exact:true}).waitFor();
  for (let cycle=0; cycle<2; cycle++) {
    await click('Toggle component child');
    const child = page.locator('.child-host .owned');
    await child.getByText('Local: 0',{exact:true}).waitFor();
    await child.getByRole('button',{name:'Component increment'}).click();
    await child.getByText('Local: 1',{exact:true}).waitFor();
    await click('Toggle component child');
    assert.equal(await child.count(),0);
  }
  await click('Bubbling child'); await text('Bubbled: 10');
  await page.getByRole('button',{name:'Loop increment',exact:true}).nth(1).click(); await text('Loop taps: 1');
  await click('Grow loop');
  assert.equal(await page.getByRole('button',{name:'Loop increment',exact:true}).count(),3);
  await page.getByRole('button',{name:'Loop increment',exact:true}).nth(2).click(); await text('Loop taps: 2');
  await page.getByRole('button',{name:'Hover target',exact:true}).hover(); await text('Hovered: true');
  await page.locator('.scroll-target').hover(); await page.mouse.wheel(0,150); await text('Scrolled: true');
  await Promise.all([page.waitForURL('**/native?'),click('Native submit')]);
  assert.deepEqual(errors,[]); assert.deepEqual(violations,[]);
  negative = true;
  await page.goto(`${origin}/negative`);
  await click('Blocked');
  await page.waitForFunction(() => typeof window.inlineRan === 'undefined');
  assert.equal(await page.getByRole('button',{name:'Blocked'}).count(),1);
  // Both independent policy negatives must produce a violation, not silently pass.
  const deadline = Date.now() + 5000;
  while (violations.length < 2 && Date.now() < deadline) await new Promise(resolve => setTimeout(resolve,20));
  assert(violations.includes('script-src-elem'), JSON.stringify(violations));
  assert(violations.includes('script-src-attr'), JSON.stringify(violations));
  console.log('PASS strict CSP: original probe; Counter Add/Reset; root arguments/input/change/checkbox/keyboard/focus/blur/double click/submit/native validation; dynamic controls/remount and component ownership; no unexpected console/CSP errors; inline script/handler negatives blocked');
})().catch(error => {console.error(error);process.exitCode=1;}).finally(async () => {
  if(browser)await browser.close();
  await new Promise(resolve => server.close(resolve));
});
