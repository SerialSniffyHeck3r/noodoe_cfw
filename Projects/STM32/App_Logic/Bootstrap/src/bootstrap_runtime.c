#include "Bootstrap_Image.h"
#include "Bootstrap_Storage.h"
#include "Bootstrap_Resources.h"
#include "Bootstrap_Bond.h"
#include "App_Recovery.h"
#include "bsp_board_revision.h"
#include "Bootstrap_UI.h"
#include "Bootstrap_Confirm.h"
#include "Bootstrap_Recovery.h"
#include "Bootstrap_Target.h"
#include "Bootstrap_Screen.h"
#include "gate_policy.h"
#include "BSP_Watchdog.h"
#include "Health_Service.h"
#include "NoodoeBluetooth.h"
#include "RadioSelfTest.h"
#include "AmbientService.h"
#include "bsp_backlight.h"
#include "NDCP.h"
#include "RuntimeUpdate.h"
#include "StorageSWD.h"
#include "Resources.h"
#include "BSP_NOR.h"
#include "BSP_RAM.h"
#include "BSP_Power.h"
#include "BSP_BT_HCI.h"
#include "BSP_Buttons.h"
#include "BSP_Display.h"
#include "bsp_eve_bus.h"
#include "bsp_bringup.h"
#include "PowerService.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include "dma.h"
#include "spi.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>

/* Installer runtime deliberately owns no Product objects, fonts or persisted
 * settings. Its sole task is the storage owner. BT has its existing separate
 * owner; only copied RFCOMM bytes cross that boundary. */
typedef struct {uint32_t magic,version,heartbeat,nor,ram,display,bt,stage,error,maintenance_until,requests,replies;} BootstrapDiagnostics;
volatile BootstrapDiagnostics g_bootstrap={.magic=0x42535431,.version=1};
static BootstrapStorage *store;
static NDCP_Parser parser;
static Bluetooth_LinkState session;
static uint32_t pending_seq,pending_op,pending,permission_until,last_screen,authorized_epoch;
static BootstrapUI ui;
static GateGesture recovery_gesture;
static BootstrapConfirm recovery_confirm;
static uint32_t local_recovery,drain_started,approval_transaction,approval_epoch,operation_error;
/* Diagnostic pairing is read-only. Installation needs a separate local entry
 * even if the phone retains the same encrypted RFCOMM session. */
static uint32_t install_mode,storage_started;
static BootstrapBond bond;
static uint32_t bond_pending,bond_restored;
static uint32_t InstallHold(void);
static uint32_t display_failure_since,display_failure_active,display_runtime_error;
static void Word(uint8_t *b,uint32_t *n,uint32_t v){memcpy(b+*n,&v,4);*n+=4;}
static uint32_t sent_sequence,sent_opcode,sent_wait,inspect_next,pending_job;
static UpdateService *update;
static uint32_t InstallHold(void){return update&&InstallSession_Active(&update->install);}
/* The target hash and BT pause are bounded but can outlive the writer's
 * 250ms freshness requirement. Check all real owners after that work, before
 * requesting the five-second FLASH lease. Do not weaken the watchdog gate. */
uint32_t RuntimeUpdate_FlashReady(void)
{
    uint32_t now=HAL_GetTick();HealthService_Progress(HEALTH_STORAGE,now);
    return HealthService_Process(now,0);
}
static uint8_t tx[NDCP_FRAME_MAX];
static uint32_t EmbeddedRead(void *ctx,uint32_t off,void *buf,uint32_t n)
{return BootstrapImage_Read(ctx,off,buf,n)?1U:0U;}
static const RecoverySource embedded={NULL,EmbeddedRead};
const RecoverySource *AppRecovery_EmbeddedSource(void){return &embedded;}
static uint32_t LocalProduct(void *p,uint32_t off,void *out,uint32_t n)
{(void)p;return BootstrapStorage_ReadProduct(store,off,out,n);}
static uint32_t Read(void *p,uint32_t a,void *b,uint32_t n){(void)p;return BSP_NOR_Read(a,b,n);}
static uint32_t Grant(void *p,uint32_t a,uint32_t n,uint32_t m){(void)p;return BSP_NOR_ProvisionGrant(a,n,m);}
static uint32_t Erase(void *p,uint32_t a){(void)p;return BSP_NOR_ProvisionErase(a);}
static uint32_t Program(void *p,uint32_t a,const void *b,uint32_t n){(void)p;return BSP_NOR_ProvisionProgram(a,b,n);}
static void Lock(void *p){(void)p;BSP_NOR_ProvisionLock();}
static uint32_t Ign(void *p){(void)p;return g_bsp_power.ign_valid&&g_bsp_power.ign_on;}
static uint32_t WritePower(void *p){return Ign(p)||InstallHold();}
/* Only one operation owns NOR at a time. A verified pending update remains
 * owned until explicit abort/commit; maintenance cannot alter its inputs. */
