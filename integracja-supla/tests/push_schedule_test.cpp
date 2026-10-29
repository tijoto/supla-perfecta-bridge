#include "push_schedule.h"
#include <assert.h>
#include <cstring>
#include <iostream>
#include <vector>
using namespace PushSchedule;
int main(){
 assert(selfTest());
 uint64_t checks=0;
 // Build expected weekly coverage as a chronological calendar, independently
 // of the implementation's wrap-around / previous-weekday checks.
 const uint16_t intervals[][2]={{0,0},{0,1},{480,1020},{1320,360},{1439,1},{600,600}};
 for(const auto& interval:intervals)for(uint8_t mask=1;mask<128;mask++){
  Config c=defaults(1);Rule& rule=c.rules[0];rule.mode=Weekly;rule.days=mask;rule.start=interval[0];rule.end=interval[1];
  std::vector<bool> expected(10080,false);
  for(int day=0;day<7;day++)if(mask&(1<<day)){
   int length=rule.start==rule.end?1440:(rule.end+1440-rule.start)%1440;
   // Equal hours select the complete civil weekday, irrespective of hour.
   int start=day*1440+(rule.start==rule.end?0:rule.start);
   for(int n=0;n<length;n++)expected[(start+n)%10080]=true;
  }
  for(int n=0;n<10080;n++){
   assert(allowed(c,0,{true,uint8_t(n/1440),uint16_t(n%1440)})==expected[n]);checks++;
  }
 }
 for(uint16_t flags=0;flags<2048;flags++){
  Config c=defaults(flags);assert(valid(c));assert(legacyFlags(c)==flags);
  for(uint8_t id=0;id<Count;id++)assert(allowed(c,id,{})==bool(flags&legacyBit(id)));
  checks+=21;
 }
 Config c=defaults(2047);c.checksum=checksum(c);
 for(size_t byte=0;byte<offsetof(Config,checksum);byte++)for(uint8_t bit=0;bit<8;bit++){
  Config damaged=c;reinterpret_cast<uint8_t*>(&damaged)[byte]^=1u<<bit;
  assert(damaged.checksum!=checksum(damaged));checks++;
 }
 // Day and night are disjoint and cover all local minutes, including a
 // definition of day that itself crosses midnight.
 for(const auto& hours:intervals){
  if(hours[0]==hours[1])continue;c.dayStart=hours[0];c.dayEnd=hours[1];
  for(uint16_t minute=0;minute<1440;minute++){
   c.rules[0].mode=Day;bool day=allowed(c,0,{true,0,minute});
   c.rules[0].mode=Night;bool night=allowed(c,0,{true,0,minute});assert(day!=night);checks++;
  }
 }
 std::cout<<"PASS "<<checks<<" schedule checks\n";
}
