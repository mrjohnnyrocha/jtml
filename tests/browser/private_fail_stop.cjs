const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const os = require('node:os');
const path = require('node:path');
const {spawnSync} = require('node:child_process');
const {createRequire} = require('node:module');

const repo = path.resolve(__dirname, '../..');
const fixtures = path.join(repo, 'tests/fixtures/production');
const {chromium} = createRequire(path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT, 'package.json'))('playwright');
const work = fs.mkdtempSync(path.join(os.tmpdir(), 'jtml-private-fail-stop-'));
const sources = path.join(work, 'sources');
fs.cpSync(path.join(fixtures, 'runtime-fault-attribution'), sources, {recursive: true});
const owners = path.join(sources, 'owners.jtml');
// Preserve the original effect and keyed-loop coordinates. Add a later watch
// and an immediate show so the test can distinguish pending and delivered work.
fs.writeFileSync(owners, fs.readFileSync(owners, 'utf8').replace(
  '  watched = value', '  watched = value\n  show value') +
  '\nlet later = 0\neffect watched\n  later += 1\n');
// Keep the original keyed fault fixture itself for the component case.
for (const [mode, compiler] of [['full', process.env.JTML_REAL_BIN], ['private', process.env.JTML_BIN]]) {
  for (const [fixture, source] of Object.entries({
    effect: path.join(sources, 'index.jtml'),
    keyed: path.join(fixtures, 'runtime-fault-attribution/index.jtml'),
    nested: path.join(fixtures, 'private-fail-stop/index.jtml'),
  })) {
    const r = spawnSync(compiler, ['build', path.basename(source), '--target', 'browser',
      '--out', path.join(work, mode, fixture)],
      {cwd: path.dirname(source), encoding: 'utf8', timeout: 30000});
    assert.equal(r.status, 0, r.stdout + r.stderr);
  }
}

const server = http.createServer((request, response) => {
  if (request.url === '/favicon.ico') return response.writeHead(204).end();
  const file = path.resolve(work, '.' + new URL(request.url, 'http://localhost').pathname);
  if (!file.startsWith(work + path.sep) || !fs.existsSync(file)) return response.writeHead(404).end();
  response.writeHead(200, {'Content-Security-Policy': "script-src 'self'; object-src 'none'",
    'Content-Type': file.endsWith('.js') ? 'text/javascript' : 'text/html'}).end(fs.readFileSync(file));
});

async function open(browser, mode, fixture, test) {
  const page = await browser.newPage();
  const errors = [], shows = [];
  page.on('pageerror', error => errors.push(error.message));
  page.on('console', message => {if (message.text().startsWith('[SHOW]')) shows.push(message.text());});
  await page.addInitScript(() => {
    window.violations = [];
    document.addEventListener('securitypolicyviolation', event => violations.push(event.effectiveDirective));
  });
  const click = name => page.getByRole('button', {name, exact: true}).click();
  const view = async selector => (await page.locator(selector).textContent()).trim();
  const lifecycle = () => page.evaluate(() => window.__jtml_test_failure?.());
  try {
    await page.goto(`http://127.0.0.1:${server.address().port}/${mode}/${fixture}/index.html`);
    await test({page, errors, shows, click, view, lifecycle});
    assert.deepEqual(await page.evaluate(() => violations), []);
  } finally {
    await page.close();
  }
}

// Dispatch a browser event through the genuine component-to-root fallback
// using a mounted component. Also exercise a stale ID independently.
async function dispatchFallback(page, stale = false) {
  await page.evaluate(staleId => {
    const button = document.querySelector('.trigger button').cloneNode();
    button.setAttribute('data-jtml-direct-component-action', 'rootStep');
    if (staleId) button.setAttribute('data-jtml-direct-component-id', 'stale-id');
    document.body.append(button);
    button.click();
    button.remove();
  }, stale);
}

async function oneFault(page, errors, pattern) {
  await page.waitForFunction(() => window.__jtml_test_failure?.().status === 'FAILED', null, {timeout: 4000});
  await page.evaluate(() => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve))));
  assert.equal(errors.length, 1);
  assert.match(errors[0], pattern);
}

