// Compiler-certified module scalar storage; observe only real JTML show actions.
const assert = require('node:assert/strict');
const fs = require('node:fs'), path = require('node:path'), http = require('node:http');
const {spawnSync} = require('node:child_process'), {createRequire} = require('node:module');
const zlib = require('node:zlib');
const repo = path.resolve(__dirname, '../..');
assert(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT && process.env.JTML_BENCH_BROWSER, 'Chromium and Playwright required; no skip');
const {chromium} = createRequire(path.resolve(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT, 'package.json'))('playwright');
const bin = process.env.JTML_BIN || path.join(repo,'build/jtml');
const out = path.join(repo,'dist/module-scalar-storage');fs.mkdirSync(out,{recursive:true});
const fixture = path.join(repo,'tests/fixtures/production/module-scalar-storage');
const report={cases:[],errors:[],cleanExit:false};let root,server,browser;
const build=(source,target)=>spawnSync(bin,['build',source,'--target','browser','--out',target],{encoding:'utf8',timeout:60000});
function payload(dir){const files=['index.html','jtml-runtime.js','components/index.js','app.js'];const result={};for(const file of files){const b=fs.readFileSync(path.join(dir,file));result[file]={raw:b.length,gzip:zlib.gzipSync(b,{level:9}).length};}result.total={raw:Object.values(result).reduce((n,v)=>n+v.raw,0),gzip:Object.values(result).reduce((n,v)=>n+v.gzip,0)};return result;}
(async()=>{
 server=http.createServer((req,res)=>{const file=path.resolve(root,'.'+new URL(req.url,'http://localhost').pathname);if(!file.startsWith(root+path.sep)||!fs.existsSync(file)||!fs.statSync(file).isFile()){res.writeHead(404).end();return;}res.setHeader('Content-Type',file.endsWith('.js')?'text/javascript':'text/html');res.end(fs.readFileSync(file));});
 await new Promise(r=>server.listen(0,'127.0.0.1',r));
 for(const variant of ['two','reexport','three']){
  const source=path.join(out,variant);fs.mkdirSync(source,{recursive:true});
  let entry=fs.readFileSync(path.join(fixture,'index.jtml'),'utf8');
  fs.copyFileSync(path.join(fixture,'private.jtml'),path.join(source,'private.jtml'));
  if(variant==='reexport'){entry=entry.replace('./private.jtml','./barrel.jtml');fs.writeFileSync(path.join(source,'barrel.jtml'),'jtml 2\nexport use PrivateCard from "./private.jtml"\n');}
  const owners=['entry','private'];
  if(variant==='three'){
   owners.push('third');entry=entry.replace('jtml 2','jtml 2\nuse ThirdCard from "./aaa.jtml"')+'  ThirdCard\n';
   fs.writeFileSync(path.join(source,'aaa.jtml'),'jtml 2\nlet tone = "third"\nlet seed = tone\nwhen setThird\n  tone = tone + "!"\nwhen readThird\n  show tone\nwhen readThirdSeed\n  show seed\nwhen shadowThird tone\n  tone = tone + "?"\n  show tone\nexport make ThirdCard\n  box\n    button "Set third" click setThird\n    button "Read third" click readThird\n    button "Third seed" click readThirdSeed\n    button "Shadow third" click shadowThird("argument")\n');
  }
  fs.writeFileSync(path.join(source,'index.jtml'),entry);root=path.join(source,'dist');
  const built=build(path.join(source,'index.jtml'),root);assert.equal(built.status,0,built.stdout+built.stderr);
  if(!browser)browser=await chromium.launch({headless:true,executablePath:process.env.JTML_BENCH_BROWSER});
  const page=await browser.newPage(),shows=[];page.on('pageerror',e=>report.errors.push(e.message));page.on('console',m=>{if(m.type()==='error')report.errors.push(m.text());if(m.type()==='log'&&m.text().startsWith('[SHOW] '))shows.push(m.text().slice(7));});
  const expectShow=async(label,expected)=>{const before=shows.length;const shown=page.waitForEvent('console',{predicate:m=>m.type()==='log'&&m.text().startsWith('[SHOW] ')});await page.getByRole('button',{name:label,exact:true}).click();await shown;assert.equal(shows.length,before+1,`${variant}: ${label} must show once`);assert.equal(shows.at(-1),expected,`${variant}: ${label}`);};
  const cap=s=>s[0].toUpperCase()+s.slice(1);
  await page.goto(`http://127.0.0.1:${server.address().port}/index.html`);
  const values=Object.fromEntries(owners.map(o=>[o,o]));
  for(const o of owners){await expectShow('Read '+o,o);await expectShow(cap(o)+' seed',o);}
  for(const o of [...owners].reverse().concat(owners,[...owners].reverse(),owners)){
   await page.getByRole('button',{name:'Set '+o,exact:true}).click();values[o]+='!';
   for(const other of owners)await expectShow('Read '+other,values[other]);
  }
  for(const o of owners){await expectShow('Shadow '+o,'argument?');await expectShow('Read '+o,values[o]);await expectShow(cap(o)+' seed',o);}
  assert.equal(await page.evaluate(()=>Object.hasOwn(window.jtml.state,'tone')),false,'no public-name alias to either module');
  await page.reload();for(const o of owners)await expectShow('Read '+o,o);
  report.cases.push({variant,shows,payload:payload(root)});await page.close();
 }
 assert.deepEqual(report.errors,[]);
})().catch(e=>{report.failure=String(e);process.exitCode=1;}).finally(async()=>{if(browser){await browser.close();report.cleanExit=true;}if(server)await new Promise(r=>server.close(r));fs.writeFileSync(path.join(out,'results.json'),JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(report,null,2));});
