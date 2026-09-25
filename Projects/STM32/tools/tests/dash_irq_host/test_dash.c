/* Actual production BSP with deterministic ISR interleavings at the HAL call
 * which reenables RX. Hardware registers exist only in Unicorn mapped memory. */
#include <stddef.h>
#include <string.h>
#include "BSP_Dash.c"
UART_HandleTypeDef huart5;
volatile uint32_t g_assertions,g_failure_line,g_mock_error;
static uint32_t receives,abort_rx,abort_tx,inject,rx_stopped,nested_busy;
static const uint8_t *dma_data;
static uint32_t dma_size;
static DMA_HandleTypeDef dma_tx;
static DMA_Stream_TypeDef dma_registers;
static uint32_t abort_tx_mode,uart_inits;
static HAL_StatusTypeDef init_status;
#define CHECK(x) do{++g_assertions;if(!(x)){g_failure_line=__LINE__;return 1;}}while(0)
void *memcpy(void *dest,const void *src,size_t n){uint8_t *d=dest;const uint8_t *s=src;for(size_t i=0;i<n;++i)d[i]=s[i];return dest;}
void *memset(void *dest,int v,size_t n){uint8_t *d=dest;for(size_t i=0;i<n;++i)d[i]=(uint8_t)v;return dest;}
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *uart){++uart_inits;uart->hdmatx=&dma_tx;return init_status;}
void HAL_GPIO_Init(GPIO_TypeDef *port,GPIO_InitTypeDef *pin){(void)port;(void)pin;}
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *uart){(void)uart;++abort_rx;rx_stopped=1U;return HAL_OK;}
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *uart)
{
    ++abort_tx;
    /* Actual HAL first clears DMAT, then attempts to disable the DMA stream.
     * A subsequent call can return HAL_OK just because DMAT is now clear. */
    if(!(uart->Instance->CR3&USART_CR3_DMAT))return HAL_OK;
    CLEAR_BIT(uart->Instance->CR3,USART_CR3_DMAT);
    if(abort_tx_mode==1U)return HAL_TIMEOUT; /* EN remains set. */
    if(abort_tx_mode==2U)return HAL_OK;      /* False OK with EN still set. */
    CLEAR_BIT(dma_registers.CR,DMA_SxCR_EN);
    if(abort_tx_mode==3U)return HAL_TIMEOUT; /* Error still must be retained. */
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *uart,const uint8_t *data,uint16_t length)
{dma_data=data;dma_size=length;SET_BIT(dma_registers.CR,DMA_SxCR_EN);SET_BIT(uart->Instance->CR3,USART_CR3_DMAT);return HAL_OK;}
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *uart,uint8_t *data,uint16_t length)
{
    (void)data;(void)length;++receives;
    /* A sticky error immediately retriggers at rearm, even with RXNE clear.
     * Unicorn's SR/DR hook models only the documented hardware clear order. */
    if(uart->Instance->SR&USART_SR_ORE){uart->ErrorCode=HAL_UART_ERROR_ORE;rx_stopped=1U;BSP_Dash_OnError(uart);return HAL_OK;}
    if(nested_busy)return HAL_BUSY;
    rx_stopped=0U;uint32_t event=inject;inject=0U;
    if(event==1U || event==3U){
        /* IRQ occurs after RXNEIE is enabled but before HAL returns to task. */
        uart->ErrorCode=event==1U?HAL_UART_ERROR_ORE:HAL_UART_ERROR_DMA;
        rx_stopped=1U;BSP_Dash_OnError(uart);
    }else if(event==2U){
        nested_busy=1U;BSP_Dash_OnRxComplete(uart);nested_busy=0U;
    }else if(event==4U){rx_stopped=1U;return HAL_BUSY;}
    return HAL_OK;
}
static uint32_t Fresh(void)
{
    memset((void*)&g_bsp_dash,0,sizeof(g_bsp_dash));memset(&huart5,0,sizeof(huart5));
    /* A fresh fixture represents C startup after MCU reset, not a forbidden
     * runtime Init shortcut that clears a DMA fault latch on a live device. */
    tx_fault_locked=0U;memset(&dma_tx,0,sizeof(dma_tx));memset(&dma_registers,0,sizeof(dma_registers));
    dma_tx.Instance=&dma_registers;abort_tx_mode=uart_inits=0U;init_status=HAL_OK;UART5->CR3=0U;
    receives=abort_rx=abort_tx=inject=rx_stopped=nested_busy=0U;dma_data=NULL;dma_size=0U;
    return BSP_Dash_Init();
}

/* Simulate UART TC after normal DMA completion, including the DMA EN clear
 * which precedes actual UART TC. The public callback itself stays unchanged. */