(async () => {
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const browser = await chromium.launch({executablePath: process.env.JTML_BENCH_BROWSER});
  try {
    await open(browser, 'full', 'effect', async ({page, errors, click, view, lifecycle}) => {
      assert.equal(await lifecycle(), undefined);
      await click('Invalid write');
      assert.equal(await view('.effect-out'), '0|0');
      await click('Valid write');
      assert.equal(await view('.effect-out'), '7|1');
      assert.equal(await page.evaluate(() => jtml.state.later), 1);
      assert.equal(errors.length, 1);
      assert.match(errors[0], /owners\.jtml:6:1: E_EFFECT_SCALAR_WRITE/);
      console.log('Default effect recovery: 0|0 after fault, 7|1 after valid write');
    });
    await open(browser, 'full', 'keyed', async ({errors, click, view}) => {
      await click('Duplicate');
      assert.equal(await view('.rows'), 'ab');
      await click('Repair');
      assert.equal(await view('.rows'), 'ba');
      assert.equal(errors.length, 1);
      assert.match(errors[0], /owners\.jtml:20:7: E_DUPLICATE_KEYED_ROW/);
      console.log('Default keyed recovery: ab after fault, ba after Repair');
    });
    await open(browser, 'full', 'nested', async ({page, errors, click, view}) => {
      await click('Launch');
      assert.equal(await view('.rows'), 'ab');
      assert.equal(await view('.root-out'), '1'); // Existing delegated root click.
      await click('Repair nested');
      assert.equal(await view('.rows'), 'ba');
      await dispatchFallback(page, true);
      assert.equal(await view('.root-out'), '2');
      await dispatchFallback(page);
      assert.equal(await view('.root-out'), '3');
      await page.getByRole('textbox', {name: 'Draft'}).fill('accepted');
      assert.equal(await view('.draft'), 'accepted');
      await click('Sibling step');
      assert.equal(await page.evaluate(() => jtml.getComponentInstances()
        .find(instance => instance.component === 'Sibling').scope.clicks), 1);
      assert.equal(errors.length, 1);
      assert.match(errors[0], /E_DUPLICATE_KEYED_ROW/);
      console.log('Default nested recovery: Repair, component-to-root fallback, input, and sibling action work');
    });
    await open(browser, 'private', 'effect', async ({page, errors, shows, click, view, lifecycle}) => {
      // A synchronous host callback during a healthy patch cannot enter a
      // second root action. The property is restored before the native write.
      await page.evaluate(() => {
        const descriptor = Object.getOwnPropertyDescriptor(Node.prototype, 'textContent');
        Object.defineProperty(Node.prototype, 'textContent', {...descriptor, set(value) {
          if (this.parentElement?.closest('.effect-out')) {
            Object.defineProperty(Node.prototype, 'textContent', descriptor);
            window.reentry = jtml.runAction('writeCount', 99);
          }
          descriptor.set.call(this, value);
        }});
      });
      await click('Valid write');
      assert.equal(await page.evaluate(() => reentry), false);
      assert.equal(await view('.effect-out'), '7|1');
      const previous = await lifecycle();
      assert.equal(previous.revision, 1);
      await click('Invalid write');
      await oneFault(page, errors, /owners\.jtml:6:1: E_EFFECT_SCALAR_WRITE/);
      const failed = await lifecycle();
      assert.deepEqual(failed.partial.root, {watched: [1, 2], hits: 1, later: 1});
      assert.equal(failed.fault.invocation.action, 'writeCount');
      assert.equal(failed.fault.code, 'E_EFFECT_SCALAR_WRITE');
      assert.equal(failed.fault.site, 'owners.jtml:6:1');
      assert.equal(failed.fault.failedInvocation.kind, 'effect');
      assert.equal(await page.evaluate(() => Object.hasOwn(jtml.state, 'value')), false);
      assert.equal(failed.fault.publicationEntered, false);
      assert.equal(failed.revision, previous.revision);
      assert.deepEqual(failed.pendingWrites, []);
      assert.equal(await view('.effect-out'), '7|1');
      assert.deepEqual(shows, ['[SHOW] 7', '[SHOW] [1,2]']);
      assert.equal(await page.evaluate(() => jtml.runAction('writeCount', 11)), false);
      await click('Valid write');
      assert.deepEqual(await lifecycle(), failed);
      assert.equal(errors.length, 1);
      console.log('Private root effect:', errors[0], JSON.stringify(failed.partial.root), 'DOM 7|1, revision 1');
      await page.reload();
      assert.equal((await lifecycle()).status, 'ACTIVE');
      assert.equal((await lifecycle()).revision, 0);
      assert.equal((await lifecycle()).fault, null);
      assert.equal(await view('.effect-out'), '0|0');
    });
    await open(browser, 'private', 'keyed', async ({page, errors, click, view, lifecycle}) => {
      await page.evaluate(() => { window.firstKeyedRow = document.querySelector('.rows [data-jtml-direct-list-key]'); });
      await click('Duplicate');
      await oneFault(page, errors, /owners\.jtml:20:7: E_DUPLICATE_KEYED_ROW/);
      const failed = await lifecycle();
      assert.equal(failed.fault.publicationEntered, true);
      assert.equal(failed.revision, 0);
      assert.deepEqual(failed.partial.components.find(c => c.component === 'Board').state.items, ['a', 'a', 'c']);
      assert.equal(await view('.rows'), 'ab');
      assert.equal(await page.evaluate(() => !!firstKeyedRow &&
        firstKeyedRow === document.querySelector('.rows [data-jtml-direct-list-key]')), true);
      await click('Repair');
      await click('Valid write');
      assert.deepEqual(await lifecycle(), failed);
      assert.equal(errors.length, 1);
      console.log('Private keyed:', errors[0], 'retained a,a,c, DOM ab, revision 0');
    });
    await open(browser, 'private', 'nested', async ({page, errors, click, view, lifecycle}) => {
      // This window listener runs after the component's document listener
      // and attempts another entry in the same browser event.
      await page.evaluate(() => window.addEventListener('click', () => {
        window.lateClickEntry = jtml.runAction('rootStep');
      }, {once: true}));
      await click('Launch');
      await oneFault(page, errors, /E_DUPLICATE_KEYED_ROW/);
      const failed = await lifecycle();
      const scope = name => failed.partial.components.find(c => c.component === name).state;
      assert.equal(failed.fault.invocation.action, 'launch');
      assert.equal(failed.fault.failedInvocation.action, 'breakRows');
      assert.equal(failed.fault.publicationEntered, true);
      // The existing root delegate runs before the component listener.
      // Its completed publication counts; the failed component episode does not.
      assert.equal(failed.revision, 1);
      assert.equal(await page.evaluate(() => lateClickEntry), false);
      assert.deepEqual(failed.pendingWrites, []);
      assert.deepEqual(scope('Parent'), {items: ['a', 'a', 'c'], received: 3});
      assert.equal(scope('Trigger').after, 0);
      assert.equal(failed.partial.root.roots, 1);
      assert.equal(await view('.rows'), 'ab');
      assert.equal(await view('.root-out'), '1');
      const input = page.getByRole('textbox', {name: 'Draft'});
      await input.fill('rejected');
      await dispatchFallback(page, true);
      await dispatchFallback(page);
      await click('Sibling step');
      await click('Root step');
      await click('Repair nested');
      assert.deepEqual(scope('Sibling'), {draft: 'seed', clicks: 0});
      assert.equal(await view('.draft'), 'seed');
      assert.deepEqual(await lifecycle(), failed);
      assert.equal(errors.length, 1);
      const leaked = await page.evaluate(() => jtml.getComponentInstances()
        .some(instance => Object.hasOwn(instance.scope || {}, 'payload')));
      assert.equal(leaked, false);
      console.log('Private nested:', errors[0], 'launch -> breakRows, input and fallback denied');
    });
    console.log('PASS private fail-stop, root and keyed failures, nested emit, input, frame cleanup, reentry, reload, and default recovery controls');
  } finally {
    await browser.close();
  }
})().catch(error => {console.error(error); process.exitCode = 1;}).finally(async () => {
  if (server.listening) await new Promise(resolve => server.close(resolve));
  fs.rmSync(work, {recursive: true, force: true});
});
