const assert=require('node:assert/strict');
const fs=require('node:fs'),path=require('node:path');
const {spawnSync}=require('node:child_process');
const {pathToFileURL}=require('node:url');
const {chromium}=require(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT ? path.join(process.env.JTML_BROWSER_BENCH_PLAYWRIGHT_ROOT,'node_modules/playwright'):'playwright');
const {runCase}=require('../conformance/runner.cjs');
(async()=>{
 const browser=await chromium.launch({headless:true,executablePath:process.env.JTML_BENCH_BROWSER||undefined});
 const reports=[];
 try {
  for(const fixture of require('../conformance/attribute_cases.cjs')) {
   const report=await runCase(browser,fixture);
   // Complete attributes are accepted by Friendly lowering and production.
   // The legacy body-plan renderer is a distinct interface: retain its exact
   // known-defect observation, never call it an equivalent semantic pass.
   const complete=fixture.id.endsWith('-6');
   if(complete) {
    assert.deepEqual(report.failures,[{layer:'native',observation:'visible',
     expected:[[{result:'2',heading:['Revenue']}]],
     actual:[[{result:'2',heading:[fixture.id.split('-')[1]+' data-label="marker"']}]]}],
     'changed legacy complete-attribute failure requires review');
   } else assert.deepEqual(report.failures,[],JSON.stringify(report.failures));
   const lint=spawnSync(process.env.JTML_BIN||path.resolve('build/jtml'),['lint',report.source],{encoding:'utf8',timeout:20000});
   assert.equal(lint.status,0,lint.stdout+lint.stderr);
   const page=await browser.newPage();
   try {
    await page.goto(pathToFileURL(path.join(path.dirname(report.source),'browser/index.html')).href);
    const heading=page.locator(fixture.views.heading);
    await heading.waitFor();
    const tail=fixture.id.startsWith('parameter-')?3:Number(fixture.id.split('-').pop());
    if([0,3,6].includes(tail)) assert.equal(await heading.getAttribute('data-label'),'marker');
    if([1,3,6].includes(tail)) assert.equal(await heading.getAttribute('aria-label'),'accessible');
    if([2,3].includes(tail)) assert.equal(await heading.getAttribute('style'),'color: red');
    if(tail===4) assert.equal(await heading.getAttribute('disabled'),'');
    if(tail===5) {
     await heading.click();
     await page.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve))));
     assert.equal(await page.locator('.out-result').textContent(),'3','bound event action did not update');
    }
   } finally {await page.close();}
   reports.push({id:fixture.id,status:complete?'KNOWN DEFECT native body-plan visible interface; PASS lowering/lint/browser':'PASS',source:report.source,failures:report.failures});
   fs.writeFileSync(path.join(path.dirname(path.dirname(report.source)),'attribute-cases.json'),JSON.stringify(reports,null,2));
  }
 } finally {await browser.close();}
 const output=process.env.JTML_CONFORMANCE_OUT||path.resolve('dist/conformance');
 fs.mkdirSync(output,{recursive:true});fs.writeFileSync(path.join(output,'attribute-cases.json'),JSON.stringify(reports,null,2));
 console.log('PASS',reports.filter(r=>r.status==='PASS').length,'equivalent attribute/content collisions');
 console.log('FAIL / KNOWN DEFECT',reports.filter(r=>r.status!=='PASS').length,'native body-plan complete-attribute visible observations (one independently reproduced existing-main defect); lowering/lint/native diagnostics/browser PASS, actual discrepancies retained in traces');
})().catch(error=>{console.error(error);process.exitCode=1;});
