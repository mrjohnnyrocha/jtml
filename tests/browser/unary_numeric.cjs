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
  for(const fixture of require('../conformance/unary_cases.cjs')) {
   const report=await runCase(browser,fixture);reports.push(report);
   fs.writeFileSync(path.join(output,'unary-cases.json'),JSON.stringify(reports,null,2));
   console.log(report.failures.length?'FAIL / KNOWN DEFECT CANDIDATE':'PASS',fixture.id);
  }
 } finally {await browser.close();}
 require('../conformance/catalog.cjs').verify(reports,require('../conformance/unary-browser-known-discrepancies.json').entries);
 console.log('UNARY CATALOG VERIFIED:',reports.filter(r=>!r.failures.length).length,'equivalent;',reports.filter(r=>r.failures.length).length,'FAIL / KNOWN DEFECT compiler/browser observations; all native values and DOM PASS');
 // Unary action assignment is already unsupported; preserve explicit rejection
 // and original-source remediation instead of expanding production syntax.
 const source=path.resolve(__dirname,'../conformance/fixtures/unary/unsupported-unary-assignment.jtml');
 const dist=path.join(output,'rejected-unary-assignment');fs.rmSync(dist,{recursive:true,force:true});
 const result=require('node:child_process').spawnSync(process.env.JTML_BIN||path.resolve('build/jtml'),['build',source,'--target','browser','--out',dist],{encoding:'utf8',timeout:20000});
 const diagnostic=(result.stdout||'')+(result.stderr||'');
 fs.writeFileSync(path.join(output,'unary-assignment-rejection.json'),JSON.stringify({source,status:result.status,diagnostic},null,2));
 assert(!result.error&&!result.signal&&result.status!==0,'unsupported assignment must reject without crashing');
 assert(diagnostic.includes(source+':5:5')&&diagnostic.includes('unsupported production assignment `x`')&&diagnostic.includes('use a writable path'));
 for(const name of ['index.html','app.js','jtml-runtime.js','components/index.js']) assert(!fs.existsSync(path.join(dist,name)),'rejection emitted runnable output');
 console.log('PASS fail-closed unary action assignment: UNSUPPORTED production; exact original source and no runnable assets');
 if(process.argv.includes('--strict')&&reports.some(r=>r.failures.length)) process.exitCode=2;
})().catch(error=>{console.error(error);process.exitCode=1;});
