#pragma once
// Host doubles: no sockets, GPIO, UART or real NVS. monitor.cpp is compiled unchanged.
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <type_traits>
using std::size_t;
class String {
 std::string value;
public:
 String()=default;String(const char* s):value(s){} String(std::string s):value(s){}
 template<class T,typename std::enable_if<std::is_integral<T>::value,int>::type=0> String(T n):value(std::to_string(n)){}
 const char* c_str()const{return value.c_str();}
 String& operator+=(const String& s){value+=s.value;return *this;}
 friend String operator+(String a,const String& b){return a+=b;}
};
inline uint32_t fakeNow=10000;
inline uint32_t millis(){return fakeNow;}
inline void esp_fill_random(void* p,size_t n){memset(p,42,n);}
namespace Perfecta {
inline uint32_t crc(const uint8_t* p,size_t n,uint32_t v=0xffffffff){while(n--){v^=uint32_t(*p++)<<24;for(int i=0;i<8;i++)v=(v<<1)^((v&0x80000000)?0x04c11db7:0);}return v;}
inline uint32_t le32(const uint8_t* p){return uint32_t(p[3])<<24|uint32_t(p[2])<<16|uint32_t(p[1])<<8|p[0];}
inline size_t encode(const uint8_t* p,size_t n,const uint8_t*,const uint8_t*,uint8_t* out,size_t cap){if(n>cap)return 0;memcpy(out,p,n);return n;}
struct Decoder {uint8_t buffer[8192];size_t decode(const uint8_t* p,size_t n,const uint8_t*){memcpy(buffer,p,n);return n;}uint8_t* payload(){return buffer;}};
struct Parser {void reset(){}template<class F>void feed(uint8_t,F){}};
}
struct Preferences {
 std::map<std::string,std::vector<uint8_t>> data;bool failQuick=false,corruptQuick=false;
 size_t putBytes(const char* k,const void* p,size_t n){if(failQuick&&!strcmp(k,"z7quick"))return 0;data[k]=std::vector<uint8_t>((const uint8_t*)p,(const uint8_t*)p+n);return n;}
 size_t getBytes(const char* k,void* p,size_t n){auto it=data.find(k);if(it==data.end()||it->second.size()!=n)return 0;memcpy(p,it->second.data(),n);if(corruptQuick&&!strcmp(k,"z7quick"))((uint8_t*)p)[0]^=1;return n;}
 bool isKey(const char* k){return data.count(k)!=0;}bool remove(const char* k){return data.erase(k)!=0;}size_t freeEntries(){return 300;}
};
struct IPAddress{void fromString(const char*){}bool operator!=(const IPAddress&)const{return false;}};
struct WiFiClient{bool connected(){return false;}void stop(){}void setNoDelay(bool){}void setTimeout(int){}IPAddress remoteIP(){return {}; }size_t write(const uint8_t*,size_t n){return n;}bool available(){return false;}int read(){return 0;}};
struct WiFiServer{bool hasClient(){return false;}WiFiClient available(){return {};}};
struct HardwareSerial{bool available(){return false;}int read(){return 0;}size_t write(const uint8_t*,size_t n){return n;}void setRxBufferSize(int){}void begin(int,int,int,int){}};
struct FakeWiFi{int status(){return 3;}};inline FakeWiFi WiFi;
struct FakeSerial{void println(const char*){}template<class...A>void printf(const char*,A...){}};inline FakeSerial Serial;
constexpr int WL_CONNECTED=3,SERIAL_8N1=0,RX_PIN=16,TX_PIN=17;
struct AppState {
 bool ready=false,hasStatus=false,hasDiag=false,hasRf=false,namesReady=false,selfTest=false;
 uint8_t status[61]={},diag[137]={},rf[44]={},config[6177]={};
 uint32_t statusAt=0,diagAt=0,rfAt=0,sessionCount=0,crcErrors=0,timeouts=0,samples=0,txBytes=0,rxBytes=0;
 uint32_t bridgeTx=0,bridgeRx=0,bridgeAt=0,rejectedClients=0;
 bool historyDirty=false,bridge=false,monitorPaused=false;String error,mode;
 struct{uint32_t violation[16]={},battery[16]={};}history;
};
inline AppState app;inline Preferences prefs;inline HardwareSerial panel;inline WiFiServer bridgeServer;inline WiFiClient bridgeClient;
inline uint8_t activeOutput=0;inline uint32_t pulseCount=0,pulseEndedAt=0;
inline uint32_t panelEpoch(){return 0;}inline String jquote(const String& s){return String("\"")+s+"\"";}
void processPayload(const uint8_t*,size_t);
bool z7Busy();bool z7Backup(uint8_t*,size_t);
bool requestZ7Partition1(bool,uint32_t,String&,bool=false,bool=false);
