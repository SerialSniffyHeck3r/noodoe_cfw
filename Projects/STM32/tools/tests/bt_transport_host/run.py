"""Run actual BT queues, patch selector, key DB and DMA handoff on ARM Unicorn.
Hardware/HCI sends and RTOS scheduling are mocked; no radio success is claimed.
"""
import hashlib,json,struct,subprocess,sys
from pathlib import Path
HERE=Path(__file__).resolve().parent
PROJECT=HERE.parents[2]
sys.path.insert(0,str(PROJECT.parents[1]/'.tools/analysis-python'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS,UC_HOOK_MEM_READ,UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_R0,UC_ARM_REG_XPSR
TC=Path('C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin')
OUT=HERE/'output'; OUT.mkdir(parents=True,exist_ok=True)
(OUT/'FreeRTOS.h').write_text('''#pragma once
#include <stdint.h>
#include <stddef.h>
typedef int BaseType_t; typedef unsigned UBaseType_t; typedef uint32_t TickType_t;
typedef uint32_t StackType_t; typedef struct{int x;} StaticTask_t;
typedef struct{int x;} StaticQueue_t;
#define configMAX_PRIORITIES 56
#define pdTRUE 1
#define pdFALSE 0
#define portMAX_DELAY 0xffffffffU
#define pdMS_TO_TICKS(x) (x)
void MockEnterCritical(void); void MockExitCritical(void);
#define taskENTER_CRITICAL() MockEnterCritical()
#define taskEXIT_CRITICAL() MockExitCritical()
void MockYield(BaseType_t);
#define portYIELD_FROM_ISR(x) MockYield(x)
''')
(OUT/'task.h').write_text('''#pragma once
#include "FreeRTOS.h"
typedef void *TaskHandle_t;
typedef enum { eNoAction=0,eSetBits=1 } eNotifyAction;
TaskHandle_t xTaskGetCurrentTaskHandle(void);
BaseType_t xTaskNotifyFromISR(TaskHandle_t,uint32_t,eNotifyAction,BaseType_t*);
TaskHandle_t xTaskCreateStatic(void(*fn)(void*),const char*,uint32_t,void*,unsigned,StackType_t*,StaticTask_t*);
void vTaskDelay(TickType_t); unsigned uxTaskGetStackHighWaterMark(TaskHandle_t);
TickType_t xTaskGetTickCount(void); void vTaskDelayUntil(TickType_t*,TickType_t);
''')
(OUT/'queue.h').write_text('''#pragma once
#include "FreeRTOS.h"
typedef void *QueueHandle_t;
QueueHandle_t xQueueCreateStatic(unsigned,unsigned,uint8_t*,StaticQueue_t*);
unsigned uxQueueMessagesWaiting(QueueHandle_t);
int xQueueSend(QueueHandle_t,const void*,unsigned); int xQueueReceive(QueueHandle_t,void*,unsigned);
''')
(OUT/'test.c').write_text(r'''
#include "bluetooth_service.c"
#include "bluetooth_port.c"
#include "bluetooth_keys.h"
#include "usart.h"
#include <string.h>
static uint32_t assertions,disconnects,sent_length,send_failure,queue_failure,accepts,declines;
static uint8_t sent[512],last_request[64];
static uint32_t dma_failure,dma_count,dma_destination,dma_arm_at;
static uint32_t reset_releases,reset_release_at,reset_without_rx,delay10_count,delay150_count;
static uint32_t uart_init_calls,uart_msp_calls,uart_init_order_errors,enable_order_errors,abort_calls;
static uint32_t startup_late,delay_until_calls;
static uint32_t now;
static uint32_t power_mode,timer_ms,credit_grants,send_events,bt_power_ack;
/* Watchdog policy has its own real-ARM suite; this transport fixture records
 * completed-owner notifications without starting real/mock IWDG hardware. */
static uint32_t health_bt_progress,queue_pending;
unsigned uxQueueMessagesWaiting(QueueHandle_t q){(void)q;return queue_pending;}
void HealthService_Register(uint32_t owner,uint32_t time){(void)owner;(void)time;}
void HealthService_Progress(uint32_t owner,uint32_t time){(void)time;if(owner==HEALTH_BT)++health_bt_progress;}
uint32_t PowerService_Mode(void){return power_mode;}
void PowerService_Acknowledge(uint32_t owner,uint32_t value){if(owner==POWER_OWNER_BT)bt_power_ack=value;}
void btstack_run_loop_set_timer(btstack_timer_source_t *ts,uint32_t ms){(void)ts;timer_ms=ms;}
void btstack_run_loop_add_timer(btstack_timer_source_t *ts){(void)ts;}
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t task){(void)task;return 128;}
int xQueueReceive(QueueHandle_t q,void *item,unsigned wait){(void)q;(void)item;(void)wait;return pdFALSE;}
uint8_t rfcomm_grant_credits(uint16_t cid,uint8_t count){(void)cid;(void)count;++credit_grants;return 0;}
uint8_t rfcomm_request_can_send_now_event(uint16_t cid){(void)cid;++send_events;return 0;}
static uint32_t notifications,notification_input_invalid,notification_args_invalid,yield_calls,yield_value,should_wake;
static uint32_t quiesce_autopoll,tx_submissions,transmit_length;
static TaskHandle_t caller_task=(TaskHandle_t)0x1234U;
static const uint8_t *transmit_buffer;
static uint32_t critical_depth,critical_errors,critical_preempt,critical_preempt_role;
static const uint8_t *selected_script;
static uint32_t selected_size;
static hci_connection_t pending_connection;
static uint32_t pending_exists,acl_disconnects,create_cancels,created_channels;
static uint32_t abort_failure,abort_leaves_enabled,power_ons,power_offs;
static uint32_t raw_mode,raw_started,raw_sent,raw_bad_payload,raw_api_result;
static void RawStep(void);
static HCI_STATE mock_hci_state=HCI_STATE_OFF;
const hci_cmd_t hci_create_connection_cancel={HCI_OPCODE_HCI_CREATE_CONNECTION_CANCEL,"B"};
hci_connection_t *hci_connection_for_bd_addr_and_type(const bd_addr_t address,bd_addr_type_t type){
    (void)address;(void)type;return pending_exists?&pending_connection:NULL;
}
uint8_t gap_disconnect(hci_con_handle_t handle){(void)handle;acl_disconnects++;pending_connection.state=SENT_DISCONNECT;return 0;}
bool hci_can_send_command_packet_now(void){return true;}
bool sdp_client_ready(void){return true;}
uint8_t sdp_client_query_rfcomm_channel_and_name_for_uuid(btstack_packet_handler_t cb,bd_addr_t address,uint16_t uuid){(void)cb;(void)address;(void)uuid;return 0;}
uint8_t hci_send_cmd(const hci_cmd_t *cmd,...){if(cmd==&hci_create_connection_cancel)create_cancels++;return 0;}
int hci_power_control(HCI_POWER_MODE mode){if(mode==HCI_POWER_ON)power_ons++;else power_offs++;return 0;}
HCI_STATE hci_get_state(void){return mock_hci_state;}
void hci_set_chipset(const btstack_chipset_t *chipset){(void)chipset;}
const btstack_chipset_t *btstack_chipset_cc256x_instance(void){return NULL;}
void gap_local_bd_addr(bd_addr_t address){const uint8_t a[6]={0x98,0x07,0x2d,0x01,0x71,0x0f};memcpy(address,a,6);}
void gap_set_local_name(const char *name){(void)name;}
int gap_pin_code_response(const bd_addr_t a,const char*p){(void)a;(void)p;return 0;}
int gap_pin_code_negative(bd_addr_t a){(void)a;return 0;}
int gap_ssp_confirmation_response(const bd_addr_t a){(void)a;return 0;}
int gap_ssp_confirmation_negative(const bd_addr_t a){(void)a;return 0;}
uint8_t rfcomm_create_channel_with_initial_credits(btstack_packet_handler_t cb,bd_addr_t a,uint8_t ch,uint8_t credit,uint16_t *cid){
    (void)cb;(void)a;(void)ch;(void)credit;created_channels++;*cid=71;return 0;
}
uint8_t rfcomm_accept_connection(uint16_t cid){(void)cid;++accepts;return 0;}
uint8_t rfcomm_decline_connection(uint16_t cid){(void)cid;++declines;return 0;}
void gap_discoverable_control(uint8_t enable){(void)enable;}
void gap_drop_link_key_for_bd_addr(bd_addr_t a){(void)a;}
int gap_inquiry_start(uint8_t len){(void)len;return 0;}
int gap_dedicated_bonding(bd_addr_t a,int mitm){(void)a;(void)mitm;return 0;}
UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart1_tx;
void *memset(void*p,int value,size_t count){uint8_t*b=p;for(size_t i=0;i<count;i++)b[i]=(uint8_t)value;return p;}
void *memcpy(void*d,const void*s,size_t count){uint8_t*b=d;const uint8_t*a=s;for(size_t i=0;i<count;i++)b[i]=a[i];return d;}
int memcmp(const void*a,const void*b,size_t count){const uint8_t*x=a,*y=b;for(size_t i=0;i<count;i++)if(x[i]!=y[i])return(int)x[i]-(int)y[i];return 0;}
#define CHECK(x) do{assertions++;if(!(x))return __LINE__;}while(0)
uint32_t get_assertions(void){return assertions;}
void MockEnterCritical(void){
    /* Model the BT owner preempting immediately before queue ownership is
     * acquired. A check performed before ENTER would accept a stale session. */
    if(!critical_depth&&critical_preempt){
        volatile Bluetooth_LinkState *link=&g_bluetooth.links[critical_preempt_role];
        if(critical_preempt==1U)link->reconnects++;
        else if(critical_preempt==2U)link->cid++;
        else if(critical_preempt==3U)link->address[0]^=0x80U;
        else link->status=BLUETOOTH_LINK_OFF;
        critical_preempt=0U;
    }
    critical_depth++;
}
void MockExitCritical(void){if(critical_depth)critical_depth--;else critical_errors++;}
static uint32_t virtual_clock_calls;
uint32_t btstack_run_loop_get_time_ms(void){virtual_clock_calls++;return now;}
uint32_t HAL_RCC_GetPCLK2Freq(void){return 84000000U;}
uint32_t HAL_GetTick(void){return now;}
TaskHandle_t xTaskGetCurrentTaskHandle(void){return caller_task;}
BaseType_t xTaskNotifyFromISR(TaskHandle_t task,uint32_t bits,eNotifyAction action,BaseType_t *higher){
    notifications++; if(*higher!=pdFALSE)notification_input_invalid++;
    if(task!=(TaskHandle_t)0x1234U||bits!=1U||action!=eSetBits)notification_args_invalid++;
    /* Real FreeRTOS leaves the output unchanged when no higher task wakes. */
    if(should_wake)*higher=pdTRUE;return pdTRUE;
}
void MockYield(BaseType_t value){yield_calls++;yield_value=(uint32_t)value;}
void btstack_run_loop_set_data_source_handler(btstack_data_source_t *ds,void(*fn)(btstack_data_source_t*,btstack_data_source_callback_type_t)){(void)ds;(void)fn;}
void btstack_run_loop_enable_data_source_callbacks(btstack_data_source_t *ds,uint16_t type){(void)ds;(void)type;}
void btstack_run_loop_add_data_source(btstack_data_source_t *ds){(void)ds;}
int btstack_run_loop_remove_data_source(btstack_data_source_t *ds){(void)ds;return 1;}
uint8_t rfcomm_disconnect(uint16_t cid){(void)cid;disconnects++;return 0;}
uint8_t rfcomm_send(uint16_t cid,uint8_t*data,uint16_t len){(void)cid;if(send_failure)return 1;sent_length=len;memcpy(sent,data,len);return 0;}
int xQueueSend(QueueHandle_t q,const void*r,unsigned timeout){(void)q;(void)timeout;if(queue_failure)return 0;memcpy(last_request,r,sizeof(Request));return 1;}
void btstack_chipset_cc256x_set_init_script(uint8_t*data,uint32_t size){selected_script=data;selected_size=size;}
HAL_StatusTypeDef HAL_DMA_Start_IT(DMA_HandleTypeDef*dma,uint32_t src,uint32_t dst,uint32_t count){(void)src;dma_destination=dst;dma_count=count;dma_arm_at=now;
    if(raw_mode&&!dma_failure){dma->Instance->NDTR=count;dma->Instance->CR|=DMA_SxCR_EN;}
    return dma_failure?HAL_ERROR:HAL_OK;}
HAL_StatusTypeDef HAL_UART_Abort(UART_HandleTypeDef*uart){
    abort_calls++;
    if(abort_failure)return HAL_TIMEOUT;
    if(!abort_leaves_enabled){DMA2_Stream5->CR&=~DMA_SxCR_EN;DMA2_Stream7->CR&=~DMA_SxCR_EN;}
    uart->gState=HAL_UART_STATE_READY;uart->RxState=HAL_UART_STATE_READY;return HAL_OK;}
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef*uart){
    uart_init_calls++;
    if(((GPIOA->MODER>>16)&3U)!=1U || (GPIOA->ODR&GPIO_PIN_8) ||
       ((GPIOI->MODER>>2)&3U)!=1U || !(GPIOI->ODR&GPIO_PIN_1))uart_init_order_errors++;
    if(uart->gState==HAL_UART_STATE_RESET){
        uart_msp_calls++;
        uart->hdmarx=&hdma_usart1_rx;uart->hdmatx=&hdma_usart1_tx;
        hdma_usart1_rx.Instance=DMA2_Stream5;hdma_usart1_tx.Instance=DMA2_Stream7;
        hdma_usart1_rx.Parent=uart;hdma_usart1_tx.Parent=uart;
        for(unsigned pin=9;pin<=12;pin++)MODIFY_REG(GPIOA->MODER,3U<<(pin*2),2U<<(pin*2));
    }
    uart->gState=HAL_UART_STATE_READY;uart->RxState=HAL_UART_STATE_READY;
    uart->Instance->CR1=USART_CR1_UE|USART_CR1_RE|USART_CR1_TE;uart->Instance->CR3=uart->Init.HwFlowCtl;return HAL_OK;}
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef*uart,const uint8_t*data,uint16_t size){(void)uart;tx_submissions++;transmit_buffer=data;transmit_length=size;
    if(raw_mode){
        if(size!=4||data[0]!=1||data[1]!=3||data[2]!=12||data[3]!=0)raw_bad_payload++;
        if(raw_mode==4)return HAL_ERROR;
        DMA2_Stream7->NDTR=size;DMA2_Stream7->CR|=DMA_SxCR_EN;USART1->SR&=~USART_SR_TC;raw_started=1;
        if(raw_mode==10){
            DMA2_Stream7->NDTR=0;DMA2_Stream7->CR&=~DMA_SxCR_EN;USART1->SR|=USART_SR_TC|USART_SR_FE;
            DMA2_Stream5->NDTR=0;g_bsp_bt_hci.errors++;g_bsp_bt_hci.last_hal_error=HAL_UART_ERROR_FE;
        }
    }return HAL_OK;}
void HAL_GPIO_Init(GPIO_TypeDef*gpio,GPIO_InitTypeDef*io){for(unsigned i=0;i<16;i++)if(io->Pin&(1U<<i)){
    MODIFY_REG(gpio->MODER,3UL<<(i*2),(io->Mode&3UL)<<(i*2));
    if((io->Mode&3U)==1U)MODIFY_REG(gpio->IDR,1U<<i,gpio->ODR&(1U<<i));}}
void HAL_GPIO_WritePin(GPIO_TypeDef*gpio,uint16_t pin,GPIO_PinState state){
    if(state)gpio->ODR|=pin;else gpio->ODR&=~pin;
    for(unsigned i=0;i<16;i++)if((pin&(1U<<i))&&((gpio->MODER>>(i*2))&3U)==1U)
        MODIFY_REG(gpio->IDR,1U<<i,gpio->ODR&(1U<<i));
    if(gpio==GPIOI&&pin==GPIO_PIN_1&&state&&
       (((GPIOA->MODER>>16)&3U)!=1U || (GPIOA->IDR&GPIO_PIN_8)))enable_order_errors++;
    if(gpio==GPIOA&&pin==GPIO_PIN_8&&state){reset_releases++;reset_release_at=now;
        if(!dma_count||!g_bsp_bt_hci.rx_busy||!(USART1->CR3&USART_CR3_DMAR)||((GPIOA->MODER>>(12*2))&3U)!=2U)reset_without_rx++;}
}
void HAL_NVIC_SetPriority(IRQn_Type irq,uint32_t p,uint32_t sub){(void)irq;(void)p;(void)sub;}
void HAL_NVIC_EnableIRQ(IRQn_Type irq){(void)irq;}
void vTaskDelay(TickType_t ticks){if(ticks==10U)delay10_count++;if(ticks==150U)delay150_count++;now+=ticks;
    RawStep();
    if(quiesce_autopoll){TaskHandle_t saved=caller_task;caller_task=(TaskHandle_t)0x1234U;Poll(NULL,DATA_SOURCE_CALLBACK_POLL);caller_task=saved;}}
TickType_t xTaskGetTickCount(void){return now;}
void vTaskDelayUntil(TickType_t *deadline,TickType_t increment){
    delay_until_calls++;*deadline+=increment;
    if((int32_t)(*deadline-now)>0)now=*deadline;
    if(startup_late){now+=startup_late;startup_late=0;}
    uint32_t elapsed=now-reset_release_at;
    if(elapsed>=5U&&elapsed<50U)GPIOA->IDR&=~GPIO_PIN_11;else GPIOA->IDR|=GPIO_PIN_11;
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef*uart){BSP_BT_HCI_OnRxComplete(uart);}
void HAL_UART_ErrorCallback(UART_HandleTypeDef*uart){BSP_BT_HCI_OnError(uart);}
static void RawStep(void){
    if(!raw_mode||!raw_started||raw_sent)return;
    if(raw_request_active)raw_api_result=(uint32_t)Bluetooth_Stop();
    if(raw_mode==5)return;
    DMA2_Stream7->NDTR=0;DMA2_Stream7->CR&=~DMA_SxCR_EN;USART1->SR|=USART_SR_TC;
    BSP_BT_HCI_OnTxComplete(&huart1);raw_sent=1;
    if(raw_mode==6){huart1.ErrorCode=HAL_UART_ERROR_ORE;USART1->SR|=USART_SR_ORE;BSP_BT_HCI_OnError(&huart1);return;}
    if(raw_mode!=2){
        uint8_t p[]={4,14,4,2,3,12,0};
        if(raw_mode==3)p[5]=13;
        if(raw_mode==7)p[6]=12;
        memcpy((void*)dma_destination,p,sizeof(p));DMA2_Stream5->NDTR=dma_count-sizeof(p);
        if(raw_mode==9){DMA2_Stream5->NDTR=0;USART1->SR|=USART_SR_FE;g_bsp_bt_hci.errors++;g_bsp_bt_hci.last_hal_error=HAL_UART_ERROR_FE;}
    }
    if(raw_mode==8)abort_failure=1;
}
static unsigned ValidatePatch(void){
    unsigned pos=0,commands=0;
    while(pos<selected_size){if(selected_size-pos<4||selected_script[pos]!=1)return 0;
        unsigned n=selected_script[pos+3];if(n>selected_size-pos-4)return 0;
        if(selected_script[pos+1]==0x36&&selected_script[pos+2]==0xff)return 0;
        pos+=4+n;commands++;
    } return commands;
}
uint32_t test_retained(void){
    power_mode=POWER_DISPLAY_SLEEP;g_bluetooth.state=BLUETOOTH_STATE_READY;mock_hci_state=HCI_STATE_WORKING;
    for(uint32_t i=0;i<BLUETOOTH_ROLE_COUNT;++i){g_bluetooth.links[i].status=BLUETOOTH_LINK_UP;
        g_bluetooth.links[i].cid=10+i;g_bluetooth.links[i].mtu=128;links[i].tx_count=1;}
    /* Two phones and OBD retain their sessions even beyond the old timeout;
     * credit and send requests are still serviced while the display sleeps. */
    for(uint32_t i=0;i<100;++i){now=i*120000U;Tick(&timer);
        CHECK(timer_ms==100&&!power_offs&&!power_ons);
        for(uint32_t n=0;n<BLUETOOTH_ROLE_COUNT;++n)CHECK(g_bluetooth.links[n].status==BLUETOOTH_LINK_UP&&g_bluetooth.links[n].cid==10+n);}
    CHECK(credit_grants==BLUETOOTH_ROLE_COUNT&&send_events==BLUETOOTH_ROLE_COUNT);
    power_mode=POWER_RUN;Tick(&timer);CHECK(timer_ms==10&&!power_offs);
    power_mode=POWER_DISPLAY_SLEEP;g_bluetooth.state=BLUETOOTH_STATE_STARTING;start_tick=now;
    Tick(&timer);CHECK(timer_ms==10&&!power_offs);
    /* Final stage waits for real controller/transport shutdown; ON restarts once. */
    g_bluetooth.state=BLUETOOTH_STATE_READY;power_mode=POWER_DEEP;g_bsp_bt_hci.opened=1;
    Tick(&timer);CHECK(power_offs==1&&!bt_power_ack&&timer_ms==10);
    Tick(&timer);CHECK(power_offs==1&&!bt_power_ack);
    mock_hci_state=HCI_STATE_OFF;g_bsp_bt_hci.opened=0;
    Tick(&timer);CHECK(bt_power_ack&&timer_ms==1000&&g_bluetooth.state==BLUETOOTH_STATE_OFF);
    power_mode=POWER_RUN;Tick(&timer);CHECK(!bt_power_ack&&power_ons==1&&timer_ms==10);
    Tick(&timer);CHECK(power_ons==1);
    return 0;
}
uint32_t test_main(void){
    uint8_t input[2048],output[2048];
    /* Cold startup has neither an owner nor a run-loop instance. Health
     * registration's clock must already work without virtual dispatch. */
    CHECK(!owner);now=12345U;
    CHECK(Now()==12345U&&virtual_clock_calls==0);now=0;
    /* The actual RFCOMM handler reserves the sole slot at admission. A second
     * peer cannot replace either a pending session or a live queued packet. */
    uint8_t incoming[13]={RFCOMM_EVENT_INCOMING_CONNECTION,11,1,2,3,4,5,6,1,41,0,0,0};
    Packet(HCI_EVENT_PACKET,0,incoming,sizeof(incoming));
    CHECK(accepts==1&&g_bluetooth.links[BLUETOOTH_PHONE].cid==41);
    incoming[9]=42;Packet(HCI_EVENT_PACKET,0,incoming,sizeof(incoming));
    CHECK(accepts==1&&declines==1);
    incoming[2]=2;Packet(HCI_EVENT_PACKET,0,incoming,sizeof(incoming));
    CHECK(accepts==1&&declines==2&&g_bluetooth.links[0].cid==41);
    Opened(BLUETOOTH_PHONE,41,512);
    CHECK(g_bluetooth.links[0].status==BLUETOOTH_LINK_UP&&g_bluetooth.links[0].reconnects==1);
    ReceiveFrame(BLUETOOTH_PHONE,(const uint8_t*)"one",3);
    Packet(HCI_EVENT_PACKET,0,incoming,sizeof(incoming));
    CHECK(accepts==1&&declines==3&&links[0].rx_count==3);
    CHECK(Bluetooth_Receive(BLUETOOTH_PHONE,output,3)==3&&!memcmp(output,"one",3));
    CHECK(Bluetooth_Connect(BLUETOOTH_PHONE2,incoming+2,1)==BLUETOOTH_UNSUPPORTED);
    for(unsigned role=1;role<=2;++role){
        Bluetooth_LinkState unused;
        CHECK(Bluetooth_GetLinkState(role,&unused)==BLUETOOTH_INVALID);
        CHECK(Bluetooth_Send(role,input,1)==BLUETOOTH_INVALID);
        CHECK(Bluetooth_Receive(role,output,1)==BLUETOOTH_INVALID);
        CHECK(Bluetooth_Disconnect(role)==BLUETOOTH_INVALID);
    }
    /* Closing a slot never frees it for another phone before cleanup drains. */
    CancelRole(BLUETOOTH_PHONE,BLUETOOTH_ERROR_LOCAL_CANCEL);
    g_bluetooth.links[0].cid=0;Packet(HCI_EVENT_PACKET,0,incoming,sizeof(incoming));
    CHECK(accepts==1&&declines==4);
    memset(links,0,sizeof(links));memset((void*)g_bluetooth.links,0,sizeof(g_bluetooth.links));
    for(unsigned i=0;i<sizeof(input);i++)input[i]=(uint8_t)(i*13+7);
    CHECK(Bluetooth_Send((Bluetooth_Role)3,input,1)==BLUETOOTH_INVALID);
    CHECK(Bluetooth_Send(BLUETOOTH_PHONE,input,1)==BLUETOOTH_NOT_READY);
    CHECK(Bluetooth_Receive(BLUETOOTH_PHONE2,NULL,1)==BLUETOOTH_INVALID);
    for(unsigned r=0;r<BLUETOOTH_ROLE_COUNT;r++){
        g_bluetooth.links[r].status=BLUETOOTH_LINK_UP;g_bluetooth.links[r].cid=(uint16_t)(r+1);g_bluetooth.links[r].mtu=512;
        CHECK(Bluetooth_Send((Bluetooth_Role)r,input,1024)==0);
        CHECK(Bluetooth_Send((Bluetooth_Role)r,input,1)==BLUETOOTH_QUEUE_FULL);
        CHECK(links[r].tx_count==1024);
        send_failure=1;SendFrame(r);CHECK(links[r].tx_count==1024);
        send_failure=0;SendFrame(r);CHECK(sent_length==512&&memcmp(sent,input,512)==0);
        CHECK(links[r].tx_count==512);
        CHECK(Bluetooth_Send((Bluetooth_Role)r,input,512)==0);
        SendFrame(r);CHECK(memcmp(sent,input+512,512)==0);
        SendFrame(r);CHECK(memcmp(sent,input,512)==0&&links[r].tx_count==0);
        links[r].credit=1;ReceiveFrame(r,input,1700);CHECK(links[r].rx_count==1700&&links[r].credit==0);
        CHECK(Bluetooth_Receive((Bluetooth_Role)r,output,1000)==1000&&memcmp(input,output,1000)==0);
        ReceiveFrame(r,input,1000);CHECK(links[r].rx_count==1700);
        CHECK(Bluetooth_Receive((Bluetooth_Role)r,output,2048)==1700);
        CHECK(memcmp(output,input+1000,700)==0&&memcmp(output+700,input,1000)==0);
        ReceiveFrame(r,input,2048);ReceiveFrame(r,input,1);
        CHECK(links[r].rx_count==2048&&g_bluetooth.links[r].rx_overflow==1);
        ClearBuffers(r);CHECK(!links[r].rx_count&&!links[r].tx_count);
    }
    CHECK(disconnects==1);
    /* Queue access is tied to one snapshot even if the higher-priority BT
     * owner closes/reopens immediately before the critical section. Reused
     * CID/address with a changed epoch must be rejected in both directions. */
    CHECK(Bluetooth_SendSession(BLUETOOTH_PHONE,NULL,input,1)==BLUETOOTH_INVALID);
    CHECK(Bluetooth_ReceiveSession(BLUETOOTH_PHONE,NULL,output,1)==BLUETOOTH_INVALID);
    for(unsigned role=0;role<BLUETOOTH_ROLE_COUNT;role++){
        Bluetooth_LinkState expected;
        ClearBuffers(role);g_bluetooth.links[role].status=BLUETOOTH_LINK_UP;
        ReceiveFrame(role,input,16);CHECK(Bluetooth_GetLinkState((Bluetooth_Role)role,&expected)==0);
        CHECK(Bluetooth_SendSession((Bluetooth_Role)role,&expected,input,8)==0&&links[role].tx_count==8);
        CHECK(Bluetooth_ReceiveSession((Bluetooth_Role)role,&expected,output,8)==8&&!memcmp(input,output,8));
        CHECK(links[role].rx_count==8);
        for(unsigned change=1;change<=4U;change++){
            /* Every case starts from the current UP session and inserts one
             * change immediately before the send/receive queue lock. */
            g_bluetooth.links[role].status=BLUETOOTH_LINK_UP;
            CHECK(Bluetooth_GetLinkState((Bluetooth_Role)role,&expected)==0);
            critical_preempt_role=role;critical_preempt=change;
            memset(output,0xa5,16);
            CHECK(Bluetooth_ReceiveSession((Bluetooth_Role)role,&expected,output,8)==BLUETOOTH_NOT_READY);
            CHECK(links[role].rx_count==8&&output[0]==0xa5&&output[7]==0xa5);
            CHECK(Bluetooth_SendSession((Bluetooth_Role)role,&expected,input,1)==BLUETOOTH_NOT_READY&&links[role].tx_count==8);
            g_bluetooth.links[role].status=BLUETOOTH_LINK_UP;
            CHECK(Bluetooth_GetLinkState((Bluetooth_Role)role,&expected)==0);
            critical_preempt=change;
            CHECK(Bluetooth_SendSession((Bluetooth_Role)role,&expected,input,1)==BLUETOOTH_NOT_READY&&links[role].tx_count==8);
        }
        g_bluetooth.links[role].status=BLUETOOTH_LINK_UP;
        CHECK(Bluetooth_GetLinkState((Bluetooth_Role)role,&expected)==0);
        expected.status=BLUETOOTH_LINK_OFF;
        CHECK(Bluetooth_ReceiveSession((Bluetooth_Role)role,&expected,output,8)==BLUETOOTH_NOT_READY);
        ClearBuffers(role);
    }
    CHECK(critical_depth==0&&critical_errors==0&&critical_preempt==0);
    g_bluetooth.links[BLUETOOTH_PHONE].reconnects=41;
    Opened(BLUETOOTH_PHONE,8,512);
    CHECK(g_bluetooth.links[BLUETOOTH_PHONE].reconnects==42);
    Retry(BLUETOOTH_PHONE,0);Opened(BLUETOOTH_PHONE,9,512);
    CHECK(g_bluetooth.links[BLUETOOTH_PHONE].reconnects==43&&g_bluetooth.links[BLUETOOTH_PHONE].status==BLUETOOTH_LINK_UP);
    requests=(QueueHandle_t)1;
    uint8_t addr[6]={1,2,3,4,5,6};
    CHECK(Bluetooth_Connect(BLUETOOTH_PHONE,addr,0)==BLUETOOTH_UNSUPPORTED);
    CHECK(Bluetooth_Connect(BLUETOOTH_ELM,addr,31)==BLUETOOTH_UNSUPPORTED);
    CHECK(Bluetooth_Connect(BLUETOOTH_ELM,addr,0)==BLUETOOTH_UNSUPPORTED);
    queue_failure=1;CHECK(Bluetooth_Discover()==BLUETOOTH_UNSUPPORTED);queue_failure=0;
    CHECK(Bluetooth_GetDiscovered(NULL,0)==0);
    CHECK(Bluetooth_Pair(addr,"1234")==BLUETOOTH_UNSUPPORTED);
    CHECK(!PairAllowed(addr));pairing_until=now+1000;
    CHECK(PairAllowed(addr));now+=1001;CHECK(!PairAllowed(addr));pairing_until=0;
    CHECK(Bluetooth_SetPairingWindow(181)==BLUETOOTH_INVALID);
    CHECK(Bluetooth_SetPairingWindow(120)==0);
    uint32_t bytes=77;
    CHECK(Bluetooth_SelectPatch(15,0x1b90,&bytes)!=0&&bytes==77);
    CHECK(Bluetooth_SelectPatch(13,0xffff,&bytes)!=0&&bytes==77);
    CHECK(Bluetooth_SelectPatch(13,0x1b90,&bytes)==0&&bytes==9243&&ValidatePatch()>30);
    CHECK(Bluetooth_SelectPatch(13,0x9a1a,&bytes)==0&&bytes==6760&&ValidatePatch()>20);
    CHECK(Bluetooth_SelectPatch(13,0x9a1a,NULL)!=0);
    Bluetooth_KeyStore keys={.version=1};
    CHECK(Bluetooth_ImportKeys(&keys)==0);
    const btstack_link_key_db_t*db=Bluetooth_KeyDB();link_key_t key;link_key_type_t type;
    CHECK(!db->get_link_key(addr,key,&type));
    db->put_link_key(addr,input,COMBINATION_KEY);CHECK(db->get_link_key(addr,key,&type));
    CHECK(type==COMBINATION_KEY&&!memcmp(key,input,16));
    CHECK(Bluetooth_ExportKeys(&keys)==0&&keys.generation==1);
    Bluetooth_MarkKeysPersisted(0);CHECK(g_bluetooth.key_persisted_generation==0);
    Bluetooth_MarkKeysPersisted(1);CHECK(g_bluetooth.key_persisted_generation==1);
    for(unsigned i=0;i<8;i++){addr[0]=(uint8_t)(20+i);db->put_link_key(addr,input,COMBINATION_KEY);}
    btstack_link_key_iterator_t it;CHECK(db->iterator_init(&it));unsigned found=0;
    while(db->iterator_get_next(&it,addr,key,&type))found++;CHECK(found==6);db->iterator_done(&it);
    db->delete_link_key(addr);CHECK(!db->get_link_key(addr,key,&type));
    CHECK(g_bluetooth.key_generation>g_bluetooth.key_persisted_generation);
    keys.version=0;CHECK(Bluetooth_ImportKeys(&keys)==BLUETOOTH_INVALID);
    CHECK(Bluetooth_BondCount()==5);
    g_bluetooth.state=BLUETOOTH_STATE_READY;CHECK(Bluetooth_ForgetAll()==BLUETOOTH_BUSY);
    g_bluetooth.state=BLUETOOTH_STATE_OFF;mock_hci_state=HCI_STATE_WORKING;CHECK(Bluetooth_ForgetAll()==BLUETOOTH_BUSY);
    mock_hci_state=HCI_STATE_OFF;requests=(QueueHandle_t)1;queue_pending=1;CHECK(Bluetooth_ForgetAll()==BLUETOOTH_BUSY);
    queue_pending=0;CHECK(Bluetooth_ForgetAll()==0&&Bluetooth_BondCount()==0);
    CHECK(g_bluetooth.key_generation!=g_bluetooth.key_persisted_generation);
    now=100;g_bluetooth.state=BLUETOOTH_STATE_READY;Request window={.operation=OP_WINDOW,.value=120};ProcessRequest(&window);
    CHECK(Bluetooth_PairingSeconds()==120);now+=119999;CHECK(Bluetooth_PairingSeconds()==1);now++;CHECK(Bluetooth_PairingSeconds()==0);
    window.value=0;ProcessRequest(&window);CHECK(pairing_until==0);
    g_bluetooth.state=BLUETOOTH_STATE_STARTING;window.value=180;ProcessRequest(&window);
    CHECK(pairing_until==0&&pairing_seconds==180&&!PairAllowed(addr));
    now+=19000;uint8_t working[]={BTSTACK_EVENT_STATE,1,HCI_STATE_WORKING};Packet(HCI_EVENT_PACKET,0,working,sizeof(working));
    CHECK(Bluetooth_PairingSeconds()==180&&PairAllowed(addr)&&pairing_seconds==0);
    now+=180000;CHECK(!PairAllowed(addr));window.value=0;ProcessRequest(&window);
    now=0;g_bluetooth.state=BLUETOOTH_STATE_OFF;

    /* A pending RXNE byte in DR must survive the next normal DMA arm. The
     * Unicorn runner additionally asserts that production never reads DR. */
    /* First-ever Open starts with a zero handle and inherited non-output mux.
     * Reset must reach LOW before enable; UART/MSP must follow both writes. */
    CHECK(huart1.Instance==NULL&&huart1.gState==HAL_UART_STATE_RESET);
    CHECK(BSP_BT_HCI_SetBaud(115200)!=0&&BSP_BT_HCI_SetFlowControl(0)!=0);
    CHECK(!BSP_BT_HCI_IsQuiescent()&&BSP_BT_HCI_Close()==0);
    BSP_BT_HCI_SetPaused(1);
    CHECK(!BSP_BT_HCI_IsPaused()&&GPIOA->MODER==0&&GPIOI->MODER==0);
    GPIOA->MODER=3U<<16;GPIOA->ODR=GPIO_PIN_8;GPIOA->IDR=GPIO_PIN_8|GPIO_PIN_10|GPIO_PIN_11;
    GPIOI->MODER=2U<<2;GPIOI->ODR=0;GPIOI->IDR=0;
    USART1->SR=0;USART1->DR=0;now=1000;dma_count=0;
    WakeFromISR();CHECK(notifications==0&&yield_calls==0);
    initial_baud=115200;CHECK(Open()==0);
    CHECK(abort_calls==0&&uart_init_calls==1&&uart_msp_calls==1&&!uart_init_order_errors&&!enable_order_errors);
    CHECK(huart1.Instance==USART1&&huart1.Init.BaudRate==115200&&huart1.Init.Mode==UART_MODE_TX_RX);
    CHECK(huart1.Init.WordLength==UART_WORDLENGTH_8B&&huart1.Init.StopBits==UART_STOPBITS_1&&
          huart1.Init.Parity==UART_PARITY_NONE&&huart1.Init.OverSampling==UART_OVERSAMPLING_16);
    CHECK(transport_owner==(TaskHandle_t)0x1234U);
    WakeFromISR();CHECK(notifications==1&&!notification_input_invalid&&!notification_args_invalid&&yield_value==pdFALSE);
    should_wake=1;WakeFromISR();CHECK(notifications==2&&yield_calls==2&&yield_value==pdTRUE);
    should_wake=0;
    CHECK(now==1000&&delay10_count==0&&delay150_count==0&&reset_releases==0);
    CHECK((GPIOI->ODR&GPIO_PIN_1)!=0&&(GPIOA->ODR&GPIO_PIN_8)==0);
    CHECK(BSP_BT_HCI_Send(input,4)!=0);
    CHECK(BSP_BT_HCI_Receive(output,1)==0);
    CHECK(dma_arm_at==1000&&reset_release_at==1010&&now==1160);
    CHECK(reset_releases==1&&reset_without_rx==0&&delay10_count==1&&delay150_count==0&&delay_until_calls==8);
    CHECK(sizeof(BSP_BT_HCI_StartupTrace)==472&&sizeof(BSP_BT_HCI_StartupSample)==28);
    CHECK(g_bsp_bt_hci_startup.magic==0x42545431U&&g_bsp_bt_hci_startup.version==1);
    CHECK(g_bsp_bt_hci_fault.magic==0x42464631U&&g_bsp_bt_hci_fault.version==1);
    CHECK(g_bsp_bt_hci_startup.attempt==1&&g_bsp_bt_hci_startup.sample_count==14&&g_bsp_bt_hci_startup.flags==1);
    CHECK(!(g_bsp_bt_hci_startup.sequence&1U));
    CHECK(g_bsp_bt_hci_startup.samples[1].stage==2&&!(g_bsp_bt_hci_startup.samples[1].gpioa_idr&GPIO_PIN_8));
    CHECK(g_bsp_bt_hci_startup.samples[2].stage==3&&(g_bsp_bt_hci_startup.samples[2].gpioi_idr&GPIO_PIN_1));
    static const uint8_t expected_offsets[]={1,2,5,10,20,50,100,150};
    for(unsigned i=0;i<8;i++){
        CHECK(g_bsp_bt_hci_startup.samples[6+i].stage==7);
        CHECK(g_bsp_bt_hci_startup.samples[6+i].tick_ms==reset_release_at+expected_offsets[i]);
    }
    CHECK(!(g_bsp_bt_hci_startup.samples[8].gpioa_idr&GPIO_PIN_11)&&
          (g_bsp_bt_hci_startup.samples[11].gpioa_idr&GPIO_PIN_11));
    CHECK(g_bsp_bt_hci.reset_count==1&&g_bsp_bt_hci.opened==1);
    huart1.hdmarx->XferCpltCallback(huart1.hdmarx);
    CHECK(BSP_BT_HCI_TakeEvents()==1);
    USART1->SR=USART_SR_RXNE;USART1->DR=0x5a;
    GPIOA->MODER=0;g_bsp_bt_hci.opened=1;
    CHECK(BSP_BT_HCI_Receive(output,3)==0);
    CHECK(dma_destination==(uint32_t)output&&dma_count==3);
    CHECK(huart1.RxState==HAL_UART_STATE_BUSY_RX&&g_bsp_bt_hci.rx_busy==1);
    CHECK((GPIOA->MODER&(3UL<<24))==(2UL<<24));
    CHECK(BSP_BT_HCI_Receive(output,1)!=0);
    huart1.hdmarx->XferCpltCallback(huart1.hdmarx);
    CHECK((GPIOA->MODER&(3UL<<24))==(1UL<<24));
    CHECK(!g_bsp_bt_hci.rx_busy&&huart1.RxState==HAL_UART_STATE_READY);
    CHECK(BSP_BT_HCI_TakeEvents()==1&&BSP_BT_HCI_TakeEvents()==0);
    dma_failure=1;CHECK(BSP_BT_HCI_Receive(output,2)!=0&&!g_bsp_bt_hci.rx_busy);dma_failure=0;
    USART1->SR=USART_SR_ORE;CHECK(BSP_BT_HCI_Receive(output,2)!=0);
    USART1->SR=0;CHECK(BSP_BT_HCI_Receive(output,2)==0);
    huart1.hdmarx->XferErrorCallback(huart1.hdmarx);
    CHECK(BSP_BT_HCI_TakeEvents()==4&&g_bsp_bt_hci.last_hal_error==HAL_UART_ERROR_DMA);
    CHECK(notification_input_invalid==0&&notification_args_invalid==0);
    /* Public quiesce is invoked by storage; only the emulated BT-owner poll
     * changes GPIO. Test the asynchronous mailbox and IRQ-disabled writer's
     * required guard without actually masking IRQs or touching flash. */
    uint32_t token=99,other=99;
    CHECK(Bluetooth_QuiesceTransport(0,&token)==BLUETOOTH_INVALID);
    CHECK(Bluetooth_QuiesceTransport(20,NULL)==BLUETOOTH_INVALID);
    CHECK(Bluetooth_QuiesceTransport(20,&token)==BLUETOOTH_INVALID); /* BT owner */
    caller_task=(TaskHandle_t)0x5678U;
    g_bluetooth.state=BLUETOOTH_STATE_FAULT;
    CHECK(Bluetooth_QuiesceTransport(20,&token)==BLUETOOTH_NOT_READY&&token==0);
    g_bluetooth.state=BLUETOOTH_STATE_READY;USART1->SR=USART_SR_TC;
    fault_pending=0;g_bsp_bt_hci.rx_busy=0;quiesce_autopoll=1;
    uint32_t before_pause=now;
    CHECK(Bluetooth_QuiesceTransport(20,&token)==0&&token!=0);
    CHECK(now-before_pause>=2U&&now-before_pause<=20U&&BSP_BT_HCI_IsPaused());
    CHECK(((GPIOA->MODER>>24)&3U)==1U&&(GPIOA->BSRR&GPIO_PIN_12));
    CHECK(Bluetooth_QuiesceTransport(20,&other)==BLUETOOTH_BUSY&&other==0);
    CHECK(Bluetooth_ResumeTransport(token+1U)==BLUETOOTH_INVALID&&BSP_BT_HCI_IsPaused());
    CHECK(BSP_BT_HCI_Receive(output,3)==0&&((GPIOA->MODER>>24)&3U)==1U);
    huart1.hdmarx->XferCpltCallback(huart1.hdmarx);
    Poll(NULL,DATA_SOURCE_CALLBACK_POLL);
    CHECK(BSP_BT_HCI_Receive(output,4)==0&&((GPIOA->MODER>>24)&3U)==1U);
    uint32_t before_tx=tx_submissions;
    Send(input,4);CHECK(tx_submissions==before_tx&&deferred_tx==input&&deferred_tx_size==4U&&!fault_pending);
    CHECK(BSP_BT_HCI_Send(input,4)!=0); /* Direct starts remain guarded too. */
    CHECK(Bluetooth_ResumeTransport(token)==0&&!BSP_BT_HCI_IsPaused());
    CHECK(((GPIOA->MODER>>24)&3U)==2U&&tx_submissions==before_tx+1U&&transmit_buffer==input&&transmit_length==4U);
    CHECK(deferred_tx_size==0&&g_bsp_bt_hci.tx_busy);
    /* Active TX and an undrained/error RX byte cannot be acknowledged. A
     * timeout cancels its token; later owner service releases RTS again. */
    CHECK(Bluetooth_QuiesceTransport(5,&token)==BLUETOOTH_BUSY&&token==0&&pause_request==0);
    Poll(NULL,DATA_SOURCE_CALLBACK_POLL);CHECK(!BSP_BT_HCI_IsPaused());
    BSP_BT_HCI_OnTxComplete(&huart1);Poll(NULL,DATA_SOURCE_CALLBACK_POLL);
    USART1->SR=USART_SR_TC|USART_SR_RXNE;
    CHECK(Bluetooth_QuiesceTransport(5,&token)==BLUETOOTH_BUSY&&token==0);
    Poll(NULL,DATA_SOURCE_CALLBACK_POLL);USART1->SR=USART_SR_TC|USART_SR_ORE;
    CHECK(Bluetooth_QuiesceTransport(5,&token)==BLUETOOTH_BUSY&&token==0);
    Poll(NULL,DATA_SOURCE_CALLBACK_POLL);USART1->SR=USART_SR_TC;
    CHECK(Bluetooth_QuiesceTransport(20,&token)==0);
    quiesce_autopoll=0;uint32_t before_timeout=now;
    CHECK(Bluetooth_ResumeTransport(token)==BLUETOOTH_BUSY&&now-before_timeout==100U);
    Poll(NULL,DATA_SOURCE_CALLBACK_POLL);CHECK(!BSP_BT_HCI_IsPaused());
    /* The first failure snapshot retains active transport registers after
     * Close resets live state, never reads DR, and ignores follow-on faults. */
    USART1->SR=USART_SR_TC;USART1->CR3=0x3c1;DMA2_Stream7->NDTR=3;
    BSP_BT_HCI_CaptureFault(0x302);CHECK(g_bsp_bt_hci_fault.reason==0x302);
    CHECK(g_bsp_bt_hci_fault.transport.opened==1&&g_bsp_bt_hci_fault.dma_tx[1]==3);
    CHECK(Close()==0&&transport_owner==NULL);
    CHECK(g_bsp_bt_hci_startup.flags==7&&g_bsp_bt_hci_startup.sample_count==16);
    BSP_BT_HCI_CaptureFault(0x999);CHECK(g_bsp_bt_hci_fault.reason==0x302);
    CHECK(g_bsp_bt_hci_fault.uart_cr3==0x3c1&&!(g_bsp_bt_hci_fault.sequence&1));
    uint32_t before_wake=notifications;WakeFromISR();CHECK(notifications==before_wake);
    /* Reopen retains initialized DMA/MSP handles and uses USART-only reset.
     * Delayed scheduling is visible, without shifting the final150ms target. */
    CHECK(Open()==0&&g_bsp_bt_hci_startup.attempt==2&&g_bsp_bt_hci_startup.sample_count==4);
    CHECK(uart_init_calls==2&&uart_msp_calls==1&&!uart_init_order_errors&&!enable_order_errors);
    startup_late=30;CHECK(BSP_BT_HCI_Receive(output,1)==0);
    CHECK(g_bsp_bt_hci_startup.samples[6].tick_ms==reset_release_at+31U);
    CHECK(g_bsp_bt_hci_startup.samples[13].tick_ms==reset_release_at+150U);
    CHECK(Close()==0);
    CHECK(critical_depth==0&&critical_errors==0);
    /* Pending inbound cancel retains ownership until ACL cleanup. */
    unsigned er=BLUETOOTH_PHONE;
    memset(links,0,sizeof(links));memset((void*)g_bluetooth.links,0,sizeof(g_bluetooth.links));
    g_bluetooth.links[er].status=BLUETOOTH_LINK_CONNECTING;
    g_bluetooth.links[er].address[0]=0xa1;pending_exists=1;
    pending_connection.state=OPEN;pending_connection.con_handle=7;
    Request cancel={.operation=OP_DISCONNECT,.role=BLUETOOTH_PHONE};ProcessRequest(&cancel);
    CHECK(links[er].cancelling);
    CHECK(g_bluetooth.links[er].status==BLUETOOTH_LINK_CANCELLING&&ByAddress((const uint8_t*)g_bluetooth.links[er].address)==(int)er);
    PendingTick(er);CHECK(acl_disconnects==1&&links[er].cancel_sent);
    pending_exists=0;PendingTick(er);CHECK(!links[er].cancelling);
    CHECK(g_bluetooth.links[er].status==BLUETOOTH_LINK_OFF&&g_bluetooth.links[er].last_error==BLUETOOTH_ERROR_LOCAL_CANCEL);
    /* Late RFCOMM open is disconnected and never increments a usable epoch. */
    links[er].deadline=now+15000;g_bluetooth.links[er].cid=71;
    g_bluetooth.links[er].status=BLUETOOTH_LINK_CONNECTING;pending_connection.state=OPEN;pending_exists=1;
    ProcessRequest(&cancel);uint32_t before_reject=disconnects,epoch=g_bluetooth.links[er].reconnects;
    Opened(er,71,512);CHECK(disconnects==before_reject+1&&g_bluetooth.links[er].status==BLUETOOTH_LINK_CANCELLING);
    CHECK(g_bluetooth.links[er].reconnects==epoch);
    PendingTick(er);CHECK(acl_disconnects==2);
    uint8_t rfclosed[]={RFCOMM_EVENT_CHANNEL_CLOSED,2,71,0};Packet(HCI_EVENT_PACKET,0,rfclosed,sizeof(rfclosed));
    CHECK(links[er].cancelling&&g_bluetooth.links[er].cid==0);
    pending_exists=0;PendingTick(er);
    CHECK(g_bluetooth.links[er].status==BLUETOOTH_LINK_OFF);
    /* A new inbound open increments its session epoch after cleanup.
     * PHONE cancellation must reject a late inbound open as well. */
    g_bluetooth.links[er].reconnects=epoch;Opened(er,72,512);
    CHECK(g_bluetooth.links[er].status==BLUETOOTH_LINK_UP&&g_bluetooth.links[er].reconnects==epoch+1);
    CancelRole(BLUETOOTH_PHONE,BLUETOOTH_ERROR_LOCAL_CANCEL);before_reject=disconnects;
    Opened(BLUETOOTH_PHONE,88,512);CHECK(disconnects==before_reject+1&&g_bluetooth.links[BLUETOOTH_PHONE].status!=BLUETOOTH_LINK_UP);
    Retry(BLUETOOTH_PHONE,0);
    /* Stage timeout starts one bounded cleanup; polls do not extend it. */
    links[er].deadline=now+10;g_bluetooth.links[er].status=BLUETOOTH_LINK_CONNECTING;
    pending_exists=1;pending_connection.state=SENT_CREATE_CONNECTION;links[er].cancel_sent=0;
    now+=11;PendingTick(er);CHECK(links[er].cancelling&&create_cancels==1);
    uint32_t fixed_deadline=links[er].cancel_deadline;PendingTick(er);
    CHECK(links[er].cancel_deadline==fixed_deadline&&create_cancels==1);
    /* Connection completion can win the create-cancel race; follow it with
     * ACL disconnect, not a stale 'cancel sent' flag and global timeout. */
    pending_connection.state=OPEN;PendingTick(er);
    CHECK(acl_disconnects==3&&links[er].cancel_sent==1&&links[er].cancel_deadline==fixed_deadline);
    now=fixed_deadline;PendingTick(er);CHECK(fault_pending==BLUETOOTH_ERROR_CANCEL_TIMEOUT);
    CHECK(g_bluetooth.links[er].status==BLUETOOTH_LINK_CANCELLING);
    /* Real owner mailbox -> existing queue -> dispatch, then real HCI event
     * callback completion. Duplicate/BUSY requests cannot replay power-on or
     * steal an active operation's ID. No hardware success is inferred. */
    requests=(QueueHandle_t)1;g_bluetooth.state=BLUETOOTH_STATE_FAULT;mock_hci_state=HCI_STATE_OFF;
    fault_pending=0;queue_failure=0;
    CHECK(Bluetooth_RequestStart(0)==BLUETOOTH_INVALID);
    CHECK(Bluetooth_RequestStart(1)==0&&g_bluetooth_control.ack_sequence==0&&power_ons==0);
    CHECK(Bluetooth_RequestStart(2)==BLUETOOTH_BUSY);
    PollControl();CHECK(g_bluetooth_control.ack_sequence==1&&!g_bluetooth_control.accept_result);
    CHECK(g_bluetooth_control.phase==1&&g_bluetooth_control.operation_id==1&&power_ons==0);
    Request queued;memcpy(&queued,last_request,sizeof(queued));ProcessRequest(&queued);
    CHECK(g_bluetooth_control.phase==2&&g_bluetooth.state==BLUETOOTH_STATE_STARTING&&power_ons==1);
    g_bluetooth_control.request_sequence=2;PollControl();
    CHECK(g_bluetooth_control.accept_result==BLUETOOTH_BUSY&&g_bluetooth_control.operation_id==1);
    ProcessRequest(&queued);CHECK(power_ons==1); /* old queued START cannot replay */
    uint8_t ready[]={BTSTACK_EVENT_STATE,1,HCI_STATE_WORKING};
    Packet(HCI_EVENT_PACKET,0,ready,sizeof(ready));
    CHECK(g_bluetooth_control.phase==3&&g_bluetooth_control.completion_sequence==1&&!g_bluetooth_control.completion_result);
    CHECK(g_bluetooth.state==BLUETOOTH_STATE_READY);
    CHECK(!memcmp(local_name,"FuckNudo CFW 01710F",sizeof(local_name)));
    CHECK(Bluetooth_RequestStart(3)==0);PollControl();
    CHECK(g_bluetooth_control.accept_result==BLUETOOTH_NOT_READY&&power_ons==1);
    CHECK(g_bluetooth_control.operation_id==1&&g_bluetooth_control.completion_sequence==1);
    g_bluetooth.state=BLUETOOTH_STATE_FAULT;mock_hci_state=HCI_STATE_HALTING;
    CHECK(Bluetooth_RequestStart(4)==0);PollControl();CHECK(g_bluetooth_control.accept_result==BLUETOOTH_NOT_READY);
    mock_hci_state=HCI_STATE_OFF;queue_failure=1;
    CHECK(Bluetooth_RequestStart(5)==0);PollControl();CHECK(g_bluetooth_control.accept_result==BLUETOOTH_QUEUE_FULL);
    queue_failure=0;CHECK(Bluetooth_RequestStart(6)==0);PollControl();memcpy(&queued,last_request,sizeof(queued));
    ProcessRequest(&queued);CHECK(power_ons==2&&g_bluetooth_control.phase==2);
    FailController(0x302);CHECK(g_bluetooth_control.phase==4&&g_bluetooth_control.completion_sequence==6);
    CHECK(g_bluetooth_control.completion_result==0x302&&power_ons==2);
    Packet(HCI_EVENT_PACKET,0,ready,sizeof(ready));CHECK(g_bluetooth.state==BLUETOOTH_STATE_FAULT);
    PollControl();CHECK(power_ons==2); /* no automatic retry after fault */
    g_bluetooth_control.command=99;g_bluetooth_control.request_sequence=7;PollControl();
    CHECK(g_bluetooth_control.accept_result==BLUETOOTH_INVALID&&g_bluetooth_control.completion_sequence==6);
    g_bluetooth_control.command=1;g_bluetooth_control.version=2;g_bluetooth_control.request_sequence=8;PollControl();
    CHECK(g_bluetooth_control.accept_result==BLUETOOTH_INVALID);g_bluetooth_control.version=1;
    CHECK(Bluetooth_RequestStart(8)==BLUETOOTH_INVALID&&Bluetooth_RequestStart(7)==BLUETOOTH_INVALID);
    CHECK(!(g_bluetooth_control.result_sequence&1));
    Bluetooth_ControlMailbox control;Bluetooth_GetControl(&control);
    CHECK(control.completion_sequence==6&&control.phase==4&&sizeof(control)==48);
    return 0;
}
static uint32_t AbortTest(uint32_t timeout){
    uint8_t retained[4]={1,2,3,4};
    huart1.Instance=USART1;huart1.hdmarx=&hdma_usart1_rx;huart1.hdmatx=&hdma_usart1_tx;
    hdma_usart1_rx.Instance=DMA2_Stream5;hdma_usart1_tx.Instance=DMA2_Stream7;
    initial_baud=115200;CHECK(Open()==0);
    DMA2_Stream5->CR=DMA_SxCR_EN;DMA2_Stream7->CR=DMA_SxCR_EN;
    USART1->CR3|=USART_CR3_DMAR|USART_CR3_DMAT;
    g_bsp_bt_hci.tx_busy=1;g_bsp_bt_hci.rx_busy=1;
    deferred_tx=retained;deferred_tx_size=4;
    abort_failure=timeout;abort_leaves_enabled=!timeout;
    CHECK(Close()!=0&&BSP_BT_HCI_IsQuarantined());
    CHECK(g_bsp_bt_hci.tx_busy==1&&g_bsp_bt_hci.rx_busy==1&&!g_bsp_bt_hci.opened);
    CHECK(!(USART1->CR3&(USART_CR3_DMAR|USART_CR3_DMAT)));
    CHECK(deferred_tx==retained&&deferred_tx_size==4);
    CHECK(g_bsp_bt_hci_fault.reason==0x105&&g_bsp_bt_hci_fault.dma_rx[0]&DMA_SxCR_EN);
    CHECK(g_bsp_bt_hci_fault.dma_tx[0]&DMA_SxCR_EN);
    /* Even if disable eventually cleared EN, the failure latch forbids reuse
     * and does not pretend HAL state or a later success erases the failure. */
    abort_failure=abort_leaves_enabled=0;
    CHECK(BSP_BT_HCI_Open()!=0&&BSP_BT_HCI_IsQuarantined());
    CHECK(BSP_BT_HCI_Send(retained,4)!=0&&BSP_BT_HCI_Receive(retained,4)!=0);
    CHECK(Close()!=0&&deferred_tx==retained&&deferred_tx_size==4);
    /* A real STOP request followed by BTstack's OFF notification must still
     * report failed transport shutdown; HCI discards the Close return code. */
    g_bluetooth.state=BLUETOOTH_STATE_READY;g_bluetooth.last_error=0;
    Request stop={.operation=OP_STOP};ProcessRequest(&stop);
    CHECK(g_bluetooth.state==BLUETOOTH_STATE_STOPPING);
    uint8_t off[]={BTSTACK_EVENT_STATE,1,HCI_STATE_OFF};
    Packet(HCI_EVENT_PACKET,0,off,sizeof(off));
    CHECK(g_bluetooth.state==BLUETOOTH_STATE_FAULT&&g_bluetooth.last_error==0x105);
    g_bluetooth.last_error=0x302;Packet(HCI_EVENT_PACKET,0,off,sizeof(off));
    CHECK(g_bluetooth.state==BLUETOOTH_STATE_FAULT&&g_bluetooth.last_error==0x302);
    requests=(QueueHandle_t)1;g_bluetooth.state=BLUETOOTH_STATE_FAULT;
    CHECK(Bluetooth_RequestStart(1)==0);PollControl();
    CHECK(g_bluetooth_control.accept_result==BLUETOOTH_NOT_READY&&power_ons==0);
    return 0;
}
uint32_t test_abort_timeout(void){return AbortTest(1);}
uint32_t test_abort_enabled(void){return AbortTest(0);}
uint32_t test_initial_active(void){
    CHECK(huart1.Instance==NULL);
    DMA2_Stream5->CR=DMA_SxCR_EN;
    CHECK(BSP_BT_HCI_Open()!=0&&BSP_BT_HCI_IsQuarantined());
    CHECK(abort_calls==0&&uart_init_calls==0&&huart1.Instance==NULL);
    CHECK(g_bsp_bt_hci_fault.reason==0x105&&(g_bsp_bt_hci_fault.dma_rx[0]&DMA_SxCR_EN));
    CHECK(g_bsp_bt_hci_startup.attempt==1&&g_bsp_bt_hci_startup.flags==2);
    CHECK(g_bsp_bt_hci_startup.sample_count==2&&g_bsp_bt_hci_startup.samples[1].stage==8);
    CHECK(BSP_BT_HCI_Close()!=0&&BSP_BT_HCI_Open()!=0);
    CHECK(!BSP_BT_HCI_IsQuiescent()&&g_bsp_bt_hci_startup.attempt==1);
    return 0;
}
static void RawSetup(uint32_t mode){
    raw_mode=mode;raw_started=raw_sent=raw_bad_payload=0;dma_failure=0;
    requests=(QueueHandle_t)1;owner=(TaskHandle_t)0x1234U;
    g_bluetooth.state=BLUETOOTH_STATE_FAULT;mock_hci_state=HCI_STATE_OFF;
    USART1->SR=USART_SR_TC|USART_SR_TXE;
}
static uint32_t RawOne(uint32_t mode,uint32_t expected,uint32_t seq){
    RawSetup(mode);uint32_t tx_before=tx_submissions;
    CHECK(Bluetooth_RequestResetDiagnostic(seq)==0);PollControl();
    CHECK(g_bluetooth_control.accept_result==0&&raw_request_active);
    Request raw=*(Request*)last_request;
    CHECK(raw.operation==OP_RAW_RESET&&raw.value==seq);
    Request legacy_start={.operation=OP_START};
    CHECK(Bluetooth_Stop()==BLUETOOTH_BUSY&&Enqueue(&legacy_start)==BLUETOOTH_BUSY);
    CHECK(Bluetooth_RequestStart(seq+1)==BLUETOOTH_BUSY);
    Request old_start={.operation=OP_START};ProcessRequest(&old_start);
    CHECK(power_ons==0&&g_bluetooth.state==BLUETOOTH_STATE_FAULT);
    ProcessRequest(&raw);
    const volatile BSP_BT_ResetDiagnostic *d=&g_bsp_bt_reset_diagnostic;
    CHECK(d->operation_id==seq&&d->request_sequence==seq&&d->phase==6&&!(d->sequence&1));
    CHECK(d->result==expected&&g_bluetooth_control.completion_result==(int)expected);
    CHECK(g_bluetooth_control.completion_sequence==seq&&!raw_request_active);
    CHECK(g_bluetooth_control.phase==(expected?4U:3U)&&g_bluetooth.state==BLUETOOTH_STATE_FAULT);
    CHECK(d->tx_submit_count==1&&d->tx_requested_bytes==4&&tx_submissions==tx_before+1);
    CHECK(!raw_bad_payload&&power_ons==0&&power_offs==0&&Bluetooth_TransportIsDetached());
    CHECK(d->flow_restore==1&&(d->cr3_before&USART_CR3_CTSE)&&!(d->cr3_bypass&USART_CR3_CTSE));
    CHECK((d->cr3_restored&USART_CR3_CTSE)&&!(USART1->CR3&USART_CR3_CTSE));
    CHECK(!d->close_result&&d->data_valid&&!(DMA2_Stream5->CR&DMA_SxCR_EN)&&!(DMA2_Stream7->CR&DMA_SxCR_EN));
    CHECK(d->elapsed_ms<=660&&d->elapsed_ms>=160);
    CHECK(!g_bsp_bt_hci.opened&&!(GPIOA->IDR&GPIO_PIN_8)&&!(GPIOI->IDR&GPIO_PIN_1));
    if(mode!=4&&mode!=5){CHECK(d->tx_completed_bytes==4&&d->dma_tx_remaining==0&&(d->uart_sr&USART_SR_TC));}
    else CHECK(d->tx_completed_bytes==0);
    if(mode==1){CHECK(d->rx_captured_bytes==7&&d->raw_data[3]==2&&d->reset_status==0&&d->hal_error==0);}
    if(mode==2)CHECK(d->rx_captured_bytes==0&&d->reset_status==UINT32_MAX);
    if(mode==7)CHECK(d->reset_status==12);
    if(mode==9||mode==10)CHECK(d->primary_result==BSP_BT_RESET_UART_ERROR&&(d->uart_sr&USART_SR_FE));
    BSP_BT_ResetDiagnostic snapshot;
    CHECK(Bluetooth_GetResetDiagnosticSnapshot(&snapshot)==1&&snapshot.operation_id==seq&&snapshot.result==expected);
    return 0;
}
uint32_t test_raw(void){
    /* Prior attempt error must not turn a clean Reset response into failure. */
    g_bsp_bt_hci.errors=9;g_bsp_bt_hci.last_hal_error=HAL_UART_ERROR_FE;
    const uint32_t results[]={0,0,7,8,5,6,9,10};
    for(uint32_t mode=1;mode<=7;mode++){uint32_t failure=RawOne(mode,results[mode],mode*10);if(failure)return failure;}
    for(uint32_t mode=9;mode<=10;mode++){uint32_t failure=RawOne(mode,BSP_BT_RESET_UART_ERROR,mode*10);if(failure)return failure;}
    CHECK(!critical_depth&&!critical_errors&&raw_api_result==(uint32_t)BLUETOOTH_BUSY);
    /* Active H4 rejects raw before opening or replacing its evidence. */
    mock_hci_state=HCI_STATE_WORKING;
    CHECK(Bluetooth_RequestResetDiagnostic(110)==0);PollControl();
    CHECK(g_bluetooth_control.accept_result==BLUETOOTH_NOT_READY&&g_bsp_bt_reset_diagnostic.operation_id==100);
    mock_hci_state=HCI_STATE_OFF;transport_owner=(TaskHandle_t)0x1234;
    CHECK(Bluetooth_RequestResetDiagnostic(120)==0);PollControl();
    CHECK(g_bluetooth_control.accept_result==BLUETOOTH_NOT_READY&&tx_submissions==9);
    transport_owner=NULL;dma_failure=1;raw_mode=1;raw_started=raw_sent=0;
    CHECK(Bluetooth_RequestResetDiagnostic(130)==0);PollControl();Request raw=*(Request*)last_request;ProcessRequest(&raw);
    CHECK(g_bsp_bt_reset_diagnostic.result==BSP_BT_RESET_RX_ARM&&g_bsp_bt_reset_diagnostic.tx_submit_count==0);
    CHECK(g_bsp_bt_reset_diagnostic.flow_restore==0&&!g_bsp_bt_reset_diagnostic.close_result&&!g_bsp_bt_hci.opened);
    CHECK(sizeof(BSP_BT_ResetDiagnostic)==192);
    BSP_BT_ResetDiagnostic sentinel;memset(&sentinel,0x5a,sizeof(sentinel));
    uint32_t record_seq=g_bsp_bt_reset_diagnostic.sequence;
    CHECK(BSP_BT_ResetDiagnosticRun(0)==BSP_BT_RESET_CONTEXT&&g_bsp_bt_reset_diagnostic.sequence==record_seq);
    __set_PRIMASK(1);uint32_t rejected=BSP_BT_ResetDiagnosticRun(200);__set_PRIMASK(0);
    CHECK(rejected==BSP_BT_RESET_CONTEXT&&g_bsp_bt_reset_diagnostic.sequence==record_seq);
    g_bsp_bt_reset_diagnostic.sequence++;
    CHECK(Bluetooth_GetResetDiagnosticSnapshot(&sentinel)==0&&sentinel.magic==0x5a5a5a5aU);
    g_bsp_bt_reset_diagnostic.sequence++;
    CHECK(Bluetooth_GetResetDiagnosticSnapshot(NULL)==0);
    return 0;
}
uint32_t test_raw_abort(void){
    RawSetup(8);
    CHECK(Bluetooth_RequestResetDiagnostic(1)==0);PollControl();Request raw=*(Request*)last_request;ProcessRequest(&raw);
    CHECK(g_bsp_bt_reset_diagnostic.result==BSP_BT_RESET_CLEANUP);
    CHECK(g_bsp_bt_reset_diagnostic.close_result==-1&&g_bsp_bt_reset_diagnostic.data_valid==0);
    CHECK(g_bsp_bt_reset_diagnostic.tx_submit_count==1&&g_bsp_bt_reset_diagnostic.flow_restore==1);
    CHECK(g_bluetooth.state==BLUETOOTH_STATE_FAULT&&g_bluetooth.last_error==0x105&&BSP_BT_HCI_IsQuarantined());
    CHECK(!g_bsp_bt_hci.opened&&g_bsp_bt_hci.rx_busy&&g_bsp_bt_hci.tx_busy==0);
    uint32_t seq=g_bsp_bt_reset_diagnostic.sequence;
    CHECK(Bluetooth_RequestResetDiagnostic(2)==0);PollControl();
    CHECK(g_bluetooth_control.accept_result==BLUETOOTH_NOT_READY&&g_bsp_bt_reset_diagnostic.sequence==seq);
    CHECK(Bluetooth_RequestStart(3)==0);PollControl();
    CHECK(g_bluetooth_control.accept_result==BLUETOOTH_NOT_READY&&power_ons==0&&tx_submissions==1);
    return 0;
}
''')
(OUT/'test.ld').write_text('MEMORY { FLASH(rx): ORIGIN = 0x08000000, LENGTH = 128K\nRAM(rwx): ORIGIN = 0x20000000, LENGTH = 128K }\nSECTIONS { .text : { *(.text*) *(.rodata*) } > FLASH\n.data : { *(.data*) } > RAM\n.bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM\n}\n')
manifest=json.loads((PROJECT/'Middlewares/Noodoe/Bluetooth/module.build.json').read_text())
includes=[OUT,PROJECT/'Middlewares/Noodoe/Health/inc',PROJECT/'Middlewares/Noodoe/Power/inc',PROJECT/'Middlewares/Noodoe/Bluetooth/src',PROJECT/'Drivers/BSP/inc',PROJECT/'Core/Inc',PROJECT/'Drivers/CMSIS/Include',PROJECT/'Drivers/CMSIS/Device/ST/STM32F4xx/Include',PROJECT/'Drivers/STM32F4xx_HAL_Driver/Inc']+[PROJECT/p for p in manifest['include_paths']]
sources=[PROJECT/'Middlewares/Noodoe/Bluetooth/src/bluetooth_patch.c',PROJECT/'Middlewares/Noodoe/Bluetooth/src/bluetooth_keys.c',PROJECT/'Drivers/BSP/src/bsp_bt_hci.c',PROJECT/'Drivers/BSP/src/BSP_BT_ResetDiagnostic.c',PROJECT/'Middlewares/Third_Party/BTstack/src/btstack_util.c']
results=[]
for opt in ['-O0','-Os']:
    elf=OUT/(opt[1:]+'.elf')
    cmd=[str(TC/'arm-none-eabi-gcc.exe'),'-mcpu=cortex-m4','-mthumb','-std=gnu11',opt,'-ffreestanding','-fno-builtin','-ffunction-sections','-fdata-sections','-nostdlib','-DSTM32F429xx','-DUSE_HAL_DRIVER']
    for inc in includes:cmd+=['-I',str(inc)]
    cmd += [str(s) for s in sources]+[str(OUT/'test.c'),'-T',str(OUT/'test.ld'),'-Wl,-e,test_main,--gc-sections','-Wl,-u,get_assertions,-u,test_abort_timeout,-u,test_abort_enabled,-u,test_initial_active,-u,test_raw,-u,test_raw_abort,-u,test_retained','-lgcc','-o',str(elf)]
    r=subprocess.run(cmd,capture_output=True,text=True)
    if r.returncode:raise RuntimeError(r.stdout+r.stderr)
    nm=subprocess.check_output([str(TC/'arm-none-eabi-nm.exe'),str(elf)],text=True)
    symbols={p[2]:int(p[0],16) for line in nm.splitlines() if len(p:=line.split())==3}
    data=elf.read_bytes();phoff=struct.unpack_from('<I',data,28)[0];phsize,phnum=struct.unpack_from('<HH',data,42)
    for entry in ['test_main','test_abort_timeout','test_abort_enabled','test_initial_active','test_raw','test_raw_abort','test_retained']:
        uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
        uc.mem_map(0x08000000,0x20000);uc.mem_map(0x20000000,0x20000);uc.mem_map(0x40000000,0x40000)
        for i in range(phnum):
            kind,offset,va,pa,filesz,memsz,flags,align=struct.unpack_from('<8I',data,phoff+i*phsize)
            if kind==1 and filesz:uc.mem_write(va,data[offset:offset+filesz])
        dr_reads=[];stale_dr_reads=[];resets=[]
        def on_dr_read(u,access,address,size,value,context):
            pending=struct.unpack('<I',u.mem_read(0x40011000,4))[0]&0x20
            (dr_reads if pending else stale_dr_reads).append(address)
        def on_reset(u,access,address,size,value,context):
            old=struct.unpack('<I',u.mem_read(address,4))[0]
            if (old^value)&~0x10:raise RuntimeError('Reset changed an unrelated APB2 peripheral')
            if value&0x10:
                if any(struct.unpack('<I',u.mem_read(a,4))[0]&1 for a in [0x40026488,0x400264b8]):
                    raise RuntimeError('USART reset before both DMA streams were disabled')
                resets.append(value)
                u.mem_write(0x40011000,bytes(0x1c))
        uc.hook_add(UC_HOOK_MEM_READ,on_dr_read,begin=0x40011004,end=0x40011007)
        uc.hook_add(UC_HOOK_MEM_WRITE,on_reset,begin=0x40023824,end=0x40023827)
        trace_address=symbols['g_bsp_bt_hci_startup']
        def on_trace_write(u,access,address,size,value,context):
            sequence=struct.unpack('<I',u.mem_read(trace_address+8,4))[0]
            if address==trace_address+8:
                if bool(sequence&1)==bool(value&1):raise RuntimeError('Trace sequence did not alternate publication phases')
            elif not sequence&1:
                raise RuntimeError('Trace data changed outside odd publication sequence')
        uc.hook_add(UC_HOOK_MEM_WRITE,on_trace_write,begin=trace_address,end=trace_address+471)
        raw_address=symbols['g_bsp_bt_reset_diagnostic']
        def on_raw_write(u,access,address,size,value,context):
            sequence=struct.unpack('<I',u.mem_read(raw_address+8,4))[0]
            if address!=raw_address+8 and not sequence&1:
                raise RuntimeError('Raw diagnostic changed outside odd publication sequence')
        uc.hook_add(UC_HOOK_MEM_WRITE,on_raw_write,begin=raw_address,end=raw_address+191)
        uc.reg_write(UC_ARM_REG_XPSR,0x1000000);uc.reg_write(UC_ARM_REG_SP,0x2001f000);uc.reg_write(UC_ARM_REG_LR,0x0801fff1)
        uc.emu_start(symbols[entry]|1,0x0801fff0,count=6000000)
        if uc.reg_read(UC_ARM_REG_R0):raise RuntimeError(f'{opt} {entry}: harness line {uc.reg_read(UC_ARM_REG_R0)} failed')
        if dr_reads:raise RuntimeError(f'Pending DR byte was read/cleared: {dr_reads}')
        expected_resets=0 if entry in ('test_initial_active','test_retained') else 4 if entry=='test_main' else 20 if entry=='test_raw' else 1
        if len(resets)!=expected_resets:raise RuntimeError('Unexpected USART reset count')
        uc.reg_write(UC_ARM_REG_LR,0x0801fff1);uc.emu_start(symbols['get_assertions']|1,0x0801fff0,count=1000)
        results.append(dict(optimization=opt,scenario=entry,status='pass',assertions=uc.reg_read(UC_ARM_REG_R0),usart_only_resets=len(resets),unexpected_dr_reads=len(dr_reads),reset_held_stale_dr_clears=len(stale_dr_reads)))
sources+=[PROJECT/'Middlewares/Noodoe/Bluetooth/src/bluetooth_service.c',PROJECT/'Middlewares/Noodoe/Bluetooth/src/bluetooth_port.c',PROJECT/'Drivers/BSP/inc/BSP_BT_HCI.h',PROJECT/'Drivers/BSP/inc/BSP_BT_ResetDiagnostic.h',PROJECT/'Middlewares/Noodoe/Bluetooth/inc/NoodoeBluetooth.h',PROJECT/'Middlewares/Noodoe/Bluetooth/inc/bluetooth_port.h',PROJECT/'App_Logic/Runtime/src/NoodoeRuntime.c']
for source in [PROJECT/'Drivers/BSP/src/BSP_BT_ResetDiagnostic.c',PROJECT/'Middlewares/Noodoe/Bluetooth/src/bluetooth_service.c',PROJECT/'Middlewares/Noodoe/Bluetooth/src/bluetooth_port.c']:
    strict=[str(TC/'arm-none-eabi-gcc.exe'),'-mcpu=cortex-m4','-mthumb','-std=gnu11','-Os','-Wall','-Wextra','-Werror','-DSTM32F429xx','-DUSE_HAL_DRIVER']
    for inc in includes:strict+=['-I',str(inc)]
    subprocess.run(strict+['-c',str(source),'-o',str(OUT/(source.stem+'-strict.o'))],check=True)
runtime=(PROJECT/'App_Logic/Runtime/src/NoodoeRuntime.c').read_text()
import re
runtime_without_comments=re.sub(r'/\*.*?\*/|//[^\n]*','',runtime,flags=re.S)
assert not re.search(r'\bMX_USART1_UART_Init\s*\(',runtime_without_comments),'Runtime initialized UART before BT owner'
report={'results':results,'runtime_uart_owned_by_bt':True,'sources':{str(p.relative_to(PROJECT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}}
(OUT/'results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
