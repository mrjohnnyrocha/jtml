const assert=require('node:assert/strict'), fs=require('fs'),path=require('path'),http=require('http');
const {spawnSync}=require('child_process'),{createRequire}=require('module');
const repo=path.resolve(__dirname,'../..'), out=path.join(repo,'dist/component-redirect');
const {chromium}=createRequire(path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT,'package.json'))('playwright');
const sources=['examples/friendly_redirect.jtml','studio/samples/redirect.jtml','tests/fixtures/production/component-redirect/index.jtml'];
const rows=[];let browser;
const server=http.createServer((req,res)=>{const file=path.resolve(out,'.'+new URL(req.url,'http://localhost').pathname);res.setHeader('Content-Security-Policy',"script-src 'self'");if(!file.startsWith(out+path.sep)||!fs.existsSync(file)){res.writeHead(404).end();return;}res.setHeader('Content-Type',file.endsWith('.js')?'text/javascript':'text/html');res.end(fs.readFileSync(file));});
(async()=>{fs.mkdirSync(out,{recursive:true});await new Promise(r=>server.listen(0,'127.0.0.1',r));browser=await chromium.launch({executablePath:process.env.JTML_BENCH_BROWSER,headless:true});
for(let i=0;i<sources.length;i++){
 const command=[process.env.JTML_BIN||path.join(repo,'build/jtml'),'build',sources[i],'--target','browser','--out',path.join(out,''+i)];
 const built=spawnSync(command[0],command.slice(1),{cwd:repo,encoding:'utf8'});const row={source:sources[i],command,exit:built.status,stderr:built.stderr,checks:[]};rows.push(row);if(built.status!==0)continue;
 const page=await browser.newPage();page.setDefaultTimeout(2000);const errors=[];page.on('pageerror',e=>errors.push(e.message));
 const url=`http://127.0.0.1:${server.address().port}/${i}/index.html`;
 const check=async(name,fn)=>{try{await fn();row.checks.push({name,pass:true});}catch(e){row.checks.push({name,pass:false,error:e.message});}};
 const click=async(label,destination)=>{await page.goto(url);await page.waitForFunction(()=>window.jtml?.getCurrentRoute());
 const client=await page.context().newCDPSession(page);await client.send('Profiler.enable');await client.send('Profiler.startPreciseCoverage',{callCount:true,detailed:true});
 row.metadata=await page.getByRole('button',{name:label,exact:true}).evaluate(el=>el.outerHTML);
 await page.getByRole('button',{name:label,exact:true}).click();
 const coverage=await client.send('Profiler.takePreciseCoverage');const trace=coverage.result.flatMap(s=>s.functions.filter(f=>/runDirectComponent|executeClientAction|sendEvent|redirect|checkGuards|applyRoutes/.test(f.functionName)).map(f=>({name:f.functionName,count:f.ranges[0].count})));
 row.traces=row.traces||[];row.traces.push({label,trace});await client.detach();await page.waitForFunction(p=>jtml.getCurrentRoute()?.path===p,destination);
 if(i===2 && label.endsWith('local') && label!=='Protected local') {
   assert.equal(await page.getByRole('heading',{name:'Destination',exact:true}).filter({visible:true}).count(),1);
   await page.goBack();await page.waitForFunction(()=>jtml.getCurrentRoute()?.path==='/');
   await page.getByText(label.startsWith('First')?'First: 2':'Second: 2',{exact:true}).waitFor();
   await page.getByText(label.startsWith('First')?'Second: 0':'First: 0',{exact:true}).waitFor();
   assert.deepEqual(await page.evaluate(()=>jtml.directComponentFallbacks||[]),[]);
 }
 };
 if(i<2)await check('original component button navigates',()=>click(i===0?'Go to Dashboard':'Open dashboard','/dashboard'));
 else {
 for(const label of ['Root global','First global','First local','Second local'])await check(label,()=>click(label,'/destination'));
 await check('local redirect preserves guards',()=>click('Protected local','/login'));
 await check('fallback forwards arguments',async()=>{await page.goto(url);await page.getByRole('button',{name:'First argument',exact:true}).click();await page.getByText('Received: argument',{exact:true}).waitFor();});
 }
 await check('strict CSP and clean assets',async()=>{assert.deepEqual(errors,[]);assert.equal(await page.evaluate(()=>jtml.browserBuild.mode),'prod');assert(!fs.existsSync(path.join(out,''+i,'jtml-update-plans.js')));});
 row.errors=errors;await page.close();
}
})().catch(e=>{console.error(e);process.exitCode=1;}).finally(async()=>{if(browser)await browser.close();await new Promise(r=>server.close(r));fs.writeFileSync(path.join(out,'results.json'),JSON.stringify(rows,null,2)+'\n');console.log(JSON.stringify(rows,null,2));if(rows.some(r=>r.exit!==0||r.checks.some(c=>!c.pass)||r.errors.length))process.exitCode=1;});
