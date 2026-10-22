const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const {spawnSync}=require('node:child_process');
module.exports=function rejections(binary,root,output) {
 const reports=[];
 for(const [name,line,column,token] of [['borrowed-alias',9,5,'alias.inner.score = 1'],['target-call',7,5,'items[mystery()].score = 1']]) {
  const original=path.join(root,'tests/fixtures/production/taskboard-rejections',name+'.jtml');
  const source=fs.readFileSync(original,'utf8');
  assert.equal(source.split('\n')[line-1].indexOf(token)+1,column);
  const dist=path.join(output,'rejected-'+name);fs.rmSync(dist,{recursive:true,force:true});
  const result=spawnSync(binary,['build',original,'--target','browser','--out',dist],{encoding:'utf8',timeout:20000});
  assert(!result.error&&!result.signal,'compile timeout/crash is not a fail-closed rejection');
  assert.notEqual(result.status,0);
  const diagnostic=result.stdout+result.stderr;
  assert(diagnostic.includes(`${original}:${line}:${column}`)&&diagnostic.includes('component Candidate')&&diagnostic.includes('use '));
  for(const asset of ['index.html','app.js','jtml-runtime.js','components/index.js']) assert(!fs.existsSync(path.join(dist,asset)),'rejected shape emitted runnable assets');
  reports.push({id:'reject-'+name,status:'PASS',backend:'clean-production compile',native:'INCOMPATIBLE: production ownership restriction is not a native acceptance contract',source:original,line,column,diagnostic});
 }
 return reports;
};
