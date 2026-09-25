#include "gate_store.h"
#include "gate_board.h"
#include "BSP_Watchdog.h"
#include <stddef.h>
#define NOR ((uint8_t*)0x11000000U)
static uint32_t writes,cut,grant,corrupt,now;
volatile uint32_t assertions;
void *memcpy(void *d,const void *s,size_t n){uint8_t *a=d;const uint8_t *b=s;while(n--)*a++=*b++;return d;}
void *memset(void *d,int c,size_t n){uint8_t *a=d;while(n--)*a++=c;return d;}
int memcmp(const void *a,const void *b,size_t n){const uint8_t *p=a,*q=b;while(n--){if(*p!=*q)return *p-*q;p++;q++;}return 0;}
#define T(v) do{assertions++;if(!(v))return __LINE__;}while(0)
void GateJournalTest_Seed(const GateJournalRecord *,uint32_t);
uint32_t GateJournalTest_Append(const GateJournalRecord *);
uint32_t GateJournalTest_Scan(GateJournalRecord *);
uint32_t GateBoard_Millis(void){return ++now;}
uint32_t BSP_Watchdog_Checkpoint(uint32_t ms,uint32_t progress){(void)ms;(void)progress;return 1;}
void GateBoard_Display(uint32_t a,uint32_t b,uint32_t c){(void)a;(void)b;(void)c;}
uint32_t GateStore_Address(const GateStore *s,uint32_t file,uint32_t off,uint32_t *a)
{(void)s;if(file!=GATE_FILE_BOOT||off>=0x10000)return 1;*a=0x9000+off;return 0;}
uint32_t GateStore_Read(GateStore *s,uint32_t file,uint32_t off,void *p,uint32_t n)
{(void)s;if(file!=GATE_FILE_BOOT||off>=0x10000||n>0x10000-off)return 1;uint8_t *b=p;
 for(uint32_t i=0;i<n;i++)b[i]=NOR[(off+i)^1];if(corrupt&&writes>=17&&off==4096){b[32]^=1;corrupt=0;}return 0;}
uint32_t GateBoard_SetWriteRange(uint32_t address,uint32_t bytes)
{if(address<0x9000||address>=0x19000||bytes!=4096||address&4095)return 1;grant=address;return 0;}
void GateBoard_LockNor(void){grant=0;}
uint32_t GateBoard_NorErase(uint32_t a)
{if(a!=grant)return 1;writes++;memset(NOR+a-0x9000,255,cut==writes?2048:4096);return cut==writes;}
uint32_t GateBoard_NorProgram(uint32_t a,const void *p,uint32_t n)
{if(!grant||a<grant||a>=grant+4096||n>grant+4096-a||n>256||n>256-(a&255))return 1;
 writes++;const uint8_t *b=p;uint32_t count=cut==writes?n/2:n;for(uint32_t i=0;i<count;i++)NOR[a-0x9000+i]&=b[i];return cut==writes;}
static uint8_t header[4096];
uint32_t TestJournal(uint32_t mode)
{memset(NOR,255,0x10000);GateJournalRecord old={.sequence=1,.state=GATE_J_CONFIRMED,.active=0,.candidate=GATE_NO_SLOT,.uid={1,2,3},.active_generation=1};
 GateJournal_Encode(header,&old);for(uint32_t i=0;i<4096;i++)NOR[i^1]=header[i];GateJournalTest_Seed(&old,0);
 GateJournalRecord next=old;next.sequence=2;next.state=GATE_J_BOOT_PENDING;next.attempts=1;cut=mode<19?mode:0;corrupt=mode==19;
 uint32_t result=GateJournalTest_Append(&next);T(!grant);T(!memcmp(&old.uid,&next.uid,12));
 GateJournalRecord scan;T(!GateJournalTest_Scan(&scan));
 if(mode){T(result);T(scan.sequence==1);T(!memcmp(&scan,&old,sizeof(old)));}
 else{T(!result);T(scan.sequence==2);T(!memcmp(&scan,&next,sizeof(next)));}
 /* The previous completed sector is byte-exact at every cut. */
 GateJournal_Encode(header,&old);for(uint32_t i=0;i<4096;i++)T(NOR[i^1]==header[i]);return 0;}
