#include "bsp_boot.h"
#include "stm32f4xx.h"

/* Application profiles override these without making BSP include APP headers. */
__attribute__((weak)) void BSP_BootSafetyStart(void) {}
__attribute__((weak)) void BSP_ApplicationEarly(void) {}

/* 초기값을 붙이지 않는다. 링커가 이 섹션을 .bss 밖의 NOLOAD로 유지해야 한다. */
volatile BSP_BootDiagnostics g_bsp_boot
    __attribute__((section(".noinit.bsp_boot"), aligned(8), used));
_Static_assert(sizeof(BSP_BootDiagnostics) <= 256U, "Boot diagnostics exceed reserved budget");

/* tick이 아직 없으므로 횟수로 제한한다. 실제 밀리초 timeout을 뜻하지 않는다. */
#define BOOT_WAIT_LIMIT 2000000UL
#define BOOT_DMA_CLOCKS (RCC_AHB1ENR_DMA1EN | RCC_AHB1ENR_DMA2EN)
#define BOOT_PLL_READY (RCC_CR_PLLRDY | RCC_CR_PLLI2SRDY | RCC_CR_PLLSAIRDY)

/*
 * 실패 상태를 남기고 더 이상 watchdog을 갱신하지 않는다. 독립 S4 gate가
 * 시작한 IWDG가 초기화 실패도 회수한다. 실패 루프에서 flash를 쓰지 않는다.
 * IRQ는 계속 막고 전원 GPIO는 건드리지 않는다. WFI는 디버깅 편의를 위해 쓰지 않는다.
 */
__attribute__((noreturn)) static void BootFail(uint32_t status, uint32_t detail)
{
    __disable_irq();
    g_bsp_boot.error_detail = detail;
    g_bsp_boot.status = status;
    __DSB();
    for (;;) {
        g_bsp_boot.failure_spins++;
        __NOP();
    }
}

/*
 * 레지스터의 지정 비트가 원하는 값이 될 때까지만 기다린다. HAL_GetTick, 전역
 * 타이머, libc를 사용하지 않아 .data/.bss 초기화 전에도 호출할 수 있다.
 * 마지막 레지스터 값/주소/남은 횟수를 남겨 timeout 원인을 구분한다.
 */
static void BootWaitBits(volatile uint32_t *reg, uint32_t mask,
                         uint32_t expected, uint32_t failure)
{
    uint32_t remaining = BOOT_WAIT_LIMIT;
    uint32_t value;
    do {
        value = *reg;
        if ((value & mask) == expected) {
            return;
        }
    } while (--remaining != 0U);
    g_bsp_boot.wait_address = (uint32_t)reg;
    g_bsp_boot.wait_value = value;
    g_bsp_boot.wait_mask = mask;
    g_bsp_boot.wait_expected = expected;
    g_bsp_boot.wait_remaining = remaining;
    BootFail(failure, (uint32_t)reg);
}

/*
 * STM32F4의 DMA stream은 첫 stream부터 0x18 byte 간격이다. 상수 포인터 배열을
 * .data에 두지 않고 직접 계산한다. DMA clock을 켠 뒤에만 이 함수를 사용한다.
 */
static DMA_Stream_TypeDef *BootDmaStream(uint32_t first, uint32_t index)
{
    return (DMA_Stream_TypeDef *)(first + index * 0x18U);
}

/*
 * DMA의 EN을 먼저 모두 내리고 실제 EN clear를 확인한 뒤 DMA1/2만 reset한다.
 * 새 APP의 RAM을 DMA가 계속 쓰는 일을 막기 위한 처리이며 GPIO reset은 하지 않는다.
 * 원래 DMA clock enable 비트는 진단에 보존하고, 작업 뒤 원래 enable 상태로 돌린다.
 */
