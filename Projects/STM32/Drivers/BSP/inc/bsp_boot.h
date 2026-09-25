#ifndef BSP_BOOT_H
#define BSP_BOOT_H

#include <stdint.h>

/* Product is entered by the erase-isolated S4 RecoveryGate. Legacy bring-up
 * profiles and Bootstrap retain the stock BL entry. Match the selected linker. */
#if NOODOE_PRODUCT || NOODOE_DIAGNOSTIC
#define BSP_BOOT_APP_BASE             0x08020000UL
#else
#define BSP_BOOT_APP_BASE             0x08010000UL
#endif
#define BSP_BOOT_DIAGNOSTIC_MAGIC     0x424F4F54UL
#define BSP_BOOT_DIAGNOSTIC_VERSION   1UL

/* 성공 단계는 작은 값, 복귀하지 않는 실패는 최상위 비트가 켜진 값으로 구분한다. */
#define BSP_BOOT_ENTERED              0x01UL
#define BSP_BOOT_DMA_QUIET            0x02UL
#define BSP_BOOT_HSI_SELECTED         0x03UL
#define BSP_BOOT_EARLY_READY          0x10UL
#define BSP_BOOT_RUNTIME_READY        0x20UL
#define BSP_BOOT_FAIL_ENTRY           0x80000001UL
#define BSP_BOOT_FAIL_DMA1            0x80000002UL
#define BSP_BOOT_FAIL_DMA2            0x80000003UL
#define BSP_BOOT_FAIL_HSI             0x80000004UL
#define BSP_BOOT_FAIL_SWITCH          0x80000005UL
#define BSP_BOOT_FAIL_PLL             0x80000006UL
#define BSP_BOOT_FAIL_FLASH           0x80000007UL
#define BSP_BOOT_FAIL_RUNTIME         0x80000008UL

/*
 * 디버거 관측용 기록이다. .data 복사/.bss 삭제에서 제외되는 NOLOAD 섹션이 필요하다.
 * 부팅마다 필드를 다시 쓰므로 이전 부팅의 기록을 영구 보관하는 구조는 아니다.
 * entry_*는 초기 인계 당시 관측값이며, GPIO clock이 꺼졌다면 해당 포트 값은
 * 유효한 핀 상태라고 해석하지 않는다. after_*는 초기 BSP 처리가 끝난 값이다.
 */
typedef struct {
    uint32_t magic, version, status, error_detail;
    uint32_t entry_control, entry_basepri, entry_ipsr, entry_vtor;
    uint32_t entry_rcc_cr, entry_rcc_pllcfgr, entry_rcc_cfgr, entry_rcc_csr;
    uint32_t entry_ahb1enr, entry_apb1enr, entry_apb2enr, entry_flash_acr;
    uint32_t entry_gpiod_moder, entry_gpiod_odr;
    uint32_t entry_gpiog_moder, entry_gpiog_odr, entry_gpiog_idr;
    uint32_t entry_gpioi_moder, entry_gpioi_odr;
    uint32_t entry_dma1_en_mask, entry_dma2_en_mask;
    uint32_t entry_nvic_iser[3], entry_nvic_ispr[3];
    uint32_t entry_systick_ctrl, entry_icsr, entry_shcsr;
    uint32_t entry_cfsr, entry_hfsr, mpu_ctrl_before_c_cleanup, entry_scr;
    uint32_t after_rcc_cr, after_rcc_pllcfgr, after_rcc_cfgr, after_flash_acr;
    uint32_t after_ahb1enr, after_gpiod_odr, after_gpiog_odr, after_gpioi_odr;
    uint32_t wait_address, wait_value, wait_mask, wait_expected;
    uint32_t wait_remaining, failure_spins;
    uint32_t runtime_vtor, runtime_primask, runtime_system_core_clock;
} BSP_BootDiagnostics;

extern volatile BSP_BootDiagnostics g_bsp_boot;

/*
 * 강한 Reset_Handler가 C 런타임보다 먼저 호출한다. 호출 전 PRIMASK=1,
 * CONTROL=0, BASEPRI=FAULTMASK=0, 유효 MSP, 자체 VTOR 설정이 필요하다.
 * 전달 인자는 위 레지스터들을 변경하기 전에 assembly에서 확보한 원래 값이다.
 */
void BSP_BootEarly(uint32_t entry_control, uint32_t entry_basepri,
                   uint32_t entry_ipsr, uint32_t entry_vtor);

/* .data/.bss 초기화 후 main의 첫 USER CODE에서 호출한다. 성공할 때만 IRQ를 푼다. */
void BSP_BootRuntimeReady(void);

#endif
