// Embedded live / calibration page. Plain HTML + JS on purpose: it is the
// functional baseline that talks to the WebSocket API; the full dashboard
// (Vite + React + shadcn/ui) will replace it and use the same API.
#pragma once

#include <pgmspace.h>

static const char INDEX_HTML[] PROGMEM = R"RIPPLE_PAGE(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Ripple</title>
<style>
:root{--bg:#f6f7f9;--card:#fff;--fg:#14171c;--mut:#667085;--line:#e4e7ec;--acc:#2563eb;--ok:#16a34a;--bad:#dc2626;--warn:#d97706}
@media(prefers-color-scheme:dark){:root{--bg:#0e1116;--card:#171b22;--fg:#e8eaee;--mut:#8b93a1;--line:#272d38;--acc:#60a5fa;--ok:#4ade80;--bad:#f87171;--warn:#fbbf24}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.45 system-ui,-apple-system,Segoe UI,sans-serif}
main{max-width:760px;margin:0 auto;padding:16px}
h1{font-size:20px;margin:4px 0 14px;display:flex;align-items:center;gap:10px}
h2{font-size:13px;text-transform:uppercase;letter-spacing:.06em;color:var(--mut);margin:0 0 10px;font-weight:600}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:16px;margin-bottom:14px}
.dot{width:10px;height:10px;border-radius:50%;background:var(--mut);display:inline-block}
.dot.on{background:var(--ok)}
.status{display:flex;gap:10px;flex-wrap:wrap}
.pill{flex:1;min-width:140px;border:1px solid var(--line);border-radius:10px;padding:12px}
.pill b{display:block;font-size:22px;margin-top:2px}
.pill.alert{border-color:var(--bad)}.pill.alert b{color:var(--bad)}
.pill.good b{color:var(--ok)}
canvas{width:100%;height:190px;display:block}
.legend{display:flex;gap:14px;color:var(--mut);font-size:12px;margin-top:6px}
.legend i{display:inline-block;width:10px;height:3px;margin-right:5px;vertical-align:middle}
button{font:inherit;border:1px solid var(--line);background:var(--card);color:var(--fg);border-radius:8px;padding:9px 14px;cursor:pointer}
button.primary{background:var(--acc);border-color:var(--acc);color:#fff}
button:disabled{opacity:.5;cursor:default}
.row{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
.bar{height:8px;background:var(--line);border-radius:4px;overflow:hidden;margin:10px 0}
.bar>div{height:100%;background:var(--acc);width:0;transition:width .3s}
.big{font-size:18px;font-weight:600;margin:2px 0 4px}
.mut{color:var(--mut)}
table{border-collapse:collapse;width:100%;margin-top:8px}td{padding:5px 0;border-top:1px solid var(--line)}td:last-child{text-align:right;font-variant-numeric:tabular-nums}
.q-good{color:var(--ok)}.q-fair{color:var(--warn)}.q-poor{color:var(--bad)}
input[type=number]{font:inherit;width:90px;padding:8px;border-radius:8px;border:1px solid var(--line);background:var(--card);color:var(--fg)}
</style></head><body><main>
<h1><span class="dot" id="link"></span>Ripple <span class="mut" id="fw" style="font-size:13px;font-weight:400"></span></h1>

<div class="card"><div class="status">
 <div class="pill" id="pMotion"><span class="mut">Motion</span><b id="vMotion">-</b></div>
 <div class="pill" id="pPresence"><span class="mut">Presence</span><b id="vPresence">-</b></div>
 <div class="pill"><span class="mut">RSSI</span><b id="vRssi">-</b></div>
</div></div>

<div class="card"><h2>Signal fluctuation (last 60 s)</h2>
 <canvas id="chart"></canvas>
 <div class="legend"><span><i style="background:var(--acc)"></i>variance</span><span><i style="background:var(--bad)"></i>threshold</span></div>
</div>

<div class="card"><h2>Calibration</h2>
 <div id="wizBody"></div>
</div>

<div class="card"><h2>Threshold</h2>
 <div class="row">
  <input type="number" id="thrIn" step="0.1" min="0.1">
  <button id="thrSet">Set</button>
  <span class="mut" id="thrInfo"></span>
 </div>
 <p class="mut" style="margin:10px 0 0;font-size:13px">Motion starts when the variance goes above the threshold. Lower is more sensitive, higher is more robust against false alarms.</p>
</div>

<div class="card"><h2>Active calibration</h2><div id="calInfo" class="mut">-</div></div>
</main>
<script>
const $=id=>document.getElementById(id);
let ws,hist=[],last=null,thrFocus=false;
const N=300;
function send(o){if(ws&&ws.readyState===1)ws.send(JSON.stringify(o))}
function connect(){
 ws=new WebSocket('ws://'+location.host+'/ws');
 ws.onopen=()=>$('link').classList.add('on');
 ws.onclose=()=>{$('link').classList.remove('on');setTimeout(connect,1500)};
 ws.onmessage=e=>{try{update(JSON.parse(e.data))}catch(_){}};
}
const STEPS=['Leave the room','Walk around the room','Sit still in the room','Leave the room again'];
const HINT=['Go out and close the door. The board will learn what an empty room looks like.',
 'Walk back and forth through the room, between the board and the router.',
 'Sit down and stay as still as you can.',
 'Leave again. This checks that the empty-room noise is stable.'];
function pct(x){return Math.round(x*100)+'%'}
function wiz(s){
 const w=s.wiz,b=$('wizBody');let h='';
 const step=w.stage;
 if(w.state==='waiting'){
  h+=`<div class="mut">Step ${step} of 4</div><div class="big">${STEPS[step-1]}</div><p class="mut">${HINT[step-1]}</p>
  <div class="row"><button class="primary" onclick="send({cmd:'wizard_ready'})">I'm in position, start</button><button onclick="send({cmd:'wizard_cancel'})">Cancel</button></div>`;
 }else if(w.state==='countdown'){
  h+=`<div class="mut">Step ${step} of 4</div><div class="big">${STEPS[step-1]}</div><p>Get in position... recording starts in <b>${w.left}</b> s</p>
  <div class="row"><button onclick="send({cmd:'wizard_cancel'})">Cancel</button></div>`;
 }else if(w.state==='recording'){
  const pc=Math.max(0,Math.min(100,100-w.left/30*100));
  h+=`<div class="mut">Step ${step} of 4</div><div class="big">Recording: ${STEPS[step-1].toLowerCase()}</div>
  <div class="bar"><div style="width:${pc}%"></div></div><p class="mut">${w.left} s left. ${HINT[step-1]}</p>
  <div class="row"><button onclick="send({cmd:'wizard_cancel'})">Cancel</button></div>`;
 }else{
  if(w.state==='failed')h+=`<p class="q-poor">The last run failed: not enough samples were received (is the WiFi link stable?).</p>`;
  if(s.phase==='calibrating')h+=`<div class="big">Quick calibration...</div><div class="bar"><div style="width:${pct(s.cal)}"></div></div><p class="mut">Keep the room empty.</p>`;
  else h+=`<p class="mut" style="margin-top:0">The guided calibration learns your room in four short steps and sets the threshold from your own data. Run it once the board is in its final position.</p>
  <div class="row"><button class="primary" onclick="send({cmd:'wizard_start'})">Start guided calibration</button>
  <button onclick="if(confirm('Quick calibration needs the room to be EMPTY for about 45 s. Continue?'))send({cmd:'quick_calibrate'})">Quick calibration</button></div>`;
 }
 if(b.dataset.k!==h){b.innerHTML=h;b.dataset.k=h}
}
function calInfo(s){
 let h='';const r=s.res;
 if(s.phase==='uncalibrated'){h='Not calibrated yet.'}
 else{
  h=`<div>Source: <b>${s.source||'-'}</b>${s.saved?' (saved)':' (not saved)'}</div>`;
  if(r){
   h+=`<div class="big q-${r.quality}">Quality: ${r.quality}</div><table>
   <tr><td>Empty-room noise (mean / max)</td><td>${r.mean.toFixed(2)} / ${r.ceil.toFixed(2)}</td></tr>
   <tr><td>Threshold</td><td>${s.thr.toFixed(2)}</td></tr>
   <tr><td>Walking seen</td><td>${pct(r.mov)} of seconds</td></tr>
   <tr><td>Sitting still seen</td><td>${pct(r.still)} of seconds</td></tr>
   <tr><td>False seconds in the 2nd empty step</td><td>${r.hold}</td></tr></table>`;
   if(r.quality==='poor')h+=`<p class="q-poor">Walking cannot be told apart from an empty room at this position. Try moving the board a few metres away from the router (so the signal is weaker) and run the calibration again.</p>`;
   else if(r.quality==='fair')h+=`<p class="q-fair">Detection works but misses part of the movement. A different position may improve it.</p>`;
   if(!r.stillOk)h+=`<p class="mut">A person sitting still is not detectable with this signal. Presence stays on for a while after the last movement.</p>`;
  }else{h+=`<table><tr><td>Empty-room noise (mean / max)</td><td>${s.mean.toFixed(2)} / ${s.max.toFixed(2)}</td></tr><tr><td>Threshold</td><td>${s.thr.toFixed(2)}</td></tr></table>`}
 }
 if($('calInfo').dataset.k!==h){$('calInfo').innerHTML=h;$('calInfo').dataset.k=h}
}
function update(s){
 last=s;
 $('fw').textContent='v'+s.fw;
 $('vRssi').textContent=s.ok?s.rssi+' dBm':'no link';
 const m=s.phase==='running'&&s.motion,p=s.phase==='running'&&s.presence;
 $('vMotion').textContent=s.phase!=='running'?'-':(m?'Detected':'None');
 $('vPresence').textContent=s.phase!=='running'?'-':(p?'Present':'Empty');
 $('pMotion').className='pill'+(m?' alert':'');
 $('pPresence').className='pill'+(p?' alert':(s.phase==='running'?' good':''));
 if(!thrFocus){$('thrIn').value=s.thr>0?s.thr.toFixed(2):''}
 $('thrInfo').textContent=s.phase==='running'?'current '+s.thr.toFixed(2):'';
 hist.push({v:s.peak,t:s.phase==='running'?s.thr:0,m:m});if(hist.length>N)hist.shift();
 wiz(s);calInfo(s);draw();
}
function draw(){
 const c=$('chart'),dpr=window.devicePixelRatio||1,w=c.clientWidth,h=c.clientHeight;
 if(c.width!==w*dpr){c.width=w*dpr;c.height=h*dpr}
 const g=c.getContext('2d');g.setTransform(dpr,0,0,dpr,0,0);g.clearRect(0,0,w,h);
 const st=getComputedStyle(document.documentElement);
 const acc=st.getPropertyValue('--acc'),bad=st.getPropertyValue('--bad'),line=st.getPropertyValue('--line'),mut=st.getPropertyValue('--mut');
 let max=2;for(const p of hist){max=Math.max(max,p.v,p.t)}max*=1.15;
 const X=i=>i/(N-1)*w+(N-hist.length)*0, Y=v=>h-4-(v/max)*(h-12);
 g.strokeStyle=line;g.lineWidth=1;g.fillStyle=mut;g.font='11px system-ui';
 for(let k=0;k<=3;k++){const v=max*k/3,y=Y(v);g.beginPath();g.moveTo(0,y);g.lineTo(w,y);g.stroke();g.fillText(v.toFixed(1),2,y-3)}
 const off=N-hist.length;
 g.fillStyle=bad+'22';
 hist.forEach((p,i)=>{if(p.m)g.fillRect((i+off)/(N-1)*w,0,w/(N-1)+1,h)});
 g.strokeStyle=acc;g.lineWidth=2;g.beginPath();
 hist.forEach((p,i)=>{const x=(i+off)/(N-1)*w,y=Y(p.v);i?g.lineTo(x,y):g.moveTo(x,y)});g.stroke();
 g.strokeStyle=bad;g.lineWidth=1.5;g.setLineDash([6,4]);g.beginPath();let st2=false;
 hist.forEach((p,i)=>{if(!p.t){st2=false;return}const x=(i+off)/(N-1)*w,y=Y(p.t);st2?g.lineTo(x,y):g.moveTo(x,y);st2=true});g.stroke();g.setLineDash([]);
}
$('thrIn').addEventListener('focus',()=>thrFocus=true);
$('thrIn').addEventListener('blur',()=>thrFocus=false);
$('thrSet').onclick=()=>{const v=parseFloat($('thrIn').value);if(v>0)send({cmd:'set_threshold',value:v});thrFocus=false;$('thrIn').blur()};
window.addEventListener('resize',draw);
connect();
</script></body></html>)RIPPLE_PAGE";
