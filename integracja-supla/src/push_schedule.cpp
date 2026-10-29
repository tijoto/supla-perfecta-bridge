#include "app.h"
#include "private_supla.h"
#include "push_schedule_runtime.h"
#include <cstring>
#include <cstdlib>
namespace {
using namespace PushSchedule;
portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
Config config;
Clock clockValue;
uint32_t clockAt=0;
char clockLabel[24]="",clockSource[12]="brak";
bool persisted=false,recovered=false;
Config copyConfig(){Config c;portENTER_CRITICAL(&mux);c=config;portEXIT_CRITICAL(&mux);return c;}
Clock copyClock(){Clock c;uint32_t at;portENTER_CRITICAL(&mux);c=clockValue;at=clockAt;portEXIT_CRITICAL(&mux);if(uint32_t(millis()-at)>2500)c.valid=false;return c;}
bool saveConfig(Config c){
 if(!valid(c))return false;
 c.checksum=checksum(c);
 Config old=copyConfig();if(persisted&&memcmp(&c,&old,sizeof(c))==0)return true;
 if(prefs.putBytes("pushsched",&c,sizeof(c))!=sizeof(c))return false;
 Config readback;if(prefs.getBytes("pushsched",&readback,sizeof(readback))!=sizeof(readback)||memcmp(&c,&readback,sizeof(c)))return false;
 portENTER_CRITICAL(&mux);config=c;persisted=true;recovered=false;portEXIT_CRITICAL(&mux);return true;
}
bool number(const char*& p,uint16_t& value,char delimiter){
 if(*p<'0'||*p>'9')return false;
 unsigned v=0,digits=0;
 do{if(++digits>4)return false;v=v*10+unsigned(*p-'0');p++;}while(*p>='0'&&*p<='9');
 if(*p!=delimiter)return false;
 value=v;if(delimiter)p++;return true;
}
bool scalar(const String& s,uint16_t& value){const char* p=s.c_str();return number(p,value,'\0');}
int bcd(uint8_t v){return (v>>4)<=9&&(v&15)<=9?(v>>4)*10+(v&15):-1;}
String eventLabel(uint8_t i){
 if(i<2)return String("Alarm strefy ")+String(i+1);
 if(i<4)return String("Zmiana czuwania strefy ")+String(i-1);
 uint8_t n=i<12?i-4:i-12;
 return String(i<12?"Naruszenie ":"Slaba bateria ")+String(n+9)+": "+SUPLA_SENSOR_NAMES[n];
}
}
bool setupPushSchedule(uint16_t flags){
 if(!PushSchedule::selfTest())return false;
 Config stored;size_t size=prefs.getBytesLength("pushsched");
 if(size==sizeof(stored)&&prefs.getBytes("pushsched",&stored,sizeof(stored))==sizeof(stored)&&valid(stored)&&stored.checksum==checksum(stored)){
  config=stored;persisted=true;
 }else{config=defaults(flags);config.checksum=checksum(config);recovered=size!=0;}
 servicePushClock();return true;
}
void servicePushClock(){
 uint32_t now=millis();if(clockAt&&uint32_t(now-clockAt)<1000)return;
 tm civil={};time_t epoch=time(nullptr);bool ok=epoch>1700000000&&localtime_r(&epoch,&civil);
 const char* source="NTP";
 if(!ok&&app.hasStatus&&uint32_t(now-app.statusAt)<4000){
  int year=bcd(app.status[1]),month=bcd(app.status[2]),day=bcd(app.status[3]);
  int hour=bcd(app.status[4]),minute=bcd(app.status[5]),second=bcd(app.status[6]);
  if(year>=23&&month>=1&&month<=12&&day>=1&&day<=31&&hour>=0&&hour<24&&minute>=0&&minute<60&&second>=0&&second<60){
   civil.tm_year=100+year;civil.tm_mon=month-1;civil.tm_mday=day;civil.tm_hour=hour;civil.tm_min=minute;civil.tm_sec=second;civil.tm_isdst=-1;
   epoch=mktime(&civil);ok=epoch>1700000000&&civil.tm_year==100+year&&civil.tm_mon==month-1&&civil.tm_mday==day&&civil.tm_hour==hour&&civil.tm_min==minute;
  }
  source="centrala";
 }
 Clock c;if(ok){c.valid=true;c.weekday=(civil.tm_wday+6)%7;c.minute=civil.tm_hour*60+civil.tm_min;}
 char label[24]="";if(ok)strftime(label,sizeof(label),"%Y-%m-%d %H:%M:%S",&civil);
 portENTER_CRITICAL(&mux);clockValue=c;clockAt=now;strncpy(clockLabel,label,sizeof(clockLabel));strncpy(clockSource,ok?source:"brak",sizeof(clockSource));portEXIT_CRITICAL(&mux);
}
bool scheduledPushAllowed(uint8_t rule){return PushSchedule::allowed(copyConfig(),rule,copyClock());}
uint16_t scheduledPushFlags(){return PushSchedule::legacyFlags(copyConfig());}
bool saveLegacyPushFlags(uint16_t flags){
 if(flags&~0x7ff)return false;
 Config c=copyConfig();
 for(uint8_t i=0;i<Count;i++)c.rules[i].enabled=(flags&legacyBit(i))!=0;
 return saveConfig(c);
}
bool savePushSchedule(const String& dayStart,const String& dayEnd,const String& rules,String& error){
 Config c;uint16_t from,to;
 if(!scalar(dayStart,c.dayStart)||!scalar(dayEnd,c.dayEnd)||rules.length()>900){error="Nieprawidlowe godziny dnia";return false;}
 const char* p=rules.c_str();
 for(uint8_t i=0;i<Count;i++){
  uint16_t enabled,mode,days;
  if(!number(p,enabled,',')||!number(p,mode,',')||!number(p,days,',')||!number(p,from,',')||!number(p,to,i==Count-1?'\0':';')||enabled>1||mode>Weekly||days>127){error="Nieprawidlowa lista 20 regul";return false;}
  c.rules[i].enabled=enabled;c.rules[i].mode=mode;c.rules[i].days=days;c.rules[i].start=from;c.rules[i].end=to;
 }
 if(!valid(c)){error="Sprawdz godziny i dni tygodnia. Poczatek i koniec dnia musza byc rozne.";return false;}
 if(!saveConfig(c)){error="Nie zapisano harmonogramu w pamieci ESP";return false;}return true;
}
String pushScheduleJson(){
 servicePushClock();Config c=copyConfig();Clock clock=copyClock();
 char label[24],source[12];portENTER_CRITICAL(&mux);memcpy(label,clockLabel,sizeof(label));memcpy(source,clockSource,sizeof(source));portEXIT_CRITICAL(&mux);
 String out;out.reserve(4200);
 out="{\"version\":1,\"day_start\":"+String(c.dayStart)+",\"day_end\":"+String(c.dayEnd)+",\"time_valid\":"+String(clock.valid?"true":"false")+",\"local_time\":"+jquote(label)+",\"time_source\":"+jquote(source)+",\"timezone\":\"Europe/Warsaw\",\"weekday\":"+String(clock.weekday)+",\"minute\":"+String(clock.minute)+",\"persisted\":"+String(persisted?"true":"false")+",\"recovered\":"+String(recovered?"true":"false")+",\"self_test\":"+String(selfTest()?"true":"false")+",\"rules\":[";
 for(uint8_t i=0;i<Count;i++){
  const Rule& r=c.rules[i];if(i)out+=',';
  out+="{\"id\":"+String(i)+",\"label\":"+jquote(eventLabel(i))+",\"enabled\":"+String(r.enabled?"true":"false")+",\"mode\":"+String(r.mode)+",\"days\":"+String(r.days)+",\"start\":"+String(r.start)+",\"end\":"+String(r.end)+",\"active_now\":"+String(allowed(c,i,clock)?"true":"false")+"}";
 }return out+"]}";
}
