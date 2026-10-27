// Independent browser-only diagnostic. Never classifies a capability automatically.
import fs from 'node:fs';
import path from 'node:path';
import {spawnSync} from 'node:child_process';
import {chromium,expect} from '@playwright/test';
import {root,serve,hash} from '../harness.mjs';
const out=path.resolve(process.argv[2]);fs.mkdirSync(out);
const source=path.join(root,'evaluator/probes/into-reset.jtml');
fs.copyFileSync(source,path.join(out,'main.jtml'));
const result={kind:'toolchain-probe',classification:'unresolved',sourceSha256:hash(source)};
let browser,server,context;
try{
 const compiler=process.env.JTML_BIN;result.compilerSha256=hash(compiler);
 const build=spawnSync(compiler,['build',path.join(out,'main.jtml'),'--target','browser','--out',path.join(out,'dist')],{encoding:'utf8',timeout:60000});
 fs.writeFileSync(path.join(out,'build.log'),(build.stdout||'')+(build.stderr||''));
 if(build.status!==0)throw new Error('Build failed');
 browser=await chromium.launch({headless:true,...(process.env.BENCH_BROWSER?{executablePath:process.env.BENCH_BROWSER}:{})});
 result.browser=browser.version();
 const serving=await serve(path.join(out,'dist'));server=serving.server;
 context=await browser.newContext();await context.tracing.start({screenshots:true,snapshots:true});
 const page=await context.newPage();await page.goto(serving.url);
 await page.getByRole('textbox',{name:'Draft'}).fill('Before reset');
 await expect(page.getByText('Draft: [Before reset]',{exact:true})).toBeVisible();
 await page.getByRole('button',{name:'Clear',exact:true}).click();
 await expect(page.getByText('Draft: []',{exact:true})).toBeVisible();
 result.stateResetVisible=true;
 try{await expect(page.getByRole('textbox',{name:'Draft'})).toHaveValue('');result.inputResetVisible=true;}
 catch(error){result.inputResetVisible=false;result.error=String(error);}
 await page.screenshot({path:path.join(out,'after-reset.png')});
 await context.tracing.stop({path:path.join(out,'trace.zip')});
}catch(error){result.error=String(error);}
finally{
 if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));
 fs.writeFileSync(path.join(out,'result.json'),JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(result,null,2));
 process.exitCode=result.stateResetVisible&&result.inputResetVisible?0:1;
}
