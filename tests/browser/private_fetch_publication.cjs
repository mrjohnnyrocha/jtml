const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const os = require('node:os');
const path = require('node:path');
const {execFileSync} = require('node:child_process');
const {createRequire} = require('node:module');

const {chromium} = createRequire(path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT, 'package.json'))('playwright');
const fixture = path.resolve(__dirname, '../fixtures/production/private-fetch-publication');
const work = fs.mkdtempSync(path.join(os.tmpdir(), 'jtml-fetch-publication-'));
const domMessage = 'one-shot fetch DOM write failure';
const coercionPayload = {valueOf: null, toString: null};
let requests, gates;

const server = http.createServer((request, response) => {
  const pathname = new URL(request.url, 'http://localhost').pathname;
  if (pathname.startsWith('/api/')) {
    const name = pathname.slice(5);
    requests[name] = (requests[name] || 0) + 1;
    const send = value => {
      if (response.writableEnded || response.destroyed) return;
      response.writeHead(200, {'Content-Type': 'application/json', 'Cache-Control': 'no-store'});
      response.end(JSON.stringify(value));
    };
    if (name === 'tail' || requests[name] > 1) send(41);
    else gates[name] = send;
    return;
  }
  if (pathname === '/favicon.ico') return response.writeHead(204).end();
  const file = path.resolve(work, '.' + pathname);
  if (!file.startsWith(work + path.sep) || !fs.existsSync(file)) return response.writeHead(404).end();
  response.writeHead(200, {'Content-Security-Policy': "script-src 'self'; object-src 'none'",
    'Content-Type': file.endsWith('.js') ? 'text/javascript' : 'text/html'}).end(fs.readFileSync(file));
});

async function waitForGate(name) {
  for (let i = 0; i < 200 && !gates[name]; i++) await new Promise(resolve => setTimeout(resolve, 20));
  assert.equal(requests[name], 1);
  assert.ok(gates[name], `Missing ${name} response gate`);
}

const click = (page, name) => page.getByRole('button', {name, exact: true}).click();
const frames = page => page.evaluate(() => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve))));
const snapshot = page => page.evaluate(() => {
  const text = selector => document.querySelector(selector).textContent.trim();
  return {
    lifecycle: window.__jtml_test_failure?.(),
    source: Object.fromEntries(['job', 'side', 'later', 'tail', 'count', 'hits'].map(name => [name, jtml.state[name]])),
    value: jtml.state.value,
    dom: [text('.early'), text('.late'), text('.root-out'), text('.component').replace(/\s+/g, '')],
    component: jtml.getComponentInstances().find(instance => instance.component === 'Counter').scope.clicks,
    mutations: window.mutations || 0
  };
});

async function injectDomFault(page) {
  await page.evaluate(message => {
    const descriptor = Object.getOwnPropertyDescriptor(Node.prototype, 'textContent');
    Object.defineProperty(Node.prototype, 'textContent', {...descriptor, set(value) {
      if (this.parentElement?.closest('.late') && String(value) === '42') {
        Object.defineProperty(Node.prototype, 'textContent', descriptor);
        throw Error(message);
      }
      descriptor.set.call(this, value);
    }});
  }, domMessage);
}

