#include <stdint.h>
#include <stddef.h>
#include "BSP_Display.h"
#include "BSP_RAM.h"
#include "BSP_Power.h"
#include "bsp_eve_bus.h"
#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include "src/draw/eve/lv_draw_eve_private.h"
#include "src/draw/eve/lv_draw_eve_ram_g.h"

volatile uint32_t g_assertions,g_failure_line,g_mock_error,g_test_crc;
volatile BSP_EVE_Diagnostics g_bsp_eve;
volatile BSP_EVE_BusDiagnostics g_bsp_eve_bus;
TestIwdg test_iwdg;
lv_global_t lv_global;
static lv_draw_eve_unit_t unit;
static uint8_t heap[4096];static uint32_t heap_used;
static uint32_t tick,force_busy,commands,send_phase,pending_swap,stuck_swap,fail_swap_read;
static uint8_t last_command[20];
volatile BSP_Power_Diagnostics g_bsp_power;
static uint32_t allocations;
static uint32_t sleeping;
uint32_t BSP_Display_IsSleeping(void){return sleeping;}
void *BSP_RAM_AllocateNamed(uint32_t owner,size_t n)
{++allocations;if(owner==BSP_RAM_CAPTURE&&n==460800)return (void*)0xC0010000;
 if(owner==BSP_RAM_CAPTURE_DL&&n==4096)return (void*)0xC0090000;
 return 0;}
static void Complete(void){for(uint32_t i=0;i<3000&&g_bsp_capture.request_seq!=g_bsp_capture.response_seq;++i)BSP_Display_CaptureProcess();}

#define CHECK(x) do{++g_assertions;if(!(x)){g_failure_line=__LINE__;return 1;}}while(0)
void *memcpy(void *dest,const void *src,size_t n){uint8_t *d=dest;const uint8_t *s=src;for(size_t i=0;i<n;++i)d[i]=s[i];return dest;}
void *memset(void *dest,int value,size_t n){uint8_t *d=dest;for(size_t i=0;i<n;++i)d[i]=(uint8_t)value;return dest;}
void *lv_calloc(size_t count,size_t size){size_t bytes=count*size;if(heap_used+bytes>sizeof(heap)){g_mock_error=1;return NULL;}void *result=heap+heap_used;heap_used+=(uint32_t)bytes;memset(result,0,bytes);return result;}
void lv_free(void *p){(void)p;}
void lv_log_add(lv_log_level_t level,const char *file,int line,const char *function,const char *format,...){(void)level;(void)file;(void)line;(void)function;(void)format;}
void Graphics_AssertFail(const char *file,uint32_t line){(void)file;g_failure_line=line;for(;;){}}
uint32_t HAL_GetTick(void){return tick++;}
osKernelState_t osKernelGetState(void){return osKernelRunning;}
uint32_t osDelay(uint32_t ticks){tick+=ticks;return 0;}
BSP_EVE_Status BSP_EVE_BusSelect(void){if(g_bsp_eve_bus.selected){g_mock_error=2;return BSP_EVE_ERROR_CONTEXT;}g_bsp_eve_bus.selected=1;send_phase=0;return BSP_EVE_OK;}
BSP_EVE_Status BSP_EVE_BusDeselect(void){g_bsp_eve_bus.selected=0;return BSP_EVE_OK;}
BSP_EVE_Status BSP_EVE_BusSend(const void *data,uint32_t length)
{
    const uint8_t *bytes=data;
    if(!g_bsp_eve_bus.selected){g_mock_error=3;return BSP_EVE_ERROR_CONTEXT;}
    if(send_phase++==0){if(length!=3 || bytes[0]!=0xB0 || bytes[1]!=0x25 || bytes[2]!=0x78)g_mock_error=4;}
    else {if(length!=20){g_mock_error=5;return BSP_EVE_ERROR_ARGUMENT;}if(pending_swap||stuck_swap)g_mock_error=8;memcpy(last_command,data,20);++commands;}
    return BSP_EVE_OK;
}
BSP_EVE_Status BSP_EVE_BusRead(uint32_t address,void *data,uint32_t length)
{
    uint8_t *bytes=data;if(g_bsp_eve_bus.selected){g_mock_error=6;return BSP_EVE_ERROR_CONTEXT;}
    if(address==0x00302574U && length==2){bytes[0]=force_busy?0:0xFC;bytes[1]=force_busy?0:0x0F;return BSP_EVE_OK;}
    if(address==0x00302054U && length==1){if(fail_swap_read)return BSP_EVE_ERROR_CONTEXT;bytes[0]=(pending_swap||stuck_swap)?2:0;if(pending_swap)--pending_swap;return BSP_EVE_OK;}
    if(address==0x00302004U && length==4){bytes[0]=42;bytes[1]=bytes[2]=bytes[3]=0;return BSP_EVE_OK;}
    if(address==0x00300000U && length==4096){for(uint32_t i=0;i<length;++i)bytes[i]=(uint8_t)(i*11U+1U);return BSP_EVE_OK;}
    if(address<BSP_DISPLAY_CAPTURE_RAM_G || address>=BSP_DISPLAY_CAPTURE_RAM_G+BSP_DISPLAY_CAPTURE_STRIPE_BYTES || length>BSP_DISPLAY_CAPTURE_RAM_G+BSP_DISPLAY_CAPTURE_STRIPE_BYTES-address){g_mock_error=7;return BSP_EVE_ERROR_ARGUMENT;}
    for(uint32_t i=0;i<length;++i)bytes[i]=(uint8_t)((((uint32_t)last_command[14]|((uint32_t)last_command[15]<<8))*960U+address-BSP_DISPLAY_CAPTURE_RAM_G+i)*7U+3U);
    return BSP_EVE_OK;
}

