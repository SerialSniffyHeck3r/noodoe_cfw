#include "NoodoeRuntime.h"
#include "RoutineUpdate.h"
#include "Health_Service.h"
#include "BSP_Watchdog.h"
#include "PowerService.h"
#include "BSP_Dash.h"
#include "AmbientService.h"
#include "BSP_Clock.h"
#include "ClockService.h"
#include "DashService.h"
#include "NoodoeBluetooth.h"
#include "StorageBackup.h"
#include "StorageService.h"
#include "StorageSWD.h"
#include "PhotoService.h"
#if NOODOE_PRODUCT
#include "PhoneVisual.h"
#endif
#include "BSP_RAM.h"
#include "Resources.h"
#include "ResourceStore.h"
#include "SystemError.h"
#include "BSP_Power.h"
#include "NoodoeControl.h"
#include "RuntimeUpdate.h"
#include "SettingsService.h"
#include "App_Settings.h"
#include "OilUsageService.h"
#if NOODOE_PRODUCT
#include "Cfw_Store.h"
#include "Config_Store.h"
#include "App_Persistence.h"
#include "Photo_Store.h"
#include "Cfw_Install.h"
#include "App_Recovery.h"
#include "DeviceLog.h"
#endif
#include "dma.h"
#include "spi.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include <string.h>

volatile NoodoeRuntime_Diagnostics g_noodoe_runtime;
static GnssService gnss;
static GnssSnapshot gnss_snapshot;
static uint32_t snapshots_valid;
static uint32_t storage_initialized;
#if NOODOE_PRODUCT
/* State transitions, not frames or GPS samples. Queue coalescing handles
 * repeated identical errors without logging private phone content. */
static void RecordTransitions(uint32_t now)
{
 static uint32_t previous[6];
 uint32_t value[6]={g_runtime_update.state,g_runtime_update.service_result,g_bluetooth.state,
     g_bluetooth.last_error,g_config_store.error,g_app_persistence.ride_status};
 const uint32_t code[6]={LOG_UPDATE,LOG_UPDATE,LOG_BT,LOG_BT,LOG_STORAGE,LOG_STORAGE};
 for(uint32_t i=0;i<6;i++)if(previous[i]!=value[i]){
  DeviceEvent e={.code=code[i],.detail=value[i],.boot=g_device_log.boot_id,.time_ms=now};
  e.data[0]=i;e.data[1]=previous[i];
  UpdateService *u=RuntimeUpdate_GetService();if(code[i]==LOG_UPDATE&&u)e.transaction=u->transaction;
  if(DeviceLog_Post(&e))previous[i]=value[i];
 }
}
#endif
static NoodoeSystemSnapshot system_snapshot;

/* A separate task checks completed owner iterations. Its own timer is not
 * sufficient evidence: any stalled mandatory owner stops hardware refresh. */
static void HealthTask(void *argument)
{
    (void)argument;
    for(;;){
        (void)HealthService_Process(HAL_GetTick(),PowerService_Mode()!=POWER_RUN);
        (void)osDelay(100U);
    }
}

/* Primary-phone control runs on this same I/O owner. Phone fixes retain their
 * validated monotonic timestamps; no external NMEA stream is connected. */
static uint32_t PhoneGPS(void *context,const GnssFix *fix,uint32_t now_ms)
{ return GnssService_UpdatePhone((GnssService *)context,fix,now_ms); }

/* Protocol state has one owner. Publish complete snapshots in a short
 * interrupt-masked copy so upper layers never read a partially updated fix. */
