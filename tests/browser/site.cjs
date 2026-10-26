// Actual static website and compiler output, no simulated Counter state.
const assert = require('node:assert/strict');
const fs = require('node:fs'), path = require('node:path'), http = require('node:http'), crypto = require('node:crypto');
const {createRequire} = require('node:module');
const root = path.resolve(process.argv[2] || 'dist/site');
assert(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT && process.env.JTML_BENCH_BROWSER, 'real browser required; no skip');
const {chromium} = createRequire(path.resolve(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT,'package.json'))('playwright');
const sha = p => crypto.createHash('sha256').update(fs.readFileSync(p)).digest('hex');
const info = JSON.parse(fs.readFileSync(path.join(root,'site-build.json')));
assert.equal(info.baseURL,'https://mrjohnnyrocha.github.io/jtml/');
assert(!fs.existsSync(path.join(root,'CNAME')));
assert.equal(sha(path.join(root,'counter.jtml')),info.counterSourceSHA256);
for(const name of ['jtml-runtime.js','components/index.js','app.js'])assert.equal(sha(path.join(root,'demo/counter',name)),info.generatedAssetsSHA256[name]);
assert.deepEqual(fs.readdirSync(path.join(root,'demo/counter')).sort(),['app.js','components','index.html','jtml-runtime.js']);
for(const line of fs.readFileSync(path.join(root,'manifest.txt'),'utf8').trim().split('\n')){
  const [hash,file] = line.split('  '); assert.equal(sha(path.join(root,file)),hash,file);
}
assert(fs.readFileSync(path.join(root,'sitemap.xml'),'utf8').includes('https://mrjohnnyrocha.github.io/jtml/demo/counter/'));
assert(!fs.readFileSync(path.join(root,'robots.txt'),'utf8').includes('jtml.org'));
const pages=fs.readdirSync(root).filter(n=>n.endsWith('.html'));
const errors=[], violations=[], requests=[], mime=[];
const server=http.createServer((req,res)=>{
  const url=new URL(req.url,'http://localhost');let relative=decodeURIComponent(url.pathname);
  if(relative.startsWith('/jtml/'))relative=relative.slice('/jtml'.length);
  if(relative.endsWith('/'))relative+='index.html';
  if(relative==='/__csp_negative__'){
    const meta=fs.readFileSync(path.join(root,'index.html'),'utf8').match(/<meta http-equiv="Content-Security-Policy"[^>]*>/)[0];
    res.setHeader('Content-Type','text/html');res.end(`${meta}<script>window.inlineRan=true</script><button onclick="this.textContent='FAILED'">Blocked</button>`);return;
  }
  const file=path.resolve(root,'.'+relative);
  if(!file.startsWith(root+path.sep)||!fs.existsSync(file)||!fs.statSync(file).isFile()){res.writeHead(404);res.end('Not found');return;}
  const type={'.html':'text/html','.js':'text/javascript','.css':'text/css','.json':'application/json','.jtml':'text/plain','.md':'text/plain','.png':'image/png','.txt':'text/plain','.xml':'application/xml'}[path.extname(file)]||'application/octet-stream';
  res.setHeader('Content-Type',type);res.end(fs.readFileSync(file));
});
let browser;
(async()=>{
 await new Promise(r=>server.listen(0,'127.0.0.1',r));
 const origin=`http://127.0.0.1:${server.address().port}`;
 browser=await chromium.launch({headless:true,executablePath:process.env.JTML_BENCH_BROWSER});
 for(const prefix of ['/','/jtml/'])for(const viewport of [{width:1440,height:1000},{width:390,height:844}]){
  const context=await browser.newContext({viewport});const page=await context.newPage();page.setDefaultTimeout(7000);
  page.on('pageerror',e=>errors.push(e.message));page.on('console',m=>{if(m.type()==='error')errors.push(m.text());});
  page.on('response',r=>{if(r.status()>=400)errors.push(`${r.status()} ${r.url()}`);if(r.url().endsWith('.js')){assert(r.headers()['content-type'].startsWith('text/javascript'));mime.push(r.url());}});
  page.on('request',r=>requests.push(r.url()));page.on('websocket',ws=>errors.push('unexpected WebSocket '+ws.url()));
  await page.exposeFunction('observeWebsiteCSP',e=>violations.push(e));
  await page.addInitScript(()=>document.addEventListener('securitypolicyviolation',e=>window.observeWebsiteCSP(e.effectiveDirective)));
  for(const file of pages){
   await page.goto(origin+prefix+file);
   assert.equal(await page.locator('meta[http-equiv="Content-Security-Policy"]').count(),1);
   assert((await page.locator('meta[http-equiv="Content-Security-Policy"]').getAttribute('content')).includes("script-src 'self'"));
   assert.equal(await page.locator('script:not([src]):not([type="application/json"])').count(),0);
   assert.equal(await page.evaluate(()=>[...document.querySelectorAll('*')].filter(e=>e.getAttributeNames().some(a=>/^on/i.test(a))).length),0);
   assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),'no page-wide horizontal overflow: '+file);
   // Resolve actual navigation/assets and authored fragment targets under the mount prefix.
   const links=await page.locator('a[href],link[rel="stylesheet"],script[src],iframe[src]').evaluateAll(es=>es.map(e=>e.href||e.src));
   for(const link of links){
    const u=new URL(link);if(u.origin!==origin)continue;
    assert(u.pathname.startsWith(prefix),'link escapes project path: '+u.pathname);
    const response=await context.request.get(u.href);assert.equal(response.status(),200,u.href);
    if(u.hash&&u.pathname===new URL(page.url()).pathname)assert(await page.locator(u.hash).count(),'missing anchor '+u.hash);
   }
  }
  await page.goto(origin+prefix);
  assert.equal(await page.locator('#snippet-code').textContent(),fs.readFileSync(path.join(root,'counter.jtml'),'utf8'));
  await page.getByRole('tab',{name:'HTML',exact:true}).click();await page.keyboard.press('ArrowRight');
  assert.equal(await page.getByRole('tab',{name:'React',exact:true}).getAttribute('aria-selected'),'true');
  await page.keyboard.press('Home');assert.equal(await page.getByRole('tab',{name:'JTML',exact:true}).getAttribute('aria-selected'),'true');
  await page.getByRole('navigation',{name:'Primary'}).getByRole('link',{name:'Tools',exact:true}).click();
  await page.getByRole('link',{name:'JTML home'}).click();
  const frame=page.frameLocator('iframe[title="Interactive Friendly JTML Counter"]');
  await frame.getByText('Count: 0',{exact:true}).waitFor();
  await frame.getByRole('button',{name:'Add one',exact:true}).click();await frame.getByText('Count: 1',{exact:true}).waitFor();
  await frame.getByRole('button',{name:'Add one',exact:true}).press('Enter');await frame.getByText('Count: 2',{exact:true}).waitFor();
  await frame.getByRole('button',{name:'Reset',exact:true}).click();await frame.getByText('Count: 0',{exact:true}).waitFor();
  const child=page.frames().find(f=>f.url().endsWith('/demo/counter/'));
  assert(await child.evaluate(()=>jtml.browserBuild.cleanProduction));
  assert(await child.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),'Counter mobile overflow');
  await page.screenshot({path:path.join(root,`../website-${prefix==='/'?'root':'project'}-${viewport.width}.png`),fullPage:true});
  await page.evaluate(()=>scrollTo(0,0));
  await page.screenshot({path:path.join(root,`../website-viewport-${prefix==='/'?'root':'project'}-${viewport.width}.png`)});
  await page.goto(origin+prefix+'demo/counter/');
  await page.getByRole('button',{name:'Add one',exact:true}).click();await page.getByText('Count: 1',{exact:true}).waitFor();
  await page.reload();await page.getByText('Count: 0',{exact:true}).waitFor();
  await context.close();
 }
 const negative=await browser.newPage();const blocked=[];
 await negative.exposeFunction('observeBlocked',d=>blocked.push(d));
 await negative.addInitScript(()=>document.addEventListener('securitypolicyviolation',e=>window.observeBlocked(e.effectiveDirective)));
 await negative.goto(origin+'/__csp_negative__');await negative.getByRole('button',{name:'Blocked'}).click();
 assert.equal(await negative.evaluate(()=>window.inlineRan),undefined);
 assert.equal(await negative.getByRole('button').textContent(),'Blocked');
 await negative.waitForFunction(()=>document.readyState==='complete');
 assert(blocked.includes('script-src-elem')&&blocked.includes('script-src-attr'),'both inline negatives must actually be blocked');
 await negative.close();
 assert.deepEqual(errors,[]);assert.deepEqual(violations,[]);assert(mime.length>0);
 assert(requests.every(u=>new URL(u).origin===origin),'no external runtime requests');
 assert(!requests.some(u=>/\/api\/|\/ws\b/.test(u)),'no backend or live runtime dependency');
 console.log('PASS website: root/project paths; all HTML navigation/assets and JS MIME; desktop/mobile no overflow; real JTML Counter click/keyboard/reset/reload in iframe and standalone; strict meta script CSP with independent inline-script/handler negatives; snippets keyboard navigation; no unexpected console/CSP/backend requests');
})().catch(e=>{console.error(e);process.exitCode=1;}).finally(async()=>{if(browser)await browser.close();await new Promise(r=>server.close(r));});
