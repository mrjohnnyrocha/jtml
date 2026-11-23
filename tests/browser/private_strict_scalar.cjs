// Independently authored witnesses for the compiler-selected private runtime.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const os = require('node:os');
const path = require('node:path');
const {spawnSync} = require('node:child_process');
const {createRequire} = require('node:module');
const {createHash} = require('node:crypto');

const compiler = process.env.JTML_CORE_PRIVATE_COMPILER;
const browserPath = process.env.JTML_BENCH_BROWSER;
const driver = process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT;
assert(compiler && browserPath && driver, 'Private compiler and real Chromium required');
const {chromium} = createRequire(path.join(driver, 'package.json'))('playwright');
const work = fs.mkdtempSync(path.join(os.tmpdir(), 'jtml-private-scalar-'));
const runnable = ['index.html', 'app.js', 'jtml-runtime.js', 'components/index.js'];
const csp = "script-src 'self'; object-src 'none'; base-uri 'none'";
const server = http.createServer((request, response) => {
  if (request.url === '/favicon.ico') return response.writeHead(204).end();
  const file = path.resolve(work, '.' + new URL(request.url, 'http://localhost').pathname);
  if (!file.startsWith(work + path.sep) || !fs.existsSync(file) || !fs.statSync(file).isFile())
    return response.writeHead(404).end();
  response.writeHead(200, {'Content-Security-Policy': csp,
    'Content-Type': file.endsWith('.js') ? 'text/javascript' : 'text/html'}).end(fs.readFileSync(file));
});
function source(name, text) {
  const file = path.join(work, name + '.jtml');
  fs.writeFileSync(file, 'jtml 2\n' + text);
  return file;
}
const capture = argument => `let inputValue = 3
let observed = 0
when capture value
  observed = value
page
  box class "out"
    text observed
  button "Step" click capture(${argument})
`;
const localCounter = projection => `make Card
  let divisor = 2
  when step
    divisor = 0
  box class "out"
    text ${projection}
    button "Step" click step
page
  Card
`;
function build(input, output) {
  const result = spawnSync(compiler, [input, output], {encoding: 'utf8', timeout: 30000});
  assert.equal(result.error, undefined);
  assert.equal(result.signal, null);
  return result;
}
function accept(name, text) {
  const target = path.join(work, name);
  const result = build(source(name, text), target);
  assert.equal(result.status, 0, result.stderr);
  assert.deepEqual(fs.readdirSync(target).sort(), ['app.js', 'components', 'index.html', 'jtml-runtime.js']);
  const runtime = fs.readFileSync(path.join(target, 'jtml-runtime.js'));
  assert(runtime.length <= 273000, 'private runtime exceeds its budget');
  return target;
}
function reject(name, text, line, reason, preserved) {
  const input = source(name, text), fresh = path.join(work, name);
  const first = build(input, fresh);
  assert.notEqual(first.status, 0);
  assert.match(first.stderr, new RegExp(name + '\\.jtml:' + line + ':\\d+: E_STRICT_SCALAR: ' + reason));
  for (const asset of runnable) assert(!fs.existsSync(path.join(fresh, asset)));
  const before = runnable.map(asset => fs.readFileSync(path.join(preserved, asset)));
  assert.notEqual(build(input, preserved).status, 0);
  assert.deepEqual(runnable.map(asset => fs.readFileSync(path.join(preserved, asset))), before);
  console.log('PASS reject ' + name + ' with source diagnostic and asset preservation');
}

async function observe(browser, target, expectedErrors, assertions, bootFails = false) {
  const page = await browser.newPage();
  const pageErrors = [], networkErrors = [], consoleErrors = [];
  page.on('pageerror', error => pageErrors.push(error.message));
  page.on('console', message => { if (message.type() === 'error') consoleErrors.push(message.text()); });
  page.on('requestfailed', request => networkErrors.push(request.url()));
  page.on('response', response => { if (response.status() >= 400) networkErrors.push(response.url()); });
  await page.addInitScript(() => {
    window.cspErrors = [];
    document.addEventListener('securitypolicyviolation', event => cspErrors.push(event.effectiveDirective));
  });
  try {
    const fault = bootFails ? page.waitForEvent('pageerror') : null;
    await page.goto('http://127.0.0.1:' + server.address().port + '/' + path.basename(target) + '/index.html');
    if (fault) await fault;
    await assertions(page);
    assert.deepEqual(pageErrors, expectedErrors);
    assert.deepEqual(consoleErrors, []);
    assert.deepEqual(networkErrors, []);
    assert.deepEqual(await page.evaluate(() => cspErrors), []);
  } finally { await page.close(); }
}

