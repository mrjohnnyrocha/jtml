import {test} from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {spawnSync} from 'node:child_process';
import {root,manifest,snapshot,fileLimits,hash} from '../harness.mjs';
import {freeze,verifySmokeEvidence,Budget} from './core.mjs';
import {stop} from './worker.mjs';
import {sharedRequest,failureStatus,stopCohort,runTrial} from './run.mjs';
import {stubConfig,smokeFixture} from './test-fixtures.mjs';
const base=JSON.parse(fs.readFileSync(path.join(root,'agent/cohort.example.json')));
const temp=()=>fs.mkdtempSync(path.join(os.tmpdir(),'jtml-review-regression-'));
test('provenance: receipt, raw evidence, frozen and executed controller must agree',()=>{
  const dir=temp();try{
    const file=smokeFixture(dir,base);verifySmokeEvidence(file);
    const r=JSON.parse(fs.readFileSync(file));r.controllerHashes['agent/run.mjs']='0'.repeat(64);fs.writeFileSync(file,JSON.stringify(r));
    assert.throws(()=>verifySmokeEvidence(file),/executed source/);
    const source=path.join(path.dirname(file),'frozen/evaluator/agent/run.mjs');fs.appendFileSync(source,'\n// changed\n');
    assert.throws(()=>verifySmokeEvidence(file),/raw evidence/);
    assert.throws(()=>verifySmokeEvidence(path.join(root,'evidence/infrastructure-smoke/success-and-failure.json')),/manifest/);
  }finally{fs.rmSync(dir,{recursive:true,force:true});}
});
test('teardown: removal failure cannot be accepted while a worker exists or daemon is unavailable',async()=>{
  const events=[];
  await assert.rejects(()=>stop('test',e=>events.push(e),async args=>args[0]==='rm'?{exit:1,output:'denied'}:{exit:0,output:'still-running'}),/unconfirmed/);
  assert.equal(events.length,2);
  await assert.rejects(()=>stop('test',()=>{},async()=>({exit:1,output:'daemon unavailable'})),/unconfirmed/);
  assert.equal((await stop('test',()=>{},async args=>({exit:args[0]==='rm'?1:0,output:''}))).confirmed,true);
});
test('source inspection: real FIFO and socket are rejected before reads; count, file and total limits apply',()=>{
  const dir=temp();try{
    const src=path.join(dir,'src');fs.mkdirSync(src);
    assert.equal(spawnSync('mkfifo',[path.join(src,'pipe')]).status,0);
    for(const fn of ['snapshot','manifest']){
      const code=`import {${fn}} from ${JSON.stringify(path.join(root,'harness.mjs'))};try{${fn}(${JSON.stringify(src)},${fn==='snapshot'?JSON.stringify(path.join(dir,'dest')):''});process.exit(2)}catch(e){console.log(String(e))}`;
      const r=spawnSync(process.execPath,['--input-type=module','-e',code],{encoding:'utf8',timeout:3000});
      assert.equal(r.status,0,r.stderr);assert.match(r.stdout,/regular files/);
    }
    assert.throws(()=>hash(path.join(src,'pipe')),/regular files/);
    assert.throws(()=>hash('/dev/null'),/regular files/);
    fs.unlinkSync(path.join(src,'pipe'));fs.writeFileSync(path.join(src,'a'),'1234');fs.writeFileSync(path.join(src,'b'),'5678');
    for(const limits of [{...fileLimits,entries:1},{...fileLimits,fileBytes:3},{...fileLimits,totalBytes:7}]){
      assert.throws(()=>manifest(src,limits),/limit exceeded/);
      assert.throws(()=>snapshot(src,path.join(dir,'limited'),false,limits),/limit exceeded/);
      assert(!fs.existsSync(path.join(dir,'limited')));
    }
    // Actual Unix socket, created in a bounded child so the test cannot hang.
    const socket=path.join(src,'socket');
    const code=`import net from 'node:net';import {manifest} from ${JSON.stringify(path.join(root,'harness.mjs'))};const s=net.createServer();s.listen(${JSON.stringify(socket)},()=>{try{manifest(${JSON.stringify(src)});process.exitCode=2}catch(e){console.log(String(e))}finally{s.close()}});`;
    const r=spawnSync(process.execPath,['--input-type=module','-e',code],{encoding:'utf8',timeout:3000});assert.equal(r.status,0,r.stderr);assert.match(r.stdout,/regular files/);
  }finally{fs.rmSync(dir,{recursive:true,force:true});}
});
test('deadline accounting: unknown usage wins at exactly zero remaining time, retaining the entire reservation',()=>{
  const c=stubConfig(base),b=new Budget(c);b.countCall();const reservation=b.reserve(100);
  assert.throws(()=>b.settle(null));
  assert.equal(failureStatus(Error('aborted'),b,0<=0),'usage_unknown');
  assert.equal(b.tokens,100+reservation.outputCap);assert.equal(b.upperUSD,reservation.upperUSD);
  assert.throws(()=>b.countCall());
  assert(stopCohort({status:'wall_limit',billingExposureUnresolved:true,teardown:{confirmed:true},acceptance:{outcome:'pass'}}));
});
test('request parity: count and generation share reasoning, tools, model and conversation; output cap includes all output',()=>{
  const c=stubConfig(base),common=sharedRequest(c,'instructions',[{role:'user',content:'task'}]);
  const b=new Budget(c),r=b.reserve(100);
  const generation={...common,store:false,include:['reasoning.encrypted_content'],max_output_tokens:r.outputCap};
  const {store,include,max_output_tokens,...input}=generation;
  assert.deepEqual(input,common);assert.equal(common.parallel_tool_calls,false);assert.equal(common.reasoning.effort,c.reasoningEffort);
  b.settle({input_tokens:100,output_tokens:100,output_tokens_details:{reasoning_tokens:90}});assert.equal(b.tokens,200);
});
test('controller failures: interrupted stub generation, failed removal and FIFO submission preserve errors without evaluation races',async()=>{
  const dir=temp(),oldPath=process.env.PATH;
  try{
    const docker=path.join(dir,'docker'),flag=path.join(dir,'removal-failure');
    fs.writeFileSync(docker,`#!${process.execPath}\nconst fs=require('fs');const a=process.argv.slice(2);if(a[0]==='inspect')console.log('[]');if(a[0]==='rm'&&fs.existsSync(${JSON.stringify(flag)}))process.exitCode=1;if(a[0]==='ps'&&fs.existsSync(${JSON.stringify(flag)}))console.log('worker-still-running');`);fs.chmodSync(docker,0o755);
    process.env.PATH=dir+path.delimiter+oldPath;
    const c=stubConfig({...base,wallSeconds:1}),frozen=path.join(dir,'frozen');freeze(c,frozen,{stub:true});
    let calls=0;
    const result=await runTrial(frozen,c.trials[0],path.join(dir,'interrupted'),{stub:{send:async endpoint=>{calls++;if(endpoint.endsWith('input_tokens'))return {input_tokens:100};await new Promise(r=>setTimeout(r,1100));throw Error('interrupted generation');}}});
    const counted=JSON.parse(fs.readFileSync(path.join(dir,'interrupted/request-1.json'))).body;
    const generated=JSON.parse(fs.readFileSync(path.join(dir,'interrupted/request-2.json'))).body;
    const {store,include,max_output_tokens,...shared}=generated;assert.deepEqual(shared,counted);
    assert.equal(counted.parallel_tool_calls,false);assert.deepEqual(counted.reasoning,{effort:c.reasoningEffort});
    assert.equal(result.status,'usage_unknown');assert.equal(result.wallLimitExpired,true);assert.equal(result.billingExposureUnresolved,true);assert(result.stubBudgetAccounting.tokens>=100);assert.equal(calls,2);assert(stopCohort(result));
    assert.match(fs.readFileSync(path.join(dir,'interrupted/events.jsonl'),'utf8'),/interrupted generation/);
    fs.writeFileSync(flag,'fail');
    const removed=await runTrial(frozen,c.trials[0],path.join(dir,'removal'),{stub:{send:async e=>e.endsWith('input_tokens')?{input_tokens:100}:{id:'stub',model:c.modelSnapshot,status:'completed',usage:{input_tokens:100,output_tokens:1},output:[]}}});
    assert.equal(removed.teardown.confirmed,false);assert.equal(removed.status,'infrastructure_error');assert(!fs.existsSync(path.join(dir,'removal/evaluation')));assert(stopCohort(removed));
    fs.unlinkSync(flag);
    const fifo=await runTrial(frozen,c.trials[0],path.join(dir,'fifo'),{stub:{send:async e=>{if(e.endsWith('input_tokens'))return {input_tokens:100};assert.equal(spawnSync('mkfifo',[path.join(dir,'fifo/candidate/pipe')]).status,0);return {id:'stub',model:c.modelSnapshot,status:'completed',usage:{input_tokens:100,output_tokens:1},output:[]};}}});
    assert.equal(fifo.acceptance.outcome,'infrastructure_error');assert.match(fifo.acceptance.error,/regular files/);assert(!fs.existsSync(path.join(dir,'fifo/evaluation/source')));
    assert.equal(JSON.parse(fs.readFileSync(path.join(dir,'fifo/trial.json'))).acceptance.outcome,'infrastructure_error');
  }finally{process.env.PATH=oldPath;fs.rmSync(dir,{recursive:true,force:true});}
});
