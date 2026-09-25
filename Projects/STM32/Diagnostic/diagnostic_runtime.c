#include "diagnostic.h"
#include "gate_policy.h"
#include "Bootstrap_Confirm.h"
#include "Bootstrap_Screen.h"
#include "Health_Service.h"
#include "NoodoeBluetooth.h"
#include "Bluetooth_KeyCodec.h"
#include "RadioSelfTest.h"
#include "AmbientService.h"
#include "Config_Store.h"
#include "StorageDisk.h"
#include "event_log.h"
#include "NDCP.h"
#include "Update_Service.h"
#include "BSP_Buttons.h"
#include "BSP_Display.h"
#include "BSP_Power.h"
#include "BSP_RAM.h"
#include "BSP_NOR.h"
#include "BSP_BT_HCI.h"
#include "PowerService.h"
#include "cmsis_os2.h"
#include "dma.h"
#include "spi.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>
/* This task owns storage and test requests. The unmodified Bluetooth service
 * owns HCI/SPP. No Product settings model, UI heap, or update confirmation is
 * linked; only existing config fields containing pairing keys may be written. */
static NDCP_Parser parser;
static UpdateSha256 identity_hash;
static uint8_t identity_digest[64];
static uint32_t identity_offset,identity_phase;
static Bluetooth_LinkState link;
static uint8_t tx[NDCP_FRAME_MAX];
static uint32_t tx_bytes,selection,restore,restore_since,storage_ready,radio_started;
static uint32_t key_revision,key_generation,operation,last_result,test_sequence;
static uint32_t pending_test,test_started,log_lease_until,log_offset;
static uint32_t action_epoch,action_sequence,action_value,action_result,action_operation;
static uint32_t last_screen,nor_error,ram_error,display_error,audit_error,log_started;
static GateGesture gesture;
static BootstrapConfirm confirm;
static EventLog log_store;
static DeviceEvent events[8];
static uint32_t event_read,event_write,log_lost;
static const char *const names[]={"Live status","Bluetooth start","Pair phone (120s)","Bluetooth raw reset",
 "Light sensor probe","Light sensor ID","Light sensor addresses","Back to stock"};
/* Journal records contain only diagnostic states/IDs, never bond keys or
 * personal phone content. A full queue is explicitly reported, not hidden. */
static void Log(uint32_t code,uint32_t detail)
{
 if(event_write-event_read>=8){++log_lost;return;}
 DeviceEvent *e=&events[event_write++%8];memset(e,0,sizeof(*e));
 e->code=code;e->detail=detail;e->boot=g_recovery_mailbox.request.sequence;
 e->transaction=g_recovery_mailbox.boot.transaction;e->time_ms=HAL_GetTick();e->count=1;
 e->data[0]=3;e->data[1]=operation;e->data[2]=last_result;
}
static uint32_t LogRead(void *p,uint32_t o,void *b,uint32_t n){(void)p;return CfwFiles_Read(CFW_LOG,o,b,n);}
static uint32_t LogGrant(void *p){(void)p;return CfwFiles_Grant(CFW_LOG);}
static uint32_t LogErase(void *p,uint32_t o){(void)p;return CfwFiles_Erase(CFW_LOG,o);}
static uint32_t LogProgram(void *p,uint32_t o,const void *b,uint32_t n){(void)p;return CfwFiles_Program(CFW_LOG,o,b,n);}
/* A broken log file cannot prevent local stock recovery. Config errors remain
 * visible and inhibit key persistence instead of resetting user settings. */
