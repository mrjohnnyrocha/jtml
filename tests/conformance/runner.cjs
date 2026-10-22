const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const {spawnSync} = require('node:child_process');
const {pathToFileURL} = require('node:url');
const {cases,generated,algebra} = require('./cases.cjs');
const {roots,checkPlans,compare}=require('./oracle.cjs');
const {chromium} = require(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT
 ? path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT,'node_modules/playwright') : 'playwright');
const root=path.resolve(__dirname,'../..');
const binary=path.resolve(process.env.JTML_BIN || path.join(root,'build/jtml'));
const probe=path.resolve(process.env.JTML_CONFORMANCE_PROBE || path.join(path.dirname(binary),'tests/jtml_conformance_probe'));
const output=path.resolve(process.env.JTML_CONFORMANCE_OUT || path.join(root,'dist/conformance'));
function command(executable,args,log) {
 const result=spawnSync(executable,args,{encoding:'utf8',timeout:20000,maxBuffer:16*1024*1024});
 fs.writeFileSync(log,(result.stdout||'')+(result.stderr||''));
 if(result.error || result.status!==0) throw new Error(`subprocess failed: ${executable} ${args.join(' ')}; status=${result.status} signal=${result.signal}; ${result.error||''}; log=${log}`);
}
function nativeStates(fixture,native) {
 return native.snapshots.map(snapshot=>snapshot.components.filter(c=>c.component==='Probe').map(c=>
  Object.fromEntries(fixture.fields.map(field=>[field,c.locals[field] && Object.hasOwn(c.locals[field],'value') ? c.locals[field].value : {$missing:true}]))));
}
async function browserTrace(browser,fixture,dist) {
 const page=await browser.newPage();
 page.setDefaultTimeout(5000);
 await page.addInitScript(()=>{window.conformanceCspViolations=[];document.addEventListener('securitypolicyviolation',event=>window.conformanceCspViolations.push(event.violatedDirective));});
 const errors=[],requests=[];
 page.on('pageerror',error=>errors.push(error.message));
 page.on('request',request=>{if(!request.url().startsWith(pathToFileURL(dist+path.sep).href)) requests.push(request.url());});
 try {
  // The page executes only compiler-produced external scripts; CSP prohibits eval.
  await page.goto(pathToFileURL(path.join(dist,'index.html')).href);
  await page.waitForFunction(()=>window.jtml?.getComponentInstances && document.querySelector('.probe'));
  if(fixture.retain) await page.evaluate(selector=>{window.conformanceRetained=[...document.querySelectorAll(selector)];},fixture.retain);
  const snapshot=()=>page.evaluate(({fields,views,retain})=>{
   const values=[...document.querySelectorAll('.probe')].map(element=>{
    const wrapper=element.closest('[data-jtml-instance], [data-jtml-direct-instance]');
    const id=wrapper.getAttribute('data-jtml-instance')||wrapper.getAttribute('data-jtml-direct-instance');
    const scope=window.jtml.findComponentInstance(id).scope;
    return {state:Object.fromEntries(fields.map(field=>[field,Object.hasOwn(scope,field)?scope[field]:{$missing:true}])),
     visible:Object.fromEntries([...fields.map(field=>[field,element.querySelector('.out-'+field)?.textContent ?? null]), ...Object.entries(views).map(([key,selector])=>[key,[...element.querySelectorAll(selector)].map(el=>el.textContent)])])};
   });
   return {values,retained:!retain||window.conformanceRetained.every((el,i)=>el.isConnected&&el===document.querySelectorAll(retain)[i]),fallback:!!(window.jtml.directComponentCreateFallback||window.jtml.directComponentPatchFallback||window.jtml.directComponentBodyPlanFallback||window.jtml.directComponentStaticUpdateError),mode:window.jtml.browserBuild};
  },{fields:fixture.fields,views:fixture.views||{},retain:fixture.retain});
  const snapshots=[await snapshot()];
  for(const step of fixture.steps) {
   await page.locator('.probe').nth(step.ordinal||0).getByRole('button',{name:step.action,exact:true}).click();
   // Flush scheduler work at a fixed rendering boundary, never wait for the
   // expected value (which could hide transient or stale-state defects).
   await page.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve))));
   snapshots.push(await snapshot());
  }
  assert.deepEqual(await page.evaluate(()=>window.conformanceCspViolations),[],'CSP violations');
  assert.deepEqual(errors,[],'unexpected browser exceptions');
  assert.deepEqual(requests,[],'unexpected network requests');
  for(const s of snapshots) {assert.equal(s.fallback,false,'clean-production fallback');assert.equal(s.retained,true,'nested DOM identity');assert.equal(s.mode.cleanProduction,true);assert.equal(s.mode.compatibility,false);}
  // Scope values are supplemental diagnostics; DOM text is the public oracle.
  return {state:snapshots.map(s=>s.values.map(v=>v.state)),visible:snapshots.map(s=>s.values.map(v=>v.visible))};
 } finally {await page.close();}
}
async function nativeVisible(browser,fixture,native) {
 const page=await browser.newPage();
 try {return await page.evaluate(({snapshots,fields,views})=>snapshots.map(s=>s.components.filter(c=>c.component==='Probe').map(c=>{
  const document=new DOMParser().parseFromString(c.renderedHtml,'text/html');
  return Object.fromEntries([...fields.map(f=>[f,document.querySelector('.out-'+f)?.textContent ?? null]), ...Object.entries(views).map(([key,selector])=>[key,[...document.querySelectorAll(selector)].map(el=>el.textContent)])]);
 })),{snapshots:native.snapshots,fields:fixture.fields,views:fixture.views||{}});} finally {await page.close();}
}
function prepare(fixture) {
 const dir=path.join(output,fixture.id);fs.mkdirSync(dir,{recursive:true});
 const source=path.join(dir,'input.jtml'),request=path.join(dir,'request.json'),result=path.join(dir,'native.json'),dist=path.join(dir,'browser');
 fs.writeFileSync(source,fixture.source);fs.writeFileSync(request,JSON.stringify({steps:fixture.steps}));
 command(binary,['build',source,'--target','browser','--out',dist],path.join(dir,'compile.log'));
 for(const name of ['index.html','jtml-runtime.js','components/index.js','app.js']) {
  const asset=fs.readFileSync(path.join(dist,name),'utf8');
  assert(!/\bnew\s+Function\b|\beval\s*\(/.test(asset),`unsafe runtime execution in ${name}`);
  assert(!asset.includes('jtml-update-plans.js'),`compatibility asset in ${name}`);
  if(name==='components/index.js') for(const forbidden of ['\"bodyPlan\"','\"bodyHex\"','__jtml_static_update_plans']) assert(!asset.includes(forbidden),`production metadata leak: ${forbidden}`);
 }
 assert(!fs.existsSync(path.join(dist,'jtml-update-plans.js')),'unexpected compatibility file');
 const html=path.join(dist,'index.html');
 assert(fs.readFileSync(html,'utf8').includes('<head>'),'CSP insertion point absent');
 fs.writeFileSync(html,fs.readFileSync(html,'utf8').replace('<head>',`<head><meta http-equiv="Content-Security-Policy" content="default-src 'none'; script-src 'self'; style-src 'unsafe-inline'; img-src data:; connect-src 'none'">`));
 assert(fs.statSync(path.join(dist,'jtml-runtime.js')).size<=272000,'runtime budget');
 return {dir,source,request,result,dist};
}
async function runCase(browser,fixture) {
 const {dir,source,request,result,dist}=prepare(fixture);
 fixture={...fixture,sourceFile:fs.realpathSync(source)};
 command(probe,[source,request,result],path.join(dir,'native.log'));
 const native=JSON.parse(fs.readFileSync(result));
 assert(native.dispatch.every(d=>d.ok),JSON.stringify(native.dispatch));
 const observed={native:{state:nativeStates(fixture,native),visible:await nativeVisible(browser,fixture,native)},browser:await browserTrace(browser,fixture,dist)};
 const failures=compare(fixture,observed,checkPlans(fixture,native));
 const diagnostics=fs.readFileSync(path.join(dir,'native.log'),'utf8').split('\n').filter(line=>line.startsWith('Interpreter Error:')).map(line=>line.replace(/\b\d+\.(?=[A-Za-z_])/g,'<instance>.').replace(/InstanceID \d+/g,'InstanceID <instance>'));
 if(diagnostics.length) failures.push({layer:'native-diagnostics',details:diagnostics});
 fs.writeFileSync(path.join(dir,'lowered.jtml'),native.lowered);
 const report={id:fixture.id,source,authoredSource:fixture.authoredSource,descriptor:fixture.descriptor,sha256:crypto.createHash('sha256').update(fixture.source).digest('hex'),steps:fixture.steps,expected:fixture.expected,observed,failures};
 fs.writeFileSync(path.join(dir,'trace.json'),JSON.stringify(report,null,2));
 return report;
}
async function main() {
 fs.mkdirSync(output,{recursive:true});
 const browser=await chromium.launch({headless:true,executablePath:process.env.JTML_BENCH_BROWSER||undefined});
 const reports=[];
 try {
  for(const fixture of [...cases,...generated()]) {
   const report=await runCase(browser,fixture);reports.push(report);
   fs.writeFileSync(path.join(output,'report.json'),JSON.stringify({cases:reports.length,reports},null,2));
   console.log(`${report.failures.length?'FAIL / KNOWN DEFECT CANDIDATE':'PASS'} ${fixture.id} ${report.failures.map(f=>f.layer+':'+(f.observation||'metadata')).join(',')}`);
  }
  // End-to-end negative control: both real backends agree on the deliberately
  // changed source, but neither may pass the independently retained oracle.
  const mutation={...cases[0],id:'negative-shared-wrong-answer',source:cases[0].source.replace('x += 1','x += 2')};
  const negative=await runCase(browser,mutation);
  assert(negative.failures.some(f=>f.layer==='native'&&f.observation==='state'));
  assert(negative.failures.some(f=>f.layer==='browser'&&f.observation==='state'));
  assert.deepEqual(negative.observed.native.state,negative.observed.browser.state);
  console.log('PASS negative control: agreeing backends rejected by independent oracle');
  const lifecycleFixture={id:'keyed-lifecycle',source:fs.readFileSync(path.join(__dirname,'fixtures/lifecycle.jtml'),'utf8'),steps:[]};
  const lifecycleResult=await require('./lifecycle.cjs')(browser,prepare(lifecycleFixture).dist);
  fs.writeFileSync(path.join(output,'lifecycle.json'),JSON.stringify(lifecycleResult,null,2));
  console.log('PASS keyed-lifecycle (browser-only; native identity UNKNOWN)');
  if(process.argv.includes('--minimize')) {
   const {minimize,algebraCandidates}=require('./minimize.cjs');
   const grouped=await runCase(browser,algebra('resolved-grouped-control',{x:0,a:0,b:1,sign:1,deltas:[],form:'group'}));
   assert.deepEqual(grouped.failures,[],'grouped and ungrouped controls must now agree');
   const start=generated().find(f=>f.id==='generated-24-0');
   let trial=0;
   const reduction=await minimize(start.descriptor,algebraCandidates,async descriptor=>{
    const report=await runCase(browser,algebra(`reduction-${trial++}`,descriptor));
    return report.failures.some(f=>f.layer==='native'&&f.observation==='state'&&typeof f.actual[0][0].x==='string')
      && !report.failures.some(f=>!['native','native-diagnostics'].includes(f.layer));
   });
   assert.equal(reduction.value.form,'identity','reduction must isolate remaining unary number-type defect');
   assert.deepEqual(reduction.value.deltas,[],'startup discrepancy requires no events');
   fs.writeFileSync(path.join(output,'minimization.json'),JSON.stringify(reduction,null,2));
   fs.writeFileSync(path.join(output,'minimal.jtml'),algebra('minimal',reduction.value).source);
   console.log('MINIMIZED',JSON.stringify(reduction));
  }
 } finally {await browser.close();}
 const rejected=require('./rejections.cjs')(binary,root,output);
 fs.writeFileSync(path.join(output,'rejections.json'),JSON.stringify(rejected,null,2));
 const discrepancies=reports.filter(r=>r.failures.length);
 const summary={semanticCases:reports.length,browserOnlyCases:1,rejectionCases:rejected.length,
  equivalentCases:reports.length-discrepancies.length,discrepancies:discrepancies.length,reports};
 fs.writeFileSync(path.join(output,'report.json'),JSON.stringify(summary,null,2));
 if(process.argv.includes('--verify-catalog')) {
  const catalog=JSON.parse(fs.readFileSync(path.join(__dirname,'known-discrepancies.json'))).entries;
  require('./catalog.cjs').verify(reports,catalog);
  console.log(`CATALOG VERIFIED: ${summary.equivalentCases} equivalent cases; ${summary.discrepancies} reproduced discrepancies (NOT semantic passes); 1 browser-only lifecycle; ${rejected.length} fail-closed rejections`);
 } else if(discrepancies.length) process.exitCode=2;
}
if(require.main===module) main().catch(error=>{fs.mkdirSync(output,{recursive:true});fs.writeFileSync(path.join(output,'failure.json'),JSON.stringify({error:String(error),stack:error.stack},null,2));console.error(error);process.exitCode=1;});
module.exports={compare,roots,checkPlans,runCase};
