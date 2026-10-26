// Browser observations only: actual generated production assets, real DOM actions.
const assert = require('node:assert/strict');
const fs = require('node:fs'), path = require('node:path'), http = require('node:http');
const {spawnSync} = require('node:child_process');
const {createRequire} = require('node:module');
const repo = path.resolve(__dirname, '../..'), out = path.join(repo, 'dist/root-conditional-mount');
assert(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT && process.env.JTML_BENCH_BROWSER, 'real Chromium required; no skip');
const {chromium} = createRequire(path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT,'package.json'))('playwright');
for (const [name, source] of Object.entries({probe:'apps/releasedesk/probes/root-conditional-component.jtml', siblings:'tests/fixtures/production/root-conditional-mount/index.jtml'})) {
  const build = spawnSync(process.env.JTML_BIN || path.join(repo,'build/jtml'), ['build',path.join(repo,source),'--target','browser','--out',path.join(out,name)],{encoding:'utf8'});
  assert.equal(build.status,0,build.stdout+build.stderr);
  for (const asset of ['index.html','jtml-runtime.js','components/index.js','app.js']) {
    const text=fs.readFileSync(path.join(out,name,asset),'utf8');
    assert(!/\bnew\s+Function\b|\beval\s*\(|\son[a-z]+\s*=/i.test(text), asset);
    if (asset==='index.html') assert(!/<script\b(?![^>]*\bsrc=)(?![^>]*\btype="application\/json")/i.test(text));
  }
  assert(fs.statSync(path.join(out,name,'jtml-runtime.js')).size<=272000);
}
// Independent rejection: root loops have no per-item component ownership plan.
const rejected = path.join(out,'rejected');fs.rmSync(rejected,{recursive:true,force:true});
const negative = spawnSync(process.env.JTML_BIN || path.join(repo,'build/jtml'),
  ['build',path.join(repo,'tests/fixtures/production/root-conditional-mount/root-loop-rejected.jtml'),'--target','browser','--out',rejected],{encoding:'utf8'});
assert.notEqual(negative.status,0);
assert(negative.stderr.includes('per-item instance ownership') && negative.stderr.includes('root-loop-rejected.jtml:10:'), negative.stderr);
for(const file of ['index.html','jtml-runtime.js','components/index.js','app.js'])assert(!fs.existsSync(path.join(rejected,file)),'unsupported root loop must not emit runnable assets');
const server=http.createServer((req,res)=>{
  const file=path.resolve(out,'.'+new URL(req.url,'http://localhost').pathname+(req.url.endsWith('/')?'index.html':''));
  res.setHeader('Content-Security-Policy',"script-src 'self'");
  if(!file.startsWith(out+path.sep)||!fs.existsSync(file)){res.writeHead(404);res.end();return;}
  res.setHeader('Content-Type',file.endsWith('.js')?'text/javascript':'text/html');res.end(fs.readFileSync(file));
});
let browser;
(async()=>{
  await new Promise(r=>server.listen(0,'127.0.0.1',r));
  browser=await chromium.launch({headless:true,executablePath:process.env.JTML_BENCH_BROWSER});
  const page=await browser.newPage(), errors=[],violations=[];
  page.setDefaultTimeout(5000);
  page.on('pageerror',e=>errors.push(e.message));page.on('console',m=>{if(m.type()==='error')errors.push(m.text());});
  await page.exposeFunction('observeCSP',v=>violations.push(v));
  await page.addInitScript(()=>document.addEventListener('securitypolicyviolation',e=>window.observeCSP(e.effectiveDirective)));
  const click=name=>page.getByRole('button',{name,exact:true}).click();
  const text=value=>page.getByText(value,{exact:true}).waitFor();
  const load=async name=>{
    const response=await page.goto(`http://127.0.0.1:${server.address().port}/${name}/`);
    assert.equal(response.headers()['content-security-policy'],"script-src 'self'");
    await page.waitForFunction(()=>window.jtml?.browserBuild?.cleanProduction);
  };
  const registry=()=>page.evaluate(()=>({dynamic:jtml.getRuntimeRegistrySnapshot(), static:jtml.getComponentInstances().map(i=>({id:i.id,connected:!!i.element?.isConnected}))}));
  const check=async()=>{
    assert.deepEqual(errors,[]);assert.deepEqual(violations,[]);
    const state=await page.evaluate(()=>({registry:jtml.getRuntimeRegistrySnapshot(), dom:[...document.querySelectorAll('[data-jtml-direct-instance]')].map(e=>e.dataset.jtmlDirectInstance).sort(),
      fallback:['directComponentPatchFallback','directComponentCreateFallback','directComponentBodyPlanFallback','directComponentStaticUpdateError','directComponentStaticDestroyError'].filter(k=>jtml[k]),
      leaks:['registryLeaks','scopeLeaks','handlerLeaks','dispatchLeaks'].flatMap(k=>jtml.directComponentDestroyLifecycle?.[k]||[])}));
    assert.deepEqual(Object.keys(state.registry.dynamicGenerations).sort(),state.dom,'live dynamic DOM and registry must agree exactly');
    assert.equal(new Set(state.dom).size,state.dom.length);assert.deepEqual(state.fallback,[],JSON.stringify(await page.evaluate(()=>jtml.directComponentPatchFallback)));assert.deepEqual(state.leaks,[]);
    assert((await registry()).static.every(i=>i.connected),'no dormant/disconnected instance in active registry');
    return state.registry;
  };
  await load('probe');
  assert.deepEqual((await check()).dynamicGenerations,{});
  assert.deepEqual((await registry()).static,[]);
  let previousGeneration=0;
  for(let cycle=0;cycle<4;cycle++){
    await click('Toggle');await text('Child: 0');
    await click('Child increment');await text('Child: 1');
    await click('Child increment');await text('Child: 2');
    const live=await check();assert.equal(Object.keys(live.dynamicGenerations).length,1);
    const generation=Object.values(live.dynamicGenerations)[0];assert(generation>previousGeneration);previousGeneration=generation;
    await page.evaluate(()=>{window.detachedButton=document.querySelector('[data-jtml-direct-component-action]');});
    await click('Toggle');assert.equal(await page.getByRole('button',{name:'Child increment'}).count(),0);
    assert.deepEqual((await check()).dynamicGenerations,{});
    // Detached retained controls must not dispatch or resurrect scopes/instances.
    await page.evaluate(()=>detachedButton.click());assert.deepEqual((await check()).dynamicGenerations,{});
  }
  await load('siblings');
  const a=page.locator('.first .board'),b=page.locator('.second .board');
  const row=(board,id)=>board.locator(`.row[data-id="${id}"]`);
  await b.getByRole('button',{name:'Increment',exact:true}).first().click();await row(b,'a').getByText('Count: 1',{exact:true}).waitFor();
  await page.evaluate(()=>{window.keptB=[...document.querySelectorAll('.second, .second *')];});
  const initial=await check();
  for(let cycle=0;cycle<3;cycle++){
    await click('Toggle first');await row(a,'a').getByText('Count: 0',{exact:true}).waitFor();
    await row(a,'a').getByRole('button',{name:'Increment'}).click();await row(a,'a').getByText('Count: 1',{exact:true}).waitFor();
    assert.equal(await row(a,'a').getByLabel('Draft').inputValue(),'Seed');
    await row(a,'a').getByLabel('Draft').fill('Retained draft');
    await page.evaluate(()=>{window.keptA=[...document.querySelectorAll('.first .row')];});
    await a.getByRole('button',{name:'Reorder'}).click();
    assert.deepEqual(await a.locator('.row').evaluateAll(es=>es.map(e=>e.dataset.id)),['b','a']);
    assert(await page.evaluate(()=>keptA.every(e=>e.isConnected)),'keyed row DOM retained');
    assert.equal(await row(a,'a').getByLabel('Draft').inputValue(),'Retained draft');
    await row(a,'b').getByRole('button',{name:'Ping'}).click();await a.getByText('Seen: b',{exact:true}).waitFor();await a.getByText('Pings: 1',{exact:true}).waitFor();
    await a.getByRole('button',{name:'Remove B'}).click();await check();
    await a.getByRole('button',{name:'Restore'}).click();await row(a,'b').getByText('Count: 0',{exact:true}).waitFor();
    const sibling=page.locator('.first > [data-jtml-component=Counter]');
    await sibling.getByRole('button',{name:'Increment'}).click();await sibling.getByText('Count: 1',{exact:true}).waitFor();
    await click('Rename sibling');await sibling.getByText('Label: Sibling changed',{exact:true}).waitFor();
    await sibling.getByText('Count: 1',{exact:true}).waitFor();
    await click('Conditional root action');
    await click('Grow root loop');assert.equal(await page.getByRole('button',{name:'Loop root action',exact:true}).count(),3);
    await page.getByRole('button',{name:'Loop root action',exact:true}).last().click();await text(`Root clicks: ${cycle*2+2}`);
    assert(await page.evaluate(()=>keptA.filter(e=>e.dataset.id==='a').every(e=>e.isConnected)),'root updates retain component keyed DOM');
    assert.equal(await row(a,'a').getByLabel('Draft').inputValue(),'Retained draft');
    await check();
    await click('Toggle first');assert.deepEqual(await check(),initial);
    assert(await page.evaluate(()=>keptB.every(e=>e.isConnected)),'unrelated sibling DOM retained');
    await row(b,'a').getByText('Count: 1',{exact:true}).waitFor();
  }
  await click('Toggle second');await check();
  await click('Toggle second');await row(b,'a').getByText('Count: 0',{exact:true}).waitFor();await check();
  console.log('PASS root conditional mounting: unchanged probe, repeated fresh state, siblings, nested keyed retention/removal, callback cleanup, detached controls, root conditional/loop events and strict HTTP script CSP; no console/CSP errors or registry/scope/handler/dispatch leaks');
})().catch(e=>{console.error(e);process.exitCode=1;}).finally(async()=>{if(browser)await browser.close();await new Promise(r=>server.close(r));});
