import {spawn} from 'node:child_process';
import crypto from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';

export function command(args, {timeout=60000, maxBytes=1024*1024} = {}) {
  return new Promise((resolve, reject) => {
    const child = spawn('docker', args, {stdio:['ignore','pipe','pipe']});
    const chunks = []; let bytes=0, truncated=false, timedOut=false;
    const add = chunk => { if(bytes < maxBytes) chunks.push(chunk.subarray(0, maxBytes-bytes)); bytes += chunk.length; if(bytes>maxBytes)truncated=true; };
    child.stdout.on('data', add); child.stderr.on('data', add);
    const timer = setTimeout(() => {timedOut=true;child.kill('SIGKILL');}, Math.max(1, timeout));
    child.on('error', error => {clearTimeout(timer);reject(error);});
    child.on('close', (exit,signal) => {clearTimeout(timer);resolve({exit,signal,timedOut,truncated,output:Buffer.concat(chunks).toString()});});
  });
}
export function isolationArgs(config, name) {
  return ['--name',name,'--pull','never','--network','none','--read-only','--cap-drop','ALL','--security-opt','no-new-privileges',
    '--pids-limit',String(config.pids),'--memory',config.memory,'--cpus',String(config.cpus),'--tmpfs','/tmp:rw,nosuid,size=512m','--user','1000:1000','--workdir','/work'];
}
export function bind(source, target, readonly=false) {
  if (source.includes(',')) throw new Error('Comma in mount path is unsupported');
  return ['--mount',`type=bind,src=${path.resolve(source)},dst=${target}${readonly?',readonly':''}`];
}
export async function start(config, candidate, name=`jtml-eval-${crypto.randomUUID()}`, timeout=60000, record=()=>{}) {
  return startIsolated(config,bind(candidate,'/work'),name,timeout,record);
}
export async function startIsolated(config, mounts, name, timeout=60000, record=()=>{}) {
  const seconds=Math.ceil(config.wallSeconds);
  if(!Number.isFinite(seconds)||seconds<1||seconds>900)throw Error('Worker lifetime must be 1..900 seconds');
  // Root-owned PID 1 cannot be stopped by uid 1000 tool commands. Its exit kills
  // the entire PID namespace; Docker --rm removes it even if the controller dies.
  const args=['run','-d','--rm',...isolationArgs(config,name),'--user','0:0','--entrypoint','/bin/sleep',...mounts,config.workerImage,String(seconds)];
  record({kind:'worker-start-intent',name,lifetimeSeconds:seconds,args});
  const r=await command(args,{timeout});
  record({kind:'worker-start-result',name,...r});
  if(r.exit!==0){await stop(name,record);throw new Error(r.output);}
  return name;
}
export async function stop(name, record=()=>{}, execute=command) {
  const removal=await execute(['rm','-f',name],{timeout:10000});
  record({kind:'worker-removal',name,...removal});
  // Auto-removal may already have won the race. Only a successful daemon query
  // proving absence permits source inspection; a failed rm alone proves nothing.
  const verification=await execute(['ps','-aq','--no-trunc','--filter',`name=^/${name}$`],{timeout:10000});
  record({kind:'worker-teardown',name,...verification});
  if(verification.exit!==0||verification.timedOut||verification.output.trim())throw Error(`Worker teardown unconfirmed: ${name}; removal=${removal.exit}, verification=${verification.exit}`);
  return {name,confirmed:true,removal,verification};
}
export async function shell(name, text, seconds) {
  // timeout inside the container terminates command descendants; final rm kills the entire worker.
  return command(['exec','--user','1000:1000',name,'timeout','-k','1',String(seconds),'bash','-c',text],{timeout:(seconds+2)*1000});
}
export async function probe(config, directory) {
  fs.mkdirSync(directory);
  const candidate=path.join(directory,'candidate');fs.mkdirSync(candidate,{mode:0o777});
  const name=`jtml-probe-${crypto.randomUUID()}`; const record=value=>fs.appendFileSync(path.join(directory,'lifecycle.jsonl'),JSON.stringify({at:new Date().toISOString(),...value})+'\n'); const result={kind:'isolation-smoke',pass:false,checks:[]};
  try {
    await start(config,candidate,name,60000,record);
    const checks={
      'compiler-hash':'sha256sum /opt/jtml/bin/jtml',
      'root-readonly':'if touch /opt/should-not-write 2>/dev/null; then exit 1; fi',
      'no-evaluator-or-host':'test ! -e /var/run/docker.sock && test ! -e /opt/evaluator/harness.mjs && test ! -e /Users && test ! -e /source && test ! -e /work/TASK.md',
      'no-credentials':"node -e \"if(Object.keys(process.env).some(k=>/API_KEY|ADMIN_KEY/.test(k)))process.exit(1)\"",
      'no-network':"node -e \"const s=require('net').connect(443,'1.1.1.1');s.on('connect',()=>process.exit(1));s.on('error',()=>process.exit(0));setTimeout(()=>process.exit(1),2000)\"",
      'local-browser':"node -e \"const {chromium}=require('/opt/deps/node_modules/playwright');(async()=>{const b=await chromium.launch({headless:true});const p=await b.newPage();await p.setContent('<p>ready</p>');if(await p.textContent('p')!=='ready')throw Error('renderer');await b.close()})().catch(()=>process.exit(1))\""
    };
    for(const [id,cmd]of Object.entries(checks)) {
      const r=await shell(name,cmd,30);const pass=r.exit===0&&(id!=='compiler-hash'||r.output.startsWith(config.workerCompilerSha256));
      result.checks.push({id,pass,...r});
    }
    const inspect=await command(['inspect',name]);
    fs.writeFileSync(path.join(directory,'inspect.json'),inspect.output);
    const i=JSON.parse(inspect.output)[0];
    result.inspectValid=i.HostConfig.NetworkMode==='none'&&i.HostConfig.ReadonlyRootfs&&i.Mounts.length===1&&i.Mounts[0].Destination==='/work';
    result.pass=result.inspectValid&&result.checks.every(c=>c.pass);
  } catch(error) {result.error=String(error);}
  finally {try{result.teardown=await stop(name,record);}catch(error){result.pass=false;result.error=String(error);}fs.writeFileSync(path.join(directory,'result.json'),JSON.stringify(result,null,2)+'\n');}
  return result;
}
