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
  for(const fixture of require('../conformance/logical_cases.cjs')) {
   const report=await runCase(browser,fixture);reports.push(report);
   fs.writeFileSync(path.join(output,'logical-cases.json'),JSON.stringify(reports,null,2));
   console.log(report.failures.length?'FAIL':'PASS',fixture.id);
  }
 } finally {await browser.close();}
 assert.deepEqual(reports.filter(r=>r.failures.length).map(r=>({id:r.id,failures:r.failures})),[],'new logical cases require independently specified native and DOM outcomes');
 console.log('PASS',reports.length,'logical cases: original provenance, compiler reads, native state and DOM, clean production DOM/events and sibling isolation');
})().catch(error=>{console.error(error);process.exitCode=1;});
