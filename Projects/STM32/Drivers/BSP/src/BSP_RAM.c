#include "BSP_RAM.h"
#include "fmc.h"
#include "dma.h"
#include "FreeRTOS.h"
#include "task.h"

volatile BSP_RAM_Diagnostics g_bsp_ram;
volatile BSP_RAM_Allocation g_bsp_ram_allocations[32];
static uint32_t allocation_count;
static uint32_t dma_source[64] __attribute__((aligned(4)));

/* A bounded command preserves the first failure for SWD diagnostics. */
static uint32_t Command(uint32_t mode,uint32_t refresh,uint32_t reg)
{
    FMC_SDRAM_CommandTypeDef cmd={0};
    cmd.CommandMode=mode;cmd.CommandTarget=FMC_SDRAM_CMD_TARGET_BANK1;
    cmd.AutoRefreshNumber=refresh;cmd.ModeRegisterDefinition=reg;
    return HAL_SDRAM_SendCommand(&hsdram1,&cmd,100U)==HAL_OK?0U:1U;
}
static uint32_t Bad(uint32_t address,uint32_t expected,uint32_t actual,uint32_t result)
{
    g_bsp_ram.first_bad_address=address;g_bsp_ram.expected=expected;
    g_bsp_ram.observed=actual;g_bsp_ram.result=result;return result;
}

/* This verifies data/address lines and64KiB patterns, not every bit of64MiB.
 * Separate physical address aliases are detected before exporting the arena;
 * the configured FMC geometry alone is never reported as measured capacity. */
uint32_t BSP_RAM_Init(void)
{
    if(g_bsp_ram.ready)return 0U;
    g_bsp_ram.magic=0x52414D31U;g_bsp_ram.geometry_bytes=BSP_RAM_GEOMETRY_BYTES;
    MX_FMC_Init();
    if(Command(FMC_SDRAM_CMD_CLK_ENABLE,1U,0U))return Bad(0,0,0,1U);
    vTaskDelay(pdMS_TO_TICKS(1U));
    if(Command(FMC_SDRAM_CMD_PALL,1U,0U) ||
       Command(FMC_SDRAM_CMD_AUTOREFRESH_MODE,8U,0U) ||
       Command(FMC_SDRAM_CMD_LOAD_MODE,1U,0x230U))return Bad(0,0,0,2U);
    /* 84MHz*(64ms/8192rows)-20=636.25, matching the recovered configuration. */
    if(HAL_SDRAM_ProgramRefreshRate(&hsdram1,636U)!=HAL_OK)return Bad(0,0,0,3U);
    volatile uint32_t *ram=(volatile uint32_t *)BSP_RAM_BASE;
    for(uint32_t bit=0;bit<32U;++bit){
        uint32_t pattern=1UL<<bit;ram[0]=pattern;__DSB();
        if(ram[0]!=pattern)return Bad(BSP_RAM_BASE,pattern,ram[0],4U);
        ram[0]=~pattern;__DSB();
        if(ram[0]!=~pattern)return Bad(BSP_RAM_BASE,~pattern,ram[0],4U);
    }
    uint32_t capacity=BSP_RAM_GEOMETRY_BYTES;
    /* An upper address mirroring base means a smaller physical device. Test
     * each power-of-two byte address independently to locate that boundary. */
    for(uint32_t offset=4U;offset<BSP_RAM_GEOMETRY_BYTES;offset<<=1U){
        volatile uint32_t *probe=(volatile uint32_t *)(BSP_RAM_BASE+offset);
        ram[0]=0x13579BDFU;*probe=0x2468ACE0U;__DSB();
        if(ram[0]==0x2468ACE0U){capacity=offset;break;}
        if(ram[0]!=0x13579BDFU || *probe!=0x2468ACE0U)
            return Bad(BSP_RAM_BASE+offset,0x2468ACE0U,*probe,5U);
    }
    if(capacity<BSP_RAM_TEST_BYTES*2U)return Bad(BSP_RAM_BASE,131072U,capacity,6U);
    g_bsp_ram.capacity_bytes=capacity;
    /* Unique values at all address-bit offsets catch aliases between probes,
     * including stuck-high lines that the base-only test would not detect. */
    for(uint32_t offset=4U;offset<capacity;offset<<=1U)
        *(volatile uint32_t *)(BSP_RAM_BASE+offset)=offset^0xA55AA55AU;
    __DSB();
    for(uint32_t offset=4U;offset<capacity;offset<<=1U){
        uint32_t value=*(volatile uint32_t *)(BSP_RAM_BASE+offset);
        if(value!=(offset^0xA55AA55AU))return Bad(BSP_RAM_BASE+offset,offset^0xA55AA55AU,value,7U);
    }
    for(uint32_t pass=0;pass<2U;++pass){
        for(uint32_t i=0;i<BSP_RAM_TEST_BYTES/4U;++i)ram[i]=(i*0x9E3779B1U)^(pass?0xFFFFFFFFU:0U);
        __DSB();
        for(uint32_t i=0;i<BSP_RAM_TEST_BYTES/4U;++i){
            uint32_t expected=(i*0x9E3779B1U)^(pass?0xFFFFFFFFU:0U);
            if(ram[i]!=expected)return Bad(BSP_RAM_BASE+i*4U,expected,ram[i],8U);
        }
        vTaskDelay(1U);
    }
    for(uint32_t i=0;i<64U;++i)dma_source[i]=0x55AA0000U+i;
    /* The generated memory-to-memory DMA stream isbyte-wide, so length is
     *256 transfers. Polling mode owns its completion and uses no IRQ callback. */
    if(HAL_DMA_Start(&hdma_memtomem_dma2_stream0,(uint32_t)dma_source,BSP_RAM_BASE,sizeof(dma_source))!=HAL_OK ||
       HAL_DMA_PollForTransfer(&hdma_memtomem_dma2_stream0,HAL_DMA_FULL_TRANSFER,100U)!=HAL_OK){
        /* HAL polling timeout can mark READY without clearing EN; stop the
         * stream explicitly before returning unready or permitting a retry. */
        __HAL_DMA_DISABLE(&hdma_memtomem_dma2_stream0);
        uint32_t start=HAL_GetTick();
        while(hdma_memtomem_dma2_stream0.Instance->CR&DMA_SxCR_EN)
            if(HAL_GetTick()-start>=100U)return Bad(BSP_RAM_BASE,0,0,11U);
        return Bad(BSP_RAM_BASE,0,0,9U);
    }
    __DSB();
    for(uint32_t i=0;i<64U;++i)if(ram[i]!=dma_source[i])return Bad(BSP_RAM_BASE+i*4U,dma_source[i],ram[i],10U);
    g_bsp_ram.dma_verified=1U;g_bsp_ram.tested_bytes=BSP_RAM_TEST_BYTES;
    g_bsp_ram.result=0U;g_bsp_ram.ready=1U;return 0U;
}