static void Storage(uint32_t now)
{
 if(nor_error||ram_error)return;
 if(!storage_ready){audit_error=CfwFiles_Process();if(audit_error==CFW_PENDING)return;
  if(audit_error)return;storage_ready=1;CfwStore_Init();
  EventLogIO io={0,LogRead,LogGrant,LogErase,LogProgram};
  EventLog_Init(&log_store,&io,(const uint32_t*)0x1fff7a10);log_started=1;Log(LOG_BOOT,3);
 }
 CfwStore_Process(now);ConfigStore_Process(now);
 if(log_lease_until&&(restore||(int32_t)(now-log_lease_until)>=0))log_lease_until=0;
 if(log_started&&!log_lease_until){
  EventLog_Process(&log_store);
  if(log_store.phase==LOG_READY&&event_write!=event_read&&
     EventLog_Submit(&log_store,&events[event_read%8],1))++event_read;
 }
 if(!radio_started&&(g_config_store.ready||g_config_store.error)){
  uint8_t raw[BLUETOOTH_KEY_BYTES];uint32_t count=0;Bluetooth_KeyStore keys;
  if(!ConfigStore_Get(CONFIG_FIELD_BT_KEYS,raw,sizeof(raw),&count)&&Bluetooth_DecodeKeys(&keys,raw,count))
   (void)Bluetooth_ImportKeys(&keys);
  (void)Bluetooth_Start();radio_started=1;
 }
 if(key_revision){uint32_t r=ConfigStore_Result(key_revision);
  if(r!=CFW_PENDING){if(!r)Bluetooth_MarkKeysPersisted(key_generation);else Log(LOG_STORAGE,r);key_revision=0;}
 }else if(radio_started&&!restore&&g_config_store.ready&&!g_config_store.error&&
          g_bluetooth.key_generation!=g_bluetooth.key_persisted_generation){
  Bluetooth_KeyStore keys;uint8_t raw[BLUETOOTH_KEY_BYTES];
  if(!Bluetooth_ExportKeys(&keys)){Bluetooth_EncodeKeys(raw,&keys);
   if(!ConfigStore_Set(CONFIG_FIELD_BT_KEYS,raw,sizeof(raw),&key_revision))key_generation=keys.generation;}
 }
}
void Diagnostic_RequestStock(void)
{if(!restore){restore=1;restore_since=HAL_GetTick();Log(LOG_UPDATE,GATE_REASON_STOCK);}}
/* Commands are fixed one-shot tests; there is no arbitrary register, address,
 * factory write, NOR write, or remote confirmation of stock restoration. */
static uint32_t Test(uint32_t which)
{
 if(restore||pending_test)return CFW_BUSY;
 uint32_t r=CFW_ARGUMENT,id=0;
 if(which==1)r=(uint32_t)Bluetooth_RequestStart(++test_sequence);
 else if(which==2){++test_sequence;r=(uint32_t)Bluetooth_SetPairingWindow(120);}
 else if(which==3)r=(uint32_t)Bluetooth_RequestResetDiagnostic(++test_sequence);
 else if(which==4)r=AmbientService_RequestProbe(80000,&id);
 else if(which==5)r=AmbientService_RequestIDDiagnostic(&id);
 else if(which==6)r=AmbientService_RequestAddressDiagnostic(&id);
 operation=id?id:test_sequence;last_result=r;
 if(!r){pending_test=which;test_started=HAL_GetTick();last_result=CFW_PENDING;}
 Log(which<=3?LOG_BT:LOG_STORAGE,r);return r;
}
/* Queue acceptance never masquerades as a successful hardware test. Match
 * the exact owner completion token and retain the final result for the UI. */
static void TestProgress(uint32_t now)
{
 if(!pending_test)return;
 uint32_t done=0,result=CFW_PENDING;
 if(pending_test>=4){Ambient_Snapshot a;AmbientService_GetSnapshot(&a);
  if(a.completed_id==operation){done=1;result=a.completed_result;}
 }else if(pending_test==2){if(Bluetooth_PairingSeconds()){done=1;result=0;}}
 else {Bluetooth_ControlMailbox b;uint32_t mask=__get_PRIMASK();__disable_irq();
  memcpy(&b,(const void*)&g_bluetooth_control,sizeof(b));__set_PRIMASK(mask);
  if(b.ack_sequence==operation&&b.accept_result){done=1;result=(uint32_t)b.accept_result;}
  else if(b.completion_sequence==operation&&b.phase>=3){done=1;result=(uint32_t)b.completion_result;}
 }
 if(!done&&now-test_started>=35000){done=1;result=CFW_EXPIRED;}
 if(done){uint32_t kind=pending_test;pending_test=0;last_result=result;
  Log(kind<=3?LOG_BT:LOG_STORAGE,result);}
}
/* Results are copied before return. A reply is retained across a full TX
 * queue but discarded on a link epoch change, never replayed to another peer. */
