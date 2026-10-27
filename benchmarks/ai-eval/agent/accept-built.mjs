// Runs only after the agent and build containers are gone. App scripts execute in Chromium.
import fs from 'node:fs';
import path from 'node:path';
import {chromium} from '@playwright/test';
import {serve, manifest} from '../harness.mjs';
import {acceptance} from '../evaluator/acceptance.mjs';
const [task, stack, dist, out] = process.argv.slice(2);
let browser, server;
const result = {kind:'independent-acceptance', task, stack, outcome:'infrastructure_error', checks:[]};
try {
  result.outputHashes = manifest(path.resolve(dist));
  browser = await chromium.launch({headless:true});
  result.browser = browser.version();
  const serving = await serve(path.resolve(dist)); server = serving.server;
  result.checks = await acceptance(browser, serving.url, task, out);
  result.baselineFailures = result.checks.filter(c => !c.pass && c.id.startsWith('baseline.')).map(c => c.id);
  result.outcome = result.checks.every(c => c.pass) ? 'pass' : 'functional_failure';
} catch(error) { result.error = String(error); }
finally {
  if(browser) await browser.close(); if(server) await new Promise(r => server.close(r));
  fs.writeFileSync(path.join(out, 'result.json'), JSON.stringify(result,null,2)+'\n');
  process.exitCode = result.outcome === 'pass' ? 0 : 1;
}
