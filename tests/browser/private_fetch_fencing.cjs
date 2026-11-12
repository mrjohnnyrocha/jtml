const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const os = require('node:os');
const path = require('node:path');
const {execFileSync} = require('node:child_process');
const {createRequire} = require('node:module');

const {chromium} = createRequire(path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT, 'package.json'))('playwright');
const fixture = path.resolve(__dirname, '../fixtures/production/private-fetch-fencing');
const work = fs.mkdtempSync(path.join(os.tmpdir(), 'jtml-fetch-fencing-'));
const csp = "script-src 'self'; object-src 'none'";
let requests;
let hold;

const server = http.createServer((request, response) => {
  const pathname = new URL(request.url, 'http://localhost').pathname;
  if (pathname.startsWith('/api/')) {
    const name = pathname.slice(5);
    requests[name] = (requests[name] || 0) + 1;
    const gate = hold?.name === name && requests[name] === 1 ? hold : null;
    const status = gate?.status || (name === 'plain' ? 503 : 200);
    const send = () => response.end('{"value":42}');
    const headers = () => response.writeHead(status, {'Content-Type': 'application/json', 'Cache-Control': 'no-store'});
    if (!gate) {
      headers();
      return send();
    }
    if (gate.body) {
      headers();
      response.write('{"value":');
    }
    gate.release = () => {
      if (response.writableEnded || response.destroyed) return;
      if (gate.disconnect) return response.destroy();
      if (gate.body) response.end('42}');
      else { headers(); send(); }
    };
    return;
  }
  if (pathname === '/favicon.ico') return response.writeHead(204).end();
  const file = path.resolve(work, '.' + pathname);
  if (!file.startsWith(work + path.sep) || !fs.existsSync(file)) return response.writeHead(404).end();
  response.writeHead(200, {'Content-Security-Policy': csp,
    'Content-Type': file.endsWith('.js') ? 'text/javascript' : 'text/html'}).end(fs.readFileSync(file));
});

const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
async function waitForRequest(name) {
  for (let i = 0; i < 200 && !requests[name]; i++) await delay(20);
  assert.equal(requests[name], 1, JSON.stringify(requests));
}

async function open(browser, mode, test, gate = null) {
  requests = {};
  hold = gate;
  const page = await browser.newPage();
  const errors = [], shows = [];
  page.on('pageerror', error => errors.push(error.message));
  page.on('console', message => { if (message.text().startsWith('[SHOW]')) shows.push(message.text()); });
  await page.addInitScript(() => {
    window.probe = {violations: [], bodyStarted: 0, bodyFinished: 0, publications: 0, derivedWrites: 0};
    document.addEventListener('securitypolicyviolation', event => probe.violations.push(event.effectiveDirective));
    const parse = Response.prototype.json;
    Response.prototype.json = async function () {
      if (!this.url.includes('/api/')) return parse.call(this);
      probe.bodyStarted++;
      try { return await parse.call(this); }
      finally { probe.bodyFinished++; }
    };
  });
  try {
    await page.goto(`http://127.0.0.1:${server.address().port}/${mode}/index.html`);
    await test(page, errors, shows);
    assert.deepEqual(await page.evaluate(() => probe.violations), []);
  } finally {
    hold?.release?.();
    await page.close();
  }
}

const click = (page, name) => page.getByRole('button', {name, exact: true}).click();
async function triggerFault(page) {
  // Playwright can deliver pageerror after the in-page latch is observable.
  const [error] = await Promise.all([
    page.waitForEvent('pageerror', {timeout: 10000}),
    click(page, 'Fault')
  ]);
  assert.match(error.message, /index\.jtml:10:1: E_EFFECT_SCALAR_WRITE/);
}

