import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import http from 'node:http';
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
import {chromium} from '@playwright/test';
import {acceptance} from './evaluator/acceptance.mjs';
export const root=path.dirname(fileURLToPath(import.meta.url));
export function hash(file){return crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');}
export function snapshot(source,dest,includeOutputs=false){
  fs.mkdirSync(dest,{recursive:true});
  for(const entry of fs.readdirSync(source,{withFileTypes:true})){
    if(['node_modules','.git',...(!includeOutputs?['dist']:[])].includes(entry.name))continue;
    const from=path.join(source,entry.name),to=path.join(dest,entry.name);
    if(entry.isSymbolicLink())throw new Error(`Symlink not allowed: ${from}`);
    if(entry.isDirectory())snapshot(from,to,includeOutputs);else fs.copyFileSync(from,to,fs.constants.COPYFILE_EXCL);
  }
}
export function manifest(dir){
 const out={};
 function walk(d){for(const e of fs.readdirSync(d,{withFileTypes:true})){
  const f=path.join(d,e.name);if(e.isSymbolicLink())throw new Error(`Symlink not allowed: ${f}`);if(e.isDirectory())walk(f);else out[path.relative(dir,f)]=hash(f);
 }} walk(dir);return out;
}
export function validate(task,stack){if(!['01','02'].includes(task)||!['jtml','react'].includes(stack))throw new Error('Only pilot tasks 01/02 and stacks jtml/react are executable');}
export function prepare(task,stack,dest){
 validate(task,stack);fs.mkdirSync(dest);snapshot(path.join(root,'starters',task,stack),dest);
 fs.copyFileSync(path.join(root,'tasks',`${task}.md`),path.join(dest,'TASK.md'));
 fs.writeFileSync(path.join(dest,'STACK.md'), stack==='jtml' ? 'Use Friendly JTML 2. Entry: main.jtml. Build: "$JTML_BIN" build main.jtml --target browser --out dist\n' : 'Use React with JSX and Vite. Entry: index.html and src.jsx. Build: vite build\n');
}
export async function serve(dir){
 const server=http.createServer((req,res)=>{
  try {
   const pathname=decodeURIComponent(new URL(req.url,'http://localhost').pathname);
   const file=path.resolve(dir,'.'+(pathname==='/'?'/index.html':pathname));
   if(!file.startsWith(dir+path.sep)){res.writeHead(403).end();return;}
   const types={'.html':'text/html','.js':'text/javascript','.css':'text/css','.json':'application/json'};
   res.setHeader('Content-Type',types[path.extname(file)]||'application/octet-stream');
   res.end(fs.readFileSync(file));
  }catch{res.writeHead(404).end();}
 });
 await new Promise((resolve,reject)=>{server.once('error',reject);server.listen(0,'127.0.0.1',resolve);});
 return {server,url:`http://127.0.0.1:${server.address().port}`};
}
export async function evaluate(task,stack,source,out){
 validate(task,stack);source=path.resolve(source);out=path.resolve(out);
 if(out===source||out.startsWith(source+path.sep))throw new Error('Evidence must be outside candidate');
 fs.mkdirSync(out); // Exclusive: failed runs are never silently overwritten.
 const began=Date.now();
 const result={schemaVersion:1,kind:'harness-validation',task,stack,startedAt:new Date().toISOString(),outcome:'infrastructure_error',checks:[],agentMetrics:null};
 let browser,server;
 try {
  snapshot(source,path.join(out,'source'));
  result.sourceHashes=manifest(path.join(out,'source'));
  result.harnessHashes={};
  for(const f of ['harness.mjs','evaluator/acceptance.mjs','package.json','package-lock.json','protocol.json',`tasks/${task}.md`]){
   result.harnessHashes[f]=hash(path.join(root,f));
   const archived=path.join(out,'evaluator-source',f);
   fs.mkdirSync(path.dirname(archived),{recursive:true});fs.copyFileSync(path.join(root,f),archived);
  }
  fs.copyFileSync(path.join(root,'tasks',`${task}.md`),path.join(out,'TASK.md'));
  fs.copyFileSync(path.join(root,'protocol.json'),path.join(out,'protocol.json'));
  result.node=process.version;result.platform=`${process.platform}/${process.arch}`;
  const candidate=path.join(out,'source'),dist=path.join(candidate,'dist');
  let command,args;
  if(stack==='jtml'){
   command=process.env.JTML_BIN;if(!command)throw new Error('JTML_BIN required');
   if(process.env.JTML_COMPILER_SHA&&!/^[a-f0-9]{40}$/.test(process.env.JTML_COMPILER_SHA))throw new Error('JTML_COMPILER_SHA must be a full source commit');
   result.compiler={sourceCommit:process.env.JTML_COMPILER_SHA||null,sha256:hash(command),version:spawnSync(command,['--version'],{encoding:'utf8'}).stdout?.trim()};
   args=['build','main.jtml','--target','browser','--out',dist];
  }else{
   // Dependencies are provisioned by the evaluator, never by candidate package scripts.
   fs.symlinkSync(path.join(root,'node_modules'),path.join(candidate,'node_modules'),'dir');
   command=process.execPath;args=[path.join(root,'node_modules/vite/bin/vite.js'),'build','--outDir',dist];
  }
  result.buildCommand=[command,...args];
  const build=spawnSync(command,args,{cwd:candidate,encoding:'utf8',timeout:60000,maxBuffer:16*1024*1024});
  fs.writeFileSync(path.join(out,'build.log'),(build.stdout||'')+(build.stderr||'')+(build.error||''));
  result.buildExit=build.status;
  if(build.error)throw build.error;
  if(build.status!==0){result.outcome='build_failure';return result;}
  result.outputHashes=manifest(dist);
  browser=await chromium.launch({headless:true,...(process.env.BENCH_BROWSER?{executablePath:process.env.BENCH_BROWSER}:{})});
  result.browser=browser.version();
  const serving=await serve(dist);server=serving.server;
  result.checks=await acceptance(browser,serving.url,task,out);
  result.outcome=result.checks.every(c=>c.pass)?'pass':'functional_failure';
  result.baselineFailures=result.checks.filter(c=>c.id.startsWith('baseline.')&&!c.pass).map(c=>c.id);
 }catch(error){result.error=String(error);}
 finally{
  if(browser)await browser.close().catch(()=>{});
  if(server)await new Promise(resolve=>server.close(resolve));
  result.evaluatorLatencyMs=Date.now()-began;
  fs.writeFileSync(path.join(out,'result.json'),JSON.stringify(result,null,2)+'\n');
 }
 return result;
}
if(process.argv[1]&&path.resolve(process.argv[1])===fileURLToPath(import.meta.url)){
 const [command,task,stack,source,out]=process.argv.slice(2);
 if(command==='prepare')prepare(task,stack,path.resolve(source));
 else if(command==='evaluate'){
  const result=await evaluate(task,stack,source,out);console.log(JSON.stringify(result,null,2));process.exitCode=result.outcome==='pass'?0:1;
 }else throw new Error('Usage: node harness.mjs prepare TASK STACK DEST | evaluate TASK STACK SOURCE EVIDENCE');
}
