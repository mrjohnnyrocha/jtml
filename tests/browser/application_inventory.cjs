// Honest acceptance probe for existing showcases. Failures are reported, not waived.
const assert = require('node:assert/strict');
const fs = require('node:fs'), path = require('node:path'), http = require('node:http');
const {spawnSync} = require('node:child_process'), {createRequire} = require('node:module');
const repo = path.resolve(__dirname, '../..');
assert(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT && process.env.JTML_BENCH_BROWSER, 'Chromium and Playwright required; no skip');
const {chromium} = createRequire(path.resolve(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT, 'package.json'))('playwright');
const out = path.join(repo, 'dist/application-browser-inventory');
fs.mkdirSync(out, {recursive:true});
let root, browser, server;
const report = {checks:[], failures:[], applications:[]};
const check = (name, ok, detail) => {report.checks.push({name, ok, detail}); if(!ok)report.failures.push(name);};
(async()=>{
 server=http.createServer((req,res)=>{
  const url=new URL(req.url,'http://localhost');
  const file=path.resolve(root,'.'+(url.pathname==='/'?'/index.html':url.pathname));
  if(!file.startsWith(root+path.sep)||!fs.existsSync(file)||!fs.statSync(file).isFile()){res.writeHead(404).end();return;}
  res.setHeader('Content-Type',file.endsWith('.js')?'text/javascript':file.endsWith('.svg')?'image/svg+xml':'text/html');
  res.end(fs.readFileSync(file));
 });
 await new Promise(r=>server.listen(0,'127.0.0.1',r));
 const origin=`http://127.0.0.1:${server.address().port}`;
 browser=await chromium.launch({headless:true,executablePath:process.env.JTML_BENCH_BROWSER});
 report.browser=browser.version();
 for(const [name, source, routes] of [
  ['opspulse','demos/opspulse',[['/','Overview'],['/usage','Usage Explorer'],['/people','People'],['/incidents','Incident Centre'],['/settings','Workspace Settings'],['/missing','Page not found']]],
  ['reference','demos/example.jtml',[['/','Summary'],['/usage','Usage'],['/people','People'],['/incidents','Incident'],['/releases','Release'],['/media','Media'],['/settings','Workspace'],['/missing','Page not found']]],
 ]){
  root=path.join(out,name);
  const built=spawnSync(process.env.JTML_BIN||path.join(repo,'build/jtml'),['build',path.join(repo,source),'--target','browser','--out',root],{encoding:'utf8',timeout:60000});
  assert.equal(built.status,0,built.stdout+built.stderr);
  const page=await browser.newPage(), errors=[], requests=[];
  page.setDefaultTimeout(2000);
  page.on('pageerror',e=>errors.push(e.message));
  page.on('requestfailed',r=>requests.push({url:r.url(),error:r.failure()}));
  page.on('response',r=>{if(r.status()>=400)requests.push({url:r.url(),status:r.status()});});
  const app={name,source,routes:[],errors,requests};report.applications.push(app);
  for(const [route,expected] of routes){
   await page.goto(origin+'/#'+route);
   await page.waitForFunction(route=>window.jtml?.getCurrentRoute?.()?.path===route,route);
   const text=await page.locator('body').innerText();
   const headings=await page.locator('h2:visible').allTextContents();
   app.routes.push({route,headings,text, runtime:await page.evaluate(()=>({current:window.jtml.getCurrentRoute(),routes:window.jtml.getRoutes()}))});
   check(`${name} route ${route}`,headings.some(h=>h.includes(expected)),headings);
  }
  await page.goto(origin+'/#/');
  for(const label of ['Usage','People','Incidents','Settings','Dashboard']){
   try{await page.getByRole('link',{name:label,exact:true}).filter({visible:true}).click();await page.waitForTimeout(100);
    check(`${name} navigation ${label}`,!((await page.locator('body').innerText()).includes('Page not found')),await page.locator('h2:visible').allTextContents());
   }catch(e){check(`${name} navigation ${label}`,false,e.message);}
  }
  for(const label of ['Theme','Portugal']){
   try{const before=await page.locator('body').innerText();
    const appClass=await page.locator('.app:visible').first().getAttribute('class').catch(()=>null);
    await page.getByRole('button',{name:label,exact:true}).filter({visible:true}).click();await page.waitForTimeout(100);
    const after=await page.locator('body').innerText();
    check(`${name} state ${label}`,label==='Theme'?appClass!==await page.locator('.app:visible').first().getAttribute('class').catch(()=>null):before!==after,{before,after});
   }catch(e){check(`${name} state ${label}`,false,e.message);}
  }
  await page.screenshot({path:path.join(out,name+'.png'),fullPage:true});
  check(`${name} no page errors`,errors.length===0,errors);
  check(`${name} no failed resources`,requests.length===0,requests);
  await page.close();
 }
})().catch(e=>{report.failures.push(String(e));}).finally(async()=>{
 if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));
 fs.writeFileSync(path.join(out,'results.json'),JSON.stringify(report,null,2)+'\n');
 console.log(JSON.stringify({checks:report.checks.length,failures:report.failures},null,2));
 process.exitCode=report.failures.length?1:0;
});
