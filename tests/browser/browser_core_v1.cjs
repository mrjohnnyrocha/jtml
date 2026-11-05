// Additive inventory of unchanged authored vectors. Expected values never come
// from emitted plans. Existing phase, lifecycle and application suites remain mandatory.
const assert = require('node:assert/strict');
const fs = require('node:fs'), path = require('node:path'), http = require('node:http');
const {spawnSync} = require('node:child_process');
const {createRequire} = require('node:module');
const {createHash} = require('node:crypto');
const {isDeepStrictEqual} = require('node:util');
assert(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT && process.env.JTML_BENCH_BROWSER,
  'Real Chromium and Playwright are required; no skip');
const {chromium} = createRequire(path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT, 'package.json'))('playwright');
const repo = path.resolve(__dirname, '../..');
const binary = process.env.JTML_BIN || path.join(repo, 'build/jtml');
const output = path.resolve(process.env.JTML_CORE_OUT || path.join(repo, 'dist/browser-core-v1'));
const catalogBytes = fs.readFileSync(path.join(repo, 'docs/spec/reactive-core-v1/vectors.json'));
const vectors = JSON.parse(catalogBytes).vectors;
const hash = bytes => createHash('sha256').update(bytes).digest('hex');
const labels = {V06: ['Step', 'Step'], V08: ['Right', 'Toggle', 'Left', 'Toggle'],
  V09: ['Private', 'Entry', 'Read private', 'Read entry'], V16: ['Local', 'Toggle', 'Bump', 'Toggle'],
  'owner-authority': ['Shadow', 'Owner', 'Read']};
const ownerSnapshot = (seed, show = []) => ({outcome: 'commit', state: {'private::seed': seed},
  derived: {}, dom: {'.out': [String(seed)]}, effects: [], notifications: [], show});
const ownerControl = {id: 'owner-authority', entry: 'index.jtml', files: {
  'private.jtml': 'jtml 2\nexport let seed = 7\nexport when change\n  seed = 9\n',
  'index.jtml': 'jtml 2\nuse {seed, change} from "./private.jtml"\nwhen shadow seed\n  seed += 1\n  show seed\nwhen readSeed\n  show seed\npage\n  box class "out"\n    text seed\n  button "Shadow" click shadow(3)\n  button "Owner" click change\n  button "Read" click readSeed\n'
}, expected_initial: ownerSnapshot(7), steps: [
  {expected: ownerSnapshot(7, [4])}, {expected: ownerSnapshot(9)}, {expected: ownerSnapshot(9, [9])}
]};
const rejection = {
  V18: /cyclic root effect writes/, V23: /string interpolation requires resolved expression nodes/,
  V24: /component-local effects are unsupported/
};
const assets = ['index.html', 'jtml-runtime.js', 'components/index.js', 'app.js'];
const report = {catalogSHA256: hash(catalogBytes), compilerSHA256: hash(fs.readFileSync(binary)),
  scope: 'bounded vector observations; not general commit/observer atomicity or failure-policy ratification',
  vectors: [], cleanBrowserExit: false, cleanServerExit: false};
let target, browser;
const server = http.createServer((request, response) => {
  if (request.url === '/favicon.ico') return response.writeHead(204).end();
  const file = path.resolve(target, '.' + new URL(request.url, 'http://localhost').pathname);
  if (!file.startsWith(target + path.sep) || !fs.existsSync(file) || !fs.statSync(file).isFile())
    return response.writeHead(404).end();
  response.writeHead(200, {'Content-Security-Policy': "script-src 'self'; object-src 'none'; base-uri 'none'",
    'Content-Type': file.endsWith('.js') ? 'text/javascript' : 'text/html'}).end(fs.readFileSync(file));
});

