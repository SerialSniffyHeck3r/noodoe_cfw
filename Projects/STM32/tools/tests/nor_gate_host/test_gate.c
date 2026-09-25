/* Real BSP_NOR.c policy with real STM32 register types. HAL SPI mocks never
 * touch a physical device; tests force bus BUSY except the revocation test. */
#include "BSP_NOR.h"
#include "cmsis_os2.h"
#include <stddef.h>
SPI_HandleTypeDef hspi5;
volatile uint32_t g_assertions,g_failure_line,g_mock_error;
static uint32_t tick,revoke_on_wren,revoke_container,mutations;
#define CHECK(x) do{++g_assertions;if(!(x)){g_failure_line=__LINE__;return 1;}}while(0)
uint32_t HAL_GetTick(void){return ++tick;}
void HAL_GPIO_WritePin(GPIO_TypeDef *port,uint16_t pins,GPIO_PinState state){(void)port;(void)pins;(void)state;}
void HAL_GPIO_Init(GPIO_TypeDef *port,GPIO_InitTypeDef *init){(void)port;(void)init;}
uint32_t HAL_RCC_GetPCLK2Freq(void){return 84000000U;}
HAL_StatusTypeDef HAL_SPI_Abort(SPI_HandleTypeDef *spi){(void)spi;return HAL_OK;}
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *spi,const uint8_t *tx,uint8_t *rx,uint16_t n,uint32_t timeout)
{
    (void)spi;(void)timeout;for(uint32_t i=0;i<n;++i)rx[i]=0U;
    if(tx[0]==0x05U && n==2U)rx[1]=2U;
    if(tx[0]==0x06U && revoke_on_wren)BSP_NOR_LockStorage();
    if(tx[0]==0x06U && revoke_container)BSP_NOR_ClearContainers();
    if(tx[0]==0x12U || tx[0]==0x21U)++mutations;
    return HAL_OK;
}
osKernelState_t osKernelGetState(void){return osKernelRunning;}
osStatus_t osDelay(uint32_t delay){(void)delay;return osOK;}
int NorGate_TestMain(void)
{
    volatile uint32_t *meta=(volatile uint32_t*)0x08008000U;
    meta[0]=0xE0000U;meta[4]=0U;g_bsp_nor.ready=1U;g_bsp_nor.busy=1U;
    uint8_t byte=0x55U;
    CHECK(!BSP_NOR_IsStorageUnlocked() && !BSP_NOR_CanWriteStorage());
    CHECK(BSP_NOR_Program(0,&byte,1)==BSP_NOR_LOCKED);
    CHECK(BSP_NOR_EnableProvisionedStorage(0)==BSP_NOR_LOCKED);
    CHECK(BSP_NOR_EnableProvisionedStorage(BSP_NOR_PROVISIONED_TOKEN)==BSP_NOR_OK);
    CHECK(BSP_NOR_CanWriteStorage() && !BSP_NOR_IsStorageUnlocked());
    CHECK(BSP_NOR_BeginFormat()==BSP_NOR_LOCKED);
    CHECK(BSP_NOR_Program(0,&byte,1)==BSP_NOR_BUSY);
    BSP_NOR_LockStorage();CHECK(BSP_NOR_CanWriteStorage());
    CHECK(BSP_NOR_Program(0x07F80000U,&byte,1)==BSP_NOR_ARGUMENT);
    CHECK(BSP_NOR_Program(0x07F90000U,&byte,1)==BSP_NOR_ARGUMENT);
    CHECK(BSP_NOR_UnlockStorage(0x42414B32U)==BSP_NOR_OK);
    CHECK(BSP_NOR_BeginFormat()==BSP_NOR_OK);
    CHECK(BSP_NOR_BeginFormat()==BSP_NOR_BUSY);
    CHECK(BSP_NOR_EnableProvisionedStorage(BSP_NOR_PROVISIONED_TOKEN)==BSP_NOR_BUSY);
    BSP_NOR_LockStorage();CHECK(!BSP_NOR_CanWriteStorage());
    CHECK(BSP_NOR_Program(0,&byte,1)==BSP_NOR_LOCKED);
    CHECK(BSP_NOR_OTAEnable(1)==BSP_NOR_OK);
    CHECK(BSP_NOR_OTAProgram(0x07F90000U,&byte,1)==BSP_NOR_BUSY);
    CHECK(BSP_NOR_OTAProgram(0x07F80000U,&byte,1)==BSP_NOR_ARGUMENT);
    BSP_NOR_EndFormat();CHECK(BSP_NOR_CanWriteStorage());
    /* Physical page eligibility is checked again after WREN's yielding wait.
     * An IRQ-equivalent host reset cannot use runtime permit during format. */
    g_bsp_nor.busy=0U;hspi5.Instance=SPI5;revoke_on_wren=1U;
    CHECK(BSP_NOR_UnlockStorage(0x42414B32U)==BSP_NOR_OK);
    CHECK(BSP_NOR_BeginFormat()==BSP_NOR_OK);
    CHECK(BSP_NOR_Program(0,&byte,1)==BSP_NOR_LOCKED);CHECK(mutations==0U);
    BSP_NOR_EndFormat();BSP_NOR_DisableProvisionedStorage();CHECK(!BSP_NOR_CanWriteStorage());
    meta[4]=1U;CHECK(BSP_NOR_EnableProvisionedStorage(BSP_NOR_PROVISIONED_TOKEN)==BSP_NOR_LOCKED);
    CHECK(BSP_NOR_UnlockStorage(0x42414B32U)==BSP_NOR_LOCKED);
    meta[4]=0U;meta[0]=0xFFFFFFFFU;CHECK(BSP_NOR_EnableProvisionedStorage(BSP_NOR_PROVISIONED_TOKEN)==BSP_NOR_LOCKED);
    meta[0]=0xE0000U;CHECK(BSP_NOR_EnableProvisionedStorage(BSP_NOR_PROVISIONED_TOKEN)==BSP_NOR_OK);
    CHECK(BSP_NOR_UnlockStorage(0x42414B32U)==BSP_NOR_OK);
    /* Init clears every capability even if hardware initialization fails. */
    hspi5.Instance=NULL;CHECK(BSP_NOR_Init()==BSP_NOR_NOT_READY);
    CHECK(!BSP_NOR_CanWriteStorage() && !BSP_NOR_IsStorageUnlocked());
    g_bsp_nor.ready=1U;CHECK(BSP_NOR_OTAProgram(0x07F90000U,&byte,1)==BSP_NOR_LOCKED);
    /* Fragmented fixed files grant only their own audited clusters. Neither
     * generic storage permission nor a host unlock expands this capability. */
    uint32_t cfg[4]={0x9000,0x19000,0x29000,0x39000};
    uint32_t ride[8]={0x9000,0x11000,0x21000,0x31000,0x41000,0x49000,0x51000,0x59000};
    CHECK(BSP_NOR_GrantContainer(0,cfg,4)==BSP_NOR_OK);
    CHECK(BSP_NOR_GrantContainer(1,ride,8)==BSP_NOR_PROTECTED);
    cfg[1]=cfg[0];CHECK(BSP_NOR_GrantContainer(0,cfg,4)==BSP_NOR_ARGUMENT);cfg[1]=0x19000;
    cfg[3]=0x07F71000;CHECK(BSP_NOR_GrantContainer(0,cfg,4)==BSP_NOR_ARGUMENT);cfg[3]=0x39000;
    g_bsp_nor.busy=1;
    CHECK(BSP_NOR_ContainerProgram(0,0x9000,&byte,1)==BSP_NOR_BUSY);
    CHECK(BSP_NOR_ContainerProgram(0,0x10FFF,&byte,1)==BSP_NOR_BUSY);
    CHECK(BSP_NOR_ContainerProgram(0,0x11000,&byte,1)==BSP_NOR_LOCKED);
    CHECK(BSP_NOR_ContainerProgram(1,0x9000,&byte,1)==BSP_NOR_LOCKED);
    CHECK(BSP_NOR_ContainerProgram(0,0x07F70000,&byte,1)==BSP_NOR_LOCKED);
    CHECK(BSP_NOR_ContainerProgram(0,0x7F80000,&byte,1)==BSP_NOR_LOCKED);
    CHECK(BSP_NOR_ContainerProgram(0,0xFFFFFFFF,&byte,1)==BSP_NOR_LOCKED);
    CHECK(BSP_NOR_ContainerErase(0,0xA000)==BSP_NOR_BUSY);
    CHECK(BSP_NOR_ContainerErase(0,0x9001)==BSP_NOR_ARGUMENT);
    /* New firmware slots and boot journal remain separate fixed-file
     * capabilities, including from the bootstrap provisioning capability5. */
    uint32_t a[16],b[16],boot[2]={0x209000U,0x211000U};
    for(uint32_t n=0;n<16;++n){a[n]=0x109000U+n*32768U;b[n]=0x189000U+n*32768U;}
    CHECK(BSP_NOR_GrantContainer(3,a,16)==BSP_NOR_OK);
    CHECK(BSP_NOR_GrantContainer(4,a,16)==BSP_NOR_PROTECTED);
    CHECK(BSP_NOR_GrantContainer(4,b,16)==BSP_NOR_OK);
    CHECK(BSP_NOR_GrantContainer(5,boot,2)==BSP_NOR_OK);
    CHECK(BSP_NOR_GrantContainer(5,boot,1)==BSP_NOR_ARGUMENT);
    CHECK(BSP_NOR_GrantContainer(6,boot,2)==BSP_NOR_ARGUMENT);
    CHECK(BSP_NOR_ContainerErase(3,a[0])==BSP_NOR_BUSY);
    CHECK(BSP_NOR_ContainerErase(4,b[0])==BSP_NOR_BUSY);
    CHECK(BSP_NOR_ContainerErase(5,boot[0])==BSP_NOR_BUSY);
    CHECK(BSP_NOR_ContainerErase(3,b[0])==BSP_NOR_LOCKED);
    CHECK(BSP_NOR_ContainerErase(4,boot[0])==BSP_NOR_LOCKED);
    CHECK(BSP_NOR_ContainerErase(5,0x1000U)==BSP_NOR_LOCKED);
    CHECK(BSP_NOR_ContainerErase(5,0x07F90000U)==BSP_NOR_LOCKED);
    /* The capability is checked again after a yielding WREN. */
    hspi5.Instance=SPI5;g_bsp_nor.busy=0;revoke_container=1;
    CHECK(BSP_NOR_ContainerProgram(0,0x9000,&byte,1)==BSP_NOR_LOCKED);
    CHECK(BSP_NOR_ContainerProgram(0,0x9000,&byte,1)==BSP_NOR_LOCKED);
    CHECK(mutations==0U);CHECK(g_mock_error==0U);return 0;
}
