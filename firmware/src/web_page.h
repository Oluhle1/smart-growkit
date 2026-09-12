#pragma once
#include <Arduino.h>

static const char PAGE_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>GrowKit-PoC</title>
<style>
body{font-family:sans-serif;margin:12px;background:#111;color:#eee}
h1{font-size:1.15rem;margin:0 0 8px}
.muted{color:#aaa;font-size:.9rem}
.warn{color:#f6a;min-height:1.2em}
.row{display:flex;gap:8px;flex-wrap:wrap;margin:10px 0;align-items:center}
button{font-size:1rem;padding:10px 14px;border:0;border-radius:8px;background:#345;color:#fff}
button.active{background:#2a8}
button.on{background:#2a8}
button.off{background:#444}
button:disabled{opacity:.45}
input{font-size:1rem;padding:8px;border-radius:8px;border:0}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.card{background:#1b1b1b;padding:10px;border-radius:8px}
.k{color:#aaa;font-size:.8rem}
</style>
</head>
<body>
<h1>GrowKit PoC</h1>
<p id="meta" class="muted">connecting…</p>
<p id="clockline" class="muted">clock …</p>
<p id="doseline" class="muted">dose …</p>
<p id="lock" class="warn"></p>
<p id="clockwarn" class="warn"></p>
<div class="row">
<button onclick="setPhoneClock()">Set clock from phone</button>
<input id="iso" type="datetime-local">
<button onclick="setTypedClock()">Set this time</button>
</div>
<div class="row" id="modes"></div>
<p class="muted" id="hint"></p>
<div class="row" id="acts"></div>
<div class="row">
<label class="muted">pulse ms
<input id="dosems" type="number" min="1" max="2000" value="500" style="width:5em">
</label>
<button id="doseA" onclick="dose('A')">Dose A</button>
<button id="doseB" onclick="dose('B')">Dose B</button>
<button disabled>Dose C off</button>
</div>
<div id="simbox" style="display:none">
<p class="muted">bench sim (RAM only; fills NAN sensors). Heater and auto-dose stay off.</p>
<div class="row">
<label class="muted">level<input id="sim_level_pct" type="number" value="80" style="width:4.5em"></label>
<label class="muted">pH<input id="sim_ph" type="number" step="0.1" style="width:4.5em"></label>
<label class="muted">air°C<input id="sim_air_c" type="number" step="0.1" style="width:4.5em"></label>
<label class="muted">water°C<input id="sim_water_c" type="number" step="0.1" style="width:4.5em"></label>
<button onclick="postSim()">POST /sim</button>
</div>
</div>
<div class="grid" id="vals"></div>
<script>
function n(v){return (v===null||v===undefined)?'—':v;}
function pad(n){return String(n).padStart(2,'0');}
function localIso(d){
  return d.getFullYear()+'-'+pad(d.getMonth()+1)+'-'+pad(d.getDate())+'T'+
    pad(d.getHours())+':'+pad(d.getMinutes())+':'+pad(d.getSeconds());
}
function post(u){fetch(u,{method:'POST'}).then(load).catch(load);}
function setPhoneClock(){post('/clock?iso='+encodeURIComponent(localIso(new Date())));}
function setTypedClock(){
  const v=document.getElementById('iso').value;
  if(!v){return;}
  post('/clock?iso='+encodeURIComponent(v));
}
function dose(ch){
  let ms=parseInt(document.getElementById('dosems').value,10);
  if(!ms) ms=500;
  if(ms>2000) ms=2000;
  post('/dose?ch='+ch+'&ms='+ms);
}
function postSim(){
  const q=[];
  [['sim_level_pct','level_pct'],['sim_ph','ph'],['sim_air_c','air_c'],['sim_water_c','water_c']].forEach(p=>{
    const v=document.getElementById(p[0]).value;
    if(v!=='') q.push(p[1]+'='+encodeURIComponent(v));
  });
  post('/sim'+(q.length?'?'+q.join('&'):''));
}
function load(){
  fetch('/status').then(r=>r.json()).then(s=>{
    document.getElementById('meta').textContent=
      s.device_id+'  fw '+s.fw+'  '+s.ip+'  up '+s.uptime_s+'s'+
      (s.bench_sim?'  BENCH_SIM':'');
    document.getElementById('simbox').style.display=s.bench_sim?'block':'none';
    document.getElementById('clockline').textContent=
      'clock_ok='+s.clock_ok+'  '+s.clock;
    document.getElementById('doseline').textContent=
      'dose='+s.dose+'  last='+(s.dose_last||'—')+'  pulse_ms='+s.dose_ms+
      '  settle_s='+s.dose_settle_s;
    document.getElementById('lock').textContent=
      s.water_lock?'LOW WATER: pump and dosing locked off':'';
    document.getElementById('clockwarn').textContent=
      (!s.clock_ok && s.mode==='auto')?
        'AUTO light 16/8 waiting for clock. Pump 15/45 still runs.':'';
    document.getElementById('hint').textContent=
      s.mode==='manual'?
        'Toggles apply now. Dose A/B are one pulse, then 15 min settle. No auto pH.':
        'Switch to manual to toggle pump / light / fan or pulse dose A/B.';
    document.getElementById('modes').innerHTML=['safe','auto','manual'].map(m=>
      '<button class="'+(s.mode===m?'active':'')+'" onclick="post(\'/mode?m='+m+'\')">'+m+'</button>'
    ).join('');
    const man=s.mode==='manual';
    function tog(ch,on){
      return '<button class="'+(on?'on':'off')+'" '+(man?'':'disabled ')+
        'onclick="post(\'/act?ch='+ch+'&on='+(on?0:1)+'\')">'+ch+' '+(on?'ON':'OFF')+'</button>';
    }
    document.getElementById('acts').innerHTML=
      tog('pump',s.pump)+tog('light',s.light)+tog('fan',s.fan)+
      '<button class="off" disabled>heat OFF</button>';
    const canDose=man && !s.water_lock && s.dose==='off' && s.dose_settle_s===0;
    document.getElementById('doseA').disabled=!canDose;
    document.getElementById('doseB').disabled=!canDose;
    function cell(k,v){return '<div class="card"><div class="k">'+k+'</div><div>'+n(v)+'</div></div>';}
    document.getElementById('vals').innerHTML=
      cell('clock_ok',s.clock_ok)+cell('clock',s.clock)+
      cell('dose',s.dose)+cell('settle_s',s.dose_settle_s)+
      cell('air °C',s.air_c)+cell('RH %',s.rh)+cell('water °C',s.water_c)+
      cell('pH',s.ph)+cell('level %',s.level_pct)+cell('heater_en',s.heater_en)+
      cell('bench_sim',s.bench_sim);
  }).catch(()=>{document.getElementById('meta').textContent='waiting for kit…';});
}
setInterval(load,2000);
load();
</script>
</body>
</html>
)HTML";
