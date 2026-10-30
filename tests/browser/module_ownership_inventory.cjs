// Audit of unchanged inventory sources. Deliberately exits nonzero for unresolved behavior.
// This is not a required green suite; test endpoints are an explicit local HTTP fixture.
const fs=require('fs'),path=require('path'),http=require('http'),assert=require('assert/strict');
const {createRequire}=require('module'), {spawnSync}=require('child_process');
assert(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT && process.env.JTML_BENCH_BROWSER, 'real Chromium required; no skip');
const {chromium}=createRequire(path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT,'package.json'))('playwright');
const repo=path.resolve(__dirname,'../..'), rows=[], requests={};
const out=path.join(repo,'dist/module-ownership-audit');
fs.mkdirSync(out,{recursive:true});
const server=http.createServer((req,res)=>{
 const url=new URL(req.url,'http://localhost');
 if(['/api/users','/api/teams'].includes(url.pathname)) {requests[url.pathname]=(requests[url.pathname]||0)+1;res.setHeader('Content-Type','application/json');res.end(JSON.stringify([{id:1,name:'Ada',email:'ada@example.test'}]));return;}
 const file=path.join(out,url.pathname.endsWith('/')?url.pathname+'index.html':url.pathname);
 if(!file.startsWith(out+path.sep)||!fs.existsSync(file)){res.writeHead(404).end();return;}
 res.setHeader('Content-Security-Policy',"script-src 'self'");res.setHeader('Content-Type',file.endsWith('.js')?'text/javascript':'text/html');res.end(fs.readFileSync(file));
});
(async()=>{
 await new Promise(r=>server.listen(0,'127.0.0.1',r));
 const browser=await chromium.launch({executablePath:process.env.JTML_BENCH_BROWSER,headless:true});
 try{for(const source of ['demos/index',...['effect','fetch_invalidate','redirect','route_guards'].map(s=>'examples/friendly_'+s),...['effects','fetch','redirect','routes'].map(s=>'studio/samples/'+s)]){
 const command=[process.env.JTML_BIN || path.join(repo,'build/jtml'),'build',source+'.jtml','--target','browser','--out',path.join(out,source)];
 const built=spawnSync(command[0],command.slice(1),{cwd:repo,encoding:'utf8'});
 if(built.status!==0){rows.push({source,command,exit:built.status,stdout:built.stdout,stderr:built.stderr,checks:[{name:'clean build',pass:false}]});continue;}
 const page=await browser.newPage();page.setDefaultTimeout(3000);const errors=[], failedResources=[];page.on('pageerror',e=>errors.push(e.message));page.on('response',r=>{if(r.status()>=400)failedResources.push({url:r.url(),status:r.status()});});
 const row={source,command,exit:built.status,checks:[]};const check=async(name,fn)=>{try{await fn();row.checks.push({name,pass:true});}catch(e){row.checks.push({name,pass:false,error:e.message});}};
 await page.goto(`http://127.0.0.1:${server.address().port}/${source}/`);
 await check('clean startup',async()=>{await page.waitForFunction(()=>window.jtml?.browserBuild);assert.equal(await page.evaluate(()=>jtml.browserBuild.mode),'prod');assert.deepEqual(errors,[]);});
 if(source.endsWith('effect')||source.endsWith('effects')){
 await check('increment',async()=>{await page.getByRole('button',{name:'+ Increment',exact:true}).click();await page.waitForFunction(()=>document.body.innerText.includes('Count changed to 1'));});
 row.body=await page.locator('body').innerText();
 }else if(source.endsWith('redirect')){
 const label=source.startsWith('studio')?'Open dashboard':'Go to Dashboard';
 await check('action redirect',async()=>{await page.getByRole('button',{name:label,exact:true}).click();await page.waitForFunction(()=>jtml.getCurrentRoute()?.path==='/dashboard');});
 }else if(source.endsWith('route_guards')){
 await check('deny empty token',async()=>{await page.getByRole('link',{name:'Dashboard',exact:true}).filter({visible:true}).click();await page.waitForFunction(()=>jtml.getCurrentRoute()?.path==='/login');});
 }else if(source.endsWith('/routes')){
 await check('allow populated token',async()=>{await page.getByRole('link',{name:'Open dashboard',exact:true}).click();await page.waitForFunction(()=>jtml.getCurrentRoute()?.path==='/dashboard');});
 }else if(source.endsWith('fetch_invalidate')||source.endsWith('/fetch')){
 await check('initial fetch',async()=>{await page.getByText('Ada',{exact:true}).first().waitFor();});
 const before=requests['/api/users']||0;
 await check('invalidate repeats HTTP request',async()=>{await page.getByRole('button',{name:source.endsWith('/fetch')?'Save + invalidate group':'Save user',exact:true}).click();await new Promise(r=>setTimeout(r,500));assert((requests['/api/users']||0)>before);});
  }else {
 await check('prefilled token permits command',async()=>{await page.getByRole('link',{name:'Command',exact:true}).filter({visible:true}).click();await page.waitForFunction(()=>jtml.getCurrentRoute()?.path==='/command');});
 await check('command page content',async()=>{await page.getByRole('heading',{name:'Live operations workspace',exact:true}).waitFor();});
 await page.getByRole('link',{name:'Login',exact:true}).filter({visible:true}).click();
 await check('sign in navigates to command',async()=>{await page.getByRole('textbox',{name:'Email',exact:true}).filter({visible:true}).fill('ada@example.test');await page.getByRole('button',{name:'Sign in',exact:true}).filter({visible:true}).click();await page.waitForFunction(()=>jtml.getCurrentRoute()?.path==='/command');});
 row.body=(await page.locator('body').innerText()).slice(0,1200);
 }
 row.errors=errors;row.failedResources=failedResources;row.route=await page.evaluate(()=>({current:window.jtml?.getCurrentRoute?.(),routes:window.jtml?.getRoutes?.(),links:[...document.querySelectorAll('a')].filter(a=>a.getClientRects().length).map(a=>({text:a.textContent,href:a.getAttribute('href')}))}));rows.push(row);console.log(JSON.stringify(row));await page.close();
 }}finally{await browser.close();await new Promise(r=>server.close(r));fs.writeFileSync(path.join(out,'results.json'),JSON.stringify({browserVersion:browser.version(),rows},null,2)+'\n');
 if(rows.some(row=>row.checks.some(check=>!check.pass)))process.exitCode=1;}
})().catch(e=>{console.error(e);process.exitCode=1});
