#include "uninstall_core.h"
#include <string.h>
static UninstallCore core;
/* I/O entry points are replaced by the emulator's physical NOR/journal model. */
#define STUB(name,args) __attribute__((noinline)) uint32_t name args {__asm volatile("");return 1;}
STUB(TestRead,(void *c,uint32_t a,void *p,uint32_t n))
STUB(TestErase,(void *c,uint32_t a))
STUB(TestProgram,(void *c,uint32_t a,const void *p,uint32_t n))
STUB(TestJournalRead,(void *c,uint32_t a,void *p,uint32_t n))
STUB(TestJournalProgram,(void *c,uint32_t a,const void *p,uint32_t n))
uint32_t TestInit(uint32_t wrong)
{uint32_t uid[3]={1,2,3};uint8_t identity[32]={7};if(wrong)uid[0]=wrong;
 UninstallIO io={0,TestRead,TestErase,TestProgram,TestJournalRead,TestJournalProgram,0};
 return Uninstall_Init(&core,&io,uid,identity);}
uint32_t TestAudit(void){return Uninstall_Audit(&core);}
uint32_t TestApprove(void){return Uninstall_Approve(&core,UNINSTALL_CONFIRM);}
uint32_t TestStep(void){return Uninstall_Process(&core);}
uint32_t TestState(void){return core.state;}
uint32_t TestBytes(void){return core.bytes;}
uint32_t TestError(void){return core.error;}
