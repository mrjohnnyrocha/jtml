// A complete, billed, independently reviewed cohort is required before scoring.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {verifySeal} from './core.mjs';
import {hash} from '../harness.mjs';
export function summarizePilot(config, trials, seal) {
  const blockers=[], sessions=new Set(), responses=new Set();const rows=[];
  for(const planned of config.trials) {
    const t=trials.find(t=>t.id===planned.id);
    if(!t){blockers.push(`${planned.id}: no trial`);continue;}
    const issues=[];
    if(t.ledgerError)issues.push(t.ledgerError);
    if(t.kind!=='agent-pilot-attempt')issues.push('infrastructure/stub records are never agent trials');
    for(const [key,expected]of Object.entries({task:planned.task,stack:planned.stack,model:config.modelSnapshot,reasoningEffort:config.reasoningEffort,workerImage:config.workerImage,workerCompilerSha256:config.workerCompilerSha256,compilerSourceSHA:config.compilerSourceSHA,controllerSeal:seal,billingProject:planned.billingProject}))if(t[key]!==expected)issues.push(`mismatched ${key}`);
    if(!t.sessionId||sessions.has(t.sessionId))issues.push('missing or reused session');sessions.add(t.sessionId);
    for(const id of t.responseIds||[]){if(responses.has(id))issues.push('response shared between trials');responses.add(id);}
    if(!t.usageKnown||!t.usage?.length||t.usage.some(u=>!Number.isInteger(u?.input_tokens)||!Number.isInteger(u?.output_tokens)))issues.push('unknown provider usage');
    if(!Number.isFinite(t.actualCostUSD)||!t.billingReceipt?.settled)issues.push('actual billing unreconciled');
    if(!t.integrityReview?.reviewer||t.integrityReview.assignedStack!==true||t.integrityReview.noEvaluatorAccess!==true)issues.push('source/integrity review missing');
    if(['infrastructure_error','usage_unknown'].includes(t.status)||t.acceptance?.outcome==='infrastructure_error')issues.push('infrastructure failure');
    if(t.attempts!==1)issues.push('first pilot requires one session');
    if(!Number.isFinite(t.wallLatencyMs))issues.push('wall latency missing');
    const input=t.usage?.every(u=>Number.isInteger(u?.input_tokens))?t.usage.reduce((n,u)=>n+u.input_tokens,0):null;
    const output=t.usage?.every(u=>Number.isInteger(u?.output_tokens))?t.usage.reduce((n,u)=>n+u.output_tokens,0):null;
    const withinBudget=input!==null&&output!==null&&input+output<=config.tokenBudget&&t.actualCostUSD<=config.costBudgetUSD&&t.wallLatencyMs<=config.wallSeconds*1000;
    blockers.push(...issues.map(i=>`${planned.id}: ${i}`));
    rows.push({id:t.id,stack:t.stack,status:t.status,acceptance:t.acceptance?.outcome,inputTokens:input,outputTokens:output,actualCostUSD:t.actualCostUSD??null,wallLatencyMs:t.wallLatencyMs,eligible:issues.length===0,success:issues.length===0&&withinBudget&&t.acceptance?.outcome==='pass',withinBudget});
  }
  if(trials.length!==4)blockers.push('Exactly four trial records required');
  return {kind:'real-agent-pilot-summary',attemptedSessions:rows.length,measuredTrials:trials.filter(t=>t.kind==='agent-pilot-attempt'&&t.responseIds?.length).length,blockers,scorable:blockers.length===0,rows,recommendation:blockers.length?'NO-GO: complete isolation, telemetry, billing and integrity checks before five paired replicates':'GO for methodology review before five paired replicates; four public-task observations do not establish significance',successRate:blockers.length?null:rows.filter(r=>r.success).length/4};
}
if(process.argv[1]&&path.resolve(process.argv[1])===fileURLToPath(import.meta.url)){
  const [frozen,results]=process.argv.slice(2);const seal=verifySeal(path.resolve(frozen));
  const config=JSON.parse(fs.readFileSync(path.join(frozen,'cohort.json')));
  const trials=config.trials.flatMap(t=>{
    const directory=path.join(results,t.id),f=path.join(directory,'trial.billed.json');
    if(!fs.existsSync(f))return [];
    const trial=JSON.parse(fs.readFileSync(f)),review=path.join(directory,'integrity-review.json');
    try{
      const originalPath=path.join(directory,'trial.json'),original=JSON.parse(fs.readFileSync(originalPath));
      assert.equal(trial.originalTrialSha256,hash(originalPath),'Original trial changed after billing');
      for(const [key,value]of Object.entries(original))if(key!=='actualCostUSD')assert.deepEqual(trial[key],value,`Billed copy changed ${key}`);
      assert.equal(trial.actualCostUSD,trial.billingReceipt.actualCostUSD);
      const raw=trial.billingReceipt.rawEvidence;assert.equal(raw,path.basename(raw));
      assert.equal(hash(path.join(directory,raw)),trial.billingReceipt.rawEvidenceSha256,'Billing export changed');
    }catch(error){trial.ledgerError=String(error);}
    if(fs.existsSync(review)){
      const r=JSON.parse(fs.readFileSync(review));
      if(r.trialSha256===hash(path.join(directory,'trial.json')))trial.integrityReview=r;
    }
    return [trial];
  });
  console.log(JSON.stringify(summarizePilot(config,trials,seal),null,2));
}
