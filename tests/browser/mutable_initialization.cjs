// Source bytes/oracles are read unchanged; no app-state mutation or model changes.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),http=require('node:http'),zlib=require('node:zlib');
const {spawnSync}=require('node:child_process'),{createRequire}=require('node:module');
assert(process.env.JTML_BENCH_BROWSER&&process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT,'real Chromium required; no skip');
const {chromium}=createRequire(path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT,'package.json'))('playwright');
const repo=path.resolve(__dirname,'../..'),out=path.join(repo,'dist/mutable-initialization'),bin=process.env.JTML_BIN||path.join(repo,'build/jtml');
const vectors=JSON.parse(fs.readFileSync(path.join(repo,'docs/spec/reactive-core-v1/vectors.json'))).vectors;
const report={cases:[],errors:[],cleanExit:false};let root,server,browser;
function build(files,id,flags=[]) {
 const src=path.join(out,id+'-source');fs.mkdirSync(src,{recursive:true});
 for(const [name,text]of Object.entries(files))fs.writeFileSync(path.join(src,name),text);
 root=path.join(out,id);return spawnSync(bin,['build',path.join(src,'index.jtml'),'--target','browser','--out',root,...flags],{cwd:repo,encoding:'utf8'});
}
function compile(files,id,flags=[]) {const r=build(files,id,flags);assert.equal(r.status,0,r.stdout+r.stderr);}
async function open() {
 const p=await browser.newPage();p.setDefaultTimeout(4000);p.on('pageerror',e=>report.errors.push(e.message));p.on('console',m=>{if(m.type()==='error')report.errors.push(m.text());});
 await p.goto(`http://127.0.0.1:${server.address().port}/index.html`);return p;
}
async function check(p,dom,values) {
 assert.deepEqual((await p.locator('.out').allTextContents()).map(s=>s.trim()),dom);
 const actual=await p.evaluate(()=>jtml.state);for(const [id,value]of Object.entries(values))assert.deepEqual(actual[id.split('::').pop()],value,id);
}
(async()=>{
 fs.mkdirSync(out,{recursive:true});server=http.createServer((q,r)=>{
  const f=path.resolve(root,'.'+new URL(q.url,'http://localhost').pathname);
  if(!f.startsWith(root+path.sep)||!fs.existsSync(f)||!fs.statSync(f).isFile()){r.writeHead(404).end();return;}
  r.setHeader('Content-Security-Policy',"script-src 'self'");r.setHeader('Content-Type',f.endsWith('.js')?'text/javascript':'text/html');r.end(fs.readFileSync(f));
 });await new Promise(r=>server.listen(0,'127.0.0.1',r));
 browser=await chromium.launch({executablePath:process.env.JTML_BENCH_BROWSER,headless:true});report.browser=browser.version();
 for(const id of ['V11','V01','V03','V04','V05']) {
  const v=vectors.find(v=>v.id===id);compile(v.files,id);const p=await open();
  await check(p,v.expected_initial.dom['.out'],{...v.expected_initial.state,...v.expected_initial.derived});
  for(const step of v.steps){await p.getByRole('button',{name:'Step',exact:true}).click();await check(p,step.expected.dom['.out'],{...step.expected.state,...step.expected.derived});}
  await p.close();report.cases.push(id+' original oracle PASS');
 }
 // Preserve a real successful four-asset build, not just an empty output dir.
 const savedRoot=path.join(out,'V11'),assetNames=['index.html','jtml-runtime.js','components/index.js','app.js'];
 const saved=assetNames.map(name=>fs.readFileSync(path.join(savedRoot,name)));
 const invalid=vectors.find(v=>v.id==='V12');const failed=build(invalid.files,'V11');
 assert.notEqual(failed.status,0);assert(failed.stderr.includes(invalid.expected_initial.diagnostic));
 for(let i=0;i<assetNames.length;i++)assert(fs.readFileSync(path.join(savedRoot,assetNames[i])).equals(saved[i]),assetNames[i]);
 const retained=await open();await check(retained,['3|5'],{a:3,z:2,result:5});await retained.close();report.cases.push('real successful assets preserved and still executable PASS');
 const v=vectors.find(v=>v.id==='V12');
 const rejects=[['V12',v.files,v.expected_initial.diagnostic],
  ['self',{'index.jtml':'jtml 2\nlet x = x + 1\npage\n  text x\n'},'E_INIT_FORWARD'],
  ['derived',{'index.jtml':'jtml 2\nget before = 2\nlet x = before + 1\npage\n  text x\n'},'E_INIT_DERIVED'],
  ['call',{'index.jtml':'jtml 2\nlet x = mystery()\npage\n  text x\n'},'E_INIT_UNSUPPORTED'],
  ['unresolved',{'index.jtml':'jtml 2\nlet x = missing + 1\npage\n  text x\n'},'E_INIT_UNRESOLVED']];
 for(const [id,files,code]of rejects) {
  const dest=path.join(out,id);fs.mkdirSync(dest,{recursive:true});fs.writeFileSync(path.join(dest,'index.html'),'previous output');fs.writeFileSync(path.join(dest,'app.js'),'previous app');
  const r=build(files,id);assert.notEqual(r.status,0,r.stdout+r.stderr);assert(r.stderr.includes(code),r.stderr);assert.match(r.stderr,/index\.jtml:\d+:\d+/);
  assert.equal(fs.readFileSync(path.join(dest,'index.html'),'utf8'),'previous output');assert.equal(fs.readFileSync(path.join(dest,'app.js'),'utf8'),'previous app');assert(!fs.existsSync(path.join(dest,'jtml-runtime.js')));
  report.cases.push(id+' rejection/preservation PASS');
 }
 for(const [i,order]of [['z','y','a'],['y','z','a']].entries()) {
  const defs={z:'let z = 2',y:'let y = 3',a:'let a = z + y'};
  compile({'index.jtml':'jtml 2\n'+order.map(n=>defs[n]).join('\n')+'\nget total = a + y + z\npage\n  box class "out"\n    text "{a}|{total}"\n'},'source-permutation-'+i);
  const p=await open();await check(p,['5|10'],{a:5,total:10,z:2,y:3});await p.close();
 }
 for(const reverse of [false,true]) {
  const names=Array.from({length:24},(_,i)=>'v'+String(reverse?23-i:i).padStart(2,'0'));
  const source='jtml 2\n'+names.map((n,i)=>`let ${n} = ${i?names[i-1]+' + 1':'2'}`).join('\n')+`\nget total = ${names[23]} + ${names[0]}\nwhen step\n  ${names[0]} = ${names[0]} + 1\npage\n  box class "out"\n    text "{${names[23]}}|{total}"\n  button "Step" click step\n`;
  compile({'index.jtml':source},'chain-'+reverse);const p=await open(),values=Object.fromEntries(names.map((n,i)=>[n,i+2]));
  await check(p,['25|27'],{...values,total:27});await p.getByRole('button',{name:'Step',exact:true}).click();await check(p,['25|28'],{...values,[names[0]]:3,total:28});await p.close();
 }
 report.cases.push('permutations/long chains/one-time mutable initialization PASS');
 // Foreign initializer dependencies retain the existing contract, not V21 scheduling.
 for(const [id,initial,decl,dom]of [
  ['import-let','export let aaSeed = 7','let zzCopy = aaSeed + 1','8'],
  ['import-store','export store shop\n  let x = 7','let zzCopy = shop.x + 1','8']]) {
  const symbol=id==='import-let'?'aaSeed':'shop';
  compile({'index.jtml':`jtml 2\nuse ${symbol} from "./private.jtml"\n${decl}\npage\n  box class "out"\n    text zzCopy\n`,'private.jtml':'jtml 2\n'+initial+'\n'},id);
  const p=await open();await check(p,[dom],{zzCopy:8});await p.close();report.cases.push(id+' existing foreign-owner initializer PASS');
 }
 compile({'index.jtml':'jtml 2\nuse "./private.jtml"\nlet zzCopy = aaSeed + 1\npage\n  box class "out"\n    text zzCopy\n','private.jtml':'jtml 2\nlet aaSeed = 7\n'},'side-effect-import');
 const sideEffect=await open();await check(sideEffect,['8'],{zzCopy:8});await sideEffect.close();report.cases.push('legacy whole-file import initializer preserved PASS');
 compile({'index.jtml':'jtml 2\nuse shop from "./barrel.jtml"\nlet zzCopy = shop.items[0].x + 1\npage\n  box class "out"\n    text zzCopy\n','barrel.jtml':'jtml 2\nexport use shop from "./private.jtml"\n','private.jtml':'jtml 2\nexport store shop\n  let items = [{x: 7}]\n'},'barrel-store');
 const barrel=await open();await check(barrel,['8'],{zzCopy:8});assert.equal(await barrel.evaluate(()=>JSON.parse(document.getElementById('__jtml_client_manifest').textContent).stateOrder),null);await barrel.close();report.cases.push('barrel store structured initializer/null boundary preserved PASS');

 for(const flag of ['--compat','--debug']) {
  const source={'index.jtml':'jtml 2\nlet z = 2\nlet a = z + 1\nget result = a + z\npage\n  box class "out"\n    text "{a}|{result}"\n'};
  compile(source,'mode-'+flag.slice(2),[flag]);const p=await open();assert.deepEqual(await p.evaluate(()=>({z:jtml.state.z,a:jtml.state.a})),{z:2,a:undefined});await p.close();
  const r=build(v.files,'forward-'+flag.slice(2),[flag]);assert.equal(r.status,0,r.stderr);report.cases.push(flag+' legacy initialization unchanged');
 }
 const asset=fs.readFileSync(path.join(root,'jtml-runtime.js'));report.runtime={raw:asset.length,gzip:zlib.gzipSync(asset,{level:9}).length};assert(asset.length<=273000);assert(!/\beval\s*\(|\bnew\s+Function\b/.test(asset.toString()));assert.deepEqual(report.errors,[]);
})().catch(e=>{report.failure=String(e);console.error(e);process.exitCode=1;}).finally(async()=>{
 if(browser){await browser.close();report.cleanExit=true;}if(server)await new Promise(r=>server.close(r));fs.writeFileSync(path.join(out,'results.json'),JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(report,null,2));
});
