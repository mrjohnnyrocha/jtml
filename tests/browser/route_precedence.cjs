// Real production assets and DOM actions; declaration order is the route contract.
const assert = require('node:assert/strict');
const fs = require('node:fs'), path = require('node:path'), http = require('node:http');
const {spawnSync} = require('node:child_process'), {createRequire} = require('node:module');
const repo = path.resolve(__dirname, '../..'), out = path.join(repo, 'dist/route-precedence');
assert(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT && process.env.JTML_BENCH_BROWSER, 'real Chromium required; no skip');
const {chromium} = createRequire(path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT, 'package.json'))('playwright');
const built = spawnSync(process.env.JTML_BIN || path.join(repo,'build/jtml'), ['build',path.join(repo,'tests/fixtures/production/route-precedence/index.jtml'),'--target','browser','--out',out], {encoding:'utf8'});
assert.equal(built.status,0,built.stdout+built.stderr);
const server = http.createServer((req,res)=>{
  const url = new URL(req.url,'http://localhost');
  const file = path.resolve(out,'.'+(url.pathname==='/'?'/index.html':url.pathname));
  res.setHeader('Content-Security-Policy',"script-src 'self'");
  if(!file.startsWith(out+path.sep)||!fs.existsSync(file)){res.writeHead(404).end();return;}
  res.setHeader('Content-Type',file.endsWith('.js')?'text/javascript':'text/html');
  res.end(fs.readFileSync(file));
});
let browser;
(async()=>{
  await new Promise(r=>server.listen(0,'127.0.0.1',r));
  browser = await chromium.launch({headless:true,executablePath:process.env.JTML_BENCH_BROWSER});
  const page = await browser.newPage(), errors=[];
  page.on('pageerror',e=>errors.push(e.message));
  page.setDefaultTimeout(5000);
  const origin=`http://127.0.0.1:${server.address().port}`;
  const expectedOrder=['/','/user/new','/user/:id','*'];
  const current=async (url,pattern,heading)=>{
    await page.waitForFunction(url=>window.jtml?.getCurrentRoute?.()?.path===url,url);
    assert.equal((await page.evaluate(()=>jtml.getCurrentRoute())).pattern,pattern);
    assert.deepEqual(await page.locator('h1:visible').allTextContents(),[heading]);
    assert.equal(await page.locator('[data-jtml-route]:visible').count(),1);
    assert.deepEqual(await page.evaluate(()=>jtml.getRoutes().map(r=>r.path)),expectedOrder);
    assert.deepEqual(await page.evaluate(()=>JSON.parse(document.querySelector('#__jtml_client_manifest').textContent).routes.map(r=>r.path)),expectedOrder);
  };
  // Direct document loads, including the unfragmented home and encoded params.
  for(const [hash,url,pattern,heading] of [['','/','/','Home'],['#/user/new','/user/new','/user/new','New user form'],['#/user/Ada%20L','/user/Ada%20L','/user/:id','Profile Ada L'],['#/absent','/absent','*','Missing page']]){
    await page.goto(origin+'/'+hash);await current(url,pattern,heading);
  }
  await page.goto(origin+'/#/');await current('/','/','Home');
  const click=name=>page.getByRole('button',{name,exact:true}).click();
  const text=value=>page.getByText(value,{exact:true}).waitFor();
  await page.evaluate(()=>{window.routeElements=[...document.querySelectorAll('[data-jtml-route]')];});
  await click('Home child');await click('Home sibling');
  await text('Home child: 1');await text('Home sibling: 1');
  await page.getByRole('link',{name:'New user',exact:true}).click();await current('/user/new','/user/new','New user form');
  await click('New child');await text('New child: 1');
  await page.getByRole('link',{name:'Home',exact:true}).filter({visible:true}).click();await current('/','/','Home');
  // Routes retain hidden pages; independent conditional children remount fresh.
  await text('Home child: 1');await text('Home sibling: 1');
  await click('Toggle child');await page.getByText('Home child: 1',{exact:true}).waitFor({state:'detached'});
  await click('Toggle child');await text('Home child: 0');await text('Home sibling: 1');
  await click('Home child');await text('Home child: 1');
  for(let cycle=0;cycle<3;cycle++){
    await page.getByRole('link',{name:'Ada',exact:true}).click();await current('/user/ada','/user/:id','Profile ada');
    await click('Profile child');await text(`Profile child: ${cycle+1}`);
    await page.evaluate(()=>{location.hash='/unknown';});await current('/unknown','*','Missing page');
    await page.getByRole('link',{name:'Home',exact:true}).filter({visible:true}).click();await current('/','/','Home');
    await text('Home child: 1');await text('Home sibling: 1');
  }
  // Public programmatic navigation, including an unchanged hash, must use
  // the same first-match table without replacing retained page instances.
  await page.evaluate(()=>jtml.navigate('/user/new'));await current('/user/new','/user/new','New user form');
  await text('New child: 1');
  await page.evaluate(()=>jtml.navigate('/user/new'));await current('/user/new','/user/new','New user form');
  await text('New child: 1');
  await page.evaluate(()=>jtml.navigate('/'));await current('/','/','Home');
  await text('Home sibling: 1');
  assert(await page.evaluate(()=>routeElements.every((el,i)=>el.isConnected && el === document.querySelectorAll('[data-jtml-route]')[i])));
  assert.deepEqual(errors,[]);
  console.log('PASS route precedence: direct/hash/link/parameter navigation, fallback, retained independent routes and conditional child remount');
})().catch(e=>{console.error(e);process.exitCode=1;}).finally(async()=>{if(browser)await browser.close();await new Promise(r=>server.close(r));});
