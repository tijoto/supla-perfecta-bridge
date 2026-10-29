#pragma once
#include <stdint.h>
#include <stddef.h>

namespace PushSchedule {
constexpr uint8_t Count=20,Test=255;
constexpr uint32_t Magic=0x50534831;
enum Mode:uint8_t {Always=0,Day=1,Night=2,Weekly=3};
struct Rule {
 uint16_t start=0,end=0;
 uint8_t enabled=0,mode=Always,days=127,reserved=0;
};
struct Config {
 uint32_t magic=Magic;
 uint16_t version=1,dayStart=360,dayEnd=1320,reserved=0;
 Rule rules[Count];
 uint32_t checksum=0;
};
struct Clock {bool valid=false;uint8_t weekday=0;uint16_t minute=0;};
static_assert(sizeof(Rule)==8&&sizeof(Config)==176,"Schedule storage layout changed");
inline uint16_t legacyBit(uint8_t id){return id<2?1:id<4?2:id<12?uint16_t(1u<<(id-1)):4;}
inline uint16_t legacyFlags(const Config& c){uint16_t flags=0;for(uint8_t i=0;i<Count;i++)if(c.rules[i].enabled)flags|=legacyBit(i);return flags;}
inline Config defaults(uint16_t flags){Config c;for(uint8_t i=0;i<Count;i++)c.rules[i].enabled=(flags&legacyBit(i))!=0;return c;}
inline bool valid(const Config& c){
 if(c.magic!=Magic||c.version!=1||c.reserved||c.dayStart>=1440||c.dayEnd>=1440||c.dayStart==c.dayEnd)return false;
 for(const Rule& r:c.rules)if(r.start>=1440||r.end>=1440||r.enabled>1||r.mode>Weekly||r.days>127||r.reserved||(r.enabled&&r.mode==Weekly&&!r.days))return false;
 return true;
}
inline uint32_t checksum(const Config& c){
 const uint8_t* p=reinterpret_cast<const uint8_t*>(&c);uint32_t hash=2166136261u;
 for(size_t i=0;i<offsetof(Config,checksum);i++){hash^=p[i];hash*=16777619u;}return hash;
}
inline bool inRange(uint16_t minute,uint16_t start,uint16_t end){
 return start==end|| (start<end?(minute>=start&&minute<end):(minute>=start||minute<end));
}
inline bool allowed(const Config& c,uint8_t id,Clock clock){
 if(id==Test)return true;
 if(id>=Count||!c.rules[id].enabled)return false;
 const Rule& r=c.rules[id];if(r.mode==Always)return true;
 if(!clock.valid||clock.weekday>6||clock.minute>=1440)return false;
 const bool day=inRange(clock.minute,c.dayStart,c.dayEnd);
 if(r.mode==Day)return day;
 if(r.mode==Night)return !day;
 if(r.mode!=Weekly||!inRange(clock.minute,r.start,r.end))return false;
 uint8_t weekday=clock.weekday;
 // Overnight intervals belong to the weekday on which they START.
 if(r.start>r.end&&clock.minute<r.end)weekday=(weekday+6)%7;
 return (r.days&(1u<<weekday))!=0;
}
inline bool selfTest(){
 Config c=defaults(2047);Clock t{true,0,359};
 if(!valid(c)||legacyFlags(c)!=2047||!allowed(c,0,{})||allowed(c,20,t))return false;
 c.rules[0].mode=Day;
 if(allowed(c,0,t)||!allowed(c,0,{true,0,360})||!allowed(c,0,{true,6,1319})||allowed(c,0,{true,6,1320})||allowed(c,0,{}))return false;
 c.rules[0].mode=Night;
 if(!allowed(c,0,t)||allowed(c,0,{true,0,360})||allowed(c,0,{true,0,1319})||!allowed(c,0,{true,0,1320})||allowed(c,0,{}))return false;
 c.dayStart=1320;c.dayEnd=360;c.rules[0].mode=Day;
 if(!allowed(c,0,{true,0,0})||allowed(c,0,{true,0,360})||!allowed(c,0,{true,0,1320}))return false;
 c.rules[0].mode=Weekly;c.rules[0].days=1;c.rules[0].start=1320;c.rules[0].end=360;
 if(!allowed(c,0,{true,0,1320})||!allowed(c,0,{true,1,359})||allowed(c,0,{true,1,360})||allowed(c,0,{true,0,359})||allowed(c,0,{true,1,1320}))return false;
 c.rules[0].days=64;
 if(!allowed(c,0,{true,6,1439})||!allowed(c,0,{true,0,0})||allowed(c,0,{true,0,360}))return false;
 c.rules[0].days=31;c.rules[0].start=480;c.rules[0].end=1020;
 if(!allowed(c,0,{true,4,480})||!allowed(c,0,{true,4,1019})||allowed(c,0,{true,4,1020})||allowed(c,0,{true,5,600}))return false;
 c.rules[0].start=0;c.rules[0].end=0;
 if(!allowed(c,0,{true,4,1439})||allowed(c,0,{true,5,0}))return false;
 c.rules[0].enabled=0;if(allowed(c,0,t)||!allowed(c,Test,{}))return false;
 Config stored=defaults(7);stored.checksum=checksum(stored);
 if(legacyFlags(stored)!=7||stored.checksum!=checksum(stored))return false;
 stored.rules[4].enabled=1;if(stored.checksum==checksum(stored)||legacyFlags(stored)!=15)return false;
 stored.dayStart=stored.dayEnd;if(valid(stored))return false;
 stored=defaults(0);stored.rules[0].enabled=1;stored.rules[0].mode=Weekly;stored.rules[0].days=0;if(valid(stored))return false;
 stored=defaults(0);stored.rules[0].mode=4;if(valid(stored))return false;
 stored=defaults(0);stored.rules[0].start=1440;if(valid(stored))return false;
 return true;
}
}
