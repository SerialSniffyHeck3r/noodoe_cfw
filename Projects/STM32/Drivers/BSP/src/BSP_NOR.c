#include "BSP_NOR.h"
#include "Recovery_Target.h"
#include "spi.h"
#include "cmsis_os2.h"

/* Verified stock transport: PF6 CS, SPI5 mode0 42MHz; command bytes are 8-bit,
 * bulk is 16-bit RX MINC/TX fixed dummy. Stock READ13 and Macronix MX66L1G45G
 * dedicated 4-byte opcode avoid changing the chip-wide addressing mode. */
#define NOR_TIMEOUT_MS 100U
volatile BSP_NOR_Diagnostics g_bsp_nor;
static uint16_t bounce[BSP_NOR_READ_CHUNK_BYTES/2U] __attribute__((aligned(4)));
static uint16_t dummy=0xFFFFU;
static volatile uint32_t dma_done, dma_failed;
static volatile uint32_t storage_unlocked,ota_transaction;
static volatile uint32_t runtime_write_permit,format_active;

/* Record a stable public result. Callers must also inspect ready before use. */
static BSP_NOR_Status Result(BSP_NOR_Status status)
{
    g_bsp_nor.result=(uint32_t)status;
    g_bsp_nor.hal_error=hspi5.ErrorCode;
    return status;
}
/* Only task context with a live HAL tick can use bounded bus waits. */
static uint32_t ContextOkay(void)
{
    return __get_IPSR()==0U && __get_PRIMASK()==0U && __get_BASEPRI()==0U;
}
/* The tiny critical section protects bus ownership, never waits for hardware. */
static uint32_t Acquire(void)
{
    uint32_t irq=__get_PRIMASK();__disable_irq();
    if(g_bsp_nor.busy) {__set_PRIMASK(irq);return 0U;}
    g_bsp_nor.busy=1U;__set_PRIMASK(irq);return 1U;
}
/* All exits deselect NOR; bytes after CS rises cannot extend a prior command. */
static void Select(uint32_t active)
{
    HAL_GPIO_WritePin(GPIOF,GPIO_PIN_6,active?GPIO_PIN_RESET:GPIO_PIN_SET);
}
/* DFF may only change with SPI disabled and no unfinished transfer. HAL's
 * DataSize must match CR1 because its DMA size argument is measured in elements. */
static BSP_NOR_Status SetBits(uint32_t bits)
{
    uint32_t start=HAL_GetTick();
    while(__HAL_SPI_GET_FLAG(&hspi5,SPI_FLAG_BSY)) {
        if(HAL_GetTick()-start>NOR_TIMEOUT_MS)return Result(BSP_NOR_TIMEOUT);
    }
    __HAL_SPI_DISABLE(&hspi5);
    MODIFY_REG(hspi5.Instance->CR1,SPI_CR1_DFF,bits==16U?SPI_CR1_DFF:0U);
    hspi5.Init.DataSize=bits==16U?SPI_DATASIZE_16BIT:SPI_DATASIZE_8BIT;
    __HAL_SPI_CLEAR_OVRFLAG(&hspi5);
    return BSP_NOR_OK;
}
/* Byte transaction always drains RX, including command/address bytes. */
static BSP_NOR_Status Bytes(const uint8_t *tx,uint8_t *rx,uint16_t length)
{
    HAL_StatusTypeDef status=HAL_SPI_TransmitReceive(&hspi5,tx,rx,length,NOR_TIMEOUT_MS);
    g_bsp_nor.hal_status=(uint32_t)status;
    return status==HAL_OK?BSP_NOR_OK:Result(status==HAL_TIMEOUT?BSP_NOR_TIMEOUT:BSP_NOR_IO);
}
/* READ13 always carries four big-endian address bytes, including below16MiB.
 * This avoids crossing the 24-bit boundary under a 03 transaction. */
static BSP_NOR_Status ReadHeader(uint32_t address)
{
    uint8_t tx[5]={0x13U,(uint8_t)(address>>24),(uint8_t)(address>>16),(uint8_t)(address>>8),(uint8_t)address};
    uint8_t rx[5];
    BSP_NOR_Status status=SetBits(8U);
    if(status!=BSP_NOR_OK)return status;
    Select(1U);return Bytes(tx,rx,5U);
}
/* Failure cleanup is task-only. HAL abort is bounded and does not send a NOR
 * opcode. Keeping a half-finished DMA alive would overwrite the next response. */