const snapshot = page => page.evaluate(() => ({
  lifecycle: window.__jtml_test_failure?.(),
  source: {job: jtml.state.job, plain: jtml.state.plain, tail: jtml.state.tail, ticker: jtml.state.ticker, timed: jtml.state.timed},
  summary: jtml.state.summary,
  fetchDom: document.querySelector('.fetch-out').textContent.trim(),
  effectDom: document.querySelector('.effect-out').textContent.trim(),
  watched: jtml.state.watched, hits: jtml.state.hits,
  status: document.documentElement.dataset.jtmlStatus,
  publications: probe.publications, derivedWrites: probe.derivedWrites
}));

async function fault(page, errors, shows, name, revision = 1) {
  await click(page, 'Valid');
  await triggerFault(page);
  await page.waitForFunction(() => __jtml_test_failure().status === 'FAILED');
  const failed = await snapshot(page);
  assert.equal(failed.lifecycle.revision, revision);
  assert.equal(failed.lifecycle.fault.code, 'E_EFFECT_SCALAR_WRITE');
  assert.equal(failed.lifecycle.fault.site, 'index.jtml:10:1');
  assert.equal(failed.lifecycle.fault.publicationEntered, false);
  assert.deepEqual(failed.lifecycle.partial.root, {...failed.source, watched: [1, 2], hits: 1});
  assert.deepEqual(failed.lifecycle.pendingWrites, []);
  assert.deepEqual(failed.watched, [1, 2]);
  assert.equal(failed.hits, 1);
  assert.equal(failed.effectDom, '7|1');
  assert.deepEqual(shows, ['[SHOW] 7']);
  assert.equal(errors.length, 1);
  assert.match(errors[0], /index\.jtml:10:1: E_EFFECT_SCALAR_WRITE/);
  if (name) {
    const {loading, data, error, attempts, status, updatedAt} = failed.source[name];
    assert.deepEqual({loading, data, error, attempts, status, updatedAt},
      {loading: true, data: [], error: '', attempts: 0, status: 0, updatedAt: 0});
  }
  if (name === 'job') {
    assert.equal(failed.summary, 'true|0|0');
    assert.equal(failed.fetchDom, 'true|0|0');
  }
  // The browser reports writes to the real DOM, including idempotent rewrites.
  await page.evaluate(() => {
    let summary = jtml.state.summary;
    Object.defineProperty(jtml.state, 'summary', {configurable: true, enumerable: true,
      get: () => summary,
      set: value => { probe.derivedWrites++; summary = value; }
    });
    const observer = new MutationObserver(records => { probe.publications += records.length; });
    observer.observe(document.querySelector('.fetch-out'), {childList: true, subtree: true, characterData: true});
    observer.observe(document.querySelector('.effect-out'), {childList: true, subtree: true, characterData: true});
  });
  return failed;
}

async function remainsFailed(page, errors, shows, failed, counts) {
  await click(page, 'Valid');
  assert.equal(await page.evaluate(() => jtml.runAction('writeCount', 8)), false);
  await page.evaluate(() => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve))));
  assert.deepEqual(await snapshot(page), failed);
  assert.deepEqual(requests, counts);
  assert.deepEqual(shows, ['[SHOW] 7']);
  assert.equal(errors.length, 1);
}

const cases = [
  ['success with headers held, invalidation chain', {name: 'job', status: 200}, true],
  ['HTTP error as data after failure', {name: 'plain', status: 503}, false],
  ['body parsing already in progress', {name: 'job', status: 200, body: true}, false],
  ['HTTP error cannot start retry', {name: 'job', status: 503}, false],
  ['interrupted body cannot start retry', {name: 'job', body: true, disconnect: true}, false],
];