async function run(browser, mode, fault) {
  requests = {};
  gates = {};
  const page = await browser.newPage();
  const errors = [], shows = [];
  page.on('pageerror', error => errors.push(error.message));
  page.on('console', message => { if (message.text().startsWith('[SHOW]')) shows.push(message.text()); });
  await page.addInitScript(() => {
    window.violations = [];
    document.addEventListener('securitypolicyviolation', event => violations.push(event.effectiveDirective));
  });
  try {
    await page.goto(`http://127.0.0.1:${server.address().port}/${mode}/index.html`);
    const componentButton = page.getByRole('button', {name: 'Component step', exact: true});
    assert.ok(await componentButton.getAttribute('data-jtml-direct-component-action'));
    await click(page, 'Root step');
    await click(page, 'Component step');
    if (mode === 'private' && fault) {
      await page.evaluate(() => {
        const result = name => jtml.refreshFetch(name).then(() => 'settled', error => error.message);
        window.sideResult = result('side');
        window.laterResult = result('later');
      });
      await waitForGate('side');
      await waitForGate('later');
    }
    await click(page, 'Load');
    await waitForGate('job');
    const before = await snapshot(page);
    assert.deepEqual(before.dom, ['0', '0', '1|1', '1Componentstep']);
    assert.equal(before.component, 1);
    assert.equal(before.lifecycle?.revision, mode === 'private' ? 3 : undefined);
    assert.equal(before.source.job.loading, true);
    if (fault === 'dom') await injectDomFault(page);

    const firstError = fault ? page.waitForEvent('pageerror', {timeout: 10000}) : null;
    gates.job(fault === 'derived' ? coercionPayload : 41);
    if (firstError) {
      const error = await firstError;
      assert.match(error.message, fault === 'dom' ? /one-shot fetch DOM write failure/ : /Cannot convert object to primitive value/);
    } else {
      await page.waitForFunction(() => jtml.state.tail?.ok);
    }
    const completed = await snapshot(page);
    assert.deepEqual(completed.source.job.data, fault === 'derived' ? coercionPayload : 41);
    assert.equal(completed.source.job.status, 200);
    assert.equal(completed.source.job.loading, false);
    assert.equal(completed.value, fault === 'derived' ? 0 : 42);
    assert.deepEqual(completed.dom, fault === 'derived' ? before.dom
      : [ '42', fault === 'dom' ? '0' : '42', '1|1', '1Componentstep']);

    if (fault && mode === 'private') {
      await page.evaluate(() => {
        window.mutations = 0;
        const observer = new MutationObserver(records => { mutations += records.length; });
        for (const selector of ['.early', '.late', '.root-out', '.component']) {
          observer.observe(document.querySelector(selector), {childList: true, subtree: true, characterData: true});
        }
      });
      // Keep the server gate closed while checking the timed request abort,
      // then release it to expose any late publication on the baseline.
      const beforeRelease = await page.evaluate(() => Promise.race([
        sideResult, new Promise(resolve => setTimeout(() => resolve('pending'), 3000))
      ]));
      gates.side(23);
      gates.later(99);
      const [sideResult, laterResult] = await page.evaluate(() => Promise.all([window.sideResult, window.laterResult]));
      await click(page, 'Component step');
      await click(page, 'Root step');
      await page.evaluate(() => jtml.refreshFetch('job'));
      await frames(page);
      const after = await snapshot(page);
      console.log(`${mode} ${fault} ${JSON.stringify({status: completed.lifecycle.status,
        beforeRevision: before.lifecycle.revision, afterRevision: after.lifecycle.revision,
        domAtFault: completed.dom, domAfterDispatch: after.dom, beforeRelease, sideResult, laterResult, requests, errors})}`);

      assert.equal(completed.lifecycle.status, 'FAILED');
      assert.equal(completed.lifecycle.revision, before.lifecycle.revision);
      assert.equal(completed.lifecycle.fault.invocation.kind, 'fetch');
      assert.equal(completed.lifecycle.fault.invocation.name, 'job');
      assert.deepEqual(completed.lifecycle.fault.failedInvocation, completed.lifecycle.fault.invocation);
      assert.equal(completed.lifecycle.fault.publicationEntered, fault === 'dom');
      assert.equal(completed.lifecycle.fault.message, errors[0]);
      assert.equal(completed.lifecycle.fault.code, null);
      assert.equal(completed.lifecycle.fault.site, null);
      assert.deepEqual(completed.lifecycle.pendingWrites, []);
      assert.deepEqual(completed.lifecycle.partial.root, completed.source);
      assert.equal(completed.source.side.loading, true);
      assert.equal(completed.source.later.loading, true);
      assert.equal(beforeRelease, 'settled');
      assert.equal(sideResult, 'settled');
      assert.equal(laterResult, 'settled');
      assert.deepEqual(after, completed);
      assert.deepEqual(requests, {side: 1, later: 1, job: 1});
      assert.deepEqual(shows, ['[SHOW] 1']);
      assert.equal(errors.length, 1);

      await page.reload();
      assert.equal((await snapshot(page)).lifecycle.status, 'ACTIVE');
      assert.equal((await snapshot(page)).lifecycle.revision, 0);
      await click(page, 'Load');
      await page.waitForFunction(() => jtml.state.tail?.ok);
      assert.deepEqual((await snapshot(page)).dom, ['42', '42', '0|0', '0Componentstep']);
      assert.deepEqual(requests, {side: 1, later: 1, job: 2, tail: 1});
    } else {
      if (fault) await page.evaluate(() => jtml.refreshFetch('job'));
      await click(page, 'Root step');
      await click(page, 'Component step');
      await frames(page);
      const after = await snapshot(page);
      assert.deepEqual(after.dom, ['42', '42', '2|2', '2Componentstep']);
      assert.equal(after.lifecycle?.status, mode === 'private' ? 'ACTIVE' : undefined);
      assert.deepEqual(requests, fault ? {job: 2} : {job: 1, tail: 1});
      assert.deepEqual(shows, ['[SHOW] 1', '[SHOW] 2']);
      assert.equal(errors.length, fault ? 1 : 0);
      console.log(`PASS ${mode} ${fault || 'healthy'} control`);
    }
    assert.deepEqual(await page.evaluate(() => violations), []);
  } finally {
    for (const send of Object.values(gates)) send(41);
    await page.close();
  }
}

(async () => {
  for (const [mode, compiler] of [['full', process.env.JTML_REAL_BIN], ['private', process.env.JTML_BIN]]) {
    execFileSync(compiler, ['build', 'index.jtml', '--target', 'browser', '--out', path.join(work, mode)],
      {cwd: fixture, timeout: 30000});
  }
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const browser = await chromium.launch({executablePath: process.env.JTML_BENCH_BROWSER});
  try {
    const failures = [];
    for (const [mode, fault] of [['full', null], ['full', 'dom'], ['full', 'derived'],
      ['private', null], ['private', 'dom'], ['private', 'derived']]) {
      try { await run(browser, mode, fault); }
      catch (error) { failures.push(error); }
    }
    if (failures.length) throw new AggregateError(failures, 'Private fetch publication ownership');
  } finally {
    await browser.close();
  }
})().catch(error => { console.error(error); process.exitCode = 1; }).finally(async () => {
  if (server.listening) await new Promise(resolve => server.close(resolve));
  fs.rmSync(work, {recursive: true, force: true});
});
