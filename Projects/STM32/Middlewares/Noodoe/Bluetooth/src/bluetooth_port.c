#include "bluetooth_port.h"
#include "NoodoeBluetooth.h"
#include "BSP_BT_HCI.h"
#include "btstack_run_loop.h"
#include "btstack_run_loop_freertos.h"
#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"

static btstack_data_source_t source;
static void (*rx_callback)(void),(*tx_callback)(void);
static uint32_t initial_baud;
static TaskHandle_t volatile transport_owner;
static const uint8_t *deferred_tx;
static uint16_t deferred_tx_size;
static volatile uint32_t pause_request,pause_ack,owner_pause_token;
static uint32_t pause_counter,pause_since;
#define PAUSE_SETTLE_MS 2U

/* A dedicated atomic mailbox cannot be starved by a full user-command queue.
 * Only the BT owner changes GPIO/DMA state. The requesting storage task owns
 * one token until Resume, or cancels its token on a bounded timeout. */
static void ApplyPause(void)
{
    uint32_t requested=__atomic_load_n(&pause_request,__ATOMIC_ACQUIRE);
    uint32_t current=__atomic_load_n(&owner_pause_token,__ATOMIC_ACQUIRE);
    if (requested==current) return;
    __atomic_store_n(&pause_ack,0U,__ATOMIC_RELEASE);
    if (requested) {
        BSP_BT_HCI_SetPaused(1U); pause_since=HAL_GetTick();
    } else BSP_BT_HCI_SetPaused(0U);
    __atomic_store_n(&owner_pause_token,requested,__ATOMIC_RELEASE);
}

static void AcknowledgePause(void)
{
    uint32_t token=__atomic_load_n(&owner_pause_token,__ATOMIC_ACQUIRE);
    /* Two milliseconds exceed20 character times at the initial115200 baud.
     * DMA and callbacks run during this interval and drain the byte already
     * in flight. A full DR or UART error prevents acknowledgement entirely. */
    if (token && HAL_GetTick()-pause_since>=PAUSE_SETTLE_MS
        && BSP_BT_HCI_IsQuiescent()
        && __atomic_load_n(&pause_request,__ATOMIC_ACQUIRE)==token)
        __atomic_store_n(&pause_ack,token,__ATOMIC_RELEASE);
}

static void FlushDeferredTX(void)
{
    if (deferred_tx_size && !BSP_BT_HCI_IsPaused()) {
        const uint8_t *data=deferred_tx; uint16_t size=deferred_tx_size;
        deferred_tx=NULL; deferred_tx_size=0U;
        if (BSP_BT_HCI_Send(data,size)) Bluetooth_TransportFault(0x103U);
    }
}

/* The pinned FreeRTOS run loop waits on notification bit 1. Its upstream ISR
 * helper leaves xHigherPriorityTaskWoken uninitialized, while FreeRTOS only
 * sets that output when a higher priority task is actually awakened. Keep
 * this correction in our port and preserve the upstream package unchanged.
 * USART/DMA IRQ priorities are at the FreeRTOS syscall ceiling or lower. */
