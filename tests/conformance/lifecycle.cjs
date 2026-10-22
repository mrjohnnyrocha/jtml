const assert=require('node:assert/strict');
const path=require('node:path');
const {pathToFileURL}=require('node:url');
// Browser-only observations. Native DOM identity is deliberately not fabricated.
module.exports=async function lifecycle(browser,dist) {
 const page=await browser.newPage();page.setDefaultTimeout(5000);
 const errors=[];page.on('pageerror',error=>errors.push(error.message));
 const trace=[];
 const settle=()=>page.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve))));
 const snapshot=()=>page.locator('.row').evaluateAll(rows=>rows.map(row=>({key:row.getAttribute('data-key'),count:row.querySelector('.child-count').textContent,draft:row.querySelector('input').value})));
 const expectedRows=(order,count=0,draft='seed')=>order.map(key=>({key:String(key),count:String(key===1?count:0),draft:key===1?draft:'seed'}));
 const record=async expected=>{const actual=await snapshot();trace.push(actual);assert.deepEqual(actual,expected);};
 try {
  await page.goto(pathToFileURL(path.join(dist,'index.html')).href);
  await page.waitForFunction(()=>window.jtml?.getComponentInstances&&document.querySelectorAll('.row').length===2);
  await record(expectedRows([1,2]));
  await page.evaluate(()=>{window.conformanceRows=[...document.querySelectorAll('.row')];window.conformanceOldButton=window.conformanceRows[0].querySelector('button');window.conformanceOldId=window.conformanceOldButton.getAttribute('data-jtml-direct-component-id');});
  const first=page.locator('.row[data-key="1"]');
  await first.getByRole('button',{name:'+',exact:true}).click();
  await first.locator('input').fill('unsaved');await settle();
  await record(expectedRows([1,2],1,'unsaved'));
  for(let cycle=0;cycle<3;cycle++) {
   await page.getByRole('button',{name:'reorder',exact:true}).click();await settle();
   await record(expectedRows([2,1],1,'unsaved'));
   assert(await page.evaluate(()=>window.conformanceRows.every(row=>row.isConnected&&row===document.querySelector(`.row[data-key="${row.getAttribute('data-key')}"]`))), 'keyed identity lost');
  }
  await page.getByRole('button',{name:'hide',exact:true}).click();await settle();await record([]);
  assert(await page.evaluate(()=>window.conformanceRows.every(row=>!row.isConnected)&&!window.jtml.findComponentInstance(window.conformanceOldId)),'removed owner remains registered');
  await page.evaluate(()=>window.conformanceOldButton.click());await settle();await record([]);
  await page.getByRole('button',{name:'reveal',exact:true}).click();await settle();
  await record(expectedRows([2,1]));
  assert(await page.evaluate(()=>window.conformanceRows.every(old=>old!==document.querySelector(`.row[data-key="${old.getAttribute('data-key')}"]`))),'remount reused removed DOM');
  await first.getByRole('button',{name:'+',exact:true}).click();await settle();await record(expectedRows([2,1],1));
  await page.getByRole('button',{name:'hide',exact:true}).click();await settle();await record([]);
  await page.getByRole('button',{name:'reveal',exact:true}).click();await settle();await record(expectedRows([2,1]));
  assert.deepEqual(errors,[]);
  assert(await page.evaluate(()=>window.jtml.browserBuild.cleanProduction&&!window.jtml.directComponentCreateFallback&&!window.jtml.directComponentPatchFallback&&!window.jtml.directComponentStaticUpdateError));
  return {id:'keyed-lifecycle',status:'PASS',backend:'browser-only',native:'UNKNOWN: DOM/retained-input identity has no normalized native observation in this slice',trace};
 } finally {await page.close();}
};
