const assert=require('node:assert/strict');
const fs=require('node:fs'),path=require('node:path'),{spawnSync}=require('node:child_process'),{pathToFileURL}=require('node:url');
const {chromium}=require(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT?path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT,'node_modules/playwright'):'playwright');
(async()=>{
 const output=path.join(process.env.JTML_CONFORMANCE_OUT||path.resolve('dist/conformance'),'global-argument-updates');
 fs.mkdirSync(output,{recursive:true});
 const binary=process.env.JTML_BIN||path.resolve('build/jtml');
 const source=path.resolve(__dirname,'../conformance/fixtures/nested/global-argument-updates.jtml'),dist=path.join(output,'browser');
 const build=spawnSync(binary,['build',source,'--target','browser','--out',dist],{encoding:'utf8',timeout:20000});
 fs.writeFileSync(path.join(output,'build.log'),(build.stdout||'')+(build.stderr||''));
 assert(!build.error&&!build.signal&&build.status===0,'global caller argument build');
 assert(fs.statSync(path.join(dist,'jtml-runtime.js')).size<=272000,'runtime budget');
 const html=path.join(dist,'index.html');
 assert(!fs.readFileSync(html,'utf8').includes('data-jtml-component-args-hex'),'compiler expression transport must not leak into browser HTML');
 fs.writeFileSync(html,fs.readFileSync(html,'utf8').replace('<head>',`<head><meta http-equiv="Content-Security-Policy" content="default-src 'none'; script-src 'self'; style-src 'unsafe-inline'; connect-src 'none'">`));
 const browser=await chromium.launch({headless:true,executablePath:process.env.JTML_BENCH_BROWSER||undefined});
 const traces=[];
 try {
  const page=await browser.newPage();page.setDefaultTimeout(5000);const errors=[],requests=[];
  page.on('pageerror',e=>errors.push(e.message));
  page.on('request',r=>{if(!r.url().startsWith(pathToFileURL(dist+path.sep).href))requests.push(r.url());});
  await page.addInitScript(()=>{window.violations=[];document.addEventListener('securitypolicyviolation',e=>window.violations.push(e.violatedDirective));});
  await page.goto(pathToFileURL(html).href);
  await page.waitForFunction(()=>window.jtml?.browserBuild);
  await page.evaluate(()=>{window.retained=[...document.querySelectorAll('.probe, .out-result')];});
  const check=async(count,local)=>{
   const actual=await page.evaluate(()=>({values:[...document.querySelectorAll('.probe')].map(el=>['count','result','initial','label'].map(name=>el.querySelector('.out-'+name).textContent)),retained:window.retained.every((el,i)=>el.isConnected&&el===[...document.querySelectorAll('.probe, .out-result')][i]),fallback:!!(window.jtml.directComponentCreateFallback||window.jtml.directComponentPatchFallback||window.jtml.directComponentBodyPlanFallback||window.jtml.directComponentStaticUpdateError),csp:window.violations,clean:window.jtml.browserBuild.cleanProduction}));
   // Independent arithmetic and public DOM observations; scope inspection is unnecessary.
   const item=(count-100)%2?5:3;
   const expected=[[String(local),String(count*2),'100','Revenue & growth'],['10',String((count+1)*4),'202','literal count'],['10',String((count+1+item)*2),'104','indexed']];
   traces.push({count,expected,actual});fs.writeFileSync(path.join(output,'trace.json'),JSON.stringify(traces,null,2));
   assert.deepEqual(actual,{values:expected,retained:true,fallback:false,csp:[],clean:true});
  };
  const settle=()=>page.evaluate(()=>new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(r))));
  const click=async(name)=>{await page.getByRole('button',{name,exact:true}).first().click();await settle();};
  // Public action adapter supplies global writes; component actions use DOM events.
  // Legacy top-level inline onclick is outside this CSP-safe component contract.
  const globalAction=async(name)=>{assert.equal(await page.evaluate(name=>window.jtml.runAction(name,...(name==='shadow'?[999]:[])),name),true);await settle();};
  await check(100,10);await click('bump');await check(100,11);
  await globalAction('shadow');await check(100,11);
  for(const count of [101,102,103]){await globalAction('step');await check(count,11);await globalAction('shadow');await check(count,11);}
  assert.deepEqual(errors,[]);assert.deepEqual(requests,[]);await page.close();
 } finally {await browser.close();}
 console.log('PASS global arguments: typed/grouped/indexed/derived caller values; literal strings; repeated global updates; init-once state; parameter shadowing; sibling and DOM identity; CSP; clean browser exit');
})().catch(e=>{console.error(e);process.exitCode=1;});
