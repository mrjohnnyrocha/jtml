// Import externally executed trials. This does not run or simulate a coding agent.
import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import {fileURLToPath} from 'node:url';
import {manifest,snapshot} from './harness.mjs';
export function validateTrial(t){
 assert.equal(t.kind,'agent-trial');
 for(const k of ['id','pairId','task','stack','model','agentVersion','reasoningEffort','protocolSha256','pricingSnapshot','guidanceSha256','startedAt','finishedAt'])assert.ok(typeof t[k]==='string'&&t[k].length,`Missing ${k}`);
 assert.ok(['01','02'].includes(t.task));assert.ok(['jtml','react'].includes(t.stack));
 assert.ok(Number.isFinite(Date.parse(t.startedAt))&&Date.parse(t.finishedAt)>=Date.parse(t.startedAt));
 assert.ok(['agent_failure','unsupported','toolchain_defect','infrastructure_error','unresolved','success'].includes(t.classification));
 for(const k of ['wallSeconds','tokenBudget','costBudgetUSD','maxAttempts'])assert.ok(Number.isFinite(t.budgets?.[k])&&t.budgets[k]>0,`Missing budget ${k}`);
 assert.ok(Array.isArray(t.attempts)&&t.attempts.length>0);
 for(const [i,a]of t.attempts.entries()){
  assert.equal(a.number,i+1,'Preserve every attempt in order');
  for(const k of ['inputTokens','outputTokens','cachedInputTokens','costUSD','latencyMs'])assert.ok(a[k]===null||(Number.isFinite(a[k])&&a[k]>=0),`${k}: unknown must be null, never zero`);
  for(const k of ['inputTokens','outputTokens','cachedInputTokens'])assert.ok(a[k]===null||Number.isInteger(a[k]),`${k} must be an integer`);
  if(a.inputTokens!==null&&a.cachedInputTokens!==null)assert.ok(a.cachedInputTokens<=a.inputTokens,'Cached input is a subset of inputTokens');
  assert.ok(typeof a.evidence==='string'&&typeof a.transcript==='string');
 }
 if(['unsupported','toolchain_defect'].includes(t.classification))assert.ok(t.adjudication&&t.adjudication.reproducer&&t.adjudication.reviewer&&t.adjudication.reason,'Independent reproducer and reviewer required');
 return t;
}
export function importTrial(input,dest){
 const t=validateTrial(JSON.parse(fs.readFileSync(input)));
 const base=path.dirname(path.resolve(input));fs.mkdirSync(dest);
 for(const a of t.attempts){
  const evidence=path.resolve(base,a.evidence),transcript=path.resolve(base,a.transcript);
  const result=JSON.parse(fs.readFileSync(path.join(evidence,'result.json')));
  assert.equal(result.task,t.task);assert.equal(result.stack,t.stack);
  const folder=`attempt-${a.number}`;snapshot(evidence,path.join(dest,folder),true);
  fs.copyFileSync(transcript,path.join(dest,`${folder}.transcript`));
  a.outcome=result.outcome;a.baselineFailures=result.baselineFailures||[];
  a.evidence=folder;a.transcript=`${folder}.transcript`;
 }
 const passed=t.attempts.some(a=>a.outcome==='pass');
 assert.equal(t.classification==='success',passed,'Success classification must agree with functional evidence');
 if(t.adjudication?.reproducer){
  const from=path.resolve(base,t.adjudication.reproducer);
  snapshot(from,path.join(dest,'adjudication'));t.adjudication.reproducer='adjudication';
 }
 t.budgetAudit={attempts:t.attempts.length<=t.budgets.maxAttempts,wall:(Date.parse(t.finishedAt)-Date.parse(t.startedAt))/1000<=t.budgets.wallSeconds,
  tokens:t.attempts.every(a=>a.inputTokens!==null&&a.outputTokens!==null)?t.attempts.reduce((n,a)=>n+a.inputTokens+a.outputTokens,0)<=t.budgets.tokenBudget:null,
  cost:t.attempts.every(a=>a.costUSD!==null)?t.attempts.reduce((n,a)=>n+a.costUSD,0)<=t.budgets.costBudgetUSD:null};
 t.budgetQualifiedSuccess=Object.values(t.budgetAudit).some(v=>v===false)?false:Object.values(t.budgetAudit).some(v=>v===null)?null:passed;
 t.evidenceHashes=manifest(dest);
 fs.writeFileSync(path.join(dest,'trial.json'),JSON.stringify(t,null,2)+'\n');
 return t;
}
export function summarize(trials){
 const ids=new Set();const pairs=new Map();
 for(const t of trials){
  assert.ok(!ids.has(t.id),'Duplicate trial ID');ids.add(t.id);
  const key=`${t.pairId}/${t.task}`;const p=pairs.get(key)||[];p.push(t);pairs.set(key,p);
 }
 const pairAudit=[...pairs.entries()].map(([id,ts])=>{
  const fields=['model','agentVersion','reasoningEffort','protocolSha256','pricingSnapshot'];
  const matched=ts.length===2&&new Set(ts.map(t=>t.stack)).size===2&&fields.every(k=>ts[0][k]===ts[1][k])&&JSON.stringify(ts[0].budgets)===JSON.stringify(ts[1].budgets);
  return {id,matched};
 });
 const percentile=(values,p)=>{if(!values.length)return null;values.sort((a,b)=>a-b);return values[Math.ceil(p*values.length)-1];};
 const sums={};
 for(const stack of ['jtml','react']){
  const ts=trials.filter(t=>t.stack===stack);const supported=ts.filter(t=>!['unsupported','infrastructure_error','unresolved'].includes(t.classification));
  const sum=k=>ts.length&&ts.every(t=>t.attempts.every(a=>a[k]!==null))?ts.reduce((n,t)=>n+t.attempts.reduce((s,a)=>s+a[k],0),0):null;
  const wall=ts.map(t=>Date.parse(t.finishedAt)-Date.parse(t.startedAt));
  sums[stack]={trials:ts.length,firstAttemptSuccesses:ts.filter(t=>t.attempts[0].outcome==='pass').length,
   budgetQualifiedSuccesses:ts.filter(t=>t.budgetQualifiedSuccess===true).length,
   budgetUnknown:ts.filter(t=>t.budgetQualifiedSuccess==null).length,
   medianWallLatencyMs:percentile([...wall],0.5),p95WallLatencyMs:percentile([...wall],0.95),successes:ts.filter(t=>t.classification==='success').length,successRate:ts.length?ts.filter(t=>t.classification==='success').length/ts.length:null,
   supportedTrials:supported.length,supportedSuccessRate:supported.length?supported.filter(t=>t.classification==='success').length/supported.length:null,
   classifications:Object.fromEntries(['success','agent_failure','unsupported','toolchain_defect','infrastructure_error','unresolved'].map(c=>[c,ts.filter(t=>t.classification===c).length])),
   attempts:ts.reduce((n,t)=>n+t.attempts.length,0),inputTokens:sum('inputTokens'),outputTokens:sum('outputTokens'),cachedInputTokens:sum('cachedInputTokens'),costUSD:sum('costUSD'),attemptLatencyMs:sum('latencyMs'),
   newRegressionCount:ts.reduce((n,t)=>n+t.attempts.reduce((s,a)=>s+(a.baselineFailures||[]).filter(id=>t.task==='02'&&!['baseline.reset-and-reload','baseline.reset-confirmation'].includes(id)).length,0),0),
   baselineFailureCount:ts.reduce((n,t)=>n+t.attempts.reduce((s,a)=>s+(a.baselineFailures||[]).length,0),0)};
 }
 return {descriptiveOnly:true,pairAudit,comparability:'Pair protocol, model, agent, pricing and budget audits required before inference; no significance claim',stacks:sums};
}
if(process.argv[1]&&path.resolve(process.argv[1])===fileURLToPath(import.meta.url)){
 const [command,...args]=process.argv.slice(2);
 if(command==='import')importTrial(args[0],args[1]);
 else if(command==='summary')console.log(JSON.stringify(summarize(args.map(p=>validateTrial(JSON.parse(fs.readFileSync(p))))),null,2));
 else throw new Error('Usage: node records.mjs import trial.json NEW_DIRECTORY | summary trial.json ...');
}