static void BootStopDma(void)
{
    uint32_t index;
    RCC->AHB1ENR |= BOOT_DMA_CLOCKS;
    (void)RCC->AHB1ENR;
    __DSB();
    for (index = 0U; index < 8U; index++) {
        DMA_Stream_TypeDef *dma1 = BootDmaStream(DMA1_Stream0_BASE, index);
        DMA_Stream_TypeDef *dma2 = BootDmaStream(DMA2_Stream0_BASE, index);
        if ((dma1->CR & DMA_SxCR_EN) != 0U) {
            g_bsp_boot.entry_dma1_en_mask |= 1UL << index;
        }
        if ((dma2->CR & DMA_SxCR_EN) != 0U) {
            g_bsp_boot.entry_dma2_en_mask |= 1UL << index;
        }
        dma1->CR &= ~DMA_SxCR_EN;
        dma2->CR &= ~DMA_SxCR_EN;
    }
    for (index = 0U; index < 8U; index++) {
        BootWaitBits(&BootDmaStream(DMA1_Stream0_BASE, index)->CR,
                     DMA_SxCR_EN, 0U, BSP_BOOT_FAIL_DMA1);
        BootWaitBits(&BootDmaStream(DMA2_Stream0_BASE, index)->CR,
                     DMA_SxCR_EN, 0U, BSP_BOOT_FAIL_DMA2);
    }
    RCC->AHB1RSTR |= RCC_AHB1RSTR_DMA1RST | RCC_AHB1RSTR_DMA2RST;
    __DSB();
    RCC->AHB1RSTR &= ~(RCC_AHB1RSTR_DMA1RST | RCC_AHB1RSTR_DMA2RST);
    RCC->AHB1ENR = (RCC->AHB1ENR & ~BOOT_DMA_CLOCKS)
                 | (g_bsp_boot.entry_ahb1enr & BOOT_DMA_CLOCKS);
    __DSB();
    g_bsp_boot.status = BSP_BOOT_DMA_QUIET;
}

/*
 * 사용 가능한 인터럽트와 이벤트 발생원을 초기 상태로 만든다. PRIMASK만 내려서는
 * pending이 남으므로 NVIC pending 및 SysTick/PendSV pending도 별도로 지운다.
 * 시스템 예외 우선순위/MPU/sleep 설정은 새 런타임의 기본값으로 되돌리며,
 * GPIO/EXTI 선의 물리 입력 방향은 바꾸지 않고 EXTI interrupt/event만 막는다.
 */
static void BootQuietCpu(void)
{
    uint32_t index;
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;
    for (index = 0U; index < 3U; index++) {
        NVIC->ICER[index] = 0xFFFFFFFFUL;
        NVIC->ICPR[index] = 0xFFFFFFFFUL;
    }
    for (index = 0U; index <= (uint32_t)DMA2D_IRQn; index++) {
        NVIC->IP[index] = 0U;
    }
    for (index = 0U; index < 12U; index++) {
        SCB->SHP[index] = 0U;
    }
    EXTI->IMR = 0U;
    EXTI->EMR = 0U;
    EXTI->PR = 0x007FFFFFUL;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
    SCB->SCR = 0U;
    SCB->CCR = SCB_CCR_STKALIGN_Msk;
    SCB->SHCSR = 0U;
    SCB->CFSR = SCB->CFSR;
    SCB->HFSR = SCB->HFSR;
    SCB->DFSR = SCB->DFSR;
    SCB->AIRCR = (0x5FAUL << SCB_AIRCR_VECTKEY_Pos)
               | (SCB->AIRCR & SCB_AIRCR_ENDIANESS_Msk);
    __DSB();
    __ISB();
}

/*
 * 클록은 HSI로 천천히 낮춘 뒤 분주를 /1로 만든다. 고속 PLL인 동안에는 전압
 * 스케일과 FLASH latency를 낮추지 않는다. 순정 BL의 latency5보다 낮추지 않은
 * 상태에서 HSI를 선택하고, PLL 세 종류가 꺼진 것을 확인한 다음 기본 PLL 값을 쓴다.
 * HSE와 RTC용 RTCPRE, BDCR, CSR(LSI), PWR는 보존한다. RTC가 HSE를 쓰더라도
 * HSE를 꺼서 시간을 멈추지 않기 위함이다. HSI 값은 nominal 16MHz 기준이다.
 */
