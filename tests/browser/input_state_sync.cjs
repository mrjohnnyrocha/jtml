// Real-browser regression: only UI actions change application state.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const http = require('node:http');
const crypto = require('node:crypto');
const {spawnSync} = require('node:child_process');
const {createRequire} = require('node:module');
const repo = path.resolve(__dirname, '../..');
const compiler = process.env.JTML_BIN || path.join(repo, 'build/jtml');
const out = path.resolve(process.env.JTML_INPUT_SYNC_OUT || path.join(repo, 'dist/input-state-sync'));
const driver = process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT;
assert(driver && process.env.JTML_BENCH_BROWSER, 'real Chromium required; no skip');
const {chromium} = createRequire(path.join(driver, 'package.json'))('playwright');
const modes = process.env.JTML_INPUT_SYNC_MODES ? process.env.JTML_INPUT_SYNC_MODES.split(',') : ['clean', 'compat', 'debug'];
assert(modes.every(mode => ['clean', 'compat', 'debug'].includes(mode)));
fs.mkdirSync(out, {recursive: true});
const digest = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const result = {
  compiler: {path: compiler, sha256: digest(compiler), version: spawnSync(compiler, ['--version'], {encoding:'utf8'}).stdout.trim()},
  node: process.version, platform: `${process.platform}/${process.arch}`, checks: []
};
for (const mode of modes) for (const fixture of ['minimal', 'index']) {
  const source = path.join(repo, 'tests/fixtures/production/input-state-sync', `${fixture}.jtml`);
  const dest = path.join(out, mode, fixture);
  const build = spawnSync(compiler, ['build', source, '--target', 'browser', '--out', dest, ...(mode === 'clean' ? [] : [`--${mode}`])], {encoding:'utf8', timeout:60000});
  fs.mkdirSync(dest, {recursive: true});
  fs.copyFileSync(source, path.join(dest, 'source.jtml'));
  fs.writeFileSync(path.join(dest, 'build.log'), (build.stdout || '') + (build.stderr || ''));
  assert.equal(build.status, 0, build.stdout + build.stderr);
}
const server = http.createServer((req, res) => {
  const pathname = new URL(req.url, 'http://localhost').pathname;
  const file = path.resolve(out, '.' + pathname + (pathname.endsWith('/') ? 'index.html' : ''));
  if (!file.startsWith(out + path.sep) || !fs.existsSync(file)) { res.writeHead(404).end(); return; }
  if (pathname.startsWith('/clean/')) res.setHeader('Content-Security-Policy', "script-src 'self'");
  res.setHeader('Content-Type', file.endsWith('.js') ? 'text/javascript' : 'text/html');
  res.end(fs.readFileSync(file));
});
let browser;
(async () => {
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  browser = await chromium.launch({headless:true, executablePath:process.env.JTML_BENCH_BROWSER});
  result.browser = browser.version();
  const cases = {
    'minimal-reset': {fixture:'minimal', run: async page => {
      const input = page.getByRole('textbox', {name:'Draft', exact:true});
      await input.fill('Before reset');
      await page.getByText('Draft: [Before reset]', {exact:true}).waitFor();
      await page.getByRole('button', {name:'Clear', exact:true}).click();
      await page.getByText('Draft: []', {exact:true}).waitFor();
      assert.equal(await input.inputValue(), '', 'state echo cleared: input must also clear');
    }},
    'programmatic-and-retained-inputs': {fixture:'index', run: async page => {
      const first = page.getByRole('region', {name:'First editor', exact:true});
      const second = page.getByRole('region', {name:'Second editor', exact:true});
      const input = first.getByRole('textbox', {name:'Name', exact:true});
      const mirror = first.getByRole('textbox', {name:'Mirror', exact:true});
      const secondInput = second.getByRole('textbox', {name:'Name', exact:true});
      const identity = await input.elementHandle();
      const check = async value => {
        assert.equal(await input.inputValue(), value);
        assert.equal(await mirror.inputValue(), value);
        await first.getByText(`Name: [${value}]`, {exact:true}).waitFor();
        assert.equal(await secondInput.inputValue(), '', 'sibling component state is independent');
        assert(await input.evaluate((element, original) => element === original, identity), 'input identity retained');
      };
      await first.getByRole('button', {name:'Load', exact:true}).click();
      await check('Loaded & <safe>');
      await first.getByRole('button', {name:'Toggle extra', exact:true}).click();
      const extra = first.getByRole('textbox', {name:'Extra', exact:true});
      assert.equal(await extra.inputValue(), 'Loaded & <safe>', 'new conditional control initializes from state');
      await input.fill('Typed name');
      await check('Typed name');
      assert.equal(await extra.inputValue(), 'Typed name');
      await input.press('ArrowLeft'); await input.press('Shift+ArrowLeft');
      const selection = await input.evaluate(element => [element.selectionStart, element.selectionEnd, element.selectionDirection]);
      await input.press('F3');
      await first.getByText('Ticks: 1', {exact:true}).waitFor();
      assert(await input.evaluate(element => document.activeElement === element));
      assert.deepEqual(await input.evaluate(element => [element.selectionStart, element.selectionEnd, element.selectionDirection]), selection, 'unrelated state change preserves selection');
      await input.press('F2'); await check('Focused replacement');
      assert(await input.evaluate(element => document.activeElement === element));
      await input.press('Escape'); await check('');
      assert.equal(await extra.inputValue(), '', 'focused programmatic clear synchronizes all bindings');
      await input.pressSequentially('New draft'); await check('New draft');
      await first.getByRole('button', {name:'Clear', exact:true}).click(); await check('');
      await first.getByRole('button', {name:'Load', exact:true}).click(); await check('Loaded & <safe>');
      await first.getByRole('button', {name:'Toggle extra', exact:true}).click();
      assert.equal(await extra.count(), 0);
      await first.getByRole('button', {name:'Clear', exact:true}).click();
      await first.getByRole('button', {name:'Toggle extra', exact:true}).click();
      assert.equal(await extra.inputValue(), '', 'remounted control initializes empty state');
      await page.reload();
      assert.equal(await first.getByRole('textbox', {name:'Name', exact:true}).inputValue(), '', 'reload starts fresh');
    }}
  };
  for (const mode of modes) for (const [name, scenario] of Object.entries(cases)) {
    const context = await browser.newContext();
    await context.tracing.start({screenshots:true, snapshots:true});
    const page = await context.newPage(); page.setDefaultTimeout(5000);
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    page.on('console', message => { if (message.type() === 'error') errors.push(message.text()); });
    try {
      await page.goto(`http://127.0.0.1:${server.address().port}/${mode}/${scenario.fixture}/`);
      await scenario.run(page);
      assert.deepEqual(errors, []);
      result.checks.push({mode, name, pass:true});
      console.log(`PASS input-state-sync ${mode}/${name}`);
    } catch (error) {
      result.checks.push({mode, name, pass:false, error:String(error)});
      console.error(`FAIL input-state-sync ${mode}/${name}: ${error}`);
    }
    await page.screenshot({path:path.join(out, `${mode}-${name}.png`), fullPage:true});
    await context.tracing.stop({path:path.join(out, `${mode}-${name}.zip`)});
    await context.close();
  }
  process.exitCode = result.checks.every(check => check.pass) ? 0 : 1;
})().catch(error => { result.error = String(error); console.error(error); process.exitCode = 1; }).finally(async () => {
  if (browser) await browser.close();
  server.close();
  fs.writeFileSync(path.join(out, 'result.json'), JSON.stringify(result, null, 2) + '\n');
});
