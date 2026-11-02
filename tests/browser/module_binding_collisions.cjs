const assert = require('node:assert/strict');
const fs = require('node:fs'), path = require('node:path'), http = require('node:http');
const {spawnSync} = require('node:child_process'), {createRequire} = require('node:module');
const repo = path.resolve(__dirname, '../..');
assert(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT && process.env.JTML_BENCH_BROWSER, 'Chromium and Playwright required; no skip');
const {chromium} = createRequire(path.resolve(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT, 'package.json'))('playwright');
const fixture = path.join(repo,'tests/fixtures/production/module-binding-collision');
const out = path.join(repo,'dist/module-binding-collisions');
fs.mkdirSync(out,{recursive:true});
const bin=process.env.JTML_BIN||path.join(repo,'build/jtml');
const build=(source,target)=>spawnSync(bin,['build',source,'--target','browser','--out',target],{encoding:'utf8',timeout:60000});
let server,browser,root;
(async()=>{
 const rejected=fs.mkdtempSync(path.join(out,'rejected-'));
 const sentinel=path.join(rejected,'index.html');fs.writeFileSync(sentinel,'previous successful output');
 const result=build(path.join(fixture,'index.jtml'),rejected);
 assert.notEqual(result.status,0,'Unsafe cross-module collision must reject before publishing assets');
 for(const name of ['tone','state','index.jtml','private.jtml'])assert(result.stderr.includes(name),result.stderr);
 assert.equal(fs.readFileSync(sentinel,'utf8'),'previous successful output');
 for(const file of ['app.js','jtml-runtime.js','components/index.js'])assert(!fs.existsSync(path.join(rejected,file)));
 const positive=path.join(out,'positive');fs.mkdirSync(positive,{recursive:true});
 for(const name of ['index','private'])fs.writeFileSync(path.join(positive,name+'.jtml'),fs.readFileSync(path.join(fixture,name+'.jtml'),'utf8').replace(/\btone\b/g,name==='index'?'entryTone':'privateTone'));
 root=path.join(out,'positive-dist');
 const good=build(path.join(positive,'index.jtml'),root);assert.equal(good.status,0,good.stdout+good.stderr);
 server=http.createServer((req,res)=>{
  const file=path.resolve(root,'.'+new URL(req.url,'http://localhost').pathname);
  if(!file.startsWith(root+path.sep)||!fs.existsSync(file)||!fs.statSync(file).isFile()){res.writeHead(404).end();return;}
  res.setHeader('Content-Type',file.endsWith('.js')?'text/javascript':'text/html');res.end(fs.readFileSync(file));
 });
 await new Promise(r=>server.listen(0,'127.0.0.1',r));
 browser=await chromium.launch({headless:true,executablePath:process.env.JTML_BENCH_BROWSER});
 const page=await browser.newPage(),errors=[];page.on('pageerror',e=>errors.push(e.message));
 await page.goto(`http://127.0.0.1:${server.address().port}/index.html`);
 const state=()=>page.evaluate(()=>({entry:window.jtml.state.entryTone,private:window.jtml.state.privateTone}));
 assert.deepEqual(await state(),{entry:'entry',private:'private'});
 await page.getByRole('button',{name:'Private',exact:true}).click();
 assert.deepEqual(await state(),{entry:'entry',private:'private-updated'});
 await page.getByRole('button',{name:'Entry',exact:true}).click();
 assert.deepEqual(await state(),{entry:'entry-updated',private:'private-updated'});
 assert.deepEqual(errors,[]);
})().finally(async()=>{if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));})
 .then(()=>console.log('PASS module binding collisions: rejected before assets; disjoint module actions isolated; clean Chromium exit'))
 .catch(e=>{console.error(e);process.exitCode=1;});