static uint32_t UpdateIdle(void)
{return !update||(!update->commit_uncertain&&(update->state==UPDATE_IDLE||update->state==UPDATE_FAILED)&&update->request_read==update->request_write);}
static uint32_t StorageIdle(void)
{return !bond_pending&&g_storage_swd.state!=STORAGE_SWD_BUSY&&(!store||((store->state==BS_IDLE||store->state==BS_SAVED||store->state==BS_FAILED)&&!store->request_pending&&!BootstrapStorage_BenchActive(store)));}
static uint32_t Authorized(void)
{return install_mode&&!g_bootstrap.display&&!g_bootstrap.nor&&!g_bootstrap.ram&&g_bluetooth.state==BLUETOOTH_STATE_READY&&!local_recovery&&!AppRecovery_RuntimeRequested()&&(Ign(NULL)||InstallHold())&&authorized_epoch&&authorized_epoch==session.reconnects&&session.status==BLUETOOTH_LINK_UP&&Bluetooth_LinkSecure(BLUETOOTH_PHONE,&session);}
/* A completed upload is not enough: file bodies and current FAT ownership are
 * independently inspected before admitting a Product boot dependency. */
static uint32_t TargetRead(uint32_t address,void *data,uint32_t bytes){return BSP_NOR_Read(address,data,bytes);}
uint32_t BootstrapUpdate_ValidateTarget(uint32_t target,const uint8_t requirement[44],uint32_t version,const uint8_t sha[32])
{
    if(target==UPDATE_TARGET_STOCK)return RecoveryStock_Matches(version,sha)?0:UPDATE_LOCKED;
    if(!store||!requirement||!BootstrapStorage_FilesReady(store)||store->verified_files!=511U)return UPDATE_LOCKED;
    /* Initial layout2 is a 448KiB gate+Product image. Prove that its Product
     * bytes match BOTH recovery slots and the initial CONFIRMED journal. */
    uint32_t proof=BootstrapTarget_Check(TargetRead,HAL_GetTick,store->gate_sha,store->gate_boot_sha,requirement);
    if(proof)return proof;
    ResourceRequirement r;memcpy(&r,requirement,sizeof(r));
    return r.magic==0x51534352U&&r.version==1U&&r.required==1U&&
        BootstrapStorage_ResourceCompatible(store,r.sha256)?0:UPDATE_LOCKED;
}
/* Rendering consumes a snapshot; hardware and protocol states stay in the owner. */
static void Screen(void)
{
 BootstrapView view;BootstrapUI_View(&ui,&view);
 if(!g_bootstrap.display){uint32_t result=BootstrapScreen_Draw(&view),now=HAL_GetTick();
  if(!result)display_failure_active=0;
  else {if(!display_failure_active){display_failure_active=1;display_failure_since=now;}
   /* SPI success alone is not a submitted frame. Publish a stuck FIFO/swap
    * through phone status too; do not reset EVE in the middle of a write. */
   if(now-display_failure_since>=10000U)display_runtime_error=result;
  }
 }
}
/* Reply is retained until the RFCOMM queue accepts it. Never drop a completed
 * storage response merely because the radio is still transmitting a packet. */
