#include "Noodoe_Crc32.h"
#include "BSP_Display.h"
#include "bsp_eve.h"
#include "bsp_eve_bus.h"
#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include "BSP_RAM.h"
#include "BSP_Power.h"
#include <string.h>

/* FT81x Programmer Guide v1.2 §5.64: SNAPSHOT2 captures the actual current
 * display list output into RAM_G. RGB565=7,20byte command, little-endian fields.
 * Hardware suspends output briefly; this does not measure panel glass/backlight.
 * https://brtchip.com/wp-content/uploads/Support/Documentation/Programming_Guides/ICs/EVE/FT81X_Series_Programmer_Guide.pdf */
#define REG_CMDB_SPACE 0x00302574U
#define REG_CMDB_WRITE 0x00302578U
#define REG_FRAMES 0x00302004U
#define REG_DLSWAP 0x00302054U
#define SNAPSHOT_TIMEOUT_MS 2000U
volatile BSP_Display_CaptureMailbox g_bsp_capture;
static uint32_t snapshot_valid,phase,active_seq,active_kind,started,row,copy_offset,ign;
static uint8_t *pixels,*display_list;
uint32_t BSP_Display_CaptureBusy(void){return phase!=0;}
static void Finish(BSP_Display_Status result);
_Static_assert(sizeof(BSP_Display_CaptureMailbox)==80U+BSP_DISPLAY_CAPTURE_CHUNK,"SWD capture mailbox ABI");
_Static_assert(BSP_DISPLAY_CAPTURE_RAM_G+BSP_DISPLAY_CAPTURE_STRIPE_BYTES<=0x00100000U,"Snapshot exceeds FT81x RAM_G");
void BSP_Display_CaptureInvalidate(void)
{snapshot_valid=0U;if(phase)Finish(BSP_DISPLAY_ERROR_STATE);++g_bsp_capture.generation;__DMB();}

