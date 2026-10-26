// Test driver only: no frontend code, state injection or mocked API responses.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const {spawn, spawnSync} = require('node:child_process');
const {createRequire} = require('node:module');
const repo = path.resolve(__dirname, '../..');
const driver = process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT;
if (!driver || !process.env.JTML_BENCH_BROWSER) throw new Error('Playwright root and real browser path are required; no skip');
const {chromium} = createRequire(path.resolve(driver, 'package.json'))('playwright');
const dist = path.resolve(process.argv[2] || path.join(repo, 'dist/releasedesk'));
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'releasedesk-e2e-'));
const db = path.join(temporary, 'issues.sqlite3');
let server, browser, activePage;
let log = '';
async function start(port = 0) {
  log = '';
  server = spawn('python3', [path.join(__dirname, 'server.py'), '--dist', dist, '--db', db, '--port', String(port)], {env: {...process.env, PYTHONDONTWRITEBYTECODE: '1'}});
  server.stdout.on('data', chunk => log += chunk);
  server.stderr.on('data', chunk => log += chunk);
  // Hosted macOS can cold-start Python/SQLite slowly; readiness is not an app interaction budget.
  const started = Date.now(), deadline = started + 60000;
  while (Date.now() < deadline) {
    const match = log.match(/http:\/\/127\.0\.0\.1:(\d+)/);
    if (match) {console.log(`Backend ready after ${Date.now() - started}ms`); return `http://127.0.0.1:${match[1]}`;}
    if (server.exitCode !== null) throw new Error(log);
    await new Promise(resolve => setTimeout(resolve, 50));
  }
  throw new Error('Backend readiness not reached within 60s (Python startup/SQLite initialization): ' + log);
}
async function stop() {
  if (!server || server.exitCode !== null) return;
  const exit = new Promise(resolve => server.once('exit', resolve));
  server.kill('SIGTERM');
  await exit;
}
function stored() {
  const script = 'import json,sqlite3,sys; c=sqlite3.connect(sys.argv[1]);c.row_factory=sqlite3.Row;print(json.dumps([dict(r) for r in c.execute("SELECT * FROM issues ORDER BY id")]))';
  const result = spawnSync('python3', ['-c', script, db], {encoding:'utf8'});
  assert.equal(result.status, 0, result.stderr);
  return JSON.parse(result.stdout);
}
async function visible(locator) { await locator.waitFor({state:'visible'}); }
(async () => {
  let origin = await start();
  const assets = [];
  const walk = directory => {for (const item of fs.readdirSync(directory, {withFileTypes:true})) {
    const file = path.join(directory,item.name); if(item.isDirectory())walk(file);else assets.push(path.relative(dist,file));
  }};
  walk(dist);
  assert.deepEqual(assets.sort(), ['app.js','components/index.js','index.html','jtml-runtime.js']);
  for (const file of assets) {
    const text = fs.readFileSync(path.join(dist,file),'utf8');
    assert(!/\bnew\s+Function\b|\beval\s*\(/.test(text), file);
    assert(!text.includes('jtml-update-plans.js'), file);
    assert(!/\son[a-z]+\s*=/i.test(text), 'inline event handler in ' + file);
    if (file === 'index.html') assert(!/<script\b(?![^>]*\bsrc=)(?![^>]*\btype="application\/json")/i.test(text), 'inline executable script');
  }
  browser = await chromium.launch({executablePath: process.env.JTML_BENCH_BROWSER, headless:true});
  const page = await browser.newPage({viewport:{width:1100,height:900}});
  activePage = page;
  page.setDefaultTimeout(10000);
  const unexpected = [], requests = [], csp = [];
  await page.exposeFunction('observeCSP', directive => csp.push(directive));
  await page.addInitScript(() => document.addEventListener('securitypolicyviolation', event => window.observeCSP(event.effectiveDirective)));
  let expectedHTTPFailure = false;
  page.on('websocket', socket => unexpected.push('unexpected WebSocket: ' + socket.url()));
  page.on('pageerror', error => unexpected.push(error.message));
  page.on('console', message => {if(message.type()==='error' && !(expectedHTTPFailure && /Failed to load resource.*(?:400|503|net::ERR_CONNECTION_REFUSED)/.test(message.text()))) unexpected.push(message.text());});
  page.on('request', request => requests.push({url:request.url(),method:request.method()}));
  let releaseLoad;
  const loadGate = new Promise(resolve => releaseLoad = resolve);
  await page.route('**/api/issues', async route => {if(route.request().method()==='GET') await loadGate; await route.continue();});
  const response = await page.goto(origin);
  const policy = response.headers()['content-security-policy'];
  assert(policy.includes("script-src 'self';"), policy);
  await visible(page.getByText('Loading issues…', {exact:true}));
  releaseLoad();
  await visible(page.getByTestId('empty-state'));
  await page.unroute('**/api/issues');
  assert.equal(stored().length,0);
  const postCount = () => requests.filter(request => request.method==='POST').length;
  assert.equal(await page.getByLabel('Severity',{exact:true}).inputValue(),'','severity has an explicit required choice');
  assert.equal(postCount(),0,'lazy POST must not run on startup');
  await page.getByRole('button',{name:'Save issue',exact:true}).click();
  assert.equal(postCount(),0,'native required validation prevents empty POST');
  await page.getByLabel('Title',{exact:true}).fill('macOS release regression');
  await page.getByRole('button',{name:'Save issue',exact:true}).click();
  assert.equal(postCount(),0,'description is required too');
  await page.getByLabel('Description',{exact:true}).fill('CLI fails before startup.\nReproduce on a clean machine.');
  await page.getByLabel('Severity',{exact:true}).selectOption('critical');
  assert.equal(postCount(),0,'editing must not send a mutation');
  let releaseSave;
  const saveGate = new Promise(resolve => releaseSave = resolve);
  await page.route('**/api/issues', async route => {if(route.request().method()==='POST') await saveGate; await route.continue();});
  // Keyboard submit goes through the real JTML form event.
  await page.getByLabel('Title',{exact:true}).focus();
  await page.keyboard.press('Enter');
  await visible(page.getByText('Saving issue…',{exact:true}));
  assert(await page.getByRole('button',{name:'Save issue',exact:true}).isDisabled(),'in-flight mutation disables repeat submission');
  assert.equal(stored().length,0,'loading is not a confirmed save');
  releaseSave();
  await visible(page.getByTestId('save-success'));
  await visible(page.getByRole('heading',{name:'Issue 1 — macOS release regression',exact:true}));
  await page.unroute('**/api/issues');
  assert.equal(page.url(),origin + '/','JTML submit must not navigate or reload the page');
  assert.equal(postCount(),1);
  assert.deepEqual(stored().map(({title,description,severity})=>({title,description,severity})), [{title:'macOS release regression',description:'CLI fails before startup.\nReproduce on a clean machine.',severity:'critical'}]);
  await page.getByRole('button',{name:'Clear text for next issue',exact:true}).click();
  assert.equal(await page.getByLabel('Title',{exact:true}).inputValue(),'');
  assert.equal(await page.getByLabel('Description',{exact:true}).inputValue(),'');
  assert.equal(await page.getByLabel('Severity',{exact:true}).inputValue(),'critical');
  await page.reload();
  await visible(page.getByRole('heading',{name:'Issue 1 — macOS release regression',exact:true}));
  assert.equal(await page.getByTestId('save-success').count(),0,'reload is not a new save confirmation');
  assert.equal(postCount(),1,'reload must not POST');
  console.log('PASS empty/loading, keyboard create, actual SQLite row, full reload, clean assets, no eager mutations');

  // Whitespace passes HTML required; the real server must still reject it.
  await page.getByLabel('Title',{exact:true}).fill('   ');
  await page.getByLabel('Description',{exact:true}).fill('Valid description');
  await page.getByLabel('Severity',{exact:true}).selectOption('medium');
  expectedHTTPFailure = true;
  const invalid = page.waitForResponse(response=>response.url().endsWith('/api/issues') && response.request().method()==='POST');
  await page.getByRole('button',{name:'Save issue',exact:true}).click();
  assert.equal((await invalid).status(),400);
  await visible(page.getByTestId('save-error'));
  assert.equal(stored().length,1);
  assert.equal(await page.getByTestId('save-success').count(),0);
  expectedHTTPFailure = false;

  // A genuine database failure, not a mock HTTP response or frontend state patch.
  fs.renameSync(db,db+'.backup'); fs.mkdirSync(db);
  try {
    expectedHTTPFailure = true;
    await page.getByRole('button',{name:'Reload issues',exact:true}).click();
    await visible(page.getByTestId('load-error'));
    await page.getByLabel('Title',{exact:true}).fill('Retry after database repair');
    const failure = page.waitForResponse(response=>response.url().endsWith('/api/issues') && response.request().method()==='POST');
    await page.getByRole('button',{name:'Save issue',exact:true}).click();
    assert.equal((await failure).status(),503);
    await visible(page.getByTestId('save-error'));
    assert.equal(await page.getByTestId('save-success').count(),0);
    assert.equal(await page.getByLabel('Title',{exact:true}).inputValue(),'Retry after database repair');
  } finally {fs.rmdirSync(db);fs.renameSync(db+'.backup',db);}
  expectedHTTPFailure = false;
  assert.equal(stored().length,1,'failed writes cannot create rows');
  await page.getByRole('button',{name:'Reload issues',exact:true}).click();
  await page.getByTestId('load-error').waitFor({state:'detached'});
  await page.getByRole('button',{name:'Save issue',exact:true}).click();
  await visible(page.getByTestId('save-success'));
  await visible(page.getByRole('heading',{name:'Issue 2 — Retry after database repair',exact:true}));
  assert.equal(stored().length,2);
  const samePort = Number(new URL(origin).port);
  await stop();
  expectedHTTPFailure = true;
  await page.getByRole('button',{name:'Reload issues',exact:true}).click();
  await visible(page.getByTestId('load-error'));
  expectedHTTPFailure = false;
  origin = await start(samePort);
  await page.getByRole('button',{name:'Reload issues',exact:true}).click();
  await page.getByTestId('load-error').waitFor({state:'detached'});
  await page.reload();
  await visible(page.getByRole('heading',{name:'Issue 1 — macOS release regression',exact:true}));
  await visible(page.getByRole('heading',{name:'Issue 2 — Retry after database repair',exact:true}));
  assert.equal(await page.getByTestId('issue').count(),2,'process restart retains both rows');
  assert(!requests.some(request=>/\/api\/(event|component-action)/.test(request.url)), 'no JTML live-backend fallback');
  const contract = await page.evaluate(()=>window.jtml.browserBuild);
  assert.equal(contract.mode,'prod');
  assert.equal(contract.cleanProduction,true);
  assert.equal(contract.compatibility,false);
  assert.equal(contract.debug,false);
  assert(!Object.hasOwn(contract.assets,'compatibilityUpdatePlans'));
  assert.deepEqual(unexpected,[],'unexpected console/page errors');
  assert.deepEqual(csp,[],'strict CSP violations');
  await page.screenshot({path:path.join(repo,'dist/releasedesk-e2e.png'),fullPage:true});
  await page.setViewportSize({width:390,height:844});
  assert(await page.evaluate(()=>document.documentElement.scrollWidth <= window.innerWidth),'mobile layout must not scroll horizontally');
  await page.screenshot({path:path.join(repo,'dist/releasedesk-mobile.png'),fullPage:true});
  console.log('PASS invalid submissions, real SQLite/API failure and stopped-server recovery, retained draft, retry, process restart, no unexpected errors');
})().catch(async error => {console.error(error.stack);console.error(log);if(activePage){console.error(await activePage.evaluate(()=>({text:document.body.innerText,state:window.jtml?.state})));fs.writeFileSync(path.join(repo,'dist/releasedesk-failure.html'),await activePage.content());await activePage.screenshot({path:path.join(repo,'dist/releasedesk-failure.png'),fullPage:true});}process.exitCode=1;}).finally(async()=>{
  if(browser)await browser.close(); await stop(); fs.rmSync(temporary,{recursive:true,force:true});
});
