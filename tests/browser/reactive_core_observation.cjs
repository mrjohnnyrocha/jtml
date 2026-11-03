// Execute authored vectors unchanged. All hooks live in the Playwright page.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const os = require('node:os');
const path = require('node:path');
const {spawnSync} = require('node:child_process');
const {createRequire} = require('node:module');
const {gzipSync} = require('node:zlib');

const {chromium} = createRequire(path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT, 'package.json'))('playwright');
const catalog = require('../../docs/spec/reactive-core-v1/vectors.json').vectors;
const bin = process.env.JTML_BIN || path.resolve(__dirname, '../../build/jtml');
const {isDeepStrictEqual} = require('node:util');
const labels = {V02: ['Step'], V07: ['Step', 'Step'], V16: ['Local', 'Toggle', 'Bump', 'Toggle'],
  V17: ['Step'], V20: ['Step']};
const tie = {
  id: 'tie',
  entry: 'index.jtml',
  files: {'index.jtml': `jtml 2
let x = 0
let first = 0
let second = 0
effect x
  first = 1
effect x
  second = first + 1
when step
  x = 1
page
  box class "out"
    text "{first}|{second}"
  button "Step" click step
`},
  expected_initial: {state: {'main::x': 0, 'main::first': 0, 'main::second': 0},
    derived: {}, dom: {'.out': ['0|0']}, notifications: [], effects: [], show: []},
  steps: [{expected: {state: {'main::x': 1, 'main::first': 1, 'main::second': 2},
    derived: {}, dom: {'.out': ['1|2']}, notifications: ['E1:main::x=1', 'E2:main::x=1'],
    effects: ['E1', 'E2'], show: []}}]
};

const fresh = {
  id: 'effect-freshness',
  entry: 'index.jtml',
  files: {'index.jtml': `jtml 2
let x = 0
let y = 0
let seen = 0
get twice = y * 2
effect x
  y = 2
  show twice
  seen = twice
when step
  x = 1
page
  box class "out"
    text "{y}|{twice}|{seen}"
  button "Step" click step
`},
  expected_initial: {state: {'main::x': 0, 'main::y': 0, 'main::seen': 0},
    derived: {'main::twice': 0}, dom: {'.out': ['0|0|0']}, notifications: [], effects: [], show: []},
  steps: [{expected: {state: {'main::x': 1, 'main::y': 2, 'main::seen': 4},
    derived: {'main::twice': 4}, dom: {'.out': ['2|4|4']}, notifications: ['E1:main::x=1'],
    effects: ['E1'], show: [4]}}]
};