static uint32_t ContextOkay(void){return !__get_IPSR() && !__get_PRIMASK() && !__get_BASEPRI();}
static void Put32(uint8_t *p,uint32_t value){for(uint32_t i=0;i<4U;++i)p[i]=(uint8_t)(value>>(i*8U));}
static uint32_t Get32(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
/* Direct CMDB writes are legal only with an idle renderer/bus. No vendor
 * display-list cache is changed, and no new DISPLAY/SWAP is submitted. */
static BSP_EVE_Status WriteCommand(const uint8_t *data,uint32_t length)
{
    const uint8_t prefix[3]={(uint8_t)((REG_CMDB_WRITE>>16)|0x80U),(uint8_t)(REG_CMDB_WRITE>>8),(uint8_t)REG_CMDB_WRITE};
    BSP_EVE_Status status=BSP_EVE_BusSelect();
    if(status!=BSP_EVE_OK)return status;
    status=BSP_EVE_BusSend(prefix,3U);
    if(status==BSP_EVE_OK)status=BSP_EVE_BusSend(data,length);
    BSP_EVE_Status close=BSP_EVE_BusDeselect();return status==BSP_EVE_OK?close:status;
}
/* A poll never waits for hardware.0=idle,1=pending,2=bus/fifo error. */
static uint32_t PollIdle(void)
{
    uint8_t bytes[2];
    if(BSP_EVE_BusRead(REG_CMDB_SPACE,bytes,2)!=BSP_EVE_OK)return 2;
    uint32_t space=bytes[0]|((uint32_t)bytes[1]<<8);if(space&3)return 2;
    if(space!=0xFFC)return 1;
    if(BSP_EVE_BusRead(REG_DLSWAP,bytes,1)!=BSP_EVE_OK)return 2;
    return bytes[0]?1:0;
}
static uint32_t Crc32(const volatile uint8_t *data,uint32_t length)
{return Noodoe_Crc32(data,length);}
BSP_Display_Status BSP_Display_CaptureRequest(uint32_t sequence,uint32_t kind,uint32_t generation,uint32_t offset,uint32_t length)
{
    if(!ContextOkay())return BSP_DISPLAY_ERROR_CONTEXT;
    if(!sequence || kind<1U || kind>4U)return BSP_DISPLAY_ERROR_ARGUMENT;
    uint32_t irq=__get_PRIMASK();__disable_irq();
    if((kind!=3U&&g_bsp_capture.request_seq!=g_bsp_capture.response_seq) || sequence==g_bsp_capture.response_seq){__set_PRIMASK(irq);return BSP_DISPLAY_ERROR_STATE;}
    g_bsp_capture.command=kind;g_bsp_capture.expected_generation=generation;
    g_bsp_capture.offset=offset;g_bsp_capture.length=length;__DMB();g_bsp_capture.request_seq=sequence;
    __set_PRIMASK(irq);return BSP_DISPLAY_OK;
}
/* Publish only complete snapshots. Pixel download afterward has no renderer
 * lock and reads SDRAM, never a texture a subsequent frame can overwrite. */
static void Finish(BSP_Display_Status result)
{
    if(result!=BSP_DISPLAY_OK){if(phase||active_kind==1||active_kind==4)snapshot_valid=0;++g_bsp_capture.failures;}
    g_bsp_capture.status=result;g_bsp_capture.duration_ms=HAL_GetTick()-started;
    phase=0;__DMB();g_bsp_capture.response_seq=active_seq;
}
void BSP_Display_CaptureProcess(void)
{
    if(!g_bsp_capture.magic){g_bsp_capture.magic=0x43415031;g_bsp_capture.version=1;g_bsp_capture.width=480;g_bsp_capture.height=480;g_bsp_capture.format=7;g_bsp_capture.total_bytes=BSP_DISPLAY_CAPTURE_BYTES;}
    if(!ContextOkay()||g_bsp_eve_bus.selected)return;
    uint32_t sequence=g_bsp_capture.request_seq;
    if(phase){
        if(BSP_Display_IsSleeping()||(sequence!=active_seq&&g_bsp_capture.command==3U)||HAL_GetTick()-started>=SNAPSHOT_TIMEOUT_MS
#if NOODOE_INTEGRATED
            ||ign!=g_bsp_power.raw_ign_off
#endif
        ) {Finish(BSP_DISPLAY_ERROR_STATE);return;}
    }else{
        if(!sequence||sequence==g_bsp_capture.response_seq)return;
        __DMB();active_seq=sequence;active_kind=g_bsp_capture.command;started=HAL_GetTick();
        g_bsp_capture.payload_length=g_bsp_capture.payload_crc32=0;++g_bsp_capture.requests;
        if(active_kind==3){snapshot_valid=0;Finish(BSP_DISPLAY_OK);return;}
        if(active_kind==2){
            uint32_t off=g_bsp_capture.offset,n=g_bsp_capture.length;
            if(!snapshot_valid||g_bsp_capture.expected_generation!=g_bsp_capture.generation){Finish(BSP_DISPLAY_ERROR_STATE);return;}
            if(!n||n>BSP_DISPLAY_CAPTURE_CHUNK||off>=BSP_DISPLAY_CAPTURE_BYTES||n>BSP_DISPLAY_CAPTURE_BYTES-off){Finish(BSP_DISPLAY_ERROR_ARGUMENT);return;}
            memcpy((void *)g_bsp_capture.data,pixels+off,n);g_bsp_capture.payload_length=n;
            g_bsp_capture.payload_crc32=Crc32(g_bsp_capture.data,n);Finish(BSP_DISPLAY_OK);return;
        }
        if(active_kind!=1&&active_kind!=4){Finish(BSP_DISPLAY_ERROR_ARGUMENT);return;}
        /* Retained SDRAM downloads/cancel above are safe while the EVE sleeps.
         * A new snapshot must never touch its gated registers or wake glass. */
        if(BSP_Display_IsSleeping()){Finish(BSP_DISPLAY_ERROR_STATE);return;}
        snapshot_valid=0;
        if(!pixels)pixels=BSP_RAM_AllocateNamed(BSP_RAM_CAPTURE,BSP_DISPLAY_CAPTURE_BYTES);
        if(!display_list)display_list=BSP_RAM_AllocateNamed(BSP_RAM_CAPTURE_DL,4096);
        if(!pixels||!display_list||!g_bsp_eve.ready){Finish(BSP_DISPLAY_ERROR_STATE);return;}
#if NOODOE_INTEGRATED
        ign=g_bsp_power.raw_ign_off;
#endif
        phase=1;row=0;copy_offset=0;return;
    }
    if(phase==1||phase==3){
        uint32_t idle=PollIdle();if(idle==2){Finish(BSP_DISPLAY_ERROR_EVE);return;}if(idle)return;
        if(phase==1){
            uint8_t frame[4];
            if(BSP_EVE_BusRead(0x300000,display_list,4096)!=BSP_EVE_OK||BSP_EVE_BusRead(REG_FRAMES,frame,4)!=BSP_EVE_OK){Finish(BSP_DISPLAY_ERROR_EVE);return;}
            g_bsp_capture.eve_frames=Get32(frame);phase=2;
        }else {phase=4;copy_offset=0;}
        return;
    }
    if(phase==2){
        uint8_t command[20];Put32(command,0xFFFFFF37);Put32(command+4,7);Put32(command+8,BSP_DISPLAY_CAPTURE_RAM_G);
        Put32(command+12,row<<16);Put32(command+16,480U|(BSP_DISPLAY_CAPTURE_ROWS<<16));
        if(WriteCommand(command,20)!=BSP_EVE_OK){Finish(BSP_DISPLAY_ERROR_EVE);return;}
        phase=3;return;
    }
    if(phase==4){
        uint32_t n=BSP_DISPLAY_CAPTURE_STRIPE_BYTES-copy_offset;if(n>4096)n=4096;
        if(BSP_EVE_BusRead(BSP_DISPLAY_CAPTURE_RAM_G+copy_offset,pixels+row*960+copy_offset,n)!=BSP_EVE_OK){Finish(BSP_DISPLAY_ERROR_EVE);return;}
        copy_offset+=n;if(copy_offset!=BSP_DISPLAY_CAPTURE_STRIPE_BYTES)return;
        row+=BSP_DISPLAY_CAPTURE_ROWS;if(row<480){phase=2;return;}
        snapshot_valid=1;++g_bsp_capture.generation;if(!g_bsp_capture.generation)++g_bsp_capture.generation;
        if(active_kind==4){memcpy((void*)g_bsp_capture.data,display_list,4096);g_bsp_capture.payload_length=4096;g_bsp_capture.payload_crc32=Crc32(g_bsp_capture.data,4096);}
        Finish(BSP_DISPLAY_OK);
    }
}
