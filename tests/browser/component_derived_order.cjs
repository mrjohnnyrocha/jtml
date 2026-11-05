// Original failing source is preserved; the positive uses dependency-ordered
// getters. Browser values are independently calculated, never read from plans.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),http=require('node:http');
const {spawnSync}=require('node:child_process'),{createRequire}=require('node:module');
assert(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT&&process.env.JTML_BENCH_BROWSER,'Real Chromium required; no skip');
const {chromium}=createRequire(path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT,'package.json'))('playwright');
const repo=path.resolve(__dirname,'../..'),binary=process.env.JTML_BIN||path.join(repo,'build/jtml');
const out=path.resolve(process.env.JTML_COMPONENT_ORDER_OUT||path.join(repo,'dist/component-derived-order'));
const report={checks:[],errors:[],cleanBrowserExit:false,cleanServerExit:false};let target,browser;
function compile(source,destination,flags=[]) {
  const r=spawnSync(binary,['build',source,'--target','browser','--out',destination,...flags],{encoding:'utf8',timeout:60000});
  assert(!r.error&&!r.signal,'compiler infrastructure: '+r.error);return r;
}
const server=http.createServer((q,r)=>{
  if(q.url==='/favicon.ico')return r.writeHead(204).end();
  const file=path.resolve(target,'.'+new URL(q.url,'http://localhost').pathname);
  if(!file.startsWith(target+path.sep)||!fs.existsSync(file))return r.writeHead(404).end();
  r.writeHead(200,{'Content-Security-Policy':"script-src 'self'",'Content-Type':file.endsWith('.js')?'text/javascript':'text/html'}).end(fs.readFileSync(file));
});
(async()=>{
  fs.mkdirSync(out,{recursive:true});
  const source=path.join(out,'ordered.jtml');
  fs.writeFileSync(source,'jtml 2\nmake Card\n  let x = 1\n  get z = x + 1\n  get a = z * 2\n  when step\n    x += 1\n  box class "out"\n    text "{a}|{z}"\n  button "Step" click step\npage\n  Card\n  Card\n');
  target=path.join(out,'ordered');const good=compile(source,target);assert.equal(good.status,0,good.stdout+good.stderr);
  const assets=['index.html','jtml-runtime.js','components/index.js','app.js'];
  const before=assets.map(name=>fs.readFileSync(path.join(target,name)));
  const original=path.join(repo,'docs/architecture/browser-core-v1/probes/component-forward-chain.jtml');
  for(const destination of [path.join(out,'rejected'),target]) {
    const denied=compile(original,destination);report.rejection={exit:denied.status,diagnostic:denied.stderr};
    assert.notEqual(denied.status,0,'nonpreceding component getter must fail closed');
    assert.match(denied.stderr,/E_COMPONENT_DERIVED_ORDER/);assert.match(denied.stderr,/component-forward-chain\.jtml:4:3/);
    assert.match(denied.stderr,/component Card/);assert.match(denied.stderr,/dependency order/);
  }
  for(const [i,name]of assets.entries()){
    assert(!fs.existsSync(path.join(out,'rejected',name)));assert.deepEqual(fs.readFileSync(path.join(target,name)),before[i]);
    assert(!/\bnew\s+Function\b|\beval\s*\(/.test(before[i].toString()));
  }
  assert(before[1].length<=273000);report.runtimeBytes=before[1].length;
  report.checks.push('unchanged counterexample rejects at original source; no assets; successful assets preserved');
  await new Promise((ok,no)=>server.once('error',no).listen(0,'127.0.0.1',ok));
  browser=await chromium.launch({headless:true,executablePath:process.env.JTML_BENCH_BROWSER});report.browser=browser.version();
  const page=await browser.newPage();page.setDefaultTimeout(4000);
  page.on('pageerror',e=>report.errors.push(e.message));page.on('console',m=>{if(m.type()==='error')report.errors.push(m.text());});
  await page.addInitScript(()=>{window.orderCSP=[];document.addEventListener('securitypolicyviolation',e=>orderCSP.push(e.effectiveDirective));});
  await page.goto(`http://127.0.0.1:${server.address().port}/index.html`);
  const view=()=>page.locator('.out').allTextContents();assert.deepEqual(await view(),['4|2','4|2']);
  await page.evaluate(()=>window.orderNodes=[...document.querySelectorAll('.out')]);
  for(let x=2;x<=4;x++){
    await page.getByRole('button',{name:'Step',exact:true}).nth(0).click();
    assert.deepEqual(await view(),[`${(x+1)*2}|${x+1}`,'4|2']);
    assert(await page.evaluate(()=>orderNodes.every((e,i)=>e===document.querySelectorAll('.out')[i]&&e.isConnected)));
  }
  assert.equal(await page.evaluate(()=>jtml.browserBuild.cleanProduction),true);
  assert.deepEqual(await page.evaluate(()=>orderCSP),[]);assert.deepEqual(report.errors,[]);
  await page.close();report.checks.push('dependency-ordered component getters update repeatedly; sibling and DOM identity retained; strict CSP');
})().catch(e=>{report.failure=String(e);process.exitCode=1;}).finally(async()=>{
  if(browser){await browser.close();report.cleanBrowserExit=true;}
  if(server.listening){await new Promise(r=>server.close(r));report.cleanServerExit=true;}
  fs.mkdirSync(out,{recursive:true});fs.writeFileSync(path.join(out,'results.json'),JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(report,null,2));
});
