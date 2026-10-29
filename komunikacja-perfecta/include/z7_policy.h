#pragma once
#include <stdint.h>
#include <stddef.h>
namespace Z7Policy {
constexpr size_t BANK=0x1c00,OFFSET=0x1670;
constexpr size_t QUICK_BLOCK=128,QUICK_SIZE=2*QUICK_BLOCK;
inline size_t quickAddress(size_t progress){return (progress<QUICK_BLOCK?0x1400:0x1600)+progress%QUICK_BLOCK;}
inline void packQuick(const uint8_t* bank,uint8_t* snapshot){
 for(size_t i=0;i<QUICK_SIZE;i++)snapshot[i]=bank[quickAddress(i)];
}
inline void unpackQuick(const uint8_t* snapshot,uint8_t* bank){
 for(size_t i=0;i<QUICK_SIZE;i++)bank[quickAddress(i)]=snapshot[i];
}
constexpr uint8_t MASK=0x40;
inline uint8_t changed(uint8_t before,bool enabled){return enabled?before|MASK:before&uint8_t(~MASK);}
inline size_t offset(uint8_t number,uint8_t partition){return OFFSET+partition*4+(number-1)/8;}
inline uint8_t mask(uint8_t number){return uint8_t(1u<<((number-1)%8));}
inline uint8_t changedInput(uint8_t before,uint8_t number,bool enabled){uint8_t bit=mask(number);return enabled?before|bit:before&uint8_t(~bit);}
inline bool sensor(const uint8_t* config,uint8_t number){
 if(number<1||number>16||config[0x1440+number-1]>12)return false;
 bool configured=config[0x1400+number-1]!=0;
 for(unsigned i=0;i<4;i++)configured|=config[0x1500+(number-1)*4+i]!=0;
 return configured;
}
inline bool supported(const uint8_t* config){return config[0x1406]==2&&config[0x1446]==14&&(config[0x1674]&MASK);}
inline uint32_t inputRevision(const uint8_t* config,uint8_t number){
 uint32_t hash=2166136261u;
 for(unsigned i=0;i<8;i++){hash^=config[OFFSET+i];hash*=16777619u;}
 for(size_t i:{size_t(0x1400+number-1),size_t(0x1440+number-1)}){hash^=config[i];hash*=16777619u;}
 return hash;
}
inline uint32_t revision(const uint8_t* config){return inputRevision(config,7);}
inline bool selfTest(){
 for(unsigned i=0;i<256;i++)for(unsigned desired=0;desired<2;desired++){
  uint8_t value=changed(i,desired);
  if((value&uint8_t(~MASK))!=(i&uint8_t(~MASK))||bool(value&MASK)!=bool(desired))return false;
 }
 for(uint8_t n=1;n<=16;n++)for(unsigned i=0;i<256;i++)for(unsigned desired=0;desired<2;desired++){
  uint8_t bit=mask(n),value=changedInput(i,n,desired);
  if((value&uint8_t(~bit))!=(i&uint8_t(~bit))||bool(value&bit)!=bool(desired))return false;
  if(offset(n,0)>=offset(n,1)||offset(n,1)>=BANK)return false;
 }
 return OFFSET<BANK&&MASK==(1u<<6);
}
}
