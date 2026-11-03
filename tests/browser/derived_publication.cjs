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
 // Observe actual Element.textContent writes inside .out after runtime bootstrap.
 await page.addInitScript(()=>{
  window.publicationWrites=[];const d=Object.getOwnPropertyDescriptor(Node.prototype,'textContent');
  Object.defineProperty(Node.prototype,'textContent',{...d,set(value){
   d.set.call(this,value);
   if(this.nodeType===1&&this.closest('.out')&&window.jtml?.state)
    publicationWrites.push({state:JSON.parse(JSON.stringify(jtml.state)),value:d.get.call(this)});
  }});
 });
 await page.goto(`http://127.0.0.1:${server.address().port}/index.html`);return page;
}
async function snapshot(page,expected) {
 assert.deepEqual((await page.locator('.out').allTextContents()).map(x=>x.trim()),expected.dom['.out']);
 const state=await page.evaluate(()=>jtml.state);
 for(const [id,value]of Object.entries({...expected.state,...expected.derived}))assert.deepEqual(state[id.split('::').pop()],value,id);
}
// These fixtures have one dynamic text projection per .out. Expected values
// come from authored vector snapshots or independent scalar arithmetic.
function assertPublications(writes,expected) {
 assert(writes.length,'DOM writes actually observed in this phase');
 assert.equal(expected.dom['.out'].length,1);
 for(const write of writes) {
  assert.equal(write.value,expected.dom['.out'][0],'DOM publication mismatch');
  for(const [id,value]of Object.entries({...expected.state,...expected.derived}))
   assert.deepEqual(write.state[id.split('::').pop()],value,'cache at DOM write: '+id);
 }
}
async function publication(page,expected) {
 await snapshot(page,expected);
 assertPublications(await page.evaluate(()=>publicationWrites.splice(0)),expected);
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
  await publication(page,v.expected_initial);
  for(const step of v.steps){await page.getByRole('button',{name:'Step',exact:true}).click();await publication(page,step.expected);}
  await page.close();report.checks.push(id+' authored initial/action/state/derived/DOM PASS');
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
   const derived={};for(let j=0;j<12;j++)derived['z'+j]=x+j+1;
   Object.assign(derived,{left:x+13,right:2*(x+12),total:3*(x+12)+1,final:4*(x+12)+1});
   await publication(page,{state:{x},derived,dom:{'.out':[`${4*(x+12)+1}|${x+13}|${2*(x+12)}`]}});
  }
  await page.close();report.checks.push('long chain / diamond permutation '+i+' PASS');
 }
 // Consumer module can sort before its defining imported getter's owner.
 compile({'index.jtml':'jtml 2\nuse {Card, z} from "./private.jtml"\nget a = z * 2\npage\n  box class "out"\n    text "{a}|{z}"\n  Card\n',
 'private.jtml':'jtml 2\nlet x = 1\nexport get z = x + 1\nwhen step\n  x = x + 1\nexport make Card\n  button "Step" click step\n'},'modules');
 const page=await open();for(let x=1;x<=4;x++){if(x>1)await page.getByRole('button',{name:'Step',exact:true}).click();await publication(page,{state:{x},derived:{a:2*(x+1),z:x+1},dom:{'.out':[`${2*(x+1)}|${x+1}`]}});}await page.close();
 report.checks.push('explicit imported getter ownership PASS');
 // Keep the existing structured collection-getter publication contract.
 compile({'index.jtml':'jtml 2\nlet x = 1\nget a = z.length\nget z = [x, x + 1]\nwhen step\n  x = x + 1\npage\n  box class "out"\n    text "{a}|{z[1]}"\n  button "Step" click step\n'},'collection');
 const collection=await open();for(let x=1;x<=3;x++){if(x>1)await collection.getByRole('button',{name:'Step',exact:true}).click();await publication(collection,{state:{x},derived:{a:2,z:[x,x+1]},dom:{'.out':[`2|${x+1}`]}});}await collection.close();report.checks.push('existing structured collection getter PASS');
 const storeFiles={};for(const name of ['index.jtml','private.jtml'])storeFiles[name]=fs.readFileSync(path.join(repo,'tests/fixtures/production/imported-store-getter',name),'utf8');
 compile(storeFiles,'imported-store');const store=await open();
 for(let x=1;x<=4;x++) {
  if(x>1)await store.getByRole('button',{name:'Step',exact:true}).click();
  await publication(store,{state:{shop:{x}},derived:{label:x+1},dom:{'.out':[`${x+1}|${x}`]}});
 }
 await store.close();report.checks.push('imported store: baseline 2|1 -> 3|2 -> 4|3 -> 5|4 PASS');
 const asset=fs.readFileSync(path.join(root,'jtml-runtime.js'));report.runtime={raw:asset.length,gzip:zlib.gzipSync(asset,{level:9}).length};
 assert(asset.length<=273000,'unchanged runtime budget');assert(!/\beval\s*\(|\bnew\s+Function\b/.test(asset.toString()));
 for(const flag of ['--compat','--debug']) {
  compile(vectors.find(v=>v.id==='V04').files,flag.slice(2),[flag]);const p=await open();
  await p.getByRole('button',{name:'Step',exact:true}).click();assert.equal((await p.locator('.out').textContent()).trim(),'4|3');await p.close();
  report.checks.push(flag+' unchanged PASS');
 }
 // The same observer/assertion must reject a transient bad write even when
 // final DOM and every cached getter are correct. No assertion or budget waiver.
 const vector=vectors.find(v=>v.id==='V04');compile(vector.files,'negative-control');
 const runtimePath=path.join(root,'jtml-runtime.js'),original=fs.readFileSync(runtimePath,'utf8');
 const needle='          if (result.found) el.textContent = renderTemplateValue(result.value);';
 const at=original.indexOf(needle,original.indexOf('function applyClientExpressions()'));
 assert(at>=0,'negative control must alter the real root text publication path');
 const mutant=original.slice(0,at)+'if(result.found)el.textContent=(el.textContent="X",result.value);'+original.slice(at+needle.length);
 assert(Buffer.byteLength(mutant)<=Buffer.byteLength(original),'negative control must not consume runtime headroom');
 fs.writeFileSync(runtimePath,mutant);let bad;
 try {
  bad=await open();await snapshot(bad,vector.expected_initial);
  const writes=await bad.evaluate(()=>publicationWrites.splice(0));
  for(const {state:s}of writes){assert.equal(s.a,4);assert.equal(s.z,2);}
  assert.throws(()=>assertPublications(writes,vector.expected_initial),/DOM publication mismatch/);
  assert(writes.some(w=>w.value==='X'),'negative publication really executed');
  report.checks.push('negative control: transient X rejected despite correct final DOM/cache PASS');
 } finally {fs.writeFileSync(runtimePath,original);if(bad)await bad.close();}
 assert.deepEqual(report.errors,[]);
})().catch(e=>{report.failure=String(e);console.error(e);process.exitCode=1;}).finally(async()=>{
 if(browser){await browser.close();report.cleanExit=true;}if(server)await new Promise(r=>server.close(r));
 fs.writeFileSync(path.join(out,'results.json'),JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(report,null,2));
});
