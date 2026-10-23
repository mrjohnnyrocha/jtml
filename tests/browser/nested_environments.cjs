const assert=require('node:assert/strict');
const fs=require('node:fs'),path=require('node:path');
const {chromium}=require(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT?path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT,'node_modules/playwright'):'playwright');
const {runCase}=require('../conformance/runner.cjs');
(async()=>{
 const output=process.env.JTML_CONFORMANCE_OUT||path.resolve('dist/conformance');
 fs.mkdirSync(output,{recursive:true});
 const browser=await chromium.launch({headless:true,executablePath:process.env.JTML_BENCH_BROWSER||undefined});
 const reports=[];
 try {
  for(const fixture of require('../conformance/nested_cases.cjs')) {
   const report=await runCase(browser,fixture);reports.push(report);
   fs.writeFileSync(path.join(output,'nested-cases.json'),JSON.stringify(reports,null,2));
   console.log(report.failures.length?'FAIL':'PASS',fixture.id);
  }
 } finally {await browser.close();}
 assert.deepEqual(reports.filter(r=>r.failures.length).map(r=>({id:r.id,failures:r.failures})),[],'new nested ownership cases require independently specified native and DOM outcomes');
 console.log('PASS',reports.length,'nested ownership cases: original provenance, compiler reads, native state and DOM, clean production DOM/events and sibling isolation');
 // A separately proven Friendly lowering capture is outside the native repair.
 // Keep its independent 100*2 oracle and exact reviewed signature visible.
 const source=path.resolve(__dirname,'../conformance/fixtures/nested/known-parameter-capture.jtml');
 const directory=path.join(output,'known-native-parameter-capture');fs.mkdirSync(directory,{recursive:true});
 const binary=process.env.JTML_BIN||path.resolve('build/jtml');
 const probe=process.env.JTML_CONFORMANCE_PROBE||path.join(path.dirname(binary),'tests/jtml_conformance_probe');
 const request=path.join(directory,'request.json'),result=path.join(directory,'native.json');
 fs.writeFileSync(request,JSON.stringify({steps:[]}));
 const command=require('node:child_process').spawnSync(probe,[source,request,result],{encoding:'utf8',timeout:20000});
 const log=(command.stdout||'')+(command.stderr||'');fs.writeFileSync(path.join(directory,'native.log'),log);
 assert(!command.error&&!command.signal&&command.status===0,'capture probe must not crash');
 assert(!log.includes('Interpreter Error:'),'new diagnostic cannot hide behind the lowering counterexample');
 const state=JSON.parse(fs.readFileSync(result)).snapshots[0];
 const parent=state.components.find(c=>c.component==='Parent'),child=state.components.find(c=>c.component==='Child');
 const expected={parentCount:100,childCount:10,result:200};
 const actual={parentCount:parent?.locals.count?.value,childCount:child?.locals.count?.value,result:child?.locals.result?.value};
 const report={id:'native-parameter-capture',sha256:require('node:crypto').createHash('sha256').update(fs.readFileSync(source)).digest('hex'),expected,failures:JSON.stringify(actual)===JSON.stringify(expected)?[]:[{layer:'native-lowering',observation:'expanded-state',actual}]};
 fs.writeFileSync(path.join(directory,'trace.json'),JSON.stringify(report,null,2));
 require('../conformance/catalog.cjs').verify([report],require('../conformance/nested-known-discrepancies.json').entries);
 console.log('FAIL / KNOWN DEFECT: separate Friendly parameter capture; native expanded result 20, independent expected 200; exact catalog verified');
 if(process.argv.includes('--strict')) process.exitCode=2;

})().catch(error=>{console.error(error);process.exitCode=1;});