static void Reply(const NDCP_Frame *f,uint32_t result,const void *data,uint32_t bytes)
{
 uint8_t payload[1024];if(bytes>sizeof(payload)-4)return;
 memcpy(payload,&result,4);if(bytes)memcpy(payload+4,data,bytes);
 tx_bytes=(uint32_t)NDCP_Encode(tx,sizeof(tx),f->opcode,NDCP_FLAG_RESPONSE|(result?NDCP_FLAG_ERROR:0),f->sequence,payload,bytes+4);
}
static void Frame(void *p,const NDCP_Frame *f)
{
 (void)p;if(f->flags||tx_bytes)return;
 if(f->opcode==1&&!f->length){uint8_t b[68]={0};uint32_t u[]={NDCP_VERSION,HAL_GetUIDw0(),HAL_GetUIDw1(),HAL_GetUIDw2()};
  memcpy(b,u,16);memcpy(b+16,(void*)0x08008000,20);memcpy(b+36,"NOODOE-DIAGNOSTIC-v1",20);Reply(f,0,b,sizeof(b));return;}
 if(f->opcode==0x58&&!f->length){
  if(identity_phase!=2){Reply(f,2,0,0);return;}
  uint8_t b[84];uint32_t u[]={1,3,HAL_GetUIDw0(),HAL_GetUIDw1(),HAL_GetUIDw2()};
  memcpy(b,u,20);memcpy(b+20,identity_digest,64);Reply(f,0,b,sizeof(b));return;
 }
 if(!Bluetooth_LinkSecure(BLUETOOTH_PHONE,&link)){Reply(f,CFW_ARGUMENT,0,0);return;}
 if(f->opcode==0x96&&!f->length){uint32_t b[]={1,3,selection,operation,last_result,restore,
  g_bluetooth.state,g_bluetooth.last_error,g_ambient_service.driver.raw,g_ambient_service.driver.millilux,
  g_ambient_service.driver.valid&&!g_ambient_service.stale,g_ambient_service.driver.error,
  audit_error,g_config_store.error,log_store.phase,log_store.error,log_lost,link.reconnects};Reply(f,0,b,sizeof(b));
 }else if(f->opcode==0x96&&f->length==8){uint32_t epoch,action;memcpy(&epoch,f->payload,4);memcpy(&action,f->payload+4,4);
  /* Physical confirmation still owns restoration. Phone merely opens it. */
  uint32_t r=epoch!=link.reconnects?CFW_EXPIRED:CFW_OK;
  if(!r&&action_epoch==epoch&&action_sequence==f->sequence){
   r=action_value==action?action_result:CFW_ARGUMENT;Reply(f,r,&action_operation,4);return;}
  if(!r){if(action==7){selection=7;BootstrapConfirm_Init(&confirm);}else r=Test(action);
   action_epoch=epoch;action_sequence=f->sequence;action_value=action;action_result=r;action_operation=operation;}
  Reply(f,r,&operation,4);
 }else if(f->opcode==0x92){uint8_t b[528];uint32_t n=0;
  uint32_t r=RadioSelfTest_Handle(f->payload,f->length,link.reconnects,HAL_GetTick(),b,&n);Reply(f,r,b,r?0:n);
 }else if(f->opcode==0x97&&f->length==8){uint32_t off,n;uint8_t b[960];memcpy(&off,f->payload,4);memcpy(&n,f->payload+4,4);
  uint32_t r=!n||n>sizeof(b)||off>EVENT_LOG_BYTES||n>EVENT_LOG_BYTES-off?CFW_ARGUMENT:CFW_OK;
  /* A short read-only lease pins completed log sectors while the phone
   * exports them. Expiry, disconnect or recovery always releases the writer. */
  if(!r&&!off){if(!log_started||log_store.phase!=LOG_READY)r=CFW_BUSY;
   else {log_offset=0;log_lease_until=HAL_GetTick()+15000;}}
  if(!r&&(!log_lease_until||off!=log_offset))r=CFW_EXPIRED;
  if(!r){r=CfwFiles_Read(CFW_LOG,off,b,n);if(!r){log_offset+=n;log_lease_until=HAL_GetTick()+15000;
   if(log_offset==EVENT_LOG_BYTES)log_lease_until=0;}}
  Reply(f,r,b,r?0:n);
 }else Reply(f,CFW_ARGUMENT,0,0);
}
static void Screen(void)
{
 BootstrapView v={0};char a[64],b[64],c[64];v.title="DIAGNOSTIC MODE";v.line1=names[selection];v.percent=101;
 snprintf(a,sizeof(a),"BT %lu / error %lX",(unsigned long)g_bluetooth.state,(unsigned long)g_bluetooth.last_error);
 snprintf(b,sizeof(b),"Light raw %lu / error %lu",(unsigned long)g_ambient_service.driver.raw,(unsigned long)g_ambient_service.driver.error);
 snprintf(c,sizeof(c),pending_test?"Request %lu / running":"Request %lu / result %ld",(unsigned long)operation,(long)last_result);
 v.line2=a;v.line3=b;v.hint=c;v.notice="UP / DOWN - O to run";
 if(selection==7){v.line2="Done testing? Back to stock.";v.line3="Release O, then hold for 2s.";v.notice="No phone needed.";}
 if(restore){v.line2="Finishing the last write.";v.line3="Stock recovery is next.";v.notice="Keep permanent power connected.";}
 if(audit_error>CFW_PENDING||nor_error||ram_error||display_error){v.error=audit_error?audit_error:nor_error?nor_error:ram_error?ram_error:display_error;v.notice="Storage unavailable. Recovery still works.";}
 else if(log_store.phase==LOG_ERROR)v.notice="Log unavailable. Recovery still works.";
 (void)BootstrapScreen_Draw(&v);
}
/* Generated defaultTask calls only LCDTest in every profile. Diagnostics keep
 * the unit awake regardless of IGN; the recovery gesture remains independent
 * of PH9, radio, storage health, and display submission. */
