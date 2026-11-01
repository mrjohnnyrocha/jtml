// Actual static website and compiler output, no simulated Counter state.
const assert = require('node:assert/strict');
const fs = require('node:fs'), path = require('node:path'), http = require('node:http'), crypto = require('node:crypto');
const {createRequire} = require('node:module');
const root = path.resolve(process.argv[2] || 'dist/site');
assert(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT && process.env.JTML_BENCH_BROWSER, 'real browser required; no skip');
const {chromium} = createRequire(path.resolve(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT,'package.json'))('playwright');
const sha = p => crypto.createHash('sha256').update(fs.readFileSync(p)).digest('hex');
const info = JSON.parse(fs.readFileSync(path.join(root,'site-build.json')));
assert.equal(info.schema,5);
assert.equal(info.originalCompilerOutputSuffix,'.compiler-output');
const originals=root+info.originalCompilerOutputSuffix;
assert.equal(info.sharedRuntime.path,'jtml-runtime.js');
assert.deepEqual(info.sharedRuntime.verifiedByteEqualPages,[...Object.keys(info.pages)].sort());
assert.equal(sha(path.join(root,info.sharedRuntime.path)),info.sharedRuntime.sha256);
const registry=JSON.parse(fs.readFileSync(path.join(root,'pages.json')));
assert.equal(sha(path.join(root,'pages.json')),info.registrySHA256);
assert.equal(registry.pages.length,6);
assert.deepEqual(Object.keys(info.pages).sort(),registry.pages.map(page=>page.output).sort());
const authored=fs.readdirSync(path.resolve(__dirname,'../../site'),{recursive:true});
assert(!authored.some(file=>file.endsWith('.html')||file.endsWith('.js')),'site sources must be JTML, not page HTML/application JS');
const migrated=JSON.parse(fs.readFileSync(path.join(__dirname,'../fixtures/site/migrated-pages.json')));
const normalize=text=>text.replace(/\s+/g,' ').trim();
assert.equal(sha(path.join(root,'index.jtml')),info.homepageSourceSHA256);
assert.equal(info.compiler.binarySHA256.length,64);
for(const [page,entry] of Object.entries(info.pages)){
 for(const [file,hash] of Object.entries(entry.sourceInputsSHA256))assert.equal(sha(path.join(root,file.replace(/^site\//,''))),hash);
 for(const [name,hash] of Object.entries(entry.generatedAssetsSHA256))assert.equal(sha(path.join(originals,entry.metadata.id,name)),hash,'original compiler output');
 for(const name of ['jtml-runtime.js','components/index.js','app.js']){
  const deployed=path.join(root,name==='jtml-runtime.js'?'':entry.assetDirectory,name);
  assert.equal(sha(deployed),entry.generatedAssetsSHA256[name]);
  assert(fs.readFileSync(deployed).equals(fs.readFileSync(path.join(originals,entry.metadata.id,name))),'byte-identical deployed JS');
 }
}
for(const page of registry.pages.filter(page=>page.assetDirectory!=='.'))assert.deepEqual(fs.readdirSync(path.join(root,page.assetDirectory)).sort(),['app.js','components']);
assert(!/\bCounter\b/.test(fs.readFileSync(path.join(root,'reference-assets/components/index.js'),'utf8')),'Reference must not include homepage Counter');
assert(!/\bReferenceTopics\b/.test(fs.readFileSync(path.join(root,'components/index.js'),'utf8')),'homepage must not include Reference state');
const payload=JSON.parse(fs.readFileSync(path.join(root,'site-payloads.json')));
assert.equal(payload.schema,3);
assert.deepEqual(Object.keys(payload.pages).sort(),[...Object.keys(info.pages)].sort());
assert.equal(payload.crossPage.compiledRuntimeCopies,6);
assert.equal(payload.crossPage.deployedRuntimeCopies,1);
for(const entry of Object.values(payload.pages))for(const [file,size] of Object.entries(entry.files)){
 const bytes=fs.readFileSync(path.join(root,file));assert.equal(bytes.length,size.raw);assert(size.gzip>0&&size.gzip<=size.raw,'gzip measurement bounds');
}
const snippets=JSON.parse(fs.readFileSync(path.join(__dirname,'../fixtures/site/reference-snippets.json')));
const baselineInput=process.argv[3]||process.env.JTML_WEBSITE_BASELINE;
const baseline=baselineInput ? path.resolve(baselineInput) : null;

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
const pages=registry.pages.map(page=>page.output);
assert.deepEqual(fs.readdirSync(root).filter(n=>n.endsWith('.html')).sort(),[...pages].sort());
const errors=[], violations=[], requests=[], mime=[];
const server=http.createServer((req,res)=>{
  const url=new URL(req.url,'http://localhost');let relative=decodeURIComponent(url.pathname);
  if(relative.startsWith('/jtml/'))relative=relative.slice('/jtml'.length);
  if(relative.endsWith('/'))relative+='index.html';
  if(relative==='/__csp_negative__'){
    const meta=fs.readFileSync(path.join(root,'index.html'),'utf8').match(/<meta http-equiv="Content-Security-Policy"[^>]*>/)[0];
    res.setHeader('Content-Type','text/html');res.end(`${meta}<script>window.inlineRan=true</script><button onclick="this.textContent='FAILED'">Blocked</button>`);return;
  }
  let fileRoot=root;
  if(baseline&&relative.startsWith('/__baseline/')){fileRoot=baseline;relative=relative.slice('/__baseline'.length);}
  const file=path.resolve(fileRoot,'.'+relative);
  if(!file.startsWith(fileRoot+path.sep)||!fs.existsSync(file)||!fs.statSync(file).isFile()){res.writeHead(404);res.end('Not found');return;}
  if(relative==='/jtml-runtime.js')res.setHeader('Cache-Control','public, max-age=3600');
  const type={'.html':'text/html','.js':'text/javascript','.css':'text/css','.json':'application/json','.jtml':'text/plain','.md':'text/plain','.png':'image/png','.txt':'text/plain','.xml':'application/xml','.svg':'image/svg+xml'}[path.extname(file)]||'application/octet-stream';
  res.setHeader('Content-Type',type);res.end(fs.readFileSync(file));
});
let browser;
(async()=>{
 await new Promise(r=>server.listen(0,'127.0.0.1',r));
 const origin=`http://127.0.0.1:${server.address().port}`;
 browser=await chromium.launch({headless:true,executablePath:process.env.JTML_BENCH_BROWSER});
 for(const prefix of ['/','/jtml/'])for(const viewport of [{width:1440,height:1000},{width:900,height:1100},{width:390,height:844},{width:320,height:720}]){
  const context=await browser.newContext({viewport});const page=await context.newPage();page.setDefaultTimeout(7000);
  page.on('pageerror',e=>errors.push(e.message));page.on('console',m=>{if(m.type()==='error')errors.push(m.text());});
  page.on('response',r=>{if(r.status()>=400)errors.push(`${r.status()} ${r.url()}`);if(r.url().endsWith('.js')){assert(r.headers()['content-type'].startsWith('text/javascript'));mime.push(r.url());}});
  page.on('request',r=>requests.push(r.url()));page.on('websocket',ws=>errors.push('unexpected WebSocket '+ws.url()));
  await page.exposeFunction('observeWebsiteCSP',e=>violations.push(e));
  await page.addInitScript(()=>document.addEventListener('securitypolicyviolation',e=>window.observeWebsiteCSP(e.effectiveDirective)));
  const documents=new Map(),fragmentLinks=[];
  for(const file of pages){
   const requestStart=requests.length;
   await page.goto(origin+prefix+file);
   const definition=registry.pages.find(page=>page.output===file);
   if(info.pages[file]){
    assert(await page.evaluate(()=>jtml.browserBuild.cleanProduction));
    const assetRoot=new URL(info.pages[file].assetDirectory+'/',origin+prefix).pathname;
    const js=requests.slice(requestStart).filter(url=>new URL(url).pathname.endsWith('.js')).map(url=>new URL(url).pathname).sort();
    assert.deepEqual(js,[prefix+'jtml-runtime.js',assetRoot+'app.js',assetRoot+'components/index.js'].sort(),'shared runtime and isolated page bundle '+file);
   }
   assert.equal(await page.locator('meta[http-equiv="Content-Security-Policy"]').count(),1);
   assert.equal(await page.locator('link[rel="icon"]').count(),1);
   assert.equal(await page.title(),definition.title);
   assert.equal(await page.locator('meta[name="description"]').getAttribute('content'),definition.description);
   assert.equal(await page.locator('meta[name="viewport"]').count(),1);
   assert.equal(await page.locator('html').getAttribute('lang'),'en');
   assert(/<h1\b/.test(fs.readFileSync(path.join(root,file),'utf8')),'generated HTML contains native heading structure');
   if(definition.themeColor)assert.equal(await page.locator('meta[name="theme-color"]').getAttribute('content'),definition.themeColor);
   assert.equal(await page.locator('h1').count(),1);assert.equal(await page.locator('main').count(),1);
   assert.equal(await page.locator('link[rel="canonical"]').getAttribute('href'),info.baseURL+(file==='index.html'?'':file));
   assert.deepEqual(await page.locator('link[rel="stylesheet"]').evaluateAll(es=>es.map(e=>e.getAttribute('href'))),definition.css.map(css=>'./'+css));
   documents.set(file,new Set(await page.locator('[id]').evaluateAll(es=>es.map(e=>e.id))));
   if(migrated[file]){
    const expected=migrated[file];
    assert.equal(await page.getByRole('button').count(),0,'no invented interactive controls on static pages');
    for(const tag of ['h1','h2','h3'])assert.deepEqual((await page.locator('main '+tag).allTextContents()).map(normalize),expected.headings.filter(h=>h.tag===tag).map(h=>h.text),file+' headings');
    assert.deepEqual((await page.locator('main p').allTextContents()).map(normalize),expected.paragraphs,file+' prose');
    assert.deepEqual(await page.locator('main code').allTextContents(),expected.code,file+' literal listings');
    assert.equal(normalize(await page.locator('.footer-note').innerText()),expected.footer,file+' footer material');
    const links=await page.getByRole('link').evaluateAll(es=>es.map(e=>({href:e.href,name:e.getAttribute('aria-label')||e.textContent.replace(/\s+/g,' ').trim()})));
    const target=href=>{const u=new URL(href,page.url());if(u.pathname.endsWith('/'))u.pathname+='index.html';return u.href;};
    for(const link of expected.links)assert(links.some(actual=>target(actual.href)===target(link.href)&&actual.name===link.text),file+' preserved link '+JSON.stringify(link));
    await page.keyboard.press('Tab');assert.equal(await page.locator(':focus').textContent(),'Skip to content');
    await page.keyboard.press('Enter');assert.equal(await page.locator(':focus').getAttribute('id'),'page-start');
    await page.evaluate(()=>{document.activeElement.blur();scrollTo({top:0,left:0,behavior:'instant'});});
   }
   if(viewport.width!==320)await page.screenshot({path:path.join(root,`../website-six-${file.replace('.html','')}-${prefix==='/'?'root':'project'}-${viewport.width}.png`),fullPage:true});
   assert((await page.locator('meta[http-equiv="Content-Security-Policy"]').getAttribute('content')).includes("script-src 'self'"));
   assert.equal(await page.locator('script:not([src]):not([type="application/json"])').count(),0);
   assert.equal(await page.evaluate(()=>[...document.querySelectorAll('*')].filter(e=>e.getAttributeNames().some(a=>/^on/i.test(a))).length),0);
   const layout=await page.evaluate(()=>({viewport:innerWidth,document:document.documentElement.scrollWidth}));
   assert(layout.document<=layout.viewport,`no page-wide horizontal overflow: ${prefix}${file} ${JSON.stringify(layout)}`);
   // Resolve actual navigation/assets and authored fragment targets under the mount prefix.
   const links=await page.locator('a[href],link[rel="stylesheet"],link[rel="icon"],script[src],iframe[src]').evaluateAll(es=>es.map(e=>e.href||e.src));
   for(const link of links){
    const u=new URL(link);if(u.origin!==origin)continue;
    assert(u.pathname.startsWith(prefix),'link escapes project path: '+u.pathname);
    const response=await context.request.get(u.href);assert.equal(response.status(),200,u.href);
    if(u.hash){let target=u.pathname.slice(prefix.length)||'index.html';fragmentLinks.push({target,hash:decodeURIComponent(u.hash.slice(1))});}
   }
  }
  for(const link of fragmentLinks)if(documents.has(link.target))assert(documents.get(link.target).has(link.hash),'missing target fragment '+JSON.stringify(link));
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
  if(baseline){
   for(const file of ['index.html','reference.html']){
    await page.goto(origin+prefix+file);
    const current=await page.screenshot({fullPage:true});
    await page.goto(origin+'/__baseline/'+file);
    assert(current.equals(await page.screenshot({fullPage:true})),file+' visual regression against clean-main baseline');
   }
  }
  await page.goto(origin+prefix+'reference.html');
  assert.equal(await page.title(),'JTML Reference — Friendly JTML 2');
  assert.equal(await page.locator('main').count(),1);assert.equal(await page.locator('h1').count(),1);
  assert.equal(await page.locator('html').getAttribute('lang'),'en');
  await page.keyboard.press('Tab');
  assert.equal(await page.locator(':focus').textContent(),'Skip to reference');
  await page.keyboard.press('Enter');
  assert.equal(await page.locator(':focus').getAttribute('id'),'reference-start');
  assert.deepEqual(await page.locator('.reference-topic pre code').allTextContents(),snippets.map(item=>item.code),'preserve all original source examples literally');
  const toggle=page.getByRole('button',{name:'Hide code examples',exact:true});
  assert.equal(await toggle.getAttribute('aria-expanded'),'true');
  await toggle.click();
  assert.equal(await page.locator('.reference-topic pre').count(),0);
  const show=page.getByRole('button',{name:'Show code examples',exact:true});
  assert(await show.evaluate(el=>el===document.activeElement),'toggle retains keyboard focus');
  assert.equal(await show.getAttribute('aria-expanded'),'false');
  await show.press('Enter');
  assert.equal(await page.locator('.reference-topic pre').count(),9);
  await page.getByRole('button',{name:'Hide code examples',exact:true}).press('Space');
  assert.equal(await page.locator('.reference-topic pre').count(),0);
  await page.reload();assert.equal(await page.locator('.reference-topic pre').count(),9);
  for(const item of snippets){
   await page.getByRole('navigation',{name:'Reference topics',exact:true}).getByRole('link',{name:item.title,exact:true}).click();
   assert.equal(new URL(page.url()).hash,'#'+item.id);
   assert.equal(await page.locator('#'+item.id+' h2').textContent(),item.title);
  }
  await page.getByRole('navigation',{name:'Primary',exact:true}).getByRole('link',{name:'Language',exact:true}).click();
  assert.equal(new URL(page.url()).pathname,prefix+'index.html');assert.equal(new URL(page.url()).hash,'#syntax');
  await page.goto(origin+prefix+'reference.html');
  await page.evaluate(()=>scrollTo({top:0,left:0,behavior:'instant'}));
  await page.screenshot({path:path.join(root,`../website-reference-${prefix==='/'?'root':'project'}-${viewport.width}.png`),fullPage:true});
  await page.goto(origin+prefix+'demo/counter/');
  assert(await page.evaluate(()=>jtml.browserBuild.cleanProduction));
  await page.getByRole('button',{name:'Add one',exact:true}).click();await page.getByText('Count: 1',{exact:true}).waitFor();
  await page.getByRole('button',{name:'Add one',exact:true}).press('Enter');await page.getByText('Count: 2',{exact:true}).waitFor();
  await page.getByRole('button',{name:'Reset',exact:true}).click();await page.getByText('Count: 0',{exact:true}).waitFor();
  await page.reload();await page.getByText('Count: 0',{exact:true}).waitFor();
  console.log(`PASS ${prefix} ${viewport.width}px: all six JTML pages/content/metadata, isolated bundles, Reference and both Counters`);
  await context.close();
 }
 // Real HTTP cache reuse across navigation; CDP reports cache hits as well as requests.
 for(const prefix of ['/','/jtml/']){
  const context=await browser.newContext();const page=await context.newPage();
  const cdp=await context.newCDPSession(page);await cdp.send('Network.enable');
  const cached=new Set(),runtimeRequests=new Set();
  cdp.on('Network.requestWillBeSent',e=>{if(e.request.url===origin+prefix+'jtml-runtime.js')runtimeRequests.add(e.requestId);});
  cdp.on('Network.requestServedFromCache',e=>cached.add(e.requestId));
  for(const file of pages)await page.goto(origin+prefix+file);
  assert.equal(runtimeRequests.size,6,'each page requires the shared runtime');
  assert([...runtimeRequests].filter(id=>cached.has(id)).length>=5,'later pages reuse cacheable shared runtime');
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
 console.log('PASS website: root/project paths; all HTML navigation/assets and JS MIME; desktop/mobile no overflow; isolated JTML Reference disclosure and both Counters; strict meta script CSP with independent inline-script/handler negatives; skip link and visible mobile navigation; no unexpected console/CSP/backend requests');
})().catch(e=>{console.error(e);process.exitCode=1;}).finally(async()=>{if(browser)await browser.close();await new Promise(r=>server.close(r));});
