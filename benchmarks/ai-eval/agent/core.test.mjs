// These deterministic protocol fixtures are unit tests, never model/agent trial evidence.
import {test} from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {root, manifest, hash} from '../harness.mjs';
import {Budget, validateConfig, freeze, verifySeal, reconcile} from './core.mjs';
import {isolationArgs} from './worker.mjs';
const config = () => ({...JSON.parse(fs.readFileSync(path.join(root,'agent/cohort.example.json'))),workerImage:'sha256:'+'1'.repeat(64),workerCompilerSha256:'2'.repeat(64),modelSnapshot:'unit-fixture-2000-01-01',reasoningEffort:'high',pricing:{source:'unit-fixture',evidence:'unit-fixture',inputUSDPerMillionUpperBound:2,outputUSDPerMillionUpperBound:10,tokenCountUSDPerCallUpperBound:0},billingReadiness:{reviewer:'unit-test',evidence:'unit-fixture'},trials:JSON.parse(fs.readFileSync(path.join(root,'agent/cohort.example.json'))).trials.map((t,i)=>({...t,billingProject:`unit-project-${i}`}))});
test('partial or floating model configuration cannot qualify a pilot',()=>{
  assert.throws(()=>validateConfig(JSON.parse(fs.readFileSync(path.join(root,'agent/cohort.example.json')))));
  assert.doesNotThrow(()=>validateConfig(config()));
  const alias=config();alias.modelSnapshot='floating-model';assert.throws(()=>validateConfig(alias));
  const expanded=config();expanded.tokenBudget=30001;assert.throws(()=>validateConfig(expanded));
});
test('reserve before dispatch: cumulative input+output including cached input obeys cap',()=>{
  const c=config();c.tokenBudget=100;c.maxOutputTokensPerCall=50;const b=new Budget(c);
  b.countCall();assert.equal(b.reserve(40).outputCap,50);
  b.settle({input_tokens:40,output_tokens:20,input_tokens_details:{cached_tokens:30}});
  assert.equal(b.tokens,60);assert.equal(b.reserve(30).outputCap,10);
  b.settle({input_tokens:30,output_tokens:10});assert.throws(()=>b.reserve(0),/exhausted/);
});
test('cost reservation limits a call even when token headroom remains',()=>{
  const c=config();c.costBudgetUSD=0.001;c.maxOutputTokensPerCall=1000;const b=new Budget(c);
  const r=b.reserve(100);assert(r.outputCap<=80);assert(r.upperUSD<=c.costBudgetUSD);
});
test('missing or inconsistent usage is charged pessimistically and prevents another call',()=>{
  for(const usage of [null,{input_tokens:9,output_tokens:1},{input_tokens:10,output_tokens:9999}]){
    const b=new Budget(config());const r=b.reserve(10);assert.throws(()=>b.settle(usage));
    assert(b.unknown);assert.equal(b.tokens,10+r.outputCap);assert.equal(b.upperUSD,r.upperUSD);
    assert.throws(()=>b.reserve(1));assert.throws(()=>b.countCall());
  }
});
test('container boundary has no network, host credentials, Docker socket or evaluator mount',()=>{
  const args=isolationArgs(config(),'unit-test');
  assert(args.includes('--read-only'));assert(args.includes('none'));assert(args.includes('ALL'));assert(args.includes('no-new-privileges'));
  assert(!args.includes('--privileged'));assert(!args.includes('--env'));assert(!args.some(a=>a.includes('docker.sock')));
});
test('freeze exports only one assigned task/guide and seals controller, receipts and inputs',()=>{
  const tmp=fs.mkdtempSync(path.join(os.tmpdir(),'jtml-agent-freeze-unit-'));
  try {
    const c=config();const billing=path.join(tmp,'billing.json');fs.writeFileSync(billing,JSON.stringify({kind:'billing-readiness',supportsActualUSD:true,exclusiveProjects:true,projects:c.trials.map(t=>t.billingProject)}));c.billingReadiness.evidence=billing;
    for(const field of ['mainCiEvidence','smokeEvidence','calibrationEvidence','providerEvidence']){c[field]=path.join(tmp,field+'.json');fs.writeFileSync(c[field],'{}');}
    const raw=path.join(tmp,'provider-raw.txt');fs.writeFileSync(raw,'unit-only provider bytes');fs.writeFileSync(c.providerEvidence,JSON.stringify({rawEvidence:'provider-raw.txt',rawEvidenceSha256:hash(raw)}));
    c.pricing.evidence=path.join(tmp,'rates.txt');fs.writeFileSync(c.pricing.evidence,'unit-only rates');
    const dest=path.join(tmp,'frozen');freeze(c,dest);verifySeal(dest);
    for(const t of c.trials){
      const files=Object.keys(manifest(path.join(dest,'packets',t.id)));
      assert.deepEqual(files.sort(),['STACK.md','TASK.md',...(t.stack==='jtml'?['main.jtml']:['index.html','src.jsx'])].sort());
    }
    fs.appendFileSync(path.join(dest,'packets/01-jtml-r1/STACK.md'),'tampered');assert.throws(()=>verifySeal(dest),/modified/);
    assert.throws(()=>freeze(c,dest));
  }finally{fs.rmSync(tmp,{recursive:true,force:true});}
});
test('comparable guidance scope is versioned and kept outside task/reference solutions',()=>{
  const lengths=['jtml','react'].map(s=>fs.readFileSync(path.join(root,'agent/guides',`${s}.md`),'utf8').split(/\s+/).length);
  assert(Math.max(...lengths)/Math.min(...lengths)<1.2);
});
test('actual billing cannot be replaced by rate estimates or incomplete response attribution',()=>{
  const trial={kind:'agent-pilot-attempt',billingProject:'unit-project',responseIds:['unit-response'],budgets:{costBudgetUSD:5}};
  assert.throws(()=>reconcile(trial,{kind:'estimate'}));
  const receipt={kind:'provider-billing-receipt',project:'unit-project',currency:'USD',settled:true,exclusiveProjectUse:true,source:'unit-export',reviewer:'unit-reviewer',actualCostUSD:0.01,responseIds:['unit-response']};
  assert.equal(reconcile(trial,receipt).actualCostUSD,0.01);
  assert.throws(()=>reconcile(trial,{...receipt,responseIds:[]}));
  assert.throws(()=>reconcile(trial,{...receipt,settled:false}));
});
test('generated-output symlinks cannot expose evaluator filesystem to the browser server',()=>{
  const tmp=fs.mkdtempSync(path.join(os.tmpdir(),'jtml-eval-output-unit-'));
  try{fs.symlinkSync('/tmp',path.join(tmp,'escape'));assert.throws(()=>manifest(tmp),/Symlink/);}
  finally{fs.rmSync(tmp,{recursive:true,force:true});}
});
test('unbilled/unreviewed or reused sessions cannot produce a paired success rate',async()=>{
  const {summarizePilot}=await import('./summary.mjs');const c=config();
  assert.equal(summarizePilot(c,[],'unit-seal').successRate,null);
  const trials=c.trials.map((p,i)=>({...p,kind:'agent-pilot-attempt',sessionId:`unit-session-${i}`,model:c.modelSnapshot,reasoningEffort:c.reasoningEffort,workerImage:c.workerImage,workerCompilerSha256:c.workerCompilerSha256,compilerSourceSHA:c.compilerSourceSHA,controllerSeal:'unit-seal',responseIds:[`unit-response-${i}`],usageKnown:true,usage:[{input_tokens:10,output_tokens:5}],actualCostUSD:0.01,billingReceipt:{settled:true},integrityReview:{reviewer:'unit',assignedStack:true,noEvaluatorAccess:true},status:'submitted',acceptance:{outcome:'pass'},attempts:1,wallLatencyMs:1000}));
  assert.equal(summarizePilot(c,trials,'unit-seal').successRate,1);
  const stubs=trials.map(t=>({...t,kind:'infrastructure-smoke-attempt'}));assert.equal(summarizePilot(c,stubs,'unit-seal').measuredTrials,0);
  trials[0].actualCostUSD=null;assert.equal(summarizePilot(c,trials,'unit-seal').successRate,null);
  trials[0].actualCostUSD=0.01;trials[0].sessionId=trials[1].sessionId;assert.equal(summarizePilot(c,trials,'unit-seal').successRate,null);
});
test('deterministic stub uses the tool-call interface and clearly marks synthetic usage',async()=>{
  const {StubProvider}=await import('./stub.mjs');const provider=new StubProvider('jtml 2\npage\n  text "stub"\n');
  const counted=await provider.send('responses/input_tokens',{});assert.equal(counted.kind,'deterministic-stub');
  const r=await provider.send('responses',{model:'stub'});
  assert.equal(r.kind,'deterministic-stub');assert.equal(r.output[0].name,'shell');
  assert(JSON.parse(r.output[0].arguments).command.includes('main.jtml'));
  const done=await provider.send('responses',{model:'stub'});assert.equal(done.output[0].type,'message');
  await assert.rejects(()=>provider.send('responses',{model:'stub'}));
  assert.throws(()=>reconcile({kind:'infrastructure-smoke-attempt'},{kind:'provider-billing-receipt'}));
});
test('paid execution refuses authorization before creating a worker or result directory',async()=>{
  const {runTrial}=await import('./run.mjs');
  const tmp=fs.mkdtempSync(path.join(os.tmpdir(),'jtml-paid-guard-unit-'));
  try {
    const c=config();c.billingReadiness.evidence=path.join(tmp,'billing.json');
    fs.writeFileSync(c.billingReadiness.evidence,JSON.stringify({kind:'billing-readiness',supportsActualUSD:true,exclusiveProjects:true,projects:c.trials.map(t=>t.billingProject)}));
    for(const field of ['mainCiEvidence','smokeEvidence','calibrationEvidence','providerEvidence']){c[field]=path.join(tmp,field+'.json');fs.writeFileSync(c[field],'{}');}
    const raw=path.join(tmp,'provider-raw.txt');fs.writeFileSync(raw,'unit-only provider bytes');fs.writeFileSync(c.providerEvidence,JSON.stringify({rawEvidence:'provider-raw.txt',rawEvidenceSha256:hash(raw)}));
    c.pricing.evidence=path.join(tmp,'rates.txt');fs.writeFileSync(c.pricing.evidence,'unit-only rates');
    const frozen=path.join(tmp,'frozen');freeze(c,frozen);
    await assert.rejects(()=>runTrial(frozen,c.trials[0],path.join(tmp,'trial')),/authorization/);
    assert(!fs.existsSync(path.join(tmp,'trial')));
  }finally{fs.rmSync(tmp,{recursive:true,force:true});}
});
