// The production controller/evaluator path with a deterministic provider. No HTTP/model calls.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {root,hash,manifest} from '../harness.mjs';
import {freeze,executedHashes,assertController,verifySeal,verifySmokeEvidence} from './core.mjs';
import {command,probe} from './worker.mjs';
import {runTrial} from './run.mjs';
import {StubProvider} from './stub.mjs';
export async function smoke(base, out) {
  fs.mkdirSync(out);out=path.resolve(out);
  const result={kind:'infrastructure-smoke',provider:'deterministic-stub',paidApiCalls:0,agentTrials:0,pass:false,status:'blocked',cases:[],workerImage:base.workerImage,workerCompilerSha256:base.workerCompilerSha256,oracleSha256:hash(path.join(root,'evaluator/acceptance.mjs')),controllerHashes:executedHashes};
  try {
    const daemon=await command(['info','--format','{{.ServerVersion}}'],{timeout:10000});
    if(daemon.exit!==0||!/^\d+\.\d+/.test(daemon.output.trim()))throw Error('Docker daemon unavailable: '+daemon.output);
    assert(base.workerImage&&base.workerCompilerSha256,'Pin worker image and compiler hash before smoke');
    const config={...base,phase:'infrastructure-smoke',modelSnapshot:'deterministic-stub',reasoningEffort:'none',pricing:{inputUSDPerMillionUpperBound:1,outputUSDPerMillionUpperBound:1,tokenCountUSDPerCallUpperBound:0},trials:[{id:'stub-success',task:'01',stack:'jtml'},{id:'stub-failure',task:'01',stack:'jtml'}]};
    const frozen=path.join(out,'frozen');freeze(config,frozen,{stub:true});assertController(frozen);result.controllerSeal=verifySeal(frozen);
    result.isolation=await probe(config,path.join(out,'isolation'));
    if(!result.isolation.pass)throw Error('Isolation smoke failed; preserve evidence and stop');
    for(const t of config.trials){
      const source=fs.readFileSync(path.join(frozen,'evaluator/agent/stubs',t.id==='stub-success'?'success.jtml':'failure.jtml'),'utf8');
      const attempt=await runTrial(frozen,t,path.join(out,t.id),{stub:new StubProvider(source)});
      const applicationOutcome=attempt.acceptance.outcome;
      const caught=attempt.acceptance.checks?.some(c=>c.id==='baseline.reset-confirmation'&&!c.pass);
      const validationPassed=attempt.acceptance.checks?.some(c=>c.id==='baseline.validation-and-submit'&&c.pass);
      const pass=attempt.status==='submitted'&&(t.id==='stub-success'?applicationOutcome==='pass':applicationOutcome==='functional_failure'&&caught&&validationPassed);
      result.cases.push({id:t.id,kind:attempt.kind,status:attempt.status,applicationOutcome,expected:t.id==='stub-success'?'pass':'functional_failure',pass});
    }
    assertController(frozen);
    result.pass=result.cases.length===2&&result.cases.every(c=>c.pass);result.status=result.pass?'passed':'failed';
  } catch(error){result.error=String(error);}
  finally{try{result.evidenceHashes=manifest(out);}catch(error){result.pass=false;result.status='failed';result.error=String(error);}
    fs.writeFileSync(path.join(out,'result.json'),JSON.stringify(result,null,2)+'\n');}
  if(result.pass)verifySmokeEvidence(path.join(out,'result.json'));
  return result;
}
if(process.argv[1]&&path.resolve(process.argv[1])===new URL(import.meta.url).pathname){
  const [config,out]=process.argv.slice(2);const r=await smoke(JSON.parse(fs.readFileSync(config)),out);
  console.log(JSON.stringify(r,null,2));process.exitCode=r.pass?0:1;
}
