/* SPDX-License-Identifier: GPL-3.0-only */
/* DOSBox worker/frame handling follows the msdos-kakuro browser emulator. */
'use strict';
const $ = id => document.getElementById(id);
const canvas = $('screen'), ctx = canvas.getContext('2d', {alpha:false});
let engine = null, loading = false, paused = false, generation = 0, frame, dirty = false, scriptPromise;
const held = new Set(), modifiers = new Set();
const keys = {Enter:257,NumpadEnter:257,Escape:256,Tab:258,Backspace:259,Insert:260,
  Delete:261,ArrowRight:262,ArrowLeft:263,ArrowDown:264,ArrowUp:265,PageUp:266,
  PageDown:267,Home:268,End:269,Space:32,ShiftLeft:340,ShiftRight:344,
  ControlLeft:341,ControlRight:345,AltLeft:342,AltRight:346,Minus:45,Equal:61,
  Backslash:92,BracketLeft:91,BracketRight:93,Semicolon:59,Quote:39,
  Period:46,Comma:44,Slash:47,Backquote:96};
for(let i=1;i<=12;i++) keys['F'+i] = 289+i;
for(let i=0;i<10;i++) {keys['Digit'+i]=48+i;keys['Numpad'+i]=48+i;}
for(let i=0;i<26;i++) keys['Key'+String.fromCharCode(65+i)]=65+i;
function status(text){$('status').textContent=text;}
function releaseKeys(){
  for(const key of held) engine?.sendKeyEvent(key,false);
  held.clear(); modifiers.clear();
  for(const button of document.querySelectorAll('[data-modifier]')) button.setAttribute('aria-pressed','false');
}
function buttons(){
  for(const id of ['pause','restart']) $(id).disabled=!engine||loading;
  for(const button of document.querySelectorAll('[data-key],[data-modifier]')) button.disabled=!engine||loading||paused;
  $('pause').textContent=paused?'Resume':'Pause';
  $('pause').setAttribute('aria-pressed',String(paused));
}
function resize(width,height){
  if(width<1||height<1) return;
  canvas.width=width;canvas.height=height;frame=ctx.createImageData(width,height);
}
function draw(rgb,rgba){
  if(!frame) return;
  if(rgba) frame.data.set(rgba);
  else if(rgb) for(let i=0,j=0;i<rgb.length;i+=3,j+=4){
    frame.data[j]=rgb[i];frame.data[j+1]=rgb[i+1];frame.data[j+2]=rgb[i+2];frame.data[j+3]=255;
  }
  dirty=true;
}
function paint(){
  if(dirty&&frame){ctx.putImageData(frame,0,0);dirty=false;}
  requestAnimationFrame(paint);
}
requestAnimationFrame(paint);
function loadEmulator(){
  if(!scriptPromise) scriptPromise=new Promise((resolve,reject)=>{
    const script=document.createElement('script');script.src='emulator/emulators.js';
    script.onload=resolve;
    script.onerror=()=>{script.remove();scriptPromise=null;reject(new Error('The emulator could not load. Check your connection and retry.'));};
    document.head.append(script);
  });
  return scriptPromise;
}
async function start(){
  if(loading) return;
  const ticket=++generation;
  loading=true;$('start').disabled=true;$('error').hidden=true;
  status('Booting Nightwatch…');releaseKeys();buttons();
  let instance;
  try{
    if(engine){const previous=engine;engine=null;await previous.exit();}
    paused=false;await loadEmulator();
    window.emulators.pathPrefix=new URL('emulator/',location.href).href;
    const response=await fetch('NIGHTWATCH.jsdos');
    if(!response.ok) throw new Error('The DOS bundle could not load. Please retry.');
    instance=await window.emulators.dosboxWorker(new Uint8Array(await response.arrayBuffer()));
    if(ticket!==generation){await instance.exit();return;}
    engine=instance;resize(engine.width(),engine.height());
    engine.events().onFrameSize((w,h)=>{if(engine===instance) resize(w,h);});
    engine.events().onFrame((rgb,rgba)=>{if(engine===instance) draw(rgb,rgba);});
    const finished=()=>{
      if(engine!==instance) return;
      releaseKeys();engine=null;paused=false;dirty=false;buttons();
      $('cover').hidden=false;status('Session ended · ready to run');
    };
    engine.events().onExit(finished);
    engine.events().onStdout(message=>{
      if(message.includes('NIGHTWATCH-BROWSER-SESSION-END')&&engine===instance){
        finished();instance.exit().catch(console.error);
      }
    });
    engine.mute();$('cover').hidden=true;status('Running · D: Work / E: Backup');canvas.focus();
  }catch(error){
    if(instance) await instance.exit().catch(()=>{});
    engine=null;dirty=false;$('cover').hidden=false;
    $('error').textContent=error.message||'Nightwatch could not start. Please retry.';
    $('error').hidden=false;status('Unable to start');
  }finally{loading=false;$('start').disabled=false;buttons();}
}
$('start').addEventListener('click',start);
$('restart').addEventListener('click',()=>{
  releaseKeys();
  if(window.confirm('Reset both demo drives? Changes made in this session will be lost.')) start();
});
$('pause').addEventListener('click',()=>{
  if(!engine) return;
  releaseKeys();paused=!paused;
  if(paused) engine.pause();else engine.resume();
  status(paused?'Paused':'Running · D: Work / E: Backup');buttons();canvas.focus();
});
$('fullscreen').addEventListener('click',async()=>{
  try{
    if(document.fullscreenElement) await document.exitFullscreen();
    else await $('player').requestFullscreen();
    canvas.focus();
  }catch(_){status('Fullscreen is unavailable in this browser');}
});
if(!$('player').requestFullscreen) $('fullscreen').hidden=true;
function syncModifiers(event,key){
  // Virtual keyboards can report Shift on a character without sending a
  // separate Shift key-up. Reconcile both transitions before the next key.
  const shifted=event.key.length===1&&'ABCDEFGHIJKLMNOPQRSTUVWXYZ~!@#$%^&*()_+{}|:"<>?'.includes(event.key);
  for(const [pressed,codes] of [[event.shiftKey||shifted,[340,344]],[event.ctrlKey,[341,345]],[event.altKey,[342,346]]]){
    if(pressed){
      if(!codes.some(code=>held.has(code))){
        const code=codes.includes(key)?key:codes[0];
        held.add(code);engine.sendKeyEvent(code,true);
      }
    }else for(const code of codes) if(held.delete(code)) engine.sendKeyEvent(code,false);
  }
}
canvas.addEventListener('keydown',event=>{
  const key=keys[event.code];
  if(!engine||loading||paused||event.metaKey||key===undefined) return;
  event.preventDefault();
  // Reconcile modifiers after focus changes; Ctrl-U/Shift-F3 must reach DOS.
  syncModifiers(event,key);
  if(!held.has(key)){held.add(key);engine.sendKeyEvent(key,true);}
});
canvas.addEventListener('keyup',event=>{
  const key=keys[event.code];
  if(engine&&!loading&&!paused&&!event.metaKey) syncModifiers(event,key);
  if(!held.has(key)) return;
  event.preventDefault();held.delete(key);engine?.sendKeyEvent(key,false);
});
canvas.addEventListener('blur',releaseKeys);window.addEventListener('blur',releaseKeys);
document.addEventListener('visibilitychange',()=>{if(document.hidden) releaseKeys();});
for(const button of document.querySelectorAll('[data-key],[data-combo]')) button.addEventListener('click',()=>{
  if(!engine||paused||loading) return;
  const chord=button.dataset.combo ? button.dataset.combo.split(',').map(Number) : [Number(button.dataset.key)];
  engine.simulateKeyPress(...modifiers,...chord);
  modifiers.clear();canvas.focus();
  for(const toggle of document.querySelectorAll('[data-modifier]')) toggle.setAttribute('aria-pressed','false');
});
for(const button of document.querySelectorAll('[data-modifier]')) button.addEventListener('click',()=>{
  const code=Number(button.dataset.modifier);
  if(modifiers.has(code)) modifiers.delete(code);else modifiers.add(code);
  button.setAttribute('aria-pressed',String(modifiers.has(code)));
});
window.addEventListener('pagehide',()=>{generation++;releaseKeys();engine?.exit();});
buttons();

