// Real browser origins, live WebSocket events, Studio srcdoc and CSRF denial.
const assert = require('node:assert/strict');
const http = require('node:http');
const path = require('node:path');
const {chromium} = require(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT
  ? path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT, 'node_modules/playwright') : 'playwright');
const port = Number(process.argv[2]), mode = process.argv[3], event = JSON.parse(process.argv[4]);
const code = 'jtml 2\nlet count = 0\nwhen increment\n  count += 1\npage\n  button "Increment" click increment\n  show count\n';
(async () => {
  const browser = await chromium.launch({headless:true, executablePath:process.env.JTML_BENCH_BROWSER || undefined});
  const attacker = http.createServer((req,res) => res.end('<!doctype html><title>untrusted origin</title>'));
  await new Promise(resolve => attacker.listen(0, '127.0.0.1', resolve));
  try {
    for (const host of ['localhost', '127.0.0.1']) {
      const page = await browser.newPage();
      // Exercise Studio's HTTP origin and srcdoc transport without the optional
      // editor CDN or its automatic welcome-program reload racing this fixture.
      let populated = false, eventFallbacks = 0;
      page.on('request', request => {
        if (request.method() === 'POST' && new URL(request.url()).pathname === '/api/event') eventFallbacks += 1;
      });
      page.on('websocket', ws => ws.on('framereceived', frame => {
        try { if (JSON.parse(String(frame.payload)).type === 'populateBindings') populated = true; } catch {}
      }));
      await page.goto(`http://${host}:${port}/${mode === 'studio' ? 'api/lessons' : ''}`);
      if (mode === 'studio') await page.setContent('<!doctype html><body>Studio transport oracle</body>');
      let view = page;
      if (mode === 'studio') {
        const result = await page.evaluate(async code => {
          const response = await fetch('/api/run', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({code})});
          const result = await response.json();
          // A transport oracle independent of the optional Monaco/editor startup.
          // Match Studio's documented srcdoc sandbox and inherited origin.
          const frame = document.createElement('iframe');
          frame.id = 'security-preview';
          frame.setAttribute('sandbox', 'allow-scripts allow-same-origin allow-forms');
          frame.style.cssText = 'position:fixed;inset:0;width:700px;height:500px;z-index:9999;background:white';
          frame.srcdoc = result.html;
          document.body.appendChild(frame);
          return result.ok;
        }, code);
        assert.equal(result, true);
        view = page.frameLocator('#security-preview');
      }
      await view.getByRole('button', {name:'Increment', exact:true}).waitFor();
      const deadline = Date.now() + 10000;
      while (!populated) {
        assert(Date.now() < deadline, 'browser live WebSocket did not receive bindings');
        await page.waitForTimeout(25);
      }
      const readCounter = () => page.evaluate(port => new Promise((resolve,reject) => {
        const ws = new WebSocket(`ws://${location.hostname}:${port + 80}`);
        ws.onerror = () => reject(new Error('trusted browser WebSocket denied'));
        ws.onmessage = event => { const msg = JSON.parse(event.data); if(msg.type === 'populateBindings') {ws.close();resolve(msg.bindings.state.count);} };
      }), port);
      const before = await readCounter();
      await view.getByRole('button', {name:'Increment', exact:true}).click();
      while (await readCounter() !== before + 1) {
        assert(Date.now() < deadline, 'browser event did not increment exactly once');
        await page.waitForTimeout(25);
      }
      assert.equal(eventFallbacks, 1, 'the existing live browser HTTP event workflow changed');
      await page.evaluate(({port,event,expected}) => new Promise((resolve,reject) => {
        const ws = new WebSocket(`ws://${location.hostname}:${port + 80}`);
        const timer = setTimeout(() => {ws.close();reject(new Error('browser WebSocket event timed out'));}, 10000);
        ws.onerror = () => {clearTimeout(timer);reject(new Error('browser WebSocket event denied'));};
        ws.onmessage = response => {
          const msg = JSON.parse(response.data);
          if (msg.type === 'populateBindings') ws.send(JSON.stringify({type:'event',...event}));
          if (msg.type === 'updateBinding' && String(msg.value) === String(expected)) {
            clearTimeout(timer);ws.close();resolve();
          }
        };
      }), {port,event,expected:before + 2});
      assert.equal(await readCounter(), before + 2, 'browser WebSocket event did not increment exactly once');
      await page.close();
    }
    const attackerPage = await browser.newPage();
    await attackerPage.goto(`http://127.0.0.1:${attacker.address().port}/`);
    const denied = await attackerPage.evaluate(async ({port,code,event}) => {
      for(const endpoint of ['/api/event', '/api/run', '/api/export']) {
        await fetch(`http://127.0.0.1:${port}${endpoint}`, {method:'POST', mode:'no-cors', headers:{'Content-Type':'text/plain'}, body:JSON.stringify({code,...event})});
      }
      return new Promise(resolve => {
        const ws = new WebSocket(`ws://127.0.0.1:${port + 80}`);
        ws.onopen = () => {ws.close();resolve(false);};
        ws.onerror = () => resolve(true);
      });
    }, {port,code:code.replace('count = 0', 'count = 9000'),event});
    assert.equal(denied, true, 'foreign browser origin opened the live WebSocket');
    await attackerPage.close();
    console.log(`PASS: ${mode}: localhost/127.0.0.1 browser and Studio preview WebSockets; foreign-origin simple POST and WebSocket denied`);
  } finally {
    await browser.close();
    await new Promise(resolve => attacker.close(resolve));
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