static void CompleteTx(void)
{CLEAR_BIT(dma_registers.CR,DMA_SxCR_EN);CLEAR_BIT(UART5->CR3,USART_CR3_DMAT);BSP_Dash_OnTxComplete(&huart5);}
int DashIRQ_TestMain(void)
{
    CHECK(Fresh()==0U);CHECK(huart5.Init.Mode==UART_MODE_RX && g_bsp_dash.ready);
    UART5->SR=USART_SR_ORE;huart5.ErrorCode=HAL_UART_ERROR_ORE;BSP_Dash_OnError(&huart5);
    BSP_Dash_Process();CHECK(!(UART5->SR&USART_SR_ORE)&&!recover_rx&&!rx_stopped);
    CHECK(Fresh()==0U);
    /* Main bug: error IRQ sets a fresh recovery request within successful
     * rearm. The following task return must not overwrite it with zero. */
    huart5.ErrorCode=HAL_UART_ERROR_ORE;BSP_Dash_OnError(&huart5);
    inject=1U;BSP_Dash_Process();CHECK(rx_stopped && recover_rx && abort_rx==1U);
    BSP_Dash_Process();CHECK(!rx_stopped && !recover_rx && abort_rx==2U);
    uint32_t count=receives;BSP_Dash_Process();CHECK(receives==count);
    /* The RX-complete callback's failed nested rearm is another producer of
     * the same flag and must survive the enclosing HAL_OK return as well. */
    BSP_Dash_OnError(&huart5);inject=2U;BSP_Dash_Process();CHECK(recover_rx);
    BSP_Dash_Process();CHECK(!recover_rx);
    BSP_Dash_OnError(&huart5);inject=4U;BSP_Dash_Process();CHECK(recover_rx);
    BSP_Dash_Process();CHECK(!recover_rx && !rx_stopped);
    /* DMA error remains pending even after an unrelated RX framing error
     * changes diagnostic last_hal_error before the worker gets CPU time. */
    CHECK(BSP_Dash_EnableTx(1U)==0U);
    uint8_t bytes[3]={0xF5U,0x21U,0x01U};CHECK(BSP_Dash_Send(bytes,3U)==0U);
    bytes[0]=0U;CHECK(dma_size==3U && dma_data[0]==0xF5U && g_bsp_dash.tx_busy);
    huart5.ErrorCode=HAL_UART_ERROR_DMA;BSP_Dash_OnError(&huart5);
    huart5.ErrorCode=HAL_UART_ERROR_FE;BSP_Dash_OnError(&huart5);
    CHECK(g_bsp_dash.last_hal_error==HAL_UART_ERROR_FE && recover_tx);
    BSP_Dash_Process();CHECK(abort_tx==1U && !g_bsp_dash.tx_busy && !recover_tx);
    /* A new TX DMA error during rearm must be handled by the next pass. */
    CHECK(BSP_Dash_Send(bytes,3U)==0U);
    huart5.ErrorCode=HAL_UART_ERROR_FE;BSP_Dash_OnError(&huart5);
    inject=3U;BSP_Dash_Process();CHECK(recover_tx && g_bsp_dash.tx_busy && abort_tx==1U);
    BSP_Dash_Process();CHECK(!recover_tx && !g_bsp_dash.tx_busy && abort_tx==2U);
    /* A DMA notification followed by actual TC consumes the old latch; it
     * cannot later abort a healthy new frame when only RX has a new error. */
    CHECK(BSP_Dash_Send(bytes,3U)==0U);
    huart5.ErrorCode=HAL_UART_ERROR_DMA;BSP_Dash_OnError(&huart5);CompleteTx();
    BSP_Dash_Process();CHECK(abort_tx==2U && !recover_tx);
    CHECK(BSP_Dash_Send(bytes,3U)==0U);
    huart5.ErrorCode=HAL_UART_ERROR_NE;BSP_Dash_OnError(&huart5);BSP_Dash_Process();
    CHECK(abort_tx==2U && g_bsp_dash.tx_busy);
    CompleteTx();CHECK(BSP_Dash_EnableTx(0U)==0U);

    /* Aborted DMA's retained private copy cannot be reused under any of the
     * uncertain-stop outcomes, even if a late TC or another error follows. */
    for(uint32_t mode=1U;mode<=3U;++mode){
        CHECK(Fresh()==0U && BSP_Dash_EnableTx(1U)==0U);
        uint8_t original[3]={0xF5U,0x31U,0x42U},replacement[3]={0xAAU,0xBBU,0xCCU},received_byte;
        CHECK(BSP_Dash_Send(original,sizeof(original))==0U);
        abort_tx_mode=mode;huart5.ErrorCode=HAL_UART_ERROR_DMA;BSP_Dash_OnError(&huart5);
        BSP_Dash_Process();CHECK(abort_tx==1U && tx_fault_locked && g_bsp_dash.tx_busy);
        CHECK(!g_bsp_dash.tx_enabled && !(UART5->CR3&USART_CR3_DMAT));
        CHECK((g_bsp_dash.last_hal_error&HAL_UART_ERROR_DMA)!=0U);
        CHECK((dma_registers.CR&DMA_SxCR_EN)==(mode==3U?0U:DMA_SxCR_EN));
        CHECK(BSP_Dash_Send(replacement,sizeof(replacement))==2U);
        CHECK(dma_data[0]==0xF5U && dma_data[1]==0x31U && dma_data[2]==0x42U);
        BSP_Dash_OnTxComplete(&huart5);CHECK(tx_fault_locked && g_bsp_dash.tx_busy);
        CHECK(BSP_Dash_EnableTx(1U)==1U && BSP_Dash_EnableTx(0U)==1U);
        uint32_t before_init=uart_inits;
        CHECK(BSP_Dash_Init()==3U && uart_inits==before_init && g_bsp_dash.tx_busy);
        huart5.ErrorCode=HAL_UART_ERROR_DMA;BSP_Dash_OnError(&huart5);BSP_Dash_Process();
        CHECK(abort_tx==1U && tx_fault_locked && g_bsp_dash.tx_busy && !recover_rx);
        CHECK(BSP_Dash_Send(replacement,sizeof(replacement))==2U && dma_data[0]==0xF5U);
        /* RX remains usable instead of turning a TX fault into a dead parser. */
        rx_byte=0x67U;BSP_Dash_OnRxComplete(&huart5);
        CHECK(BSP_Dash_Read(&received_byte,1U)==1U && received_byte==0x67U);
        /* A later spontaneous EN clear does not silently reauthorize TX. */
        CLEAR_BIT(dma_registers.CR,DMA_SxCR_EN);CompleteTx();BSP_Dash_Process();
        CHECK(tx_fault_locked && g_bsp_dash.tx_busy && BSP_Dash_Send(replacement,3U)==2U);
    }
    /* Already cleared DMAT plus active EN reproduces the misleading HAL_OK
     * branch. EN verification must independently fail closed. */
    CHECK(Fresh()==0U && BSP_Dash_EnableTx(1U)==0U);
    CHECK(BSP_Dash_Send(bytes,3U)==0U);CLEAR_BIT(UART5->CR3,USART_CR3_DMAT);
    huart5.ErrorCode=HAL_UART_ERROR_DMA;BSP_Dash_OnError(&huart5);BSP_Dash_Process();
    CHECK(tx_fault_locked && g_bsp_dash.tx_busy && (dma_registers.CR&DMA_SxCR_EN));
    /* A previously ready device cannot remain ready after a new accepted Init
     * fails. Reinitialization always revokes the previous explicit TX permit. */
    CHECK(Fresh()==0U && BSP_Dash_EnableTx(1U)==0U);
    init_status=HAL_ERROR;CHECK(BSP_Dash_Init()==1U && !g_bsp_dash.ready && !g_bsp_dash.tx_enabled);
    CHECK(BSP_Dash_Send(bytes,3U)==2U);
    init_status=HAL_OK;inject=4U;CHECK(BSP_Dash_Init()==2U && !g_bsp_dash.ready && !g_bsp_dash.tx_enabled);
    CHECK(BSP_Dash_Init()==0U && g_bsp_dash.ready && !g_bsp_dash.tx_enabled);
    CHECK(Fresh()==0U && BSP_Dash_EnableTx(1U)==0U);
    CHECK(BSP_Dash_Send(bytes,3U)==0U && !g_bsp_dash.tx_completed);
    CompleteTx();CHECK(g_bsp_dash.tx_completed==1U && !g_bsp_dash.tx_failed);
    CompleteTx();CHECK(g_bsp_dash.tx_completed==1U);
    CHECK(BSP_Dash_Send(bytes,3U)==0U);
    BSP_Dash_RequestTxAbort();BSP_Dash_Process();
    CHECK(!g_bsp_dash.tx_busy && g_bsp_dash.tx_failed==1U && g_bsp_dash.tx_last_result==2U);
    CompleteTx();CHECK(g_bsp_dash.tx_completed==1U);
    CHECK(BSP_Dash_Send(bytes,3U)==0U);abort_tx_mode=1U;
    BSP_Dash_RequestTxAbort();BSP_Dash_Process();
    CHECK(g_bsp_dash.tx_busy && g_bsp_dash.tx_failed==2U && g_bsp_dash.tx_last_result==3U);
    BSP_Dash_RequestTxAbort();BSP_Dash_Process();CompleteTx();
    CHECK(g_bsp_dash.tx_failed==2U && g_bsp_dash.tx_completed==1U);
    CHECK(g_mock_error==0U);return 0;
}