// The static gallery remains useful without JavaScript; enhance its anchors.
const captures=[...document.querySelectorAll('.capture')], lightbox=$('lightbox');
let filtered=captures, expanded=false, selected=0;
function updateGallery(){
  captures.forEach(capture=>{capture.hidden=!filtered.includes(capture);});
  filtered.forEach((capture,index)=>{capture.hidden=!expanded&&index>=12;});
  $('gallery-count').textContent=filtered.length+' captures';
  $('show-more').hidden=expanded||filtered.length<=12;
  $('show-more').firstChild.textContent='Show all '+filtered.length+' captures ';
}
for(const button of document.querySelectorAll('[data-filter]')) button.addEventListener('click',()=>{
  for(const other of document.querySelectorAll('[data-filter]')) other.setAttribute('aria-pressed',String(other===button));
  filtered=captures.filter(c=>button.dataset.filter==='all'||c.dataset.category===button.dataset.filter);
  expanded=false;updateGallery();
});
$('show-more').addEventListener('click',()=>{expanded=true;updateGallery();});
function showCapture(index){
  selected=(index+filtered.length)%filtered.length;
  const capture=filtered[selected],image=capture.querySelector('img');
  $('lightbox-image').src=image.src;$('lightbox-image').alt=image.alt;
  $('lightbox-caption').textContent=capture.dataset.title;
  $('lightbox-counter').textContent=String(selected+1).padStart(2,'0')+' / '+filtered.length;
}
for(const capture of captures) capture.querySelector('a').addEventListener('click',event=>{
  if(!lightbox.showModal||event.ctrlKey||event.metaKey||event.shiftKey||event.altKey) return;
  event.preventDefault();showCapture(filtered.indexOf(capture));lightbox.showModal();
});
$('lightbox-close').addEventListener('click',()=>lightbox.close());
$('lightbox-prev').addEventListener('click',()=>showCapture(selected-1));
$('lightbox-next').addEventListener('click',()=>showCapture(selected+1));
lightbox.addEventListener('keydown',event=>{
  if(event.key==='ArrowLeft'){event.preventDefault();showCapture(selected-1);}
  if(event.key==='ArrowRight'){event.preventDefault();showCapture(selected+1);}
});
lightbox.addEventListener('click',event=>{if(event.target===lightbox) lightbox.close();});
updateGallery();
