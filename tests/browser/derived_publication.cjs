// Root publication only; original authored vector sources/oracles remain immutable.
const assert=require('node:assert/strict'), fs=require('node:fs'), path=require('node:path'), http=require('node:http'), zlib=require('node:zlib');
const {spawnSync}=require('node:child_process'),{createRequire}=require('node:module');
assert(process.env.JTML_BENCH_BROWSER&&process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT,'real Chromium required; no skip');
const {chromium}=createRequire(path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT,'package.json'))('playwright');
const repo=path.resolve(__dirname,'../..'),out=path.join(repo,'dist/derived-publication'),bin=process.env.JTML_BIN||path.join(repo,'build/jtml');
const vectors=JSON.parse(fs.readFileSync(path.join(repo,'docs/spec/reactive-core-v1/vectors.json'))).vectors;
const report={checks:[],errors:[],cleanExit:false};let root,server,browser;
function compile(files,id,flags=[]) {
 const source=path.join(out,id+'-source');fs.mkdirSync(source,{recursive:true});
 for(const [name,text]of Object.entries(files))fs.writeFileSync(path.join(source,name),text);
 root=path.join(out,id);const r=spawnSync(bin,['build',path.join(source,'index.jtml'),'--target','browser','--out',root,...flags],{cwd:repo,encoding:'utf8'});
 assert.equal(r.status,0,r.stdout+r.stderr);return root;
}
async function open() {
 const page=await browser.newPage();page.setDefaultTimeout(4000);
 page.on('pageerror',e=>report.errors.push(e.message));page.on('console',m=>{if(m.type()==='error')report.errors.push(m.text());});
 // Observe cached values at every DOM text write, not just eventual DOM snapshots.
 await page.addInitScript(()=>{
  window.publicationWrites=[];const d=Object.getOwnPropertyDescriptor(Node.prototype,'textContent');
  Object.defineProperty(Node.prototype,'textContent',{...d,set(value){
   if(this.nodeType===1&&this.closest('.out')&&window.jtml?.state)
    publicationWrites.push({state:JSON.parse(JSON.stringify(jtml.state)),value:String(value)});
   d.set.call(this,value);
  }});
 });
 await page.goto(`http://127.0.0.1:${server.address().port}/index.html`);return page;
}
async function snapshot(page,expected) {
 assert.deepEqual((await page.locator('.out').allTextContents()).map(x=>x.trim()),expected.dom['.out']);
 const state=await page.evaluate(()=>jtml.state);
 for(const [id,value]of Object.entries({...expected.state,...expected.derived}))assert.equal(state[id.split('::').pop()],value,id);
}
(async()=>{
 fs.mkdirSync(out,{recursive:true});server=http.createServer((req,res)=>{
  const file=path.resolve(root,'.'+new URL(req.url,'http://localhost').pathname);
  if(!file.startsWith(root+path.sep)||!fs.existsSync(file)||!fs.statSync(file).isFile()){res.writeHead(404).end();return;}
  res.setHeader('Content-Security-Policy',"script-src 'self'");res.setHeader('Content-Type',file.endsWith('.js')?'text/javascript':'text/html');res.end(fs.readFileSync(file));
 });await new Promise(r=>server.listen(0,'127.0.0.1',r));
 browser=await chromium.launch({headless:true,executablePath:process.env.JTML_BENCH_BROWSER});report.browser=browser.version();
 for(const id of ['V04','V05','V01','V03']) {
  const v=vectors.find(v=>v.id===id);compile(v.files,id);const page=await open();
  await snapshot(page,v.expected_initial);
  for(const step of v.steps){await page.getByRole('button',{name:'Step',exact:true}).click();await snapshot(page,step.expected);}
  const writes=await page.evaluate(()=>publicationWrites);
  if(id==='V04'||id==='V05')for(const {state:s}of writes) {
   if(id==='V04'){assert.equal(s.z,s.x+1);assert.equal(s.a,(s.x+1)*2);}
   else {assert.equal(s.zleft,s.x+1);assert.equal(s.zright,s.x*2);assert.equal(s.atotal,s.x+1+s.x*2);}
  }
  assert(writes.length,'DOM writes actually observed');await page.close();report.checks.push(id+' authored initial/action/state/derived/DOM PASS');
 }
 // Both source declaration and lexical key order vary; no fixed number of render passes suffices.
 const formulas={z0:'x + 1'};for(let i=1;i<12;i++)formulas['z'+i]=`z${i-1} + 1`;
 Object.assign(formulas,{left:'z11 + 1',right:'z11 * 2',total:'left + right',final:'total + z11'});
 const names=Object.keys(formulas);
 for(const [i,order]of [names,[...names].reverse(),[...names].sort(),[...names.slice(7),...names.slice(0,7)]].entries()) {
  const declarations=order.map(n=>`get ${n} = ${formulas[n]}`).join('\n');
  compile({'index.jtml':`jtml 2\nlet x = 1\n${declarations}\nwhen step\n  x = x + 1\npage\n  box class "out"\n    text "{final}|{left}|{right}"\n  button "Step" click step\n`},'permutation-'+i);
  const page=await open();
  for(let x=1;x<=5;x++) {
   if(x>1)await page.getByRole('button',{name:'Step',exact:true}).click();
   assert.equal((await page.locator('.out').textContent()).trim(),`${4*(x+12)+1}|${x+13}|${2*(x+12)}`);
  }
  for(const {state:s}of await page.evaluate(()=>publicationWrites)){assert.equal(s.z11,s.x+12);assert.equal(s.final,4*(s.x+12)+1);}
  await page.close();report.checks.push('long chain / diamond permutation '+i+' PASS');
 }
 // Consumer module can sort before its defining imported getter's owner.
 compile({'index.jtml':'jtml 2\nuse {Card, z} from "./private.jtml"\nget a = z * 2\npage\n  box class "out"\n    text "{a}|{z}"\n  Card\n',
 'private.jtml':'jtml 2\nlet x = 1\nexport get z = x + 1\nwhen step\n  x = x + 1\nexport make Card\n  button "Step" click step\n'},'modules');
 const page=await open();for(let x=1;x<=4;x++){if(x>1)await page.getByRole('button',{name:'Step',exact:true}).click();assert.equal((await page.locator('.out').textContent()).trim(),`${2*(x+1)}|${x+1}`);}await page.close();
 report.checks.push('explicit imported getter ownership PASS');
 // Keep the existing structured collection-getter publication contract.
 compile({'index.jtml':'jtml 2\nlet x = 1\nget a = z.length\nget z = [x, x + 1]\nwhen step\n  x = x + 1\npage\n  box class "out"\n    text "{a}|{z[1]}"\n  button "Step" click step\n'},'collection');
 const collection=await open();for(let x=1;x<=3;x++){if(x>1)await collection.getByRole('button',{name:'Step',exact:true}).click();assert.equal((await collection.locator('.out').textContent()).trim(),`2|${x+1}`);}await collection.close();report.checks.push('existing structured collection getter PASS');
 const asset=fs.readFileSync(path.join(root,'jtml-runtime.js'));report.runtime={raw:asset.length,gzip:zlib.gzipSync(asset,{level:9}).length};
 assert(asset.length<=273000,'unchanged runtime budget');assert(!/\beval\s*\(|\bnew\s+Function\b/.test(asset.toString()));
 for(const flag of ['--compat','--debug']) {
  compile(vectors.find(v=>v.id==='V04').files,flag.slice(2),[flag]);const p=await open();
  await p.getByRole('button',{name:'Step',exact:true}).click();assert.equal((await p.locator('.out').textContent()).trim(),'4|3');await p.close();
  report.checks.push(flag+' unchanged PASS');
 }
 assert.deepEqual(report.errors,[]);
})().catch(e=>{report.failure=String(e);console.error(e);process.exitCode=1;}).finally(async()=>{
 if(browser){await browser.close();report.cleanExit=true;}if(server)await new Promise(r=>server.close(r));
 fs.writeFileSync(path.join(out,'results.json'),JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(report,null,2));
});
