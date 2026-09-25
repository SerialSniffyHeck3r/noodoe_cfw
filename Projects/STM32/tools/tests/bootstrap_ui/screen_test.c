#include "Bootstrap_Screen.h"
#include "bsp_eve_bus.h"
#include <string.h>
volatile BSP_EVE_Diagnostics g_bsp_eve;
static uint32_t mode,sends,bytes,releases;
static uint8_t frame[2048];
BSP_EVE_Status BSP_EVE_BusRead(uint32_t address,void *out,uint32_t n)
{
 uint8_t *p=out;memset(out,0,n);
 if(mode==1)return BSP_EVE_ERROR_SPI;
 if(address==0x302574){p[0]=mode==2?0:0xfc;p[1]=mode==2?0:0x0f;}
 else p[0]=mode==3?1:0;
 return BSP_EVE_OK;
}
BSP_EVE_Status BSP_EVE_BusSelect(void){return mode==6?BSP_EVE_ERROR_SPI:BSP_EVE_OK;}
BSP_EVE_Status BSP_EVE_BusDeselect(void){releases++;return mode==7?BSP_EVE_ERROR_SPI:BSP_EVE_OK;}
BSP_EVE_Status BSP_EVE_BusSend(const void *p,uint32_t n)
{
 sends++;if((mode==5&&sends==1)||(mode==4&&sends==2))return BSP_EVE_ERROR_SPI;
 if(sends==2){if(n>sizeof(frame))return BSP_EVE_ERROR_ARGUMENT;bytes=n;memcpy(frame,p,n);}
 return BSP_EVE_OK;
}
#define CHECK(x) do{if(!(x))return __LINE__;}while(0)
int TestScreen(uint32_t scenario)
{
 mode=scenario;g_bsp_eve.ready=1;sends=bytes=releases=0;
 BootstrapUI s;BootstrapUI_Init(&s);s.state=BOOT_UI_ERROR;s.error=BOOT_ERR_STORAGE|4;
 BootstrapView v;BootstrapUI_View(&s,&v);
 uint32_t result=BootstrapScreen_Draw(&v);
 if(scenario==2||scenario==3){CHECK(result==1&&sends==0&&releases==0);return 0;}
 if(scenario){CHECK(result==2);CHECK(sends<2||releases==1);return 0;}
 CHECK(result==0&&bytes<=768&&bytes%4==0&&sends==2&&releases==1);
 uint32_t red=0,help=0,swear=0;
 for(uint32_t n=0;n+4<=bytes;n++){
  if(n+4<=bytes&&memcmp(frame+n,"\x40\x40\xff\x04",4)==0)red=1;
  if(n+19<=bytes&&memcmp(frame+n,"Help code: 00050004",19)==0)help=1;
  if(n+11<=bytes&&memcmp(frame+n,"Well, shit.",11)==0)swear=1;
 }
 CHECK(red&&help&&swear);
 /* Worst-case progress copy also stays within the fixed command buffer. */
 s.state=BOOT_UI_WORK;s.phase=13;s.kind=4;s.position=s.total=0xffffffffU;s.busy=s.stalled=1;s.now=0xffffffffU;
 BootstrapUI_View(&s,&v);sends=bytes=0;CHECK(!BootstrapScreen_Draw(&v)&&bytes<=768);
 return 0;
}
