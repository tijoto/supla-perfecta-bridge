#pragma once
static const char PAGE[] PROGMEM=R"HTML(<!doctype html><html lang="pl"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>PERFECTA ESP</title>
<style>*{box-sizing:border-box}body{margin:8px;background:#101824;color:#e5edf8;font:11px/1.25 Arial,sans-serif}header,.row{display:flex;gap:7px;align-items:center;flex-wrap:wrap;justify-content:space-between}h1{font-size:17px;margin:0}h2{font-size:12px;margin:0 0 4px}p{margin:4px 0}.cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(190px,1fr));gap:5px;margin:6px 0}.card{background:#1c2a3c;border:1px solid #40546e;border-radius:3px;padding:6px}.big{font-size:15px;font-weight:bold}button,input{font:inherit}button{background:#285b88;color:#fff;border:1px solid #6994b8;border-radius:3px;padding:6px 10px;cursor:pointer}button:disabled{opacity:.45;cursor:not-allowed}.alarmOn{background:#168449;border-color:#58d28d;font-weight:bold}.alarmOff{background:#b8343c;border-color:#f08085;font-weight:bold}.muted{color:#abbdd0}.good{color:#a9ecc2}.bad{color:#ffada6}.warn{color:#ffdf8e}table{width:100%;border-collapse:collapse;font-size:10px}th,td{text-align:left;padding:3px 5px;border-bottom:1px solid #36495f}th{color:#b2c8df}.scroll{overflow:auto}a{color:#95ccff}.controls{justify-content:flex-start}.check{display:flex;gap:6px;align-items:center}#message{min-height:17px}#danger{color:#ffdf8e}.mono{font-family:monospace}.tag{font-size:10px}</style>
<header><h1>PERFECTA <span class="good">ESP</span></h1><span id="connection">Wczytywanie...</span></header>
<div class="cards"><section class="card"><h2>Strefa 1</h2><div id="zone1" class="big">Nieodczytana</div><p id="alarm1"></p><p id="source1" class="muted"></p></section><section class="card"><h2>Strefa 2</h2><div id="zone2" class="big">Nieodczytana</div><p id="alarm2"></p><p id="source2" class="muted"></p></section><section class="card"><h2>Komunikacja</h2><div id="mode"></div><p id="clock" class="muted"></p><p id="error" class="warn"></p></section></div>
<section class="card"><div class="row controls"><button id="alarmToggle" class="alarmOff" disabled>Alarm: nieodczytany</button></div><p id="message" role="status"></p><p class="muted">GPIO25 &#8594; przeka&#378;nik 1 &#8594; Z7: za&#322;&#261;czenie; GPIO26 &#8594; przeka&#378;nik 2 &#8594; Z8: wy&#322;&#261;czenie. Impuls LOW przez 300 ms; spoczynek HIGH 3,3 V. Przeka&#378;niki aktywne LOW. Wynik polecenia sprawdzisz w rzeczywistym stanie stref.</p><p id="danger">Z7/Z8 pod&#322;&#261;cz przez transoptory lub styki przeka&#378;nik&#243;w do COM centrali. Nie &#322;&#261;cz wej&#347;&#263; alarmowych bezpo&#347;rednio z GPIO.</p></section>
<section class="card" style="margin-top:6px"><div class="row"><h2>Wej&#347;cia / czujki</h2><label><input type="checkbox" id="configured" checked> Tylko skonfigurowane</label></div><div class="scroll"><table><thead><tr><th>Nr</th><th>Nazwa</th><th>Stan</th><th>Bateria</th><th>RF</th><th>Ostatnie naruszenie</th><th>Ostatnia s&#322;aba bateria</th></tr></thead><tbody id="inputs"></tbody></table></div><p class="muted">Status WWW co 1 s; przechwytywanie narusze&#324; do 5 razy/s; RF i bateria co 5 s; nazwy co 30 s. RF oznacza ostatni odebrany sygna&#322;, nie potwierdza naruszenia czujki.</p></section>
<section class="card" style="margin-top:6px"><h2>Most dla PERFECTA Soft</h2><p id="bridge"></p><p class="muted">TCP RAW: <span id="endpoint"></span> &#183; UART 19200 / 8N1. Jeden klient. Podczas po&#322;&#261;czenia Softa odczyt niezale&#380;ny jest wstrzymany, a strona podgl&#261;da przechodz&#261;ce odpowiedzi.</p></section>
<section class="card" style="margin-top:6px"><h2>SUPLA</h2><p id="suplaStatus">Laczenie...</p><p id="suplaCommand" class="muted"></p><p><a href="/notifications">Powiadomienia: dzie&#324; / noc / harmonogram dla ka&#380;dej czujki i zdarzenia</a></p><p id="pushScheduleStatus" class="muted"></p><p class="muted">Odbiorcow ustaw w SUPLA Cloud: urzadzenie - Powiadomienia. PUSH dla naruszen czujek: maksymalnie raz na 30 s na czujke. Czuwanie jest odczytywane z centrali. Alarm ON/OFF wysyla tylko impuls, nie potwierdza uzbrojenia.</p><div class="row controls"><button id="pushTest" disabled>Test PUSH</button><span id="pushMessage" role="status"></span></div><p class="muted">Test PUSH pomija harmonogram.</p></section>
<section class="card" style="margin-top:6px"><h2>System</h2><p id="system" class="muted"></p><p><a href="/api/state" target="_blank">Odczyt JSON</a> &#183; <a href="/update">Aktualizacja firmware</a></p></section>
<script>
const $=id=>document.getElementById(id);let token=null,state=null,posting=false,online=false;
function text(id,v){$(id).textContent=v}
function time(v){return v?new Date(v*1000).toLocaleString('pl-PL',{timeZone:'Europe/Warsaw'}):'Brak zapisu'}
function buttons(){for(const id of ['alarmToggle'])$(id).disabled=!online||!state?.control.enabled||!state?.status_fresh||!state?.partitions.every(p=>[0,1,5].includes(p.state))||posting||state?.bridge.connected||state?.control.active_gpio!==0||state?.control.cooldown_ms>0}
function render(s){
 state=s;online=true;const fresh=s.status_fresh;const find=n=>s.inputs.find(i=>i.number===n);
 text('connection',fresh?'Centrala: online':'Brak aktualnego statusu centrali');$('connection').className=fresh?'good':'warn';
 text('mode',s.mode==='perfecta_soft'?'Podglad PERFECTA Soft':s.mode==='handoff'?'Przekazywanie portu do Softa':'Odczyt niezalezny UART');
 text('clock',s.panel_clock||'Czas centrali: nieodczytany');text('error',s.error||'');
 for(let z=1;z<=2;z++){
  const p=s.partitions[z-1];text('zone'+z,!fresh?'Nieodczytana / dane nieaktualne':p.state===0?'Rozbrojona':p.state===1?'Czas na wyjscie':p.state===5?'Czuwa':'Stan '+p.state);
  text('alarm'+z,!fresh?'Alarm: brak aktualnych danych':p.alarm===0?'Brak alarmu':p.alarm===68?'ALARM':'Nierozpoznany stan alarmu '+p.alarm);
  $('alarm'+z).className=fresh&&p.alarm===68?'bad':'';
  text('source'+z,p.source?'Ostatnie zrodlo: '+p.source+' '+(find(p.source)?.name||''):'');
 }
 const body=$('inputs');body.replaceChildren();
 for(const i of s.inputs){
  if($('configured').checked&&s.names_ready&&!i.configured)continue;
  const tr=document.createElement('tr');const values=[i.number,i.name||'Wejscie '+i.number,!fresh?'Nieodczytany':i.blocked?'Zablokowane':i.violated?'NARUSZENIE':'Brak naruszenia',!s.diagnostics_fresh?'Nieodczytana':i.low_battery?'SLABA':'Brak komunikatu',i.wireless&&s.rf_fresh?i.rf+'%':'—',time(i.last_violation),time(i.last_battery_low)];
  for(const v of values){const td=document.createElement('td');td.textContent=v;tr.appendChild(td)}
  if(fresh&&i.violated)tr.className='warn';body.appendChild(tr);
 }
 const alarmOn=s.partitions.some(p=>p.state===5);text('alarmToggle',!fresh?'Alarm: nieodczytany':alarmOn?'Alarm: ON':'Alarm: OFF');$('alarmToggle').className=alarmOn?'alarmOn':'alarmOff';$('alarmToggle').title=alarmOn?'Kliknij, aby wylaczyc czuwanie':'Kliknij, aby zalaczyc czuwanie';
 text('bridge',(s.bridge.connected?'Polaczony PERFECTA Soft':'Oczekiwanie na PERFECTA Soft')+' | PC: '+s.bridge.allowed_ip+' | TX '+s.bridge.tx_bytes+' B / RX '+s.bridge.rx_bytes+' B');
 text('endpoint',location.hostname+':2217');
 text('system',s.version+' | IP '+s.ip+' | Wi-Fi '+s.wifi_rssi+' dBm | RAM wolna '+Math.round(s.free_heap/1024)+' KB (minimum '+Math.round(s.min_heap/1024)+' KB) | Flash '+Math.round(s.flash_bytes/1048576)+' MB | program '+Math.round(s.sketch_bytes/1024)+' KB | sesje '+s.sessions+' | CRC '+s.crc_errors+' | timeout '+s.timeouts+' | autotest '+(s.self_test?'OK':'BLAD'));
 buttons();
}
async function poll(){
 try{const r=await fetch('/api/state',{cache:'no-store'});if(!r.ok)throw Error(r.status);render(await r.json())}
 catch(e){online=false;text('connection','Brak polaczenia ze strona ESP');buttons()}
 setTimeout(poll,1000);
}
async function action(which){
 if(posting||!token)return;posting=true;buttons();text('message','Wysylanie impulsu...');
 try{const r=await fetch('/api/control',{method:'POST',headers:{'X-Perfecta-Token':token,'Content-Type':'application/x-www-form-urlencoded'},body:'action='+which});const data=await r.json();if(!r.ok)throw Error(data.error||r.status);text('message','Wyslano impuls GPIO'+data.gpio+'. Sprawdz rzeczywisty stan stref.')}
 catch(e){text('message','Nie wyslano: '+e.message)}finally{posting=false;buttons()}
}
$('alarmToggle').onclick=()=>action(state?.partitions.some(p=>p.state===5||p.state===1)?'disarm':'arm');$('configured').onchange=()=>state&&render(state);

async function pollSupla(){
 try{
  const r=await fetch('/api/supla',{cache:'no-store'});if(!r.ok)throw Error(r.status);const s=await r.json();
  text('suplaStatus',(s.connected?'Polaczono':'Oczekiwanie na SUPLA (kod '+s.status+')')+' | '+s.server+' | Lokalizacja ustawiana w SUPLA Cloud | '+s.channels+' kanalow | TLS | PUSH przekazane: '+s.push_sent+' / pominiete: '+s.push_dropped);
  text('suplaCommand',s.last_command);$('pushTest').disabled=!s.connected||!token;
  const schedule=s.push_schedule;if(schedule)text('pushScheduleStatus',(schedule.time_valid?schedule.local_time+' | '+schedule.timezone:'Brak poprawnego czasu dla harmonogramow')+' | wlaczone: '+schedule.rules.filter(r=>r.enabled).length+'/20 | teraz aktywne: '+schedule.rules.filter(r=>r.active_now).length+' | wyciszone zdarzenia: '+s.push_suppressed);
 }catch(e){text('suplaStatus','Brak danych SUPLA');$('pushTest').disabled=true}
 setTimeout(pollSupla,3000);
}
async function suplaPost(path,body){
 if(!token)throw Error('Odswiez strone');
 const r=await fetch(path,{method:'POST',headers:{'X-Perfecta-Token':token,'Content-Type':'application/x-www-form-urlencoded'},body:body||''});
 const data=await r.json();if(!r.ok)throw Error(data.error||r.status);return data;
}
$('pushTest').onclick=async()=>{
 try{await suplaPost('/api/supla/test-push');text('pushMessage','Test w kolejce. Sprawdz telefon i zezwolenia PUSH w SUPLA.')}
 catch(e){text('pushMessage',e.message)}
};
pollSupla();

fetch('/api/session',{cache:'no-store'}).then(r=>r.json()).then(s=>{token=s.token}).catch(()=>text('message','Odswiez strone, aby pobrac sesje'));poll();
</script></html>)HTML";