static void BootUseHsi(void)
{
    uint32_t acr = FLASH->ACR;
    if ((acr & FLASH_ACR_LATENCY) < FLASH_ACR_LATENCY_5WS) {
        FLASH->ACR = (acr & ~FLASH_ACR_LATENCY) | FLASH_ACR_LATENCY_5WS;
        BootWaitBits(&FLASH->ACR, FLASH_ACR_LATENCY, FLASH_ACR_LATENCY_5WS,
                     BSP_BOOT_FAIL_FLASH);
    }
    RCC->CR |= RCC_CR_HSION;
    BootWaitBits(&RCC->CR, RCC_CR_HSIRDY, RCC_CR_HSIRDY, BSP_BOOT_FAIL_HSI);
    RCC->CFGR &= ~RCC_CFGR_SW;
    BootWaitBits(&RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_HSI, BSP_BOOT_FAIL_SWITCH);
    RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);
    RCC->CR &= ~(RCC_CR_PLLON | RCC_CR_PLLI2SON | RCC_CR_PLLSAION | RCC_CR_CSSON);
    BootWaitBits(&RCC->CR, BOOT_PLL_READY, 0U, BSP_BOOT_FAIL_PLL);
    RCC->PLLCFGR = 0x24003010UL;
    RCC->CIR = RCC_CIR_LSIRDYC | RCC_CIR_LSERDYC | RCC_CIR_HSIRDYC
             | RCC_CIR_HSERDYC | RCC_CIR_PLLRDYC | RCC_CIR_PLLI2SRDYC
             | RCC_CIR_PLLSAIRDYC | RCC_CIR_CSSC;
    __DSB();
    __ISB();
    g_bsp_boot.status = BSP_BOOT_HSI_SELECTED;
}

/*
 * 통신/타이머의 잔존 request와 IRQ 원인을 끈다. HAL_DeInit처럼 모든 AHB를
 * reset하지 않는다. GPIO, SYSCFG의 메모리 remap, PWR, RTC, WWDG와 FMC는 보존한다.
 * 전원 GPIO 출력은 그대로 유지되지만 timer/SPI 등의 AF 출력은 이제 구동하지 않는다.
 * 별도 BSP 장치 드라이버가 사용하기 전에 각각 다시 초기화해야 한다.
 */
static void BootResetPeripherals(void)
{
    const uint32_t apb1 = RCC_APB1RSTR_TIM2RST | RCC_APB1RSTR_TIM3RST
        | RCC_APB1RSTR_TIM4RST | RCC_APB1RSTR_TIM5RST | RCC_APB1RSTR_TIM6RST
        | RCC_APB1RSTR_TIM7RST | RCC_APB1RSTR_TIM12RST | RCC_APB1RSTR_TIM13RST
        | RCC_APB1RSTR_TIM14RST | RCC_APB1RSTR_SPI2RST | RCC_APB1RSTR_SPI3RST
        | RCC_APB1RSTR_USART2RST | RCC_APB1RSTR_USART3RST | RCC_APB1RSTR_UART4RST
        | RCC_APB1RSTR_UART5RST | RCC_APB1RSTR_UART7RST | RCC_APB1RSTR_UART8RST
        | RCC_APB1RSTR_I2C1RST | RCC_APB1RSTR_I2C2RST | RCC_APB1RSTR_I2C3RST;
    const uint32_t apb2 = RCC_APB2RSTR_TIM1RST | RCC_APB2RSTR_TIM8RST
        | RCC_APB2RSTR_TIM9RST | RCC_APB2RSTR_TIM10RST | RCC_APB2RSTR_TIM11RST
        | RCC_APB2RSTR_USART1RST | RCC_APB2RSTR_USART6RST | RCC_APB2RSTR_SDIORST
        | RCC_APB2RSTR_SPI1RST | RCC_APB2RSTR_SPI4RST | RCC_APB2RSTR_SPI5RST
        | RCC_APB2RSTR_SPI6RST | RCC_APB2RSTR_SAI1RST | RCC_APB2RSTR_LTDCRST;
    RCC->APB1RSTR |= apb1;
    RCC->APB2RSTR |= apb2;
    RCC->AHB1RSTR |= RCC_AHB1RSTR_OTGHRST | RCC_AHB1RSTR_DMA2DRST;
    RCC->AHB2RSTR |= RCC_AHB2RSTR_OTGFSRST;
    __DSB();
    RCC->APB1RSTR &= ~apb1;
    RCC->APB2RSTR &= ~apb2;
    RCC->AHB1RSTR &= ~(RCC_AHB1RSTR_OTGHRST | RCC_AHB1RSTR_DMA2DRST);
    RCC->AHB2RSTR &= ~RCC_AHB2RSTR_OTGFSRST;
    __DSB();
}

