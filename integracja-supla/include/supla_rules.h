#pragma once
#include <stdint.h>
namespace PerfectaSupla {
constexpr uint16_t PUSH_ALARM=1, PUSH_ARMED=2, PUSH_BATTERY=4, PUSH_MASK=0x7ff;
inline bool commandAllowed(uint32_t now,uint32_t created,bool fresh,bool bridge,bool paused,bool busy,uint32_t ended,bool hasPulsed){
 return uint32_t(now-created)<=1500&&fresh&&!bridge&&!paused&&!busy&&(!hasPulsed||uint32_t(now-ended)>=2000);
}
inline bool knownArming(uint8_t s){return s==0||s==1||s==5;}
inline bool armed(uint8_t s){return s==5;}
inline bool validCommandValue(int8_t v){return v==0||v==1;}
inline bool rising(uint16_t before,uint16_t after,uint8_t bit){return !(before&(1u<<bit))&&(after&(1u<<bit));}
inline bool selfTest(){
 return commandAllowed(10000,9000,true,false,false,false,8000,true)
 &&!commandAllowed(9999,9000,true,false,false,false,8000,true)
 &&commandAllowed(10000,8500,true,false,false,false,8000,true)
 &&!commandAllowed(10000,8499,true,false,false,false,8000,true)
 &&!commandAllowed(10000,9000,false,false,false,false,8000,true)
 &&!commandAllowed(10000,9000,true,true,false,false,8000,true)
 &&!commandAllowed(10000,9000,true,false,true,false,8000,true)
 &&!commandAllowed(10000,9000,true,false,false,true,8000,true)
 &&commandAllowed(20,0xfffffff0,true,false,false,false,0xfffff000,true)
 &&rising(0,1u<<8,8)&&!rising(1u<<8,1u<<8,8)&&!rising(1u<<8,0,8)
 &&armed(5)&&!armed(1)&&!knownArming(255)
 &&validCommandValue(0)&&validCommandValue(1)&&!validCommandValue(2)&&!validCommandValue(-1);
}
}
