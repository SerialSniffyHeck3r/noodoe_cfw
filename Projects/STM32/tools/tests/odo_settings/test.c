#include "Odometer_Guard.h"
#include "Config_Store.h"
#include <string.h>
volatile uint32_t assertions;
uint32_t Cfw_Get32(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
void Cfw_Put32(uint8_t *p,uint32_t v){memcpy(p,&v,4);}
#define CHECK(x) do{assertions++;if(!(x))return __LINE__;}while(0)
static uint8_t saved[48];static uint32_t read_error=CFW_MISSING,write_error,result,writes;
static uint32_t install_hold;
uint32_t PowerService_RunRequired(void){return install_hold;}
uint32_t ConfigStore_Get(uint32_t id,void *out,uint32_t size,uint32_t *bytes)
{if(read_error)return read_error;if(id!=0x204||size<48)return CFW_ARGUMENT;memcpy(out,saved,48);*bytes=48;return 0;}
uint32_t ConfigStore_Set(uint32_t id,const void *in,uint32_t size,uint32_t *rev)
{if(write_error)return write_error;if(id!=0x204||size!=48)return CFW_ARGUMENT;memcpy(saved,in,48);*rev=++writes;return 0;}
uint32_t ConfigStore_Result(uint32_t rev){(void)rev;return result;}
uint32_t SettingsDevice_Lock(void){return 0;}void SettingsDevice_Unlock(uint32_t m){(void)m;}
/* Include owner to reset boot-local state between cold-boot fixtures. */
#include "../../../../App_Logic/Vehicle/src/odometer_service.c"
static void Reset(void){memset(&guard,0,sizeof(guard));memset(&motion,0,sizeof(motion));memset(&published,0,sizeof(published));loaded=load_error=stored_revision=write_revision=last_save=flush=request=request_revision=retry_at=0;}
static void Tick(uint32_t t,uint32_t raw,uint32_t ign){OdometerService_Tick(t,t,1,raw,ign,1,0);}
uint32_t Test(void){
 OdometerGuard s={0};OdometerGuard_Feed(&s,100,100,1,13500);CHECK(s.valid&&s.display==13500);
 for(uint32_t i=0;i<4;i++)OdometerGuard_Feed(&s,1100+i*1000,1100+i*1000,1,0);
 CHECK(s.pending&&s.display==13500&&s.reason==ODO_REVERSED);
 OdometerGuard_Feed(&s,5200,5200,1,1);CHECK(OdometerGuard_Choose(&s,ODO_KEEP));CHECK(s.display==13501&&s.corrected);
 OdometerGuard_Feed(&s,6200,6200,1,2);CHECK(s.display==13502);
 OdometerGuard_Feed(&s,6500,6500,0,0);CHECK(s.display==13502&&s.valid);
 for(uint32_t i=0;i<4;i++)OdometerGuard_Feed(&s,7100+i*1000,7100+i*1000,1,0);
 CHECK(s.pending);CHECK(OdometerGuard_Choose(&s,ODO_ACCEPT));CHECK(s.display==0&&!s.corrected);
 OdometerGuard_Feed(&s,12000,12000,1,90000);CHECK(!s.pending&&s.display==0);
 OdometerGuard_Feed(&s,15000,15000,1,90000);CHECK(s.display==0);
 for(uint32_t i=0;i<4;i++)OdometerGuard_Feed(&s,16000+i*1000,16000+i*1000,1,90000);
 CHECK(s.pending&&s.reason==ODO_JUMP);CHECK(OdometerGuard_Choose(&s,ODO_LATER)&&s.pending);
 OdometerGuard_Feed(&s,20000,20000,1,1);CHECK(!s.pending&&s.display==1);
 for(uint32_t i=0;i<40;i++)OdometerGuard_Feed(&s,21000+i*100,21000,1,0);CHECK(!s.pending);
 memset(&s,0,sizeof(s));for(uint32_t i=0;i<4;i++)OdometerGuard_Feed(&s,100+i*1000,100+i*1000,1,0xffffffff);
 CHECK(s.pending&&!s.valid&&!OdometerGuard_Choose(&s,ODO_ACCEPT));
 Reset();result=CFW_PENDING;Tick(100,13000,1);CHECK(writes==1&&write_revision);
 result=CFW_IO;Tick(200,13000,1);CHECK(published.save_result==CFW_IO&&guard.dirty);Tick(201,13000,1);CHECK(writes==1);Tick(1200,13000,1);CHECK(writes==2);result=0;Tick(1300,13000,1);CHECK(published.save_result==0);
 Tick(1400,13001,1);CHECK(writes==2);Tick(1500,0,0);CHECK(writes==2&&published.display==13001);
 OdometerService_Checkpoint();Tick(1600,0,0);CHECK(writes==3);Tick(1700,0,0);CHECK(published.display==13001);
 read_error=0;Reset();Tick(100,0,1);CHECK(published.display==13001);
 for(uint32_t i=1;i<=6;i++)Tick(100+i*1000,0,1);
 CHECK(published.pending&&published.ready);CHECK(OdometerGuard_RequestDecision(published.revision+1,ODO_KEEP)==CFW_EXPIRED);
 CHECK(!OdometerGuard_RequestDecision(published.revision,ODO_KEEP));Tick(7200,1,1);Tick(7300,1,1);CHECK(published.display==13002&&published.corrected&&!published.pending);
 read_error=0;Reset();Tick(100,2,1);CHECK(published.display==13003&&published.corrected);
 install_hold=1;Tick(6000,2,1);CHECK(!published.ready);install_hold=0;
 Cfw_Put32(saved+12,0xffffffffU);Cfw_Put32(saved+16,0x7fffffffU);
 Reset();Tick(100,3,1);CHECK(published.save_result==CFW_CORRUPT&&!published.valid&&!published.ready);
 return 0;
}