static void EndTransfer(uint32_t failed)
{
    if(failed) (void)HAL_SPI_Abort(&hspi5);
    Select(0U);
    (void)SetBits(8U);
    g_bsp_nor.busy=0U;
}

BSP_NOR_Status BSP_NOR_Init(void)
{
    storage_unlocked=ota_transaction=runtime_write_permit=format_active=0U;
    BSP_NOR_ProvisionLock();
    BSP_NOR_ClearContainers();
    if(!ContextOkay())return Result(BSP_NOR_CONTEXT);
    if(hspi5.Instance!=SPI5 || hspi5.hdmarx==NULL || hspi5.hdmatx==NULL)return Result(BSP_NOR_NOT_READY);
    if(!Acquire())return Result(BSP_NOR_BUSY);
    g_bsp_nor.magic=0x4E4F5231U;g_bsp_nor.version=1U;g_bsp_nor.ready=0U;
    __HAL_RCC_GPIOF_CLK_ENABLE();Select(0U);
    GPIO_InitTypeDef gpio={0};gpio.Pin=GPIO_PIN_6;gpio.Mode=GPIO_MODE_OUTPUT_PP;
    gpio.Pull=GPIO_PULLUP;gpio.Speed=GPIO_SPEED_FREQ_HIGH;HAL_GPIO_Init(GPIOF,&gpio);
    BSP_NOR_Status status=SetBits(8U);
    uint8_t tx[4]={0x9FU,0xFFU,0xFFU,0xFFU},rx[4]={0};
    if(status==BSP_NOR_OK) {Select(1U);status=Bytes(tx,rx,4U);Select(0U);}
    if(status==BSP_NOR_OK) {
        g_bsp_nor.jedec_id=((uint32_t)rx[1]<<16)|((uint32_t)rx[2]<<8)|rx[3];
        if(g_bsp_nor.jedec_id!=BSP_NOR_EXPECTED_JEDEC_ID)status=BSP_NOR_ID_MISMATCH;
    }
    if(status==BSP_NOR_OK) {
        tx[0]=0x05U;Select(1U);status=Bytes(tx,rx,2U);Select(0U);
        g_bsp_nor.status_register=rx[1];
        if(status==BSP_NOR_OK && (rx[1]&1U))status=BSP_NOR_BUSY;
    }
    g_bsp_nor.capacity_bytes=BSP_NOR_CAPACITY_BYTES;
    g_bsp_nor.spi_clock_hz=HAL_RCC_GetPCLK2Freq()/(2U<<((hspi5.Instance->CR1&SPI_CR1_BR)>>SPI_CR1_BR_Pos));
    if(status==BSP_NOR_OK)g_bsp_nor.ready=1U;
    EndTransfer(status!=BSP_NOR_OK);return Result(status);
}

/* Independent byte path is deliberately short and does not reuse the DMA byte
 * reconstruction. Compare it with DMA at low/high addresses before trusting a
 * full backup; a successful JEDEC response alone cannot validate bulk transport. */
BSP_NOR_Status BSP_NOR_ReadPolling(uint32_t address,void *destination,uint32_t length)
{
    if(!ContextOkay())return Result(BSP_NOR_CONTEXT);
    if(!destination || length==0U || length>256U || address>=BSP_NOR_CAPACITY_BYTES || length>BSP_NOR_CAPACITY_BYTES-address)return Result(BSP_NOR_ARGUMENT);
    if(!g_bsp_nor.ready)return Result(BSP_NOR_NOT_READY);
    if(!Acquire())return Result(BSP_NOR_BUSY);
    uint8_t tx[256];for(uint32_t i=0;i<length;++i)tx[i]=0xFFU;
    BSP_NOR_Status status=ReadHeader(address);
    if(status==BSP_NOR_OK)status=Bytes(tx,destination,(uint16_t)length);
    EndTransfer(status!=BSP_NOR_OK);return Result(status);
}

