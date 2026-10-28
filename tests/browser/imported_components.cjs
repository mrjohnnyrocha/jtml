// Real compiler + Chromium; no compatibility assets or fake import declarations.
const assert=require('node:assert/strict');
const fs=require('node:fs'),path=require('node:path'),http=require('node:http'),crypto=require('node:crypto');
const {spawnSync}=require('node:child_process'),{createRequire}=require('node:module');
const repo=path.resolve(__dirname,'../..'),compiler=process.env.JTML_BIN||path.join(repo,'build/jtml');
assert(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT&&process.env.JTML_BENCH_BROWSER,'real Chromium required; no skip');
const {chromium}=createRequire(path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT,'package.json'))('playwright');
const out=path.resolve(process.env.JTML_IMPORTED_COMPONENTS_OUT||path.join(repo,'dist/imported-components'));
fs.mkdirSync(out,{recursive:true});const work=fs.mkdtempSync(path.join(out,'attempt-'));
const report={compilerSHA256:crypto.createHash('sha256').update(fs.readFileSync(compiler)).digest('hex'),commands:[],checks:[]};
function run(args){const r=spawnSync(compiler,args,{encoding:'utf8',timeout:60000});report.commands.push({args,exit:r.status,stdout:r.stdout,stderr:r.stderr});assert.equal(r.status,0,r.stdout+r.stderr);return r.stdout;}
let server,browser;
(async()=>{
 report.version=run(['--version']).trim();run(['new','app',path.join(work,'starter')]);
 for(const [id,source]of [['starter',path.join(work,'starter')],['example',path.join(repo,'examples/friendly_module_component.jtml')],['nested',path.join(repo,'tests/fixtures/production/imported-components/index.jtml')]]){
  const entry=source.endsWith('.jtml')?source:path.join(source,'index.jtml');
  run(['check',entry]);run(['lint',entry]);const dest=path.join(work,'output',id);run(['build',source,'--target','browser','--out',dest]);
  for(const f of ['index.html','jtml-runtime.js','components/index.js','app.js'])assert(fs.statSync(path.join(dest,f)).isFile(),f);
  assert(!fs.existsSync(path.join(dest,'jtml-update-plans.js')));
  for(const f of ['index.html','jtml-runtime.js','components/index.js','app.js']){
   const text=fs.readFileSync(path.join(dest,f),'utf8');assert(!/\beval\s*\(|\bnew\s+Function\b/.test(text),f);
   assert(!text.includes('friendly+import-stubs')&&!text.includes('jtml-update-plans.js'),f);
  }
  const modules=fs.readFileSync(path.join(dest,'components/index.js'),'utf8');
  for(const forbidden of ['__jtml_static_update_plans','rootCreateOperations','bodyHex','bodySource'])assert(!modules.includes(forbidden),forbidden);
 }
 const root=path.join(work,'output');server=http.createServer((req,res)=>{
  const p=new URL(req.url,'http://localhost').pathname,f=path.resolve(root,'.'+p+(p.endsWith('/')?'index.html':''));
  if(!f.startsWith(root+path.sep)||!fs.existsSync(f)||!fs.statSync(f).isFile()){res.writeHead(404).end();return;}
  res.setHeader('Content-Security-Policy',"script-src 'self'");res.setHeader('Content-Type',f.endsWith('.js')?'text/javascript':'text/html');res.end(fs.readFileSync(f));
 });await new Promise(r=>server.listen(0,'127.0.0.1',r));
 browser=await chromium.launch({headless:true,executablePath:process.env.JTML_BENCH_BROWSER});report.browser=browser.version();
 const context=await browser.newContext();await context.tracing.start({screenshots:true,snapshots:true});
 const page=await context.newPage(),errors=[],violations=[],requests=[],sockets=[];page.setDefaultTimeout(7000);
 page.on('pageerror',e=>errors.push(String(e)));page.on('console',m=>{if(m.type()==='error')errors.push(m.text());});
 page.on('request',r=>requests.push(r.url()));page.on('websocket',s=>sockets.push(s.url()));
 await page.exposeFunction('importCSP',v=>violations.push(v));await page.addInitScript(()=>document.addEventListener('securitypolicyviolation',e=>window.importCSP(e.effectiveDirective)));
 // Guard execution, not DOM spelling: generated and interpreted text share markers.
 await page.addInitScript(()=>{
  const forbidden=['renderStaticComponentRoots','renderStaticComponentTextNode','renderStaticComponentButtonNode','renderStaticComponentElementNode','renderStaticComponentRegionNode','renderStaticComponentSlotNode','renderStaticComponentNestedNode','executeComponentPatchOperation','replaceDirectComponentNode'];
  const guard=fn=>function(...args){const helpers=Object.assign({},args[3]);for(const name of forbidden)helpers[name]=()=>{throw Error('Forbidden production fallback helper: '+name);};args[3]=helpers;return fn.apply(this,args);};
  for(const name of ['__jtml_static_component_modules','__jtml_static_update_functions']){
   let registry;Object.defineProperty(window,name,{configurable:true,get:()=>registry,set:value=>{registry=value;for(const key of Object.keys(value||{})){if(name==='__jtml_static_component_modules'){value[key].create=guard(value[key].create);}else value[key]=guard(value[key]);}}});
  }
 });
 const origin=`http://127.0.0.1:${server.address().port}`,text=t=>page.getByText(t,{exact:true}).waitFor(),click=n=>page.getByRole('button',{name:n,exact:true}).click();
 try{
  await page.goto(origin+'/starter/');await text('Count: 0');await click('Add one');await text('Count: 1');
  await page.getByRole('heading',{name:'Interactive state',exact:true}).waitFor();await page.getByRole('heading',{name:'Next steps',exact:true}).waitFor();
  await page.screenshot({path:path.join(work,'starter.png')});report.checks.push('generated starter: real exported Card, props, slots and reactive slot action');
  await page.goto(origin+'/example/');await page.getByRole('heading',{name:'Reusable module',exact:true}).waitFor();await text('This Card component lives in examples/modules/card.jtml.');report.checks.push('existing Friendly imported-component example');
  await page.goto(origin+'/nested/');const first=page.getByRole('region',{name:'First',exact:true}),second=page.getByRole('region',{name:'Second',exact:true});
  const identity=await first.elementHandle();await first.getByText('Badge: First',{exact:true}).waitFor();await second.getByText('Badge: Second',{exact:true}).waitFor();
  await first.getByText('Slot: 0',{exact:true}).waitFor();
  await first.getByText('First extra',{exact:true}).waitFor();await second.getByText('Second extra',{exact:true}).waitFor();assert.equal(await first.getByText('Second extra',{exact:true}).count(),0);
  await first.getByRole('button',{name:'Increment',exact:true}).click();await first.getByText('Local: 1',{exact:true}).waitFor();await second.getByText('Local: 0',{exact:true}).waitFor();
  await first.getByText('Slot: 0',{exact:true}).waitFor();
  await click('Bump slot');await first.getByText('Slot: 1',{exact:true}).waitFor();await second.getByText('Second slot',{exact:true}).waitFor();
  await click('Rename first');const renamed=page.getByRole('region',{name:'Renamed',exact:true});await renamed.getByText('Badge: Renamed',{exact:true}).waitFor();await renamed.getByText('Local: 1',{exact:true}).waitFor();await renamed.getByText('Slot: 1',{exact:true}).waitFor();assert(await renamed.evaluate((e,original)=>e===original,identity));await second.getByText('Badge: Second',{exact:true}).waitFor();
  const state=await page.evaluate(()=>({clean:jtml.browserBuild.cleanProduction,fallback:['directComponentPatchFallback','directComponentCreateFallback','directComponentBodyPlanFallback','directComponentStaticUpdateError'].filter(k=>jtml[k])}));assert.equal(state.clean,true);assert.deepEqual(state.fallback,[]);
  await page.screenshot({path:path.join(work,'nested.png')});await page.reload();await page.getByRole('region',{name:'First',exact:true}).getByText('Local: 0',{exact:true}).waitFor();
  report.checks.push('re-export/transitive import: real nested props, independent state, retained identity, parent slots and reload');
  assert.deepEqual(errors,[]);assert.deepEqual(violations,[]);assert.deepEqual(sockets,[]);assert(requests.every(u=>u.startsWith(origin+'/'))&&!requests.some(u=>u.includes('/api/')));
 }finally{report.errors=errors;report.violations=violations;report.finalText=await page.locator('body').innerText();report.finalHTML=await page.locator('body').innerHTML();report.runtime=await page.evaluate(()=>({create:jtml.directComponentStaticCreate,update:jtml.directComponentCompiledUpdatePlan,definitions:jtml.getComponentDefinitions().map(d=>({name:d.name,moduleId:d.moduleId,nodes:d.bodyPlan.length})),plans:__jtml_static_component_plan_index.components}));await page.screenshot({path:path.join(work,'final.png')});await context.tracing.stop({path:path.join(work,'browser.zip')});await context.close();}
 report.pass=true;console.log('PASS imported Friendly components: generated starter, existing example, re-exported nested props/state/slots/identity; strict CSP; clean assets; no fallbacks');
})().catch(e=>{report.pass=false;report.error=String(e);console.error(e);process.exitCode=1;}).finally(async()=>{if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));fs.writeFileSync(path.join(work,'result.json'),JSON.stringify(report,null,2)+'\n');});
