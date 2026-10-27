import {test} from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {prepare,snapshot,validate} from './harness.mjs';
import {validateTrial,summarize} from './records.mjs';
test('export contains only assigned starter, specification and stack guidance',()=>{
 const tmp=fs.mkdtempSync(path.join(os.tmpdir(),'ai-eval-test-'));
 try {for(const stack of ['jtml','react']){
  const dest=path.join(tmp,stack);prepare('02',stack,dest);
  assert.deepEqual(fs.readdirSync(dest).sort(),['STACK.md','TASK.md',...(stack==='jtml'?['main.jtml']:['index.html','src.jsx'])].sort());
  assert.throws(()=>prepare('02',stack,dest));
 }}finally{fs.rmSync(tmp,{recursive:true,force:true});}
});
test('unsupported tasks fail closed; no silent catalog expansion',()=>assert.throws(()=>validate('20','jtml')));
test('snapshots reject symlinks instead of escaping evidence boundaries',()=>{
 const tmp=fs.mkdtempSync(path.join(os.tmpdir(),'ai-eval-link-'));
 try{fs.mkdirSync(path.join(tmp,'src'));fs.symlinkSync('/tmp',path.join(tmp,'src','escape'));assert.throws(()=>snapshot(path.join(tmp,'src'),path.join(tmp,'dst')),/Symlink/);}finally{fs.rmSync(tmp,{recursive:true,force:true});}
});
test('empty measurement cannot become zero cost or a claimed success rate',()=>{
 const r=summarize([]);assert.equal(r.stacks.jtml.costUSD,null);assert.equal(r.stacks.react.successRate,null);
});
test('incomplete telemetry records cannot be imported as measured trials',()=>assert.throws(()=>validateTrial({kind:'agent-trial'})));
test('summary exposes unmatched pairs, losses and unknown billing',()=>{
 const base={pairId:'p',task:'02',model:'model',agentVersion:'agent',reasoningEffort:'high',protocolSha256:'hash',pricingSnapshot:'rate',budgets:{wallSeconds:900},startedAt:'2026-10-27T00:00:00Z',finishedAt:'2026-10-27T00:00:10Z'};
 const attempt={number:1,outcome:'functional_failure',inputTokens:null,outputTokens:null,cachedInputTokens:null,costUSD:null,latencyMs:10000,baselineFailures:['baseline.reset-and-reload']};
 const failed={...base,id:'j',stack:'jtml',classification:'agent_failure',attempts:[attempt]};
 const passed={...base,id:'r',stack:'react',classification:'success',attempts:[{...attempt,outcome:'pass',baselineFailures:[]}]};
 const summary=summarize([failed,passed]);
 assert.equal(summary.pairAudit[0].matched,true);
 assert.equal(summary.stacks.jtml.successRate,0);assert.equal(summary.stacks.react.successRate,1);
 assert.equal(summary.stacks.jtml.costUSD,null);assert.equal(summary.stacks.jtml.newRegressionCount,0);
 assert.equal(summary.stacks.jtml.medianWallLatencyMs,10000);
 assert.equal(summarize([failed]).pairAudit[0].matched,false);
 assert.throws(()=>summarize([failed,failed]),/Duplicate/);
});
test('trial import preserves failed attempts, outputs and raw transcript; never overwrites',async()=>{
 const {importTrial}=await import('./records.mjs');
 const tmp=fs.mkdtempSync(path.join(os.tmpdir(),'ai-eval-import-'));
 try{
  fs.mkdirSync(path.join(tmp,'attempt/source/dist'),{recursive:true});
  fs.writeFileSync(path.join(tmp,'attempt/source/dist/index.html'),'failed output');
  fs.writeFileSync(path.join(tmp,'attempt/result.json'),JSON.stringify({task:'01',stack:'jtml',outcome:'functional_failure',baselineFailures:['baseline.validation-and-submit']}));
  fs.writeFileSync(path.join(tmp,'transcript.jsonl'),'unit-test fixture, not agent activity\n');
  const t={kind:'agent-trial',id:'unit-only',pairId:'unit-only',task:'01',stack:'jtml',model:'unit-only',agentVersion:'unit-only',reasoningEffort:'unit-only',protocolSha256:'unit-only',guidanceSha256:'unit-only',pricingSnapshot:'unit-only',startedAt:'2026-10-27T00:00:00Z',finishedAt:'2026-10-27T00:00:10Z',budgets:{wallSeconds:900,tokenBudget:30000,costBudgetUSD:5,maxAttempts:3},classification:'agent_failure',attempts:[{number:1,inputTokens:null,outputTokens:null,cachedInputTokens:null,costUSD:null,latencyMs:10000,evidence:'attempt',transcript:'transcript.jsonl'}]};
  fs.writeFileSync(path.join(tmp,'trial.json'),JSON.stringify(t));
  const imported=importTrial(path.join(tmp,'trial.json'),path.join(tmp,'archive'));
  assert.equal(imported.attempts[0].outcome,'functional_failure');assert.equal(imported.budgetAudit.cost,null);
  assert.equal(fs.readFileSync(path.join(tmp,'archive/attempt-1/source/dist/index.html'),'utf8'),'failed output');
  assert.throws(()=>importTrial(path.join(tmp,'trial.json'),path.join(tmp,'archive')));
 }finally{fs.rmSync(tmp,{recursive:true,force:true});}
});
