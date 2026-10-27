// Synthetic receipts for unit tests only; never operational smoke evidence.
import fs from 'node:fs';
import path from 'node:path';
import {manifest} from '../harness.mjs';
import {freeze,verifySeal,executedHashes} from './core.mjs';
export function stubConfig(base) {
  return {...base,phase:'infrastructure-smoke',workerImage:'sha256:'+'1'.repeat(64),workerCompilerSha256:'2'.repeat(64),modelSnapshot:'deterministic-stub',reasoningEffort:'none',pricing:{inputUSDPerMillionUpperBound:2,outputUSDPerMillionUpperBound:10,tokenCountUSDPerCallUpperBound:0},trials:[{id:'stub-success',task:'01',stack:'jtml'},{id:'stub-failure',task:'01',stack:'jtml'}]};
}
export function smokeFixture(tmp,base) {
  const dir=path.join(tmp,'unit-smoke');fs.mkdirSync(dir);
  const c=stubConfig(base);freeze(c,path.join(dir,'frozen'),{stub:true});
  const controllerSeal=verifySeal(path.join(dir,'frozen'));
  const cases=c.trials.map(t=>({id:t.id,pass:true}));
  for(const t of cases){fs.mkdirSync(path.join(dir,t.id));fs.writeFileSync(path.join(dir,t.id,'trial.json'),JSON.stringify({kind:'infrastructure-smoke-attempt',controllerSeal}));}
  const file=path.join(dir,'result.json');
  fs.writeFileSync(file,JSON.stringify({kind:'infrastructure-smoke',unitFixture:true,pass:true,paidApiCalls:0,agentTrials:0,controllerSeal,controllerHashes:executedHashes,cases,evidenceHashes:manifest(dir)}));
  return file;
}
