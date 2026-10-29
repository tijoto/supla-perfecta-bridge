#include "app.h"
#include "private_config.h"
#include "page.h"
#include "supla_bridge.h"
#include "push_schedule_runtime.h"
#include "notifications_page.h"
#include <Update.h>
static bool updateAuthorized=false,updating=false;
static String updateError;
static bool authenticate(){
 if(web.authenticate(ADMIN_USER,ADMIN_PASSWORD))return true;
 web.requestAuthentication(DIGEST_AUTH,"PERFECTA ESP","Wymagane logowanie");return false;
}
static void json(int code,const String& value){
 web.sendHeader("Cache-Control","no-store");web.sendHeader("X-Content-Type-Options","nosniff");
 web.send(code,"application/json; charset=utf-8",value);
}
static bool mutationAllowed(){
 if(!authenticate())return false;
 if(web.header("X-Perfecta-Token")!=csrfToken){json(403,"{\"error\":\"Odswiez strone - nieprawidlowa sesja\"}");return false;}
 String origin=web.header("Origin");
 if(origin.length()&&origin!="http://"+web.hostHeader()){json(403,"{\"error\":\"Nieprawidlowe pochodzenie zadania\"}");return false;}
 return true;
}
String stateJson(){
 uint32_t now=millis();bool fresh=app.hasStatus&&uint32_t(now-app.statusAt)<4000;
 bool diagFresh=app.hasDiag&&uint32_t(now-app.diagAt)<15000;
 bool rfFresh=app.hasRf&&uint32_t(now-app.rfAt)<15000;
 uint32_t violations=Perfecta::le32(app.status+34),blocked=Perfecta::le32(app.status+10),low=Perfecta::le32(app.diag+58);
 String s;s.reserve(9500);s="{\"version\":"+jquote(VERSION)+",\"ip\":"+jquote(WiFi.localIP().toString())+",\"wifi_rssi\":"+String(WiFi.RSSI());
 s+=",\"self_test\":"+String(app.selfTest?"true":"false")+",\"free_heap\":"+String(ESP.getFreeHeap())+",\"min_heap\":"+String(ESP.getMinFreeHeap());
 s+=",\"flash_bytes\":"+String(ESP.getFlashChipSize())+",\"sketch_bytes\":"+String(ESP.getSketchSize())+",\"mode\":"+jquote(app.mode);
 s+=",\"status_fresh\":"+String(fresh?"true":"false")+",\"diagnostics_fresh\":"+String(diagFresh?"true":"false")+",\"rf_fresh\":"+String(rfFresh?"true":"false");
 s+=",\"names_ready\":"+String(app.namesReady?"true":"false")+",\"uart_ready\":"+String(app.ready?"true":"false")+",\"error\":"+jquote(app.error);
 s+=",\"sessions\":"+String(app.sessionCount)+",\"samples\":"+String(app.samples)+",\"crc_errors\":"+String(app.crcErrors)+",\"timeouts\":"+String(app.timeouts);
 s+=",\"tx_bytes\":"+String(app.txBytes)+",\"rx_bytes\":"+String(app.rxBytes)+",\"uptime_ms\":"+String(now);
 s+=",\"sketch_md5\":"+jquote(ESP.getSketchMD5());
 s+=",\"panel_clock\":";if(app.hasStatus){char clock[24];snprintf(clock,sizeof(clock),"20%02x-%02x-%02x %02x:%02x:%02x",app.status[1],app.status[2],app.status[3],app.status[4],app.status[5],app.status[6]);s+=jquote(clock);}else s+="null";
 s+=",\"partitions\":[";
 for(int z=0;z<2;z++){if(z)s+=',';s+="{\"number\":"+String(z+1)+",\"name\":"+jquote(panelName(480+z*16))+",\"state\":"+String(app.status[52+z])+",\"alarm\":"+String(app.status[54+z])+",\"source\":"+String(app.status[56+z])+"}";}
 s+="],\"inputs\":[";
 for(int i=0;i<16;i++){
  if(i)s+=',';
  bool wireless=false;for(int k=0;k<4;k++)wireless|=app.config[0x1500+i*4+k]!=0;
  bool configured=wireless||app.config[0x1400+i]!=0;
  s+="{\"number\":"+String(i+1)+",\"name\":"+jquote(panelName(1024+i*16))+",\"configured\":"+String(configured?"true":"false");
  s+=",\"wireless\":"+String(wireless?"true":"false")+",\"violated\":"+String(violations&(1<<i)?"true":"false")+",\"blocked\":"+String(blocked&(1<<i)?"true":"false");
  s+=",\"low_battery\":"+String(low&(1<<i)?"true":"false")+",\"rf\":"+String(app.rf[5+i]);
  s+=",\"last_violation\":"+String(app.history.violation[i])+",\"last_battery_low\":"+String(app.history.battery[i])+inputPartitionJson(i+1)+"}";
 }
 s+="],\"bridge\":{\"connected\":"+String(app.bridge?"true":"false")+",\"port\":"+String(BRIDGE_PORT)+",\"allowed_ip\":"+jquote(BRIDGE_IP);
 s+=",\"tx_bytes\":"+String(app.bridgeTx)+",\"rx_bytes\":"+String(app.bridgeRx)+",\"rejected_clients\":"+String(app.rejectedClients)+"}";
 uint32_t elapsed=now-pulseEndedAt,cooldown=(pulseCount&&(activeOutput||elapsed<2000))?(activeOutput?2000:2000-elapsed):0;
 s+=",\"control\":{\"enabled\":"+String(CONTROL_ENABLED?"true":"false")+",\"active_gpio\":"+String(activeOutput)+",\"arm_gpio\":"+String(ARM_PIN)+",\"disarm_gpio\":"+String(DISARM_PIN)+",\"pulse_ms\":"+String(PULSE_MS)+",\"pulses\":"+String(pulseCount);
 s+=",\"active_level\":\"LOW\",\"idle_level\":\"HIGH\",\"drive\":\"push-pull\"";
 s+=",\"cooldown_ms\":"+String(cooldown)+",\"gpio25_level\":"+String(gpio_get_level(gpio_num_t(ARM_PIN)))+",\"gpio26_level\":"+String(gpio_get_level(gpio_num_t(DISARM_PIN)))+"},\"input7_arming\":"+z7Json()+"}";return s;
}
void setupWeb(){
 const char* headers[]={"X-Perfecta-Token","Origin"};web.collectHeaders(headers,2);
 web.on("/",HTTP_GET,[]{if(!authenticate())return;web.sendHeader("Cache-Control","no-store");web.send_P(200,"text/html; charset=utf-8",PAGE);});
 web.on("/notifications",HTTP_GET,[]{if(!authenticate())return;web.sendHeader("Cache-Control","no-store");web.send_P(200,"text/html; charset=utf-8",NOTIFICATIONS_PAGE);});
 web.on("/api/session",HTTP_GET,[]{if(authenticate())json(200,"{\"token\":"+jquote(csrfToken)+"}");});
 web.on("/api/state",HTTP_GET,[]{if(authenticate())json(200,stateJson());});
 web.on("/api/inputs/partitions",HTTP_POST,[]{
  if(!mutationAllowed())return;
  String number=web.arg("number"),p1=web.arg("partition1"),p2=web.arg("partition2"),revision=web.arg("revision");
  uint64_t parsed=0;bool valid=revision.length()>0&&revision.length()<=10;
  for(unsigned i=0;i<revision.length();i++){char c=revision[i];if(c<'0'||c>'9'){valid=false;break;}parsed=parsed*10+(c-'0');}
  bool numberValid=number.length()>0&&number.length()<=2;
  for(unsigned i=0;i<number.length();i++)if(number[i]<'0'||number[i]>'9')numberValid=false;
  int n=number.toInt();
  if(!numberValid||n<1||n>16||(p1!="0"&&p1!="1")||(p2!="0"&&p2!="1")||!valid||parsed>UINT32_MAX||web.hasArg("blocked")){
   json(400,"{\"error\":\"Nieprawidlowe przypisanie czujki; blokada uzytkownika nie jest obslugiwana przez to API\"}");return;
  }
  String error;if(!requestInputPartitions(n,p1=="1",p2=="1",uint32_t(parsed),error)){json(409,"{\"error\":"+jquote(error)+"}");return;}
  json(z7Busy()?202:200,z7Json());
 });
 web.on("/api/input7/partition1",HTTP_POST,[]{
  if(!mutationAllowed())return;
  String desired=web.arg("enabled"),revision=web.arg("revision");uint64_t parsed=0;bool valid=revision.length()>0&&revision.length()<=10;
  for(unsigned i=0;i<revision.length();i++){char c=revision[i];if(c<'0'||c>'9'){valid=false;break;}parsed=parsed*10+(c-'0');}
  if((desired!="0"&&desired!="1")||!valid||parsed>UINT32_MAX){json(400,"{\"error\":\"Nieprawidlowe ustawienie Z7\"}");return;}
  String error;
  if(!requestZ7Partition1(desired=="1",uint32_t(parsed),error)){json(409,"{\"error\":"+jquote(error)+"}");return;}
  json(z7Busy()?202:200,z7Json());
 });
 web.on("/api/input7/prepare",HTTP_POST,[]{
  if(!mutationAllowed())return;
  String revision=web.arg("revision");uint64_t parsed=0;bool valid=revision.length()>0&&revision.length()<=10;
  for(unsigned i=0;i<revision.length();i++){char c=revision[i];if(c<'0'||c>'9'){valid=false;break;}parsed=parsed*10+(c-'0');}
  String scope=web.arg("scope");
  if(!valid||parsed>UINT32_MAX||(scope!=""&&scope!="full"&&scope!="z7")){json(400,"{\"error\":\"Nieprawidlowa wersja lub zakres ustawien\"}");return;}
  String error;
  if(!requestZ7Partition1(bool(app.config[0x1670]&0x40),uint32_t(parsed),error,true,scope=="z7")){json(409,"{\"error\":"+jquote(error)+"}");return;}
  json(202,z7Json());
 });
 web.on("/api/input7/backup",HTTP_GET,[]{
  if(!authenticate())return;
  uint8_t* data=static_cast<uint8_t*>(malloc(0x1c00));
  if(!data){json(503,"{\"error\":\"Brak pamieci\"}");return;}
  if(!z7Backup(data,0x1c00)){free(data);json(404,"{\"error\":\"Brak kopii konfiguracji\"}");return;}
  web.sendHeader("Cache-Control","no-store");web.sendHeader("Content-Disposition","attachment; filename=perfecta-before-z7.bin");web.setContentLength(0x1c00);web.send(200,"application/octet-stream","");web.sendContent(reinterpret_cast<const char*>(data),0x1c00);free(data);
 });
 web.on("/api/supla",HTTP_GET,[]{if(authenticate())json(200,suplaJson());});
 web.on("/api/supla/push-schedule",HTTP_POST,[]{
  if(!mutationAllowed())return;
  String error;
  if(!savePushSchedule(web.arg("day_start"),web.arg("day_end"),web.arg("rules"),error)){json(error.startsWith("Nie zapisano")?500:400,"{\"error\":"+jquote(error)+"}");return;}
  json(200,suplaJson());
 });
 web.on("/api/supla/push",HTTP_POST,[]{
  if(!mutationAllowed())return;
  String flags=web.arg("flags");bool valid=flags.length()>0&&flags.length()<5;
  for(size_t i=0;i<flags.length();i++)if(flags[i]<'0'||flags[i]>'9')valid=false;
  if(!valid||flags.toInt()>2047){json(400,"{\"error\":\"Nieprawidlowe ustawienia PUSH\"}");return;}
  if(!saveSuplaPushFlags(flags.toInt())){json(500,"{\"error\":\"Nie zapisano ustawien\"}");return;}
  json(200,suplaJson());
 });
 web.on("/api/supla/test-push",HTTP_POST,[]{
  if(!mutationAllowed())return;
  if(!queueSuplaTestPush()){json(503,"{\"error\":\"Kolejka PUSH jest pelna\"}");return;}
  json(200,"{\"queued\":true}");
 });
 web.on("/api/control",HTTP_POST,[]{
  if(!mutationAllowed())return;
  if(app.bridge||bridgeClient.connected()||updating||z7Busy()){json(409,"{\"error\":\"Trwa polaczenie Softa, zapis ustawien lub aktualizacja\"}");return;}
  if(!CONTROL_ENABLED){json(403,"{\"error\":\"Sterowanie jest wylaczone w konfiguracji urzadzenia\"}");return;}
  String action=web.arg("action");uint8_t pin=action=="arm"?ARM_PIN:action=="disarm"?DISARM_PIN:0;
  if(!pin){json(400,"{\"error\":\"Nieprawidlowe polecenie\"}");return;}
  if(!startPulse(pin)){json(409,"{\"error\":\"Poczekaj na koniec impulsu i 2-sekundowej blokady\"}");return;}
  json(200,"{\"sent\":true,\"gpio\":"+String(pin)+",\"pulse_ms\":"+String(PULSE_MS)+",\"confirmed_armed_state\":false}");
 });
 web.on("/update",HTTP_GET,[]{
  if(!authenticate())return;
  web.sendHeader("Cache-Control","no-store");web.send(200,"text/html; charset=utf-8",
   "<!doctype html><meta charset=utf-8><title>Aktualizacja PERFECTA ESP</title><h2>Aktualizacja firmware</h2><p>Wybierz firmware.bin. Aktualizacja zrestartuje ESP i przerwie sesje UART.</p><input id=f type=file accept=.bin><button id=b>Wgraj</button><p id=m></p><a href=/>Powrot</a><script>document.getElementById('b').onclick=async()=>{const f=document.getElementById('f').files[0];if(!f)return;const s=await(await fetch('/api/session')).json();const data=new FormData();data.append('firmware',f);document.getElementById('m').textContent='Wgrywanie...';const r=await fetch('/update',{method:'POST',headers:{'X-Perfecta-Token':s.token},body:data});document.getElementById('m').textContent=await r.text();};</script>");
 });
 web.on("/update",HTTP_POST,[]{
  if(!updateAuthorized)return;
  if(updateError.length()||Update.hasError()){updating=false;json(400,"{\"error\":"+jquote(updateError.length()?updateError:"Nieudana aktualizacja")+"}");return;}
  json(200,"{\"ok\":true,\"message\":\"Aktualizacja zakonczona. ESP uruchomi sie ponownie.\"}");
  delay(150);ESP.restart();
 },[]{
  HTTPUpload& upload=web.upload();
  if(upload.status==UPLOAD_FILE_START){
   updateAuthorized=mutationAllowed();updateError="";updating=false;if(!updateAuthorized)return;
   if(app.bridge||bridgeClient.connected()||activeOutput||z7Busy()){updateError="Odlacz PERFECTA Soft i poczekaj na koniec zapisu lub impulsu";return;}
   app.monitorPaused=true;updating=true;
   if(!Update.begin(UPDATE_SIZE_UNKNOWN)){updateError="Brak miejsca na firmware";updating=false;}
  }else if(upload.status==UPLOAD_FILE_WRITE){
   if(updateAuthorized&&updating&&!updateError.length()&&Update.write(upload.buf,upload.currentSize)!=upload.currentSize)updateError="Blad zapisu obrazu";
  }else if(upload.status==UPLOAD_FILE_END){
   if(updateAuthorized&&updating&&!updateError.length()&&!Update.end(true))updateError="Nieprawidlowy obraz firmware";
   if(updateError.length()){Update.abort();updating=false;app.monitorPaused=false;}
  }else if(upload.status==UPLOAD_FILE_ABORTED){Update.abort();updating=false;app.monitorPaused=false;}
 });
 web.onNotFound([]{if(authenticate())json(404,"{\"error\":\"Nie znaleziono\"}");});web.begin();
}
void serviceWeb(){web.handleClient();}