void *BSP_RAM_Allocate(size_t bytes)
{
    if(!g_bsp_ram.ready || !bytes || bytes>UINT32_MAX-31U)return NULL;
    uint32_t rounded=((uint32_t)bytes+31U)&~31U;void *result=NULL;
    taskENTER_CRITICAL();
    uint32_t free_bytes=g_bsp_ram.capacity_bytes-BSP_RAM_TEST_BYTES-g_bsp_ram.allocated_bytes;
    if(rounded<=free_bytes&&allocation_count<32){result=(void *)(BSP_RAM_BASE+BSP_RAM_TEST_BYTES+g_bsp_ram.allocated_bytes);g_bsp_ram.allocated_bytes+=rounded;
        g_bsp_ram_allocations[allocation_count++]=(BSP_RAM_Allocation){0,(uint32_t)result,rounded,(uint32_t)__builtin_return_address(0)};}
    taskEXIT_CRITICAL();return result;
}

void *BSP_RAM_AllocateNamed(uint32_t owner,size_t bytes)
{
    if(!owner||!bytes||bytes>UINT32_MAX-31U)return NULL;
    taskENTER_CRITICAL();
    uint32_t rounded=((uint32_t)bytes+31U)&~31U;
    for(uint32_t i=0;i<allocation_count;++i)if(g_bsp_ram_allocations[i].owner==owner){
        void *result=g_bsp_ram_allocations[i].bytes==rounded?(void *)g_bsp_ram_allocations[i].address:NULL;
        taskEXIT_CRITICAL();return result;
    }
    void *result=BSP_RAM_Allocate(bytes);
    if(result)g_bsp_ram_allocations[allocation_count-1].owner=owner;
    taskEXIT_CRITICAL();return result;
}