/*
 * C 런타임 이전의 인계 전용 진입점이다. initialized global, 문자열 처리, HAL,
 * dynamic allocation을 사용하지 않는다. 기록 초기화도 volatile word 쓰기이므로
 * compiler가 memset 호출로 바꾸지 않는다. 소유권을 넘겨받은 뒤 반환하면
 * assembly가 .data/.bss 초기화 및 main 진입을 이어가며, 그때까지 IRQ는 막혀 있다.
 */
void BSP_BootEarly(uint32_t entry_control, uint32_t entry_basepri,
                   uint32_t entry_ipsr, uint32_t entry_vtor)
{
    uint32_t index;
    uint32_t entry_mpu = MPU->CTRL;
    volatile uint32_t *diagnostic = (volatile uint32_t *)&g_bsp_boot;
    __disable_irq();
    MPU->CTRL = 0U;
    __DSB();
    __ISB();
    for (index = 0U; index < sizeof(g_bsp_boot) / sizeof(uint32_t); index++) {
        diagnostic[index] = 0U;
    }
    g_bsp_boot.magic = BSP_BOOT_DIAGNOSTIC_MAGIC;
    g_bsp_boot.version = BSP_BOOT_DIAGNOSTIC_VERSION;
    g_bsp_boot.status = BSP_BOOT_ENTERED;
    g_bsp_boot.entry_control = entry_control;
    g_bsp_boot.entry_basepri = entry_basepri;
    g_bsp_boot.entry_ipsr = entry_ipsr;
    g_bsp_boot.entry_vtor = entry_vtor;
    g_bsp_boot.entry_rcc_cr = RCC->CR;
    g_bsp_boot.entry_rcc_pllcfgr = RCC->PLLCFGR;
    g_bsp_boot.entry_rcc_cfgr = RCC->CFGR;
    g_bsp_boot.entry_rcc_csr = RCC->CSR;
    g_bsp_boot.entry_ahb1enr = RCC->AHB1ENR;
    g_bsp_boot.entry_apb1enr = RCC->APB1ENR;
    g_bsp_boot.entry_apb2enr = RCC->APB2ENR;
    g_bsp_boot.entry_flash_acr = FLASH->ACR;
    g_bsp_boot.entry_gpiod_moder = GPIOD->MODER;
    g_bsp_boot.entry_gpiod_odr = GPIOD->ODR;
    g_bsp_boot.entry_gpiog_moder = GPIOG->MODER;
    g_bsp_boot.entry_gpiog_odr = GPIOG->ODR;
    g_bsp_boot.entry_gpiog_idr = GPIOG->IDR;
    g_bsp_boot.entry_gpioi_moder = GPIOI->MODER;
    g_bsp_boot.entry_gpioi_odr = GPIOI->ODR;
    for (index = 0U; index < 3U; index++) {
        g_bsp_boot.entry_nvic_iser[index] = NVIC->ISER[index];
        g_bsp_boot.entry_nvic_ispr[index] = NVIC->ISPR[index];
    }
    g_bsp_boot.entry_systick_ctrl = SysTick->CTRL;
    g_bsp_boot.entry_icsr = SCB->ICSR;
    g_bsp_boot.entry_shcsr = SCB->SHCSR;
    g_bsp_boot.entry_cfsr = SCB->CFSR;
    g_bsp_boot.entry_hfsr = SCB->HFSR;
    g_bsp_boot.mpu_ctrl_before_c_cleanup = entry_mpu;
    g_bsp_boot.entry_scr = SCB->SCR;
    if ((entry_ipsr != 0U) || (SCB->VTOR != BSP_BOOT_APP_BASE)
        || ((__get_CONTROL() & 3U) != 0U)) {
        BootFail(BSP_BOOT_FAIL_ENTRY, entry_ipsr);
    }
    BootQuietCpu();
    BootStopDma();
    BootUseHsi();
    BootResetPeripherals();
    /* reset 중 새로 생긴 pending도 지운다. 아직 어떠한 IRQ도 다시 켜지 않는다. */
    for (index = 0U; index < 3U; index++) {
        NVIC->ICPR[index] = 0xFFFFFFFFUL;
    }
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
    g_bsp_boot.after_rcc_cr = RCC->CR;
    g_bsp_boot.after_rcc_pllcfgr = RCC->PLLCFGR;
    g_bsp_boot.after_rcc_cfgr = RCC->CFGR;
    g_bsp_boot.after_flash_acr = FLASH->ACR;
    g_bsp_boot.after_ahb1enr = RCC->AHB1ENR;
    g_bsp_boot.after_gpiod_odr = GPIOD->ODR;
    g_bsp_boot.after_gpiog_odr = GPIOG->ODR;
    g_bsp_boot.after_gpioi_odr = GPIOI->ODR;
    g_bsp_boot.status = BSP_BOOT_EARLY_READY;
    __DSB();
    __ISB();
}

