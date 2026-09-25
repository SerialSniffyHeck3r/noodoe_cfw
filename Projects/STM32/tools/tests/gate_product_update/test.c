#include "RuntimeUpdate.h"
#include "Uninstall_Expected.h"
#include "BootStore.h"
#include "Cfw_Files.h"
#include "BSP_RAM.h"
#include "BSP_NOR.h"
#include "Resources.h"
#include "BSP_Buttons.h"
#include "Update_Metadata.h"
#include "fixture.h"
#include <string.h>

#define FILES ((uint8_t*)0x11000000U)
#define FIXTURE ((const uint8_t*)0x12000000U)
#define CHECK(x) do{++assertions;if(!(x)){failure=__LINE__;return failure;}}while(0)
volatile uint32_t assertions,failure,resets,erases,programs,callback_errors,metadata_commits,last_result,last_offset;
volatile BSP_RAM_Diagnostics g_bsp_ram;
volatile BSP_NOR_Diagnostics g_bsp_nor;
volatile GateMailbox g_recovery_mailbox;
static uint32_t grants,sequence,now,compatible=1,fail_program,link_during_program;
static uint32_t corrupt_readback,skip_confirm,confirm_before_scan;
static uint32_t allocation_address=0xC0020000U,allocation_calls,early_visibility,journal_fixture_case;
static UpdateService *service;
static uint8_t payload[1024],reply[NDCP_FRAME_MAX],header[4096];
void *memset(void *d,int v,size_t n){uint8_t *p=d;if((uintptr_t)d==allocation_address&&RuntimeUpdate_GetService())++early_visibility;while(n--)*p++=(uint8_t)v;return d;}
void *memcpy(void *d,const void *s,size_t n){uint8_t *a=d;const uint8_t *b=s;while(n--)*a++=*b++;return d;}
void *memmove(void *d,const void *s,size_t n){uint8_t *a=d;const uint8_t *b=s;if(a<b)while(n--)*a++=*b++;else while(n){--n;a[n]=b[n];}return d;}
int memcmp(const void *a,const void *b,size_t n){const uint8_t *x=a,*y=b;while(n--){if(*x!=*y)return *x-*y;++x;++y;}return 0;}
uint32_t HAL_GetUIDw0(void){return 1;}
uint32_t HAL_GetUIDw1(void){return 2;}
uint32_t HAL_GetUIDw2(void){return 3;}
void GateProductTest_Reset(void){++resets;}
void *BSP_RAM_Allocate(size_t n){++allocation_calls;if(RuntimeUpdate_GetService())++early_visibility;return n<65536?(void*)(uintptr_t)allocation_address:NULL;}
void *BSP_RAM_AllocateNamed(uint32_t owner,size_t n){return owner==BSP_RAM_BOOT_STORE&&n<65536?(void*)0xC0010000U:NULL;}
uint32_t ResourceStore_Compatible(const uint8_t sha[32]){return compatible&&sha[0]==0x77;}
uint32_t ResourceStore_Busy(void){return 0;}
uint32_t ResourceStore_TransferActive(void){return 0;}
uint32_t ResourceStore_TransferCancel(void){return 1;}
uint32_t ResourceStore_Transfer(uint32_t op,const uint8_t *p,uint32_t n,uint8_t *out){(void)op;(void)p;(void)n;memset(out,0,92);return 0;}
uint32_t BSP_Buttons_GetState(BSP_Buttons_Button b,BSP_Buttons_State *s){(void)b;memset(s,0,sizeof(*s));return 1;}
static uint32_t uninstall_tx,uninstall_version,uninstall_crc;
UpdateMetadataResult UpdateMetadata_Read(uint32_t words[5]){memset(words,0,20);words[0]=UPDATE_METADATA_RESIDENT;
 if(uninstall_crc){words[1]=uninstall_version;words[2]=0x7f90;words[3]=UPDATE_APP_BYTES;words[4]=uninstall_crc;}return UPDATE_METADATA_OK;}
UpdateMetadataResult UpdateMetadata_Commit(uint32_t v,uint32_t crc,uint32_t arm,void *scratch,uint32_t bytes)
{(void)v;(void)crc;(void)arm;(void)scratch;(void)bytes;++metadata_commits;return UPDATE_METADATA_HARDWARE;}
/* Peripheral adapters are isolated; the production transfer and BootStore
 * continue to run unchanged. Uninstall transport gets a separate fixture. */
