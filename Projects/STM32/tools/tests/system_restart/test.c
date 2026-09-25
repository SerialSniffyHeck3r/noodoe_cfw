#include "App_Settings.h"
#include "App_Persistence.h"
#include "BootStore.h"
#include "Config_Store.h"
static GateJournalRecord journal;
volatile ConfigStoreStatus g_config_store;
volatile uint32_t failure,checks;
static uint32_t ready=1,ride=CFW_PENDING,cfg=CFW_PENDING,resets;
#define CHECK(x) do{checks++;if(!(x)){failure=__LINE__;return;}}while(0)
uint32_t BootStore_GetBootInfo(GateJournalRecord *j){*j=journal;return ready;}
uint32_t RideStore_RequestCheckpoint(uint32_t *id){*id=17;return 0;}
uint32_t RideStore_GetResult(uint32_t id){return id==17?ride:CFW_ARGUMENT;}
uint32_t ConfigStore_Result(uint32_t rev){(void)rev;return cfg;}
void Test_Reset(void){resets++;}
void Test(void){
 journal.state=GATE_J_BOOT_PENDING;journal.flags=GATE_F_TRIAL;
 CHECK(SettingsSystem_Restart(0)==APP_SETTINGS_UNAVAILABLE);SettingsSystem_Process(5000);CHECK(!resets);
 journal.state=GATE_J_CONFIRMED;journal.flags=0;
 CHECK(SettingsSystem_Restart(100)==APP_SETTINGS_BUSY);SettingsSystem_Process(5000);CHECK(!resets);
 ride=0;SettingsSystem_Process(5000);CHECK(!resets);
 cfg=0;SettingsSystem_Process(1099);CHECK(!resets);SettingsSystem_Process(1100);CHECK(resets==1);
 CHECK(SettingsSystem_Restart(10200)==APP_SETTINGS_DEVICE_ERROR);SettingsSystem_Process(10300);CHECK(resets==1);
 CHECK(SettingsSystem_Restart(11000)==APP_SETTINGS_BUSY);ride=CFW_CORRUPT;SettingsSystem_Process(12500);CHECK(resets==1);
 CHECK(SettingsSystem_Restart(12501)==APP_SETTINGS_DEVICE_ERROR);
}
