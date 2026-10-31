// Native UI oracle for caller-owned route content supplied to a layout slot.
const assert=require('node:assert/strict'),fs=require('fs'),path=require('path'),http=require('http');
const {spawnSync}=require('child_process'),{createRequire}=require('module');
assert(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT && process.env.JTML_BENCH_BROWSER,'real Chromium required; no skip');
const {chromium}=createRequire(path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT,'package.json'))('playwright');
const repo=path.resolve(__dirname,'../..'),out=path.join(repo,'dist/route-layout'),checks=[];let browser;
const built=spawnSync(process.env.JTML_BIN||path.join(repo,'build/jtml'),['build',process.env.JTML_ROUTE_LAYOUT_SOURCE||'tests/fixtures/production/route-layout/nested/index.jtml','--target','browser','--out',out],{cwd:repo,encoding:'utf8'});
assert.equal(built.status,0,built.stdout+built.stderr);
const server=http.createServer((req,res)=>{const file=path.join(out,new URL(req.url,'http://localhost').pathname);if(!file.startsWith(out+path.sep)||!fs.existsSync(file)){res.writeHead(404).end();return;}res.setHeader('Content-Security-Policy',"script-src 'self'");res.setHeader('Content-Type',file.endsWith('.js')?'text/javascript':'text/html');res.end(fs.readFileSync(file));});
(async()=>{await new Promise(r=>server.listen(0,'127.0.0.1',r));browser=await chromium.launch({executablePath:process.env.JTML_BENCH_BROWSER,headless:true});const page=await browser.newPage();page.setDefaultTimeout(2000);const errors=[];page.on('pageerror',e=>errors.push(e.message));
const base=`http://127.0.0.1:${server.address().port}/index.html`;
for(const route of ['/plain','/command','/login','/detail/ada','/detail/bob']){
 await page.goto(base+'#'+route);await page.waitForFunction(()=>typeof window.jtml?.getCurrentRoute==='function');assert.equal(await page.evaluate(()=>jtml.getCurrentRoute().path),route);
 const heading=route.startsWith('/detail/')?'Detail content':route==='/login'?'Login content':'Command content';await page.getByRole('heading',{name:heading,exact:true}).waitFor();checks.push(route+' content visible');
 if(route.startsWith('/detail/')){await page.getByText('Detail: '+route.split('/').pop(),{exact:true}).waitFor();checks.push(route+' parameter visible');}
 if(route!=='/plain'){await page.getByRole('heading',{name:'Route shell',exact:true}).waitFor();await page.getByText('Shell footer',{exact:true}).filter({visible:true}).waitFor();checks.push(route+' layout visible');}
}
assert.deepEqual(errors,[]);checks.push('no page errors');
})().catch(e=>{checks.push({failure:e.message});console.error(e);process.exitCode=1;}).finally(async()=>{if(browser)await browser.close();await new Promise(r=>server.close(r));fs.writeFileSync(path.join(out,'results.json'),JSON.stringify({checks,exit:process.exitCode||0},null,2)+'\n');console.log(JSON.stringify(checks));});
