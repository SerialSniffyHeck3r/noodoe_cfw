#include "Graphics_EveTransport.h"
#include <string.h>
volatile uint32_t g_assertions,g_failure_line;
static uint32_t tick,cs,head,addr,payload,occupancy,reads,mode,errors,ram_bytes;
static uint8_t data[8192];
#define CHECK(x) do{++g_assertions;if(!(x)){g_failure_line=__LINE__;return 1;}}while(0)
uint32_t HAL_GetTick(void){return tick;}
uint32_t osDelay(uint32_t n){tick+=n;if(mode==0)occupancy=0;return 0;}
BSP_EVE_Status BSP_EVE_BusSelect(void){if(cs)++errors;cs=1;head=addr=0;return BSP_EVE_OK;}
BSP_EVE_Status BSP_EVE_BusDeselect(void){cs=0;return BSP_EVE_OK;}
BSP_EVE_Status BSP_EVE_BusSend(const void *p,uint32_t n)
{
 const uint8_t *b=p;if(!cs)++errors;
 while(n&&head<3){addr=(addr<<8)|*b++;--n;++head;}
 if(addr==0xb02578){
  if(n>4092-occupancy)++errors;
  for(uint32_t i=0;i<n;++i)if(b[i]!=(uint8_t)(payload+i))++errors;
  occupancy+=n;payload+=n;
 }else ram_bytes+=n;
 return BSP_EVE_OK;
}
BSP_EVE_Status BSP_EVE_BusReceive(void *p,uint32_t n){memset(p,0x5a,n);return cs?BSP_EVE_OK:BSP_EVE_ERROR_CONTEXT;}
BSP_EVE_Status BSP_EVE_BusRead(uint32_t a,void *p,uint32_t n)
{if(cs||a!=0x302574||n!=2)++errors;++reads;*(uint16_t*)p=mode==2?0xfff:(uint16_t)(4092-occupancy);return BSP_EVE_OK;}
int TestMain(void)
{
 for(uint32_t i=0;i<sizeof(data);++i)data[i]=(uint8_t)i;
 uint8_t h[3]={0xb0,0x25,0x78};
 /* Consumer deliberately stops until the producer yields. Old raw SPI send
  * overruns at byte4092. Verify exact order across 1-byte and large callbacks. */
 CHECK(!GraphicsEveTransport_Select());
 for(uint32_t i=0;i<3;++i)CHECK(!GraphicsEveTransport_Send(h+i,1));
 CHECK(!GraphicsEveTransport_Send(data,3));CHECK(!GraphicsEveTransport_Send(data+3,8189));
 CHECK(!GraphicsEveTransport_Deselect());CHECK(!cs&&!errors&&payload==8192&&tick==2&&reads==5);
 CHECK(g_eve_transport.waits==2&&g_eve_transport.max_burst==8192);
 /* Register reads preserve the original address, dummy byte and CS. */
 uint8_t read[4]={0x30,0x20,0,0},v[4];uint32_t prior=reads;
 CHECK(!GraphicsEveTransport_Select());CHECK(!GraphicsEveTransport_Send(read,4));
 CHECK(!GraphicsEveTransport_Receive(v,4));CHECK(!GraphicsEveTransport_Deselect());
 CHECK(v[0]==0x5a&&reads==prior&&ram_bytes==1);
 /* A never-consuming FIFO times out and a co-processor fault is rejected. */
 mode=1;occupancy=4092;CHECK(!GraphicsEveTransport_Select());
 CHECK(GraphicsEveTransport_Send(h,3)==BSP_EVE_ERROR_CPU_TIMEOUT);GraphicsEveTransport_Deselect();CHECK(!cs);
 mode=2;CHECK(!GraphicsEveTransport_Select());CHECK(GraphicsEveTransport_Send(h,3)==BSP_EVE_ERROR_ARGUMENT);
 GraphicsEveTransport_Deselect();CHECK(!cs&&g_eve_transport.faults==1&&g_eve_transport.timeouts==1&&!errors);
 return 0;
}