async function held(browser, label, gate, chain) {
  await open(browser, 'private', async (page, errors, shows) => {
    await page.evaluate(({name, chain}) => {
      window.operation = chain
        ? sendEvent('test', 'onclick', ['refreshChain'])
        : jtml.refreshFetch(name);
    }, {name: gate.name, chain});
    await waitForRequest(gate.name);
    if (gate.body) {
      await page.waitForFunction(() => probe.bodyStarted === 1);
      assert.equal(await page.evaluate(() => probe.bodyFinished), 0);
    } else {
      assert.equal(await page.evaluate(() => probe.bodyStarted), 0);
    }
    const failed = await fault(page, errors, shows, gate.name, chain ? 2 : 1);
    gate.release();
    await page.evaluate(() => operation);
    const settled = await snapshot(page);
    console.log(label, JSON.stringify({requests, before: failed.fetchDom, after: settled.fetchDom,
      loading: settled.source[gate.name].loading, status: settled.source[gate.name].status,
      revision: settled.lifecycle.revision, derivedWrites: settled.derivedWrites, domWrites: settled.publications}));
    await remainsFailed(page, errors, shows, failed, {[gate.name]: 1});
    if (gate.body) assert.equal(await page.evaluate(() => probe.bodyFinished), 1);
    else assert.equal(await page.evaluate(() => probe.bodyStarted), 0);
    console.log(`PASS ${label} | one request, loading retained, DOM and revision fixed, original diagnostic retained`);
  }, gate);
}

async function publicationReentry(browser) {
  const gate = {name: 'job'};
  await open(browser, 'private', async (page, errors, shows) => {
    await page.evaluate(() => { window.operation = jtml.refreshFetch('job'); });
    await waitForRequest('job');
    await page.evaluate(() => {
      const descriptor = Object.getOwnPropertyDescriptor(Node.prototype, 'textContent');
      Object.defineProperty(Node.prototype, 'textContent', {...descriptor, set(value) {
        if (this.parentElement?.closest('.fetch-out')) {
          Object.defineProperty(Node.prototype, 'textContent', descriptor);
          try { window.reentry = jtml.runAction('writeCount', [1, 2]); }
          catch (error) { window.reentry = error.message; }
        }
        descriptor.set.call(this, value);
      }});
    });
    gate.release();
    await page.evaluate(() => operation);
    assert.equal(await page.evaluate(() => reentry), false);
    const settled = await snapshot(page);
    assert.equal(settled.lifecycle.status, 'ACTIVE');
    assert.equal(settled.lifecycle.revision, 0);
    assert.equal(settled.fetchDom, 'false|200|1');
    assert.equal(settled.effectDom, '0|0');
    assert.deepEqual(settled.source.job.data, {value: 42});
    assert.deepEqual(requests, {job: 1});
    assert.deepEqual(shows, []);
    assert.deepEqual(errors, []);
    console.log('PASS healthy fetch publication rejects synchronous host reentry');
  }, gate);
}

async function timedAbort(browser) {
  const gate = {name: 'timed'};
  await open(browser, 'private', async (page, errors, shows) => {
    await page.evaluate(() => { window.operation = jtml.refreshFetch('timed'); });
    await waitForRequest('timed');
    const failed = await fault(page, errors, shows, 'timed');
    // Keep the server gate closed. The existing controller can settle the
    // browser promise without depending on server-side cancellation.
    assert.equal(await page.evaluate(() => Promise.race([
      operation.then(() => 'settled'), new Promise(resolve => setTimeout(() => resolve('pending'), 3000))
    ])), 'settled');
    assert.equal(await page.evaluate(() => probe.bodyStarted), 0);
    await remainsFailed(page, errors, shows, failed, {timed: 1});
    console.log('PASS timed request abort settles without publication or retry');
  }, gate);
}