static void WakeFromISR(void)
{
    BaseType_t higher_priority_task_woken=pdFALSE;
    if (transport_owner==NULL) return;
    (void)xTaskNotifyFromISR(transport_owner,1U,eSetBits,&higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

/* Platform clock is the application's TIM6 HAL timebase in milliseconds. */
uint32_t hal_time_ms(void) { return HAL_GetTick(); }

static void Poll(btstack_data_source_t *ds,btstack_data_source_callback_type_t type)
{
    (void)ds; (void)type;
    ApplyPause();
    uint32_t events=BSP_BT_HCI_TakeEvents();
    if (events&4U) { Bluetooth_TransportFault(0x101U); return; }
    /* H4 sees TX completion before RX command-complete if both arrived. */
    if ((events&2U) && tx_callback) tx_callback();
    if ((events&1U) && rx_callback) rx_callback();
    FlushDeferredTX();
    AcknowledgePause();
}
static int Init(const btstack_uart_config_t *config)
{ initial_baud=config->baudrate; return 0; }
static int Open(void)
{
    int result=BSP_BT_HCI_Open();
    if (result) return result;
    transport_owner=xTaskGetCurrentTaskHandle();
    deferred_tx=NULL; deferred_tx_size=0U;
    BSP_BT_HCI_SetWakeFromISR(WakeFromISR);
    btstack_run_loop_set_data_source_handler(&source,Poll);
    btstack_run_loop_enable_data_source_callbacks(&source,DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_add_data_source(&source);
    return BSP_BT_HCI_SetBaud(initial_baud);
}
static int Close(void)
{
    BSP_BT_HCI_SetWakeFromISR(NULL);
    transport_owner=NULL;
    __atomic_store_n(&pause_request,0U,__ATOMIC_RELEASE);
    __atomic_store_n(&pause_ack,0U,__ATOMIC_RELEASE);
    __atomic_store_n(&owner_pause_token,0U,__ATOMIC_RELEASE);
    (void)btstack_run_loop_remove_data_source(&source);
    int result=BSP_BT_HCI_Close();
    if(!result){deferred_tx=NULL;deferred_tx_size=0U;}
    return result;
}
static void SetRx(void (*callback)(void)) { rx_callback=callback; }
static void SetTx(void (*callback)(void)) { tx_callback=callback; }
static int Parity(int parity) { return parity==BTSTACK_UART_PARITY_OFF ? 0 : -1; }
static void Receive(uint8_t *buffer,uint16_t size)
{ if (BSP_BT_HCI_Receive(buffer,size)) Bluetooth_TransportFault(0x102U); }
static void Send(const uint8_t *buffer,uint16_t size)
{
    /* H4 owns this buffer until our completion callback. Defer its one
     * outstanding block while paused instead of turning backpressure into
     * a transport fault. No copy or completion is reported before TX starts. */
    if (BSP_BT_HCI_IsPaused()) {
        if (deferred_tx_size || !buffer || !size) Bluetooth_TransportFault(0x104U);
        else { deferred_tx=buffer; deferred_tx_size=size; }
    } else if (BSP_BT_HCI_Send(buffer,size)) Bluetooth_TransportFault(0x103U);
}

/* The wait yields each millisecond. Existing run-loop timers poll the mailbox
 * at least every10ms, keeping all register access on the Bluetooth owner. */
int Bluetooth_QuiesceTransport(uint32_t timeout_ms,uint32_t *token_out)
{
    uint32_t expected=0U,token,started;
    if (!token_out || !timeout_ms || timeout_ms>1000U) return BLUETOOTH_INVALID;
    *token_out=0U;
    if (__get_IPSR() || __get_PRIMASK() || __get_BASEPRI() || __get_FAULTMASK()
        || xTaskGetCurrentTaskHandle()==transport_owner) return BLUETOOTH_INVALID;
    if (!transport_owner || g_bluetooth.state!=BLUETOOTH_STATE_READY) return BLUETOOTH_NOT_READY;
    taskENTER_CRITICAL(); token=++pause_counter; if (!token) token=++pause_counter; taskEXIT_CRITICAL();
    if (!__atomic_compare_exchange_n(&pause_request,&expected,token,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))
        return BLUETOOTH_BUSY;
    started=HAL_GetTick();
    for (;;) {
        if (__atomic_load_n(&pause_ack,__ATOMIC_ACQUIRE)==token
            && __atomic_load_n(&pause_request,__ATOMIC_ACQUIRE)==token
            && g_bluetooth.state==BLUETOOTH_STATE_READY) {
            *token_out=token; return BLUETOOTH_OK;
        }
        if (!transport_owner || g_bluetooth.state!=BLUETOOTH_STATE_READY
            || HAL_GetTick()-started>=timeout_ms) break;
        vTaskDelay(pdMS_TO_TICKS(1U));
    }
    expected=token;
    (void)__atomic_compare_exchange_n(&pause_request,&expected,0U,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
    return g_bluetooth.state==BLUETOOTH_STATE_READY?BLUETOOTH_BUSY:BLUETOOTH_NOT_READY;
}

int Bluetooth_ResumeTransport(uint32_t token)
{
    uint32_t expected=token,started;
    if (!token || __get_IPSR() || __get_PRIMASK() || __get_BASEPRI() || __get_FAULTMASK()
        || xTaskGetCurrentTaskHandle()==transport_owner) return BLUETOOTH_INVALID;
    if (!__atomic_compare_exchange_n(&pause_request,&expected,0U,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))
        return BLUETOOTH_INVALID;
    started=HAL_GetTick();
    while (__atomic_load_n(&owner_pause_token,__ATOMIC_ACQUIRE)==token) {
        if (HAL_GetTick()-started>=100U) return BLUETOOTH_BUSY;
        vTaskDelay(pdMS_TO_TICKS(1U));
    }
    return BLUETOOTH_OK;
}
static const btstack_uart_t uart={
    .init=Init,.open=Open,.close=Close,.set_block_received=SetRx,
    .set_block_sent=SetTx,.set_baudrate=BSP_BT_HCI_SetBaud,
    .set_parity=Parity,.set_flowcontrol=BSP_BT_HCI_SetFlowControl,
    .receive_block=Receive,.send_block=Send
};
const btstack_uart_t *Bluetooth_UARTInstance(void) { return &uart; }
int Bluetooth_TransportIsDetached(void)
{ return transport_owner==NULL && deferred_tx==NULL && !deferred_tx_size && !g_bsp_bt_hci.opened; }
