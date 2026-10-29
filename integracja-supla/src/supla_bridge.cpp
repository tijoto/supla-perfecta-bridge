#include "app.h"
#include "supla_bridge.h"
#include "supla_rules.h"
#include "push_schedule_runtime.h"
#include "private_config.h"
#include "private_supla.h"
#include <SuplaDevice.h>
#include <WiFiClientSecure.h>
#include <supla/network/esp_wifi.h>
#include <supla/network/client.h>
#include <supla/channel_element.h>
#include <supla/device/notifications.h>
#include <supla/clock/clock.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <cstring>
using namespace PerfectaSupla;
namespace {
struct Snapshot {
 uint32_t at=0;
 uint16_t violated=0,battery=0;
 bool fresh=false,diagFresh=false,rfFresh=false,controls=false;
 uint8_t state[2]={},alarm[2]={},source[2]={},rf[8]={},active=0;
};
struct Command {uint32_t at;uint8_t pin;};
struct Notice {uint32_t at;int16_t context;uint8_t rule;char message[180];};
struct Summary {
 bool connected=false,started=false;int status=-1;
 uint32_t pushes=0,dropped=0,suppressed=0,accepted=0,rejected=0,stackFree=0;
 char command[96]="Brak polecen SUPLA";
};
QueueHandle_t snapshots=nullptr,commands=nullptr,notices=nullptr;
portMUX_TYPE summaryMux=portMUX_INITIALIZER_UNLOCKED;
Summary summary;
uint32_t publishedAt=0,lastSample=0;
bool primed=false,batteryPrimed=false;
Snapshot previous;
uint32_t motionPushAt[8]={};
Supla::ChannelElement* sensors[12]={};
Snapshot latest;
String detectorName(unsigned i){return app.namesReady?panelName(0x400+(i+8)*16):String(SUPLA_SENSOR_NAMES[i]);}
void dropped(){portENTER_CRITICAL(&summaryMux);summary.dropped++;portEXIT_CRITICAL(&summaryMux);}
bool notice(uint8_t rule,const String& message){
 // One Cloud recipient list for all events; ESP checkboxes select event kinds.
 if(!scheduledPushAllowed(rule)){portENTER_CRITICAL(&summaryMux);summary.suppressed++;portEXIT_CRITICAL(&summaryMux);return false;}
 Notice n={};n.at=millis();n.context=-1;n.rule=rule;message.toCharArray(n.message,sizeof(n.message));
 if(notices&&xQueueSend(notices,&n,0)==pdTRUE)return true;
 dropped();return false;
}
// Validate the user-selected trust anchor and TLS name; an optional pin is additional.
class VerifiedTlsClient : public Supla::Client {
 WiFiClientSecure client;
 public:
 int available() override{return client.available();}
 void stop() override{client.stop();}
 uint8_t connected() override{return client.connected();}
 void setTimeoutMs(uint16_t ms) override{client.setTimeout(ms);}
 protected:
 int connectImp(const char* host,uint16_t port) override{
  client.stop();IPAddress ip;if(port!=SUPLA_SERVER_PORT)return 0;
  if(!ip.fromString(host)&&WiFi.hostByName(host,ip)!=1)return 0;
  client.setHandshakeTimeout(5);client.setTimeout(3000);
  if(!client.connect(ip,port,SUPLA_TLS_SERVER_NAME,SUPLA_SERVER_CERT,nullptr,nullptr))return 0;
  uint8_t fingerprint[32]={};
  if(SUPLA_USE_PIN&&(!client.getFingerprintSHA256(fingerprint)||memcmp(fingerprint,SUPLA_CERT_SHA256,32)!=0)){client.stop();return 0;}
  srcIp=uint32_t(client.localIP());return 1;
 }
 size_t writeImp(const uint8_t* data,size_t size) override{return client.write(data,size);}
 int readImp(uint8_t* data,size_t size) override{return client.available()?client.read(data,size):-1;}
};
class AlarmChannel : public Supla::ChannelElement {
 public:
 AlarmChannel(){
  channel.setType(SUPLA_CHANNELTYPE_RELAY);channel.setFuncList(SUPLA_BIT_FUNC_POWERSWITCH);
  channel.setDefaultFunction(SUPLA_CHANNELFNC_POWERSWITCH);channel.setInitialCaption("Alarm");
 }
 void onInit() override{channel.setNewValue(false);channel.setStateOffline();}
 int32_t handleNewValueFromServer(TSD_SuplaChannelNewValue* value) override{
  // Only explicit server commands reach this method. Startup/registration never
  // restores or executes the last relay value. Feedback always comes from UART.
  if(!validCommandValue(value->value[0])||!latest.controls||uint32_t(millis()-latest.at)>1500||!knownArming(latest.state[0])||!knownArming(latest.state[1]))return 0;
  bool on=value->value[0]==1;
  if((on&&latest.state[0]==5&&latest.state[1]==5)||(!on&&latest.state[0]==0&&latest.state[1]==0))return 1;
  Command c={millis(),on?ARM_PIN:DISARM_PIN};
  if(xQueueSend(commands,&c,0)!=pdTRUE)return 0;
  return 1;
 }
};
AlarmChannel* alarmChannel=nullptr;
void updateChannels(){
 const bool valid=latest.fresh&&uint32_t(millis()-latest.at)<1500;
 for(unsigned i=0;i<12;i++){
  auto c=sensors[i]->getChannel();
  if(!valid){c->setStateOffline();continue;}
  if(i<2&&!knownArming(latest.state[i])){c->setStateOnlineAndNotAvailable();continue;}
  if(i>=2&&i<4&&latest.alarm[i-2]!=0&&latest.alarm[i-2]!=0x44){c->setStateOnlineAndNotAvailable();continue;}
  bool value=i<2?armed(latest.state[i]):i<4?latest.alarm[i-2]==0x44:(latest.violated&(1u<<(i+4)))!=0;
  c->setStateOnline();c->setNewValue(value);
  if(i>=4){
   c->setBatteryState(!latest.diagFresh?Supla::BatteryState::NotSet:(latest.battery&(1u<<(i+4)))?Supla::BatteryState::Low:Supla::BatteryState::Normal);
   c->setBridgeSignalStrength(latest.rfFresh?latest.rf[i-4]:255);
  }
 }
 auto c=alarmChannel->getChannel();
 if(valid&&knownArming(latest.state[0])&&knownArming(latest.state[1])){
  c->setNewValue(armed(latest.state[0])||armed(latest.state[1]));
  if(latest.controls)c->setStateOnline();else c->setStateOnlineAndNotAvailable();
 }else c->setStateOffline();
}
void suplaTask(void*){
 auto wifi=new Supla::ESPWifi;wifi->setSsid(WIFI_SSID);wifi->setPassword(WIFI_PASSWORD);
 wifi->setHostname("perfecta-esp",0);
 for(unsigned i=0;i<12;i++){
  sensors[i]=new Supla::ChannelElement;
  auto c=sensors[i]->getChannel();c->setType(SUPLA_CHANNELTYPE_BINARYSENSOR);
  c->setDefaultFunction(i<2?SUPLA_CHANNELFNC_ALARMARMAMENTSENSOR:i<4?SUPLA_CHANNELFNC_BINARY_SENSOR:SUPLA_CHANNELFNC_MOTION_SENSOR);
  String caption=i<2?String("Czuwanie strefy ")+String(i+1):i<4?String("Alarm strefy ")+String(i-1):String(i+5)+": "+SUPLA_SENSOR_NAMES[i-4];
  c->setInitialCaption(caption.c_str());c->setStateOffline();
  if(i>=4)c->setBatteryPowered(true);
 }
 alarmChannel=new AlarmChannel;
 Supla::Notification::RegisterNotification(-1);
 // NTP owns system UTC. The SDK's local-time sync assumes tm_isdst=0 and
 // would shift summer time by one hour. Keep one clock writer and let the
 // application use the fresh panel clock until NTP is available.
 auto suplaClock=new Supla::Clock;
 suplaClock->setUseAutomaticTimeSyncRemoteConfig(false);
 suplaClock->setAutomaticTimeSync(false);

 SuplaDevice.setLogLevel(2);
 SuplaDevice.setName("PERFECTA 16-WRL ESP");SuplaDevice.setSwVersion(VERSION);
 SuplaDevice.setAutomaticResetOnConnectionProblem(0);
 SuplaDevice.setSupla3rdPartyCACert(SUPLA_SERVER_CERT);
 SuplaDevice.setServerPort(SUPLA_SERVER_PORT);
 SuplaDevice.getSrpcLayer()->setNetworkClient(new VerifiedTlsClient);
 bool began=SuplaDevice.begin(reinterpret_cast<const char*>(SUPLA_GUID_BYTES),SUPLA_SERVER,SUPLA_EMAIL,reinterpret_cast<const char*>(SUPLA_AUTHKEY_BYTES),28);
 portENTER_CRITICAL(&summaryMux);summary.started=began;portEXIT_CRITICAL(&summaryMux);
 Notice pending={};bool hasPending=false;
 for(;;){
  Snapshot s;if(xQueueReceive(snapshots,&s,0)==pdTRUE)latest=s;
  updateChannels();SuplaDevice.iterate();
  bool connected=SuplaDevice.getSrpcLayer()->isRegisteredAndReady();
  if(!hasPending&&xQueueReceive(notices,&pending,0)==pdTRUE)hasPending=true;
  if(hasPending&&uint32_t(millis()-pending.at)>300000){hasPending=false;dropped();}
  if(hasPending&&!scheduledPushAllowed(pending.rule)){
   hasPending=false;portENTER_CRITICAL(&summaryMux);summary.suppressed++;portEXIT_CRITICAL(&summaryMux);
  }
  if(hasPending&&connected&&Supla::Notification::Send(pending.context,"PERFECTA",pending.message)){
   hasPending=false;portENTER_CRITICAL(&summaryMux);summary.pushes++;portEXIT_CRITICAL(&summaryMux);
  }
  portENTER_CRITICAL(&summaryMux);
  summary.connected=connected;summary.status=SuplaDevice.getCurrentStatus();summary.stackFree=uxTaskGetStackHighWaterMark(nullptr);
  portEXIT_CRITICAL(&summaryMux);
  vTaskDelay(pdMS_TO_TICKS(10));
 }
}
}
void setupSupla(){
 if(!PerfectaSupla::selfTest()){app.error="Blad autotestu sterowania SUPLA";return;}
 if(!setupPushSchedule(prefs.getUShort("pushflags",PUSH_ALARM|PUSH_ARMED|PUSH_BATTERY)&PUSH_MASK)){app.error="Blad autotestu harmonogramow PUSH";return;}
 if(!SUPLA_ENABLED){WiFi.mode(WIFI_STA);WiFi.setAutoReconnect(true);WiFi.begin(WIFI_SSID,WIFI_PASSWORD);return;}
 snapshots=xQueueCreate(1,sizeof(Snapshot));commands=xQueueCreate(1,sizeof(Command));notices=xQueueCreate(16,sizeof(Notice));
 if(!snapshots||!commands||!notices){app.error="Brak RAM dla SUPLA";return;}
 if(xTaskCreatePinnedToCore(suplaTask,"supla",16384,nullptr,1,nullptr,0)!=pdPASS)app.error="Nie uruchomiono SUPLA";
}
void serviceSupla(){
 servicePushClock();
 uint32_t now=millis();Command command;
 if(commands&&xQueueReceive(commands,&command,0)==pdTRUE){
  bool fresh=app.selfTest&&app.hasStatus&&uint32_t(now-app.statusAt)<4000;
  bool ok=commandAllowed(now,command.at,fresh,app.bridge||bridgeClient.connected(),app.monitorPaused,activeOutput!=0,pulseEndedAt,pulseCount!=0)&&startPulse(command.pin);
  portENTER_CRITICAL(&summaryMux);
  if(ok)summary.accepted++;else summary.rejected++;
  if(ok)snprintf(summary.command,sizeof(summary.command),"%s: impuls %u ms - sprawdz stan stref",command.pin==ARM_PIN?"Alarm ON":"Alarm OFF",unsigned(PULSE_MS));
  else snprintf(summary.command,sizeof(summary.command),"%s: odrzucono - brak danych, zajety port lub blokada",command.pin==ARM_PIN?"Alarm ON":"Alarm OFF");
  portEXIT_CRITICAL(&summaryMux);
 }
 if(!snapshots||uint32_t(now-publishedAt)<50)return;
 publishedAt=now;
 Snapshot s;s.at=now;s.fresh=app.hasStatus&&uint32_t(now-app.statusAt)<4000;
 s.diagFresh=app.hasDiag&&uint32_t(now-app.diagAt)<15000;s.rfFresh=app.hasRf&&uint32_t(now-app.rfAt)<15000;
  s.controls=CONTROL_ENABLED&&s.fresh&&!app.bridge&&!bridgeClient.connected()&&!app.monitorPaused&&!z7Busy()&&!activeOutput&&(!pulseCount||uint32_t(now-pulseEndedAt)>=2000);
 s.violated=Perfecta::le32(app.status+34);s.battery=Perfecta::le32(app.diag+58);s.active=activeOutput;
 for(unsigned i=0;i<2;i++){s.state[i]=app.status[52+i];s.alarm[i]=app.status[54+i];s.source[i]=app.status[56+i];}
 for(unsigned i=0;i<8;i++)s.rf[i]=app.rf[5+i+8];
 xQueueOverwrite(snapshots,&s);
 if(s.fresh&&lastSample!=app.samples){
  uint16_t pushFlags=scheduledPushFlags();
  lastSample=app.samples;
  if(primed){
   for(unsigned i=0;i<2;i++){
    if((pushFlags&PUSH_ARMED)&&knownArming(previous.state[i])&&knownArming(s.state[i])&&armed(s.state[i])!=armed(previous.state[i]))
     notice(2+i,String("Strefa ")+String(i+1)+(armed(s.state[i])?": czuwanie zalaczone":": czuwanie wylaczone"));
    if((pushFlags&PUSH_ALARM)&&previous.alarm[i]!=0x44&&s.alarm[i]==0x44){
     String message=String("ALARM strefy ")+String(i+1);uint8_t n=s.source[i];
     if(n>=1&&n<=16)message+=" | "+String(n)+": "+panelName(0x400+(n-1)*16);
     notice(i,message);
    }
   }
   for(unsigned i=0;i<8;i++)if((pushFlags&(1u<<(i+3)))&&rising(previous.violated,s.violated,i+8)&&(!motionPushAt[i]||uint32_t(now-motionPushAt[i])>=30000)){
    if(notice(i+4,String("Naruszenie ")+String(i+9)+": "+detectorName(i)))motionPushAt[i]=now;
   }
  }
  previous.violated=s.violated;memcpy(previous.state,s.state,2);memcpy(previous.alarm,s.alarm,2);primed=true;
 }
 if(s.diagFresh){
  if(batteryPrimed&&(scheduledPushFlags()&PUSH_BATTERY))for(unsigned i=0;i<8;i++)if(rising(previous.battery,s.battery,i+8))notice(i+12,String("Slaba bateria ")+String(i+9)+": "+detectorName(i));
  previous.battery=s.battery;batteryPrimed=true;
 }
}
String suplaJson(){
 Summary s;portENTER_CRITICAL(&summaryMux);s=summary;portEXIT_CRITICAL(&summaryMux);
 String out="{\"server\":"+jquote(SUPLA_SERVER)+",\"location_id\":null,\"location_managed_by_cloud\":true,\"connected\":"+String(s.connected?"true":"false")+",\"started\":"+String(s.started?"true":"false")+",\"status\":"+String(s.status);
 out+=",\"channels\":13,\"tls\":true,\"rules_self_test\":"+String(PerfectaSupla::selfTest()?"true":"false")+",\"push_flags\":"+String(scheduledPushFlags())+",\"push_sent\":"+String(s.pushes)+",\"push_dropped\":"+String(s.dropped)+",\"push_suppressed\":"+String(s.suppressed)+",\"commands_accepted\":"+String(s.accepted)+",\"commands_rejected\":"+String(s.rejected)+",\"last_command\":"+jquote(s.command)+",\"task_stack_free\":"+String(s.stackFree)+",\"sensor_names\":[";
 for(unsigned i=0;i<8;i++){if(i)out+=',';out+=jquote(detectorName(i));}return out+"],\"push_schedule\":"+pushScheduleJson()+"}";
}
bool saveSuplaPushFlags(uint16_t flags){return saveLegacyPushFlags(flags);}
bool queueSuplaTestPush(){return notice(PushSchedule::Test,"Test PUSH z PERFECTA ESP - bez sterowania alarmem");}
