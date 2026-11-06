// Real controls and authored expected values; no injected application state.
const assert = require('node:assert/strict');
const fs = require('node:fs'), path = require('node:path'), http = require('node:http');
const {spawnSync} = require('node:child_process'), {createRequire} = require('node:module');
const {createHash} = require('node:crypto');
assert(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT && process.env.JTML_BENCH_BROWSER,
  'Real Chromium/Playwright required; no skip');
const {chromium} = createRequire(path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT, 'package.json'))('playwright');
const repo = path.resolve(__dirname, '../..'), binary = process.env.JTML_BIN || path.join(repo, 'build/jtml');
const output = path.resolve(process.env.JTML_COMPONENT_FRESH_OUT || path.join(repo, 'dist/component-action-freshness'));
fs.mkdirSync(output, {recursive:true});
const run = fs.mkdtempSync(path.join(output, 'run-'));
const report = {observations: [], errors: [], cleanBrowserExit:false, cleanServerExit:false};
let target, browser;
function compile(source, name, flags = []) {
  target = path.join(run, name);
  const result = spawnSync(binary, ['build', path.join(repo, source), '--target', 'browser', '--out', target, ...flags],
    {encoding:'utf8', timeout:60000});
  assert.equal(result.status, 0, result.stdout + result.stderr);
  return target;
}
const server = http.createServer((request, response) => {
  if (request.url === '/favicon.ico') return response.writeHead(204).end();
  const file = path.resolve(target, '.' + new URL(request.url, 'http://localhost').pathname);
  if (!file.startsWith(target + path.sep) || !fs.existsSync(file)) return response.writeHead(404).end();
  response.writeHead(200, {'Content-Security-Policy':"script-src 'self'; object-src 'none'",
    'Content-Type':file.endsWith('.js') ? 'text/javascript' : 'text/html'}).end(fs.readFileSync(file));
});
async function open() {
  const page = await browser.newPage(); page.setDefaultTimeout(5000);
  page.on('pageerror', error => report.errors.push(error.message));
  page.on('console', message => {if (message.type() === 'error') report.errors.push(message.text());});
  page.on('requestfailed', request => report.errors.push(request.url()));
  await page.addInitScript(() => {
    window.freshnessCSP = [];
    document.addEventListener('securitypolicyviolation', event => freshnessCSP.push(event.effectiveDirective));
  });
  await page.goto(`http://127.0.0.1:${server.address().port}/index.html`);
  return page;
}
(async () => {
  await new Promise((resolve, reject) => server.once('error', reject).listen(0, '127.0.0.1', resolve));
  browser = await chromium.launch({headless:true, executablePath:process.env.JTML_BENCH_BROWSER});
  report.browser = browser.version();
  const source = 'docs/architecture/browser-core-v1/probes/component-action-fresh.jtml';
  report.originalSourceSHA256 = createHash('sha256').update(fs.readFileSync(path.join(repo, source))).digest('hex');
  compile(source, 'original');
  const runtime = fs.readFileSync(path.join(target, 'jtml-runtime.js'));
  report.runtimeBytes = runtime.length; assert(runtime.length <= 273000);
  for (const name of ['index.html','jtml-runtime.js','components/index.js','app.js']) {
    const content = fs.readFileSync(path.join(target,name),'utf8');
    assert(!/\bnew\s+Function\b|\beval\s*\(/.test(content), name);
    assert(!content.includes('jtml-update-plans.js'),name);
  }
  let page = await open();
  assert.equal((await page.locator('.out').textContent()).trim(), '0|0');
  for (let i=0;i<3;i++) {
    await page.getByRole('button',{name:'Step',exact:true}).click();
    const actual = await page.locator('.out').textContent();
    report.observations.push({case:'original',phase:i+1,actual,expected:'2|2'});
    assert.equal(actual.trim(), '2|2');
  }
  assert.deepEqual(await page.evaluate(()=>freshnessCSP), []);
  await page.close();
  compile('tests/fixtures/production/component-action-freshness/index.jtml', 'controls');
  page = await open();
  const a = page.locator('.probe[data-label="A"]'), b = page.locator('.probe[data-label="B"]');
  assert.equal(await a.locator('.values').textContent(), '0|0|0|0|0|0|0');
  assert.equal(await b.locator('.values').textContent(), '10|0|0|0|20|40|0');
  await page.evaluate(() => {
    const roots = [...document.querySelectorAll('.probe')];
    window.freshnessNodes = roots.flatMap(root => [root, ...root.querySelectorAll('*')]);
    window.freshnessInstances = jtml.getComponentInstances().slice();
    window.freshnessScopes = freshnessInstances.map(instance => instance.scope);
  });
  for (const [button, expected] of [
    ['Step','2|2|4|8|4|8|0'], ['Step','4|6|12|16|8|16|0'],
    ['Shadow x','4|8|100|16|8|16|0'], ['Shadow getter','4|77|16|16|8|16|0'],
    ['Nested','4|8|4|16|8|16|1001'], ['Iterator','4|8|20|16|8|16|1001'],
    ['While','3|6|12|16|6|12|1001'], ['Index','1|2|12|16|2|4|1001'],
    ['Change hidden','1|2|12|16|2|4|1001'], ['Read hidden','1|6|12|16|2|4|1001']
  ]) {
    await a.getByRole('button',{name:button,exact:true}).click();
    const actual = await a.locator('.values').textContent();
    report.observations.push({case:button,actual,expected}); assert.equal(actual,expected);
    assert.equal(await b.locator('.values').textContent(),'10|0|0|0|20|40|0');
    assert(await page.evaluate(() => freshnessNodes.every(node => node.isConnected) &&
      freshnessInstances.every((instance,index) => jtml.getComponentInstances().includes(instance) &&
        instance.scope === freshnessScopes[index])));
    assert.equal(await page.evaluate(()=>jtml.state.x),500);
  }
  assert.equal(await a.locator('.row-value').textContent(),'4');
  await page.evaluate(() => {
    window.oldFreshInstance = jtml.getComponentInstances().find(i=>i.scope?.label==='A');
    window.oldFreshButton = document.querySelector('.probe[data-label="A"] button');
  });
  await page.getByRole('button',{name:'Toggle A',exact:true}).click();
  assert.equal(await a.count(),0);
  await page.evaluate(()=>oldFreshButton.click());
  assert.equal(await page.evaluate(()=>jtml.getComponentInstances().includes(oldFreshInstance)),false);
  await page.getByRole('button',{name:'Toggle A',exact:true}).click();
  assert.equal(await a.locator('.values').textContent(),'0|0|0|0|0|0|0');
  assert(await page.evaluate(()=>jtml.getComponentInstances().find(i=>i.scope?.label==='A') !== oldFreshInstance));
  await page.evaluate(()=>oldFreshButton.click());
  assert.equal(await a.locator('.values').textContent(),'0|0|0|0|0|0|0');
  await a.getByRole('button',{name:'Step',exact:true}).click();
  assert.equal(await a.locator('.values').textContent(),'2|2|4|8|4|8|0');
  assert.equal(await b.locator('.values').textContent(),'10|0|0|0|20|40|0');
  assert.deepEqual(await page.evaluate(()=>freshnessCSP),[]);
  assert.equal(await page.evaluate(()=>jtml.browserBuild.cleanProduction),true);
  assert.equal(await page.evaluate(()=>!!(jtml.directComponentCreateFallback||jtml.directComponentPatchFallback||
    jtml.directComponentBodyPlanFallback||jtml.directComponentStaticUpdateError)),false);
  await page.close(); report.lifecycle='fresh remount, detached controls inert, sibling/instance/scope/DOM identity retained';
  // Existing migration modes retain their cached-read behavior, independently.
  for (const mode of ['compat','debug']) {
    compile(source, mode, ['--'+mode]); page = await open();
    await page.getByRole('button',{name:'Step',exact:true}).click();
    assert.equal((await page.locator('.out').textContent()).trim(),'0|2');
    await page.close();
  }
  assert.deepEqual(report.errors,[]);
})().catch(error=>{report.failure=String(error);process.exitCode=1;}).finally(async()=>{
  if(browser){await browser.close();report.cleanBrowserExit=true;}
  if(server.listening){await new Promise(resolve=>server.close(resolve));report.cleanServerExit=true;}
  fs.writeFileSync(path.join(output,'results.json'),JSON.stringify(report,null,2)+'\n');
  console.log(JSON.stringify(report,null,2));
});