BSP_NOR_Status BSP_NOR_Read(uint32_t address,void *destination,uint32_t length)
{
    if(!ContextOkay())return Result(BSP_NOR_CONTEXT);
    if(!destination || length==0U || address>=BSP_NOR_CAPACITY_BYTES || length>BSP_NOR_CAPACITY_BYTES-address)return Result(BSP_NOR_ARGUMENT);
    if(!g_bsp_nor.ready)return Result(BSP_NOR_NOT_READY);
    if(!Acquire())return Result(BSP_NOR_BUSY);
    g_bsp_nor.last_address=address;g_bsp_nor.last_length=length;
    uint8_t *out=destination;uint32_t remaining=length;BSP_NOR_Status status=BSP_NOR_OK;
    while(remaining && status==BSP_NOR_OK) {
        uint32_t chunk=remaining>BSP_NOR_READ_CHUNK_BYTES?BSP_NOR_READ_CHUNK_BYTES:remaining;
        status=ReadHeader(address);
        uint32_t even=chunk&~1U;
        if(status==BSP_NOR_OK && even) {
            status=SetBits(16U);
            /* Fixed dummy Tx supplies clocks without reading past two bytes.
             * Rx uses halfword increment; IRQ priority4 never calls the RTOS. */
            CLEAR_BIT(hspi5.hdmatx->Instance->CR,DMA_SxCR_MINC);
            SET_BIT(hspi5.hdmarx->Instance->CR,DMA_SxCR_MINC);
            dma_done=dma_failed=0U;__DMB();
            HAL_StatusTypeDef hal=HAL_ERROR;
            if(status==BSP_NOR_OK)hal=HAL_SPI_TransmitReceive_DMA(&hspi5,(const uint8_t*)&dummy,(uint8_t*)bounce,(uint16_t)(even/2U));
            g_bsp_nor.hal_status=(uint32_t)hal;
            if(status==BSP_NOR_OK && hal!=HAL_OK)status=BSP_NOR_IO;
            uint32_t start=HAL_GetTick();
            while(status==BSP_NOR_OK && !dma_done && !dma_failed) {
                if(HAL_GetTick()-start>NOR_TIMEOUT_MS){status=BSP_NOR_TIMEOUT;break;}
                if(osKernelGetState()==osKernelRunning)(void)osDelay(1U);
            }
            if(dma_failed)status=BSP_NOR_IO;
            __DMB();
            if(status==BSP_NOR_OK) {
                /* Wire byte0 becomes bits15..8; little-endian DMA stores low
                 * byte first. Reconstruct bytes explicitly, not with cast-copy. */
                for(uint32_t i=0;i<even/2U;++i){out[2U*i]=(uint8_t)(bounce[i]>>8);out[2U*i+1U]=(uint8_t)bounce[i];}
            }
        }
        if(status==BSP_NOR_OK && (chunk&1U)) {
            uint8_t tx=0xFFU;status=SetBits(8U);
            if(status==BSP_NOR_OK)status=Bytes(&tx,out+even,1U);
        }
        Select(0U);
        if(status==BSP_NOR_OK){address+=chunk;out+=chunk;remaining-=chunk;g_bsp_nor.bytes_read+=chunk;}
    }
    if(status==BSP_NOR_OK)++g_bsp_nor.reads;
    EndTransfer(status!=BSP_NOR_OK);return Result(status);
}

/* No HAL wait, allocation, task wakeup or RTOS API is legal in these IRQ hooks. */
void BSP_NOR_OnTxRxComplete(SPI_HandleTypeDef *spi)
{
    if(spi==&hspi5){++g_bsp_nor.dma_completions;__DMB();dma_done=1U;}
}
void BSP_NOR_OnError(SPI_HandleTypeDef *spi)
{
    if(spi==&hspi5){++g_bsp_nor.dma_errors;__DMB();dma_failed=1U;}
}

/* Stock BL consumes slot/length/CRC at these fixed internal-flash addresses.
 * A nonzero CRC/request field is treated as pending or unknown. Slot/length
 * may remain after a completed update and are therefore not a pending test;
 * no storage action tries to clear/repair internal metadata. */
