#include "app.h"
#include "private_config.h"
#include "cp1250.h"
#include "supla_bridge.h"
#include <ESPmDNS.h>
#include <esp_wifi.h>
AppState app;
HardwareSerial panel(2);
WiFiServer bridgeServer(BRIDGE_PORT);
WiFiClient bridgeClient;
WebServer web(80);
Preferences prefs;
String csrfToken;
volatile uint8_t activeOutput=0;
volatile uint32_t pulseEndedAt=0;
uint32_t lastPulseAt=0,pulseCount=0;
static esp_timer_handle_t pulseTimer;
static bool wasConnected=false;
static uint32_t logAt=0;
void releaseOutputs(void*){
 gpio_set_level(gpio_num_t(ARM_PIN),OUTPUT_IDLE_LEVEL);gpio_set_level(gpio_num_t(DISARM_PIN),OUTPUT_IDLE_LEVEL);
 activeOutput=0;pulseEndedAt=millis();
}
void setupOutputs(){
 // Set the HIGH idle latch before enabling active-LOW relay outputs.
 gpio_set_level(gpio_num_t(ARM_PIN),OUTPUT_IDLE_LEVEL);gpio_set_level(gpio_num_t(DISARM_PIN),OUTPUT_IDLE_LEVEL);
 for(uint8_t pin:{ARM_PIN,DISARM_PIN}){
  gpio_set_direction(gpio_num_t(pin),GPIO_MODE_INPUT_OUTPUT);gpio_pulldown_dis(gpio_num_t(pin));gpio_pullup_en(gpio_num_t(pin));gpio_set_level(gpio_num_t(pin),OUTPUT_IDLE_LEVEL);
 }
 esp_timer_create_args_t args={};args.callback=releaseOutputs;args.name="gpio-pulse";
 ESP_ERROR_CHECK(esp_timer_create(&args,&pulseTimer));
}
bool startPulse(uint8_t pin){
 if(!CONTROL_ENABLED||z7Busy())return false;
 if((pin!=ARM_PIN&&pin!=DISARM_PIN)||activeOutput||(pulseCount&&uint32_t(millis()-pulseEndedAt)<2000))return false;
 releaseOutputs(nullptr);activeOutput=pin;lastPulseAt=millis();pulseCount++;
 // Arm independent timer BEFORE driving LOW. Failure leaves both outputs HIGH.
 if(esp_timer_start_once(pulseTimer,PULSE_US)!=ESP_OK){releaseOutputs(nullptr);return false;}
 gpio_set_level(gpio_num_t(pin),OUTPUT_ACTIVE_LEVEL);return true;
}
String jquote(const String& value){
 String s="\"";
 for(size_t i=0;i<value.length();i++){uint8_t c=value[i];if(c=='"'||c=='\\'){s+='\\';s+=char(c);}else if(c<32){char b[7];snprintf(b,sizeof(b),"\\u%04x",c);s+=b;}else s+=char(c);}
 return s+'"';
}
String panelName(size_t offset){
 String s;for(size_t i=offset;i<offset+16;i++){
  uint16_t c=app.config[i];if(c==0)c=' ';else if(c>=128)c=CP1250[c-128];
  if(c<128)s+=char(c);else if(c<2048){s+=char(0xc0|(c>>6));s+=char(0x80|(c&63));}else{s+=char(0xe0|(c>>12));s+=char(0x80|((c>>6)&63));s+=char(0x80|(c&63));}
 }s.trim();return s;
}
static int bcd(uint8_t c){return (c>>4)*10+(c&15);}
uint32_t panelEpoch(){
 if(app.hasStatus){
  tm t={};t.tm_year=100+bcd(app.status[1]);t.tm_mon=bcd(app.status[2])-1;t.tm_mday=bcd(app.status[3]);
  t.tm_hour=bcd(app.status[4]);t.tm_min=bcd(app.status[5]);t.tm_sec=bcd(app.status[6]);t.tm_isdst=-1;
  if(t.tm_mon>=0&&t.tm_mon<12&&t.tm_mday>0&&t.tm_mday<=31){time_t epoch=mktime(&t);if(epoch>1700000000)return epoch;}
 }
 time_t now=time(nullptr);return now>1700000000?uint32_t(now):0;
}
void historyTick(){
 if(app.historyDirty&&uint32_t(millis()-app.lastHistorySave)>=60000){
  app.lastHistorySave=millis();if(prefs.putBytes("history",&app.history,sizeof(app.history))==sizeof(app.history))app.historyDirty=false;
 }
}
void setup(){
 setupOutputs();
 Serial.begin(115200);delay(100);Serial.printf("\n%s\n",VERSION);
 if(BUILD_TEST_ONLY){app.monitorPaused=true;Serial.println("BUILD TEST ONLY - networking and panel communication disabled");return;}
 prefs.begin("perfecta-esp",false);
 if(prefs.getBytes("history",&app.history,sizeof(app.history))!=sizeof(app.history)||app.history.magic!=0x50524631){app.history={};app.history.magic=0x50524631;}
 uint8_t random[16];esp_fill_random(random,sizeof(random));for(uint8_t b:random){char tmp[3];snprintf(tmp,sizeof(tmp),"%02x",b);csrfToken+=tmp;}
 WiFi.persistent(false);WiFi.setSleep(false);
 configTzTime("CET-1CEST,M3.5.0,M10.5.0/3","pool.ntp.org","time.nist.gov");
 setupPanel();setupWeb();bridgeServer.begin();bridgeServer.setNoDelay(true);setupSupla();
 Serial.printf("UART RX=%u TX=%u 19200 8N1 | ARM=%u DISARM=%u push-pull LOW %ums, idle HIGH 3.3V\n",RX_PIN,TX_PIN,ARM_PIN,DISARM_PIN,unsigned(PULSE_MS));
}
void loop(){
 if(BUILD_TEST_ONLY){delay(100);return;}
 servicePanel();serviceWeb();historyTick();serviceSupla();
 bool connected=WiFi.status()==WL_CONNECTED;
 if(connected&&!wasConnected){Serial.printf("IP: %s | http://perfecta-esp.local | bridge TCP %u\n",WiFi.localIP().toString().c_str(),BRIDGE_PORT);MDNS.begin("perfecta-esp");MDNS.addService("http","tcp",80);}
 wasConnected=connected;
 if(uint32_t(millis()-logAt)>30000){logAt=millis();Serial.printf("STATE wifi=%d ip=%s heap=%u minheap=%u panel=%s outputs=%u/%u\n",connected,WiFi.localIP().toString().c_str(),ESP.getFreeHeap(),ESP.getMinFreeHeap(),app.ready?"ready":"waiting",gpio_get_level(gpio_num_t(ARM_PIN)),gpio_get_level(gpio_num_t(DISARM_PIN)));}
 delay(1);
}
