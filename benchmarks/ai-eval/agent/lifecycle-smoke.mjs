// Explicit Docker regression command. No model provider is imported or called.
import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import crypto from 'node:crypto';
import {spawn} from 'node:child_process';
import {once} from 'node:events';
import {command,start,stop,shell} from './worker.mjs';
const [configFile,outArg]=process.argv.slice(2),out=path.resolve(outArg);
const config=JSON.parse(fs.readFileSync(configFile));fs.mkdirSync(out);
const result={kind:'docker-lifecycle-regression',workerImage:config.workerImage,paidApiCalls:0,agentTrials:0,pass:false,cases:[]};
const record=e=>fs.appendFileSync(path.join(out,'lifecycle.jsonl'),JSON.stringify({at:new Date().toISOString(),...e})+'\n');
async function present(name){const r=await command(['ps','-aq','--filter',`name=^/${name}$`]);assert.equal(r.exit,0,r.output);return !!r.output.trim();}
async function expires(name){const deadline=Date.now()+15000;while(await present(name)){assert(Date.now()<deadline,'Worker did not expire and auto-remove');await new Promise(r=>setTimeout(r,250));}}
async function scenario(id,run){const name=`jtml-lifecycle-${crypto.randomUUID()}`,candidate=path.join(out,id);fs.mkdirSync(candidate);fs.chmodSync(candidate,0o777);try{await run(name,candidate);result.cases.push({id,pass:true});}finally{await stop(name,record);}}
try{
 await scenario('normal',async(name,candidate)=>{
  await start({...config,wallSeconds:15},candidate,name,10000,record);
  const identity=await shell(name,'id -u; kill -STOP 1 2>/dev/null && exit 1; test "$(id -u)" = 1000',5);assert.equal(identity.exit,0,identity.output);record({kind:'protected-supervisor',...identity});
  const teardown=await stop(name,record);assert(teardown.confirmed);assert.equal(await present(name),false);
 });
 await scenario('timeout',async(name,candidate)=>{
  await start({...config,wallSeconds:3},candidate,name,10000,record);
  const r=await shell(name,'sleep 30',1);assert.equal(r.exit,124,r.output);record({kind:'shell-timeout',...r});
  await expires(name);
 });
 await scenario('controller-killed',async(name,candidate)=>{
  const workerURL=new URL('./worker.mjs',import.meta.url).href;
  const code=`import fs from 'node:fs';import {start} from ${JSON.stringify(workerURL)};await start(${JSON.stringify({...config,wallSeconds:4})},${JSON.stringify(candidate)},${JSON.stringify(name)},10000,e=>fs.appendFileSync(${JSON.stringify(path.join(out,'crashed-controller.jsonl'))},JSON.stringify(e)+'\\n'));process.send('started');setInterval(()=>{},1000);`;
  const child=spawn(process.execPath,['--input-type=module','-e',code],{stdio:['ignore','pipe','pipe','ipc']});let stderr='';child.stderr.on('data',b=>stderr+=b);
  try{
   await new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('Controller did not start: '+stderr)),10000);child.once('message',()=>{clearTimeout(timer);resolve();});child.once('exit',()=>{clearTimeout(timer);reject(Error('Controller exited: '+stderr));});});
   const exited=once(child,'exit');child.kill('SIGKILL');const [exit,signal]=await exited;assert.equal(signal,'SIGKILL');record({kind:'controller-killed',name,exit,signal});
   await expires(name);
  }finally{child.kill('SIGKILL');}
 });
 await scenario('removal-failure',async(name,candidate)=>{
  await start({...config,wallSeconds:15},candidate,name,10000,record);
  await assert.rejects(()=>stop(name,record,async(args,options)=>args[0]==='rm'?{exit:1,output:'injected removal failure'}:command(args,options)),/unconfirmed/);
  assert(await present(name));
 });
 result.pass=true;
}catch(error){result.error=String(error);process.exitCode=1;}
finally{fs.writeFileSync(path.join(out,'result.json'),JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(result,null,2));}
