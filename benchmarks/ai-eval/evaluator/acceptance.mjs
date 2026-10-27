import {expect} from '@playwright/test';

// Public pilot oracle. Never imported by an agent workspace. No stack-specific paths.
export async function acceptance(browser, origin, task, evidence) {
  const results = [];
  const scenarios = {
    'baseline.validation-and-submit': async page => {
      await page.getByRole('button', {name:'Reserve', exact:true}).click();
      await expect(page.getByText('Name required', {exact:true})).toBeVisible();
      const input = page.getByRole('textbox', {name:'Guest name', exact:true});
      await input.fill('Ada <b>Lovelace</b>');
      await expect(input).toBeFocused();
      await input.press('Enter');
      await expect(page.getByText(task === '01' ? 'Reserved for Ada <b>Lovelace</b>' : 'Reserved for Ada <b>Lovelace</b> (1 guests)', {exact:true})).toBeVisible();
      await expect(input).toHaveValue('Ada <b>Lovelace</b>');
      await expect(page.locator('b')).toHaveCount(0);
      await input.fill('Grace');
      await expect(page.getByText(task === '01' ? 'Reserved for Ada <b>Lovelace</b>' : 'Reserved for Ada <b>Lovelace</b> (1 guests)', {exact:true})).toBeVisible();
    },
     'baseline.reset-confirmation': async page => {
      await page.getByRole('textbox',{name:'Guest name',exact:true}).fill('Sam');
      await page.getByRole('button',{name:'Reserve',exact:true}).click();
      await expect(page.getByText(task==='01'?'Reserved for Sam':'Reserved for Sam (1 guests)',{exact:true})).toBeVisible();
      await page.getByRole('button',{name:'Reset',exact:true}).click();
      await expect(page.getByText('Not reserved',{exact:true})).toBeVisible();
    },
    'baseline.reset-and-reload': async page => {
      const input=page.getByRole('textbox',{name:'Guest name',exact:true});
      await expect(input).toHaveValue('');
      await expect(page.getByText('Not reserved',{exact:true})).toBeVisible();
      await input.fill('Lin');
      await page.getByRole('button',{name:'Reserve',exact:true}).click();
      await page.getByRole('button',{name:'Reset',exact:true}).click();
      await expect(input).toHaveValue('');
      await expect(page.getByText('Not reserved',{exact:true})).toBeVisible();
      await input.fill('New draft');
      await page.reload();
      await expect(input).toHaveValue('');
      await expect(page.getByText('Not reserved',{exact:true})).toBeVisible();
    }
  };
  if(task==='02') scenarios['extension.bounds-total-snapshot-reset'] = async page => {
    const add=page.getByRole('button',{name:'Add guest',exact:true});
    const remove=page.getByRole('button',{name:'Remove guest',exact:true});
    const check=async n=>{
      await expect(page.getByText(`Guests: ${n}`,{exact:true})).toBeVisible();
      await expect(page.getByText(`Total: EUR ${n*20}`,{exact:true})).toBeVisible();
    };
    await check(1);
    // Disabled boundary controls are also valid implementations.
    if(await remove.isEnabled()) await remove.click();
    await check(1);
    for(let n=2;n<=6;n++){await add.click();await check(n);}
    if(await add.isEnabled()) await add.click();
    await check(6);
    await page.getByRole('textbox',{name:'Guest name',exact:true}).fill('Jo');
    await page.getByRole('button',{name:'Reserve',exact:true}).click();
    await expect(page.getByText('Reserved for Jo (6 guests)',{exact:true})).toBeVisible();
    await remove.click(); await check(5);
    await expect(page.getByText('Reserved for Jo (6 guests)',{exact:true})).toBeVisible();
    await page.getByRole('button',{name:'Reserve',exact:true}).click();
    await expect(page.getByText('Reserved for Jo (5 guests)',{exact:true})).toBeVisible();
    await page.getByRole('button',{name:'Reset',exact:true}).click();
    await check(1);
    await expect(page.getByText('Not reserved',{exact:true})).toBeVisible();
    await expect(page.getByRole('textbox',{name:'Guest name',exact:true})).toHaveValue('');
  };
  for (const [id,run] of Object.entries(scenarios)) {
    const context=await browser.newContext();
    await context.tracing.start({screenshots:true,snapshots:true,sources:true});
    const page=await context.newPage(); page.setDefaultTimeout(5000);
    const errors=[]; page.on('pageerror',e=>errors.push(e.message));
    let navigations=0;
    page.on('framenavigated',f=>{if(f===page.mainFrame())navigations++;});
    try {
      await page.goto(origin); await expect(page.getByRole('heading',{name:'Reservation',exact:true})).toBeVisible(); await run(page);
      expect(errors).toEqual([]);
      expect(navigations).toBe(id.endsWith('reload') ? 2 : 1);
      results.push({id,pass:true});
    } catch(error) { results.push({id,pass:false,error:String(error)}); }
    await page.screenshot({path:`${evidence}/${id}.png`,fullPage:true}).catch(()=>{});
    await context.tracing.stop({path:`${evidence}/${id}.zip`});
    await context.close();
  }
  return results;
}
