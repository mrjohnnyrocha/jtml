// Actual static website and compiler output, no simulated Counter state.
const assert = require('node:assert/strict');
const fs = require('node:fs'), path = require('node:path'), http = require('node:http'), crypto = require('node:crypto');
const {createRequire} = require('node:module');
const root = path.resolve(process.argv[2] || 'dist/site');
assert(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT && process.env.JTML_BENCH_BROWSER, 'real browser required; no skip');
const {chromium} = createRequire(path.resolve(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT,'package.json'))('playwright');
const sha = p => crypto.createHash('sha256').update(fs.readFileSync(p)).digest('hex');
const info = JSON.parse(fs.readFileSync(path.join(root,'site-build.json')));
assert.equal(info.schema,2);
assert.equal(sha(path.join(root,'index.jtml')),info.homepageSourceSHA256);
assert.equal(info.compiler.binarySHA256.length,64);
for(const name of ['jtml-runtime.js','components/index.js','app.js'])assert.equal(sha(path.join(root,name)),info.homepageGeneratedAssetsSHA256[name]);
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
 for(const prefix of ['/','/jtml/'])for(const viewport of [{width:1440,height:1000},{width:390,height:844},{width:320,height:720}]){
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
   const layout=await page.evaluate(()=>({viewport:innerWidth,document:document.documentElement.scrollWidth}));
   assert(layout.document<=layout.viewport,`no page-wide horizontal overflow: ${prefix}${file} ${JSON.stringify(layout)}`);
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
  assert.equal(await page.getByRole('link',{name:'Download RC.3 ↗',exact:true}).getAttribute('href'),'https://github.com/mrjohnnyrocha/jtml/releases/tag/v0.3.0-rc.3');
  assert(await page.evaluate(()=>jtml.browserBuild.cleanProduction));
  assert.equal(await page.locator('h1').count(),1);
  assert.equal(await page.locator('html').getAttribute('lang'),'en');
  assert.equal(await page.locator('main').count(),1);
  await page.keyboard.press('Tab');
  assert.equal(await page.locator(':focus').textContent(),'Skip to content');
  await page.keyboard.press('Enter');
  assert.equal(await page.locator(':focus').getAttribute('id'),'content');
  const navigation=page.getByRole('navigation',{name:'Primary',exact:true});
  for(const link of await navigation.getByRole('link').all())assert(await link.isVisible(),'mobile navigation visible');
  await navigation.getByRole('link',{name:'Try it',exact:true}).click();
  assert.equal(new URL(page.url()).hash,'#demo');
  const counter=page.getByRole('region',{name:'Interactive JTML Counter'});
  await counter.locator('.count').filter({hasText:/^0$/}).waitFor();
  await counter.getByRole('button',{name:'Add one +',exact:true}).click();
  await counter.locator('.count').filter({hasText:/^1$/}).waitFor();
  assert.equal(await counter.locator('.derived').textContent(),'Doubled: 2');
  await counter.getByRole('button',{name:'Add one +',exact:true}).press('Enter');
  await counter.locator('.count').filter({hasText:/^2$/}).waitFor();
  await counter.getByRole('button',{name:'Add one +',exact:true}).press('Space');
  await counter.locator('.count').filter({hasText:/^3$/}).waitFor();
  assert.equal(await counter.locator('.derived').textContent(),'Doubled: 6');
  await counter.getByRole('button',{name:'Reset',exact:true}).click();
  await counter.locator('.count').filter({hasText:/^0$/}).waitFor();
  await page.reload();
  await counter.locator('.count').filter({hasText:/^0$/}).waitFor();
  assert.equal(await counter.locator('.derived').textContent(),'Doubled: 0');
  await page.screenshot({path:path.join(root,`../website-${prefix==='/'?'root':'project'}-${viewport.width}.png`),fullPage:true});
  await page.evaluate(()=>scrollTo({top:0,left:0,behavior:'instant'}));
  await page.screenshot({path:path.join(root,`../website-viewport-${prefix==='/'?'root':'project'}-${viewport.width}.png`)});
  await page.goto(origin+prefix+'demo/counter/');
  assert(await page.evaluate(()=>jtml.browserBuild.cleanProduction));
  await page.getByRole('button',{name:'Add one',exact:true}).click();await page.getByText('Count: 1',{exact:true}).waitFor();
  await page.getByRole('button',{name:'Add one',exact:true}).press('Enter');await page.getByText('Count: 2',{exact:true}).waitFor();
  await page.getByRole('button',{name:'Reset',exact:true}).click();await page.getByText('Count: 0',{exact:true}).waitFor();
  await page.reload();await page.getByText('Count: 0',{exact:true}).waitFor();
  console.log(`PASS ${prefix} ${viewport.width}px: all six pages, homepage Counter and original Counter`);
  await context.close();
 }
 const negative=await browser.newPage();negative.setDefaultTimeout(7000);
 await negative.addInitScript(()=>{
  window.cspNegativeEvents=[];
  document.addEventListener('securitypolicyviolation',e=>window.cspNegativeEvents.push({
   directive:e.effectiveDirective,blockedURI:e.blockedURI,disposition:e.disposition,
   trusted:e.isTrusted,browserMs:performance.now(),readyState:document.readyState
  }));
 });
 await negative.goto(origin+'/__csp_negative__');await negative.getByRole('button',{name:'Blocked'}).click();
 // CSP events are queued tasks; document readiness is not an event-delivery barrier.
 // Wait in the browser to avoid racing an exposed-function callback into Node.
 try{
  await negative.waitForFunction(()=>['script-src-elem','script-src-attr'].every(d=>window.cspNegativeEvents.some(e=>e.directive===d)));
 }catch(error){
  const observed=await negative.evaluate(()=>({events:window.cspNegativeEvents,inlineRan:window.inlineRan===true,button:document.querySelector('button').textContent}));
  throw new Error('Missing independent CSP blocks: '+JSON.stringify(observed),{cause:error});
 }
 const blocked=await negative.evaluate(()=>window.cspNegativeEvents);
 assert.deepEqual(blocked.map(e=>e.directive).sort(),['script-src-attr','script-src-elem']);
 for(const event of blocked){assert.equal(event.blockedURI,'inline');assert.equal(event.disposition,'enforce');assert.equal(event.trusted,true);}
 assert.equal(await negative.evaluate(()=>window.inlineRan),undefined);
 assert.equal(await negative.getByRole('button').textContent(),'Blocked');
 console.log('CSP negative evidence: '+JSON.stringify(blocked));
 await negative.close();
 assert.deepEqual(errors,[]);assert.deepEqual(violations,[]);assert(mime.length>0);
 assert(requests.every(u=>new URL(u).origin===origin),'no external runtime requests');
 assert(!requests.some(u=>/\/api\/|\/ws\b/.test(u)),'no backend or live runtime dependency');
 console.log('PASS website: root/project paths; all HTML navigation/assets and JS MIME; desktop/mobile no overflow; real JTML homepage component and standalone Counter click/keyboard/reset/reload; strict meta script CSP with independent inline-script/handler negatives; skip link and visible mobile navigation; no unexpected console/CSP/backend requests');
})().catch(e=>{console.error(e);process.exitCode=1;}).finally(async()=>{if(browser)await browser.close();await new Promise(r=>server.close(r));});
