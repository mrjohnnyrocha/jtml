// Real UI dispatch of original sources; no app-state writes or callback injection.
const assert = require('node:assert/strict');
const fs = require('node:fs'), path = require('node:path'), http = require('node:http');
const {spawnSync} = require('node:child_process'), {createRequire} = require('node:module');
const zlib = require('node:zlib');
assert(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT && process.env.JTML_BENCH_BROWSER, 'Chromium required; no skip');
const {chromium} = createRequire(path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT, 'package.json'))('playwright');
const repo = path.resolve(__dirname, '../..'), out = path.join(repo, 'dist/action-get-reads');
const bin = process.env.JTML_BIN || path.join(repo, 'build/jtml');
const vectors = JSON.parse(fs.readFileSync(path.join(repo, 'docs/spec/reactive-core-v1/vectors.json'))).vectors;
const report = {cases: [], errors: [], cleanExit: false};
let root, server, browser;
function build(source, target, flags = []) {
  return spawnSync(bin, ['build', source, '--target', 'browser', '--out', target, ...flags], {cwd: repo, encoding:'utf8', timeout:60000});
}
function compile(source, name, flags = []) {
  root = path.join(out, name); fs.mkdirSync(root, {recursive:true});
  const result = build(source, root, flags);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  return root;
}
function authored(id) {
  const v = vectors.find(v => v.id === id), dir = path.join(out, 'source-' + id);
  fs.mkdirSync(dir, {recursive:true});
  for (const [file, text] of Object.entries(v.files)) fs.writeFileSync(path.join(dir, file), text);
  return {v, source:path.join(dir, v.entry)};
}
async function open() {
  const page = await browser.newPage();
  page.setDefaultTimeout(4000);
  const shown = [];
  page.on('pageerror', e => report.errors.push(e.message));
  page.on('console', m => {if (m.type()==='error') report.errors.push(m.text()); if (m.text().startsWith('[SHOW] ')) shown.push(m.text().slice(7));});
  await page.goto(`http://127.0.0.1:${server.address().port}/index.html`);
  return {page, shown};
}
async function view(page) {return (await page.locator('.out').allTextContents()).map(s => s.trim());}
async function expectView(page, values) {assert.deepEqual(await view(page), values);}
(async () => {
  fs.mkdirSync(out, {recursive:true});
  server = http.createServer((req,res) => {
    const file = path.resolve(root, '.' + new URL(req.url,'http://localhost').pathname);
    res.setHeader('Content-Security-Policy', "script-src 'self'");
    if (!file.startsWith(root + path.sep) || !fs.existsSync(file) || !fs.statSync(file).isFile()) {res.writeHead(404).end();return;}
    res.setHeader('Content-Type',file.endsWith('.js')?'text/javascript':'text/html');res.end(fs.readFileSync(file));
  });
  await new Promise(r => server.listen(0,'127.0.0.1',r));
  browser = await chromium.launch({headless:true,executablePath:process.env.JTML_BENCH_BROWSER});
  for (const id of ['V03','V01']) {
    const {v,source} = authored(id); compile(source,id);
    const {page} = await open();
    await expectView(page,v.expected_initial.dom['.out']);
    for (const s of v.steps) {await page.getByRole('button',{name:'Step',exact:true}).click();await expectView(page,s.expected.dom['.out']);}
    if (id==='V03') for(let i=0;i<4;i++) {await page.getByRole('button',{name:'Step',exact:true}).click();await expectView(page,['2|2']);}
    report.cases.push({id,status:'PASS',dom:await view(page)}); await page.close();
  }
  const asset = fs.readFileSync(path.join(root,'jtml-runtime.js'));
  report.runtime = {raw:asset.length,gzip:zlib.gzipSync(asset,{level:9}).length};
  assert(asset.length<=273000,'unchanged shared-runtime budget');
  assert(!/\bnew\s+Function\b|\beval\s*\(/.test(asset.toString()));
  for (const flag of ['--compat','--debug']) {
    const {source} = authored('V03');compile(source,flag.slice(2),[flag]);
    const {page}=await open();await page.getByRole('button',{name:'Step',exact:true}).click();
    await expectView(page,['0|2']);report.cases.push({id:flag,status:'UNCHANGED',dom:await view(page)});await page.close();
  }
  compile(path.join(repo,'tests/fixtures/production/action-get-reads/index.jtml'),'repeated');
  const {page,shown}=await open();
  for(let i=1;i<=4;i++) {
    await page.getByRole('button',{name:'Batch',exact:true}).click();
    await expectView(page,[`${2*i}|${4*i-2}|${4*i}|${4*i}|true`]);
    assert.deepEqual(shown.splice(0),[String(4*i+1),'active']);
  }
  await page.getByRole('button',{name:'Shadow',exact:true}).click();
  await expectView(page,['8|16|16|16|true']);assert.deepEqual(shown.splice(0),['101','19']);
  await page.getByRole('button',{name:'Shadow get',exact:true}).click();
  await expectView(page,['8|19|300|16|true']);
  report.cases.push({id:'two-writes-repeated-conditions-show-parameter-shadowing',status:'PASS'});await page.close();
  const chainSource=path.join(out,'long-chain.jtml');
  let chain='jtml 2\nlet x = 0\nlet saved = 0\n';
  for(let i=0;i<9;i++) chain+=`get a${i} = ${i===8?'x':`a${i+1}`} + 1\n`;
  chain+='when step\n  x = x + 1\n  saved = a0\npage\n  box class "out"\n    text "saved={saved}"\n  button "Step" click step\n';
  fs.writeFileSync(chainSource,chain);compile(chainSource,'long-chain');
  const chainPage=await open();
  for(let i=1;i<=3;i++){await chainPage.page.getByRole('button',{name:'Step',exact:true}).click();await expectView(chainPage.page,[`saved=${i+9}`]);}
  report.cases.push({id:'recursive-action-read-with-missing-global-get-caches',status:'PASS'});await chainPage.page.close();
  const indexedSource=path.join(out,'indexed-read.jtml');
  fs.writeFileSync(indexedSource,'jtml 2\nlet values = [0, 0, 0]\nlet index = 0\nget next = index + 1\nwhen step\n  index = 1\n  values[next] = 9\npage\n  box class "out"\n    text "value={values[2]}"\n  button "Step" click step\n');
  compile(indexedSource,'indexed-read');const indexedPage=await open();
  await indexedPage.page.getByRole('button',{name:'Step',exact:true}).click();await expectView(indexedPage.page,['value=9']);
  report.cases.push({id:'scalar-get-in-computed-write-index',status:'PASS'});await indexedPage.page.close();
  const imported = path.join(out,'imported-source');fs.mkdirSync(imported,{recursive:true});
  fs.writeFileSync(path.join(imported,'private.jtml'),'jtml 2\nlet seed = 4\nexport get value = seed * 2\nexport make Card\n  text "Card"\n');
  const entry='jtml 2\nuse {Card, value} from "./private.jtml"\nlet saved = 0\nwhen step seed\n  saved = value\n  show seed\npage\n  box class "out"\n    text "saved={saved}"\n  button "Step" click step(100)\n  Card\n';
  fs.writeFileSync(path.join(imported,'index.jtml'),entry);compile(path.join(imported,'index.jtml'),'imported');
  const importedPage=await open();await importedPage.page.getByRole('button',{name:'Step',exact:true}).click();
  await expectView(importedPage.page,['saved=8']);assert.deepEqual(importedPage.shown,['100']);
  report.cases.push({id:'imported-get-defining-module-parameter-isolation',status:'PASS'});await importedPage.page.close();
  fs.writeFileSync(path.join(imported,'index.jtml'),entry.replace('use {Card, value}','use Card'));
  const denied=build(path.join(imported,'index.jtml'),path.join(out,'unimported'));
  assert.notEqual(denied.status,0);assert.match(denied.stderr,/unimported get `value`/);
  assert(!fs.existsSync(path.join(out,'unimported/index.html')));
  report.cases.push({id:'unimported-defining-module-get',status:'REJECTED'});
  fs.writeFileSync(path.join(imported,'private.jtml'),'jtml 2\nlet seed = 4\nget Card = seed * 2\nexport make Card\n  text "Card"\n');
  fs.writeFileSync(path.join(imported,'index.jtml'),entry.replace('use {Card, value}','use Card').replace('saved = value','saved = Card'));
  const wrongKind=build(path.join(imported,'index.jtml'),path.join(out,'component-not-value'));
  assert.notEqual(wrongKind.status,0);assert.match(wrongKind.stderr,/unimported get `Card`/);
  report.cases.push({id:'component-import-does-not-authorize-private-get',status:'REJECTED'});
  for (const [name,decl] of [
    ['call','get value = mystery()\n'], ['cycle','get value = other + 1\nget other = value + 1\n'],
    ['missing','get value = missing\n'], ['collection','get value = [1]\n'],
    ['reserved','get __proto__ = 1\nget value = __proto__ + 1\n'],
    ['ambiguous','let value = 1\nget value = 2\n'],
    ['local-shadow','get value = 1\n']]) {
    const dir=path.join(out,'reject-'+name);fs.mkdirSync(dir,{recursive:true});
    const source=path.join(dir,'index.jtml');fs.writeFileSync(source,'jtml 2\nlet saved = 0\n'+decl+'when step\n'+(name==='local-shadow'?'  let value = 7\n':'')+'  saved = value\npage\n  button "Step" click step\n');
    const dest=path.join(dir,'dist');fs.mkdirSync(dest,{recursive:true});fs.writeFileSync(path.join(dest,'index.html'),'previous output');
    const r=build(source,dest);assert.notEqual(r.status,0,name);assert.match(r.stderr,/index\.jtml:\d+:\d+/);
    assert.equal(fs.readFileSync(path.join(dest,'index.html'),'utf8'),'previous output');assert(!fs.existsSync(path.join(dest,'app.js')));
    report.cases.push({id:name,status:'REJECTED',diagnostic:r.stderr});
  }
  // A same-spelled parameter/local slot has invocation ownership, not module ownership.
  for (const kind of ['let','get']) {
    const sourceDir=path.join(out,'parameter-local-'+kind);fs.mkdirSync(sourceDir,{recursive:true});
    const source=path.join(sourceDir,'index.jtml');
    const entry='jtml 2\nuse Card from "./private.jtml"\nlet count = 0\nget twice = count * 2\nwhen step scratch\n  let scratch = 7\n  count = count + 1\n  show twice\n  show scratch\npage\n  box class "out"\n    text "{twice}"\n  button "Step" click step(100)\n  Card\n';
    fs.writeFileSync(source,entry);
    fs.writeFileSync(path.join(sourceDir,'private.jtml'),`jtml 2\n${kind} scratch = 9\nwhen inspectPrivate\n  show scratch\nexport make Card\n  button "Inspect" click inspectPrivate\n`);
    compile(source,'parameter-local-dist-'+kind);const localPage=await open();
    for(let i=1;i<=2;i++) {
      await localPage.page.getByRole('button',{name:'Step',exact:true}).click();
      await expectView(localPage.page,[String(i*2)]);
      await localPage.page.getByRole('button',{name:'Inspect',exact:true}).click();
      assert.deepEqual(localPage.shown.splice(0),[String(i*2),'7','9']);
    }
    await localPage.page.close();report.cases.push({id:'parameter-local-with-other-module-'+kind,status:'PASS'});
    // Without a parameter frame, the local really overwrites shared root storage.
    fs.writeFileSync(source,entry.replace('when step scratch','when step'));
    const denied=build(source,path.join(out,'unprotected-local-'+kind));
    assert.notEqual(denied.status,0);assert.match(denied.stderr,/(local declaration shadows root storage|unimported get) `scratch`/);
    // Distinct root value owners with different kinds remain outside scalar isolation.
    fs.writeFileSync(source,entry.replace('let count = 0','let count = 0\n'+(kind==='let'?'get':'let')+' scratch = 1'));
    const collision=build(source,path.join(out,'mixed-root-'+kind));
    assert.notEqual(collision.status,0);assert.match(collision.stderr,/browser runtime binding collision `scratch`/);
  }
  const independentDir=path.join(out,'independent-local');fs.mkdirSync(independentDir,{recursive:true});
  fs.writeFileSync(path.join(independentDir,'private.jtml'),'jtml 2\nwhen scratch\n  show 9\nexport make Card\n  text "Card"\n');
  fs.writeFileSync(path.join(independentDir,'index.jtml'),'jtml 2\nuse Card from "./private.jtml"\nlet count = 1\nget twice = count * 2\nwhen step\n  let scratch = 7\n  show twice\n  show scratch\npage\n  Card\n  button "Step" click step\n');
  compile(path.join(independentDir,'index.jtml'),'independent-local-dist');const independentPage=await open();
  await independentPage.page.getByRole('button',{name:'Step',exact:true}).click();
  assert.deepEqual(independentPage.shown,['2','7']);await independentPage.page.close();
  report.cases.push({id:'unrelated-action-namespace-and-local-outside-get-closure',status:'PASS'});
  const admission = path.join(out,'root-admission-source');fs.mkdirSync(admission,{recursive:true});
  const admissionSource = path.join(admission,'index.jtml');
  fs.writeFileSync(path.join(admission,'values.jtml'),'jtml 2\nexport let allowed = 7\nexport let privateValue = 11\n');
  const admissionEntry = 'jtml 2\nuse allowed from "./values.jtml"\nlet saved = 0\nlet rows = [{value: 2}]\nwhen step index\n  let index = index + 0\n  let base = allowed + rows[index].value\n  if base > 0\n    let nested = base + index\n    rows[index].value = nested\n    show nested\n    saved = nested\npage\n  box class "out"\n    text "{saved}|{rows[0].value}"\n  button "Step" click step(0)\n';
  fs.writeFileSync(admissionSource,admissionEntry);
  compile(admissionSource,'root-admission-positive');
  const admitted = await open();
  await expectView(admitted.page,['0|2']);
  await admitted.page.getByRole('button',{name:'Step',exact:true}).click();
  await expectView(admitted.page,['9|9']);
  assert.deepEqual(admitted.shown,['9']);
  await admitted.page.close();
  const wholeFile = admissionEntry.replace('use allowed from','use');
  fs.writeFileSync(admissionSource,wholeFile);
  compile(admissionSource,'root-admission-whole-file');
  const wholeFilePage = await open();
  await wholeFilePage.page.getByRole('button',{name:'Step',exact:true}).click();
  await expectView(wholeFilePage.page,['9|9']);
  await wholeFilePage.page.close();

  const negativeReads = [
    ['unimported','  saved = privateValue','privateValue'],
    ['before-local','  saved = late\n  let late = 2','late'],
    ['self-local','  let late = late + 1','late'],
    ['branch-local','  if true\n    let branch = 2\n  saved = branch','branch'],
    ['other-branch','  if true\n    let branch = 2\n  else\n    show branch','branch'],
    ['dead-branch','  if false\n    show missing','missing'],
    ['dead-operand','  saved = false && missing','missing'],
    ['condition','  if missing\n    saved = 1','missing'],
    ['index','  rows[missing].value = 1','missing'],
    ['member','  saved = missing.value','missing'],
    ['storage-key','  show __jtml_scalar_00000000000000000000','__jtml_scalar_00000000000000000000']
  ];
  for (const [id,body,missing] of negativeReads) {
    fs.writeFileSync(admissionSource,admissionEntry.replace(/  let base[\s\S]*?(?=page\n)/,body+'\n'));
    const dest=path.join(out,'root-admission-'+id);
    const denied=build(admissionSource,dest);
    assert.notEqual(denied.status,0,id);
    assert.match(denied.stderr,/index\.jtml:\d+:\d+: E_UNRESOLVED/);
    assert(denied.stderr.includes('`'+missing+'`'),denied.stderr);
    assert(!fs.existsSync(path.join(dest,'index.html')),id);
  }
  report.cases.push({id:'root-action-lexical-admission',status:'PASS'});
  // Compare original V04/V05 oracles; historical browser observations stay frozen.
  if (process.argv.includes('--document-gaps')) for(const id of ['V04','V05']) {
    const {v,source}=authored(id);compile(source,id);const {page}=await open();
    await page.getByRole('button',{name:'Step',exact:true}).click();
    const actual=await view(page),expected=v.steps[0].expected.dom['.out'];
    const matches = JSON.stringify(actual) === JSON.stringify(expected);
    report.cases.push({id,status:matches?'PASS':'FAIL / KNOWN GLOBAL ORDERING GAP',actual,expected});
    if (!matches) process.exitCode=1;
    await page.close();
  }
  assert.deepEqual(report.errors,[]);
})().catch(e=>{report.failure=String(e);console.error(e);process.exitCode=1;}).finally(async()=>{
  if(browser){await browser.close();report.cleanExit=true;}if(server)await new Promise(r=>server.close(r));
  fs.writeFileSync(path.join(out,'results.json'),JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(report,null,2));
});
