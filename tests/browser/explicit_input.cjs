// Explicit caller authority, exercised through real browser input events only.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),http=require('node:http');
const {spawnSync}=require('node:child_process'),{createRequire}=require('node:module');
assert(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT&&process.env.JTML_BENCH_BROWSER,'real Chromium required; no skip');
const {chromium}=createRequire(path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT,'package.json'))('playwright');
const repo=path.resolve(__dirname,'../..'),out=path.join(repo,'dist/explicit-input'),checks=[],errors=[];let browser;
const built=spawnSync(process.env.JTML_BIN||path.join(repo,'build/jtml'),['build','tests/fixtures/production/explicit-input/index.jtml','--target','browser','--out',out],{cwd:repo,encoding:'utf8'});
assert.equal(built.status,0,built.stdout+built.stderr);
const boundary=spawnSync(process.env.JTML_BIN||path.join(repo,'build/jtml'),['build','tests/fixtures/production/explicit-input/unauthorized.jtml','--target','browser','--out',path.join(out,'unauthorized')],{cwd:repo,encoding:'utf8'});
assert.equal(boundary.status,0,boundary.stdout+boundary.stderr);
const server=http.createServer((req,res)=>{const f=path.join(out,new URL(req.url,'http://localhost').pathname);if(!f.startsWith(out+path.sep)||!fs.existsSync(f)||!fs.statSync(f).isFile()){res.writeHead(404).end();return;}res.setHeader('Content-Security-Policy',"script-src 'self'");res.setHeader('Content-Type',f.endsWith('.js')?'text/javascript':'text/html');res.end(fs.readFileSync(f));});
(async()=>{await new Promise(r=>server.listen(0,'127.0.0.1',r));browser=await chromium.launch({executablePath:process.env.JTML_BENCH_BROWSER,headless:true});const page=await browser.newPage();page.setDefaultTimeout(3000);page.on('pageerror',e=>errors.push(e.message));
 const check=async(name,fn)=>{try{await fn();checks.push({name,pass:true});}catch(e){checks.push({name,pass:false,error:e.stack});}};
 await page.goto(`http://127.0.0.1:${server.address().port}/index.html`);await page.waitForFunction(()=>window.jtml?.getCurrentRoute());
 const input=name=>page.getByRole('textbox',{name,exact:true});const state=()=>page.evaluate(()=>({...jtml.state}));
 await check('compiler-issued action belongs to entry module and correct imported-component instance',async()=>{
  const mapping=await page.evaluate(()=>jtml.getComponentInstances().filter(i=>i.component==='BoundEditor').map(i=>({id:i.id,moduleId:i.moduleId,definitionModule:i.definitionModule,inputActions:i.inputActions,argumentPlans:i.argumentPlans})));
  assert.equal(mapping.length,3);for(const i of mapping){assert.equal(i.moduleId,0);assert.equal(i.definitionModule,1);assert.equal(i.inputActions.value,`__jtml_input_0_${i.id}_value`);assert(['email','other','__jtml_input_value'].includes(i.argumentPlans.value.root));}
 });
 await check('input commits owner state, effects and mirrors while retaining focus',async()=>{
  await input('Email').fill('probe@example.test');assert.equal((await state()).email,'probe@example.test');assert.equal((await state()).observed,'probe@example.test');assert.equal((await state()).hits,1);
  await page.getByText('Bound mirror: probe@example.test',{exact:true}).waitFor();await page.getByText('Email: probe@example.test',{exact:true}).waitFor();assert(await input('Email').evaluate(el=>document.activeElement===el));
 });
 await check('repeated changes, property synchronization and cursor preservation',async()=>{
  await input('Email').fill('second@example.test');await input('Email').fill('third@example.test');assert.equal((await state()).hits,3);
  await input('Email').evaluate(el=>{el.setSelectionRange(2,5);window.retainedInput=el;});
  await input('Independent').evaluate(el=>{el.value='unrelated';el.dispatchEvent(new Event('input',{bubbles:true}));});
  assert.deepEqual(await input('Email').evaluate(el=>({same:el===retainedInput,focus:document.activeElement===el,start:el.selectionStart,end:el.selectionEnd})),{same:true,focus:true,start:2,end:5});
  await page.getByRole('button',{name:'Reset',exact:true}).click();assert.equal(await input('Email').inputValue(),'');await page.getByRole('button',{name:'Restore',exact:true}).click();assert.equal(await input('Email').inputValue(),'restored@example.test');
 });
 await check('local shadowing, sibling isolation and root input behavior',async()=>{
  await input('First local').fill('first-only');assert.equal(await input('Second local').inputValue(),'local');assert.equal((await state()).email,'restored@example.test');assert.equal(await input('Independent').inputValue(),'unrelated');
  await input('Root').fill('root-write');assert.equal((await state()).other,'root-write');assert.equal(await input('Independent').inputValue(),'root-write');assert.equal(await input('Email').inputValue(),'restored@example.test');
 });
 await check('generated setter parameters cannot capture the authorized owner',async()=>{await input('Hygiene').fill('hygiene-write');assert.equal((await state()).__jtml_input_value,'hygiene-write');assert.equal((await state()).email,'restored@example.test');assert.equal((await state()).__jtml_input_value_,'trailing');for(let i=1;i<=8;i++)assert.equal((await state())['__jtml_input_value_'+i],'sentinel-'+i);assert(!Object.hasOwn(await state(),'__jtml_input_value_9'));assert(!Object.hasOwn(await state(),'__jtml_input_value_10'));});
 await check('remount retains owner state and authorizes only the new instance',async()=>{
  await page.getByRole('button',{name:'Toggle',exact:true}).click();assert.equal(await input('Email').count(),0);await page.getByRole('button',{name:'Toggle',exact:true}).click();assert.equal(await input('Email').inputValue(),'restored@example.test');await input('Email').fill('remounted@example.test');assert.equal((await state()).email,'remounted@example.test');assert.equal((await state()).observed,'remounted@example.test');
 });
 await check('explicit fixture sign-in, sign-out and guard use edited owner state',async()=>{
  await page.getByRole('button',{name:'Sign in',exact:true}).click();await page.waitForFunction(()=>jtml.getCurrentRoute().path==='/private');assert.equal((await state()).token,'remounted@example.test');assert.equal((await state()).audit,'Signed in as remounted@example.test.');
  await page.getByRole('button',{name:'Sign out',exact:true}).click();await page.waitForFunction(()=>jtml.getCurrentRoute().path==='/login');assert.equal((await state()).token,'');await page.getByRole('link',{name:'Private',exact:true}).click();await page.waitForTimeout(150);assert.equal(await page.evaluate(()=>location.hash),'#/login');
 });
 await check('unopted module writes and ordinary/derived parameters remain unauthorized',async()=>{
  await page.goto(`http://127.0.0.1:${server.address().port}/unauthorized/index.html`);await page.waitForFunction(()=>window.jtml?.getComponentInstances());
  for(const name of ['Unauthorized','Parameter','Derived'])await input(name).fill('attempted');
  assert.equal((await state()).email,'module');assert.equal((await state()).parameter,'parameter');
  const codes=await page.evaluate(()=>jtml.directComponentBindingDiagnostics.map(d=>d.code));
  assert(codes.includes('JTML_UNKNOWN_INTO_TARGET'));assert(codes.includes('JTML_INTO_PARAM_TARGET'));assert(codes.includes('JTML_INTO_DERIVED_TARGET'));
 });
 await check('strict CSP, clean assets and no page exceptions',async()=>{assert.deepEqual(errors,[]);assert.equal(await page.evaluate(()=>jtml.browserBuild.mode),'prod');assert(!fs.existsSync(path.join(out,'jtml-update-plans.js')));assert(fs.statSync(path.join(out,'jtml-runtime.js')).size<=273000);
  const html=fs.readFileSync(path.join(out,'index.html'),'utf8'),components=fs.readFileSync(path.join(out,'components/index.js'),'utf8');const manifest=JSON.parse(html.match(/<script type="application\/json" id="__jtml_client_manifest">(.*?)<\/script>/s)[1]);
  const forbidden=new Set(['bindingOrigin','bindingArguments','bindingParams','sourceSpan','sourcePath','originalLocation','originalRange','bodyHex','bodySource','parseMetadata']);
  // Baseline public body/slot plans use numeric coordinates in static-plan identity.
  // Preserve that existing contract; grant no exception to authorization metadata.
  const coordinates=new Set(['sourceLine','sourceColumn']);
  const inspect=(v,at='$')=>{if(v&&typeof v==='object')for(const [key,value] of Object.entries(v)){const field=at+'.'+key;assert(!forbidden.has(key),'leaked '+field);if(coordinates.has(key)){assert(/\.componentDefinitions\.\d+\.bodyPlan\.\d+\.(sourceLine|sourceColumn)$|\.componentInstances\.\d+\.slotPlan\.\d+\.(sourceLine|sourceColumn)$/.test(field),'coordinate outside baseline execution plan: '+field);assert(Number.isInteger(value)&&value>=0,'non-numeric execution coordinate: '+field);}inspect(value,field);}};inspect(manifest);
  assert(!html.includes('data-jtml-binding-args'));assert(!html.includes('data-jtml-binding-params'));assert(!JSON.stringify(manifest).includes(repo));for(const key of [...forbidden,...coordinates])assert(!components.includes(JSON.stringify(key)),'component leak '+key);});
})().catch(e=>{errors.push(e.stack);process.exitCode=1;}).finally(async()=>{if(browser)await browser.close();await new Promise(r=>server.close(r));if(checks.some(c=>!c.pass))process.exitCode=1;const result={checks,errors,browser:browser&&browser.version(),runtimeBytes:fs.statSync(path.join(out,'jtml-runtime.js')).size,exit:process.exitCode||0};fs.writeFileSync(path.join(out,'results.json'),JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(result,null,2));});