function instrument(declarations) {
  const events = [], seen = new WeakSet(), generations = new WeakMap();
  let nextGeneration = 0;
  const generation = instance => {
    if (!generations.has(instance)) generations.set(instance, ++nextGeneration);
    return 'main::Card@' + generations.get(instance);
  };
  function intercept(name, attach) {
    let value;
    Object.defineProperty(window, name, {configurable: true, get: () => value,
      set(next) { value = next; attach(next); }});
  }
  intercept('jtml', api => {
    if (!api?.actions || seen.has(api.actions)) return;
    seen.add(api.actions);
    const manifest = JSON.parse(document.querySelector('#__jtml_client_manifest').textContent);
    const subscriptions = manifest.subscriptions || [];
    if (subscriptions.length !== declarations.length) throw Error('Unexpected subscription inventory');
    for (const subscription of subscriptions) {
      const index = declarations.findIndex(d => d.action === subscription.action && d.watch === subscription.dependency);
      if (index < 0) throw Error('Unidentified effect declaration');
      const action = api.actions[subscription.action], body = action.body;
      Object.defineProperty(action, 'body', {enumerable: true, configurable: true, get() {
        events.push({kind: 'effect', label: 'E' + (index + 1), watch: declarations[index].watch,
          value: api.state[declarations[index].watch]});
        return body;
      }});
    }
  });
  intercept('__jtml_static_component_modules', modules => {
    for (const module of Object.values(modules)) for (const kind of ['create', 'destroy']) {
      const original = module[kind];
      module[kind] = function(instance, ...args) {
        events.push({kind, generation: generation(instance)});
        return original.call(this, instance, ...args);
      };
    }
  });
  window.coreCSP = [];
  document.addEventListener('securitypolicyviolation', event => coreCSP.push(event.effectiveDirective));
  window.coreObservation = () => {
    const manifest = JSON.parse(document.querySelector('#__jtml_client_manifest').textContent);
    return {state: Object.fromEntries(Object.entries(jtml.state).map(([key, value]) =>
      [key, typeof value === 'number' && !Number.isFinite(value) ? String(value) : value])),
      dom: [...document.querySelectorAll('.out')].map(element => element.textContent.trim()),
      instances: jtml.getComponentInstances().map(instance => ({component: instance.component,
        generation: generation(instance), scope: instance.scope})), events: events.splice(0),
      csp: coreCSP.slice(), mode: jtml.browserBuild,
      forbiddenFallback: !!(jtml.directComponentCreateFallback || jtml.directComponentPatchFallback ||
        jtml.directComponentBodyPlanFallback || jtml.directComponentStaticUpdateError),
      // V09's source names map to read-action addresses; values remain independently authored.
      addresses: Object.fromEntries(['readEntry', 'readPrivate'].filter(name => manifest.actions[name])
        .map(name => [name, manifest.actions[name].body[0].exprPlan.root]))};
  };
}