async function timerAndReload(browser) {
  await open(browser, 'private', async (page, errors, shows) => {
    // Advance Chromium's clock for real setInterval callbacks without waiting
    // a minute. HTTP requests and their completion still go through the server.
    await page.clock.install();
    await page.reload();
    assert.deepEqual(requests, {});
    await page.clock.fastForward(60000);
    await waitForRequest('ticker');
    await page.waitForFunction(() => jtml.state.ticker?.ok);
    assert.equal(await page.evaluate(() => Object.keys(jtml.fetchTimers).length), 1);
    const failed = await fault(page, errors, shows);
    await click(page, 'Refresh');
    await page.evaluate(async () => {
      await jtml.refreshFetch('job');
      await jtml.refreshFetch('missing');
      await jtml.fetches.job(true);
      await sendEvent('test', 'onclick', ['refreshChain']);
    });
    await page.clock.fastForward(180000);
    console.log('post-fault refresh and timer counts', JSON.stringify(requests));
    await remainsFailed(page, errors, shows, failed, {ticker: 1});
    assert.deepEqual(await page.evaluate(() => Object.keys(jtml.fetchTimers)), []);
    console.log('PASS explicit refresh, raw fetch dispatch and revalidation denied after failure');

    await page.reload();
    await click(page, 'Refresh');
    await waitForRequest('job');
    await page.waitForFunction(() => jtml.state.job?.ok);
    const recovered = await snapshot(page);
    assert.equal(recovered.lifecycle.status, 'ACTIVE');
    assert.equal(recovered.lifecycle.revision, 0);
    assert.equal(recovered.lifecycle.fault, null);
    assert.equal(recovered.fetchDom, 'false|200|1');
    assert.equal(recovered.effectDom, '0|0');
    assert.deepEqual(recovered.source.job.data, {value: 42});
    assert.deepEqual(requests, {ticker: 1, job: 1});
    console.log('PASS reload restores ACTIVE fetching and initial effects');
  });
}

async function recoveryControl(browser, mode) {
  const gate = {name: 'job', status: 503};
  await open(browser, mode, async (page, errors, shows) => {
    await page.evaluate(() => { window.operation = sendEvent('test', 'onclick', ['refreshChain']); });
    await waitForRequest('job');
    if (mode === 'full') {
      assert.equal(await page.evaluate(() => window.__jtml_test_failure), undefined);
      await triggerFault(page);
    }
    gate.release();
    await page.evaluate(() => operation);
    const settled = await snapshot(page);
    assert.deepEqual(requests, {job: 2, tail: 1});
    assert.equal(settled.fetchDom, 'false|200|2');
    assert.deepEqual(settled.source.job.data, {value: 42});
    assert.deepEqual(settled.source.tail.data, {value: 42});
    await page.evaluate(() => jtml.fetches.job(false));
    assert.deepEqual(requests, {job: 2, tail: 1}, 'healthy cache deduplication still works');
    await page.evaluate(() => jtml.refreshFetch('plain'));
    const {loading, status, error, attempts, ok} = (await snapshot(page)).source.plain;
    assert.deepEqual({loading, status, error, attempts, ok},
      {loading: false, status: 503, error: '503 Service Unavailable', attempts: 1, ok: false});
    assert.deepEqual(requests, {job: 2, tail: 1, plain: 1});
    await assert.rejects(page.evaluate(() => jtml.refreshFetch('missing')), /Unknown JTML fetch/);
    await click(page, 'Valid');
    assert.equal((await snapshot(page)).effectDom, '7|1');
    assert.deepEqual(shows, ['[SHOW] 7']);
    assert.equal(errors.length, mode === 'full' ? 1 : 0);
    console.log(`PASS ${mode} control retries, continues invalidation, deduplicates and dispatches`);
  }, gate);
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
    for (const test of [
      () => recoveryControl(browser, 'full'),
      () => recoveryControl(browser, 'private'),
      ...cases.map(([label, gate, chain]) => () => held(browser, label, gate, chain)),
      () => timedAbort(browser),
      () => publicationReentry(browser),
      () => timerAndReload(browser)
    ]) {
      try { await test(); }
      catch (error) { failures.push(error); }
    }
    if (failures.length) throw new AggregateError(failures, 'Private fetch fencing');
  } finally {
    await browser.close();
  }
})().catch(error => { console.error(error); process.exitCode = 1; }).finally(async () => {
  if (server.listening) await new Promise(resolve => server.close(resolve));
  fs.rmSync(work, {recursive: true, force: true});
});