static uint32_t NoPendingInstall(void)
{
    const volatile uint32_t *meta=(const volatile uint32_t *)0x08008000U;
    return RECOVERY_VERSION_SUPPORTED(meta[0]) && meta[4]==0U;
}
BSP_NOR_Status BSP_NOR_UnlockStorage(uint32_t token)
{
    if(!ContextOkay())return Result(BSP_NOR_CONTEXT);
    if(!g_bsp_nor.ready)return Result(BSP_NOR_NOT_READY);
    if(token!=0x42414B32U || !NoPendingInstall())return Result(BSP_NOR_LOCKED);
    storage_unlocked=1U;return Result(BSP_NOR_OK);
}
void BSP_NOR_LockStorage(void){storage_unlocked=0U;__DMB();}
uint32_t BSP_NOR_IsStorageUnlocked(void){return storage_unlocked;}
/* Runtime permission is restored only by the Settings owner's validated
 * provisioning marker. Keeping it separate prevents a USB cable reset from
 * disabling normal file/link-key persistence on an already provisioned unit. */
BSP_NOR_Status BSP_NOR_EnableProvisionedStorage(uint32_t token)
{
    if(!ContextOkay())return Result(BSP_NOR_CONTEXT);
    if(!g_bsp_nor.ready)return Result(BSP_NOR_NOT_READY);
    if(token!=BSP_NOR_PROVISIONED_TOKEN || !NoPendingInstall())return Result(BSP_NOR_LOCKED);
    if(format_active)return Result(BSP_NOR_BUSY);
    runtime_write_permit=1U;__DMB();return Result(BSP_NOR_OK);
}
void BSP_NOR_DisableProvisionedStorage(void){runtime_write_permit=0U;__DMB();}
uint32_t BSP_NOR_CanWriteStorage(void)
{
    return format_active?storage_unlocked:(storage_unlocked || runtime_write_permit);
}
/* A format remains host-only for its complete duration. Do not clear this
 * active flag in the USB reset IRQ: doing so would accidentally fall back to
 * runtime permission halfway through f_mkfs after the host gate was revoked. */
BSP_NOR_Status BSP_NOR_BeginFormat(void)
{
    if(!ContextOkay())return Result(BSP_NOR_CONTEXT);
    if(!g_bsp_nor.ready)return Result(BSP_NOR_NOT_READY);
    if(!storage_unlocked || !NoPendingInstall())return Result(BSP_NOR_LOCKED);
    if(format_active)return Result(BSP_NOR_BUSY);
    format_active=1U;__DMB();return Result(BSP_NOR_OK);
}
void BSP_NOR_EndFormat(void){format_active=0U;__DMB();}
BSP_NOR_Status BSP_NOR_OTAEnable(uint32_t transaction_id)
{
    if(!ContextOkay())return Result(BSP_NOR_CONTEXT);
    if(!g_bsp_nor.ready)return Result(BSP_NOR_NOT_READY);
    if(!transaction_id || !NoPendingInstall())return Result(BSP_NOR_LOCKED);
    if(ota_transaction && ota_transaction!=transaction_id)return Result(BSP_NOR_BUSY);
    ota_transaction=transaction_id;return Result(BSP_NOR_OK);
}
void BSP_NOR_OTADisable(void){ota_transaction=0U;__DMB();}

/* Read status with CS toggled each time. Polling holds software bus ownership,
 * but yields between requests so graphics/USB can run throughout a sector erase. */
static BSP_NOR_Status ReadStatus(uint8_t *value)
{
    uint8_t tx[2]={0x05U,0xFFU},rx[2]={0};
    Select(1U);BSP_NOR_Status status=Bytes(tx,rx,2U);Select(0U);
    *value=rx[1];g_bsp_nor.status_register=rx[1];return status;
}
static BSP_NOR_Status WaitReady(uint32_t timeout)
{
    uint32_t start=HAL_GetTick();
    for(;;){uint8_t value;BSP_NOR_Status status=ReadStatus(&value);
        if(status!=BSP_NOR_OK || !(value&1U))return status;
        if(HAL_GetTick()-start>timeout)return BSP_NOR_TIMEOUT;
        if(osKernelGetState()==osKernelRunning)(void)osDelay(1U);
    }
}
/* WREN is never issued until both capability and complete address bounds were
 * checked. WEL must latch; BP protection is never disabled automatically. */
