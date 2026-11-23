// Drives the compiler-emitted private runtime. Expected traces come only from
// the reviewed ASTs and the independent reference evaluator.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const os = require('node:os');
const path = require('node:path');
const {createRequire} = require('node:module');
const {spawnSync} = require('node:child_process');
const {loadFixtures} = require('../reference_model/episode_fixtures.cjs');
const {comparePhase, typed, typedRecord} = require('../reference_model/episode_compare.cjs');
const policy = 'experimental-P3';

const compiler = process.env.JTML_CORE_PRIVATE_COMPILER;
assert(compiler && process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT && process.env.JTML_BENCH_BROWSER,
  'Private compiler, real Chromium, and Playwright are required');
const {chromium} = createRequire(path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT, 'package.json'))('playwright');
const research = process.argv.includes('--research');
const output = process.env.JTML_REFERENCE_OUT ||
  path.resolve(__dirname, '../../dist/core-reference', research ? 'research.json' : 'results.json');
const csp = "script-src 'self'; object-src 'none'; base-uri 'none'";
const assets = ['index.html', 'jtml-runtime.js', 'components/index.js', 'app.js'];
const compileDiagnostics = {E_INIT_FORWARD: /\bE_INIT_FORWARD\b/, E_DERIVED_CYCLE: /\bE_DERIVED_CYCLE\b/,
  E_UNRESOLVED: /\bE_UNRESOLVED\b/, E_UNSUPPORTED_EXPR: /\bE_UNSUPPORTED_EXPR\b/,
  E_EFFECT_CYCLE: /cyclic root effect writes/};

function attachObservers(effects) {
  const events = [], passive = [], cspViolations = [];
  // This observer covers the rendered text within the audited static envelope.
  // The envelope's HTML whitespace is outside the virtual text projection.
  const dom = () => [...document.querySelectorAll('.out')].map(node => node.textContent.trim());
  document.addEventListener('securitypolicyviolation', event => cspViolations.push(event.effectiveDirective));
  const observer = new MutationObserver(records => {
    if (records.some(record => record.target.parentElement?.closest('.out') ||
        record.target.nodeType === 1 && record.target.closest('.out'))) passive.push(dom());
  });
  observer.observe(document, {subtree: true, childList: true, characterData: true});
  const log = console.log;
  console.log = function(message, ...args) {
    if (typeof message === 'string' && message.startsWith('[SHOW] '))
      events.push({kind: 'show', text: message.slice(7), dom: dom()});
    return Reflect.apply(log, this, [message, ...args]);
  };

  let api, mapped = false;
  Object.defineProperty(window, 'jtml', {configurable: true, get: () => api, set(next) {
    api = next;
    if (!api?.actions || mapped) return;
    const manifest = JSON.parse(document.querySelector('#__jtml_client_manifest').textContent);
    const subscriptions = manifest.subscriptions || [];
    if (subscriptions.length !== effects.length) throw Error('Unmapped effect inventory');
    for (const {id, watch, line} of effects) {
      // Physical action address only. Order, watch values and trace expectations
      // are independently produced; the manifest order is never used as oracle.
      const address = '__effect_' + watch + '_' + (line - 1);
      const matching = subscriptions.filter(sub => sub.action === address && sub.dependency === watch);
      if (matching.length !== 1) throw Error('Unmapped source effect ' + id);
      const action = api.actions[address];
      const descriptor = Object.getOwnPropertyDescriptor(action, 'body');
      if (!descriptor || !Object.hasOwn(descriptor, 'value')) throw Error('Unobservable effect ' + id);
      Object.defineProperty(action, 'body', {configurable: descriptor.configurable,
        enumerable: descriptor.enumerable, get() {
          events.push({kind: 'effect-enter', effect: id, value: next.state[watch], dom: dom()});
          return descriptor.value;
        }});
    }
    mapped = true;
  }});
  window.readCoreReference = declarations => {
    if (!mapped) throw Error('Effect observation setup did not complete');
    const read = kind => Object.fromEntries(declarations.filter(binding => binding.kind === kind)
      .map(({id, name}) => [id, api.state[name]]));
    return {state: read('let'), derived: read('get'), dom: dom(), strict: api.browserBuild.cleanProduction,
      csp: cspViolations.splice(0), events: events.splice(0), passive: passive.splice(0), life: __jtml_test_failure()};
  };
}

async function observe(browser, url, {program, steps}, model, row) {
  const page = await browser.newPage();
  const networkErrors = [], errors = [];
  try {
    page.setDefaultTimeout(4000);
    page.on('pageerror', error => errors.push(error.message));
    page.on('requestfailed', request => networkErrors.push(request.url()));
    page.on('response', response => { if (!response.ok()) networkErrors.push(response.url()); });
    await page.addInitScript(attachObservers, program.effects.map(({id, watch, source}) =>
      ({id, watch: watch.slice('main::'.length), line: source.line})));
    const response = await page.goto(url);
    assert.equal(response.headers()['content-security-policy'], csp);
    let previous;
    for (const [index, expected] of [{outcome: 'initialize', events: [], commit: model.initial}, ...model.steps].entries()) {
      const button = steps[index - 1]?.button;
      if (button) await page.getByRole('button', {name: button, exact: true}).click();
      const observed = await page.evaluate(program => readCoreReference(program),
        program.declarations.map(({id, name, kind}) => ({id, name, kind})));
      // Playwright can receive the renderer exception after the evaluate reply.
      // Wait for transport only when the actual private diagnostic just faulted.
      if (observed.life.status === 'FAILED' && previous?.life.status !== 'FAILED' && !errors.length) {
        await page.waitForEvent('pageerror', {timeout: 1000}).catch(error => {
          if (error.name !== 'TimeoutError') throw error;
        });
      }
      assert.deepEqual(networkErrors, [], 'Browser infrastructure failure');
      const actual = {...observed, errors: errors.splice(0), state: typedRecord(observed.state), derived: typedRecord(observed.derived),
        partial: observed.life.partial && typedRecord(Object.fromEntries(Object.entries(observed.life.partial.root)
          .map(([name, value]) => ['main::' + name, value]))),
        events: observed.events.map(event => {
          if (event.kind === 'show') return event;
          const {value, ...entry} = event;
          return {...entry, scalar: typed(value)};
        })};
      const checks = comparePhase(expected, actual, previous, index === 0);
      row.phases.push({index, button, actual, checks});
      previous = actual;
    }
  } finally { await page.close(); }
}

