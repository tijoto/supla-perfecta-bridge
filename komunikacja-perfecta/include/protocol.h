#pragma once
#include <Arduino.h>
#include <mbedtls/aes.h>
namespace Perfecta {
constexpr size_t MAX_PAYLOAD=8192, MAX_FRAME=MAX_PAYLOAD+14;
inline uint32_t crc(const uint8_t* p,size_t n,uint32_t v=0xffffffff){
 while(n--){v^=uint32_t(*p++)<<24;for(int i=0;i<8;i++)v=(v<<1)^((v&0x80000000)?0x04c11db7:0);}
 return v;
}
inline uint32_t be32(const uint8_t* p){return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];}
inline uint32_t le32(const uint8_t* p){return uint32_t(p[3])<<24|uint32_t(p[2])<<16|uint32_t(p[1])<<8|p[0];}
inline bool crypt(uint8_t* data,size_t n,const uint8_t key[16],bool decrypt){
 mbedtls_aes_context enc,dec;mbedtls_aes_init(&enc);mbedtls_aes_init(&dec);
 int rc=mbedtls_aes_setkey_enc(&enc,key,128)|mbedtls_aes_setkey_dec(&dec,key,128);
 uint8_t previous[16]={},block[16],out[16];
 rc|=mbedtls_aes_crypt_ecb(&enc,MBEDTLS_AES_ENCRYPT,previous,previous);
 size_t whole=n/16*16;
 for(size_t pos=0;pos<whole;pos+=16){
  memcpy(block,data+pos,16);
  if(decrypt){rc|=mbedtls_aes_crypt_ecb(&dec,MBEDTLS_AES_DECRYPT,block,out);for(int j=0;j<16;j++)data[pos+j]=out[j]^previous[j];memcpy(previous,block,16);}
  else{for(int j=0;j<16;j++)block[j]^=previous[j];rc|=mbedtls_aes_crypt_ecb(&enc,MBEDTLS_AES_ENCRYPT,block,previous);memcpy(data+pos,previous,16);}
 }
 if(whole<n){rc|=mbedtls_aes_crypt_ecb(&enc,MBEDTLS_AES_ENCRYPT,previous,out);for(size_t j=0;j<n-whole;j++)data[whole+j]^=out[j];}
 mbedtls_aes_free(&enc);mbedtls_aes_free(&dec);return rc==0;
}
inline size_t encode(const uint8_t* payload,size_t n,const uint8_t key[16],const uint8_t nonce[4],uint8_t* out,size_t cap){
 size_t padded=max(n,size_t(8)),total=padded+14;if(!n||n>MAX_PAYLOAD||cap<total)return 0;
 memset(out,255,4);out[4]=n>>8;out[5]=n;memcpy(out+6,nonce,4);memcpy(out+10,payload,n);memset(out+10+n,0,padded-n);
 uint32_t c=crc(out+4,total-8);for(int i=0;i<4;i++)out[total-4+i]=c>>(24-8*i);
 return crypt(out+6,total-6,key,false)?total:0;
}
struct Decoder{
 uint8_t clear[MAX_PAYLOAD+8];
 size_t decode(const uint8_t* raw,size_t n,const uint8_t key[16]){
  if(n<22||memcmp(raw,"\xff\xff\xff\xff",4))return 0;
  size_t count=(raw[4]<<8)|raw[5];if(count>MAX_PAYLOAD||n!=max(count,size_t(8))+14)return 0;
  memcpy(clear,raw+6,n-6);if(!crypt(clear,n-6,key,true))return 0;
  uint32_t c=crc(raw+4,2);c=crc(clear,n-10,c);if(c!=be32(clear+n-10))return 0;
  return count;
 }
 uint8_t* payload(){return clear+4;}
};
struct Parser{
 uint8_t data[MAX_FRAME];size_t used=0;uint32_t discarded=0;
 void reset(){used=0;}
 template<class Callback> void feed(uint8_t byte,Callback onFrame){
  if(used>=sizeof(data)){used=0;discarded++;}
  data[used++]=byte;
  while(used>=4){
   if(memcmp(data,"\xff\xff\xff\xff",4)){memmove(data,data+1,--used);discarded++;continue;}
   if(used>=5&&data[4]==255){memmove(data,data+1,--used);discarded++;continue;}
   if(used<6)return;
   size_t count=(data[4]<<8)|data[5];if(count>MAX_PAYLOAD){memmove(data,data+1,--used);discarded++;continue;}
   size_t total=max(count,size_t(8))+14;if(used<total)return;
   onFrame(data,total);used-=total;if(used)memmove(data,data+total,used);
  }
 }
};
}