static uint32_t uninstall_tx,uninstall_version,uninstall_crc;
#define UNINSTALL_STAGE ((uint8_t*)0x13000000U)
#define UNINSTALL_SOURCE ((uint8_t*)0x14000000U)
uint32_t RuntimeUninstall_Enable(uint32_t tx){uninstall_tx=tx;return 0;}
uint32_t RuntimeUninstall_Read(uint32_t a,void *p,uint32_t n){if(a<UPDATE_STAGE_BASE||n>UPDATE_APP_BYTES-(a-UPDATE_STAGE_BASE))return 1;memcpy(p,UNINSTALL_STAGE+a-UPDATE_STAGE_BASE,n);return 0;}
uint32_t RuntimeUninstall_Program(uint32_t a,const void *p,uint32_t n){if(!uninstall_tx||a<UPDATE_STAGE_BASE||n>256||n>UPDATE_APP_BYTES-(a-UPDATE_STAGE_BASE))return 1;memcpy(UNINSTALL_STAGE+a-UPDATE_STAGE_BASE,p,n);return 0;}
uint32_t RuntimeUninstall_Commit(uint32_t v,uint32_t c){uninstall_version=v;uninstall_crc=c;metadata_commits++;return 0;}
uint32_t RuntimeUninstall_Untouched(void){return 1;}
BSP_NOR_Status BSP_NOR_OTAErase4K(uint32_t a){if(!uninstall_tx||a<UPDATE_STAGE_BASE||a>=UPDATE_STAGE_BASE+UPDATE_APP_BYTES||(a&4095))return 1;memset(UNINSTALL_STAGE+a-UPDATE_STAGE_BASE,255,4096);return 0;}
void BSP_NOR_OTADisable(void){}
uint32_t CfwFiles_Status(void){return CFW_OK;}
uint32_t CfwFiles_Size(uint32_t f){return f==CFW_BOOT_JOURNAL?0x10000U:f==CFW_BOOT_A||f==CFW_BOOT_B?0x80000U:0;}
uint32_t CfwFiles_Grant(uint32_t f){if(!CfwFiles_Size(f))return CFW_ARGUMENT;grants|=1U<<f;return 0;}
static uint8_t *Range(uint32_t f,uint32_t off,uint32_t n)
{uint32_t size=CfwFiles_Size(f);if(!n||off>=size||n>size-off){++callback_errors;return NULL;}
 return FILES+(f-CFW_BOOT_A)*0x80000U+off;}
uint32_t CfwFiles_Read(uint32_t f,uint32_t off,void *d,uint32_t n)
{uint8_t *p=Range(f,off,n);if(!p||!d)return CFW_ARGUMENT;memcpy(d,p,n);
 if(corrupt_readback&&n<=256){((uint8_t*)d)[0]^=1;corrupt_readback=0;}return 0;}
uint32_t CfwFiles_Erase(uint32_t f,uint32_t off)
{uint8_t *p=Range(f,off,4096);if(!p||!(grants&(1U<<f))||(off&4095)){++callback_errors;return CFW_ARGUMENT;}++erases;memset(p,255,4096);return 0;}
uint32_t CfwFiles_Program(uint32_t f,uint32_t off,const void *data,uint32_t n)
{uint8_t *p=Range(f,off,n);const uint8_t *d=data;
 if(!p||!(grants&(1U<<f))||!d||n>256||n>256-(off&255)){++callback_errors;return CFW_ARGUMENT;}
 ++programs;if(fail_program&&!--fail_program)return CFW_IO;
 for(uint32_t i=0;i<n;++i){if((p[i]&d[i])!=d[i]){++callback_errors;return CFW_IO;}p[i]=d[i];}
 if(link_during_program){link_during_program=0;UpdateService_SetConnected(service,0);UpdateService_SetConnected(service,1);UpdateService_Authorize(service,UPDATE_STAGE_ARM);}
 return 0;}