/*
 * main의 첫 USER CODE에서 호출한다. C 전역 초기화가 끝난 뒤에만 SystemCoreClock을
 * 쓴다. 아직 HAL_Init 전이므로 tick/RTOS는 시작하지 않았어야 한다. 예상한 HSI /1
 * 상태와 초기 BSP 기록이 일치하면 IRQ를 풀어 뒤의 HAL TIM6 timebase가 동작하게 한다.
 * 예상과 다르면 계속 진행하지 않고 동일 진단 루프에 머문다.
 */
void BSP_BootRuntimeReady(void)
{
    const uint32_t clock_mask = RCC_CFGR_SWS | RCC_CFGR_HPRE
                             | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2;
    g_bsp_boot.runtime_vtor = SCB->VTOR;
    g_bsp_boot.runtime_primask = __get_PRIMASK();
    if ((g_bsp_boot.magic != BSP_BOOT_DIAGNOSTIC_MAGIC)
        || (g_bsp_boot.status != BSP_BOOT_EARLY_READY)
        || (SCB->VTOR != BSP_BOOT_APP_BASE) || (__get_IPSR() != 0U)
        || ((__get_CONTROL() & 3U) != 0U) || (__get_BASEPRI() != 0U)
        || (__get_FAULTMASK() != 0U) || (__get_PRIMASK() != 1U)
        || ((RCC->CFGR & clock_mask) != RCC_CFGR_SWS_HSI)
        || ((RCC->CR & RCC_CR_HSIRDY) == 0U)
        || ((RCC->CR & BOOT_PLL_READY) != 0U)) {
        BootFail(BSP_BOOT_FAIL_RUNTIME, RCC->CFGR);
    }
    SystemCoreClock = 16000000UL;
    g_bsp_boot.runtime_system_core_clock = SystemCoreClock;
    g_bsp_boot.status = BSP_BOOT_RUNTIME_READY;
    __DSB();
    __ISB();
    __enable_irq();
}