void LCDTest(void)
{
 HealthService_Init(HAL_GetTick());HealthService_Register(HEALTH_STORAGE,HAL_GetTick());
 MX_DMA_Init();BSP_Power_Init(0);BSP_Buttons_Init();
 GateGesture_Init(&gesture,HAL_GetTick(),0);BootstrapConfirm_Init(&confirm);
 display_error=BSP_Display_Init();if(!display_error)(void)BSP_Display_SetBrightnessPercent(25);
 MX_SPI5_Init();nor_error=BSP_NOR_Init();ram_error=BSP_RAM_Init();
 if(!nor_error&&!ram_error){audit_error=StorageDisk_Detect()?CFW_OK:CFW_IO;if(!audit_error)CfwFiles_Begin();else nor_error=audit_error;}
 AmbientService_Init();NDCP_Init(&parser,Frame,0);HealthService_BootReady();
 for(;;){
  uint32_t now=HAL_GetTick();BSP_Buttons_Process();BSP_Power_Process(now);
  BSP_Buttons_State enter;BSP_Buttons_GetState(BSP_BUTTON_ENTER,&enter);
  if(GateGesture_Process(&gesture,now,g_bsp_power.ign_valid&&g_bsp_power.ign_on,enter.raw_pressed))Diagnostic_RequestStock();
  if(selection==7&&BootstrapConfirm_Process(&confirm,now,enter.raw_pressed))Diagnostic_RequestStock();
  BSP_Buttons_Event e;while(BSP_Buttons_GetEvent(&e))if(!restore&&e.type==BSP_BUTTON_EVENT_SHORT_PRESS){
   if(e.button==BSP_BUTTON_UP||e.button==BSP_BUTTON_DOWN){selection=(selection+(e.button==BSP_BUTTON_UP?7:1))%8;BootstrapConfirm_Init(&confirm);}
   else if(e.button==BSP_BUTTON_ENTER&&selection>0&&selection<7)(void)Test(selection);
  }
  if(identity_phase<2){
   if(!identity_offset)UpdateSha256_Init(&identity_hash);
   uint32_t base=identity_phase?0x08000000:0x08020000,bytes=identity_phase?0x8000:0x60000;
   UpdateSha256_Feed(&identity_hash,(const uint8_t*)(uintptr_t)(base+identity_offset),4096);identity_offset+=4096;
   if(identity_offset==bytes){UpdateSha256_Final(&identity_hash,identity_digest+32*identity_phase++);identity_offset=0;}
  }
  Storage(now);
  if(!radio_started&&(nor_error||ram_error||audit_error>CFW_PENDING)){(void)Bluetooth_Start();radio_started=1;}
  if(!restore)AmbientService_Process(now);
  TestProgress(now);
  Bluetooth_LinkState current;Bluetooth_GetLinkState(BLUETOOTH_PHONE,&current);
  if(current.reconnects!=link.reconnects||current.status!=link.status){link=current;NDCP_Init(&parser,Frame,0);tx_bytes=0;log_lease_until=0;action_sequence=0;}
  if(link.status==BLUETOOTH_LINK_UP){uint8_t b[256];
   if(tx_bytes){if(Bluetooth_SendSession(BLUETOOTH_PHONE,&link,tx,tx_bytes)==BLUETOOTH_OK)tx_bytes=0;}
   else {int n=Bluetooth_ReceiveSession(BLUETOOTH_PHONE,&link,b,sizeof(b));if(n>0)NDCP_Feed(&parser,b,(uint32_t)n,now);}
  }
  NDCP_Poll(&parser,now);
  if(now-last_screen>=200){last_screen=now;Screen();}
  if(restore){uint32_t logs_busy=log_started&&log_store.phase!=LOG_READY&&log_store.phase!=LOG_ERROR;
   if(!CfwStore_Busy()&&!ConfigStore_Busy()&&!key_revision&&!logs_busy&&
      (event_write==event_read||!log_started||log_store.phase==LOG_ERROR))Diagnostic_ResetToGate(GATE_REASON_STOCK);
   /* Never endlessly feed a stuck writer. Gate reconstructs journal state. */
   if(now-restore_since>=10000)Diagnostic_ResetToGate(GATE_REASON_WAIT);
  }
  HealthService_Progress(HEALTH_STORAGE,HAL_GetTick());(void)HealthService_Process(HAL_GetTick(),0);osDelay(5);
 }
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *u){BSP_BT_HCI_OnRxComplete(u);}
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *u){BSP_BT_HCI_OnTxComplete(u);}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *u){BSP_BT_HCI_OnError(u);}
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *s){BSP_NOR_OnTxRxComplete(s);}
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *s){BSP_NOR_OnError(s);}
void USART1_IRQHandler(void){HAL_UART_IRQHandler(&huart1);}
void BSP_Display_CaptureInvalidate(void){}
uint32_t PowerService_Mode(void){return POWER_RUN;}
void PowerService_Acknowledge(uint32_t owner,uint32_t ready){(void)owner;(void)ready;}
void PowerService_IgnitionIRQ(void){}
