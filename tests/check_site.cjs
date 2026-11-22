/* SPDX-License-Identifier: GPL-3.0-only */
/* Real DOS executable + WebAssembly, served at the GitHub Pages subpath. */
'use strict';
const assert=require('node:assert/strict'), fs=require('node:fs'),path=require('node:path'),http=require('node:http');
const {chromium}=require('playwright');
const root=path.resolve(__dirname,'../build/site'),diagnostics=path.resolve(__dirname,'../build/site-check');
const prefix='/nightwatch/';
const mime={'.html':'text/html','.js':'application/javascript','.wasm':'application/wasm',
  '.css':'text/css','.json':'application/json','.svg':'image/svg+xml','.png':'image/png'};
async function snapshot(page){
  return page.evaluate(()=>{
    const c=document.getElementById('screen'),data=c.getContext('2d').getImageData(0,0,c.width,c.height).data;
    let hash=2166136261;const colors=new Set();
    for(let i=0;i<data.length;i+=4){const color=data[i]*65536+data[i+1]*256+data[i+2];colors.add(color);hash=Math.imul(hash^color,16777619);}
    return {hash:hash>>>0,colors:colors.size,width:c.width,height:c.height};
  });
}
async function until(page,predicate,label){
  const deadline=Date.now()+30000;let previous;
  do{
    const state=await snapshot(page);
    if(predicate(state)&&state.hash===previous) return state;
    previous=predicate(state)?state.hash:undefined;await page.waitForTimeout(150);
  }while(Date.now()<deadline);
  throw new Error('Timed out: '+label+'; '+JSON.stringify(await snapshot(page)));
}
async function key(page,name){
  await page.locator('#screen').focus();await page.keyboard.press(name,{delay:90});
  await page.waitForTimeout(180);
}
async function main(){
  fs.mkdirSync(diagnostics,{recursive:true});
  const errors=[],failed=[],remote=[],consoleMessages=[];
  const server=http.createServer((request,response)=>{
    const url=new URL(request.url,'http://localhost');
    if(!url.pathname.startsWith(prefix)){response.writeHead(404).end();return;}
    const file=path.resolve(root,decodeURIComponent(url.pathname.slice(prefix.length))||'index.html');
    if(!file.startsWith(root+path.sep)){response.writeHead(403).end();return;}
    try{const data=fs.readFileSync(file);response.setHeader('Content-Type',mime[path.extname(file)]||'application/octet-stream');response.end(data);}
    catch(_){response.writeHead(404).end('Not found');}
  });
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  let browser,page;
  try{
    browser=await chromium.launch({headless:true,...(process.env.CHROMIUM_PATH?{executablePath:process.env.CHROMIUM_PATH}:{})});
    page=await browser.newPage({viewport:{width:1440,height:1080}});
    page.setDefaultTimeout(15000);
    page.on('pageerror',error=>errors.push(error.message));
    page.on('console',message=>consoleMessages.push(message.text()));
    page.on('response',response=>{if(response.status()>=400) failed.push(response.url());});
    page.on('requestfailed',request=>failed.push(request.url()));
    page.on('request',request=>{if(!request.url().startsWith('http://127.0.0.1:')&&!/^(blob:|data:)/.test(request.url())) remote.push(request.url());});
    const url=`http://127.0.0.1:${server.address().port}${prefix}`;
    await page.goto(url);await page.screenshot({path:path.join(diagnostics,'desktop.png')});
    assert.equal(await page.locator('.capture').count(),61);
    assert.equal(await page.locator('.capture:visible').count(),12);
    await page.click('[data-filter="transfers"]');
    const count=await page.locator('#gallery-count').textContent();assert(!count.startsWith('0 '));
    await page.locator('.capture:visible a').first().click();
    assert(await page.locator('#lightbox').isVisible());
    const first=await page.locator('#lightbox-image').getAttribute('src');
    await page.keyboard.press('ArrowRight');
    assert.notEqual(await page.locator('#lightbox-image').getAttribute('src'),first);
    await page.keyboard.press('Escape');assert(!await page.locator('#lightbox').isVisible());
    await page.click('[data-filter="all"]');await page.click('#show-more');
    assert.equal(await page.locator('.capture:visible').count(),61);
    console.log('PASS: gallery filtering, expansion and keyboard lightbox');
    await page.click('#start');await page.locator('#cover').waitFor({state:'hidden',timeout:30000});
    const desktop=await until(page,s=>s.width===640&&s.height===480&&s.colors>=10,'VGA file browser');
    await page.locator('#screen').screenshot({path:path.join(diagnostics,'running.png')});
    console.log('PASS: native VGA application boot');
    await key(page,'F9');await until(page,s=>s.hash!==desktop.hash,'F9 drive picker');
    await page.locator('#screen').screenshot({path:path.join(diagnostics,'drive-picker.png')});
    await key(page,'Escape');await until(page,s=>s.hash===desktop.hash,'picker cancellation');
    await key(page,'F9');await key(page,'e');await key(page,'Enter');
    await until(page,s=>s.hash!==desktop.hash,'select backup drive');
    await key(page,'F9');await key(page,'.');await key(page,'Control+u');
    await page.keyboard.type('D:'+String.fromCharCode(92),{delay:60});
    await page.waitForTimeout(180);
    await page.locator('#screen').screenshot({path:path.join(diagnostics,'drive-path.png')});
    await key(page,'Enter');
    await until(page,s=>s.hash===desktop.hash,'picker direct-path entry');
    console.log('PASS: browser F9 picker, drive selection, direct path and Escape');
    await key(page,'Control+a');await until(page,s=>s.hash!==desktop.hash,'file properties');
    await key(page,'r');await key(page,'Escape');
    await until(page,s=>s.hash===desktop.hash,'attribute edits cancel');
    await key(page,'Alt+F7');await until(page,s=>s.hash!==desktop.hash,'recursive search prompt');
    await key(page,'Escape');await until(page,s=>s.hash===desktop.hash,'search cancels');
    // The Size button bypasses the browser-reserved Ctrl-L shortcut.
    await page.click('[data-combo="341,76"]');
    await until(page,s=>s.hash!==desktop.hash,'recursive folder byte total');
    await key(page,'Control+r');await until(page,s=>s.hash===desktop.hash,'refresh clears measured selection and resamples capacity');
    console.log('PASS: properties cancellation, Alt-F7 and recursive selection totals');

    if(await page.locator('#fullscreen').isVisible()){
      await page.click('#fullscreen');await page.waitForFunction(()=>document.fullscreenElement?.id==='player');
      await page.evaluate(()=>document.exitFullscreen());
    }
    await key(page,'F1');await until(page,s=>s.hash!==desktop.hash,'help');
    await key(page,'Escape');await until(page,s=>s.hash===desktop.hash,'help closes');
    await key(page,'Tab');await until(page,s=>s.hash!==desktop.hash,'switch pane');
    await key(page,'Tab');await until(page,s=>s.hash===desktop.hash,'switch back');
    await key(page,'Control+f');await page.keyboard.type('TODO.TXT',{delay:60});await key(page,'Enter');
    await key(page,'F3');
    const textView=await snapshot(page);assert.notEqual(textView.hash,desktop.hash);
    await key(page,'Control+g');await key(page,'0');await key(page,'Enter');
    await key(page,'Escape');
    await key(page,'Shift+F3');
    const hexView=await snapshot(page);assert.notEqual(hexView.hash,textView.hash);
    await key(page,'F4');await until(page,s=>s.hash===textView.hash,'hex-to-text toggle');
    await key(page,'Escape');
    console.log('PASS: Help, panes, physical Ctrl/Shift modifiers and text/hex viewers');
    await key(page,'F5');await key(page,'Enter');await page.waitForTimeout(500);
    // Read the guest filesystem to prove a real copy occurred.
    const copied=await page.evaluate(async()=>Array.from(await Promise.race([
      engine.fsReadFile('BACKUP/TODO.TXT'),new Promise((_,reject)=>setTimeout(()=>reject(new Error('Guest filesystem read timed out')),5000))])));
    assert(Buffer.from(copied).includes(Buffer.from('FIELD OFFICE')));
    console.log('PASS: copying a real file between demo drives');
    await page.click('#pause');assert.equal(await page.locator('#pause').getAttribute('aria-pressed'),'true');
    const frozen=await snapshot(page);await key(page,'ArrowDown');assert.equal((await snapshot(page)).hash,frozen.hash);
    await page.click('#pause');
    page.once('dialog',dialog=>dialog.accept());await page.click('#restart');
    await until(page,s=>s.hash===desktop.hash,'reset restores drives and panes');
    // Exact framebuffer equality also verifies that the backup pane has its
    // original five entries again, without the copied TODO.TXT.
    await key(page,'F10');await page.locator('#cover').waitFor({state:'visible',timeout:30000});
    assert(await page.locator('#restart').isDisabled());
    await page.setViewportSize({width:390,height:844});await page.reload();
    assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
    await page.screenshot({path:path.join(diagnostics,'mobile.png'),fullPage:true});
    await page.click('#start');await until(page,s=>s.width===640&&s.height===480&&s.colors>=10,'mobile boot');
    await page.locator('[data-key="290"]').click();await until(page,s=>s.hash!==desktop.hash,'touch help');
    await page.locator('[data-key="256"]').click();await until(page,s=>s.hash===desktop.hash,'touch Escape');
    await page.locator('[data-key="264"]').click();await until(page,s=>s.hash!==desktop.hash,'touch selection');
    await key(page,'Control+f');await page.keyboard.type('TODO.TXT',{delay:60});await key(page,'Enter');
    await page.locator('[data-modifier="340"]').click();
    assert.equal(await page.locator('[data-modifier="340"]').getAttribute('aria-pressed'),'true');
    await page.locator('[data-key="292"]').click();
    await until(page,s=>s.hash===hexView.hash,'touch Shift-F3');
    assert.equal(await page.locator('[data-modifier="340"]').getAttribute('aria-pressed'),'false');
    assert.deepEqual(errors,[],'browser exceptions');assert.deepEqual(failed,[],'missing resources');assert.deepEqual(remote,[],'runtime must be self-hosted');
    assert.equal((await page.request.get(url+'NIGHTWATCH-DOS.zip')).status(),200);
    console.log('PASS: real VGA boot, keyboard/modifiers, viewer, file copy, pause, reset, exit, touch, gallery and Pages subpath.');
  }catch(error){
    if(page) await page.screenshot({path:path.join(diagnostics,'failure.png'),fullPage:true}).catch(()=>{});
    if(page) await page.locator('#player').screenshot({path:path.join(diagnostics,'failure-player.png')}).catch(()=>{});
    if(page) console.error(await page.locator('#status').textContent(),await page.locator('#error').textContent());
    throw error;
  }finally{
    fs.writeFileSync(path.join(diagnostics,'browser.json'),JSON.stringify({errors,failed,remote,consoleMessages},null,2));
    if(browser) await browser.close();await new Promise(resolve=>server.close(resolve));
  }
}
main().catch(error=>{console.error(error);process.exitCode=1;});
