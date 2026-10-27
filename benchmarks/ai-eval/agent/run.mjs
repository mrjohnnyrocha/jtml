// Trusted controller, outside evaluated workspaces. No Codex conversation is inherited.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';
import {root, hash, manifest, snapshot} from '../harness.mjs';
import {Budget, validateConfig, freeze, verifySeal, packetCopy, reconcile, controllerFiles} from './core.mjs';
import {command, isolationArgs, bind, start, stop, shell, probe} from './worker.mjs';
const read = file => JSON.parse(fs.readFileSync(file));
function configAt(file) {
  const c=read(file),base=path.dirname(path.resolve(file));
  for(const field of ['mainCiEvidence','smokeEvidence','calibrationEvidence','providerEvidence'])if(c[field])c[field]=path.resolve(base,c[field]);
  if(c.pricing?.evidence)c.pricing.evidence=path.resolve(base,c.pricing.evidence);
  if(c.billingReadiness?.evidence)c.billingReadiness.evidence=path.resolve(base,c.billingReadiness.evidence);
  return c;
}
const write = (file,value) => fs.writeFileSync(file, JSON.stringify(value,null,2)+'\n');
const writable = dir => {fs.chmodSync(dir,0o777);for(const e of fs.readdirSync(dir,{withFileTypes:true})){const f=path.join(dir,e.name);if(e.isDirectory())writable(f);else fs.chmodSync(f,0o666);}};
export async function check(config) {
  const blockers=[];
  for(const field of ['workerImage','workerCompilerSha256','modelSnapshot','reasoningEffort'])if(!config[field])blockers.push(`Campaign must pin ${field}`);
  try { validateConfig(config); } catch(error) { blockers.push('Incomplete/invalid campaign: '+String(error).split('\n')[0]); }
  for(const t of config.trials || []) if(!process.env[t.keyEnv]) blockers.push(`Missing ${t.keyEnv}`);
  const daemon=await command(['info','--format','{{.ServerVersion}}'],{timeout:10000}).catch(e=>({exit:1,output:String(e)}));
  const daemonReady=daemon.exit===0&&/^\d+\.\d+/.test(daemon.output.trim());
  if(!daemonReady) blockers.push('Container daemon unavailable: '+daemon.output.trim());
  if(daemonReady && config.workerImage) {
    const image=await command(['image','inspect',config.workerImage]);
    if(image.exit!==0)blockers.push('Pinned worker image unavailable locally');
    else if(JSON.parse(image.output)[0].Config.Labels?.['org.jtml.compiler-source']!==config.compilerSourceSHA)blockers.push('Worker compiler source label mismatch');
  }
  for(const [field,kind] of [['smokeEvidence','infrastructure-smoke'],['calibrationEvidence','worker-calibration']]) {
    try {const r=read(config[field]);assert.equal(r.kind,kind);assert.equal(r.pass,true);assert.equal(r.workerImage,config.workerImage);assert.equal(r.workerCompilerSha256,config.workerCompilerSha256);assert.equal(r.oracleSha256,hash(path.join(root,'evaluator/acceptance.mjs')));if(field==='smokeEvidence')assert.deepEqual(r.controllerHashes,Object.fromEntries(controllerFiles.map(f=>[f,hash(path.join(root,f))])));}catch{blockers.push(`Missing or nonmatching successful ${field}`);}
  }
  try {const r=read(config.mainCiEvidence);assert.equal(r.head_sha,config.compilerSourceSHA);assert.equal(r.status,'completed');assert.equal(r.conclusion,'success');}catch{blockers.push('Campaign missing a pinned successful post-merge main CI receipt');}
  try {const r=read(config.billingReadiness.evidence);assert.equal(r.kind,'billing-readiness');assert.equal(r.supportsActualUSD,true);assert.equal(r.exclusiveProjects,true);assert.deepEqual([...r.projects].sort(),[...new Set(config.trials.map(t=>t.billingProject))].sort());}catch{blockers.push('Provider actual-cost reconciliation readiness unverified');}
  try {
    const r=read(config.providerEvidence);assert.equal(r.kind,'provider-readiness');
    assert.equal(r.model,config.modelSnapshot);assert.equal(r.reasoningEffort,config.reasoningEffort);
    assert.equal(r.tokenCountRoundTripVerified,true);assert.equal(r.usageReportingVerified,true);
    assert.equal(r.billingAttributionVerified,true);assert(r.reviewer && r.rawEvidenceSha256);
    assert.equal(hash(path.resolve(path.dirname(config.providerEvidence),r.rawEvidence)),r.rawEvidenceSha256,'Provider raw evidence hash mismatch');
  }catch{blockers.push('Live provider model/usage and actual billing attribution are unverified');}
  return {kind:'pilot-readiness',ready:blockers.length===0,blockers:[...new Set(blockers)],checkedAt:new Date().toISOString(),agentTrials:0};
}
export async function buildAndAccept(config, trial, source, evidence, evaluatorRoot=root) {
  fs.mkdirSync(evidence);const output=path.join(evidence,'output');fs.mkdirSync(output);fs.chmodSync(output,0o777);
  snapshot(source,path.join(evidence,'source'));
  const name=`jtml-build-${crypto.randomUUID()}`;
  const buildCmd=trial.stack==='jtml' ? 'jtml build /submission/main.jtml --target browser --out /output' :
    'cp -R /submission/. /tmp/build && ln -s /opt/deps/node_modules /tmp/build/node_modules && cp /opt/deps/vite.config.mjs /tmp/runner-vite.config.mjs && vite build /tmp/build --config /tmp/runner-vite.config.mjs --outDir /output';
  let build;
  try {build=await command(['run','--rm',...isolationArgs(config,name),...bind(path.join(evidence,'source'),'/submission',true),...bind(output,'/output'),config.workerImage,'timeout','-k','1','60','bash','-c',buildCmd],{timeout:65000});}
  finally {await stop(name);}
  write(path.join(evidence,'build.json'),build);
  if(build.exit!==0){const result={task:trial.task,stack:trial.stack,outcome:build.timedOut||[124,125,126,127,137,143].includes(build.exit)?'infrastructure_error':'build_failure',checks:[],build};write(path.join(evidence,'result.json'),result);return result;}
  manifest(output); // Reject output symlinks before any browser or evaluator is mounted.
  const accepts=path.join(evidence,'acceptance');fs.mkdirSync(accepts);fs.chmodSync(accepts,0o777);
  const mounts=[];
  for(const f of ['harness.mjs','package.json','agent/accept-built.mjs','evaluator/acceptance.mjs'])mounts.push(...bind(path.join(evaluatorRoot,f),`/opt/evaluator/${f}`,true));
  const evalName=`jtml-accept-${crypto.randomUUID()}`;
  let evaluation;
  try {evaluation=await command(['run','--rm',...isolationArgs(config,evalName),...mounts,...bind(output,'/dist',true),...bind(accepts,'/evidence'),config.workerImage,'node','/opt/evaluator/agent/accept-built.mjs',trial.task,trial.stack,'/dist','/evidence'],{timeout:120000});}
  finally {await stop(evalName);}
  write(path.join(evidence,'evaluation-process.json'),evaluation);
  const result=fs.existsSync(path.join(accepts,'result.json'))?read(path.join(accepts,'result.json')):{task:trial.task,stack:trial.stack,outcome:'infrastructure_error',checks:[],evaluation};
  if(evaluation.timedOut || ![0,1].includes(evaluation.exit) ||
      (result.outcome==='pass'&&evaluation.exit!==0) ||
      (result.outcome==='functional_failure'&&evaluation.exit!==1)){
    result.outcome='infrastructure_error';result.processError=evaluation;
  }
  write(path.join(evidence,'result.json'),result);return result;
}
export async function calibrate(config, dest) {
  fs.mkdirSync(dest);const cases=[];
  for(const task of ['01','02'])for(const stack of ['jtml','react']){
    const r=await buildAndAccept(config,{task,stack},path.join(root,'evaluator/references',task,stack),path.join(dest,`${task}-${stack}`));
    cases.push({task,stack,control:'reference',outcome:r.outcome,pass:r.outcome==='pass'});
  }
  for(const stack of ['jtml','react']){
    const r=await buildAndAccept(config,{task:'01',stack},path.join(root,'starters/02',stack),path.join(dest,`negative-${stack}`));
    cases.push({task:'01',stack,control:'seeded-confirmation-defect',outcome:r.outcome,pass:r.outcome==='functional_failure'&&r.baselineFailures?.includes('baseline.reset-confirmation')});
  }
  const result={kind:'worker-calibration',workerImage:config.workerImage,workerCompilerSha256:config.workerCompilerSha256,oracleSha256:hash(path.join(root,'evaluator/acceptance.mjs')),pass:cases.every(c=>c.pass),cases};
  write(path.join(dest,'result.json'),result);return result;
}
const tools=[{type:'function',name:'shell',description:'Run a shell command inside the isolated application workspace. Use for reading/writing source, local builds and self-tests.',strict:true,parameters:{type:'object',properties:{command:{type:'string'}},required:['command'],additionalProperties:false}}];
export async function runTrial(frozen, trial, dest, options={}) {
  const seal=verifySeal(frozen);const config=configAt(path.join(frozen,'cohort.json'));
  const isStub=!!options.stub;
  if(isStub)assert.equal(config.phase,'infrastructure-smoke');
  else{validateConfig(config);assert.equal(options.authorizationSeal,seal,'Explicit operator authorization for this exact seal is required for paid calls');}
  fs.mkdirSync(dest);const candidate=path.join(dest,'candidate');packetCopy(frozen,trial,candidate);writable(candidate);
  const startedAt=new Date().toISOString(), started=performance.now(), deadline=started+config.wallSeconds*1000;
  const journal=path.join(dest,'events.jsonl');let worker,seq=0;const budget=new Budget(config);
  const workerName=`jtml-eval-${crypto.randomUUID()}`;
  const watchdog=setTimeout(()=>{void stop(workerName);},config.wallSeconds*1000);
  const result={kind:isStub?'infrastructure-smoke-attempt':'agent-pilot-attempt',agentTrials:isStub?0:1,id:trial.id,task:trial.task,stack:trial.stack,billingProject:trial.billingProject,model:config.modelSnapshot,reasoningEffort:config.reasoningEffort,sessionId:crypto.randomUUID(),controllerSeal:seal,workerImage:config.workerImage,compilerSourceSHA:config.compilerSourceSHA,workerCompilerSha256:config.workerCompilerSha256,startedAt,budgets:{wallSeconds:config.wallSeconds,tokenBudget:config.tokenBudget,costBudgetUSD:config.costBudgetUSD,maxAttempts:config.maxAttempts},attempts:1,responseIds:[],actualCostUSD:null,status:'infrastructure_error',usage:[]};
  const event=value=>fs.appendFileSync(journal,JSON.stringify({at:new Date().toISOString(),...value})+'\n');
  const remaining=()=>Math.max(0,deadline-performance.now());
  async function api(endpoint,body) {
    assert(remaining()>0,'Wall budget exhausted');const id=++seq;
    write(path.join(dest,`request-${id}.json`),{endpoint,body});
    if(isStub){
      const response=await options.stub.send(endpoint,body);
      write(path.join(dest,`response-${id}.json`),response);
      event({kind:'deterministic-stub',id,endpoint});return response;
    }
    const response=await fetch(`https://api.openai.com/v1/${endpoint}`,{method:'POST',headers:{Authorization:`Bearer ${process.env[trial.keyEnv]}`,'Content-Type':'application/json'},body:JSON.stringify(body),signal:AbortSignal.timeout(Math.max(1,Math.floor(Math.min(remaining(),60000))))});
    const raw=await response.text();fs.writeFileSync(path.join(dest,`response-${id}.json`),raw);
    event({kind:'api',id,endpoint,status:response.status,requestId:response.headers.get('x-request-id')});
    if(!response.ok)throw Error(`Provider HTTP ${response.status}; raw body preserved`);
    return JSON.parse(raw);
  }
  try {
    worker=await start(config,candidate,workerName,Math.max(1,Math.min(60000,remaining())));
    const inspection=await command(['inspect',worker]);write(path.join(dest,'worker-inspect.json'),JSON.parse(inspection.output));
    const init=await shell(worker,'ln -s /opt/deps/node_modules /work/node_modules',5);assert.equal(init.exit,0);
    const instructions=fs.readFileSync(path.join(frozen,'evaluator/agent/system.md'),'utf8');
    const input=[{role:'user',content:fs.readFileSync(path.join(candidate,'TASK.md'),'utf8')+'\n\n'+fs.readFileSync(path.join(candidate,'STACK.md'),'utf8')}];
    while(remaining()>0) {
      const common={model:config.modelSnapshot,instructions,input,tools};
      budget.countCall();const count=await api('responses/input_tokens',common);
      const reservation=budget.reserve(count.input_tokens);
      event({kind:'budget-reservation',...reservation,upperUSD:budget.upperUSD,tokens:budget.tokens});
      let response;
      try {response=await api('responses',{...common,reasoning:{effort:config.reasoningEffort},store:false,include:['reasoning.encrypted_content'],parallel_tool_calls:false,max_output_tokens:reservation.outputCap});}
      catch(error){budget.settle(null);throw error;}
      result.responseIds.push(response.id);result.usage.push(response.usage || null);
      budget.settle(response.usage);
      assert.equal(response.model,config.modelSnapshot,'Provider returned a different model snapshot');
      input.push(...response.output);
      const calls=response.output.filter(item=>item.type==='function_call');
      if(!calls.length){result.status=response.status==='completed'?'submitted':'output_limit';break;}
      for(const call of calls) {
        assert.equal(call.name,'shell');const args=JSON.parse(call.arguments);assert.equal(typeof args.command,'string');
        if(remaining()<=1000)throw Error('Wall budget exhausted');
        const toolResult=await shell(worker,args.command,Math.max(1,Math.min(config.shellSeconds,Math.floor(remaining()/1000)-1)));
        event({kind:'shell',callId:call.call_id,command:args.command,...toolResult});
        const alive=await command(['inspect','--format','{{.State.Running}}',worker],{timeout:5000});
        assert(alive.exit===0&&alive.output.trim()==='true','Worker unavailable after tool execution');
        input.push({type:'function_call_output',call_id:call.call_id,output:JSON.stringify(toolResult)});
      }
    }
    if(result.status==='infrastructure_error')result.status='wall_limit';
  } catch(error) {result.error=String(error);result.status=remaining()<=0?'wall_limit':budget.unknown?'usage_unknown':/budget exhausted/.test(String(error))?'budget_limit':'infrastructure_error';}
  finally {
    clearTimeout(watchdog);if(worker)await stop(worker);else await stop(workerName);
    result.finishedAt=new Date().toISOString();result.wallLatencyMs=performance.now()-started;
    result.upperCostUSD=isStub?null:budget.upperUSD;result.accountedTokens=isStub?null:budget.tokens;result.usageKnown=!isStub&&!budget.unknown;
    if(isStub){result.stubBudgetUsage=result.usage;result.usage=[];result.telemetrySource='deterministic-stub; not provider usage';}
    event({kind:'session-ended',status:result.status});write(path.join(dest,'trial.json'),result);
  }
  // Preserve even empty/failed/timed-out submissions. Evaluation feedback is never sent to the agent.
  try {result.acceptance=await buildAndAccept(config,trial,candidate,path.join(dest,'evaluation'),path.join(frozen,'evaluator'));}
  catch(error){result.acceptance={outcome:'infrastructure_error',error:String(error)};}
  if(fs.existsSync(path.join(dest,'evaluation/source')))result.sourceHashes=manifest(path.join(dest,'evaluation/source'));
  else result.sourcePreservationError='Submission snapshot could not be made; candidate and journal retained';
  result.classification=isStub?'infrastructure-validation':result.status==='infrastructure_error'||result.status==='usage_unknown'||result.acceptance.outcome==='infrastructure_error'?'infrastructure_error':result.acceptance.outcome==='pass'?'functional_pass_pending_review':'application_failure_pending_review';
  write(path.join(dest,'trial.json'),result);return result;
}
if(process.argv[1]&&path.resolve(process.argv[1])===fileURLToPath(import.meta.url)) {
  const [action,configPath,dest]=process.argv.slice(2);
  if(action==='check'){const r=await check(configAt(configPath));console.log(JSON.stringify(r,null,2));process.exitCode=r.ready?0:1;}
  else if(action==='freeze'){freeze(configAt(configPath),path.resolve(dest));}
  else if(action==='probe'){const c=configAt(configPath);const r=await probe(c,dest);Object.assign(r,{workerImage:c.workerImage,workerCompilerSha256:c.workerCompilerSha256,oracleSha256:hash(path.join(root,'evaluator/acceptance.mjs'))});write(path.join(dest,'result.json'),r);process.exitCode=r.pass?0:1;}
  else if(action==='calibrate'){const r=await calibrate(configAt(configPath),path.resolve(dest));console.log(JSON.stringify(r,null,2));process.exitCode=r.pass?0:1;}
  else if(action==='pilot'){
    const frozen=path.resolve(configPath);const authorizationSeal=verifySeal(frozen);
    assert.equal(process.argv[5],'--authorize-paid','No paid calls without explicit operator authorization');
    assert.equal(process.argv[6],authorizationSeal,'Authorization must name this exact frozen seal');
    for(const file of controllerFiles)assert.equal(hash(path.join(root,file)),hash(path.join(frozen,'evaluator',file)),'Controller changed since freeze');
    const c=configAt(path.join(frozen,'cohort.json'));const ready=await check(c);
    if(!ready.ready){console.log(JSON.stringify(ready,null,2));process.exitCode=1;}
    else{
      assert(path.resolve(dest)!==frozen&&!path.resolve(dest).startsWith(frozen+path.sep),'Results must be outside frozen campaign');
      fs.mkdirSync(dest);const campaign={planned:c.trials.map(t=>t.id),executed:[],unstarted:[],complete:false};
      for(const t of c.trials){
        const r=await runTrial(frozen,t,path.join(path.resolve(dest),t.id),{authorizationSeal});campaign.executed.push({id:t.id,status:r.status,acceptance:r.acceptance.outcome});
        write(path.join(dest,'campaign.json'),campaign);
        if(['infrastructure_error','usage_unknown'].includes(r.status)||r.acceptance.outcome==='infrastructure_error'){campaign.blocker=`Infrastructure/telemetry failed at ${t.id}; stop the cohort`;break;}
      }
      campaign.unstarted=c.trials.map(t=>t.id).filter(id=>!campaign.executed.some(t=>t.id===id));campaign.complete=campaign.unstarted.length===0;
      write(path.join(dest,'campaign.json'),campaign);process.exitCode=campaign.complete?0:1;
    }
  } else if(action==='reconcile'){
    const trial=read(configPath),receipt=read(dest);
    const raw=path.resolve(path.dirname(path.resolve(dest)),receipt.rawEvidence);
    assert.equal(hash(raw),receipt.rawEvidenceSha256,'Raw billing export hash mismatch');
    const archived=configPath.replace(/\.json$/,'.billing-evidence');fs.copyFileSync(raw,archived,fs.constants.COPYFILE_EXCL);
    receipt.rawEvidence=path.basename(archived);
    const r=reconcile(trial,receipt);r.originalTrialSha256=hash(configPath);
    fs.writeFileSync(configPath.replace(/\.json$/,'.billed.json'),JSON.stringify(r,null,2)+'\n',{flag:'wx'});
  }
  else throw Error('Usage: run.mjs check|freeze|probe|calibrate CONFIG DEST | pilot FROZEN NEW_RESULTS | reconcile TRIAL RECEIPT');
}
