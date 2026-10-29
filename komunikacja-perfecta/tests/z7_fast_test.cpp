#include "../src/monitor.cpp"
#include <iostream>
#include <stdexcept>
static int checks=0;
static void require(bool ok,const char* reason){checks++;if(!ok)throw std::runtime_error(reason);}
static std::vector<uint8_t> bank,stage;
static std::vector<int> commands,readOffsets;
static int drop=-1;static bool altered=false,foreignAfter=false,mutateRecheck=false;
static void reset(bool keepPrefs=false){
 if(z7Before){free(z7Before);z7Before=nullptr;}
 if(!keepPrefs)prefs={};app={};fakeNow=10000;app.selfTest=app.ready=app.hasStatus=app.namesReady=true;app.statusAt=fakeNow;
 waiting=bridgePending=false;haveKey=identified=true;nextStatus=nextDiag=nextRf=nextParam=nextZ7=0xffffffffu/2;
 z7Phase=Z_IDLE;z7Status="idle";z7Quick=z7PrepareOnly=z7Sent=z7Committed=false;z7Seen=true;z7MaskAt=fakeNow;z7Offset=z7ReadOffset=0;z7WriteRequests=0;
 z7Number=7;z7SensorJob=false;z7Duration=z7At=0;rfStep=0;pulseCount=activeOutput=0;commands.clear();readOffsets.clear();drop=-1;altered=foreignAfter=mutateRecheck=false;
 if(!keepPrefs){bank.resize(Z7Policy::BANK);for(size_t i=0;i<bank.size();i++)bank[i]=(i*37+19)&255;bank[0x1406]=2;bank[0x1446]=14;bank[0x1670]&=~64;bank[0x1674]|=64;bank[0x1408]=1;bank[0x1448]=0;}
 memcpy(app.config,bank.data(),sizeof(app.config));stage=bank;
}
static bool begin(bool enabled,bool prepare=false,bool quick=false){String error;return requestZ7Partition1(enabled,Z7Policy::revision(app.config),error,prepare,quick);}
static void respond(){
 commands.push_back(pending[0]);std::vector<uint8_t> answer;
 if(kind==REQ_CFG_READ||kind==REQ_PARAM||kind==REQ_Z7_PARAM){
  size_t address=(pending[2]<<8)|pending[3],size=(pending[4]<<8)|pending[5];readOffsets.push_back(address);
  if(mutateRecheck&&z7Phase==Z_RECHECK_MASK&&!altered){bank[0x1605]^=2;altered=true;}
  answer={1,0,uint8_t(address>>8),uint8_t(address)};answer.insert(answer.end(),bank.begin()+address,bank.begin()+address+size);
 }else if(kind==REQ_STATUS){answer.resize(61);answer[0]=6;answer[1]=0x26;answer[2]=0x10;answer[3]=0x29;}
 else if(kind==REQ_CFG_BEGIN){stage=bank;answer={2};}
 else if(kind==REQ_CFG_PATCH){size_t address=(pending[2]<<8)|pending[3];stage[address]=pending[4];answer={3,0,pending[2],pending[3]};}
 else if(kind==REQ_CFG_COMMIT){bank=stage;if(foreignAfter)bank[0x1401]^=1;answer={4};}
 else throw std::runtime_error("unexpected UART request in fixture");
 if(drop==pending[0]){drop=-1;fakeNow+=3001;servicePanel();return;}
 received(answer.data(),answer.size());
}
static void drive(){
 for(int step=0;step<1000&&z7Busy();step++){
  fakeNow++;app.statusAt=fakeNow;servicePanel();if(waiting)respond();
 }
 require(!z7Busy(),"job must finish");require(pulseCount==0&&activeOutput==0,"no relay actions");
}
static bool result(const char* value){return !strcmp(z7Status.c_str(),value);}
static size_t count(int cmd){return std::count(commands.begin(),commands.end(),cmd);}
int main(){try{
 for(bool enabled:{true,false}){
  reset();if(!enabled){bank[0x1670]|=64;memcpy(app.config,bank.data(),sizeof(app.config));}
  auto original=bank;auto fullBackup=original;prefs.data["z7backup"]=original;require(begin(enabled),"start fast write");drive();
  require(result("success"),"write verified");require(count(1)==6,"six configuration reads, not 114");require(z7WriteRequests==3,"one BEGIN, one PATCH, one COMMIT");
  require(bool(bank[0x1670]&64)==enabled,"target bit correct");original[0x1670]=bank[0x1670];require(bank==original,"all other bank bytes preserved by patch");
  require(prefs.data["z7backup"]==fullBackup,"full backup retained");
  require(prefs.data["z7quick"].size()==256,"compact durable snapshot");require(z7ReadTotal()==256,"correct progress total");
 }
 reset();auto original=bank;require(begin(false,true,true),"start read-only fast check");drive();require(result("prepared"),"read-only prepared");require(count(1)==6&&z7WriteRequests==0&&bank==original,"same fast reads without write commands");
 reset();require(begin(false,true),"start full backup");drive();require(result("prepared")&&count(1)==56&&z7WriteRequests==0,"full backup endpoint preserved");
 reset();require(begin(false),"unchanged accepted");require(!z7Busy()&&result("unchanged")&&z7WriteRequests==0,"unchanged is immediate no-op");
 reset();prefs.failQuick=true;require(begin(true),"start backup failure");drive();require(result("error")&&z7WriteRequests==0,"no write without durable snapshot");
 reset();prefs.corruptQuick=true;require(begin(true),"start corrupt snapshot");drive();require(result("error")&&z7WriteRequests==0,"snapshot CRC checked");
 reset();require(begin(true),"start revision conflict");bank[0x1671]^=1;drive();require(result("conflict")&&z7WriteRequests==0,"fresh block revision checked");
 reset();mutateRecheck=true;require(begin(true),"start concurrent edit");drive();require(result("conflict")&&z7WriteRequests==0,"concurrent block change rejected");
 for(int command:{2,3,4}){
  reset();drop=command;require(begin(true),"start lost ACK");drive();require(count(command)==1,"write command never repeated");
  require(result(command==4?"success":"not_applied"),"read-only recovery resolves lost ACK");
 }
 reset();foreignAfter=true;require(begin(true),"start foreign change after commit");drive();require(result("uncertain"),"remaining bytes in both blocks compared");
 reset();require(begin(true),"start interrupted commit");
 while(z7Busy()){
  fakeNow++;app.statusAt=fakeNow;servicePanel();if(waiting&&kind==REQ_CFG_COMMIT){bank=stage;break;}if(waiting)respond();
 }
 require(waiting&&kind==REQ_CFG_COMMIT,"fixture interrupted at commit");reset(true);z7Setup();require(z7Quick&&z7Busy(),"compact journal reload");drive();
 require(result("success")&&z7WriteRequests==0&&count(1)==2,"restart only reads two blocks, never rewrites");
 reset();z7Before=(uint8_t*)malloc(Z7Policy::BANK);memcpy(z7Before,bank.data(),bank.size());z7After=bank[0x1670]|64;z7After2=bank[0x1674];z7Id=55;
 prefs.data["z7backup"]=bank;require(saveZ7Journal(true),"legacy full journal saved");reset(true);z7Setup();drive();require(result("not_applied")&&!z7Quick&&count(1)==56&&z7WriteRequests==0,"legacy full recovery supported");
 reset();String error;require(requestInputPartitions(9,true,false,Z7Policy::inputRevision(app.config,9),error),"sensor starts full route");drive();require(result("success")&&!z7Quick&&count(1)==114,"sensor editor still verifies full bank");
 reset();app.status[52]=5;require(!begin(true),"armed guard preserved");app.status[52]=0;app.bridge=true;require(!begin(true),"bridge guard preserved");
 reset();nextZ7=0;fakeNow++;servicePanel();require(waiting&&kind==REQ_Z7_PARAM&&pending[5]==8,"eight-byte refresh scheduled");respond();require(nextZ7==fakeNow+2000,"assignment refresh interval two seconds");
 reset();CONFIG_WRITES_ENABLED=false;require(!begin(true),"public default blocks Z7 writes");require(!requestInputPartitions(9,true,false,Z7Policy::inputRevision(app.config,9),error),"public default blocks sensor writes");
 require(begin(false,true,true),"read-only checks remain available with writes disabled");drive();require(result("prepared")&&z7WriteRequests==0,"disabled writes never send transaction commands");CONFIG_WRITES_ENABLED=true;
 std::cout<<"PASS "<<checks<<" actual monitor state-machine checks; no hardware or network\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