async function main(report, directory) {
  let target, browser;
  const server = http.createServer((request, response) => {
    if (request.url === '/favicon.ico') return response.writeHead(204).end();
    const file = path.resolve(target, '.' + new URL(request.url, 'http://localhost').pathname);
    if (!file.startsWith(target + path.sep) || !fs.existsSync(file) || !fs.statSync(file).isFile())
      return response.writeHead(404).end();
    response.writeHead(200, {'Content-Security-Policy': csp,
      'Content-Type': file.endsWith('.js') ? 'text/javascript' : 'text/html'}).end(fs.readFileSync(file));
  });
  try {
    await new Promise((resolve, reject) => server.once('error', reject).listen(0, '127.0.0.1', resolve));
    browser = await chromium.launch({executablePath: process.env.JTML_BENCH_BROWSER, headless: true});
    report.browser = browser.version();
    const fixtures = loadFixtures().filter(row => row.research === research);
    const probe = spawnSync('python3', [path.join(__dirname, '../reference_model/run_episodes.py')], {
      input: JSON.stringify({policy, programs: fixtures.map(({program, steps}) =>
        [program, steps.map(({action, args}) => ({action, args}))])}),
      encoding: 'utf8', timeout: 60000,
    });
    assert.equal(probe.status, 0, probe.error?.message || probe.stderr);
    const traces = JSON.parse(probe.stdout);
    assert.equal(traces.length, fixtures.length);
    for (const [index, fixture] of fixtures.entries()) {
      const {id, digest, source} = fixture;
      const model = traces[index];
      assert(['accept', 'reject'].includes(model.validation), 'Fixture exceeds the reference scope');
      const row = {id, sourceSHA256: digest, model, phases: []};
      report.cases.push(row);
      const base = path.join(directory, id);
      fs.mkdirSync(base);
      const input = path.join(base, 'index.jtml');
      fs.writeFileSync(input, source);
      target = path.join(base, 'app');
      const build = spawnSync(compiler, [input, target], {encoding: 'utf8', timeout: 60000});
      if (build.error || build.signal || build.status === null) throw Error('Compiler process failed: ' + build.error);
      row.compiler = {exit: build.status, diagnostic: build.stderr.replaceAll(input, 'index.jtml')};
      const rejected = build.status !== 0;
      if (model.validation === 'reject' || rejected) {
        const diagnostic = compileDiagnostics[model.code];
        const matches = rejected && diagnostic?.test(row.compiler.diagnostic) &&
          new RegExp('index\\.jtml:' + model.source.line + ':' + model.source.column + ':').test(row.compiler.diagnostic) &&
          assets.every(asset => !fs.existsSync(path.join(target, asset)));
        row.status = matches ? 'REJECTION MATCH' : 'DIVERGENCE';
        continue;
      }
      row.runtimeBytes = fs.statSync(path.join(target, 'jtml-runtime.js')).size;
      assert(row.runtimeBytes <= 273000);
      await observe(browser, 'http://127.0.0.1:' + server.address().port + '/index.html', fixture, model, row);
      const mismatch = row.phases.some(phase => Object.values(phase.checks).some(check => check.status === 'DIVERGENCE'));
      row.status = mismatch ? 'DIVERGENCE' : model.steps.some(step => step.outcome === 'episode-error')
        ? 'EXPERIMENTAL P3 MATCH; POLICY PENDING' : 'BOUNDED MATCH';
      if (research) row.scope = 'Exploratory comparison of derived fault timing under the provisional policy';
    }
  } finally {
    if (browser) await browser.close();
    if (server.listening) await new Promise(resolve => server.close(resolve));
  }
}

const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'jtml-core-reference-'));
const report = {scope: 'bounded root fixtures', policies: {scalar: 'experimental-finite',
  text: 'reviewed-segments', failure: policy}, historicalV19: 'PENDING_POLICY',
  unsupported: ['modules/imports', 'components/lifetimes', 'general interpolation', 'faulty initialization',
    'P1/P2', 'async and host features'], cases: []};
main(report, directory).catch(error => {
  console.error(error);
  report.infrastructureError = String(error);
  process.exitCode = 1;
}).finally(() => {
  fs.rmSync(directory, {recursive: true, force: true});
  fs.mkdirSync(path.dirname(output), {recursive: true});
  fs.writeFileSync(output, JSON.stringify(report, null, 2) + '\n');
  for (const {id, status} of report.cases) console.log(id, status);
  if (report.cases.some(row => row.status === 'DIVERGENCE')) process.exitCode = 1;
});
