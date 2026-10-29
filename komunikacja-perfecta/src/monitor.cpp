#include "app.h"
#include "private_config.h"
#include "test_vectors.h"
#include "z7_policy.h"
static Perfecta::Parser rxParser;
static Perfecta::Decoder decoder;
static uint8_t sessionKey[16]={};
static bool haveKey=false,identified=false,waiting=false,bridgePending=false;
static uint8_t phase=0,initialAttempt=0,retry=0,kind=0,pending[6]={};
static size_t pendingLength=0;
static uint32_t sentAt=0,nextInit=0,nextStatus=0,nextDiag=0,nextRf=0,nextParam=0,nextZ7=0,rfDeadline=0,rfPollAt=0,bridgeClosed=0;
static uint8_t rfStep=0,paramIndex=0;
static uint16_t previousMask=0,previousBattery=0;
static uint8_t previousAlarm[2]={},previousSource[2]={};
struct Region{uint16_t start,length;};
static const Region regions[]={{0x1e0,32},{0x400,128},{0x480,128},{0x600,128},{0x680,128},{0x1400,128},{0x1500,64},{0x15e0,16},{0x1640,32},{0x1670,8},{0x1678,8}};
enum RequestKind:uint8_t{REQ_INIT,REQ_IDENTITY,REQ_STATUS,REQ_DIAG,REQ_PARAM,REQ_Z7_PARAM,REQ_RF_CLEAR,REQ_RF_START,REQ_RF_POLL,REQ_CFG_READ,REQ_CFG_BEGIN,REQ_CFG_PATCH,REQ_CFG_COMMIT};
enum Z7Phase:uint8_t{Z_IDLE,Z_CHECK,Z_BACKUP,Z_RECHECK_TYPE,Z_RECHECK_MASK,Z_PRECHECK,Z_BEGIN,Z_PATCH,Z_COMMITCHECK,Z_COMMIT,Z_VERIFY,Z_RECOVER};
static Z7Phase z7Phase=Z_IDLE;
static uint8_t* z7Before=nullptr;
static uint32_t z7At=0,z7Duration=0,z7ExpectedRevision=0,z7Id=0,z7MaskAt=0,z7WriteRequests=0;
static uint16_t z7Offset=0,z7ReadOffset=0;
static uint8_t z7After=0,z7After2=0,z7Number=7,z7PatchIndex=0;
static bool z7Desired2=true,z7SensorJob=false;
static bool z7Seen=false,z7Desired=false,z7Sent=false,z7Committed=false,z7SameBefore=true,z7SameAfter=true,z7PrepareOnly=false;
static bool z7Quick=false;
static String z7Status="idle",z7Message;
struct Z7Journal {uint32_t magic,id,backupCrc;uint8_t pending,after,number,after2;uint32_t crc;};
static size_t z7ReadTotal(){return z7Quick?Z7Policy::QUICK_SIZE:Z7Policy::BANK;}
static uint32_t z7SnapshotCrc(){
 if(!z7Quick)return Perfecta::crc(z7Before,Z7Policy::BANK);
 uint32_t crc=Perfecta::crc(z7Before+0x1400,Z7Policy::QUICK_BLOCK);
 return Perfecta::crc(z7Before+0x1600,Z7Policy::QUICK_BLOCK,crc);
}
static bool z7LoadSnapshot(){
 if(!z7Quick)return z7Backup(z7Before,Z7Policy::BANK);
 uint8_t snapshot[Z7Policy::QUICK_SIZE];
 if(prefs.getBytes("z7quick",snapshot,sizeof(snapshot))!=sizeof(snapshot))return false;
 Z7Policy::unpackQuick(snapshot,z7Before);return true;
}
static bool saveZ7Journal(bool pending){
 Z7Journal record={};record.magic=z7Quick?0x5a375033:0x5a375032;record.id=z7Id;record.pending=pending;record.after=z7After;record.number=z7Number;record.after2=z7After2;
 record.backupCrc=z7Before?z7SnapshotCrc():0;
 record.crc=Perfecta::crc(reinterpret_cast<const uint8_t*>(&record),offsetof(Z7Journal,crc));
 return prefs.putBytes("z7job",&record,sizeof(record))==sizeof(record);
}
bool z7Busy(){return z7Phase!=Z_IDLE;}
static bool z7SafeStatus(bool sensorJob=false){
 return app.selfTest&&app.ready&&app.hasStatus&&uint32_t(millis()-app.statusAt)<2000&&
  app.status[52]==0&&app.status[53]==0&&app.status[54]==0&&app.status[55]==0&&
  (sensorJob||!(Perfecta::le32(app.status+34)&Z7Policy::MASK))&&
  !app.bridge&&!bridgePending&&!bridgeClient.connected()&&!app.monitorPaused&&!activeOutput&&
  (!pulseCount||uint32_t(millis()-pulseEndedAt)>=2000);
}
static void z7Finish(const char* status,const String& message){
 z7Duration=millis()-z7At;z7Status=status;z7Message=message;z7Phase=Z_IDLE;app.mode="reader";
 if(z7Before){if(!saveZ7Journal(!strcmp(status,"uncertain")))z7Message+="; nie zapisano wyniku w NVS";free(z7Before);z7Before=nullptr;}
 nextStatus=0;nextRf=millis()+5000;rfStep=0;
}
static void z7Failure(const String& message){
 z7Message=message;
 if(z7Phase==Z_RECOVER){z7Finish("uncertain",message+"; nie ponowiono zapisu ani COMMIT");return;}
 if(z7Sent){z7Phase=Z_RECOVER;z7Status="recovering";z7Offset=0;z7SameBefore=z7SameAfter=true;z7At=millis();}
 else z7Finish("error",message);
}
String z7Json(){
 bool fresh=z7Seen&&uint32_t(millis()-z7MaskAt)<45000;
 String s="{\"ready\":"+String(fresh&&app.namesReady?"true":"false")+",\"supported\":"+String(fresh&&Z7Policy::supported(app.config)?"true":"false");
 s+=",\"self_test\":"+String(app.selfTest?"true":"false")+",\"partition1\":"+String(app.config[0x1670]&0x40?"true":"false");
 s+=",\"partition2\":"+String(app.config[0x1674]&0x40?"true":"false")+",\"disarm_partition1\":"+String(app.config[0x1670]&0x80?"true":"false");
 s+=",\"revision\":"+String(Z7Policy::revision(app.config))+",\"can_write\":"+String(CONFIG_WRITES_ENABLED&&fresh&&app.namesReady&&Z7Policy::supported(app.config)&&z7SafeStatus()&&!z7Busy()?"true":"false");
 s+=",\"busy\":"+String(z7Busy()?"true":"false")+",\"id\":"+String(z7Id)+",\"status\":"+jquote(z7Status)+",\"message\":"+jquote(z7Message);
 s+=",\"write_attempted\":"+String(z7Sent?"true":"false")+",\"commit_sent\":"+String(z7Committed?"true":"false");
 s+=",\"write_requests\":"+String(z7WriteRequests)+",\"nvs_free_entries\":"+String(prefs.freeEntries())+",\"input_number\":"+String(z7Number);
 s+=",\"read_bytes\":"+String(z7Offset)+",\"total_bytes\":"+String(z7ReadTotal())+",\"scope\":"+jquote(z7Quick?"z7-blocks":"full");
 s+=",\"duration_ms\":"+String(z7Busy()?millis()-z7At:z7Duration)+",\"assignment_poll_ms\":2000";
 s+=",\"reaction\":"+String(app.config[0x1446])+",\"connection_type\":"+String(app.config[0x1406])+"}";return s;
}
String inputPartitionJson(uint8_t number){
 if(number<1||number>16)return "";
 bool fresh=z7Seen&&app.namesReady&&uint32_t(millis()-z7MaskAt)<45000;
 uint8_t mask=Z7Policy::mask(number);
 String s=",\"partition1\":"+String(app.config[Z7Policy::offset(number,0)]&mask?"true":"false");
 s+=",\"partition2\":"+String(app.config[Z7Policy::offset(number,1)]&mask?"true":"false");
 s+=",\"partition_settings_ready\":"+String(fresh?"true":"false")+",\"partition_revision\":"+String(Z7Policy::inputRevision(app.config,number));
 s+=",\"can_assign\":"+String(CONFIG_WRITES_ENABLED&&fresh&&Z7Policy::sensor(app.config,number)&&z7SafeStatus(true)&&!z7Busy()?"true":"false");
 s+=",\"bypass_supported\":false";return s;
}
static bool startInputJob(uint8_t number,bool p1,bool p2,uint32_t revision,String& error,bool prepareOnly,bool sensorJob,bool quickPrepare=false){
 z7Before=static_cast<uint8_t*>(malloc(Z7Policy::BANK));
 if(!z7Before){error="Brak pamieci na kopie konfiguracji";return false;}
 memset(z7Before,0,Z7Policy::BANK);
 z7Number=number;z7SensorJob=sensorJob;z7Desired=p1;z7Desired2=p2;z7ExpectedRevision=revision;z7Offset=0;z7PatchIndex=0;
 z7Sent=z7Committed=false;z7SameBefore=z7SameAfter=true;z7PrepareOnly=prepareOnly;z7Quick=number==7&&(!prepareOnly||quickPrepare);
 esp_fill_random(&z7Id,sizeof(z7Id));if(!z7Id)z7Id=1;
 z7At=millis();z7Phase=Z_CHECK;z7Status="checking";z7Message="Kopia i kontrola konfiguracji przed zmiana przypisania wejscia";app.mode="configuration";rfStep=0;
 return true;
}
bool requestInputPartitions(uint8_t number,bool p1,bool p2,uint32_t revision,String& error){
 if(!CONFIG_WRITES_ENABLED){error="Zapis ustawien jest wylaczony w konfiguracji urzadzenia";return false;}
 if(number<1||number>16){error="Nieprawidlowy numer czujki";return false;}
 if(z7Busy()){error="Trwa zapis lub odczyt kontrolny konfiguracji";return false;}
 // The same complete disarmed-state checks apply to sensor assignments.
 if(!z7SafeStatus(true)){error="Zmiana czujki wymaga rozbrojenia obu stref, skasowania alarmu i wolnego portu";return false;}
 if(!z7Seen||!app.namesReady||uint32_t(millis()-z7MaskAt)>=45000){error="Poczekaj na aktualne przypisania czujek";return false;}
 if(!Z7Policy::sensor(app.config,number)){error="Ta operacja dotyczy skonfigurowanych czujek, bez wejsc sterujacych";return false;}
 if(revision!=Z7Policy::inputRevision(app.config,number)){error="Przypisania zmienily sie; odswiez dane";return false;}
 uint8_t bit=Z7Policy::mask(number);
 if(bool(app.config[Z7Policy::offset(number,0)]&bit)==p1&&bool(app.config[Z7Policy::offset(number,1)]&bit)==p2){z7Status="unchanged";z7Message="Przypisanie pozostalo bez zmian";return true;}
 return startInputJob(number,p1,p2,revision,error,false,true);
}
bool requestZ7Partition1(bool enabled,uint32_t revision,String& error,bool prepareOnly,bool quickPrepare){
 if(!prepareOnly&&!CONFIG_WRITES_ENABLED){error="Zapis ustawien jest wylaczony w konfiguracji urzadzenia";return false;}
 if(z7Busy()){error="Trwa zapis lub odczyt kontrolny konfiguracji";return false;}
 if(!z7SafeStatus()){error="Zapis wymaga rozbrojenia obu stref, skasowania alarmu, nienaruszonego Z7 i wolnego portu";return false;}
 if(!z7Seen||!app.namesReady||uint32_t(millis()-z7MaskAt)>=45000){error="Poczekaj na aktualny odczyt przypisania Z7";return false;}
 if(!Z7Policy::supported(app.config)){error="Z7 musi miec obwod NO, reakcje 14 i przypisanie do strefy 2";return false;}
 if(revision!=Z7Policy::revision(app.config)){error="Przypisanie zmienilo sie; odswiez dane";return false;}
 if(!prepareOnly&&bool(app.config[Z7Policy::OFFSET]&Z7Policy::MASK)==enabled){z7Status="unchanged";z7Message="Z7 ma juz wybrane przypisanie";return true;}
 return startInputJob(7,enabled,true,revision,error,prepareOnly,false,quickPrepare);
}
bool z7Backup(uint8_t* output,size_t length){return length==Z7Policy::BANK&&prefs.getBytes("z7backup",output,length)==length;}
static void z7Setup(){
 Z7Journal record={};
 if(prefs.getBytes("z7job",&record,sizeof(record))!=sizeof(record)||(record.magic!=0x5a375031&&record.magic!=0x5a375032&&record.magic!=0x5a375033)||
  record.crc!=Perfecta::crc(reinterpret_cast<const uint8_t*>(&record),offsetof(Z7Journal,crc)))return;
 z7Id=record.id;
 if(!record.pending)return;
 z7Quick=record.magic==0x5a375033;
 z7Before=static_cast<uint8_t*>(malloc(Z7Policy::BANK));
 if(z7Before)memset(z7Before,0,Z7Policy::BANK);
 if(!z7Before||!z7LoadSnapshot()||z7SnapshotCrc()!=record.backupCrc){
  if(z7Before){free(z7Before);z7Before=nullptr;}z7Status="uncertain";z7Message="Poprzedni zapis przerwany; brak poprawnej kopii kontrolnej";return;
 }
 z7Number=record.magic==0x5a375031?7:record.number;
 if(z7Number<1||z7Number>16||(z7Quick&&z7Number!=7)){z7Finish("uncertain","Nieprawidlowy numer w dzienniku; zadne komendy zapisu nie zostaly wyslane");return;}
 z7After=record.after;z7After2=record.magic==0x5a375031?z7Before[Z7Policy::offset(7,1)]:record.after2;
 z7SensorJob=z7Number!=7;z7Sent=true;z7Committed=true;z7Offset=0;z7SameBefore=z7SameAfter=true;
 z7Phase=Z_RECOVER;z7Status="recovering";z7Message="Odczyt po przerwanym zapisie; komendy zapisu nie beda powtarzane";
 z7At=millis();
}
static void z7ReadReceived(const uint8_t* p,size_t n){
 processPayload(p,n);size_t size=n-4;
 if(z7Phase==Z_CHECK){
  memcpy(z7Before+z7ReadOffset,p+4,size);z7Offset+=size;
  if(z7Offset==z7ReadTotal()){
   if(!(z7SensorJob?Z7Policy::sensor(z7Before,z7Number):Z7Policy::supported(z7Before))||Z7Policy::inputRevision(z7Before,z7Number)!=z7ExpectedRevision){z7Finish("conflict","Ustawienia zmienily sie od podgladu; niczego nie zapisano");return;}
   z7After=Z7Policy::changedInput(z7Before[Z7Policy::offset(z7Number,0)],z7Number,z7Desired);
   z7After2=Z7Policy::changedInput(z7Before[Z7Policy::offset(z7Number,1)],z7Number,z7Desired2);
   z7Phase=Z_BACKUP;z7Status="backup";z7Message="Zapis kopii konfiguracji przed zmiana";
  }
 }else if(z7Phase==Z_RECHECK_TYPE||z7Phase==Z_RECHECK_MASK){
  if(memcmp(z7Before+z7ReadOffset,p+4,size)){z7Finish("conflict","Konfiguracja zmienila sie w trakcie odczytu; niczego nie zapisano");return;}
  z7Phase=z7Phase==Z_RECHECK_TYPE?Z_RECHECK_MASK:Z_PRECHECK;
 }else if(z7Phase==Z_VERIFY||z7Phase==Z_RECOVER){
  for(size_t i=0;i<size;i++){
   size_t offset=z7ReadOffset+i;uint8_t before=z7Before[offset],after=offset==Z7Policy::offset(z7Number,0)?z7After:offset==Z7Policy::offset(z7Number,1)?z7After2:before;
   z7SameBefore&=p[4+i]==before;z7SameAfter&=p[4+i]==after;
  }
  z7Offset+=size;
  if(z7Offset==z7ReadTotal()){
   if(z7SameAfter&&z7PrepareOnly)z7Finish("prepared","Kopia i kontrolny odczyt blokow Z7 potwierdzone; bez komend zapisu");
   else if(z7SameAfter)z7Finish("success",z7Quick?"Odczyt obu blokow Z7 potwierdzil zmiane i zachowanie pozostalych bitow w tych blokach":"Odczyt calej konfiguracji potwierdzil tylko wybrane przypisania wejscia");
   else if(z7SameBefore)z7Finish("not_applied","Aktywna konfiguracja pozostala bez zmian; nie ponowiono zapisu");
   else z7Finish("uncertain","Odczyt konfiguracji wymaga sprawdzenia; nie ponowiono zapisu");
  }
 }
}
static bool validClock(const uint8_t* p){
 for(int i=1;i<=6;i++)if((p[i]>>4)>9||(p[i]&15)>9)return false;
 return p[2]>=1&&p[2]<=0x12&&p[3]>=1&&p[3]<=0x31&&p[4]<=0x23&&p[5]<=0x59&&p[6]<=0x59;
}
void processPayload(const uint8_t* p,size_t n){
 if(n==61&&p[0]==6&&validClock(p)){
  memcpy(app.status,p,n);app.hasStatus=true;app.statusAt=millis();app.samples++;
  uint16_t mask=Perfecta::le32(p+34);uint32_t epoch=panelEpoch();
  for(int i=0;i<16;i++)if((mask&~previousMask)&(1<<i)){if(epoch)app.history.violation[i]=epoch;app.historyDirty=true;}
  previousMask=mask;
  for(int z=0;z<2;z++){
   uint8_t alarm=p[54+z],source=p[56+z];
   if(alarm==0x44&&source>=1&&source<=16&&(previousAlarm[z]!=0x44||previousSource[z]!=source)){if(epoch)app.history.violation[source-1]=epoch;app.historyDirty=true;}
   previousAlarm[z]=alarm;previousSource[z]=source;
  }
 }else if(n==137&&p[0]==0x0c){
  memcpy(app.diag,p,n);app.hasDiag=true;app.diagAt=millis();uint16_t low=Perfecta::le32(p+58);uint32_t epoch=panelEpoch();
  for(int i=0;i<16;i++)if((low&~previousBattery)&(1<<i)){if(epoch)app.history.battery[i]=epoch;app.historyDirty=true;}
  previousBattery=low;
 }else if(n==44&&!memcmp(p,"\xfc\x06\x29\xc2",4)){
  for(size_t i=4;i<n;i++)if(p[i]>100)return;
  memcpy(app.rf,p,n);app.hasRf=true;app.rfAt=millis();
 }else if(n>4&&p[0]==1){
  uint32_t offset=uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];
  if(offset+n-4<=sizeof(app.config))memcpy(app.config+offset,p+4,n-4);
  if(offset<=0x1670&&offset+n-4>=0x1678){z7Seen=true;z7MaskAt=millis();}
 }
}
static bool identityOk(const uint8_t* p,size_t n){return n==65&&p[0]==0&&p[1]==4&&p[2]==0x93&&!memcmp(p+3,"1.05",4);}
static void transmit(){
 uint8_t raw[64],nonce[4];esp_fill_random(nonce,4);uint8_t initial[16]={};
 if(kind==REQ_INIT&&initialAttempt>=3)memset(initial,' ',16);
 const uint8_t* key=kind==REQ_INIT?initial:sessionKey;
 size_t n=Perfecta::encode(pending,pendingLength,key,nonce,raw,sizeof(raw));
 if(n){if(kind==REQ_CFG_BEGIN||kind==REQ_CFG_PATCH||kind==REQ_CFG_COMMIT)z7WriteRequests++;panel.write(raw,n);app.txBytes+=n;sentAt=millis();waiting=true;}
}
static void request(RequestKind k,const uint8_t* p,size_t n){
 kind=k;pendingLength=n;memcpy(pending,p,n);retry=0;transmit();
}
static void single(RequestKind k,uint8_t cmd){request(k,&cmd,1);}
static void cooldown(const char* error){
 if(z7Busy())z7Failure(error);
 waiting=false;identified=haveKey=false;app.ready=false;phase=0;initialAttempt=0;nextInit=millis()+30000;rfStep=0;app.error=error;rxParser.reset();
}
static bool matches(const uint8_t* p,size_t n){
 switch(kind){
  case REQ_INIT:return n==22&&!memcmp(p,"\x00""ERROR",6);
  case REQ_IDENTITY:return identityOk(p,n);
  case REQ_STATUS:return n==61&&p[0]==6&&validClock(p);
  case REQ_DIAG:return n==137&&p[0]==12;
  case REQ_PARAM:return n==size_t(regions[paramIndex].length)+4&&p[0]==1&&p[1]==0&&p[2]==(regions[paramIndex].start>>8)&&p[3]==(regions[paramIndex].start&255);
  case REQ_Z7_PARAM:return n==12&&p[0]==1&&p[1]==0&&p[2]==0x16&&p[3]==0x70;
  case REQ_RF_CLEAR:case REQ_RF_POLL:return n>=1&&p[0]==0xfc;
  case REQ_RF_START:return n==1&&p[0]==0xfc;
  case REQ_CFG_READ:return n==132&&p[0]==1&&p[1]==0&&p[2]==(z7ReadOffset>>8)&&p[3]==(z7ReadOffset&255);
  case REQ_CFG_BEGIN:return n==1&&p[0]==2;
  case REQ_CFG_PATCH:{size_t address=Z7Policy::offset(z7Number,z7PatchIndex);return n==4&&p[0]==3&&p[1]==0&&p[2]==(address>>8)&&p[3]==(address&255);}
  case REQ_CFG_COMMIT:return n==1&&p[0]==4;
 }return false;
}
static void received(const uint8_t* raw,size_t size){
 if(app.bridge){
  size_t n=decoder.decode(raw,size,reinterpret_cast<const uint8_t*>(PANEL_KEY));
  if(n==22&&!memcmp(decoder.payload(),"\x00""ERROR",6)){memcpy(sessionKey,decoder.payload()+6,16);haveKey=true;identified=false;return;}
  if(!haveKey)return;
  n=decoder.decode(raw,size,sessionKey);if(!n){app.crcErrors++;return;}
  if(identityOk(decoder.payload(),n)){identified=true;app.ready=true;app.sessionCount++;}
  if(identified)processPayload(decoder.payload(),n);
  return;
 }
 if(!waiting)return;
 size_t n=decoder.decode(raw,size,kind==REQ_INIT?reinterpret_cast<const uint8_t*>(PANEL_KEY):sessionKey);
 if(!n){app.crcErrors++;return;}uint8_t* p=decoder.payload();
 if(!matches(p,n))return;
 waiting=false;
 switch(kind){
  case REQ_INIT:memcpy(sessionKey,p+6,16);haveKey=true;phase=1;break;
  case REQ_IDENTITY:identified=app.ready=true;app.sessionCount++;phase=2;app.error="";nextStatus=nextDiag=nextParam=nextRf=nextZ7=0;paramIndex=0;break;
  case REQ_STATUS:
   processPayload(p,n);nextStatus=millis()+200;app.error="";
   if(z7Phase==Z_PRECHECK||z7Phase==Z_COMMITCHECK){
    if(!z7SafeStatus(z7SensorJob))z7Failure("Centrala nie jest rozbrojona lub port jest zajety; przerwano zapis");
    else if(z7Phase==Z_PRECHECK&&z7PrepareOnly){z7Phase=Z_VERIFY;z7Offset=0;z7Status="verifying";}
    else z7Phase=z7Phase==Z_PRECHECK?Z_BEGIN:Z_COMMIT;
   }
   break;
  case REQ_DIAG:processPayload(p,n);nextDiag=millis()+5000;break;
  case REQ_PARAM:processPayload(p,n);paramIndex++;if(paramIndex>=sizeof(regions)/sizeof(regions[0])){app.namesReady=true;paramIndex=0;nextParam=millis()+30000;}else nextParam=millis()+30;break;
  case REQ_Z7_PARAM:processPayload(p,n);nextZ7=millis()+2000;break;
  case REQ_RF_CLEAR:rfStep=2;break;
  case REQ_RF_START:rfStep=3;rfDeadline=millis()+1500;rfPollAt=millis()+100;break;
  case REQ_RF_POLL:if(n==44&&!memcmp(p,"\xfc\x06\x29\xc2",4)){processPayload(p,n);rfStep=0;nextRf=millis()+5000;}else rfPollAt=millis()+100;break;
  case REQ_CFG_READ:z7ReadReceived(p,n);break;
  case REQ_CFG_BEGIN:z7Phase=Z_PATCH;z7Status="writing";z7Message="Zapis jednego bajtu z zachowaniem pozostalych bitow";break;
  case REQ_CFG_PATCH:z7PatchIndex++;z7Phase=Z_PATCH;break;
  case REQ_CFG_COMMIT:z7Phase=Z_VERIFY;z7Offset=0;z7SameBefore=z7SameAfter=true;z7Status="verifying";z7Message="Kontrolny odczyt calej konfiguracji po COMMIT";break;
 }
}
static void finishBridge(){
 bridgeClient.stop();app.bridge=false;bridgePending=false;waiting=false;haveKey=identified=app.ready=false;
 app.mode=app.monitorPaused?"paused":"reader";phase=0;initialAttempt=0;nextInit=millis()+1000;bridgeClosed=millis();rxParser.reset();
 Serial.println("BRIDGE closed; independent reader resumes");
}
static bool protocolSelfTest(){
 uint8_t frame[160];
 const uint8_t* payloads[]={TEST_PAYLOAD0,TEST_PAYLOAD1,TEST_PAYLOAD2};
 const uint8_t* frames[]={TEST_FRAME0,TEST_FRAME1,TEST_FRAME2};
 const size_t lengths[]={sizeof(TEST_PAYLOAD0),sizeof(TEST_PAYLOAD1),sizeof(TEST_PAYLOAD2)};
 const size_t frameLengths[]={sizeof(TEST_FRAME0),sizeof(TEST_FRAME1),sizeof(TEST_FRAME2)};
 for(int i=0;i<3;i++){
  size_t n=Perfecta::encode(payloads[i],lengths[i],TEST_KEY,TEST_NONCE,frame,sizeof(frame));
  if(n!=frameLengths[i]||memcmp(frame,frames[i],n))return false;
  if(decoder.decode(frame,n,TEST_KEY)!=lengths[i]||memcmp(decoder.payload(),payloads[i],lengths[i]))return false;
  frame[n-1]^=1;if(decoder.decode(frame,n,TEST_KEY))return false;
 }
 Perfecta::Parser* parser=new Perfecta::Parser;int found=0;
 auto cb=[&](const uint8_t*,size_t){found++;};
 parser->feed(3,cb);parser->feed(255,cb);
 for(uint8_t b:TEST_FRAME0)parser->feed(b,cb);
 for(uint8_t b:TEST_FRAME1)parser->feed(b,cb);
 bool ok=found==2;delete parser;return ok;
}
void setupPanel(){
 app.selfTest=protocolSelfTest();Serial.printf("PROTOCOL SELFTEST: %s\n",app.selfTest?"PASS":"FAIL");
 app.selfTest&=Z7Policy::selfTest();z7Setup();
 panel.setRxBufferSize(8192);panel.begin(19200,SERIAL_8N1,RX_PIN,TX_PIN);
 nextInit=millis()+1000;if(!app.selfTest){app.monitorPaused=true;app.error="Protocol self-test failed";}
}
void servicePanel(){
 uint32_t now=millis();
 if(bridgeServer.hasClient()){
  WiFiClient candidate=bridgeServer.available();IPAddress allowed;allowed.fromString(BRIDGE_IP);
  if(z7Busy()||bridgePending||app.bridge||!BRIDGE_ENABLED||candidate.remoteIP()!=allowed){app.rejectedClients++;candidate.stop();}
  else{bridgeClient=candidate;bridgeClient.setNoDelay(true);bridgeClient.setTimeout(100);bridgePending=true;app.bridgeAt=now;app.mode="handoff";}
 }
 if(app.bridge){
  if(!bridgeClient.connected()||WiFi.status()!=WL_CONNECTED){finishBridge();return;}
  uint8_t data[256];size_t n=0;while(panel.available()&&n<sizeof(data))data[n++]=panel.read();
  if(n){app.rxBytes+=n;app.bridgeRx+=n;if(bridgeClient.write(data,n)!=n){app.error="Bridge TCP write failed";finishBridge();return;}for(size_t i=0;i<n;i++)rxParser.feed(data[i],received);}
  n=0;while(bridgeClient.available()&&n<sizeof(data))data[n++]=bridgeClient.read();
  if(n){panel.write(data,n);app.txBytes+=n;app.bridgeTx+=n;}
  return;
 }
 // Drain outstanding monitor response before handing the physical UART to Soft.
 size_t drained=0;while(panel.available()&&drained++<8192){app.rxBytes++;rxParser.feed(panel.read(),received);}
 if(bridgePending&&(!waiting||uint32_t(now-sentAt)>3000)){
  waiting=false;app.bridge=true;bridgePending=false;app.mode="perfecta_soft";app.ready=false;haveKey=identified=false;rxParser.reset();
  Serial.println("BRIDGE active; reader suspended");return;
 }
 if(bridgePending)return;
 if(waiting){
  if(uint32_t(now-sentAt)<=3000)return;
  app.timeouts++;waiting=false;rxParser.reset();
  if(kind==REQ_CFG_BEGIN||kind==REQ_CFG_PATCH||kind==REQ_CFG_COMMIT){z7Failure("Brak potwierdzenia zapisu; nie ponowiono komendy ani COMMIT");return;}
  if(kind==REQ_CFG_READ){if(retry++==0){transmit();return;}z7Failure("Brak odpowiedzi podczas kontrolnego odczytu konfiguracji");return;}
  if(kind==REQ_INIT){initialAttempt++;if(initialAttempt>=5){cooldown("Brak odpowiedzi UART - sprawdz polaczenie");return;}transmit();return;}
  if(retry++==0){transmit();return;}
  if(kind>=REQ_RF_CLEAR){rfStep=0;nextRf=now+5000;app.error="Brak aktualnego odczytu RF";return;}
  if(kind==REQ_PARAM||kind==REQ_DIAG||kind==REQ_Z7_PARAM){if(kind==REQ_PARAM)nextParam=now+30000;else if(kind==REQ_Z7_PARAM)nextZ7=now+2000;else nextDiag=now+5000;return;}
  cooldown("Brak odpowiedzi statusu; ponowne polaczenie za 30s");return;
 }
 if(app.monitorPaused||!app.selfTest)return;
 if(!app.ready){
  if(int32_t(now-nextInit)<0)return;
  if(phase==0)single(REQ_INIT,0);else single(REQ_IDENTITY,0);return;
 }
 if(z7Busy()&&uint32_t(now-z7At)>120000){z7Failure("Przekroczono czas operacji konfiguracji");return;}
 if(int32_t(now-nextStatus)>=0){single(REQ_STATUS,6);return;}
 if(z7Busy()){
  if(z7Phase==Z_CHECK||z7Phase==Z_VERIFY||z7Phase==Z_RECOVER||z7Phase==Z_RECHECK_TYPE||z7Phase==Z_RECHECK_MASK){
   z7ReadOffset=z7Phase==Z_RECHECK_TYPE?0x1400:z7Phase==Z_RECHECK_MASK?0x1600:z7Quick?Z7Policy::quickAddress(z7Offset):z7Offset;
   const uint8_t payload[]={1,0,uint8_t(z7ReadOffset>>8),uint8_t(z7ReadOffset),0,128};request(REQ_CFG_READ,payload,sizeof(payload));
  }else if(z7Phase==Z_BACKUP){
   uint32_t crc=z7SnapshotCrc();
   // NVS cannot hold two generations of a 7 KiB blob atomically in this 20 KiB
   // partition. Retire only our previous backup before allocating the next one.
   // The panel is still untouched; BEGIN is unreachable until durable readback.
   if(z7Quick){
    uint8_t snapshot[Z7Policy::QUICK_SIZE];Z7Policy::packQuick(z7Before,snapshot);
    if(prefs.putBytes("z7quick",snapshot,sizeof(snapshot))!=sizeof(snapshot)){z7Finish("error","Nie zapisano kopii blokow Z7; centrala bez zmian");return;}
   }else{
    if(prefs.isKey("z7backup")&&!prefs.remove("z7backup")){z7Finish("error","Nie mozna zwolnic poprzedniej kopii Z7; centrala bez zmian");return;}
    if(prefs.putBytes("z7backup",z7Before,Z7Policy::BANK)!=Z7Policy::BANK){z7Finish("error","Brak miejsca na nowa kopie NVS; centrala bez zmian");return;}
   }
   if(!z7LoadSnapshot()||z7SnapshotCrc()!=crc){z7Finish("error","Odczyt kopii NVS nie przeszedl kontroli CRC; centrala bez zmian");return;}
   if(!saveZ7Journal(!z7PrepareOnly)){z7Finish("error","Nie zapisano dziennika transakcji; centrala bez zmian");return;}
   if(z7PrepareOnly&&!z7Quick){z7Finish("prepared","Pelny odczyt i kopia NVS potwierdzone; nie wyslano BEGIN, PATCH ani COMMIT");return;}
   z7Phase=Z_RECHECK_TYPE;
  }else if(z7Phase==Z_PRECHECK||z7Phase==Z_COMMITCHECK){single(REQ_STATUS,6);}
  else if(z7Phase==Z_BEGIN){z7Sent=true;single(REQ_CFG_BEGIN,2);}
  else if(z7Phase==Z_PATCH){
   while(z7PatchIndex<2&&z7Before[Z7Policy::offset(z7Number,z7PatchIndex)]==(z7PatchIndex?z7After2:z7After))z7PatchIndex++;
   if(z7PatchIndex>=2){z7Phase=Z_COMMITCHECK;return;}
   size_t offset=Z7Policy::offset(z7Number,z7PatchIndex);
   const uint8_t payload[]={3,0,uint8_t(offset>>8),uint8_t(offset),z7PatchIndex?z7After2:z7After};request(REQ_CFG_PATCH,payload,sizeof(payload));
  }
  else if(z7Phase==Z_COMMIT){z7Committed=true;single(REQ_CFG_COMMIT,4);}
  return;
 }
 if(app.namesReady&&int32_t(now-nextZ7)>=0){const uint8_t p[]={1,0,0x16,0x70,0,8};request(REQ_Z7_PARAM,p,sizeof(p));return;}
 if(int32_t(now-nextDiag)>=0){single(REQ_DIAG,12);return;}
 if(rfStep==2){const uint8_t p[]={0xfc,6,1,0xc2};request(REQ_RF_START,p,sizeof(p));return;}
 if(rfStep==3){
  if(int32_t(now-rfDeadline)>=0){rfStep=0;nextRf=now+5000;app.error="Brak nowego pomiaru RF";}
  else if(int32_t(now-rfPollAt)>=0){single(REQ_RF_POLL,0xfc);return;}
 }
 if(!rfStep&&int32_t(now-nextRf)>=0){rfStep=1;single(REQ_RF_CLEAR,0xfc);return;}
 if(int32_t(now-nextParam)>=0){
  const Region& r=regions[paramIndex];const uint8_t p[]={1,0,uint8_t(r.start>>8),uint8_t(r.start),uint8_t(r.length>>8),uint8_t(r.length)};
  request(REQ_PARAM,p,sizeof(p));
 }
}
