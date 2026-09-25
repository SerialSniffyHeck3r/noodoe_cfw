#include "App_Recovery.h"
#include "BootStore.h"
#include "gate_abi.h"
#include "event_log.h"
#include "NoodoeBluetooth.h"
#include "NoodoeControl.h"
#include "SystemError.h"
#include "bsp_fault.h"
#include "Config_Store.h"
#include "Photo_Store.h"
#include <string.h>
volatile uint32_t failure,checks,confirmed,reset_finished,trial_view,durable;
volatile Bluetooth_Diagnostics g_bluetooth;
volatile NoodoeControl_Diagnostics g_noodoe_control;
volatile BSP_FaultStatus g_bsp_fault;
volatile SystemErrorDiagnostics g_system_error;
volatile BootStoreDiagnostics g_boot_store;
static GateJournalRecord record;
#define CHECK(x) do{checks++;if(!(x)){failure=__LINE__;return;}}while(0)
void BootStore_Process(void){}
uint32_t BootStore_GetBootInfo(GateJournalRecord *r){*r=record;return 1;}
uint32_t BootStore_MatchTrial(uint32_t s,const uint8_t sha[32]){return s==record.sequence&&!memcmp(sha,record.active_sha,32)&&(record.flags&GATE_F_TRIAL);}
void BootStore_RequestConfirm(void){confirmed++;record.flags=GATE_F_RESET_PENDING;record.reset_epoch=record.sequence;}
uint32_t BootStore_FinishSettingsReset(uint32_t epoch){if(epoch!=record.reset_epoch)return CFW_ARGUMENT;reset_finished++;record.flags=0;record.state=GATE_J_CONFIRMED;return 0;}
void ConfigStore_SetTrialView(uint32_t v){trial_view=v;}
uint32_t ConfigStore_ResetPreferences(uint32_t epoch,uint32_t *revision){*revision=epoch;return 0;}
uint32_t ConfigStore_Result(uint32_t revision){(void)revision;return durable?CFW_OK:CFW_PENDING;}
uint32_t PhotoStore_ResetSlots(void){return CFW_OK;}
extern uint32_t PowerService_DeepAllowed(void);
extern void HealthService_StableBoot(void);
static void Init(void){record.sequence=7;record.state=GATE_J_BOOT_PENDING;record.flags=GATE_F_TRIAL;
 GateRetained r;GateRetained_Init(&r);r.sequence=7;r.crc=GatePolicy_Crc(&r,24);r.inverse=~r.crc;memcpy((void*)&g_recovery_mailbox.request,&r,sizeof(r));
 GateBootContext c={.sequence=7,.flags=GATE_F_TRIAL,.transaction=99,.generation=2};GateBootContext_Seal(&c);memcpy((void*)&g_recovery_mailbox.boot,&c,sizeof(c));
 g_bluetooth.state=BLUETOOTH_STATE_READY;AppRecovery_EarlyRun();}