async function main() {
  fs.mkdirSync(output, {recursive: true});
  await new Promise((resolve, reject) => server.once('error', reject).listen(0, '127.0.0.1', resolve));
  browser = await chromium.launch({headless: true, executablePath: process.env.JTML_BENCH_BROWSER});
  report.browser = browser.version();
  for (const vector of [...vectors, ownerControl]) {
    const row = {id: vector.id, checks: [], phases: [], errors: []};
    report.vectors.push(row);
    // Unique per invocation: stale assets cannot turn a rejection into a passing observation.
    const directory = fs.mkdtempSync(path.join(output, vector.id + '-'));
    const source = path.join(directory, 'source');
    fs.mkdirSync(source);
    for (const [name, text] of Object.entries(vector.files)) {
      const file = path.join(source, name);
      fs.mkdirSync(path.dirname(file), {recursive: true}); fs.writeFileSync(file, text);
    }
    row.sourceSHA256 = Object.fromEntries(Object.entries(vector.files).map(([name, text]) => [name, hash(text)]));
    target = path.join(directory, 'app');
    const build = spawnSync(binary, ['build', path.join(source, vector.entry), '--target', 'browser', '--out', target],
      {encoding: 'utf8', timeout: 60000});
    row.build = {exit: build.status, signal: build.signal, error: build.error?.message, diagnostic: build.stderr};
    const check = (name, actual, expected) => row.checks.push({name,
      status: isDeepStrictEqual(actual, expected) ? 'PASS' : 'FAIL', actual, expected});
    if (build.error || build.signal || build.status === null) throw Error('Compiler infrastructure failed: ' + JSON.stringify(row.build));
    if (build.status !== 0) {
      check('intended rejection', vector.expected_initial.outcome, 'reject');
      check('diagnostic', (rejection[vector.id] || new RegExp(vector.expected_initial.diagnostic)).test(build.stderr), true);
      check('original source', /(?:index|private)\.jtml:\d+:\d+/.test(build.stderr), true);
      check('no runnable assets', assets.every(name => !fs.existsSync(path.join(target, name))), true);
      row.classification = row.checks.every(c => c.status === 'PASS') ? 'REJECTED BY DESIGN' : 'CONFIRMED DEFECT';
      continue;
    }
    check('expected accepted source', vector.expected_initial.outcome, 'commit');
    for (const name of assets) {
      const bytes = fs.readFileSync(path.join(target, name));
      check(name + ' no dynamic compilation', /\bnew\s+Function\b|\beval\s*\(/.test(bytes.toString()), false);
      if (name === 'jtml-runtime.js') { row.runtimeBytes = bytes.length; check('unchanged runtime cap', bytes.length <= 273000, true); }
    }
    check('no compat asset', fs.existsSync(path.join(target, 'jtml-update-plans.js')), false);
    const shown = [], page = await browser.newPage();
    try {
      page.setDefaultTimeout(4000);
      page.on('pageerror', error => row.errors.push(error.message));
      page.on('console', message => {
        if (message.type() === 'error') row.errors.push(message.text());
        if (message.text().startsWith('[SHOW] ')) shown.push(message.text().slice(7));
      });
      page.on('requestfailed', request => row.errors.push('request failed: ' + request.url()));
      const declarations = vector.files[vector.entry].split('\n').flatMap((line, index) => {
        const match = /^effect (\w+)$/.exec(line);
        return match ? [{watch: match[1], action: `__effect_${match[1]}_${index}`}] : [];
      });
      await page.addInitScript(instrument, declarations);
      await page.goto(`http://127.0.0.1:${server.address().port}/index.html`);
      const phases = [vector.expected_initial, ...vector.steps.map(step => step.expected)];
      for (const [index, expected] of phases.entries()) {
        if (index) await page.getByRole('button', {name: labels[vector.id]?.[index - 1] || 'Step', exact: true}).click();
        const actual = await page.evaluate(() => coreObservation());
        row.phases.push({index, actual, shown: shown.splice(0)});
        check('prod ' + index, actual.mode.cleanProduction, true);
        check('CSP ' + index, actual.csp, []);
        check('no fallback ' + index, actual.forbiddenFallback, false);
        if (expected.outcome === 'pending-policy') continue; // Observation, never a chosen numeric/fault oracle.
        const project = oracle => Object.fromEntries(Object.keys(oracle || {}).map(key => {
          const parts = key.split('::');
          const value = parts.length === 3
            ? actual.instances.find(i => i.generation === parts.slice(0, 2).join('::'))?.scope?.[parts[2]]
            : vector.id === 'V09'
              ? actual.state[actual.addresses[parts[0] === 'main' ? 'readEntry' : 'readPrivate']]
              : actual.state[parts[1]];
          return [key, value];
        }));
        if (vector.id === 'V09') check('distinct owner addresses', actual.addresses.readEntry !== actual.addresses.readPrivate, true);
        check('state ' + index, project(expected.state), expected.state);
        check('derived ' + index, project(expected.derived), expected.derived);
        check('DOM ' + index, actual.dom, expected.dom?.['.out']);
        check('show ' + index, row.phases.at(-1).shown, expected.show?.map(String));
        const effects = actual.events.filter(event => event.kind === 'effect');
        check('effects ' + index, effects.map(e => e.label), expected.effects);
        check('watch at effect entry ' + index, effects.map(e => `${e.label}:main::${e.watch}=${e.value}`), expected.notifications);
        if (expected.created) check('created ' + index, actual.events.filter(e => e.kind === 'create').map(e => e.generation), expected.created);
        if (expected.destroyed) check('destroyed ' + index, actual.events.filter(e => e.kind === 'destroy').map(e => e.generation), expected.destroyed);
        if (vector.id === 'V16') check('active generations ' + index, actual.instances.map(i => i.generation),
          [...new Set(Object.keys({...expected.state, ...expected.derived}).filter(k => k.split('::').length === 3).map(k => k.split('::').slice(0, 2).join('::')))]);
      }
    } finally { await page.close(); }
    check('no browser errors', row.errors, []);
    row.classification = vector.id === 'V22' ? 'PENDING POLICY' : row.checks.some(c => c.status === 'FAIL') ? 'CONFIRMED DEFECT'
      : vector.steps.some(step => step.expected.outcome === 'pending-policy') ? 'PENDING POLICY' : 'PASS';
    if (vector.id === 'V22') row.policyConflict = 'Catalog requires read-only imports; unchanged action_get_reads.cjs requires named/reexported imported writes. Original mismatch checks remain FAIL; this is not a waived green gate.';
  }
}
main().catch(error => { report.infrastructureError = String(error); process.exitCode = 1; }).finally(async () => {
  if (browser) { await browser.close(); report.cleanBrowserExit = true; }
  if (server.listening) { await new Promise(resolve => server.close(resolve)); report.cleanServerExit = true; }
  fs.mkdirSync(output, {recursive: true});
  fs.writeFileSync(path.join(output, 'results.json'), JSON.stringify(report, null, 2) + '\n');
  console.log(JSON.stringify({vectors: report.vectors.map(row => ({id: row.id, classification: row.classification})),
    cleanBrowserExit: report.cleanBrowserExit, cleanServerExit: report.cleanServerExit,
    profileReadiness: 'NO-GO: component derived coherence and failure policy remain unresolved'}, null, 2));
  if (report.vectors.some(row => row.classification === 'CONFIRMED DEFECT' || row.classification === 'PENDING POLICY')) process.exitCode = 1;
});