function install({rootKeys, declarations}) {
  const events = [];
  const retained = new Set();
  const scopes = new WeakMap();
  const generationByInstance = new WeakMap();
  const instrumented = new Set();
  let nextGeneration = 0;
  function generation(instance) {
    if (!generationByInstance.has(instance)) generationByInstance.set(instance, ++nextGeneration);
    return 'main::Card@' + generationByInstance.get(instance);
  }
  const snapshot = state => Object.fromEntries(rootKeys.filter(key => Object.hasOwn(state, key))
    .map(key => [key, state[key]]));
  const dom = () => [...document.querySelectorAll('.out')].map(el => el.textContent.trim());
  const record = (kind, data) => events.push({kind, ...data});
  const log = console.log;
  console.log = function (message, ...args) {
    if (typeof message === 'string' && message.startsWith('[SHOW] ')) {
      record('show', {value: message.slice(7), dom: dom()});
    }
    return Reflect.apply(log, this, [message, ...args]);
  };
  for (const [prototype, property] of [[Node.prototype, 'textContent'], [Element.prototype, 'innerHTML']]) {
    const descriptor = Object.getOwnPropertyDescriptor(prototype, property);
    Object.defineProperty(prototype, property, {...descriptor, set(value) {
      const observed = this.nodeType === 1 && (this.closest('.out') ||
        [...retained].some(el => el.contains(this) || this.contains(el)));
      descriptor.set.call(this, value);
      if (observed || (this.nodeType === 1 && this.querySelector('.out'))) {
        record('domWrite', {target: this.nodeName, property, dom: dom(), state: snapshot(window.jtml?.state || {}),
          detached: [...retained].filter(el => !el.isConnected).map(el => el.textContent.trim())});
      }
    }});
  }
  function intercept(key, attach) {
    let current;
    Object.defineProperty(window, key, {configurable: true, get: () => current, set(value) {
      current = value;
      attach(value);
    }});
  }
  let hooks = 0;
  const seen = new WeakSet();
  intercept('jtml', api => {
    if (!api?.actions || seen.has(api.actions)) return;
    seen.add(api.actions);
    const subscriptions = JSON.parse(document.querySelector('#__jtml_client_manifest').textContent).subscriptions || [];
    // These fixtures have one module. The independent watch list is in source order.
    if (subscriptions.length !== declarations.length) throw Error('Unexpected subscription inventory');
    for (const subscription of subscriptions) {
      const index = declarations.findIndex(item => item.action === subscription.action && item.watch === subscription.dependency);
      if (index < 0) throw Error('Cannot identify effect declaration');
      const {watch} = declarations[index];
      const label = 'E' + (index + 1);
      const action = api.actions[subscription.action];
      const body = action.body;
      Object.defineProperty(action, 'body', {configurable: true, enumerable: true, get() {
        record('effectEntry', {label, watch, value: api.state[watch], state: snapshot(api.state), dom: dom()});
        return body;
      }});
    }
    hooks = subscriptions.length;
    observeStorage(api.state);
  });
  function observeStorage(state) {
    for (const key of rootKeys) {
      const descriptor = Object.getOwnPropertyDescriptor(state, key);
      if (!descriptor || instrumented.has(key)) continue;
      instrumented.add(key);
      let value = descriptor.value;
      Object.defineProperty(state, key, {configurable: descriptor.configurable,
        enumerable: descriptor.enumerable, get: () => value, set(next) {
          value = next;
          record('storageWrite', {key, value: next, state: snapshot(state), dom: dom()});
        }});
    }
  }
  function wrap(table, key, kind) {
    const original = table[key];
    table[key] = function (instance, definition, ...args) {
      const scope = kind === 'update' ? args[1] : args[0];
      if (kind !== 'destroy') scopes.set(instance, scope);
      record(kind, {id: instance.id, generation: generation(instance), component: instance.component});
      return Reflect.apply(original, this, [instance, definition, ...args]);
    };
  }
  intercept('__jtml_static_update_functions', functions => {
    for (const key of Object.keys(functions)) wrap(functions, key, 'update');
  });
  intercept('__jtml_static_component_modules', modules => {
    for (const module of Object.values(modules)) {
      wrap(module, 'create', 'create');
      wrap(module, 'destroy', 'destroy');
    }
  });
  window.readObservation = () => {
    observeStorage(jtml.state);
    const instances = jtml.getComponentInstances().map(instance => {
      const {id, element} = instance;
      const observer = element?.querySelector('.out');
      if (observer) retained.add(observer);
      const scope = scopes.get(instance);
      return {id, generation: generation(instance), component: instance.component, local: scope?.local, twice: scope?.twice};
    });
    return {state: snapshot(jtml.state), dom: dom(), instances, hooks,
      destroyed: jtml.directComponentDestroyLifecycle?.entries || [], events: events.splice(0)};
  };
}

