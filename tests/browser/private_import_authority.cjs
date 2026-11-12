const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const os = require('node:os');
const path = require('node:path');
const {spawnSync} = require('node:child_process');
const {createRequire} = require('node:module');

const {chromium} = createRequire(path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT, 'package.json'))('playwright');
const {vectors} = require('../../docs/spec/reactive-core-v1/vectors.json');
const work = fs.mkdtempSync(path.join(os.tmpdir(), 'jtml-import-authority-'));
const publicCompiler = process.env.JTML_REAL_BIN;
const strictCompiler = process.env.JTML_PRIVATE_COMPILER;
const assets = ['index.html', 'jtml-runtime.js', 'components/index.js', 'app.js'];
const owner = `jtml 2
export let seed = 7
export let hits = 0
export const fixed = 5
export get doubled = seed * 2
export let record = {value: 2}
export let rows = [1, 2]
effect seed
  hits += 1
export when advance delta
  seed += delta
when hiddenAction
  seed = 100
`;
const barrel = 'jtml 2\nexport use {seed, hits, doubled, advance} from "./owner.jtml"\n';

function fixture(name, files) {
  const dir = path.join(work, name);
  fs.mkdirSync(dir);
  for (const [file, content] of Object.entries(files)) fs.writeFileSync(path.join(dir, file), content);
  return dir;
}

function compile(dir, dest, strict) {
  return spawnSync(strict ? strictCompiler : publicCompiler,
    strict ? ['index.jtml', dest] : ['build', 'index.jtml', '--target', 'browser', '--out', dest],
    {cwd: dir, encoding: 'utf8', timeout: 30000});
}

function accept(dir, name, strict, ordinary) {
  const dest = path.join(work, name);
  const result = compile(dir, dest, strict);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  if (ordinary) for (const asset of assets)
    assert.deepEqual(fs.readFileSync(path.join(dest, asset)), fs.readFileSync(path.join(ordinary, asset)));
  if (strict) fs.copyFileSync(process.env.JTML_TEST_RUNTIME_ASSET, path.join(dest, 'jtml-runtime.js'));
  return dest;
}

function reject(dir, name, diagnostic, preserved) {
  const dest = path.join(work, name);
  const result = compile(dir, dest, true);
  console.log(name, result.status, result.stderr.trim());
  assert.notEqual(result.status, 0, 'strict compiler admitted ' + name);
  assert.match(result.stderr, diagnostic);
  for (const asset of assets) assert.equal(fs.existsSync(path.join(dest, asset)), false);
  if (preserved) {
    const before = assets.map(asset => fs.readFileSync(path.join(preserved, asset)));
    const again = compile(dir, preserved, true);
    assert.notEqual(again.status, 0);
    assert.match(again.stderr, diagnostic);
    assert.deepEqual(assets.map(asset => fs.readFileSync(path.join(preserved, asset))), before);
  }
}

const server = http.createServer((request, response) => {
  const pathname = new URL(request.url, 'http://localhost').pathname;
  if (pathname === '/favicon.ico') return response.writeHead(204).end();
  const file = path.resolve(work, '.' + pathname);
  if (!file.startsWith(work + path.sep) || !fs.existsSync(file) || !fs.statSync(file).isFile())
    return response.writeHead(404).end();
  response.writeHead(200, {'Content-Security-Policy': "script-src 'self'; object-src 'none'",
    'Content-Type': file.endsWith('.js') ? 'text/javascript' : 'text/html'}).end(fs.readFileSync(file));
});

async function open(browser, dest, test) {
  const page = await browser.newPage();
  const errors = [], shows = [];
  page.on('pageerror', error => errors.push(error.message));
  page.on('console', message => {
    if (message.type() === 'error') errors.push(message.text());
    if (message.text().startsWith('[SHOW]')) shows.push(message.text());
  });
  await page.addInitScript(() => {
    window.violations = [];
    document.addEventListener('securitypolicyviolation', event => violations.push(event.effectiveDirective));
  });
  try {
    await page.goto(`http://127.0.0.1:${server.address().port}/${path.basename(dest)}/index.html`);
    await test(page, shows);
    assert.deepEqual(errors, []);
    assert.deepEqual(await page.evaluate(() => violations), []);
  } finally { await page.close(); }
}

const step = page => page.getByRole('button', {name: 'Step', exact: true}).click();
const view = async page => (await page.locator('.out').textContent()).trim();
const make = (name, contents) => fixture(name, {'index.jtml': 'jtml 2\n' + contents,
  'owner.jtml': owner, 'barrel.jtml': barrel});
const write = (name, use, body) => make(name, `${use}
when step
${body}
page
  button "Step" click step
`);

