// WiFi setup page, served while the board runs its own "Ripple-xxxxxx" network.
// Deliberately tiny and dependency free (no framework, no external requests):
// it has to load instantly in a phone's captive-portal browser.
#pragma once

#include <pgmspace.h>

static const char PORTAL_HTML[] PROGMEM = R"RIPPLE_PORTAL(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Ripple setup</title>
<style>
:root{--bg:#f6f8fa;--card:#fff;--fg:#14202b;--mut:#5d6b78;--line:#e2e8ee;--acc:#1f7a8c;--accfg:#fff;--bad:#c0392b;--ok:#1f8a5b}
@media(prefers-color-scheme:dark){:root{--bg:#0f151b;--card:#18212a;--fg:#e8eef3;--mut:#8b9aa7;--line:#26313c;--acc:#5fc4d6;--accfg:#08222a;--bad:#ff7b6b;--ok:#5fd6a0}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:16px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
main{max-width:30rem;margin:0 auto;padding:1.25rem 1rem 2rem}
header{display:flex;align-items:center;gap:.75rem;margin:.25rem 0 1.25rem}
header svg{width:2.25rem;height:2.25rem;color:var(--acc)}
h1{font-size:1.25rem;margin:0;line-height:1.2}
header p{margin:0;color:var(--mut);font-size:.85rem}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:1rem;margin-bottom:1rem}
h2{font-size:.8rem;text-transform:uppercase;letter-spacing:.06em;color:var(--mut);margin:0 0 .75rem}
ul{list-style:none;margin:0;padding:0}
li button{display:flex;align-items:center;gap:.75rem;width:100%;text-align:left;background:none;border:0;border-top:1px solid var(--line);padding:.8rem .25rem;color:inherit;font:inherit;cursor:pointer}
li:first-child button{border-top:0}
li button[aria-pressed=true]{color:var(--acc);font-weight:600}
li .name{flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.bars{display:inline-flex;align-items:flex-end;gap:2px;height:1rem}
.bars i{width:4px;background:var(--line);border-radius:1px}.bars i.on{background:var(--acc)}
.lock{font-size:.8rem;color:var(--mut)}
label{display:block;font-size:.85rem;color:var(--mut);margin:.75rem 0 .3rem}
input[type=text],input[type=password]{width:100%;font:inherit;padding:.7rem;border-radius:10px;border:1px solid var(--line);background:var(--bg);color:var(--fg)}
.show{display:flex;align-items:center;gap:.5rem;margin-top:.5rem;font-size:.9rem;color:var(--mut)}
button.primary{width:100%;margin-top:1rem;padding:.85rem;font:inherit;font-weight:600;border:0;border-radius:10px;background:var(--acc);color:var(--accfg);cursor:pointer}
button.primary:disabled{opacity:.5}
button.link{background:none;border:0;color:var(--acc);font:inherit;padding:.25rem 0;cursor:pointer}
.msg{margin:.75rem 0 0;font-size:.95rem}.err{color:var(--bad)}.okc{color:var(--ok)}
.mut{color:var(--mut);font-size:.9rem}
.big{font-size:1.1rem;font-weight:600;margin:.25rem 0}
a{color:var(--acc);word-break:break-all}
.spin{display:inline-block;width:1rem;height:1rem;border:2px solid var(--line);border-top-color:var(--acc);border-radius:50%;animation:s 1s linear infinite;vertical-align:-.15rem;margin-right:.4rem}
@keyframes s{to{transform:rotate(360deg)}}
[hidden]{display:none!important}
</style></head><body><main>
<header>
 <svg viewBox="0 0 32 32" fill="none" aria-hidden="true"><circle cx="16" cy="16" r="3.2" fill="currentColor"/><circle cx="16" cy="16" r="7.6" stroke="currentColor" stroke-width="1.8" opacity=".8"/><circle cx="16" cy="16" r="12.6" stroke="currentColor" stroke-width="1.8" opacity=".42"/></svg>
 <div><h1>Ripple setup</h1><p>Connect the board to your WiFi</p></div>
</header>

<div id="pick">
 <div class="card">
  <h2>Your WiFi network</h2>
  <div id="scanning" class="mut"><span class="spin"></span>Looking for networks…</div>
  <ul id="list"></ul>
  <p id="empty" class="mut" hidden>No networks found.</p>
  <button class="link" id="rescan" type="button">Search again</button>
  <button class="link" id="other" type="button">My network is not listed</button>
 </div>
 <form class="card" id="form" autocomplete="off">
  <div id="ssidBox" hidden>
   <label for="ssid">Network name</label>
   <input id="ssid" type="text" maxlength="32" autocapitalize="off" autocorrect="off" spellcheck="false">
  </div>
  <div id="chosen" class="big" hidden></div>
  <label for="pass">Password</label>
  <input id="pass" type="password" maxlength="64" autocapitalize="off" autocorrect="off" spellcheck="false" autocomplete="off">
  <label class="show"><input id="showpw" type="checkbox"> Show password</label>
  <p id="err" class="msg err" role="alert" hidden></p>
  <button class="primary" id="go" type="submit" disabled>Connect</button>
 </form>
</div>

<div id="working" class="card" hidden>
 <h2>Connecting</h2>
 <p class="big"><span class="spin"></span><span id="wname"></span></p>
 <p class="mut">This can take up to 20 seconds.</p>
</div>

<div id="done" class="card" hidden>
 <h2>All set</h2>
 <p class="big okc">Ripple is connected.</p>
 <p>Reconnect your phone to your home WiFi, then open the dashboard:</p>
 <p><a id="ip" href="#"></a></p>
 <p class="mut">or <a id="host" href="#"></a> if your device supports it. This setup network closes in a few seconds.</p>
</div>
</main>
<script>
const $=id=>document.getElementById(id);
let chosen=null,manual=false,pollT=null,scanTries=0;
const bars=r=>{const n=r>-55?4:r>-65?3:r>-75?2:1;return '<span class="bars">'+[1,2,3,4].map(i=>'<i class="'+(i<=n?'on':'')+'" style="height:'+i*25+'%"></i>').join('')+'</span>'};
const esc=s=>s.replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
function update(){
 const ssid=manual?$('ssid').value.trim():(chosen&&chosen.ssid);
 $('go').disabled=!ssid;
 $('chosen').hidden=manual||!chosen; if(chosen&&!manual)$('chosen').textContent=chosen.ssid;
 $('ssidBox').hidden=!manual;
}
function render(nets){
 $('list').innerHTML=nets.map((n,i)=>'<li><button type="button" data-i="'+i+'" aria-pressed="'+(chosen&&chosen.ssid===n.ssid)+'"><span class="name">'+esc(n.ssid)+'</span>'+(n.secure?'<span class="lock">&#128274;</span>':'')+bars(n.rssi)+'</button></li>').join('');
 $('empty').hidden=nets.length>0;
 $('list').onclick=e=>{const b=e.target.closest('button');if(!b)return;chosen=nets[+b.dataset.i];manual=false;render(nets);update();$('pass').focus()};
}
async function scan(rescan){
 $('scanning').hidden=false;$('list').innerHTML='';$('empty').hidden=true;
 try{
  const r=await fetch('/api/wifi/scan'+(rescan?'?rescan=1':''),{cache:'no-store'});const j=await r.json();
  if(j.scanning&&scanTries++<15){setTimeout(()=>scan(false),1200);return}
  scanTries=0;$('scanning').hidden=true;render(j.networks||[]);
 }catch(e){if(scanTries++<15)setTimeout(()=>scan(false),1500);else{$('scanning').hidden=true;$('empty').hidden=false}}
}
$('rescan').onclick=()=>{scanTries=0;scan(true)};
$('other').onclick=()=>{manual=true;chosen=null;$('list').querySelectorAll('button').forEach(b=>b.setAttribute('aria-pressed','false'));update();$('ssid').focus()};
$('ssid').oninput=update;
$('showpw').onchange=e=>{$('pass').type=e.target.checked?'text':'password'};
$('form').onsubmit=async e=>{
 e.preventDefault();$('err').hidden=true;
 const ssid=manual?$('ssid').value.trim():chosen.ssid,pass=$('pass').value;
 $('go').disabled=true;
 try{
  const r=await fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:ssid,pass:pass})});
  const j=await r.json();
  if(!r.ok){$('err').textContent=j.error||'Could not start';$('err').hidden=false;$('go').disabled=false;return}
  $('pick').hidden=true;$('working').hidden=false;$('wname').textContent=ssid;poll();
 }catch(x){$('err').textContent='Lost contact with the board. Try again.';$('err').hidden=false;$('go').disabled=false}
};
function poll(){
 clearTimeout(pollT);
 fetch('/api/wifi/status',{cache:'no-store'}).then(r=>r.json()).then(s=>{
  if(s.state==='connected'){
   $('working').hidden=true;$('done').hidden=false;
   const u='http://'+s.ip+'/',h='http://'+s.host+'.local/';
   $('ip').href=u;$('ip').textContent=u;$('host').href=h;$('host').textContent=h;return;
  }
  if(s.state==='failed'){
   $('working').hidden=true;$('pick').hidden=false;$('err').textContent=s.error||'Could not connect.';$('err').hidden=false;$('go').disabled=false;return;
  }
  pollT=setTimeout(poll,1000);
 }).catch(()=>{pollT=setTimeout(poll,1500)});
}
scan(false);
</script></body></html>)RIPPLE_PORTAL";