static void Reply(uint32_t op,uint32_t seq,uint32_t error,const void *data,uint32_t n)
{
    if(pending)return;
    pending=(uint32_t)NDCP_Encode(tx,sizeof(tx),op,NDCP_FLAG_RESPONSE|(error?NDCP_FLAG_ERROR:0),seq,data,n);
}
static void Frame(void *unused,const NDCP_Frame *f)
{
    if((bond_pending||local_recovery||AppRecovery_RuntimeRequested())&&f->opcode!=0x85&&f->opcode!=0x84&&f->opcode!=0x5a&&f->opcode!=0x58&&f->opcode!=0x60&&f->opcode!=UPDATE_OP_STATUS&&f->opcode!=0x48){uint32_t r=BS_BUSY;Reply(f->opcode,f->sequence,1,&r,4);return;}
    (void)unused;if(pending||pending_job||sent_wait)return;++g_bootstrap.requests;
    if(f->flags)return;
    if(!install_mode&&f->opcode!=1&&f->opcode!=0x92&&f->opcode!=0x8c&&f->opcode!=0x86&&f->opcode!=0x85&&f->opcode!=0x84&&f->opcode!=0x5a&&f->opcode!=0x58&&f->opcode!=0x60&&f->opcode!=UPDATE_OP_STATUS){uint32_t r=BS_DENIED;Reply(f->opcode,f->sequence,1,&r,4);return;}
    if(f->opcode==0x92){uint8_t b[528]={0};uint32_t n=0;
        uint32_t r=ui.state!=BOOT_UI_BT_TEST||InstallHold()||!StorageIdle()||!UpdateIdle()||!Bluetooth_LinkSecure(BLUETOOTH_PHONE,&session)?BS_DENIED:
            RadioSelfTest_Handle(f->payload,f->length,session.reconnects,HAL_GetTick(),b+4,&n);
        memcpy(b,&r,4);Reply(f->opcode,f->sequence,r,b,r?4:n+4);
    }else if(f->opcode==0x8c&&f->length==0&&update){uint32_t b[20];InstallSession_Snapshot(&update->install,b);Reply(f->opcode,f->sequence,0,b,sizeof(b));
    }else if(f->opcode==1&&f->length==0){
        uint8_t b[72]={0};uint32_t n=0;Word(b,&n,0);Word(b,&n,NDCP_VERSION);
        Word(b,&n,HAL_GetUIDw0());Word(b,&n,HAL_GetUIDw1());Word(b,&n,HAL_GetUIDw2());
        memcpy(b+20,(const void*)0x08008000,20);memcpy(b+40,"NOODOE-BOOTSTRAP-v1",19);Reply(f->opcode,f->sequence,0,b,sizeof(b));
    }else if(f->opcode==0x5a&&f->length==0){
        uint32_t now=HAL_GetTick();uint32_t remaining=(int32_t)(permission_until-now)>0?permission_until-now:0;
        uint32_t b[9]={0,1,ui.state,ui.phase,ui.position,ui.total,ui.error,
            (ui.connected?1U:0U)|(ui.busy?2U:0U)|(ui.install_allowed?4U:0U)|(ui.can_cancel?8U:0U),remaining};
        Reply(f->opcode,f->sequence,0,b,sizeof(b));
    }else if(f->opcode==0x84&&f->length==0){
        uint32_t b[16]={0,2,ui.state,ui.phase,ui.subphase,ui.kind,ui.position,ui.total,ui.error,g_bootstrap.heartbeat,HAL_GetTick(),ui.now-ui.mark,ui.requests,ui.replies,
            (ui.stalled?1U:0U)|(ui.connected?2U:0U)|(ui.busy?4U:0U)|(Ign(NULL)?8U:0U)|16U,session.reconnects};
        Reply(f->opcode,f->sequence,0,b,sizeof(b));
    }else if(f->opcode==0x85&&f->length==0){
        /* Read-only latched commit diagnostics; no addresses or write API.
         * Schema1 exposes the failed boundary, not whichever poll ran last. */
        uint32_t b[12]={0,1,g_runtime_update.commit_phase,g_runtime_update.commit_result,
            g_runtime_update.commit_detail,g_runtime_update.commit_destructive,
            g_runtime_update.writer_result,g_runtime_update.resume_result,
            update?update->state:0,update?update->result:0,
            update?update->commit_uncertain:0,local_recovery};
        Reply(f->opcode,f->sequence,0,b,sizeof(b));
    }else if(f->opcode==0x58&&f->length==0){
        uint8_t b[88]={0};uint32_t r=AppRecovery_Identity(b+4,1U)?0:BS_BUSY;memcpy(b,&r,4);
        Reply(f->opcode,f->sequence,r,b,r?4:88);
    }else if(f->opcode==0x60&&f->length==8){
        /* Authenticated read-only bootstrap export; never arbitrary addresses.
         * Metadata may change during install, so prohibit concurrent writers. */
        uint32_t off,count;memcpy(&off,f->payload,4);memcpy(&count,f->payload+4,4);
        uint8_t b[512]={0};uint32_t n=0;
        uint32_t r=!Bluetooth_LinkSecure(BLUETOOTH_PHONE,&session)?BS_DENIED:
            local_recovery||AppRecovery_RuntimeRequested()||!StorageIdle()||!UpdateIdle()?BS_BUSY:
            !RecoveryTarget_Export((const uint8_t*)0x08000000U,off,count,b+32)?BS_ARGUMENT:0;
        Word(b,&n,r);Word(b,&n,1);Word(b,&n,off);Word(b,&n,count);
        Word(b,&n,*(const volatile uint32_t*)0x08008000U);
        Word(b,&n,*(const volatile uint16_t*)0x0800c080U);Word(b,&n,BSP_BoardRevision());
        Word(b,&n,!off?RecoveryTarget_Verify((const uint8_t*)0x08000000U):0);
        Reply(f->opcode,f->sequence,r,b,r?4:32+count);
    }else if(f->opcode==0x86&&f->length==0){
        const uint32_t caps[]={0,2,960,4096,111};Reply(f->opcode,f->sequence,0,caps,sizeof(caps));
    }else if(f->opcode==0x1f&&f->length==16){
        uint32_t v[4];memcpy(v,f->payload,16);uint32_t r=BS_DENIED;
        if(update&&store&&store->backup_valid&&(!store->scoped||store->scope_verified)&&Authorized()&&
           v[0]==HAL_GetUIDw0()&&v[1]==HAL_GetUIDw1()&&v[2]==HAL_GetUIDw2()&&v[3]==0x42414b32){
            UpdateService_Authorize(update,UPDATE_STAGE_ARM);r=0;}
        Reply(f->opcode,f->sequence,r,&r,4);
    }else if(((f->opcode>=0x40&&f->opcode<=0x47)||f->opcode==UPDATE_OP_LOCAL_DATA)&&update){
        if(f->opcode==UPDATE_OP_BEGIN){ui.install_allowed=approval_transaction=approval_epoch=0;operation_error=0;}
        uint32_t r=(f->opcode==UPDATE_OP_COMMIT&&(!Authorized()||!ui.install_allowed||approval_transaction!=update->transaction||approval_epoch!=session.reconnects))?UPDATE_LOCKED:
            !StorageIdle()||g_storage_swd.state==STORAGE_SWD_BUSY?UPDATE_BUSY:UpdateService_Handle(update,f);
        if(r){uint32_t b[5]={r,update->state,update->transaction,update->received,update->verified};Reply(f->opcode,f->sequence,1,b,sizeof(b));}
    }else if(((f->opcode>=0x50&&f->opcode<=0x57)||f->opcode==0x59||(f->opcode>=0x80&&f->opcode<=0x83)||f->opcode==0x87||f->opcode==0x88||f->opcode==0x89||f->opcode==0x8a)&&store){
        uint32_t r=inspect_next<BS_FILE_COUNT||!UpdateIdle()||BootstrapStorage_BenchActive(store)?BS_BUSY:BootstrapStorage_Request(store,f->opcode,f->payload,f->length);
        if(!r){if(f->opcode==0x50||f->opcode==0x52||f->opcode==0x81){storage_started=1;BootstrapUI_WorkStarted(&ui);}operation_error=0;pending_seq=f->sequence;pending_op=f->opcode;pending_job=1;}
        else Reply(f->opcode,f->sequence,1,&r,4);
    }else if(f->opcode==0x48&&f->length==8){
        uint32_t action,token;memcpy(&action,f->payload,4);memcpy(&token,f->payload+4,4);
        uint32_t allowed=Authorized()&&UpdateIdle()&&StorageIdle();
        uint32_t r=(action==1||action==2)&&!allowed?BS_DENIED:AppRecovery_Control(action,token,f->sequence,HAL_GetTick());
        uint32_t out[8]={r,g_app_recovery.state,g_app_recovery.source_ready,g_app_recovery.verified,RECOVERY_STOCK_BYTES,g_app_recovery.error,3,0};Reply(f->opcode,f->sequence,r,out,sizeof(out));
    }else {uint32_t r=4;Reply(f->opcode,f->sequence,1,&r,4);}
}
/* Main's generated defaultTask still calls only LCDTest(). The Bootstrap link
 * supplies this implementation, leaving the retained Product test untouched. */