uint32_t Cfw_Get32(const uint8_t *p){return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
void Cfw_Put32(uint8_t *p,uint32_t v){for(uint32_t i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
uint32_t Cfw_Crc(const void *data,uint32_t n){return GatePolicy_Crc(data,n);}
static void Ready(void)
{g_bsp_ram.ready=1;g_bsp_ram.result=0;g_bsp_ram.geometry_bytes=BSP_RAM_GEOMETRY_BYTES;g_bsp_ram.capacity_bytes=BSP_RAM_GEOMETRY_BYTES;
 g_bsp_nor.ready=1;g_bsp_nor.capacity_bytes=BSP_NOR_CAPACITY_BYTES;g_bsp_nor.jedec_id=BSP_NOR_EXPECTED_JEDEC_ID;}
static void HardwareFault(uint32_t mode)
{Ready();switch(mode){
 case 0:g_bsp_ram.ready=0;break;
 case 1:g_bsp_ram.result=1;break;
 case 2:g_bsp_ram.capacity_bytes=BSP_RAM_TEST_BYTES;break;
 case 3:g_bsp_ram.capacity_bytes=BSP_RAM_GEOMETRY_BYTES+1;break;
 case 4:g_bsp_nor.ready=0;break;
 case 5:--g_bsp_nor.capacity_bytes;break;
 case 6:g_bsp_nor.jedec_id^=1;break;
 }}

/* Real BootStore scans and confirms a canonical journal before any upload.
 * CfwFiles is a NOR semantics substitute only; it never invents a success
 * record or performs the image/header/sequence policy on the code's behalf. */
static uint32_t Initialize(uint32_t invalid_inactive)
{
 memset(FILES,255,0x110000U);GateImageInfo image={.uid={1,2,3},.generation=1,.version=0x10006};
 if(journal_fixture_case==4)image.generation=UINT32_MAX;
 if(journal_fixture_case==6)image.generation=2;
 memcpy(image.sha256,fixture_sha,32);memcpy(image.requirement,FIXTURE+0x200,44);
 GateImage_Encode(header,&image);memcpy(FILES,header,4096);
 if(!invalid_inactive)memcpy(FILES+0x80000,header,4096);
 const uint32_t uid[3]={1,2,3};
 GateIdentity_Encode(header,uid,0);memcpy(FILES+0x7f000,header,4096);
 GateIdentity_Encode(header,uid,1);memcpy(FILES+0xff000,header,4096);
 memcpy(FILES+4096,FIXTURE,UPDATE_GATE_BYTES);
 GateJournalRecord j={.sequence=1,.state=GATE_J_BOOT_PENDING,.active=0,.candidate=GATE_NO_SLOT,.attempts=1,.uid={1,2,3},.active_generation=1};
 if(journal_fixture_case==4)j.active_generation=UINT32_MAX;
 if(journal_fixture_case>=5){j.flags=GATE_F_TRIAL;j.previous=GATE_NO_SLOT;j.transaction=j.reset_epoch=GATE_RESTORE_RETAINED;if(journal_fixture_case==6)j.active_generation=2;}
 if(journal_fixture_case==7){j.flags=GATE_F_ROLLED_BACK|GATE_F_RESULT_PENDING;j.transaction=99;j.reset_epoch=0;}
 memcpy(j.active_sha,fixture_sha,32);GateJournal_Encode(header,&j);memcpy(FILES+0x100000,header,4096);
 if(journal_fixture_case==1){Cfw_Put32(header+4,3);Cfw_Put32(header+4088,GatePolicy_Crc(header,4088));memcpy(FILES+0x101000,header,4096);}
 if(journal_fixture_case==2){j.state=GATE_J_WAIT;GateJournal_Encode(header,&j);memcpy(FILES+0x101000,header,4096);}
 if(journal_fixture_case==3){j.sequence+=0x80000000U;GateJournal_Encode(header,&j);memcpy(FILES+0x101000,header,4096);}
 GateRetained r;GateRetained_Init(&r);GateRetained_BeforeBoot(&r);memcpy((void*)&g_recovery_mailbox.request,&r,sizeof(r));
 if(confirm_before_scan)BootStore_RequestConfirm();
 for(uint32_t i=0;i<17;i++){BootStore_Process();}
 if(journal_fixture_case>=1&&journal_fixture_case<=3){CHECK(!BootStore_Ready());CHECK(g_boot_store.error==(journal_fixture_case==1?CFW_VERSION:CFW_CORRUPT));CHECK(!erases&&!programs);return 0;}
 CHECK(BootStore_Ready());
 if(skip_confirm)return 0;
 if(!confirm_before_scan){BootStore_RequestConfirm();BootStore_Process();}
 CHECK(g_boot_store.confirmed);CHECK(g_boot_store.sequence==2);
 Ready();CHECK(RuntimeUpdate_Init()==0);service=RuntimeUpdate_GetService();CHECK(service);
 UpdateService_SetConnected(service,1);RuntimeUpdate_Process(++now);CHECK(!service->authorization);
 return 0;
}
static uint32_t Command(uint32_t op,uint32_t n)
{NDCP_Frame f={.opcode=op,.sequence=++sequence,.length=n,.payload=payload};
 uint32_t r=UpdateService_Handle(service,&f);if(r)return 100+r;
 RuntimeUpdate_Process(++now);if(!UpdateService_TakeReply(service,reply,sizeof(reply)))return UINT32_MAX;
 return Cfw_Get32(reply+NDCP_HEADER_SIZE);}
static void Manifest(uint32_t target,uint32_t tx)
{memset(payload,0,56);Cfw_Put32(payload,tx);Cfw_Put32(payload+4,0x10006);Cfw_Put32(payload+8,target==2?UPDATE_GATE_BYTES:UPDATE_APP_BYTES);
 Cfw_Put32(payload+12,FIXTURE_CRC);memcpy(payload+16,fixture_sha,32);Cfw_Put32(payload+48,UPDATE_STAGE_ARM);Cfw_Put32(payload+52,target);}
static uint32_t Upload(void)
{for(uint32_t at=0;at<UPDATE_GATE_BYTES;){uint32_t n=UPDATE_GATE_BYTES-at;if(n>960)n=960;if(n>4096-(at&4095))n=4096-(at&4095);
 Cfw_Put32(payload,service->transaction);Cfw_Put32(payload+4,at);memcpy(payload+8,FIXTURE+at,n);CHECK(Command(UPDATE_OP_DATA,n+8)==0);at+=n;}
 CHECK(service->received==UPDATE_GATE_BYTES);return 0;}
static uint32_t Finish(void)
{Cfw_Put32(payload,service->transaction);uint32_t first=Command(UPDATE_OP_FINISH,4);
 /* The first verification slice can reject a malformed vector immediately.
  * A synchronous error reply is a valid completion, not a missing response. */
 if(first!=UINT32_MAX)return first;
 for(uint32_t n=0;n<120&&service->state==UPDATE_VERIFYING;n++)RuntimeUpdate_Process(++now);
 CHECK(UpdateService_TakeReply(service,reply,sizeof(reply))>0);return Cfw_Get32(reply+NDCP_HEADER_SIZE);}
static void CommitPayload(void)
{Cfw_Put32(payload,service->transaction);Cfw_Put32(payload+4,UPDATE_COMMIT_ARM);memcpy(payload+8,fixture_sha,32);}

uint32_t Test_Transfer(void)
{
 CHECK(!Initialize(0));uint32_t before=erases;
 Manifest(2,7);CHECK(Command(UPDATE_OP_BEGIN,56)==UPDATE_LOCKED);CHECK(erases==before);
 UpdateService_Authorize(service,UPDATE_STAGE_ARM);
 Manifest(0,7);CHECK(Command(UPDATE_OP_BEGIN,56)==UPDATE_LOCKED);
 Manifest(1,7);CHECK(Command(UPDATE_OP_BEGIN,56)==UPDATE_LOCKED);CHECK(erases==before);
 Manifest(2,7);CHECK(Command(UPDATE_OP_BEGIN,56)==0);CHECK(erases==before+4);
 Cfw_Put32(payload,7);Cfw_Put32(payload+4,0);payload[8]=0;CHECK(Command(UPDATE_OP_DATA,9)==UPDATE_ARGUMENT);
 CHECK(service->received==0);CHECK(!Upload());CHECK(Finish()==0);CHECK(service->state==UPDATE_VERIFIED);
 CHECK(!memcmp(FILES+0x81000,FIXTURE,UPDATE_GATE_BYTES));CHECK(!metadata_commits);
 CommitPayload();CHECK(Command(UPDATE_OP_COMMIT,40)==0);CHECK(service->state==UPDATE_COMMITTED);CHECK(BootStore_Committed(fixture_sha));
 CHECK(!service->authorization&&!metadata_commits);CHECK(g_boot_store.active==0&&g_boot_store.candidate==1);
 GateJournalRecord j;const uint32_t uid[3]={1,2,3};CHECK(GateJournal_Decode(FILES+0x102000,uid,&j));CHECK(j.state==GATE_J_READY&&j.candidate==1&&j.active==0);
 CommitPayload();before=programs;CHECK(Command(UPDATE_OP_COMMIT,40)==0);CHECK(programs==before);
 Cfw_Put32(payload,7);Cfw_Put32(payload+4,UPDATE_RESET_ARM);CHECK(Command(UPDATE_OP_RESET,8)==0);
 uint32_t reset_seq=sequence;RuntimeUpdate_Process(now+5000);CHECK(!resets);
 UpdateService_NotifyReplyTransmitted(service,reset_seq+1,now);RuntimeUpdate_Process(now+1500);CHECK(!resets);
 UpdateService_NotifyReplyTransmitted(service,reset_seq,now);RuntimeUpdate_Process(now+1499);CHECK(!resets);
 UpdateService_NotifyReplyTransmitted(service,reset_seq+1,now+1000);
 UpdateService_SetConnected(service,0);RuntimeUpdate_Process(now+100);CHECK(!resets&&service->state==UPDATE_RESET_WAIT);
 RuntimeUpdate_Process(now+1500);CHECK(resets==1&&service->state==UPDATE_FAILED&&service->commit_uncertain);
 UpdateService_SetConnected(service,1);RuntimeUpdate_Process(++now);
 CHECK(Command(UPDATE_OP_RESET,8)==UPDATE_STATE);UpdateService_NotifyReplyTransmitted(service,sequence,now);
 RuntimeUpdate_Process(now+1500);CHECK(resets==1);CHECK(!metadata_commits&&!callback_errors);
 GateRetained retained;memcpy(&retained,(const void*)&g_recovery_mailbox.request,sizeof(retained));CHECK(GateRetained_Valid(&retained)&&retained.reason==GATE_REASON_INSTALL);
 return 0;
}
uint32_t Test_AbortVerified(void)
{
 CHECK(!Initialize(0));UpdateService_Authorize(service,UPDATE_STAGE_ARM);Manifest(2,8);CHECK(Command(UPDATE_OP_BEGIN,56)==0);
 CHECK(!Upload());CHECK(Finish()==0);Cfw_Put32(payload,8);CHECK(Command(UPDATE_OP_ABORT,4)==0);
 CHECK(service->state==UPDATE_IDLE);UpdateService_Authorize(service,UPDATE_STAGE_ARM);Manifest(2,9);CHECK(Command(UPDATE_OP_BEGIN,56)==0);return 0;
}
uint32_t Test_BlankInactive(void){return Initialize(1);}
uint32_t Test_PhysicalCorruption(void)
{
 CHECK(!Initialize(0));UpdateService_Authorize(service,UPDATE_STAGE_ARM);Manifest(2,10);CHECK(Command(UPDATE_OP_BEGIN,56)==0);
 CHECK(!Upload());FILES[0x81000+20000]^=1;CHECK(Finish()==UPDATE_HASH);CHECK(service->state==UPDATE_FAILED);
 CommitPayload();CHECK(Command(UPDATE_OP_COMMIT,40)==UPDATE_STATE);CHECK(!metadata_commits&&g_boot_store.candidate==GATE_NO_SLOT);return 0;
}
uint32_t Test_DisconnectDuringPage(void)
{
 CHECK(!Initialize(0));UpdateService_Authorize(service,UPDATE_STAGE_ARM);Manifest(2,11);CHECK(Command(UPDATE_OP_BEGIN,56)==0);
 Cfw_Put32(payload,11);Cfw_Put32(payload+4,0);memcpy(payload+8,FIXTURE,512);link_during_program=1;
 /* New epoch must receive no old-session reply, even when reconnect+auth
  * occurs within the first physical page programming callback. */
 CHECK(Command(UPDATE_OP_DATA,520)==UINT32_MAX);RuntimeUpdate_Process(++now);
 CHECK(service->state==UPDATE_FAILED&&service->received==0);CHECK(!metadata_commits);return 0;
}
uint32_t Test_EarlyConfirmation(void){confirm_before_scan=1;return Initialize(0);}
uint32_t Test_WrongBootSequence(void)
{
 skip_confirm=1;CHECK(!Initialize(0));CHECK(!g_boot_store.confirmed);
 CHECK(BootStore_BeginUpdate(90)==CFW_BUSY);
 GateRetained r;memcpy(&r,(const void*)&g_recovery_mailbox.request,sizeof(r));
 GateRetained_BeforeBoot(&r);memcpy((void*)&g_recovery_mailbox.request,&r,sizeof(r));
 BootStore_RequestConfirm();BootStore_Process();CHECK(!BootStore_Ready());
 CHECK(!g_boot_store.confirmed&&!programs&&!erases);return 0;
}
/* The transfer/hash pass is tested above. These cases isolate commit-last
 * durability: interrupt each header/journal edge with real BootStore code,
 * and independently decode the previous complete journal afterward. */
uint32_t Test_CommitFault(uint32_t cut)
{
 CHECK(!Initialize(0));CHECK(BootStore_BeginUpdate(91)==0);
 memcpy(FILES+0x81000,FIXTURE,UPDATE_GATE_BYTES);
 if(cut)fail_program=cut;else corrupt_readback=1;
 CHECK(BootStore_Commit(0x10006,fixture_sha,FIXTURE+0x200)!=0);
 CHECK(!BootStore_Ready());CHECK(!BootStore_Committed(fixture_sha));
 GateJournalRecord j;const uint32_t uid[3]={1,2,3};
 CHECK(GateJournal_Decode(FILES+0x101000,uid,&j));CHECK(j.state==GATE_J_CONFIRMED&&j.active==0&&j.candidate==GATE_NO_SLOT&&j.sequence==2);
 CHECK(!GateJournal_Decode(FILES+0x102000,uid,&j));CHECK(!metadata_commits&&!callback_errors);
 return 0;
}
/* No bad allocator pointer may be dereferenced or published, and hardware
 * diagnostic failures must be rejected before allocation or storage access. */
uint32_t Test_InitBounds(void)
{
 for(uint32_t mode=0;mode<7;mode++){
  HardwareFault(mode);
  CHECK(RuntimeUpdate_Init()==RUNTIME_UPDATE_NOT_READY);CHECK(!allocation_calls&&!RuntimeUpdate_GetService());
 }
 Ready();allocation_address=0;CHECK(RuntimeUpdate_Init()==RUNTIME_UPDATE_NO_MEMORY);
 const uint32_t bad[]={0x20000000U,0xC0000000U,0xC000FFE0U,0xC0020001U,0xC4000000U-32U,0xFFFFFFE0U};
 for(uint32_t i=0;i<sizeof(bad)/sizeof(bad[0]);i++){allocation_address=bad[i];CHECK(RuntimeUpdate_Init()==RUNTIME_UPDATE_BAD_MEMORY);CHECK(!RuntimeUpdate_GetService()&&!g_runtime_update.ready);}
 allocation_address=0xC0020000U;CHECK(RuntimeUpdate_Init()==0);CHECK(RuntimeUpdate_GetService());CHECK(!early_visibility);
 uint32_t calls=allocation_calls;CHECK(RuntimeUpdate_Init()==0&&allocation_calls==calls);
 CHECK(!programs&&!erases&&!metadata_commits);return 0;
}
/* Python sets the real emulated CMSIS special registers before these calls,
 * so ISR/unprivileged/PRIMASK/BASEPRI/FAULTMASK are not C predicate substitutes. */
uint32_t Test_InitContext(void)
{Ready();CHECK(RuntimeUpdate_Init()==RUNTIME_UPDATE_CONTEXT);CHECK(!allocation_calls&&!RuntimeUpdate_GetService());return 0;}
uint32_t Test_PrepareContext(void){return Initialize(0);}
uint32_t Test_CallbackContext(void)
{uint32_t words[5],calls=programs+erases,polls=g_runtime_update.polls;
 CHECK(service->platform.metadata_read(service,words)!=0);
 CHECK(service->platform.enable(service,55)!=0);
 CHECK(service->platform.program(service,UPDATE_STAGE_BASE,payload,32)!=0);
 RuntimeUpdate_Process(++now);CHECK(g_runtime_update.polls==polls);
 CHECK(programs+erases==calls&&!resets);return 0;}
uint32_t Test_HardwareRecheck(void)
{CHECK(!Initialize(0));for(uint32_t mode=0;mode<7;mode++){
 HardwareFault(mode);
 CHECK(!Test_CallbackContext());}return 0;}
uint32_t Test_VectorBoundary(uint32_t mode)
{CHECK(!Initialize(0));UpdateService_Authorize(service,UPDATE_STAGE_ARM);Manifest(2,12);CHECK(Command(UPDATE_OP_BEGIN,56)==0);
 /* Isolate the first physical verify read; full transfer/hash is exercised in
  * Test_Transfer. This injected invalid vector must fail before hashing. */
 memcpy(FILES+0x81000,FIXTURE,UPDATE_GATE_BYTES);service->received=UPDATE_GATE_BYTES;
 if(mode<3)Cfw_Put32(FILES+0x81000,mode==0?0x20007000U:mode==1?0x20006FF8U:0x2002FF08U);
 else Cfw_Put32(FILES+0x81004,0x08010101U);
 Cfw_Put32(payload,12);CHECK(Command(UPDATE_OP_FINISH,4)==UPDATE_VECTOR);CHECK(service->state==UPDATE_FAILED&&!service->verified);return 0;}
uint32_t Test_JournalGuard(uint32_t mode)
{journal_fixture_case=mode;CHECK(!Initialize(0));
 if(mode==4){CHECK(BootStore_BeginUpdate(99)==0);uint32_t before=programs+erases;
  CHECK(BootStore_Commit(0x10006,fixture_sha,FIXTURE+0x200)==CFW_ARGUMENT);CHECK(programs+erases==before);}
 return 0;}
uint32_t Test_InvalidVersion(void)
{CHECK(!Initialize(0));UpdateService_Authorize(service,UPDATE_STAGE_ARM);Manifest(2,15);Cfw_Put32(payload+4,UINT32_MAX);
 uint32_t before=programs+erases;CHECK(Command(UPDATE_OP_BEGIN,56)==UPDATE_ARGUMENT);CHECK(programs+erases==before);return 0;}
uint32_t Test_ResourcesRequired(void)
{CHECK(!Initialize(0));UpdateService_Authorize(service,UPDATE_STAGE_ARM);Manifest(2,16);CHECK(Command(UPDATE_OP_BEGIN,56)==0);
 /* Canonical image with matching independent SHA/CRC but required=0. It can
  * pass transport integrity, yet must never become an executable candidate
  * because the independent gate requires the external resource contract. */
 memcpy(FILES+0x81000,FIXTURE,UPDATE_GATE_BYTES);Cfw_Put32(FILES+0x81208,0);
 service->received=UPDATE_GATE_BYTES;service->expected_crc=NO_RESOURCES_CRC;
 memcpy(service->expected_sha,no_resources_sha,32);CHECK(Finish()==0);
 CommitPayload();memcpy(payload+8,no_resources_sha,32);uint32_t before=programs+erases;
 CHECK(Command(UPDATE_OP_COMMIT,40)==UPDATE_COMMIT_AMBIGUOUS);CHECK(service->state==UPDATE_FAILED);
 CHECK(programs+erases==before&&!g_runtime_update.commit_calls&&g_boot_store.candidate==GATE_NO_SLOT&&!metadata_commits);return 0;}
/* Completed sectors survive reconnect; active image remains byte-identical. */
uint32_t Test_Resume(uint32_t mode)
{
 CHECK(!Initialize(0));UpdateService_Authorize(service,UPDATE_STAGE_ARM);Manifest(2,123);CHECK(Command(UPDATE_OP_BEGIN,56)==0);
 for(uint32_t off=0;off<5120;off+=512){Cfw_Put32(payload,123);Cfw_Put32(payload+4,off);memcpy(payload+8,FIXTURE+off,512);CHECK(Command(UPDATE_OP_DATA,520)==0);}
 CHECK(service->received==5120);UpdateService_SetConnected(service,0);RuntimeUpdate_Process(++now);
 UpdateService_SetConnected(service,1);RuntimeUpdate_Process(++now);UpdateService_Authorize(service,UPDATE_STAGE_ARM);
 if(mode==1)FILES[0x81000+100]^=1;
 Manifest(2,mode==2?124:123);uint32_t result=Command(UPDATE_OP_BEGIN,56);
 if(mode==1){CHECK(result==UPDATE_IO);CHECK(service->state==UPDATE_FAILED);return 0;}
 CHECK(result==0);CHECK(service->received==(mode==2?0U:4096U));CHECK(!memcmp(FILES+4096,FIXTURE,UPDATE_GATE_BYTES));
 CHECK(!metadata_commits&&!callback_errors);return 0;
}

/* Exact release-pinned cleaner uses the original staging range, while current
 * CFW FAT images and journals remain byte-identical until the standalone UI. */
uint32_t Test_Uninstall(uint32_t wrong)
{
 CHECK(!Initialize(0));UpdateService_Authorize(service,UPDATE_STAGE_ARM);
 static const uint8_t approved[32]=UNINSTALL_IMAGE_SHA;
 Cfw_Put32(payload,55);Cfw_Put32(payload+4,UNINSTALL_TRANSPORT_VERSION);Cfw_Put32(payload+8,UPDATE_APP_BYTES);
 Cfw_Put32(payload+12,Cfw_Crc(UNINSTALL_SOURCE,UPDATE_APP_BYTES));memcpy(payload+16,approved,32);
 Cfw_Put32(payload+48,UPDATE_STAGE_ARM);Cfw_Put32(payload+52,UPDATE_TARGET_UNINSTALL);
 if(wrong){payload[16]^=1;CHECK(Command(UPDATE_OP_BEGIN,56)==UPDATE_HASH);CHECK(!uninstall_tx);return 0;}
 CHECK(!Command(UPDATE_OP_BEGIN,56));uint32_t old=programs+erases;
 for(uint32_t off=0;off<UPDATE_APP_BYTES;off+=512){Cfw_Put32(payload,55);Cfw_Put32(payload+4,off);memcpy(payload+8,UNINSTALL_SOURCE+off,512);last_offset=off;last_result=Command(UPDATE_OP_DATA,520);CHECK(!last_result);}
 CHECK(!Finish());CHECK(service->state==UPDATE_VERIFIED);CHECK(service->verified==UPDATE_APP_BYTES);
 Cfw_Put32(payload,55);Cfw_Put32(payload+4,UPDATE_COMMIT_ARM);memcpy(payload+8,approved,32);CHECK(!Command(UPDATE_OP_COMMIT,40));
 CHECK(service->state==UPDATE_COMMITTED&&metadata_commits==1&&programs+erases==old);
 CHECK(!memcmp(UNINSTALL_STAGE,UNINSTALL_SOURCE,UPDATE_APP_BYTES));CHECK(!memcmp(FILES+4096,FIXTURE,UPDATE_GATE_BYTES));return 0;
}

/* The restore marker is one-shot and cannot suppress resets on later updates. */
uint32_t Test_RetainedRestore(uint32_t later){journal_fixture_case=later?6:5;skip_confirm=1;CHECK(!Initialize(0));BootStore_RequestConfirm();BootStore_Process();GateJournalRecord j;CHECK(BootStore_GetBootInfo(&j));CHECK(!(j.flags&GATE_F_TRIAL));if(later){CHECK(j.flags&GATE_F_RESET_PENDING);CHECK(j.reset_epoch==2);}else{CHECK(!(j.flags&GATE_F_RESET_PENDING));CHECK(j.transaction==0&&j.reset_epoch==0);CHECK(g_boot_store.confirmed);}return 0;}

/* Typed Diagnostic shares the actual receive/checkpoint/hash path. An old
 * Gate is refused before the first erase; Product role cannot be disguised. */
uint32_t Test_Diagnostic(uint32_t mode){
 CHECK(!Initialize(0));uint32_t *cap=(uint32_t*)GATE_FEATURE_ADDRESS;
 cap[0]=GATE_FEATURE_MAGIC;cap[1]=1;cap[2]=1;cap[3]=~1U;
 if(mode==1)cap[0]=0;
 UpdateService_Authorize(service,UPDATE_STAGE_ARM);
 uint8_t *source=(uint8_t*)UNINSTALL_STAGE;memcpy(source,FIXTURE,UPDATE_GATE_BYTES);
 Cfw_Put32(source+0x208,0);memset(source+0x20c,0,32);Cfw_Put32(source+0x238,3);
 if(mode==2)Cfw_Put32(source+0x238,2);
 if(mode==3)Cfw_Put32(source+0x208,1);
 UpdateSha256 h;uint8_t digest[32];UpdateSha256_Init(&h);UpdateSha256_Feed(&h,source,UPDATE_GATE_BYTES);UpdateSha256_Final(&h,digest);
 Cfw_Put32(payload,78);Cfw_Put32(payload+4,1);Cfw_Put32(payload+8,UPDATE_GATE_BYTES);Cfw_Put32(payload+12,Cfw_Crc(source,UPDATE_GATE_BYTES));
 memcpy(payload+16,digest,32);Cfw_Put32(payload+48,UPDATE_STAGE_ARM);Cfw_Put32(payload+52,UPDATE_TARGET_DIAGNOSTIC);
 uint32_t before=programs+erases,r=Command(UPDATE_OP_BEGIN,56);
 if(mode==1){CHECK(r!=0);CHECK(programs+erases==before);return 0;}CHECK(!r);
 for(uint32_t off=0;off<UPDATE_GATE_BYTES;off+=512){Cfw_Put32(payload,78);Cfw_Put32(payload+4,off);memcpy(payload+8,source+off,512);CHECK(!Command(UPDATE_OP_DATA,520));}
 r=Finish();if(mode==2){CHECK(r==UPDATE_VECTOR);return 0;}CHECK(!r);
 Cfw_Put32(payload,78);Cfw_Put32(payload+4,UPDATE_COMMIT_ARM);memcpy(payload+8,digest,32);
 before=programs+erases;r=Command(UPDATE_OP_COMMIT,40);
 if(mode==3){CHECK(r!=0);CHECK(programs+erases==before);return 0;}CHECK(!r);
 GateJournalRecord j;CHECK(BootStore_GetBootInfo(&j));CHECK(j.flags==GATE_F_DIAGNOSTIC);CHECK(j.previous==0&&j.candidate==1);
 GateImageInfo image;CHECK(GateImage_Decode(FILES+0x80000,j.uid,&image));CHECK(image.kind==GATE_IMAGE_DIAGNOSTIC);
 CHECK(!memcmp(FILES+4096,FIXTURE,UPDATE_GATE_BYTES));CHECK(!metadata_commits);
 before=programs+erases;BootStore_RequestConfirm();BootStore_Process();CHECK(programs+erases==before);
 return 0;
}

/* Real journal: a dismissal during the thirty-second fallback health window
 * must survive until confirmation. It never confirms the boot itself. */
uint32_t Test_RollbackEarlyAck(uint32_t mode)
{
 journal_fixture_case=7;skip_confirm=1;CHECK(!Initialize(0));
 GateJournalRecord j;CHECK(BootStore_GetBootInfo(&j));CHECK(!g_boot_store.confirmed);
 CHECK(!BootStore_RequestResultAck(0));CHECK(!BootStore_RequestResultAck(98));
 if(mode==0)CHECK(BootStore_RequestResultAck(99));
 uint32_t before=programs+erases;
 for(uint32_t i=0;i<40;i++)BootStore_Process();
 CHECK(programs+erases==before);CHECK(BootStore_GetBootInfo(&j));CHECK(j.flags&GATE_F_RESULT_PENDING);
 BootStore_RequestConfirm();BootStore_Process();CHECK(g_boot_store.confirmed);
 CHECK(BootStore_GetBootInfo(&j));CHECK(!(j.flags&GATE_F_ROLLED_BACK));CHECK(j.flags&GATE_F_RESULT_PENDING);
 if(mode==1){CHECK(BootStore_RequestResultAck(99));CHECK(BootStore_RequestResultAck(99));}
 BootStore_Process();CHECK(BootStore_GetBootInfo(&j));CHECK(!(j.flags&GATE_F_RESULT_PENDING));
 CHECK(j.transaction==99);CHECK(!BootStore_RequestResultAck(99));
 before=programs+erases;BootStore_Process();CHECK(programs+erases==before);
 CHECK(BootStore_BeginUpdate(100)==0);return 0;
}
