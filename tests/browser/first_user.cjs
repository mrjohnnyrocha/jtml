// Exercise the actual public guide and generated modular starter through the CLI.
const assert = require('node:assert/strict');
const fs = require('node:fs'), path = require('node:path'), http = require('node:http');
const crypto = require('node:crypto');
const {spawnSync} = require('node:child_process');
const {createRequire} = require('node:module');
const repo = path.resolve(__dirname, '../..');
const compiler = process.env.JTML_BIN || path.join(repo, 'build/jtml');
assert(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT && process.env.JTML_BENCH_BROWSER,
  'real Chromium and Playwright required; no skip');
const {chromium} = createRequire(path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT, 'package.json'))('playwright');
const out = path.resolve(process.env.JTML_FIRST_USER_OUT || path.join(repo, 'dist/first-user'));
fs.mkdirSync(out, {recursive:true});
const work = fs.mkdtempSync(path.join(out, 'attempt-'));
const report = {compilerSHA256:crypto.createHash('sha256').update(fs.readFileSync(compiler)).digest('hex'), commands:[], checks:[]};
function command(args) {
  const r = spawnSync(compiler, args, {cwd:work, encoding:'utf8', timeout:60000});
  report.commands.push({args, exit:r.status, stdout:r.stdout, stderr:r.stderr});
  assert.equal(r.status, 0, `${args.join(' ')}\n${r.stdout}\n${r.stderr}`);
  return r.stdout;
}
function documentedSource(file, dest) {
  const text = fs.readFileSync(path.join(repo, file), 'utf8').match(/```jtml\n([\s\S]*?)```/);
  assert(text, `Missing complete source in ${file}`);
  fs.writeFileSync(path.join(work, dest), text[1]);
  command(['check', dest]); command(['lint', dest]);
}
let browser, server;
(async () => {
  report.version = command(['--version']).trim();
  documentedSource('docs/getting-started.md', 'coffee.jtml');
  documentedSource('docs/reference/language-reference.md', 'reference.jtml');
  command(['new', 'app', 'modular']);
  assert.match(fs.readFileSync(path.join(work, 'modular/components/card.jtml'), 'utf8'), /^export make Card title$/m);
  assert.match(fs.readFileSync(path.join(work, 'modular/index.jtml'), 'utf8'), /use Card from "\.\/components\/card.jtml"/);
  command(['check', 'modular/index.jtml']);
  command(['lint', 'modular/index.jtml']);
  for (const [source, name] of [['coffee.jtml','coffee'], ['modular','modular']]) {
    command(['build', source, '--target', 'browser', '--out', `output/${name}`]);
    assert.deepEqual(fs.readdirSync(path.join(work, 'output', name)).sort(), ['app.js','components','index.html','jtml-runtime.js']);
    for (const file of ['index.html','jtml-runtime.js','components/index.js','app.js']) {
      const generated = fs.readFileSync(path.join(work, 'output', name, file), 'utf8');
      assert(!/\beval\s*\(|\bnew\s+Function\b/.test(generated), file);
      assert(!generated.includes('jtml-update-plans.js') && !generated.includes('friendly+import-stubs'), file);
    }
  }
  const root = path.join(work, 'output');
  server = http.createServer((req,res) => {
    const pathname = new URL(req.url,'http://localhost').pathname;
    const f = path.resolve(root, '.'+pathname+(pathname.endsWith('/')?'index.html':''));
    if (!f.startsWith(root+path.sep) || !fs.existsSync(f) || !fs.statSync(f).isFile()) { res.writeHead(404).end(); return; }
    res.setHeader('Content-Type', f.endsWith('.js')?'text/javascript':'text/html');
    res.setHeader('Content-Security-Policy', "script-src 'self'");
    res.end(fs.readFileSync(f));
  });
  await new Promise(r => server.listen(0,'127.0.0.1',r));
  browser = await chromium.launch({headless:true, executablePath:process.env.JTML_BENCH_BROWSER});
  report.browser = browser.version();
  const context = await browser.newContext();
  await context.tracing.start({screenshots:true,snapshots:true});
  const page = await context.newPage(), errors = [], requests = [], sockets = [];
  page.setDefaultTimeout(7000);
  page.on('pageerror', e => errors.push(String(e)));
  page.on('console', m => {if(m.type()==='error')errors.push(m.text());});
  page.on('request', r => requests.push(r.url()));
  page.on('websocket', s => sockets.push(s.url()));
  const origin = `http://127.0.0.1:${server.address().port}`;
  const text = t => page.getByText(t,{exact:true}).waitFor();
  const click = name => page.getByRole('button',{name,exact:true}).click();
  try {
    await page.goto(origin+'/modular/');
    await text('Count: 0'); await click('Add one'); await text('Count: 1');
    assert(await page.evaluate(() => window.jtml.browserBuild.cleanProduction));
    await page.getByRole('heading',{name:'Interactive state',exact:true}).waitFor();
    await page.getByRole('heading',{name:'Next steps',exact:true}).waitFor();
    await page.screenshot({path:path.join(work,'modular.png')});
    report.checks.push('generated modular app: import, build, slot content and interactive count');
    await page.goto(origin+'/coffee/');
    await text('No order yet'); await text('Cups: 1'); await text('Total: EUR 3');
    const input = page.getByRole('textbox',{name:'Your name',exact:true});
    assert.equal(await input.inputValue(),'');
    await click('Place order'); await text('Enter your name');
    await input.fill('Ada'); assert(await input.evaluate(e => document.activeElement===e));
    await input.press('Enter'); await text('Order for Ada: 1 cups, EUR 3');
    assert.equal(page.url(),origin+'/coffee/');
    await click('Add cup'); await text('Total: EUR 6'); await text('Order for Ada: 1 cups, EUR 3');
    await click('Place order'); await text('Order for Ada: 2 cups, EUR 6');
    await click('Clear'); await text('No order yet'); await text('Cups: 1'); await text('Total: EUR 3');
    assert.equal(await input.inputValue(),'','Clear must clear the visible input');
    await input.fill('Reload'); await click('Add cup'); await page.reload();
    await text('No order yet'); await text('Cups: 1'); assert.equal(await input.inputValue(),'');
    assert(await page.evaluate(() => window.jtml.browserBuild.cleanProduction));
    assert.deepEqual(errors,[]); assert.deepEqual(sockets,[]);
    assert(requests.every(u => u.startsWith(origin+'/')) && !requests.some(u => u.includes('/api/')));
    report.checks.push('guide form: validation, focus, Enter, derived total, saved order, full clear and reload');
    await page.screenshot({path:path.join(work,'coffee.png')});
  } finally {
    await context.tracing.stop({path:path.join(work,'browser.zip')});
    await context.close();
  }
  report.pass = true;
  console.log('PASS first user: published guide and reference snippets; generated modular app; real production browser behavior');
})().catch(e => {report.pass=false;report.error=String(e);console.error(e);process.exitCode=1;})
  .finally(async () => {
    if(browser)await browser.close();
    if(server)await new Promise(r => server.close(r));
    fs.writeFileSync(path.join(work,'result.json'),JSON.stringify(report,null,2)+'\n');
  });
