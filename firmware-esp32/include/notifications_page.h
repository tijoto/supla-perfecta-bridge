#pragma once
static const char NOTIFICATIONS_PAGE[] PROGMEM=R"HTML(<!doctype html><html lang="pl"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Powiadomienia — PERFECTA ESP</title>
<style>*{box-sizing:border-box}body{margin:8px;background:#101824;color:#e5edf8;font:11px/1.3 Arial,sans-serif}h1{font-size:16px;margin:0}header,.row{display:flex;align-items:center;gap:8px;flex-wrap:wrap;justify-content:space-between}a{color:#95ccff}.card{background:#1c2a3c;border:1px solid #40546e;border-radius:3px;padding:7px;margin-top:7px}p{margin:5px 0}.muted{color:#abbdd0}.good{color:#a9ecc2}.warn{color:#ffdf8e}button,input,select{font:inherit}button{padding:6px 10px;background:#285b88;color:white;border:1px solid #6994b8;border-radius:3px;cursor:pointer}button:disabled{opacity:.5;cursor:not-allowed}select,input[type=time]{background:#132032;color:#fff;border:1px solid #65809e;border-radius:2px;padding:3px}input[type=time]{width:87px}input[type=checkbox]{vertical-align:middle;margin:2px 3px 2px 0}.scroll{overflow:auto}table{border-collapse:collapse;width:100%;min-width:850px;font-size:11px}th,td{padding:4px 5px;text-align:left;border-bottom:1px solid #36495f}th{color:#b2c8df}.days{white-space:nowrap}.days label{display:inline-block;margin-right:4px}.inactive{opacity:.5}.event{min-width:235px}.state{min-width:108px}#pending{color:#ffdf8e}#message{min-height:16px}</style>
<header><h1>Powiadomienia PUSH</h1><a href="/">← Podgląd centrali</a></header>
<section class="card"><div class="row"><span id="clock">Odczyt czasu…</span><span id="connection"></span></div><p>Godziny lokalne: <strong>Europe/Warsaw</strong>, automatyczna zmiana czasu letniego/zimowego.</p><label>Dzień od <input id="dayStart" type="time" value="06:00" required></label> <label>do <input id="dayEnd" type="time" value="22:00" required></label><p class="muted">Noc obejmuje pozostałe godziny. Godzina końcowa nie należy do przedziału. Zmiana definicji dnia dotyczy wszystkich pozycji w trybie dzień/noc.</p></section>
<section class="card"><div class="scroll"><table><thead><tr><th>Zdarzenie / włączone</th><th>Kiedy wysyłać</th><th>Dni rozpoczęcia harmonogramu</th><th>Od</th><th>Do</th><th>Teraz</th></tr></thead><tbody id="rules"></tbody></table></div><p class="muted">Przedział przez północ, np. poniedziałek 22:00–06:00, kończy się we wtorek o 06:00. Jednakowe godziny oznaczają całą dobę wybranych dni. Harmonogram wycisza zdarzenia poza przedziałem — nie odkłada ich do późniejszego wysłania. Przy braku poprawnego czasu działają tylko pozycje „Zawsze”.</p><div class="row" style="justify-content:flex-start"><button id="save" disabled>Zapisz wszystkie ustawienia</button><button id="reload">Wczytaj zapisane</button><span id="pending"></span></div><p id="message" role="status"></p></section>
<section class="card"><p id="stats"></p><button id="test" disabled>Test PUSH</button><span class="muted"> Test pomija harmonogram i nie steruje alarmem. Odbiorców ustawiasz w SUPLA Cloud → urządzenie → Powiadomienia.</span></section>
<script>
const $=id=>document.getElementById(id),days=['Pn','Wt','Śr','Cz','Pt','So','Nd'];let token=null,data=null,dirty=false,loading=false,rows=[];
const hhmm=n=>String(Math.floor(n/60)).padStart(2,'0')+':'+String(n%60).padStart(2,'0');
function minutes(value){if(!/^\d{2}:\d{2}$/.test(value))throw Error('Uzupełnij godziny');const [h,m]=value.split(':').map(Number);if(h>23||m>59)throw Error('Nieprawidłowa godzina');return h*60+m}
function inRange(m,a,b){return a===b||(a<b?m>=a&&m<b:m>=a||m<b)}
function editing(){dirty=true;$('pending').textContent='Niezapisane zmiany';preview()}
function input(type,value,label){const e=document.createElement('input');e.type=type;e.setAttribute('aria-label',label);if(type==='checkbox')e.checked=!!value;else e.value=value;e.onchange=editing;return e}
function ruleValue(row){let mask=0;row.days.forEach((cb,i)=>{if(cb.checked)mask|=1<<i});return {enabled:row.enabled.checked,mode:Number(row.mode.value),days:mask,start:minutes(row.start.value),end:minutes(row.end.value)}}
function isActive(rule,clock,a,b){
 if(!rule.enabled)return false;if(rule.mode===0)return true;if(!clock.time_valid)return false;
 const day=inRange(clock.minute,a,b);if(rule.mode===1)return day;if(rule.mode===2)return !day;
 if(!inRange(clock.minute,rule.start,rule.end))return false;
 const weekday=rule.start>rule.end&&clock.minute<rule.end?(clock.weekday+6)%7:clock.weekday;return !!(rule.days&(1<<weekday));
}
function preview(){
 if(!data)return;
 let a,b;try{a=minutes($('dayStart').value);b=minutes($('dayEnd').value)}catch(e){return}
 rows.forEach(row=>{
  const weekly=Number(row.mode.value)===3;row.days.forEach(cb=>cb.disabled=!weekly);row.start.disabled=row.end.disabled=!weekly;row.dayCell.className='days'+(weekly?'':' inactive');
  let rule;try{rule=ruleValue(row)}catch(e){row.state.textContent='Sprawdź godziny';row.state.className='warn';return}
  const active=isActive(rule,data.push_schedule,a,b);
  row.state.textContent=!rule.enabled?'Wyłączone':rule.mode!==0&&!data.push_schedule.time_valid?'Brak czasu':active?'Wysyłanie aktywne':'Wyciszone';row.state.className='state '+(active?'good':'muted');
 });
}
function render(config){
 $('dayStart').value=hhmm(config.day_start);$('dayEnd').value=hhmm(config.day_end);$('rules').replaceChildren();rows=[];
 config.rules.forEach(rule=>{
  const tr=document.createElement('tr'),cells=Array.from({length:6},()=>document.createElement('td'));cells.forEach(td=>tr.appendChild(td));
  const row={enabled:input('checkbox',rule.enabled,rule.label),mode:document.createElement('select'),days:[],dayCell:cells[2],start:input('time',hhmm(rule.start),rule.label+' od'),end:input('time',hhmm(rule.end),rule.label+' do'),state:cells[5]};
  const label=document.createElement('label');label.append(row.enabled,document.createTextNode(rule.label));cells[0].className='event';cells[0].append(label);
  ['Zawsze','Dzień','Noc','Harmonogram'].forEach((name,i)=>{const option=document.createElement('option');option.value=i;option.textContent=name;row.mode.append(option)});row.mode.value=rule.mode;row.mode.onchange=editing;row.mode.setAttribute('aria-label',rule.label+' tryb');cells[1].append(row.mode);
  days.forEach((name,i)=>{const label=document.createElement('label'),cb=input('checkbox',rule.days&(1<<i),rule.label+' '+name);label.append(cb,document.createTextNode(name));cells[2].append(label);row.days.push(cb)});
  cells[3].append(row.start);cells[4].append(row.end);$('rules').append(tr);rows.push(row);
 });dirty=false;$('pending').textContent='';preview();
}
async function refresh(force=false){
 const r=await fetch('/api/supla',{cache:'no-store'});if(!r.ok)throw Error('Błąd odczytu '+r.status);const current=await r.json();if(!current.push_schedule)throw Error('Firmware nie obsługuje harmonogramów');data=current;
 const c=data.push_schedule;$('clock').textContent=c.time_valid?c.local_time+' | czas: '+c.time_source:'Brak poprawnego czasu — tryby dzień/noc/harmonogram są wyciszone';$('clock').className=c.time_valid?'':'warn';
 $('connection').textContent=data.connected?'SUPLA: połączono':'SUPLA: oczekiwanie na połączenie';$('stats').textContent='Przekazane do SUPLA: '+data.push_sent+' | odrzucone: '+data.push_dropped+' | wyciszone regułami: '+data.push_suppressed+' | '+(c.persisted?'ustawienia zapisane w ESP':'ustawienia przejęte z poprzedniej wersji');
 if(c.recovered)$('message').textContent='Poprzedni zapis harmonogramu był nieprawidłowy. Przywrócono ustawienia podstawowe.';
 if(force||rows.length===0||!dirty)render(c);else preview();$('test').disabled=!token||!data.connected;$('save').disabled=!token||loading;
}
async function post(path,body){if(!token)throw Error('Odśwież stronę, aby pobrać sesję');const r=await fetch(path,{method:'POST',headers:{'X-Perfecta-Token':token,'Content-Type':'application/x-www-form-urlencoded'},body});const result=await r.json();if(!r.ok)throw Error(result.error||r.status);return result}
function payload(){
 if(rows.length!==20)throw Error('Brak pełnej listy zdarzeń');const from=minutes($('dayStart').value),to=minutes($('dayEnd').value);if(from===to)throw Error('Początek i koniec dnia muszą być różne');
 const rules=rows.map(row=>{const r=ruleValue(row);if(r.enabled&&r.mode===3&&!r.days)throw Error('Wybierz przynajmniej jeden dzień harmonogramu');return [r.enabled?1:0,r.mode,r.days,r.start,r.end].join(',')}).join(';');
 return new URLSearchParams({day_start:from,day_end:to,rules}).toString();
}
for(const id of ['dayStart','dayEnd'])$(id).onchange=editing;
$('save').onclick=async()=>{loading=true;$('save').disabled=true;try{const body=payload();data=await post('/api/supla/push-schedule',body);render(data.push_schedule);$('message').textContent='Zapisano 20 reguł w pamięci ESP. Ustawienia przetrwają restart.'}catch(e){$('message').textContent=e.message}finally{loading=false;$('save').disabled=!token}};
$('reload').onclick=()=>refresh(true).catch(e=>$('message').textContent=e.message);
$('test').onclick=async()=>{try{await post('/api/supla/test-push','');$('message').textContent='Test w kolejce — pomija harmonogram. Sprawdź telefon.'}catch(e){$('message').textContent=e.message}};
async function poll(){try{await refresh()}catch(e){$('connection').textContent='Brak połączenia z ESP';$('message').textContent=e.message;$('test').disabled=true}setTimeout(poll,5000)}
fetch('/api/session',{cache:'no-store'}).then(r=>r.json()).then(s=>{token=s.token;$('save').disabled=!data;$('test').disabled=!data?.connected}).catch(()=>$('message').textContent='Odśwież stronę, aby pobrać sesję');poll();
</script></html>)HTML";