static void Publish(uint32_t now)
{
    GnssSnapshot g;
    GnssService_GetSnapshot(&gnss,now,&g);
    Bluetooth_LinkState phone={0};
    uint32_t links=0U;
    if(Bluetooth_GetLinkState(BLUETOOTH_PHONE,&phone)==BLUETOOTH_OK&&phone.status==BLUETOOTH_LINK_UP)links|=1U;
    taskENTER_CRITICAL();
    uint32_t changed=system_snapshot.ign_valid!=g_bsp_power.ign_valid||system_snapshot.ign_on!=g_bsp_power.ign_on;
    gnss_snapshot=g; snapshots_valid=1U;
    system_snapshot.power_ms=now;system_snapshot.ign_valid=g_bsp_power.ign_valid;
    system_snapshot.ign_on=g_bsp_power.ign_on;system_snapshot.links=links;
    taskEXIT_CRITICAL();
    if(changed)PowerService_Notify(POWER_OWNER_GRAPHICS);
}

/* One phone owns its NDCP stream. Phone GPS and vehicle UART remain separate. */
static void IoTask(void *argument)
{
    (void)argument;
    uint32_t last_publish=0U,control_initialized=0U;
    GnssService_Init(&gnss,3000U);
    g_noodoe_runtime.dash_result=DashService_Init(HAL_GetTick());
    for (;;) {
        uint32_t now=HAL_GetTick();
        BSP_Power_Process(now);
#if NOODOE_PRODUCT
        /* A key transition is not an MCU reset. Observe the qualified local
         * recovery gesture even while the rest of IO is in deep standby. */
        AppRecovery_RuntimeButtons(now);
#endif
        PowerService_Acknowledge(POWER_OWNER_IO,0);
        if(PowerService_Mode()==POWER_DEEP){
            uint32_t ready=BSP_Dash_SetSleeping(1)==0;
            PowerService_Acknowledge(POWER_OWNER_IO,ready);
            Publish(now);++g_noodoe_runtime.io_heartbeat;g_noodoe_runtime.io_ms=now;
            HealthService_Progress(HEALTH_IO,HAL_GetTick());
            /* Raw ON is debounced promptly, even if the previous policy is
             * still deep. GPIO wake does not depend on UART byte reception. */
            PowerService_Wait(POWER_OWNER_IO,g_bsp_power.raw_ign_off?250U:12U);
            continue;
        }
        /* The vehicle is off; stop its RX/TX DMA while retaining the separate
         * Bluetooth transport. Raw IGN ON restores the dashboard immediately,
         * including during the short debounce before UI policy catches up. */
        uint32_t retained=PowerService_Mode()==POWER_DISPLAY_SLEEP&&g_bsp_power.raw_ign_off;
        (void)BSP_Dash_SetSleeping(retained);
        /* The low-priority storage worker publishes only after NOR/RAM and
         * updater setup. A failed updater leaves ordinary control available;
         * no PHONE packet can access an uninitialized platform or buffer. */
        if(!control_initialized && __atomic_load_n(&storage_initialized,__ATOMIC_ACQUIRE)) {
            NoodoeControl_Init(RuntimeUpdate_GetService());
            NoodoeControl_SetPhoneGPSCallback(PhoneGPS,&gnss);
            control_initialized=1U;
        }
        if(control_initialized)NoodoeControl_Process(now);
        OilUsageService_Process(now,g_bsp_power.ign_valid,g_bsp_power.ign_on,__atomic_load_n(&storage_initialized,__ATOMIC_ACQUIRE));
        if(!retained)DashService_Process(now);
        if((uint32_t)(now-last_publish)>=100U||system_snapshot.ign_on!=g_bsp_power.ign_on||
           system_snapshot.ign_valid!=g_bsp_power.ign_valid){Publish(now);last_publish=now;}
        ++g_noodoe_runtime.io_heartbeat;g_noodoe_runtime.io_ms=now;
        HealthService_Progress(HEALTH_IO,HAL_GetTick());
        g_noodoe_runtime.io_stack_free=uxTaskGetStackHighWaterMark(NULL)*sizeof(StackType_t);
        PowerService_Wait(POWER_OWNER_IO,PowerService_Mode()==POWER_RUN?2U:g_bsp_power.raw_ign_off?100U:12U);
    }
}