static BSP_NOR_Status WriteEnable(void)
{
    BSP_NOR_Status status=WaitReady(500U);uint8_t value=0U,tx=0x06U,rx;
    if(status==BSP_NOR_OK){Select(1U);status=Bytes(&tx,&rx,1U);Select(0U);}
    if(status==BSP_NOR_OK)status=ReadStatus(&value);
    if(status==BSP_NOR_OK && !(value&2U))status=BSP_NOR_PROTECTED;
    return status;
}
/* Dedicated4-byte opcodes12/21 do not modify the global addressing mode. Every
 * write verifies readback, including protected chips which may ignore an opcode
 * and nevertheless return WIP=0. Timeout invalidates readiness until re-init,
 * because hardware may still be internally busy after software gives up. */
static uint32_t container_clusters[8][32],container_count[8];
static volatile uint32_t provision_first,provision_bytes,provision_metadata;
void BSP_NOR_ProvisionLock(void){provision_bytes=0;provision_metadata=0;__DMB();}
BSP_NOR_Status BSP_NOR_ProvisionGrant(uint32_t first,uint32_t bytes,uint32_t mask)
{
    BSP_NOR_ProvisionLock();
    if(!ContextOkay())return Result(BSP_NOR_CONTEXT);
    if(!g_bsp_nor.ready||!NoPendingInstall())return Result(BSP_NOR_LOCKED);
    if(first<0x9000U||(first-0x9000U)%32768U||!bytes||(bytes%32768U)||
       bytes>1048576U||first>=0x07F70000U||bytes>0x07F70000U-first||(mask&~0x1feU))return Result(BSP_NOR_ARGUMENT);
    provision_first=first;provision_metadata=mask;__DMB();provision_bytes=bytes;
    return Result(BSP_NOR_OK);
}
void BSP_NOR_ClearContainers(void)
{for(uint32_t i=0;i<8;++i)container_count[i]=0;__DMB();}
BSP_NOR_Status BSP_NOR_GrantContainer(uint32_t file,const uint32_t *clusters,uint32_t count)
{
    if(!ContextOkay())return Result(BSP_NOR_CONTEXT);
    static const uint8_t counts[8]={4,8,32,16,16,2,8,4};
    if(file>=8||!clusters||count!=counts[file])return Result(BSP_NOR_ARGUMENT);
    if(!g_bsp_nor.ready||!NoPendingInstall())return Result(BSP_NOR_LOCKED);
    for(uint32_t i=0;i<count;++i){uint32_t a=clusters[i];
        if(a<0x9000U||(a-0x9000U)%32768U||a>0x07F70000U-32768U)return Result(BSP_NOR_ARGUMENT);
        for(uint32_t j=0;j<i;++j)if(a==clusters[j])return Result(BSP_NOR_ARGUMENT);
        for(uint32_t f=0;f<8;++f)if(f!=file)for(uint32_t j=0;j<container_count[f];++j)
            if(a==container_clusters[f][j])return Result(BSP_NOR_PROTECTED);
    }
    container_count[file]=0;for(uint32_t i=0;i<count;++i)container_clusters[file][i]=clusters[i];
    __DMB();container_count[file]=count;return Result(BSP_NOR_OK);
}
static uint32_t ContainerRange(uint32_t capability,uint32_t address,uint32_t bytes)
{
    if(capability==5U){
        uint32_t n=provision_bytes,first=provision_first;
        if(!n||!bytes)return 0;
        if(address>=first&&address-first<n&&bytes<=n-(address-first))return 1;
        uint32_t sector=address/4096U;
        return sector>=1&&sector<=8&&(provision_metadata&(1U<<sector))&&bytes<=4096U-(address&4095U);
    }
    if(capability<2||capability>10||!bytes)return 0;
    /* Capability5 remains the separate create-only provision lease. */
    uint32_t file=capability<5U?capability-2U:capability-3U;
    for(uint32_t i=0;i<container_count[file];++i){uint32_t a=container_clusters[file][i];
        if(address>=a&&address-a<32768U&&bytes<=32768U-(address-a))return 1;}
    return 0;
}
static uint32_t WriteRangeAllowed(uint32_t ota,uint32_t address,uint32_t length,uint32_t erase)
{
    uint32_t first=ota==1?0x07F90000U:0U,end=ota==1?0x08000000U:0x07F80000U;
    uint32_t bytes=erase?4096U:length;
    if(ota>=2&&!ContainerRange(ota,address,bytes))return 0;
    if(address<first || address>=end || !bytes || bytes>end-address)return 0U;
    if(erase?(address&4095U):(length>256U || (address&255U)+length>256U))return 0U;
    return 1U;
}
static BSP_NOR_Status Mutate(uint32_t ota,uint32_t address,const uint8_t *data,uint32_t length,uint32_t erase)
{
    if(!ContextOkay())return Result(BSP_NOR_CONTEXT);
    if(!g_bsp_nor.ready)return Result(BSP_NOR_NOT_READY);
    if(!(ota>=2?ContainerRange(ota,address,erase?4096U:length):ota?ota_transaction:BSP_NOR_CanWriteStorage()) || !NoPendingInstall())return Result(BSP_NOR_LOCKED);
    uint32_t bytes=erase?4096U:length;
    if(!WriteRangeAllowed(ota,address,length,erase) || (!erase && !data))return Result(BSP_NOR_ARGUMENT);
    if(!Acquire())return Result(BSP_NOR_BUSY);
    BSP_NOR_Status status=SetBits(8U);
    if(status==BSP_NOR_OK)status=WriteEnable();
    /* Re-check host revocation after the yield inside WriteEnable. Revocation
     * cannot interrupt an already submitted physical page/erase command. */
    if(status==BSP_NOR_OK && !(ota>=2?ContainerRange(ota,address,bytes):ota?ota_transaction:BSP_NOR_CanWriteStorage()))status=BSP_NOR_LOCKED;
    uint8_t header[5]={erase?0x21U:0x12U,(uint8_t)(address>>24),(uint8_t)(address>>16),(uint8_t)(address>>8),(uint8_t)address};
    uint8_t discard[256];
    if(status==BSP_NOR_OK){Select(1U);status=Bytes(header,discard,5U);
        if(status==BSP_NOR_OK && !erase)status=Bytes(data,discard,(uint16_t)length);
        Select(0U);
    }
    if(status==BSP_NOR_OK)status=WaitReady(erase?500U:10U);
    /* Verify under the same ownership, with independent8-bit reads. */
    uint8_t dummy_bytes[256];for(uint32_t i=0;i<256U;++i)dummy_bytes[i]=0xFFU;
    for(uint32_t offset=0;status==BSP_NOR_OK && offset<bytes;offset+=256U){
        uint32_t count=bytes-offset>256U?256U:bytes-offset;
        status=ReadHeader(address+offset);
        if(status==BSP_NOR_OK)status=Bytes(dummy_bytes,discard,(uint16_t)count);
        Select(0U);
        if(status==BSP_NOR_OK)for(uint32_t i=0;i<count;++i)if(discard[i]!=(erase?0xFFU:data[offset+i])){status=BSP_NOR_VERIFY;break;}
    }
    if(status==BSP_NOR_TIMEOUT)g_bsp_nor.ready=0U;
    EndTransfer(status!=BSP_NOR_OK);return Result(status);
}
BSP_NOR_Status BSP_NOR_Program(uint32_t address,const void *source,uint32_t length){return Mutate(0U,address,source,length,0U);}
BSP_NOR_Status BSP_NOR_Erase4K(uint32_t address){return Mutate(0U,address,NULL,0U,1U);}
BSP_NOR_Status BSP_NOR_OTAProgram(uint32_t address,const void *source,uint32_t length){return Mutate(1U,address,source,length,0U);}
BSP_NOR_Status BSP_NOR_OTAErase4K(uint32_t address){return Mutate(1U,address,NULL,0U,1U);}
BSP_NOR_Status BSP_NOR_ContainerProgram(uint32_t file,uint32_t address,const void *data,uint32_t bytes)
{return file<8?Mutate(file+(file<3?2U:3U),address,data,bytes,0):BSP_NOR_ARGUMENT;}
BSP_NOR_Status BSP_NOR_ContainerErase(uint32_t file,uint32_t address)
{return file<8?Mutate(file+(file<3?2U:3U),address,NULL,0,1):BSP_NOR_ARGUMENT;}
BSP_NOR_Status BSP_NOR_ProvisionProgram(uint32_t address,const void *data,uint32_t bytes)
{return Mutate(5U,address,data,bytes,0);}
BSP_NOR_Status BSP_NOR_ProvisionErase(uint32_t address)
{return Mutate(5U,address,NULL,0,1);}
