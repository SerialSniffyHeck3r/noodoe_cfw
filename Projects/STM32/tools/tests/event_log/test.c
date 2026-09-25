#include "event_log.h"
#include "gate_abi.h"
#include <string.h>
static EventLog log;
static uint8_t nor[EVENT_LOG_BYTES],identity[4096];
static uint32_t uid[3]={1,2,3},writes,cut,granted;
volatile uint32_t assertions,failure;
#define CHECK(x) do{assertions++;if(!(x)){failure=__LINE__;return failure;}}while(0)
static uint32_t Read(void *c,uint32_t o,void *p,uint32_t n){(void)c;if(o+n>sizeof(nor))return 1;memcpy(p,nor+o,n);return 0;}
static uint32_t Grant(void *c){(void)c;granted=1;return 0;}
static uint32_t Erase(void *c,uint32_t o){(void)c;if(!granted||o>=EVENT_LOG_IDENTITY||(o&4095))return 1;
 if(cut&&++writes==cut)return 1;memset(nor+o,255,4096);return 0;}
static uint32_t Program(void *c,uint32_t o,const void *p,uint32_t n){(void)c;const uint8_t *b=p;
 if(!granted||o+n>EVENT_LOG_IDENTITY||!n||n>256||(o&255)+n>256)return 1;
 if(cut&&++writes==cut){/* emulate an arbitrary torn page, including commit */
  for(uint32_t i=0;i<n/2;i++)nor[o+i]&=b[i];return 1;}
 for(uint32_t i=0;i<n;i++){if((nor[o+i]&b[i])!=b[i])return 1;nor[o+i]&=b[i];}return 0;}
static EventLogIO io={0,Read,Grant,Erase,Program};
static void Scan(void){granted=0;EventLog_Init(&log,&io,uid);for(uint32_t i=0;i<70&&log.phase==LOG_SCAN;i++)EventLog_Process(&log);}
static void Drain(void){for(uint32_t i=0;i<80&&log.phase!=LOG_READY&&log.phase!=LOG_ERROR;i++)EventLog_Process(&log);}
static void Fresh(void){memset(nor,255,sizeof(nor));EventLog_Identity(identity,uid);memcpy(nor+EVENT_LOG_IDENTITY,identity,4096);cut=writes=0;Scan();}
static uint32_t Save(uint32_t value){DeviceEvent event={.code=LOG_UPDATE,.detail=value,.count=1};return EventLog_Submit(&log,&event,1);}
uint32_t Test_Cut(uint32_t at)
{Fresh();CHECK(log.phase==LOG_READY);CHECK(Save(1));Drain();CHECK(log.sequence==1);
 cut=at;writes=0;CHECK(Save(2));Drain();cut=0;Scan();CHECK(log.phase==LOG_READY);
 CHECK(log.sequence==1||log.sequence==2);CHECK(EventLog_CheckRecord(nor,uid));CHECK(!memcmp(identity,nor+EVENT_LOG_IDENTITY,4096));return 0;}
uint32_t Test_Rotate(uint32_t unused)
{(void)unused;Fresh();for(uint32_t i=1;i<=130;i++){CHECK(Save(i));Drain();CHECK(log.phase==LOG_READY&&log.sequence==i);}
 Scan();CHECK(log.phase==LOG_READY&&log.sequence==130);CHECK(!memcmp(identity,nor+EVENT_LOG_IDENTITY,4096));return 0;}
uint32_t Test_Identity(uint32_t mode)
{Fresh();if(mode==0)nor[EVENT_LOG_IDENTITY]^=1;
 if(mode==1)uid[0]++;
 if(mode==2)memset(nor+EVENT_LOG_IDENTITY,255,4096);
 Scan();CHECK(log.phase==LOG_ERROR);CHECK(!granted);return 0;}
uint32_t Test_Fault(uint32_t unused)
{(void)unused;GateFault f={.boot=42,.code=9};GateFault_Seal(&f);CHECK(GateFault_Valid(&f));
 f.registers[3]^=1;CHECK(!GateFault_Valid(&f));return 0;}
uint32_t Test_Rollback(uint32_t mode)
{GateJournalRecord j={.sequence=7,.state=GATE_J_BOOT_PENDING,.active=1,.candidate=GATE_NO_SLOT,.flags=GATE_F_TRIAL,.previous=0,.previous_generation=8,.failed_version=10,.restored_version=9,.transaction=42,.uid={1,2,3}};
 memset(j.previous_sha,0x23,32);
 if(mode==1)j.previous=GATE_NO_SLOT;
 if(mode==2)j.flags|=GATE_F_ROLLED_BACK;
 if(mode==3)j.flags=0;
 uint32_t accepted=GateJournal_RequestRollback(&j,GATE_FAILURE_APP_TIMEOUT);
 if(mode){CHECK(!accepted);return 0;}
 CHECK(accepted);CHECK(j.state==GATE_J_READY&&j.sequence==8&&j.candidate==0&&j.candidate_generation==8);
 CHECK(j.flags==(GATE_F_ROLLED_BACK|GATE_F_RESULT_PENDING));CHECK(!memcmp(j.previous_sha,j.candidate_sha,32));
 CHECK(j.failure_reason==GATE_FAILURE_APP_TIMEOUT);CHECK(!GateJournal_RequestRollback(&j,GATE_FAILURE_RESET));
 GateJournal_Encode(identity,&j);GateJournalRecord read;CHECK(GateJournal_Decode(identity,uid,&read));CHECK(!memcmp(&j,&read,sizeof(j)));return 0;}

/* Independently compared with a Python LE/CRC fixture, not just roundtrip. */
uint32_t Test_Format(uint32_t unused)
{(void)unused;GateJournalRecord j={.sequence=19,.state=GATE_J_BOOT_PENDING,.active=1,.candidate=GATE_NO_SLOT,.attempts=2,.uid={1,2,3},.active_generation=12,.candidate_generation=13,.flags=GATE_F_TRIAL,.previous=0,.previous_generation=11,.failed_version=100,.restored_version=99,.failure_reason=GATE_FAILURE_INIT,.transaction=456,.reset_epoch=87};
 for(uint32_t i=0;i<32;i++){j.active_sha[i]=i;j.candidate_sha[i]=i+32;j.previous_sha[i]=i+64;}
 GateJournal_Encode(identity,&j);return 0;}