(async () => {
  const original = fixture('v22', vectors.find(vector => vector.id === 'V22').files);
  const ordinary = accept(original, 'ordinary-v22', false);
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const browser = await chromium.launch({executablePath: process.env.JTML_BENCH_BROWSER});
  try {
    await open(browser, ordinary, async page => {
      assert.equal(await page.evaluate(() => jtml.state.seed), 7);
      await step(page);
      assert.equal(await page.evaluate(() => jtml.state.seed), 9);
      assert.equal(await view(page), 'never');
      console.log('Default V22 accepted: seed 7 -> 9, literal DOM unchanged');
    });
    reject(original, 'strict-v22', /index\.jtml:4:3: E_IMPORT_WRITE:.*`seed`/, ordinary);

    for (const from of ['owner', 'barrel']) {
      const dir = make('action-' + from, `use {seed, hits, doubled, advance} from "./${from}.jtml"
page
  box class "out"
    text "{seed}|{hits}|{doubled}"
  button "Step" click advance(2)
`);
      const full = accept(dir, 'public-' + from, false);
      const dest = accept(dir, 'strict-' + from, true, full);
      await open(browser, dest, async page => {
        assert.equal(await view(page), '7|0|14');
        await step(page);
        assert.deepEqual(await page.evaluate(() => [jtml.state.seed, jtml.state.hits, jtml.state.doubled]), [9, 1, 18]);
        assert.equal(await view(page), '9|1|18');
        assert.equal(await page.evaluate(() => __jtml_test_failure().status), 'ACTIVE');
      });
      console.log('Owner action via ' + from + ': 7|0|14 -> 9|1|18');
    }
    const lexical = make('lexical', `use {seed, hits} from "./owner.jtml"
when step seed
  seed = seed + 1
  let seed = seed + 1
  let local = seed + 1
  local += 1
  show seed
  show local
page
  box class "out"
    text "{seed}"
  button "Step" click step(20)
`);
    await open(browser, accept(lexical, 'strict-lexical', true), async (page, shows) => {
      await step(page);
      assert.deepEqual(shows, ['[SHOW] 22', '[SHOW] 24']);
      assert.equal(await view(page), '7');
      assert.equal(await page.evaluate(() => jtml.state.seed), 7);
    });
    for (const [name, use, body, code, line, col, spelling] of [
      ['compound', 'use seed from "./owner.jtml"', '  seed += 1', 'E_IMPORT_WRITE', 4, 3, 'seed'],
      ['reexport', 'use seed from "./barrel.jtml"', '  seed = 9', 'E_IMPORT_WRITE', 4, 3, 'seed'],
      ['inactive', 'use seed from "./owner.jtml"', '  if true\n    show 0\n  else\n    seed = 9', 'E_IMPORT_WRITE', 7, 5, 'seed'],
      ['member', 'use record from "./owner.jtml"', '  record.value = 9', 'E_IMPORT_WRITE', 4, 3, 'record'],
      ['indexed', 'use rows from "./owner.jtml"', '  rows[0] = 9', 'E_IMPORT_WRITE', 4, 3, 'rows'],
      ['borrowed-alias', 'use record from "./owner.jtml"', '  let copy = record\n  copy.value = 9', 'E_STRICT_WRITE_FORM', 5, 3, 'copy'],
      ['unprotected-shadow', 'use seed from "./owner.jtml"', '  let seed = 9', 'E_UNRESOLVED', 4, 3, 'seed'],
      ['generated', 'use seed from "./owner.jtml"', '  __jtml_scalar_00000000000000000000 = 1', 'E_UNRESOLVED', 4, 3, '__jtml_scalar_00000000000000000000'],
      ['getter', 'use doubled from "./owner.jtml"', '  doubled = 1', 'E_NOT_WRITABLE', 4, 3, 'doubled'],
      ['constant', 'use fixed from "./owner.jtml"', '  fixed = 1', 'E_NOT_WRITABLE', 4, 3, 'fixed'],
      ['whole-file', 'use "./owner.jtml"', '  seed = 1', 'E_STRICT_IMPORT_FORM', 0, 0, '']
    ]) {
      const coordinate = line ? `:${line}:${col}` : '';
      reject(write(name, use, body), 'reject-' + name,
        new RegExp(`index\\.jtml${coordinate}: ${code}:.*${spelling}`));
    }
    reject(write('alias', 'use seed as alias from "./owner.jtml"', '  show alias'),
      'reject-alias', /export|import/i);
    for (const action of ['hiddenAction', 'advance']) {
      const dir = make('without-' + action, `use seed from "./owner.jtml"
page
  button "Step" click ${action}(2)
`);
      reject(dir, 'reject-' + action, new RegExp(`index\\.jtml:\\d+:\\d+: E_ACTION_AUTHORITY:.*${action}`));
    }
    const component = make('component-dispatch', `use {seed, hits} from "./owner.jtml"
make Probe
  button "Step" click hiddenAction(2)
page
  Probe
`);
    reject(component, 'reject-component-dispatch', /index\.jtml:\d+:\d+: E_ACTION_AUTHORITY:.*hiddenAction/);
    console.log('PASS strict V22, owner action dispatch, lexical isolation, and default V22');
  } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; }).finally(async () => {
  if (server.listening) await new Promise(resolve => server.close(resolve));
  fs.rmSync(work, {recursive: true, force: true});
});
