// Compiler-certified defining-module scalar captures, driven through real controls.
const assert = require('node:assert/strict');
const fs = require('node:fs'), path = require('node:path'), http = require('node:http');
const {spawnSync} = require('node:child_process'), {createRequire} = require('node:module');
const {gzipSync} = require('node:zlib');
const {chromium} = createRequire(path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT, 'package.json'))('playwright');
const repo = path.resolve(__dirname, '../..'), bin = process.env.JTML_BIN || path.join(repo, 'build/jtml');
const out = path.join(repo, 'dist/component-module-capture');
const vectors = require('../../docs/spec/reactive-core-v1/vectors.json').vectors;
let root, server, browser;
const report = {cases: [], errors: [], cleanExit: false};
function build(files, name, flags = [], destination) {
  const source = path.join(out, name + '-source');
  fs.mkdirSync(source, {recursive:true});
  for (const [file,text] of Object.entries(files)) fs.writeFileSync(path.join(source,file),text);
  root = destination || path.join(out,name);
  return spawnSync(bin,['build',path.join(source,'index.jtml'),'--target','browser','--out',root,...flags],{encoding:'utf8',timeout:60000});
}
function compile(files,name,flags) {
  const result = build(files,name,flags);
  assert.equal(result.status,0,result.stdout + result.stderr);
}
function instrument() {
  const events=[], ids=new WeakMap(); let sequence=0;
  const id = instance => {if(!ids.has(instance))ids.set(instance,++sequence);return ids.get(instance);};
  function intercept(name,attach) {
    let value;
    Object.defineProperty(window,name,{configurable:true,get:()=>value,set(next){value=next;attach(next);}});
  }
  intercept('__jtml_static_component_modules', table=>{
    for(const module of Object.values(table)) for(const kind of ['create','destroy']) {
      const original=module[kind];
      module[kind]=function(instance,...args){events.push({kind,generation:id(instance),instance:instance.id});return original.call(this,instance,...args);};
    }
  });
  intercept('__jtml_static_update_functions', table=>{
    for(const key of Object.keys(table)) {
      const original=table[key];
      table[key]=function(instance,definition,changed,...args){
        events.push({kind:'update',generation:id(instance),changed});
        return original.call(this,instance,definition,changed,...args);
      };
    }
  });
  window.captureObservation=()=>({events:events.splice(0),instances:jtml.getComponentInstances().map(i=>({id:i.id,generation:id(i)})),
    state:jtml.state,dom:[...document.querySelectorAll('.out')].map(e=>e.textContent.trim())});
}
async function open() {
  const page=await browser.newPage();
  page.setDefaultTimeout(4000);
  page.on('pageerror',e=>report.errors.push(e.message));
  page.on('console',m=>{if(m.type()==='error')report.errors.push(m.text());});
  await page.addInitScript(instrument);
  await page.goto('http://127.0.0.1:'+server.address().port+'/index.html');
  assert.equal(await page.evaluate(()=>jtml.browserBuild.mode),'prod');
  return page;
}
async function observe(page,dom,values={}) {
  const actual=await page.evaluate(()=>captureObservation());
  assert.deepEqual(actual.dom,dom);
  for(const [key,value] of Object.entries(values))assert.deepEqual(actual.state[key],value,key);
  return actual;
}
(async()=>{
  fs.mkdirSync(out,{recursive:true});
  server=http.createServer((q,r)=>{
    const file=path.resolve(root,'.'+new URL(q.url,'http://localhost').pathname);
    if(q.url==='/favicon.ico')return r.writeHead(204).end();
    if(!file.startsWith(root+path.sep)||!fs.existsSync(file))return r.writeHead(404).end();
    r.writeHead(200,{'Content-Security-Policy':"script-src 'self'",'Content-Type':file.endsWith('.js')?'text/javascript':'text/html'}).end(fs.readFileSync(file));
  });
  await new Promise((ok,no)=>server.once('error',no).listen(0,'127.0.0.1',ok));
  browser=await chromium.launch({headless:true,executablePath:process.env.JTML_BENCH_BROWSER});
  report.browser=browser.version();
  const v=vectors.find(v=>v.id==='V10');
  compile(v.files,'V10');
  const page=await open();
  const initial=await observe(page,v.expected_initial.dom['.out'],{tone:'private',label:'private!',entryTone:'entry'});
  for(let i=1;i<=3;i++) {
    await page.getByRole('button',{name:'Step',exact:true}).click();
    const phase=await observe(page,['private'+'!'.repeat(i+1)],{tone:'private'+'!'.repeat(i),label:'private'+'!'.repeat(i+1)});
    assert.deepEqual(phase.instances,initial.instances,'retained component identity');
    assert(phase.events.some(e=>e.kind==='update'&&e.changed.includes('label')),'capture invalidation reaches generated update');
  }
  await page.close();report.cases.push('unchanged V10 and repeated retained updates PASS');
  const isolation=vectors.find(v=>v.id==='V09');
  compile(isolation.files,'V09');
  const isolated=await open(), shown=[];
  isolated.on('console',m=>{if(m.text().startsWith('[SHOW] '))shown.push(m.text().slice(7));});
  await observe(isolated,isolation.expected_initial.dom['.out']);
  for(const button of ['Private','Entry','Read private','Read entry'])
    await isolated.getByRole('button',{name:button,exact:true}).click();
  assert.deepEqual(shown,['private!','entry!']);
  const independent=await isolated.evaluate(()=>Object.values(jtml.state).filter(v=>typeof v==='string').sort());
  assert.deepEqual(independent,['entry!','private!']);
  await isolated.close();report.cases.push('unchanged V09 isolated module state PASS');
  const owner = `jtml 2
let tone = "private"
get label = tone + "!"
when change
  tone = tone + "!"
export make Card
  let local = 0
  get combined = label + "|" + local
  when localStep
    local = local + 1
  box class "out"
    text combined
  button "Local" click localStep
  button "Step" click change
`;
  const lifecycle={'index.jtml':`jtml 2
use Card from "./private.jtml"
let visible = true
when toggle
  visible = !visible
page
  if visible
    Card
  Card
  button "Toggle" click toggle
`,'private.jtml':owner};
  compile(lifecycle,'lifecycle');
  const life=await open();
  const first=await observe(life,['private!|0','private!|0']);
  await life.getByRole('button',{name:'Local',exact:true}).first().click();
  await observe(life,['private!|1','private!|0']);
  await life.getByRole('button',{name:'Toggle',exact:true}).click();
  const removed=await observe(life,['private!|0']);
  const dead=first.instances.find(i=>!removed.instances.some(j=>j.generation===i.generation));
  assert(dead);
  assert(removed.events.some(e=>e.kind==='destroy'&&e.generation===dead.generation));
  await life.getByRole('button',{name:'Step',exact:true}).click();
  const updated=await observe(life,['private!!|0']);
  assert(!updated.events.some(e=>e.kind==='update'&&e.generation===dead.generation));
  await life.getByRole('button',{name:'Toggle',exact:true}).click();
  const remount=await observe(life,['private!!|0','private!!|0']);
  assert(!remount.instances.some(i=>i.generation===dead.generation));
  await life.close();report.cases.push('independent siblings, local getter, destroy and fresh remount PASS');

  for(const [name,component,invocation,dom] of [
    ['scalar','export make Card\n  box class "out"\n    text tone','Card',['private']],
    ['parameter-shadow','export make Card tone\n  box class "out"\n    text tone','Card "argument"',['argument']],
    ['local-shadow','export make Card\n  let tone = "local"\n  box class "out"\n    text tone','Card',['local']],
  ]) {
    compile({'index.jtml':'jtml 2\nuse Card from "./private.jtml"\npage\n  '+invocation+'\n',
      'private.jtml':'jtml 2\nlet tone = "private"\n'+component+'\n'},name);
    const control=await open();await observe(control,dom);await control.close();
  }
  report.cases.push('private scalar and lexical parameter/local shadowing PASS');
  compile({'index.jtml':`jtml 2
use Card from "./private.jtml"
make Host
  let rows = [{id: 1}, {id: 2}]
  when reverse
    rows = [rows[1], rows[0]]
  for row in rows key row.id
    Card
  button "Reverse" click reverse
page
  Host
`,'private.jtml':v.files['private.jtml']},'keyed');
  const keyed=await open();
  await observe(keyed,['private!','private!']);
  await keyed.evaluate(()=>window.retainedNodes=[...document.querySelectorAll('.out')]);
  await keyed.getByRole('button',{name:'Reverse',exact:true}).click();
  assert(await keyed.evaluate(()=>{const nodes=[...document.querySelectorAll('.out')];return nodes[0]===retainedNodes[1]&&nodes[1]===retainedNodes[0];}));
  await keyed.getByRole('button',{name:'Step',exact:true}).first().click();
  await observe(keyed,['private!!','private!!']);
  await keyed.close();report.cases.push('keyed nested capture retains and moves existing DOM PASS');

  compile({'index.jtml':`jtml 2
use Card from "./private.jtml"
use label from "./private.jtml"
page
  Card label
`,'private.jtml':`jtml 2
let tone = "private"
export get label = tone + "!"
when change
  tone = tone + "!"
export make Card value
  box class "out"
    text value
  button "Step" click change
`},'explicit-parameter');
  const parameter=await open();
  await observe(parameter,['private!']);
  await parameter.getByRole('button',{name:'Step',exact:true}).click();
  await observe(parameter,['private!!']);
  await parameter.close();report.cases.push('explicit caller parameter alternative PASS');

  const previous=root, assets=['index.html','jtml-runtime.js','components/index.js','app.js'];
  const saved=assets.map(file=>fs.readFileSync(path.join(previous,file)));
  for(const [name,privateSource,entryExtra] of [
    ['unresolved','export make Card\n  text missing',''],
    ['unrelated','export make Card\n  text entryOnly','let entryOnly = 9\n'],
    ['object-getter','get label = [{value: 2}]\nexport make Card\n  text label',''],
    ['member-getter','let record = {value: 2}\nget label = record.value\nexport make Card\n  text label',''],
    ['write','let tone = "private"\nexport make Card\n  when bad\n    tone = "foreign"\n  text tone\n  button "Bad" click bad',''],
    ['nested-shadow','let tone = "private"\nexport make Card\n  when bad\n    let tone = "nested"\n  text tone\n  button "Bad" click bad',''],
  ]) {
    const files={'index.jtml':'jtml 2\nuse Card from "./private.jtml"\n'+entryExtra+'page\n  Card\n','private.jtml':'jtml 2\n'+privateSource+'\n'};
    const denied=build(files,'denied-'+name);
    assert.notEqual(denied.status,0,name);
    assert.match(denied.stderr,/private\.jtml:\d+:\d+/);
    for(const file of assets)assert(!fs.existsSync(path.join(root,file)),file);
    assert.notEqual(build(files,'denied-'+name,[],previous).status,0);
    for(const [i,file] of assets.entries())assert.deepEqual(fs.readFileSync(path.join(previous,file)),saved[i]);
  }
  const collision={'index.jtml':'jtml 2\nuse Card from "./private.jtml"\nlet tone = "caller"\npage\n  Card\n',
    'private.jtml':'jtml 2\nlet tone = "private"\nexport make Card\n  text tone\n'};
  const collided=build(collision,'unrelated-same-spelling');
  assert.notEqual(collided.status,0);
  assert.match(collided.stderr,/binding collision/);
  assert.match(collided.stderr,/private\.jtml:2:1/);
  assert(!fs.existsSync(path.join(root,'index.html')));
  report.cases.push('unresolved/unrelated/object/write rejections preserve assets PASS');
  const runtime=fs.readFileSync(path.join(previous,'jtml-runtime.js'));
  report.runtime={raw:runtime.length,gzip:gzipSync(runtime,{level:9}).length};
  assert(runtime.length<=273000);
  assert.deepEqual(report.errors,[]);
})().catch(e=>{report.failure=String(e);process.exitCode=1;console.error(e);}).finally(async()=>{
  if(browser){await browser.close();report.cleanExit=true;}
  if(server)await new Promise(ok=>server.close(ok));
  fs.mkdirSync(out,{recursive:true});
  fs.writeFileSync(path.join(out,'results.json'),JSON.stringify(report,null,2)+'\n');
  console.log(JSON.stringify(report,null,2));
});