void Test_Confirm(void){Init();CHECK(trial_view&&!PowerService_DeepAllowed());
 CHECK(AppRecovery_TrialPhase()==TRIAL_PHONE);
 g_bluetooth.state=BLUETOOTH_STATE_OFF;CHECK(AppRecovery_TrialPhase()==TRIAL_RADIO);g_bluetooth.state=BLUETOOTH_STATE_READY;
 g_noodoe_control.connected=1;g_noodoe_control.link_generation=5;
 CHECK(AppRecovery_TrialPhase()==TRIAL_SCREEN);
 AppRecovery_StorageProcess();CHECK(!confirmed);HealthService_StableBoot();AppRecovery_StorageProcess();CHECK(!confirmed);
 CHECK(!AppRecovery_ConfirmTrial(6,record.active_sha,5));CHECK(!AppRecovery_ConfirmTrial(7,record.active_sha,0));
 CHECK(!AppRecovery_ConfirmTrial(7,record.active_sha,5));
 CHECK(!AppRecovery_VisualConfirm(6,record.active_sha,5));CHECK(!AppRecovery_VisualConfirm(7,record.active_sha,0));
 CHECK(AppRecovery_VisualConfirm(7,record.active_sha,5));CHECK(!AppRecovery_ConfirmTrial(7,record.active_sha,6));
 CHECK(AppRecovery_ConfirmTrial(7,record.active_sha,5));CHECK(AppRecovery_TrialPhase()==TRIAL_SAVING);g_noodoe_control.connected=1;g_noodoe_control.link_generation=6;CHECK(AppRecovery_TrialPhase()==TRIAL_SCREEN);AppRecovery_StorageProcess();CHECK(!confirmed);
 g_noodoe_control.link_generation=5;g_system_error.active=1;CHECK(AppRecovery_TrialPhase()==TRIAL_ERROR);AppRecovery_StorageProcess();CHECK(!confirmed);
 g_system_error.active=0;AppRecovery_StorageProcess();CHECK(confirmed==1);
 AppRecovery_StorageProcess();CHECK(!reset_finished);durable=1;AppRecovery_StorageProcess();CHECK(reset_finished==1);
 AppRecovery_TrialTick(181000,1);CHECK(PowerService_DeepAllowed());
}
/* Reproduces the user's state: healthy firmware and a connected phone are
 * still NOT acknowledged. Presentation must request action, not say checking. */
void Test_LinkWithoutApproval(void){Init();g_noodoe_control.connected=1;g_noodoe_control.link_generation=8;
 HealthService_StableBoot();AppRecovery_TrialTick(100,1);
 for(uint32_t t=100;t<180000;t+=1000){AppRecovery_StorageProcess();AppRecovery_TrialTick(t,1);CHECK(!confirmed);CHECK(AppRecovery_TrialPhase()==TRIAL_SCREEN);}
 CHECK(AppRecovery_VisualConfirm(7,record.active_sha,8));CHECK(AppRecovery_TrialPhase()==TRIAL_SCREEN);
 CHECK(AppRecovery_ConfirmTrial(7,record.active_sha,8));CHECK(AppRecovery_TrialPhase()==TRIAL_SAVING);
 AppRecovery_StorageProcess();CHECK(confirmed==1);AppRecovery_StorageProcess();CHECK(!reset_finished);
 durable=1;AppRecovery_StorageProcess();CHECK(reset_finished==1);AppRecovery_TrialTick(180001,1);CHECK(PowerService_DeepAllowed());
}
void Test_ApprovalBeforeHealthy(void){Init();g_noodoe_control.connected=1;g_noodoe_control.link_generation=8;
 CHECK(AppRecovery_VisualConfirm(7,record.active_sha,8));CHECK(AppRecovery_ConfirmTrial(7,record.active_sha,8));
 CHECK(AppRecovery_TrialPhase()==TRIAL_HEALTH);AppRecovery_StorageProcess();CHECK(!confirmed);
 g_boot_store.error=CFW_IO;CHECK(AppRecovery_TrialPhase()==TRIAL_ERROR);HealthService_StableBoot();AppRecovery_StorageProcess();CHECK(!confirmed);
 g_boot_store.error=0;AppRecovery_StorageProcess();CHECK(confirmed==1);CHECK(AppRecovery_TrialPhase()==TRIAL_SAVING);
}
void Test_Timeout(uint32_t mode){Init();AppRecovery_TrialTick(100,1);AppRecovery_TrialTick(179999,1);CHECK(!confirmed);
 if(mode==3){g_noodoe_control.connected=1;g_noodoe_control.link_generation=8;HealthService_StableBoot();AppRecovery_StorageProcess();CHECK(!confirmed);CHECK(AppRecovery_TrialPhase()==TRIAL_SCREEN);}
 if(mode==1)g_bluetooth.state=BLUETOOTH_STATE_FAULT;
 if(mode==2)g_system_error.active=1;
 AppRecovery_TrialTick(180101,0);CHECK(!confirmed);AppRecovery_TrialTick(180102,1);failure=__LINE__;
}

