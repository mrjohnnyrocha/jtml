// Observe operand reads in real compiled actions. Accessors are test probes,
// since arithmetic such as 1 / 0 currently returns Infinity in the browser.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const os = require('node:os');
const path = require('node:path');
const {spawnSync} = require('node:child_process');
const {createRequire} = require('node:module');

const repo = path.resolve(__dirname, '../..');
const fixture = path.join(repo, 'tests/fixtures/production/short-circuit');
const binary = process.env.JTML_BIN || path.join(repo, 'build/jtml');
assert(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT && process.env.JTML_BENCH_BROWSER,
  'Real Chromium and Playwright are required');
const {chromium} = createRequire(path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT, 'package.json'))('playwright');
const work = fs.mkdtempSync(path.join(os.tmpdir(), 'jtml-short-circuit-'));
const cases = [
  {button: 'Skip and', fault: 'rightValue', reads: [], value: false},
  {button: 'Skip or', fault: 'rightValue', reads: [], value: true},
  {button: 'Take and', reads: ['rightValue'], value: 'right'},
  {button: 'Take or', reads: ['rightValue'], value: 'right'},
  {button: 'Choose yes', fault: 'noValue', reads: ['yesValue'], value: 'yes'},
  {button: 'Choose no', fault: 'yesValue', reads: ['noValue'], value: 'no'},
  {button: 'Nested', fault: 'rightValue', reads: ['firstValue', 'secondValue', 'thirdValue'], value: 'third'},
];
const operands = ['rightValue', 'yesValue', 'noValue', 'firstValue', 'secondValue', 'thirdValue', 'generatedValue'];
const mismatches = [];
let target, browser;

function build(name, flags = []) {
  target = path.join(work, path.basename(name, '.jtml') + (flags[0] || '-prod'));
  return spawnSync(binary, ['build', path.join(fixture, name), '--target', 'browser', '--out', target, ...flags],
    {encoding: 'utf8', timeout: 30000});
}

const server = http.createServer((request, response) => {
  if (request.url === '/favicon.ico') return response.writeHead(204).end();
  const file = path.resolve(target, '.' + new URL(request.url, 'http://localhost').pathname);
  if (!file.startsWith(target + path.sep) || !fs.existsSync(file)) return response.writeHead(404).end();
  response.writeHead(200, {'Content-Security-Policy': "script-src 'self'; object-src 'none'; base-uri 'none'",
    'Content-Type': file.endsWith('.js') ? 'text/javascript' : 'text/html'}).end(fs.readFileSync(file));
});

function compare(label, actual, expected) {
  try {
    assert.deepEqual(actual, expected);
    console.log('PASS', label, JSON.stringify(actual));
  } catch {
    mismatches.push(label);
    console.error('FAIL', label, JSON.stringify({actual, expected}));
  }
}

async function clickAndObserve(page, button, component, fault, errors) {
  await page.evaluate(({component, fault, operands}) => {
    const state = component ? jtml.getComponentInstances().find(i => i.component === component).scope : jtml.state;
    const descriptors = operands.filter(name => Object.hasOwn(state, name))
      .map(name => [name, Object.getOwnPropertyDescriptor(state, name)]);
    const reads = [];
    for (const [name, descriptor] of descriptors) {
      Object.defineProperty(state, name, {configurable: true, enumerable: true, get() {
        reads.push(name);
        if (name === fault) throw Error('Unexpected operand read: ' + name);
        return descriptor.value;
      }});
    }
    window.observeOperands = () => {
      for (const [name, descriptor] of descriptors) Object.defineProperty(state, name, descriptor);
      return {reads, value: state.answer, choice: state.choice};
    };
  }, {component, fault, operands});
  await button.click();
  await page.evaluate(() => new Promise(resolve => requestAnimationFrame(resolve)));
  return {...await page.evaluate(() => observeOperands()), errors: errors.splice(0)};
}

async function exercise(flags) {
  const result = build('index.jtml', flags);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  const mode = flags[0] || 'prod';
  const page = await browser.newPage();
  try {
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    page.on('console', message => {if (message.type() === 'error') errors.push(message.text());});
    await page.addInitScript(() => {
      window.violations = [];
      document.addEventListener('securitypolicyviolation', event => violations.push(event.effectiveDirective));
    });
    await page.goto(`http://127.0.0.1:${server.address().port}/index.html`);
    for (const component of [null, 'ActionProbe']) {
      const scope = page.locator(component ? '.action-probe' : '.root-probe');
      for (const {button, fault, reads, value} of cases) {
        const observed = await clickAndObserve(page, scope.getByRole('button', {name: button, exact: true}), component, fault, errors);
        const dom = component ? await scope.locator('p').textContent() : await scope.locator('.root-out').textContent();
        compare(`${mode}/${component || 'root'}/${button}`,
          {...observed, dom: dom.trim()}, {reads, value, choice: undefined, errors: [], dom: String(value)});
      }
    }
    for (const [name, fault, reads, choice, generated] of [
      ['Skip projection', 'rightValue', [], true, 'true'],
      ['Skip projection', 'generatedValue', [], true, 'true'],
      ['Take projection', undefined, ['rightValue', 'generatedValue'], 'right', 'generated'],
    ]) {
      const scope = page.locator('.projection-probe');
      const observed = await clickAndObserve(page, scope.getByRole('button', {name, exact: true}), 'ProjectionProbe', fault, errors);
      const dom = await scope.locator('.derived-out, .generated-out').allTextContents();
      compare(`${mode}/${name}/${fault || 'active'}`, {...observed, dom, patch: await page.evaluate(() => jtml.directComponentLastPatch?.mode)},
        {reads, value: undefined, choice, errors: [], dom: [String(choice), generated], patch: 'compiled-update-plan'});
    }
    assert.deepEqual(await page.evaluate(() => violations), []);
  } finally {
    await page.close();
  }
}

(async () => {
  for (const [name, diagnostic] of [
    ['unresolved.jtml', /unresolved\.jtml:4:3: E_UNRESOLVED:.*missing/],
    ['unsupported.jtml', /unsupported\.jtml:4:3: E_UNSUPPORTED_EXPR:.*missingFunction/],
  ]) {
    const result = build(name);
    assert.equal(result.status, 1, result.stdout + result.stderr);
    assert.match(result.stderr, diagnostic);
    for (const file of ['index.html', 'app.js', 'jtml-runtime.js', 'components/index.js']) {
      assert(!fs.existsSync(path.join(target, file)), file);
    }
    console.log('PASS static rejection', name);
  }

  await new Promise((resolve, reject) => server.once('error', reject).listen(0, '127.0.0.1', resolve));
  browser = await chromium.launch({headless: true, executablePath: process.env.JTML_BENCH_BROWSER});
  for (const flags of [[], ['--compat'], ['--debug']]) await exercise(flags);
  assert.deepEqual(mismatches, [], 'Short-circuit observations differ from the authored expectations');
})().catch(error => {
  console.error(error);
  process.exitCode = 1;
}).finally(async () => {
  if (browser) await browser.close();
  if (server.listening) await new Promise(resolve => server.close(resolve));
  fs.rmSync(work, {recursive: true, force: true});
});