int Capture_TestMain(void)
{
 lv_draw_eve_unit_g=&unit;uint32_t address=0,limit=BSP_DISPLAY_CAPTURE_RAM_G;
 CHECK(!lv_draw_eve_ramg_get_addr(&address,1,limit+1,1)&&address==UINT32_MAX);
 CHECK(!lv_draw_eve_ramg_get_addr(&address,2,limit-4,4)&&address==0);
 CHECK(!lv_draw_eve_ramg_get_addr(&address,3,4,4)&&address==limit-4);
 CHECK(!lv_draw_eve_ramg_get_addr(&address,4,1,1)&&address==UINT32_MAX);
 CHECK(lv_draw_eve_ramg_get_addr(&address,2,limit-4,4)&&address==0);
 g_bsp_eve.ready=1;BSP_Display_CaptureProcess();pending_swap=4;
 CHECK(BSP_Display_CaptureRequest(1,4,0,0,0)==BSP_DISPLAY_OK);
 BSP_Display_CaptureProcess();CHECK(BSP_Display_CaptureBusy()&&g_bsp_capture.response_seq==0);
 CHECK(BSP_Display_CaptureRequest(2,1,0,0,0)==BSP_DISPLAY_ERROR_STATE);
 Complete();CHECK(g_bsp_capture.response_seq==1&&g_bsp_capture.status==0&&!BSP_Display_CaptureBusy());
 CHECK(commands==480/BSP_DISPLAY_CAPTURE_ROWS&&allocations==2&&g_bsp_capture.generation==1&&pending_swap==0);
 CHECK(last_command[14]==((480-BSP_DISPLAY_CAPTURE_ROWS)&255)&&last_command[15]==((480-BSP_DISPLAY_CAPTURE_ROWS)>>8));
 CHECK(last_command[18]==BSP_DISPLAY_CAPTURE_ROWS&&last_command[19]==0);
 CHECK(g_bsp_capture.payload_length==4096);
 for(uint32_t i=0;i<4096;++i)CHECK(g_bsp_capture.data[i]==(uint8_t)(i*11U+1U));
 CHECK(BSP_Display_CaptureRequest(2,2,1,17,4096)==BSP_DISPLAY_OK);Complete();
 CHECK(g_bsp_capture.status==0&&g_bsp_capture.payload_length==4096);g_test_crc=g_bsp_capture.payload_crc32;
 for(uint32_t i=0;i<4096;++i)CHECK(g_bsp_capture.data[i]==(uint8_t)((i+17U)*7U+3U));
 /* Chunk straddles a stripe; assembled bytes must equal one full-frame raster. */
 CHECK(BSP_Display_CaptureRequest(3,2,1,23000,4096)==BSP_DISPLAY_OK);Complete();
 for(uint32_t i=0;i<4096;++i)CHECK(g_bsp_capture.data[i]==(uint8_t)((i+23000U)*7U+3U));
 CHECK(commands==480/BSP_DISPLAY_CAPTURE_ROWS&&!BSP_Display_CaptureBusy());
 CHECK(BSP_Display_CaptureRequest(4,2,9,0,1)==BSP_DISPLAY_OK);Complete();CHECK(g_bsp_capture.status==BSP_DISPLAY_ERROR_STATE);
 CHECK(BSP_Display_CaptureRequest(5,1,0,0,0)==BSP_DISPLAY_OK);Complete();CHECK(!g_bsp_capture.status&&allocations==2);
 uint32_t generation=g_bsp_capture.generation;
 CHECK(BSP_Display_CaptureRequest(6,2,generation,460799,2)==BSP_DISPLAY_OK);Complete();CHECK(g_bsp_capture.status==BSP_DISPLAY_ERROR_ARGUMENT);
 force_busy=1;CHECK(BSP_Display_CaptureRequest(7,1,0,0,0)==BSP_DISPLAY_OK);Complete();
 CHECK(g_bsp_capture.status==BSP_DISPLAY_ERROR_STATE&&!BSP_Display_CaptureBusy()&&g_bsp_capture.duration_ms>=2000);
 force_busy=0;CHECK(BSP_Display_CaptureRequest(8,1,0,0,0)==BSP_DISPLAY_OK);BSP_Display_CaptureProcess();
 CHECK(BSP_Display_CaptureRequest(9,3,0,0,0)==BSP_DISPLAY_OK);Complete();CHECK(!BSP_Display_CaptureBusy());
 CHECK(BSP_Display_CaptureRequest(10,1,0,0,0)==BSP_DISPLAY_OK);BSP_Display_CaptureProcess();
 g_bsp_power.raw_ign_off=1;Complete();CHECK(g_bsp_capture.status==BSP_DISPLAY_ERROR_STATE&&!BSP_Display_CaptureBusy());
 g_bsp_power.raw_ign_off=0;fail_swap_read=1;
 CHECK(BSP_Display_CaptureRequest(11,1,0,0,0)==BSP_DISPLAY_OK);Complete();CHECK(g_bsp_capture.status==BSP_DISPLAY_ERROR_EVE);
 fail_swap_read=0;
 CHECK(BSP_Display_CaptureRequest(12,1,0,0,0)==BSP_DISPLAY_OK);Complete();CHECK(!g_bsp_capture.status);
 generation=g_bsp_capture.generation;uint32_t previous_commands=commands;sleeping=1;
 CHECK(BSP_Display_CaptureRequest(13,2,generation,0,4096)==BSP_DISPLAY_OK);Complete();CHECK(!g_bsp_capture.status);
 CHECK(commands==previous_commands); /* SDRAM download never wakes the GPU. */
 CHECK(BSP_Display_CaptureRequest(14,1,0,0,0)==BSP_DISPLAY_OK);Complete();
 CHECK(g_bsp_capture.status==BSP_DISPLAY_ERROR_STATE&&!BSP_Display_CaptureBusy()&&commands==previous_commands);
 sleeping=0;CHECK(BSP_Display_CaptureRequest(15,1,0,0,0)==BSP_DISPLAY_OK);BSP_Display_CaptureProcess();
 sleeping=1;Complete();CHECK(g_bsp_capture.status==BSP_DISPLAY_ERROR_STATE&&!BSP_Display_CaptureBusy());
 CHECK(!g_mock_error&&allocations==2);return 0;
}