static void PublishClock(uint32_t now)
{
            BSP_Clock_Time time={0};
            uint32_t result=g_noodoe_runtime.clock_result?BSP_CLOCK_NOT_READY:BSP_Clock_Read(&time);
            /* Publish complete calendar fields only after the serialized read.
             * UI observes failure/age, never a mixture across a minute rollover. */
            taskENTER_CRITICAL();
            uint32_t changed=system_snapshot.clock_valid!=(result==BSP_CLOCK_OK&&time.valid)||
                system_snapshot.hour!=time.hour||system_snapshot.minute!=time.minute;
            system_snapshot.clock_ms=now;system_snapshot.clock_valid=result==BSP_CLOCK_OK&&time.valid;
            system_snapshot.year=time.year;system_snapshot.month=time.month;system_snapshot.day=time.day;
            system_snapshot.weekday=time.weekday;
            system_snapshot.hour=time.hour;system_snapshot.minute=time.minute;system_snapshot.second=time.second;
            taskEXIT_CRITICAL();
            if(changed)PowerService_Notify(POWER_OWNER_GRAPHICS);
}

/* All SPI5/NOR access belongs to this worker. SWD reads a bounded SDRAM
 * snapshot while display and HCI run in independent tasks. A missing
 * ALS is serviced through its request/state layer with finite recovery retries;
 * no upper/UI call performs I2C. RTC is never reset here. */
