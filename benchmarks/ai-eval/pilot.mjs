import path from 'node:path';
import fs from 'node:fs';
import {root,evaluate} from './harness.mjs';
const out=path.resolve(process.argv[2]||'runs/pilot');fs.mkdirSync(out,{recursive:true});
const results=[];
for(const task of ['01','02'])for(const stack of ['jtml','react']){
 const result=await evaluate(task,stack,path.join(root,'evaluator/references',task,stack),path.join(out,`${task}-${stack}-reference`));
 results.push({task,stack,control:'reference',outcome:result.outcome,expected:'pass',checks:result.checks});
}
// Broken starters must fail the retained reset contract in both stacks.
for(const stack of ['jtml','react']){
 const result=await evaluate('01',stack,path.join(root,'starters/02',stack),path.join(out,`reset-${stack}-negative`));
 const caught=result.outcome==='functional_failure'&&result.baselineFailures.includes('baseline.reset-confirmation');
 results.push({task:'01',stack,control:'seeded-reset-defect',outcome:result.outcome,expected:'functional_failure',caught,checks:result.checks});
}
fs.writeFileSync(path.join(out,'pilot.json'),JSON.stringify({kind:'harness-validation',agentTrials:0,results},null,2)+'\n');
console.log(JSON.stringify(results,null,2));
process.exitCode=results.every(r=>r.outcome===r.expected&&r.caught!==false)?0:1;