async function main() {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'jtml-core-'));
  const report = {cases: []};
  let browser, target;
  const server = http.createServer((request, response) => {
    if (request.url === '/favicon.ico') return response.writeHead(204).end();
    const file = path.resolve(target, '.' + new URL(request.url, 'http://localhost').pathname);
    if (!file.startsWith(target + path.sep) || !fs.existsSync(file)) return response.writeHead(404).end();
    response.writeHead(200, {'Content-Security-Policy': "script-src 'self'",
      'Content-Type': file.endsWith('.js') ? 'text/javascript' : 'text/html'});
    response.end(fs.readFileSync(file));
  });
  try {
    await new Promise((resolve, reject) => server.once('error', reject).listen(0, '127.0.0.1', resolve));
    assert(process.env.JTML_BENCH_BROWSER, 'Chromium required');
    browser = await chromium.launch({headless: true, executablePath: process.env.JTML_BENCH_BROWSER});
    report.chromium = browser.version();
    for (const vector of [...Object.keys(labels).map(id => catalog.find(item => item.id === id)), tie, fresh]) {
      const {id} = vector;
      const base = path.join(dir, id);
      fs.mkdirSync(base);
      for (const [name, source] of Object.entries(vector.files)) fs.writeFileSync(path.join(base, name), source);
      target = path.join(base, 'dist');
      const build = spawnSync(bin, ['build', path.join(base, vector.entry), '--target', 'browser', '--out', target],
        {encoding: 'utf8', timeout: 60000});
      assert.equal(build.status, 0, build.stdout + build.stderr);
      assert(!fs.existsSync(path.join(target, 'jtml-update-plans.js')));
      const asset = fs.readFileSync(path.join(target, 'jtml-runtime.js'));
      report.runtime = {raw: asset.length, gzip: gzipSync(asset, {level: 9}).length};
      assert(asset.length <= 273000);
      const initial = vector.expected_initial;
      const keys = Object.keys({...initial.state, ...initial.derived}).filter(key => key.split('::').length === 2);
      const rootKeys = keys.map(key => key.split('::')[1]);
      // The bounded fixtures are single-module, with unmangled scalar keys.
      // Friendly's generated key uses the line index after its dialect header.
      // Identify declarations independently of the emitted schedule order.
      const declarations = vector.files[vector.entry].split('\n').flatMap((line, index) => {
        const match = /^effect (\w+)$/.exec(line);
        return match ? [{watch: match[1], action: `__effect_${match[1]}_${index}`}] : [];
      });
      const page = await browser.newPage();
      const errors = [];
      page.on('pageerror', error => errors.push(error.message));
      page.on('console', message => { if (message.type() === 'error') errors.push(message.text()); });
      await page.addInitScript(install, {rootKeys, declarations});
      await page.goto(`http://127.0.0.1:${server.address().port}/index.html`);
      assert.equal(await page.evaluate(() => jtml.browserBuild.mode), 'prod');
      const row = {id, phases: [], errors};
      report.cases.push(row);
      const buttons = labels[id] || ['Step'];
      let previousDOM;
      for (const [index, expected] of [initial, ...vector.steps.map(step => step.expected)].entries()) {
        if (index) await page.getByRole('button', {name: buttons[index - 1], exact: true}).click({timeout: 4000});
        const actual = await page.evaluate(() => window.readObservation());
        assert.equal(actual.hooks, declarations.length, 'effect hooks installed');
        const values = Object.fromEntries(Object.entries(actual.state).map(([key, value]) => ['main::' + key, value]));
        for (const instance of actual.instances) {
          const {generation} = instance;
          values[generation + '::local'] = instance.local;
          values[generation + '::twice'] = instance.twice;
        }
        const project = oracle => Object.fromEntries(Object.keys(oracle).map(key => [key, values[key]]));
        const entries = actual.events.filter(event => event.kind === 'effectEntry');
        const show = actual.events.filter(event => event.kind === 'show').map(event => event.value);
        const writes = actual.events.filter(event => event.kind === 'domWrite');
        const checks = {};
        const compare = (name, observed, oracle) => {
          checks[name] = {status: isDeepStrictEqual(observed, oracle) ? 'PASS' : 'FAIL', observed, oracle};
        };
        compare('state', project(expected.state), expected.state);
        compare('derived', project(expected.derived), expected.derived);
        compare('dom', actual.dom, expected.dom['.out']);
        compare('effects', entries.map(e => e.label), expected.effects);
        compare('show', show, expected.show.map(String));
        compare('watchAtEffectEntry', entries.map(e => `${e.label}:main::${e.watch}=${e.value}`), expected.notifications);
        checks.notificationDelivery = {status: declarations.length ? 'UNVERIFIED' : 'PASS'};
        checks.logicalCommit = {status: 'UNVERIFIED'};
        checks.passiveObserverAtomicity = {status: 'UNVERIFIED'};
        if (index && (entries.length || show.length)) {
          compare('domDuringImperativeObservations', actual.events.filter(e =>
            e.kind === 'effectEntry' || e.kind === 'show').map(e => e.dom),
          Array.from({length: entries.length + show.length}, () => previousDOM));
        }
        if (index && id === 'V20') {
          compare('derivedAtEffectEntry', entries.map(e => e.state.twice), [expected.derived['main::twice']]);
        }
        if (id === 'V16' && !index) compare('initialCreate', actual.events.filter(e => e.kind === 'create')
          .map(e => e.generation), ['main::Card@1']);
        previousDOM = expected.dom['.out'];
        if (index && id !== 'V16') {
          compare('synchronousDOMWrites', writes.length > 0 && writes.every(e =>
            isDeepStrictEqual(e.dom, expected.dom['.out'])), true);
          compare('cacheAtDOMWrite', writes.length > 0 && writes.every(e =>
            keys.every(key => e.state[key.split('::')[1]] === values[key])), true);
        }
        if (id === 'V16') {
          const active = Object.keys({...expected.state, ...expected.derived})
            .filter(key => key.split('::').length === 3).map(key => key.split('::').slice(0, 2).join('::'));
          compare('activeGenerations', actual.instances.map(instance => instance.generation), [...new Set(active)]);
        }
        if (expected.destroyed) {
          const destroyed = actual.events.filter(e => e.kind === 'destroy');
          compare('destroyed', destroyed.map(e => e.generation), expected.destroyed);
          compare('registryScopeHandlerCleanup', destroyed.every(e =>
            actual.destroyed.some(entry => entry.id === e.id && entry.r && entry.s && entry.e && entry.d)), true);
        }
        if (expected.created) compare('created', actual.events.filter(e => e.kind === 'create')
          .map(e => e.generation), expected.created);
        if (expected.forbidden_notifications) {
          compare('generatedUpdatesToDestroyed', actual.events.filter(e => e.kind === 'update' &&
            expected.forbidden_notifications.includes(e.generation)), []);
          compare('domWritesWhileUnmounted', writes, []);
          checks.otherDestroyedDeliveries = {status: 'UNVERIFIED'};
        }
        row.phases.push({index, checks, actual});
      }
      await page.close();
    }
    // Clean production changed. The explicitly selected migration modes retain the
    // old cached-read behavior on the same counterexample.
    for (const mode of ['compat', 'debug']) {
      target = path.join(dir, mode);
      const result = spawnSync(bin, ['build', path.join(dir, fresh.id, fresh.entry),
        '--target', 'browser', '--out', target, '--' + mode], {encoding: 'utf8', timeout: 60000});
      assert.equal(result.status, 0, result.stdout + result.stderr);
      const page = await browser.newPage();
      const shown = [], errors = [];
      page.on('pageerror', error => errors.push(error.message));
      page.on('console', message => {
        if (message.type() === 'error') errors.push(message.text());
        if (message.text().startsWith('[SHOW] ')) shown.push(message.text().slice(7));
      });
      await page.goto(`http://127.0.0.1:${server.address().port}/index.html`);
      await page.getByRole('button', {name: 'Step', exact: true}).click({timeout: 4000});
      const dom = (await page.locator('.out').textContent()).trim();
      assert.equal(dom, '2|4|0');
      assert.deepEqual(shown, ['0']);
      assert.deepEqual(errors, []);
      (report.migrationModes ??= []).push({mode, dom, shown});
      await page.close();
    }
  } finally {
    if (browser) await browser.close();
    report.cleanBrowserExit = !!browser;
    if (server.listening) await new Promise(resolve => server.close(resolve));
    fs.rmSync(dir, {recursive: true, force: true});
    console.log(JSON.stringify(report, null, 2));
  }
  assert(report.cases.every(row => !row.errors.length && row.phases.every(phase =>
    Object.values(phase.checks).every(check => check.status !== 'FAIL'))), 'Reactive Core observation mismatch');
}

main().catch(error => { console.error(error); process.exitCode = 1; });
