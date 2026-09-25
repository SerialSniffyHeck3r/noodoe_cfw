/* REG_CMDB_WRITE is a bounded 4092-byte producer/consumer FIFO, not RAM.
 * DLSTART/SWAP may stall its consumer at a frame boundary. HAL chunking alone
 * does not supply flow control. Keep a conservative byte credit and re-open
 * only this FIFO address after polling REG_CMDB_SPACE; ordinary RAM/register
 * transfers retain their original address and chip-select lifetime.
 * The pinned LVGL/EVE library remains unchanged. One graphics task owns SPI. */
#include "Graphics_EveTransport.h"
#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include <string.h>
#define FIFO_SPACE 0x302574U
#define FIFO_WRITE 0x302578U
#define TIMEOUT_MS 250U
static uint8_t header[3];
static uint32_t selected,header_size,fifo,physical,credit,burst_bytes;
volatile GraphicsEveTransportDiagnostics g_eve_transport;

/* Poll only with physical CS high. A low-bit fault is never masked into an
 * apparently valid capacity. Every path has a finite timeout and releases CS. */
static BSP_EVE_Status OpenFifo(void)
{
    uint32_t began=HAL_GetTick();uint16_t space;
    if(physical){BSP_EVE_BusDeselect();physical=0;}
    for(;;){
        BSP_EVE_Status s=BSP_EVE_BusRead(FIFO_SPACE,&space,sizeof(space));
        if(s!=BSP_EVE_OK)return s;
        ++g_eve_transport.space_reads;g_eve_transport.last_space=space;
        if(space<g_eve_transport.min_space)g_eve_transport.min_space=space;
        if((space&3U)||space>4092U){++g_eve_transport.faults;return BSP_EVE_ERROR_ARGUMENT;}
        if(space>=4U)break;
        ++g_eve_transport.waits;
        if(HAL_GetTick()-began>=TIMEOUT_MS){++g_eve_transport.timeouts;return BSP_EVE_ERROR_CPU_TIMEOUT;}
        osDelay(1U);
    }
    credit=space;g_eve_transport.credit=credit;
    BSP_EVE_Status s=BSP_EVE_BusSelect();
    if(s==BSP_EVE_OK){physical=1;s=BSP_EVE_BusSend(header,3);}
    return s;
}

BSP_EVE_Status GraphicsEveTransport_Select(void)
{
    if(selected)return BSP_EVE_ERROR_CONTEXT;
    if(!g_eve_transport.magic){g_eve_transport.magic=0x45565431;g_eve_transport.version=1;g_eve_transport.min_space=4092;}
    selected=1;header_size=fifo=physical=credit=burst_bytes=0;g_eve_transport.active=1;
    return BSP_EVE_OK;
}

/* Header bytes can arrive one at a time or share a buffered send with data.
 * Credits count payload only. A split at zero credit is necessarily on a
 * four-byte boundary even when a string reaches us as individual bytes. */
BSP_EVE_Status GraphicsEveTransport_Send(const void *data,uint32_t length)
{
    if(!selected||(!data&&length))return BSP_EVE_ERROR_CONTEXT;
    const uint8_t *p=data;
    while(header_size<3U&&length){header[header_size++]=*p++;--length;
        if(header_size==3U){
            uint32_t address=((uint32_t)(header[0]&0x3f)<<16)|((uint32_t)header[1]<<8)|header[2];
            fifo=(header[0]&0x80U)&&address==FIFO_WRITE;
            BSP_EVE_Status s;
            if(fifo){++g_eve_transport.bursts;s=OpenFifo();}
            else{s=BSP_EVE_BusSelect();if(s==BSP_EVE_OK){physical=1;s=BSP_EVE_BusSend(header,3);}}
            if(s!=BSP_EVE_OK)return s;
        }
    }
    if(!length)return BSP_EVE_OK;
    if(!fifo)return BSP_EVE_BusSend(p,length);
    while(length){
        if(!credit){BSP_EVE_Status s=OpenFifo();if(s!=BSP_EVE_OK)return s;}
        uint32_t n=length<credit?length:credit;
        BSP_EVE_Status s=BSP_EVE_BusSend(p,n);if(s!=BSP_EVE_OK)return s;
        p+=n;length-=n;credit-=n;burst_bytes+=n;
        g_eve_transport.bytes+=n;g_eve_transport.credit=credit;
        if(burst_bytes>g_eve_transport.max_burst)g_eve_transport.max_burst=burst_bytes;
    }
    return BSP_EVE_OK;
}
BSP_EVE_Status GraphicsEveTransport_Receive(void *data,uint32_t length)
{if(!selected||!physical||fifo)return BSP_EVE_ERROR_CONTEXT;return BSP_EVE_BusReceive(data,length);}
BSP_EVE_Status GraphicsEveTransport_Deselect(void)
{
    BSP_EVE_Status result=selected&&header_size==3U&&(!fifo||!(burst_bytes&3U))?BSP_EVE_OK:BSP_EVE_ERROR_CONTEXT;
    if(physical)BSP_EVE_BusDeselect();
    selected=physical=0;g_eve_transport.active=0;return result;
}
