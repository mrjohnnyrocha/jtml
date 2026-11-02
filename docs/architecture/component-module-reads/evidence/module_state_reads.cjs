// Desired module-read contract; baseline negative regression, not a waived test.
const assert = require('node:assert/strict');
const fs = require('node:fs'), path = require('node:path'), http = require('node:http');
const {spawnSync} = require('node:child_process'), {createRequire} = require('node:module');
const repo = path.resolve(__dirname, '../../../..');
assert(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT && process.env.JTML_BENCH_BROWSER, 'Chromium and Playwright required; no skip');
const {chromium} = createRequire(path.resolve(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT, 'package.json'))('playwright');
const fixtures = path.join(__dirname, 'fixtures');
const out = path.resolve(process.env.JTML_MODULE_READ_RESULTS || path.join(repo, 'dist/module-state-reads'));
fs.mkdirSync(out, {recursive:true});
const results = {cases:[], failures:[], cleanExit:false};
let root, server, browser;
(async()=>{
 server=http.createServer((req,res)=>{
  const file=path.resolve(root,'.'+new URL(req.url,'http://localhost').pathname);
  if(!file.startsWith(root+path.sep)||!fs.existsSync(file)||!fs.statSync(file).isFile()){res.writeHead(404).end();return;}
  res.setHeader('Content-Type',file.endsWith('.js')?'text/javascript':'text/html');
  res.end(fs.readFileSync(file));
 });
 await new Promise(r=>server.listen(0,'127.0.0.1',r));
 browser=await chromium.launch({headless:true,executablePath:process.env.JTML_BENCH_BROWSER});
 results.browser=browser.version();
 for(const name of fs.readdirSync(fixtures).sort()){
  root=path.join(out,name);
  const built=spawnSync(process.env.JTML_BIN||path.join(repo,'build/jtml'),['build',path.join(fixtures,name,'index.jtml'),'--target','browser','--out',root],{encoding:'utf8',timeout:60000});
  assert.equal(built.status,0,built.stdout+built.stderr);
  const page=await browser.newPage(), row={name,classes:[],retained:[],errors:[]};
  page.on('pageerror',e=>row.errors.push(e.message));
  await page.goto(`http://127.0.0.1:${server.address().port}/index.html`);
  await page.evaluate(()=>{
   window.probeParent=document.querySelector('[data-jtml-component="Probe"]');
   window.probeElement=window.probeParent.querySelector('[data-jtml-direct-node="page"], [data-jtml-direct-node="box"]');
  });
  for(let step=0;step<3;step++){
   if(step&& !'AB'.includes(name[0]))await page.getByRole('button',{name:'Toggle',exact:true}).click();
   const snapshot=await page.evaluate(()=>{
    const parent=document.querySelector('[data-jtml-component="Probe"]');
    const el=parent.querySelector('[data-jtml-direct-node="page"], [data-jtml-direct-node="box"]');
    return {value:el.getAttribute('class'),retained:el===window.probeElement&&parent===window.probeParent};
   });
   row.classes.push(snapshot.value);row.retained.push(snapshot.retained);
  }
  const prefix=name.includes('-box-')?'jtml-box ':'';
  const expected='AB'.includes(name[0])?Array(3).fill(prefix+'literal'):
   name[0]==='E'?['false','true','false'].map(x=>prefix+x):['light','dark','light'].map(x=>prefix+x);
  row.expected=expected;
  try{assert.deepEqual(row.classes,expected);assert(row.retained.every(Boolean));assert.deepEqual(row.errors,[]);}
  catch(e){results.failures.push({name,message:e.message});}
  results.cases.push(row);await page.close();
 }
})().catch(e=>results.failures.push({fatal:String(e)})).finally(async()=>{
 try{if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));results.cleanExit=true;}
 finally{fs.writeFileSync(path.join(out,'results.json'),JSON.stringify(results,null,2)+'\n');}
 console.log(JSON.stringify(results,null,2));process.exitCode=results.failures.length?1:0;
});