static void StorageTask(void *argument)
{
    (void)argument;
    MX_SPI5_Init();
    g_noodoe_runtime.storage_result=StorageTransport_Init();
    if(!g_noodoe_runtime.storage_result)(void)StorageService_Init();
    /* Import only a validated device/layout-bound settings record before HCI
     * starts pairing. Invalid or legacy media stays untouched and unprovisioned. */
#if !NOODOE_PRODUCT
    (void)SettingsService_Init();
#endif
#if !NOODOE_PRODUCT
    g_noodoe_runtime.bluetooth_result=(uint32_t)Bluetooth_Start();
    (void)Bluetooth_SetPairingWindow(120U);
    if(g_noodoe_runtime.bluetooth_result)__atomic_fetch_or(&g_noodoe_runtime.start_error,4U,__ATOMIC_RELAXED);
#endif
    /* RAM-only registration queues the default sensor probe. The worker below
     * owns its finite bus operations; failure no longer disables all later
     * recovery. Pending is distinct from a completed successful transaction. */
    AmbientService_Init();
    g_noodoe_runtime.ambient_result=UINT32_MAX;
    g_noodoe_runtime.clock_result=BSP_Clock_Init();
    (void)BSP_RAM_Init();
#if NOODOE_PRODUCT
    CfwStore_Init();
    (void)Resources_RequestLoad();
#endif
    /* The remote bridge borrows only its own tested SDRAM allocation. It is
     * idle until an explicit mailbox request and never changes NOR contents. */
    (void)StorageSWD_Init();
    (void)RuntimeUpdate_Init();
    for(uint32_t slot=0;slot<PHOTO_SLOTS;++slot)(void)PhotoService_RequestLoad(slot);
    __atomic_store_n(&storage_initialized,1U,__ATOMIC_RELEASE);
    uint32_t last_clock=HAL_GetTick();
#if NOODOE_PRODUCT
    uint32_t bt_started=0;
    uint32_t recovery_lease=0;
#endif
    for (;;) {
        uint32_t clock_now=HAL_GetTick();
#if NOODOE_PRODUCT
        AppRecovery_IdentityProcess();
        /* A confirmed return-to-stock drains this boot, then reboots to the
         * pre-RTOS recovery owner. It never borrows the SWD caller's lease. */
        if(AppRecovery_RuntimeRequested()&&!g_cfw_quiesce.request){g_cfw_quiesce.request=0x52454331U;recovery_lease=1;}
        if(!AppRecovery_RuntimeRequested()&&recovery_lease){
            if(g_cfw_quiesce.request==0x52454331U){g_cfw_quiesce.request=0;g_cfw_quiesce.ack=0;}recovery_lease=0;}
#endif
        PowerService_Acknowledge(POWER_OWNER_STORAGE,0);
        uint32_t update_busy=g_runtime_update.state==UPDATE_RECEIVING||g_runtime_update.state==UPDATE_VERIFYING||g_runtime_update.state==UPDATE_RESET_WAIT;
        uint32_t resource_busy=ResourceStore_Busy()||g_resource_install.sequence!=g_resource_install.ack||Resources_GetStatus()==RESOURCES_QUEUED||Resources_GetStatus()==RESOURCES_HEADER||Resources_GetStatus()==RESOURCES_LOADING;
        uint32_t busy=resource_busy||update_busy||g_storage_swd.state==STORAGE_SWD_BUSY||g_photos.active||g_photos.pending||g_photo_import.sequence!=g_photo_import.ack;
#if NOODOE_PRODUCT
        busy|=CfwStore_Busy()||ConfigStore_Busy()||AppPersistence_Busy()||PhotoStore_Busy()||CfwInstall_Busy()||DeviceLog_Busy();
        busy|=g_cfw_quiesce.request;
        if(!g_cfw_quiesce.request)g_cfw_quiesce.ack=0;
#endif
        uint32_t quiet=PowerService_Mode()>=POWER_DISPLAY_SLEEP;
        if(!AmbientService_SetSleeping(quiet)){
            AmbientService_Process(clock_now);busy=1;
        }
        if(PowerService_Mode()==POWER_DEEP&&!busy){
            if(clock_now-last_clock>=1000U){PublishClock(clock_now);last_clock=clock_now;}
            PowerService_Acknowledge(POWER_OWNER_STORAGE,1U);
            ++g_noodoe_runtime.storage_heartbeat;g_noodoe_runtime.storage_ms=clock_now;
            HealthService_Progress(HEALTH_STORAGE,HAL_GetTick());
            PowerService_Wait(POWER_OWNER_STORAGE,1000U);continue;
        }
#if NOODOE_PRODUCT
        if(!CfwInstall_Busy()){
#endif
            StorageSWD_Process();
#if NOODOE_PRODUCT
        }
#endif
#if NOODOE_PRODUCT
        /* Audited steady writes remain bounded and precede photo transfers.
         * Backup/update owns the medium exclusively; delayed checkpoints are
         * reported, never acknowledged as saved. */
        if(!update_busy&&g_storage_swd.state!=STORAGE_SWD_BUSY&&!ResourceStore_Busy()){
            /* Finish the current bounded transaction before accepting an
             * installer request. Pausing a photo here would deadlock both
             * owners: the installer must never acquire its active medium. */
            if(CfwInstall_Busy()&&g_cfw_install.state!=1){
                if(CfwStore_Busy())CfwStore_Process(clock_now);
                else if(PhotoStore_Busy())PhotoStore_Process(clock_now);
                else if(DeviceLog_Busy())DeviceLog_Process(clock_now,1);
            }
            if(!CfwStore_Busy()&&!PhotoStore_Busy()&&!DeviceLog_Busy())CfwInstall_Process();
            if(!CfwInstall_Busy()){
            CfwStore_Process(clock_now);
            if(!g_cfw_quiesce.request){ConfigStore_Process(clock_now);AppPersistence_Process(clock_now);}
            if(!CfwStore_Busy()&&(g_cfw_quiesce.request||!AppPersistence_Busy()))PhotoStore_Process(clock_now);
            /* Durable boot confirmation is a storage writer too. It cannot
             * run inside an acknowledged backup lease or between another
             * journal/photo/install transaction's bounded steps. */
            if(!g_cfw_quiesce.request&&!CfwStore_Busy()&&!PhotoStore_Busy()&&
                !AppPersistence_Busy()&&!CfwInstall_Busy()&&!resource_busy&&
                g_photo_import.sequence==g_photo_import.ack)
                {AppRecovery_StorageProcess();RoutineUpdate_Process();SettingsSystem_Process(clock_now);}
            if(!CfwStore_Busy()&&!PhotoStore_Busy()&&!AppPersistence_Busy()&&!resource_busy&&
                g_photo_import.sequence==g_photo_import.ack&&(!g_cfw_quiesce.request||DeviceLog_Busy()))
                DeviceLog_Process(clock_now,PowerService_Mode()==POWER_DEEP);
            }
        }
        if(!CfwInstall_Busy()&&!update_busy&&g_storage_swd.state!=STORAGE_SWD_BUSY){
            if((!g_cfw_quiesce.request||ResourceStore_Busy())&&!CfwStore_Busy()&&!PhotoStore_Busy()&&!DeviceLog_Busy())ResourceStore_Process();
            if(!ResourceStore_Busy())Resources_Process();
        }
        if(!bt_started&&g_settings.initialized&&Resources_GetStatus()==RESOURCES_READY&&!SystemError_GetStatus(NULL)){
            bt_started=1;
    g_noodoe_runtime.bluetooth_result=(uint32_t)Bluetooth_Start();
    (void)Bluetooth_SetPairingWindow(180U);
    if(g_noodoe_runtime.bluetooth_result)__atomic_fetch_or(&g_noodoe_runtime.start_error,4U,__ATOMIC_RELAXED);
        }
#endif
        uint32_t photos_allowed=1;
#if NOODOE_PRODUCT
        photos_allowed=!CfwInstall_Busy()&&!CfwStore_Busy()&&!AppPersistence_Busy();
        if(g_cfw_quiesce.request)photos_allowed=g_photo_import.sequence!=g_photo_import.ack;
#endif
        if(photos_allowed&&!update_busy&&!ResourceStore_Busy()&&Resources_GetStatus()!=RESOURCES_FAILED&&g_storage_swd.state!=STORAGE_SWD_BUSY)PhotoService_Process();
#if NOODOE_PRODUCT
        if(photos_allowed&&!update_busy&&!g_cfw_quiesce.request)PhoneVisual_Process(HAL_GetTick());
#endif
        uint32_t now=HAL_GetTick();
#if NOODOE_PRODUCT
        if(!CfwInstall_Busy()&&(!g_cfw_quiesce.request||update_busy))
#endif
            RuntimeUpdate_Process(now);
        SettingsService_Process(now);
#if NOODOE_PRODUCT
        /* Acknowledge only after every active physical writer has drained.
         * Dirty RAM deliberately does not prevent a read-only snapshot. */
        if(g_cfw_quiesce.request&&!CfwStore_Busy()&&!PhotoStore_Busy()&&
            !CfwInstall_Busy()&&!ResourceStore_Busy()&&!DeviceLog_Busy()&&!update_busy&&
            g_photo_import.sequence==g_photo_import.ack){__DMB();g_cfw_quiesce.ack=1;}
        AppRecovery_RuntimeProcess(now,recovery_lease&&g_cfw_quiesce.request==0x52454331U&&g_cfw_quiesce.ack&&
            g_storage_swd.state!=STORAGE_SWD_BUSY);
        AppRecovery_TrialTick(now,!CfwStore_Busy()&&!PhotoStore_Busy()&&!CfwInstall_Busy()&&
            !ResourceStore_Busy()&&!update_busy&&g_storage_swd.state!=STORAGE_SWD_BUSY&&
            g_photo_import.sequence==g_photo_import.ack);
        RecordTransitions(now);
#endif
        ClockService_Process();
        /* Quiet OFF does not poll the light sensor. Explicit storage/phone
         * operations, recovery mailboxes and RTC requests remain serviceable. */
        if(PowerService_Mode()<POWER_DISPLAY_SLEEP)AmbientService_Process(now);
        uint32_t ambient_state=g_ambient_service.state;
        g_noodoe_runtime.ambient_result=(ambient_state==AMBIENT_STATE_OFF ||
            ambient_state==AMBIENT_STATE_QUEUED || ambient_state==AMBIENT_STATE_RUNNING)
            ?UINT32_MAX:g_ambient_service.driver.error;
        if((uint32_t)(now-last_clock)>=1000U){
            PublishClock(now);
            last_clock=now;
        }
        ++g_noodoe_runtime.storage_heartbeat;g_noodoe_runtime.storage_ms=now;
        HealthService_Progress(HEALTH_STORAGE,HAL_GetTick());
        g_noodoe_runtime.storage_stack_free=uxTaskGetStackHighWaterMark(NULL)*sizeof(StackType_t);
        g_noodoe_runtime.heap_free=xPortGetFreeHeapSize();
        g_noodoe_runtime.heap_min=xPortGetMinimumEverFreeHeapSize();
        PowerService_Wait(POWER_OWNER_STORAGE,PowerService_Mode()==POWER_RUN||busy?1U:
            PowerService_Mode()==POWER_DISPLAY_SLEEP?1000U:100U);
    }
}

