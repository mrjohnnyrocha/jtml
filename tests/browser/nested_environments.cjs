const assert=require('node:assert/strict');
const fs=require('node:fs'),path=require('node:path');
const {chromium}=require(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT?path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT,'node_modules/playwright'):'playwright');
const {runCase}=require('../conformance/runner.cjs');
(async()=>{
 const output=process.env.JTML_CONFORMANCE_OUT||path.resolve('dist/conformance');
 fs.mkdirSync(output,{recursive:true});
 const browser=await chromium.launch({headless:true,executablePath:process.env.JTML_BENCH_BROWSER||undefined});
 const reports=[],mandatory=[];
 try {
  for(const fixture of require('../conformance/nested_cases.cjs')) {
   const report=await runCase(browser,fixture);
   if (fixture.expectedExpanded) {
    const native=JSON.parse(fs.readFileSync(path.join(output,fixture.id,'native.json')));
    const actual=native.snapshots.map(snapshot=>Object.fromEntries(['Child','Leaf'].map(name=>[name,
     snapshot.components.filter(c=>c.component===name).map(c=>c.locals[name==='Child'?'result':'triple'].value).sort((a,b)=>a-b)])));
    // Expanded state is independent of the parent's dynamically rendered view.
    report.expanded={expected:fixture.expectedExpanded,actual};
    if(JSON.stringify(actual)!==JSON.stringify(fixture.expectedExpanded)) report.failures.push({layer:'native-expanded',observation:'state',expected:fixture.expectedExpanded,actual});
    for(const snapshot of native.snapshots) for(const c of snapshot.components) {
     if(c.component==='Child') assert.equal(c.locals.count.value,10,'child owns count');
     if(c.component==='Leaf') assert.equal(c.locals.result.value,1,'leaf owns result');
    }
   }
   if(fixture.knownDiscrepancy) report.expected={state:fixture.expected,views:fixture.expectedViews};
   else mandatory.push(report);
   reports.push(report);
   fs.writeFileSync(path.join(output,'nested-cases.json'),JSON.stringify(reports,null,2));
   console.log(report.failures.length?'FAIL / KNOWN DEFECT CANDIDATE':'PASS',fixture.id);
  }
 } finally {await browser.close();}
 assert.deepEqual(mandatory.filter(r=>r.failures.length).map(r=>({id:r.id,failures:r.failures})),[],'new nested ownership cases require independently specified native and DOM outcomes');
 console.log('PASS',mandatory.length,'nested ownership cases: original provenance, compiler reads, native state and DOM, clean production DOM/events and sibling isolation');
 // Keep the original source and its independently authored 100*2 oracle.
 // Check expanded state, native rendered HTML and real browser DOM separately.
 const source=path.resolve(__dirname,'../conformance/fixtures/nested/known-parameter-capture.jtml');
 const directory=path.join(output,'known-native-parameter-capture');fs.mkdirSync(directory,{recursive:true});
 const binary=process.env.JTML_BIN||path.resolve('build/jtml');
 const probe=process.env.JTML_CONFORMANCE_PROBE||path.join(path.dirname(binary),'tests/jtml_conformance_probe');
 const request=path.join(directory,'request.json'),result=path.join(directory,'native.json');
 fs.writeFileSync(request,JSON.stringify({steps:[]}));
 const command=require('node:child_process').spawnSync(probe,[source,request,result],{encoding:'utf8',timeout:20000});
 const log=(command.stdout||'')+(command.stderr||'');fs.writeFileSync(path.join(directory,'native.log'),log);
 assert(!command.error&&!command.signal&&command.status===0,'capture probe must not crash');
 assert(!log.includes('Interpreter Error:'),'new diagnostic cannot hide behind the lowering counterexample');
 const state=JSON.parse(fs.readFileSync(result)).snapshots[0];
 const parent=state.components.find(c=>c.component==='Parent'),child=state.components.find(c=>c.component==='Child');
 const expected={parentCount:100,childCount:10,result:200};
 const native=JSON.parse(fs.readFileSync(result));
 const fixture={fields:[],steps:[],source:fs.readFileSync(source,'utf8'),sourceFile:fs.realpathSync(source),checks:[{component:'Child',text:'get result = value * 2',reads:['value'],irReads:['value']}]};
 assert.deepEqual(require('../conformance/oracle.cjs').checkPlans(fixture,native),[],'original source provenance and dependency ownership');
 const dist=path.join(directory,'browser');
 const build=require('node:child_process').spawnSync(binary,['build',source,'--target','browser','--out',dist],{encoding:'utf8',timeout:20000});
 fs.writeFileSync(path.join(directory,'build.log'),(build.stdout||'')+(build.stderr||''));
 assert(!build.error&&!build.signal&&build.status===0,'original fixture clean-production build');
 const originalBrowser=await chromium.launch({headless:true,executablePath:process.env.JTML_BENCH_BROWSER||undefined});
 let views;
 try {
  const page=await originalBrowser.newPage();const errors=[],requests=[];
  page.on('pageerror',error=>errors.push(error.message));
  const {pathToFileURL}=require('node:url');
  page.on('request',request=>{if(!request.url().startsWith(pathToFileURL(dist+path.sep).href)) requests.push(request.url());});
  await page.addInitScript(()=>{window.captureCsp=[];document.addEventListener('securitypolicyviolation',e=>window.captureCsp.push(e.violatedDirective));});
  await page.goto(pathToFileURL(path.join(dist,'index.html')).href);
  await page.waitForFunction(()=>window.jtml?.browserBuild);
  views=await page.evaluate(html=>({browser:[...document.querySelectorAll('p')].map(p=>p.textContent),nativeRendered:[...new DOMParser().parseFromString(html,'text/html').querySelectorAll('p')].map(p=>p.textContent),clean:window.jtml.browserBuild.cleanProduction,csp:window.captureCsp}),parent.renderedHtml);
  assert.deepEqual(views,{browser:['200'],nativeRendered:['200'],clean:true,csp:[]},'independent original DOM and native rendered view');
  assert.deepEqual(errors,[]);assert.deepEqual(requests,[]);await page.close();
 } finally {await originalBrowser.close();}

 const actual={parentCount:parent?.locals.count?.value,childCount:child?.locals.count?.value,result:child?.locals.result?.value};
 const report={id:'native-parameter-capture',sha256:require('node:crypto').createHash('sha256').update(fs.readFileSync(source)).digest('hex'),expected,views,failures:JSON.stringify(actual)===JSON.stringify(expected)?[]:[{layer:'native-lowering',observation:'expanded-state',actual}]};
 fs.writeFileSync(path.join(directory,'trace.json'),JSON.stringify(report,null,2));
 require('../conformance/catalog.cjs').verify([...reports,report],require('../conformance/nested-known-discrepancies.json').entries);
 assert.deepEqual(report.failures,[],'resolved original capture must remain a mandatory pass');
 console.log('PASS original parameter capture: expanded state 200; native rendered view 200; clean-production DOM 200; original source and oracle unchanged');
 const known=reports.filter(r=>r.failures.length);
 console.log('CATALOG VERIFIED:',known.length,'FAIL / KNOWN DEFECT cases:',known.map(r=>r.id).join(', '),'; independent oracles retained; reproduced on frozen main');
 if(process.argv.includes('--strict')&&known.length) process.exitCode=2;

})().catch(error=>{console.error(error);process.exitCode=1;});