(async () => {
  const healthy = accept('numeric-read', capture('inputValue'));
  reject('argument-addition', capture('1 + 2'), 9, 'click arguments', healthy);
  reject('argument-fault', capture('1 / 0'), 9, 'click arguments', healthy);
  reject('argument-kind', capture('"3"'), 9, 'click arguments', healthy);
  reject('argument-arity', capture('3, 4'), 9, 'click must match', healthy);
  reject('generated-projection', localCounter('"{1 / divisor}"'), 7, 'projection', healthy);
  reject('root-projection', `let x = 1
page
  text "{x + x}"
`, 4, 'projection', healthy);
  reject('escaped-text', 'let value = 1\npage\n  text "\\u007bvalue\\u007d"\n',
    4, 'escaped source text', healthy);
  reject('truthiness', `let x = 1
when step
  if x
    x = 2
page
  button "Step" click step
`, 4, 'action condition must be Boolean', healthy);
  reject('logical-coercion', `let x = true
when step
  x = x && 1
page
  button "Step" click step
`, 4, 'unsupported scalar expression or operand kinds', healthy);
  reject('mixed-arithmetic', `let x = 1
when step
  x = "2" * 3
page
  button "Step" click step
`, 4, 'unsupported scalar expression or operand kinds', healthy);
  reject('mixed-equality', `let x = false
get unequal = x == 0
page
  text unequal
`, 3, 'unsupported scalar expression or operand kinds', healthy);
  reject('changing-slot-kind', `let x = 1
when step
  x = "2"
page
  button "Step" click step
`, 4, 'assignment must preserve', healthy);
  reject('fraction-literal-fidelity', `let x = 0.0000000000000001
page
  text x
`, 2, 'numeric literals must be', healthy);
  reject('component-forward', `make Card
  let first = later
  let later = 2
  text first
page
  Card
`, 3, 'scalar binding', healthy);
  reject('component-coercion', `make Card
  let x = "1"
  when step
    x += "2"
  button "Step" click step
page
  Card
`, 5, 'assignment must preserve', healthy);
  reject('root-dropped-action-statement', `let x = 1
when step
  while x > 0
    x = 0
page
  button "Step" click step
`, 4, 'unsupported action statement', healthy);
  reject('external-bridge', `let job = fetch "/api/example" lazy
page
  p "hello"
`, 2, 'unsupported scalar expression or operand kinds', healthy);

  const boot = accept('failed-boot', `let x = 1
get bad = x / 0
when step
  x = 2
page
  box class "out"
    text "Value {x}"
  button "Step" click step
`);
  const failedComponent = accept('failed-component-boot', `make Card
  let seed = 1
  get bad = seed / 0
  box class "out"
    text bad
page
  Card
`);
  const hidden = accept('unrendered-write', localCounter('"stable"'));
  await new Promise((resolve, reject) => server.once('error', reject).listen(0, '127.0.0.1', resolve));
  const browser = await chromium.launch({headless: true, executablePath: browserPath});
  try {
    await observe(browser, healthy, [], async page => {
      assert.equal(await page.evaluate(() => jtml.runAction('capture', '3')), false);
      assert.equal(await page.evaluate(() => jtml.runAction('capture', Infinity)), false);
      assert.equal(await page.evaluate(() => jtml.runAction('capture')), false);
      await page.getByRole('button', {name: 'Step'}).click();
      assert.equal(await page.locator('.out').textContent().then(text => text.trim()), '3');
      assert.deepEqual(await page.evaluate(() => [typeof jtml.state.observed, jtml.state.observed]), ['number', 3]);
    });
    for (const target of [boot, failedComponent]) {
      await observe(browser, target, ['Non-finite arithmetic result for /'], async page => {
        const life = await page.evaluate(() => __jtml_test_failure());
        assert.equal(life.status, 'FAILED');
        assert.equal(life.revision, 0);
        assert.deepEqual(life.fault.invocation, {kind: 'initialization'});
        assert.equal(life.fault.code, 'E_NON_FINITE_ARITHMETIC');
        assert.deepEqual(life.pendingWrites, []);
        assert.equal(await page.evaluate(() => jtml.runAction('step')), false);
        if (target === boot) {
          assert.deepEqual(life.partial.root, {x: 1});
          assert.equal(life.fault.publicationEntered, false);
        } else {
          assert.deepEqual(life.partial.components.map(instance => instance.state), [{seed: 1}]);
        }
      }, true);
    }
    await observe(browser, hidden, [], async page => {
      await page.getByRole('button', {name: 'Step'}).click();
      assert.equal(await page.evaluate(() => jtml.getComponentInstances()[0].scope.divisor), 0);
      assert.equal(await page.evaluate(() => __jtml_test_failure().revision), 0);
      assert.match(await page.locator('.out').textContent(), /stable/);
    });
  } finally { await browser.close(); }
  const runtime = fs.readFileSync(path.join(healthy, 'jtml-runtime.js'));
  console.log('PASS private scalar closure, initialization failure and publication revision boundary',
    {runtimeBytes: runtime.length, sha256: createHash('sha256').update(runtime).digest('hex')});
})().catch(error => { console.error(error); process.exitCode = 1; }).finally(async () => {
  if (server.listening) await new Promise(resolve => server.close(resolve));
  fs.rmSync(work, {recursive: true, force: true});
});