uint32_t NoodoeRuntime_Start(void)
{
    if(g_noodoe_runtime.started)return g_noodoe_runtime.start_error;
    g_noodoe_runtime.magic=0x4E525431U;g_noodoe_runtime.version=1U;
    g_noodoe_runtime.started=1U;
    HealthService_Init(HAL_GetTick());
    HealthService_Register(HEALTH_IO,HAL_GetTick());
    HealthService_Register(HEALTH_STORAGE,HAL_GetTick());
    HealthService_Register(HEALTH_GRAPHICS,HAL_GetTick());
    const osThreadAttr_t health={.name="HealthSupervisor",.stack_size=1024U,.priority=osPriorityAboveNormal};
    if(!osThreadNew(HealthTask,NULL,&health)){
        BSP_Watchdog_Fail(0x220U);g_noodoe_runtime.start_error|=8U;
    }
#if !NOODOE_PRODUCT
    HealthService_BootReady();
#endif
    /* Generated DMA setup runs once, before any task can enable a stream. */
    MX_DMA_Init();
    BSP_Power_Init(0U); /* Board revision-specific shutdown remains unarmed. */
    /* The Bluetooth owner sets reset/enable before its first UART/MSP init.
     * Configuring TX/RTS here would precede stock's PA8 LOW/PI1 HIGH sequence. */
    g_noodoe_runtime.bluetooth_result=UINT32_MAX; /* Pending storage/key setup. */
    const osThreadAttr_t io={.name="NoodoeIO",.stack_size=4096U,.priority=osPriorityNormal1};
    const osThreadAttr_t storage={.name="NoodoeStorage",.stack_size=4096U,.priority=osPriorityBelowNormal};
    if(!osThreadNew(IoTask,NULL,&io))__atomic_fetch_or(&g_noodoe_runtime.start_error,1U,__ATOMIC_RELAXED);
    if(!osThreadNew(StorageTask,NULL,&storage))__atomic_fetch_or(&g_noodoe_runtime.start_error,2U,__ATOMIC_RELAXED);
    return g_noodoe_runtime.start_error;
}

void NoodoeRuntime_GraphicsHeartbeat(void)
{ ++g_noodoe_runtime.graphics_heartbeat;g_noodoe_runtime.graphics_ms=HAL_GetTick();
  HealthService_Progress(HEALTH_GRAPHICS,g_noodoe_runtime.graphics_ms); }

/* Snapshot getters copy under the same short critical section as Publish. */
uint32_t NoodoeRuntime_GetVehicle(VehicleSnapshot *out)
{return DashService_GetVehicle(out);}
uint32_t NoodoeRuntime_GetGnss(GnssSnapshot *out)
{if(!out)return 0U;taskENTER_CRITICAL();*out=gnss_snapshot;uint32_t ready=snapshots_valid;taskEXIT_CRITICAL();return ready;}
uint32_t NoodoeRuntime_GetSystem(NoodoeSystemSnapshot *out)
{if(!out)return 0U;taskENTER_CRITICAL();*out=system_snapshot;uint32_t ready=snapshots_valid;taskEXIT_CRITICAL();return ready;}