void LCDTest(void)
{
    BootstrapUI_Init(&ui);
    GateGesture_Init(&recovery_gesture,HAL_GetTick(),0);
    HealthService_Init(HAL_GetTick());
    HealthService_Register(HEALTH_STORAGE,HAL_GetTick());
    MX_DMA_Init();BSP_Power_Init(0);BSP_Buttons_Init();
    g_bootstrap.display=BSP_Display_Init();if(!g_bootstrap.display)(void)BSP_Display_SetBrightnessPercent(25);
    /* Submit the greeting as soon as EVE is usable, before NOR/radio/RAM init.
     * Continue immediately: watchdog, radio and file checks are never delayed. */
    if(!g_bootstrap.display){BootstrapUI_ShowWelcome(&ui,HAL_GetTick());Screen();last_screen=HAL_GetTick();}
    MX_SPI5_Init();g_bootstrap.nor=BSP_NOR_Init();
    g_bootstrap.bt=(uint32_t)Bluetooth_Start();(void)Bluetooth_SetPairingWindow(0);
    g_bootstrap.ram=BSP_RAM_Init();
    if(!g_bootstrap.ram&&!g_bootstrap.nor){
        store=BSP_RAM_Allocate(sizeof(*store));void *arena=BSP_RAM_Allocate(0x100000);
        const BootstrapStoragePlatform io={NULL,Read,Grant,Erase,Program,Lock,WritePower};
        if(store&&arena){BootstrapStorage_Init(store,&io,arena,0x100000,(const uint32_t*)0x1fff7a10);store->resident=(const uint8_t*)0x08000000U;store->read_embedded=EmbeddedRead;}
        else{store=NULL;g_bootstrap.error=BS_SPACE;}
        if(!RuntimeUpdate_Init()){update=RuntimeUpdate_GetService();if(update)update->platform.local_source=LocalProduct;}
        (void)StorageSWD_Init();
    }
    AmbientService_Init();
    NDCP_Init(&parser,Frame,NULL);
    HealthService_BootReady();
    for(;;){
        uint32_t now=HAL_GetTick();BSP_Buttons_Process();BSP_Power_Process(now);AppRecovery_IdentityProcess();
        BSP_Buttons_State enter;BSP_Buttons_GetState(BSP_BUTTON_ENTER,&enter);
        if(!local_recovery&&GateGesture_Process(&recovery_gesture,now,Ign(NULL),enter.raw_pressed)){
            local_recovery=1;drain_started=now;authorized_epoch=0;
            if(update)UpdateService_Authorize(update,0);
        }
        /* Bootstrap's explicit restore page accepts a fresh O hold without
         * an IGN cycle. Emergency key gesture above remains available outside
         * the menu. Both converge on the same owner-drain/reset path. */
        if(ui.state==BOOT_UI_RECOVERY&&!local_recovery){
            if(BootstrapConfirm_Process(&recovery_confirm,now,enter.raw_pressed)){
                local_recovery=1;drain_started=now;authorized_epoch=0;install_mode=0;
                if(update)UpdateService_Authorize(update,0);
            }
            ui.recovery_hold_ms=recovery_confirm.elapsed;
        }else{BootstrapConfirm_Init(&recovery_confirm);ui.recovery_hold_ms=0;}
        /* Refuse new work immediately. A bounded physical page completes first;
         * no interrupted FIFO command is mistaken for a durable transaction. */
        if(local_recovery==1){
            uint32_t drained=BootstrapRecovery_Drain(update,StorageIdle(),now-drain_started);
            if(drained==BOOT_DRAIN_READY)AppRecovery_RequestConfirmed();
            else if(drained==BOOT_DRAIN_UNRESOLVED){
                /* Pending/ambiguous commit is a terminal refusal, not work
                 * that will finish if we wait for a watchdog reset. */
                local_recovery=2;operation_error=BOOT_ERR_RECOVERY|1U;
            }else if(drained==BOOT_DRAIN_TIMEOUT){
                local_recovery=2;operation_error=BOOT_ERR_RECOVERY|2U;
            }
        }
        if(update&&InstallHold()&&!local_recovery){
            InstallSession_Button(&update->install,now,enter.raw_pressed);
            if(update->install.cancel_requested){
                uint32_t result=UpdateService_CancelUncommitted(update);
                if(result==UPDATE_PENDING){InstallSession_End(&update->install,INSTALL_UNKNOWN);update->install.error=UPDATE_PENDING;}
                else if(!bond_pending&&(!store||BootstrapStorage_Cancel(store))){
                    InstallSession_End(&update->install,INSTALL_CANCELLED);
                    install_mode=authorized_epoch=approval_epoch=approval_transaction=0;
                    pending=pending_job=sent_wait=0;NDCP_Init(&parser,Frame,NULL);
                    operation_error=0;BootstrapUI_Init(&ui);ui.state=BOOT_UI_READY;
                }
            }
        }
        BSP_Buttons_Event event;while(BSP_Buttons_GetEvent(&event)){
            if(event.type!=BSP_BUTTON_EVENT_SHORT_PRESS||(!Ign(NULL)&&!InstallHold())||local_recovery)continue;
            uint32_t action=BootstrapUI_Input(&ui,event.button==BSP_BUTTON_UP?-1:event.button==BSP_BUTTON_DOWN?1:0,event.button==BSP_BUTTON_ENTER,now);
            if(action==BOOT_ACTION_CANCEL&&update)update->install.cancel_requested=1;
            if(action==BOOT_ACTION_ALLOW_INSTALL&&update&&!bond_pending&&!update->local_install){
                approval_transaction=update->transaction;approval_epoch=session.reconnects;
                /* Pairing must survive the stock BL, which replaces SRAM.
                 * Publish the audited CFG key journal before local COMMIT. */
                uint32_t r=StorageIdle()?BootstrapBond_Begin(&bond,store):BS_BUSY;
                if(!r)bond_pending=1;
                if(r)operation_error=BOOT_ERR_UPDATE|r;
            }
            if(action==BOOT_ACTION_PAIR||action==BOOT_ACTION_BT_TEST){
                install_mode=action==BOOT_ACTION_PAIR;authorized_epoch=0;
                if(install_mode&&update)InstallSession_Begin(&update->install,now,now|1U);
                if(update)UpdateService_Authorize(update,0);
                permission_until=now+120000U;(void)Bluetooth_SetPairingWindow(120);
            }
            if((ui.state==BOOT_UI_READY&&!InstallHold())||action==BOOT_ACTION_RECOVERY){
                install_mode=0;authorized_epoch=0;
                if(update)UpdateService_Authorize(update,0);
            }
        }
        Bluetooth_LinkState current;Bluetooth_GetLinkState(BLUETOOTH_PHONE,&current);
        if(current.status!=BLUETOOTH_LINK_UP||current.reconnects!=session.reconnects||current.cid!=session.cid){
            if(update)UpdateService_SetConnected(update,0);
            session=current;pending=pending_job=authorized_epoch=sent_wait=0;ui.install_allowed=approval_transaction=approval_epoch=0;NDCP_Init(&parser,Frame,NULL);AppRecovery_ControlDisconnected();
            if(update)UpdateService_SetConnected(update,session.status==BLUETOOTH_LINK_UP);
        }
        if(store){
            uint32_t secure=session.status==BLUETOOTH_LINK_UP&&Bluetooth_LinkSecure(BLUETOOTH_PHONE,&session);
            if(!secure||(!Ign(NULL)&&!InstallHold())||local_recovery){ui.install_allowed=approval_transaction=approval_epoch=0;authorized_epoch=0;if(update)UpdateService_Authorize(update,0);}
            else if(install_mode&&(InstallHold()||(int32_t)(permission_until-now)>0))authorized_epoch=session.reconnects;
            /* Two-minute local window permits entry; a long backup keeps its
             * lease while this same encrypted session and IGN remain alive. */
            /* The startup file audit reuses the upload arena. Publish the
             * physical mailbox only after its final inspection has retired. */
            if(UpdateIdle()&&inspect_next>=BS_FILE_COUNT&&store->state!=BS_INSPECTING)BootstrapStorage_BenchProcess(store);
            if(!BootstrapStorage_BenchActive(store))BootstrapStorage_SetSession(store,session.status==BLUETOOTH_LINK_UP?session.reconnects:0,
                Authorized());
            if(bond_pending){
                if(BootstrapBond_Process(&bond,store)){
                    bond_pending=0;
                    uint32_t r=bond.error;
                    if(!r&&!local_recovery&&!update->install.cancel_requested)r=UpdateService_ConfirmLocal(update);
                    if(r)operation_error=BOOT_ERR_UPDATE|r;
                }
            }else if(g_storage_swd.state!=STORAGE_SWD_BUSY){
                /* Bounded work budget, not a 5ms sleep after each 256B page.
                 * Yield to BT/IGN/watchdog after at most eight operations/2ms. */
                uint32_t began=HAL_GetTick();
                for(uint32_t slice=0;slice<8;slice++){
                    BootstrapStorage_Process(store);
                    if(store->reply_pending||store->state==BS_IDLE||store->state==BS_SAVED||store->state==BS_FAILED||store->state==BS_PREPARED||HAL_GetTick()-began>=2)break;
                }
            }
            if(!bond_restored&&store->kind==BS_CONFIG&&store->state==BS_SAVED){
                (void)BootstrapBond_Restore(store);bond_restored=1;
            }
            g_bootstrap.stage=store->state;g_bootstrap.error=store->error;
            if(!pending_job&&UpdateIdle()&&!BootstrapStorage_BenchActive(store)&&inspect_next<BS_FILE_COUNT&&(store->state==BS_IDLE||store->state==BS_SAVED||store->state==BS_FAILED)){
                if(!BootstrapStorage_Inspect(store,inspect_next))++inspect_next;
            }
            if(pending_job&&!pending&&!sent_wait){uint8_t response[1024];uint32_t n=0;if(!BootstrapStorage_TakeReply(store,response,sizeof(response),&n)){
                uint32_t error=BS_FORMAT;if(n>=4)memcpy(&error,response,4);
                operation_error=error?BOOT_ERR_STORAGE|error:0;Reply(pending_op,pending_seq,error,response,n);pending_job=0;}}
            /* Read-only SWD shares this storage owner. Never read half-written
             * metadata as a completed stable diagnostic snapshot. */
            if(!bond_pending&&store->state!=BS_WRITING&&UpdateIdle())StorageSWD_Process();
        }
        if(update){RuntimeUpdate_Process(now);if(!pending&&!sent_wait)pending=(uint32_t)UpdateService_TakeReply(update,tx,sizeof(tx));}
        if(session.status==BLUETOOTH_LINK_UP){
            if(sent_wait&&!current.tx_queued){
                if(sent_opcode>=0x40&&sent_opcode<=0x47&&update)UpdateService_NotifyReplyTransmitted(update,sent_sequence,now);
                if(sent_opcode==0x48){AppRecovery_ReplySent(sent_sequence,now);}
                sent_wait=0;
            }
            if(pending&&!sent_wait&&!Bluetooth_SendSession(BLUETOOTH_PHONE,&session,tx,pending)){
                memcpy(&sent_sequence,tx+8,4);sent_opcode=tx[5];sent_wait=1;pending=0;++g_bootstrap.replies;}
            if(!pending&&!pending_job&&!sent_wait){uint8_t rx[512];int n=Bluetooth_ReceiveSession(BLUETOOTH_PHONE,&session,rx,sizeof(rx));if(n>0)NDCP_Feed(&parser,rx,n,now);}
        }
        NDCP_Poll(&parser,now);AppRecovery_RuntimeProcess(now,StorageIdle()&&UpdateIdle());
        uint32_t phase=store?store->state:0,position=store?store->position:0,total=store?store->bytes:0;
        uint32_t error=g_bootstrap.bt||g_bluetooth.state==BLUETOOTH_STATE_FAULT?BOOT_ERR_BT|(g_bluetooth.last_error&65535):g_bootstrap.nor?BOOT_ERR_NOR|(g_bootstrap.nor&65535):g_bootstrap.ram||!store||!update?BOOT_ERR_RAM:g_bootstrap.display?BOOT_ERR_DISPLAY:operation_error;
        if(!error&&display_runtime_error)error=BOOT_ERR_DISPLAY|display_runtime_error;
        if(!error&&storage_started&&store&&store->state==BS_FAILED)error=BOOT_ERR_STORAGE|store->error;
        uint32_t busy=!StorageIdle()||!UpdateIdle();
        if(store&&(phase==BS_BACKUP_A||phase==BS_BACKUP_B)){position=store->backup_position;total=0x8000000U;}
        if(phase==BS_SCOPE_HASH)total=0x8000000U;
        if(phase==BS_SCOPE_AUDIT)total=0;
        if(store&&phase==BS_WRITING&&(store->phase==0||store->phase==4||store->phase==5))total=0x9000U;
        if(store&&phase==BS_WRITING&&store->phase==7)total=4096U;
        if(store&&store->resource_update)total=BootstrapResources_ProgressTotal(store);
        if(update){
            if(update->state==UPDATE_RECEIVING||update->state==UPDATE_VERIFYING){phase=update->state==UPDATE_RECEIVING?BS_UPLOADING:BS_VERIFYING;position=update->state==UPDATE_RECEIVING?update->received:update->verified;total=UpdateService_ImageBytes(update);}
            if(update->state==UPDATE_VERIFIED){phase=100;busy=0;}
            if((update->local_install&&update->state!=UPDATE_FAILED)||update->state==UPDATE_COMMITTED||update->state==UPDATE_RESET_WAIT)phase=101;
            if(update->state==UPDATE_FAILED)error=BOOT_ERR_UPDATE|update->result;
        }
        if(local_recovery||AppRecovery_RuntimeRequested()){phase=103;busy=1;position=now-drain_started;total=60000;}
        if(local_recovery==2){phase=104;busy=0;error=operation_error;}
        ui.commit_phase=g_runtime_update.commit_phase;ui.commit_result=g_runtime_update.commit_result;
        ui.bt_state=g_bluetooth.state;ui.bt_error=g_bluetooth.last_error;
        ui.bt_secure=session.status==BLUETOOTH_LINK_UP&&Bluetooth_LinkSecure(BLUETOOTH_PHONE,&session);
        ui.requests=g_bootstrap.requests;ui.replies=g_bootstrap.replies;
        BootstrapUI_Context(&ui,store&&(inspect_next<BS_FILE_COUNT||store->state==BS_INSPECTING),
            inspect_next-(store&&store->state==BS_INSPECTING&&inspect_next?1U:0U),install_mode);
        BootstrapUI_Update(&ui,now,Ign(NULL),session.status==BLUETOOTH_LINK_UP,phase,position,total,busy,error);
        BootstrapUI_Track(&ui,now,store?store->phase:0,store?store->kind:0xffffffffU);
        /* A responsive loop can still have a stalled storage state machine.
         * Stop between completed NOR calls, revoke proof, preserve evidence.
         * Committed updater/reboot phases are never cancelled by this guard. */
        if(store&&storage_started&&ui.stalled&&now-ui.mark>=120000U&&UpdateIdle()){
            BootstrapStorage_Timeout(store);operation_error=BOOT_ERR_STORAGE|BS_TIMEOUT;
            install_mode=authorized_epoch=0;
        }
        if(update&&InstallHold()){
            uint32_t state=update->commit_uncertain?INSTALL_UNKNOWN:error?INSTALL_ERROR:phase==101?INSTALL_COMMIT:phase==100?INSTALL_CONFIRM:INSTALL_TRANSFER;
            uint32_t kind=store?store->kind:9;
            static const uint8_t order[9]={1,2,3,4,0,6,7,8,5};
            uint32_t app=update->state==UPDATE_RECEIVING||update->state==UPDATE_VERIFYING||phase==100||phase==101;
            uint32_t checked=app?update->verified:store&&store->state==BS_SAVED?store->bytes:
                store&&store->state==BS_WRITING&&store->phase==2?store->position:0;
            InstallSession_Observe(&update->install,now,state,phase,app?0:kind<9?order[kind]:9,app?1:9,position,total,
                checked,error,session.status==BLUETOOTH_LINK_UP);
            ui.cancel_pending=update->install.state==INSTALL_CANCELLING;
            InstallSession_Snapshot(&update->install,ui.progress);
        }
        /* Diagnostic sampling shares the same bounded service as Product.
         * No I2C probe runs inside an installation or recovery transaction. */
        if(ui.state==BOOT_UI_AMBIENT&&!InstallHold()&&!local_recovery&&StorageIdle()&&UpdateIdle())AmbientService_Process(now);
        if(now-last_screen>=(ui.state==BOOT_UI_AMBIENT?500U:200U)){
            Ambient_Snapshot light;AmbientService_GetSnapshot(&light);
            ui.ambient[0]=light.driver.manufacturer;ui.ambient[1]=light.driver.device;
            ui.ambient[2]=light.driver.raw;ui.ambient[3]=light.driver.millilux;
            ui.ambient[4]=light.driver.valid&&!light.stale;ui.ambient[5]=light.sample_age_ms;
            ui.ambient[6]=light.driver.error;ui.ambient[7]=light.driver.phase;
            ui.ambient[8]=g_bsp_backlight.percent;ui.ambient[9]=g_bsp_backlight.compare;ui.ambient[10]=g_bsp_backlight.period+1;
            RadioSelfTestState radio=g_radio_self_test;
            ui.radio[0]=radio.bytes;ui.radio[1]=radio.crc;ui.radio[2]=radio.elapsed;
            ui.radio[3]=radio.complete&&radio.epoch==session.reconnects&&ui.bt_secure;
            ui.radio[4]=radio.errors;Screen();last_screen=now;
        }
        g_bootstrap.maintenance_until=permission_until;++g_bootstrap.heartbeat;BSP_BringupSample();
        HealthService_Progress(HEALTH_STORAGE,HAL_GetTick());
        (void)HealthService_Process(HAL_GetTick(),0);osDelay(5);
    }
}
/* Profile-specific hardware callbacks have one consumer each. Product's
 * multi-service IRQ dispatcher is not linked into the installer image. */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *u){BSP_BT_HCI_OnRxComplete(u);}
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *u){BSP_BT_HCI_OnTxComplete(u);}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *u){BSP_BT_HCI_OnError(u);}
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *s){BSP_NOR_OnTxRxComplete(s);}
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *s){BSP_NOR_OnError(s);}
void USART1_IRQHandler(void){HAL_UART_IRQHandler(&huart1);}
void BSP_Display_CaptureInvalidate(void){}
/* Installer stays awake while it owns transfers. No fictitious STOP ACKs. */
uint32_t PowerService_Mode(void){return POWER_RUN;}
void PowerService_Acknowledge(uint32_t owner,uint32_t ready){(void)owner;(void)ready;}
void PowerService_IgnitionIRQ(void){}
