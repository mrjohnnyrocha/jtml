import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {root, hash, manifest, snapshot, prepare} from '../harness.mjs';

export const controllerFiles = ['agent/core.mjs', 'agent/worker.mjs', 'agent/run.mjs', 'agent/accept-built.mjs', 'agent/system.md', 'agent/summary.mjs', 'agent/Worker.Dockerfile', 'agent/Worker.Dockerfile.dockerignore', 'agent/stub.mjs', 'agent/smoke.mjs', 'agent/stubs/success.jtml', 'agent/stubs/failure.jtml'];
export function validateConfig(c) {
  assert.equal(c.version, 2);
  assert.equal(c.phase, 'public-agent-pilot');
  assert.match(c.compilerSourceSHA || '', /^[a-f0-9]{40}$/);
  assert.match(c.workerImage || '', /^(?:sha256:[a-f0-9]{64}|[^\s]+@sha256:[a-f0-9]{64})$/);
  assert.match(c.workerCompilerSha256 || '', /^[a-f0-9]{64}$/);
  assert.ok(c.modelSnapshot && /\d{4}-\d{2}-\d{2}/.test(c.modelSnapshot), 'Exact dated model snapshot required; no floating alias');
  assert.ok(['low', 'medium', 'high', 'xhigh'].includes(c.reasoningEffort));
  for (const [key, max] of Object.entries({wallSeconds:900, tokenBudget:30000, costBudgetUSD:5, maxAttempts:3, maxOutputTokensPerCall:4096, shellSeconds:60})) {
    assert.ok(Number.isFinite(c[key]) && c[key] > 0 && c[key] <= max, `Invalid ${key}`);
  }
  assert.equal(c.plannedAttempts, 1, 'First pilot is one independent session per trial');
  assert.equal(c.memory, '2g'); assert.equal(c.cpus, 2); assert.equal(c.pids, 128);
  assert.ok(c.pricing.source && c.pricing.evidence, 'Reviewed pricing snapshot and archived evidence required');
  for (const key of ['inputUSDPerMillionUpperBound', 'outputUSDPerMillionUpperBound', 'tokenCountUSDPerCallUpperBound']) {
    assert.ok(Number.isFinite(c.pricing[key]) && c.pricing[key] >= 0, `Missing pricing ${key}`);
  }
  assert.ok(c.pricing.inputUSDPerMillionUpperBound > 0 && c.pricing.outputUSDPerMillionUpperBound > 0);
  assert.ok(c.billingReadiness?.reviewer && c.billingReadiness?.evidence, 'Actual provider billing reconciliation must be available');
  assert.equal(c.trials.length, 4);
  assert.deepEqual(c.trials.map(t => `${t.task}/${t.stack}`).sort(), ['01/jtml','01/react','02/jtml','02/react']);
  assert.equal(new Set(c.trials.map(t => t.id)).size, 4);
  for (const t of c.trials) {
    assert.match(t.id, /^[a-zA-Z0-9-]+$/);
    assert.match(t.keyEnv, /^JTML_EVAL_[A-Z0-9_]+_KEY$/);
    assert.ok(typeof t.billingProject === 'string' && t.billingProject.length > 0);
  }
  return c;
}
export class Budget {
  constructor(config) { this.c = config; this.tokens = 0; this.upperUSD = 0; this.unknown = false; this.pending = null; }
  countCall() {
    assert(!this.unknown && !this.pending, 'Unsettled request; stop rather than retry unknown usage');
    const next = this.upperUSD + this.c.pricing.tokenCountUSDPerCallUpperBound;
    assert(next <= this.c.costBudgetUSD, 'Cost budget exhausted before token count');
    this.upperUSD = next;
  }
  reserve(inputTokens) {
    assert(!this.unknown && !this.pending);
    assert(Number.isInteger(inputTokens) && inputTokens >= 0);
    const p = this.c.pricing;
    const inputUSD = inputTokens * p.inputUSDPerMillionUpperBound / 1e6;
    const affordable = Math.floor((this.c.costBudgetUSD - this.upperUSD - inputUSD) * 1e6 / p.outputUSDPerMillionUpperBound);
    const outputCap = Math.min(this.c.maxOutputTokensPerCall, this.c.tokenBudget - this.tokens - inputTokens, affordable);
    assert(outputCap >= 1, 'Token or cost budget exhausted');
    this.pending = {inputTokens, outputCap, upperUSD:inputUSD + outputCap * p.outputUSDPerMillionUpperBound / 1e6};
    return this.pending;
  }
  settle(usage) {
    const r = this.pending; assert(r);
    const valid = usage && Number.isInteger(usage.input_tokens) && usage.input_tokens === r.inputTokens &&
      Number.isInteger(usage.output_tokens) && usage.output_tokens >= 0 && usage.output_tokens <= r.outputCap;
    if (!valid) {
      // Reserve the entire dispatched request and terminate on missing/mismatched usage.
      this.tokens += r.inputTokens + r.outputCap; this.upperUSD += r.upperUSD;
      this.pending = null; this.unknown = true;
      throw new Error('Missing or inconsistent provider usage; no further requests permitted');
    }
    this.tokens += usage.input_tokens + usage.output_tokens;
    this.upperUSD += (usage.input_tokens * this.c.pricing.inputUSDPerMillionUpperBound + usage.output_tokens * this.c.pricing.outputUSDPerMillionUpperBound) / 1e6;
    this.pending = null;
  }
}
export function freeze(config, dest, {stub=false}={}) {
  if(stub) {
    assert.equal(config.phase, 'infrastructure-smoke');
    assert.match(config.workerImage, /^(?:sha256:[a-f0-9]{64}|[^\s]+@sha256:[a-f0-9]{64})$/);
    assert.match(config.workerCompilerSha256, /^[a-f0-9]{64}$/);
    assert.equal(config.trials.length, 2);
  } else validateConfig(config);
  fs.mkdirSync(dest);
  const providerSource = config.providerEvidence;
  config = structuredClone(config);
  for (const field of (stub ? [] : ['mainCiEvidence', 'smokeEvidence', 'calibrationEvidence', 'providerEvidence'])) {
    const to = path.resolve(dest, `${field}.json`);
    fs.copyFileSync(config[field], to); config[field] = path.basename(to);
  }
  if(!stub) {
    const original = JSON.parse(fs.readFileSync(path.join(dest,'providerEvidence.json')));
    // Keep the referenced provider bytes with the seal, not a dangling machine-local path.
    const sourceReceipt = providerSource;
    const raw = path.resolve(path.dirname(sourceReceipt), original.rawEvidence);
    assert.equal(hash(raw), original.rawEvidenceSha256, 'Provider raw evidence hash mismatch');
    fs.copyFileSync(raw,path.join(dest,'provider-raw-evidence'));
    original.rawEvidence='provider-raw-evidence';
    fs.writeFileSync(path.join(dest,'providerEvidence.json'),JSON.stringify(original,null,2)+'\n');
  }
  if(!stub) {const pricing = path.resolve(dest, 'pricing-evidence.txt');
  fs.copyFileSync(config.pricing.evidence, pricing); config.pricing.evidence = path.basename(pricing);}
  for (const trial of config.trials) {
    const packet = path.join(dest, 'packets', trial.id);
    fs.mkdirSync(path.dirname(packet), {recursive:true});
    prepare(trial.task, trial.stack, packet);
    fs.copyFileSync(path.join(root, 'agent/guides', `${trial.stack}.md`), path.join(packet, 'STACK.md'));
  }
  const evaluator = path.join(dest, 'evaluator');
  for (const file of [...controllerFiles, 'harness.mjs', 'evaluator/acceptance.mjs', 'package.json', 'package-lock.json', 'agent/guides/jtml.md', 'agent/guides/react.md']) {
    const to = path.join(evaluator, file); fs.mkdirSync(path.dirname(to), {recursive:true});
    fs.copyFileSync(path.join(root, file), to);
  }
  fs.cpSync(path.join(root, 'tasks'), path.join(evaluator, 'tasks'), {recursive:true});
  if(!stub) {const evidence = JSON.parse(fs.readFileSync(config.billingReadiness.evidence));
  assert.equal(evidence.kind, 'billing-readiness');
  assert.equal(evidence.supportsActualUSD, true);
  assert.equal(evidence.exclusiveProjects, true);
  assert.deepEqual([...evidence.projects].sort(), [...new Set(config.trials.map(t=>t.billingProject))].sort());
  fs.writeFileSync(path.join(dest, 'billing-readiness.json'), JSON.stringify(evidence, null, 2));
  config.billingReadiness.evidence = 'billing-readiness.json';}
  fs.writeFileSync(path.join(dest, 'cohort.json'), JSON.stringify(config, null, 2) + '\n');
  const seal = manifest(dest);
  fs.writeFileSync(path.join(dest, 'seal.json'), JSON.stringify(seal, null, 2) + '\n');
  return seal;
}
export function verifySeal(dir) {
  const expected = JSON.parse(fs.readFileSync(path.join(dir, 'seal.json')));
  const actual = manifest(dir); delete actual['seal.json'];
  assert.deepEqual(actual, expected, 'Frozen campaign was modified');
  return hash(path.join(dir, 'seal.json'));
}
export function packetCopy(frozen, trial, candidate) {
  snapshot(path.join(frozen, 'packets', trial.id), candidate);
}
export function reconcile(trial, receipt) {
  assert.equal(trial.kind, 'agent-pilot-attempt', 'Stub/infrastructure records cannot become billed agent trials');
  assert.equal(receipt.kind, 'provider-billing-receipt');
  assert.equal(receipt.project, trial.billingProject);
  assert.equal(receipt.currency, 'USD');
  assert.equal(receipt.settled, true);
  assert.equal(receipt.exclusiveProjectUse, true);
  assert.ok(receipt.source && receipt.reviewer);
  assert(Number.isFinite(receipt.actualCostUSD) && receipt.actualCostUSD >= 0);
  assert.deepEqual([...receipt.responseIds].sort(), [...trial.responseIds].sort(), 'Billing must cover every response');
  return {...trial, actualCostUSD:receipt.actualCostUSD, billingReceipt:receipt, billingWithinBudget:receipt.actualCostUSD <= trial.budgets.costBudgetUSD};
}
