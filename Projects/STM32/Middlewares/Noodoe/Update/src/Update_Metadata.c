#include "Update_Metadata.h"

#define META_WORDS (UPDATE_METADATA_SECTOR_SIZE / 4U)
#define META_ERASE_POLL_LIMIT 20000000UL
#define META_PROGRAM_POLL_LIMIT 2000000UL

/* linker의 .data에 포함되어 startup이 SRAM으로 복사한다. ARM flash busy 중
 * 이 경로가 호출하는 함수/상수는 모두 SRAM 또는 peripheral에 있어야 한다. */
#define META_RAM __attribute__((section(".RamFunc.update_metadata"), noinline, noclone))
/* caller의 검증된 SDRAM/SRAM scratch를 빌린다. 이 포인터만 내부 .bss를 차지한다. */
static uint32_t *g_sector;
static uint32_t g_busy;
static uint32_t g_attempted;
/* 영구 busy로 caller가 돌아오지 못해도 SWD에서 마지막 단계가 보이도록 volatile. */
static volatile UpdateMetadataDiagnostics g_diag;

#ifdef UPDATE_METADATA_TESTING
#define MetaEnter UpdateMetadataTest_Enter
#define MetaLeave UpdateMetadataTest_Leave
#define MetaReadWord UpdateMetadataTest_ReadWord
#define MetaReadable UpdateMetadataTest_Readable
#define MetaBegin UpdateMetadataTest_Begin
#define MetaErase UpdateMetadataTest_Erase
#define MetaProgram UpdateMetadataTest_Program
#define MetaEnd UpdateMetadataTest_End
#else
#include "stm32f4xx.h"
#include "BSP_Watchdog.h"
#if NOODOE_RECOVERY_GATE
extern uint32_t GateBoard_Millis(void);
#define MetaNow GateBoard_Millis
#elif NOODOE_BOOTSTRAP
extern uint32_t Bootstrap_MetadataMillis(void);
#define MetaNow Bootstrap_MetadataMillis
#else
extern uint32_t HAL_GetTick(void);
#define MetaNow HAL_GetTick
#endif
#define META_ERRORS (FLASH_SR_OPERR | FLASH_SR_WRPERR | FLASH_SR_PGAERR \
                   | FLASH_SR_PGPERR | FLASH_SR_PGSERR | FLASH_SR_RDERR)
#define META_MODES (FLASH_CR_PG | FLASH_CR_SER | FLASH_CR_MER1 | FLASH_CR_MER2 \
                  | FLASH_CR_SNB | FLASH_CR_PSIZE | FLASH_CR_STRT)

/* ISR/비특권/이미 IRQ가 막힌 호출을 거절하고 task 전환을 막는다. 다른 flash
 * writer와 공유 mutex 역할은 하지 않으므로 애플리케이션도 writer를 단일화한다. */
static UpdateMetadataResult MetaEnter(void)
{
    if ((__get_IPSR() != 0U) || ((__get_CONTROL() & 1U) != 0U)
        || (__get_PRIMASK() != 0U) || (__get_BASEPRI() != 0U)
        || (__get_FAULTMASK() != 0U)) {
        return UPDATE_METADATA_CONTEXT;
    }
    __disable_irq();
    __DSB();
    __ISB();
    return UPDATE_METADATA_OK;
}

/* MetaEnter 성공 시만 호출한다. entry가 IRQ enabled임을 확인했으므로 원상복구.
 * Flash busy가 비정상적으로 남으면 caller의 flash fetch가 stall할 수 있다. */
static META_RAM void MetaLeave(void)
{
    __DSB();
    __ISB();
    __enable_irq();
}

/* 내부 S2 정렬 word 읽기다. 엔진은 readable 확인 후 알려진 주소만 전달한다. */
static META_RAM uint32_t MetaReadWord(uint32_t address)
{
    return *(volatile const uint32_t *)(uintptr_t)address;
}

/* busy 중 flash array를 읽지 않기 위한 검사다. HAL tick/시간 상태는 사용 안 함. */
static META_RAM uint32_t MetaReadable(void)
{
    g_diag.flash_status = FLASH->SR;
    return (FLASH->SR & FLASH_SR_BSY) == 0U;
}

/* 유한 peripheral poll 횟수와 별도의5초 DWT lease를 함께 적용한다. IRQ tick은
 * 필요 없으며 정상 lease 안에서만 RAM watchdog checkpoint를 허용한다. */
static META_RAM uint32_t MetaWait(uint32_t remaining)
{
    do {
        uint32_t status = FLASH->SR;
        g_diag.flash_status = status;
        if ((status & FLASH_SR_BSY) == 0U) {
            return (status & META_ERRORS) == 0U;
        }
        if ((remaining & 0x3FFU) == 0U) {
            if(!BSP_Watchdog_RamCheckpoint())return 0U;
        }
    } while (--remaining != 0U);
    g_diag.poll_exhausted = 1U;
    return 0U;
}

/* S2가 읽기 cache에 남아 검증을 속이지 않도록 D-cache를 비우고 원래 enable을
 * 복원한다. BSY가 내려간 뒤만 호출한다. latency/prefetch/I-cache 설정은 보존. */
static META_RAM void MetaFlushDataCache(void)
{
    uint32_t old = FLASH->ACR;
    FLASH->ACR = old & ~FLASH_ACR_DCEN;
    FLASH->ACR = (old & ~FLASH_ACR_DCEN) | FLASH_ACR_DCRST;
    FLASH->ACR = old & ~FLASH_ACR_DCRST;
    __DSB();
    __ISB();
}

/* 실제 0x419/512KiB 장치만 허용한다. WWDG active이면 reload 주기 보장을 못
 * 하므로 거절한다. 이미 unlocked/busy인 타 writer를 빼앗지 않는다. 옵션 금지. */
static META_RAM uint32_t MetaBegin(void)
{
    if (!MetaReadable() || ((DBGMCU->IDCODE & 0xFFFU) != 0x419U)
        || (*(volatile const uint16_t *)0x1FFF7A22UL != 512U)
        || ((FLASH->CR & FLASH_CR_LOCK) == 0U)
        || ((FLASH->CR & ((META_MODES & ~FLASH_CR_PSIZE)
                         | FLASH_CR_EOPIE | FLASH_CR_ERRIE)) != 0U)
        || (((RCC->APB1ENR & RCC_APB1ENR_WWDGEN) != 0U)
            && ((WWDG->CR & WWDG_CR_WDGA) != 0U))) {
        return 0U;
    }
    FLASH->KEYR = 0x45670123UL;
    FLASH->KEYR = 0xCDEF89ABUL;
    if ((FLASH->CR & FLASH_CR_LOCK) != 0U) {
        return 0U;
    }
    FLASH->SR = META_ERRORS | FLASH_SR_EOP;
    return 1U;
}

/* 파라미터가 없어 다른 sector를 선택할 수 없다. MER는 항상 clear하고 sector
 * 번호 2/32-bit parallelism만 사용한다. VDD 2.7~3.6V 유지가 물리 전제다. */
static META_RAM uint32_t MetaErase(void)
{
    uint32_t ok;
    if(!BSP_Watchdog_RamCheckpoint())return 0U;
    FLASH->CR = (FLASH->CR & ~META_MODES) | FLASH_CR_PSIZE_1
              | FLASH_CR_SER | (2UL << FLASH_CR_SNB_Pos);
    FLASH->CR |= FLASH_CR_STRT;
    ok = MetaWait(META_ERASE_POLL_LIMIT);
    if (MetaReadable()) {
        FLASH->CR &= ~META_MODES;
        MetaFlushDataCache();
    }
    return ok;
}

/* S2 내 4-byte aligned store 한 번만 실행한다. CRC도 이 원시 연산 한 번으로
 * 마지막에 기록한다. 실패 후 재쓰기/다른 sector erase를 시도하지 않는다. */
static META_RAM uint32_t MetaProgram(uint32_t address, uint32_t value)
{
    uint32_t ok;
    if ((address < UPDATE_METADATA_ADDRESS)
        || (address >= UPDATE_METADATA_ADDRESS + UPDATE_METADATA_SECTOR_SIZE)
        || ((address & 3U) != 0U) || !MetaReadable()) {
        return 0U;
    }
    FLASH->SR = META_ERRORS | FLASH_SR_EOP;
    FLASH->CR = (FLASH->CR & ~META_MODES) | FLASH_CR_PSIZE_1 | FLASH_CR_PG;
    *(volatile uint32_t *)(uintptr_t)address = value;
    __DSB();
    ok = MetaWait(META_PROGRAM_POLL_LIMIT);
    if (MetaReadable()) {
        FLASH->CR &= ~META_MODES;
        MetaFlushDataCache();
    }
    if(!BSP_Watchdog_RamCheckpoint())return 0U;
    return ok;
}

/* unlock 성공 이후 모든 경로에서 실행한다. busy 고착이면 CR write 자체가
 * stall될 수 있으므로 접근을 피하고 lock 실패를 보고한다. 정상/오류 완료처럼
 * BSY=0이면 mode clear 후 반드시 LOCK을 설정하고 실제 readback으로 확인한다. */
static META_RAM uint32_t MetaEnd(void)
{
    if (!MetaReadable()) {
        g_diag.flash_locked = (FLASH->CR & FLASH_CR_LOCK) != 0U;
        return 0U;
    }
    FLASH->CR = (FLASH->CR & ~META_MODES) | FLASH_CR_LOCK;
    __DSB();
    g_diag.flash_locked = (FLASH->CR & FLASH_CR_LOCK) != 0U;
    return g_diag.flash_locked;
}
#endif

/* resident 밖 보존 영역의 CRC32(IEEE, reflected)를 계산한다. staging 이미지의
 * 전송 CRC와는 별개의 RAM/readback 비교값이며 metadata에 저장하지 않는다.
 * from_flash=0은 원본 RAM, 1은 현재 flash를 읽고 길이는 항상 S2 끝까지 고정. */
static META_RAM uint32_t MetaPreservedCrc(uint32_t from_flash)
{
    uint32_t crc = 0xFFFFFFFFUL;
    uint32_t i;
    for (i = UPDATE_METADATA_WORD_COUNT; i < META_WORDS; ++i) {
#ifndef UPDATE_METADATA_TESTING
        /* 전체16KiB 검증도 같은 절대 시간 lease 안의 RAM checkpoint만 사용한다. */
        if ((i & 0xFFU) == 0U) {
            if(!BSP_Watchdog_RamCheckpoint())return 0U;
        }
#endif
        uint32_t value = from_flash ? MetaReadWord(UPDATE_METADATA_ADDRESS + 4U*i)
                                   : g_sector[i];
        uint32_t byte;
        for (byte = 0U; byte < 4U; ++byte) {
            uint32_t bit;
            crc ^= value & 0xFFU;
            value >>= 8U;
            for (bit = 0U; bit < 8U; ++bit) {
                crc = (crc >> 1U) ^ ((0U - (crc & 1U)) & 0xEDB88320UL);
            }
        }
    }
    return crc ^ 0xFFFFFFFFUL;
}

/* sector 전체를 RAM 예상값과 byte-equivalent word 비교한다. CRC word는 호출
 * 시점에 따라 FFFFFFFF 또는 최종 CRC다. 첫 불일치 주소와 보존 CRC를 남긴다. */
static META_RAM uint32_t MetaVerify(void)
{
    uint32_t i;
    if (!MetaReadable()) {
        return 0U;
    }
    /* word 비교 실패 때에도 이전 단계의 CRC가 남지 않도록 먼저 갱신한다. */
    g_diag.preserved_crc_after = MetaPreservedCrc(1U);
    for (i = 0U; i < META_WORDS; ++i) {
        uint32_t address = UPDATE_METADATA_ADDRESS + 4U*i;
        if (MetaReadWord(address) != g_sector[i]) {
            g_diag.failed_address = address;
            return 0U;
        }
    }
    return g_diag.preserved_crc_before == g_diag.preserved_crc_after;
}

/* RAM 백업→두 번째 read 검증→S2 erase→CRC 제외 복원→전체 비교→CRC 마지막
 * store→전체 비교 순서다. IRQ가 막힌 상태에서 실행하며 라이브러리/HAL/heap/
 * RTOS를 호출하지 않는다. erase 이후 모든 실패는 AMBIGUOUS로 남긴다. */
static META_RAM UpdateMetadataResult MetaTransaction(uint32_t version, uint32_t crc)
{
    uint32_t i;
    UpdateMetadataResult result = UPDATE_METADATA_HARDWARE;
    g_diag.stage = UPDATE_METADATA_STAGE_BACKUP;
    if (!MetaReadable()) {
        return result;
    }
    for (i = 0U; i < META_WORDS; ++i) {
        g_sector[i] = MetaReadWord(UPDATE_METADATA_ADDRESS + 4U*i);
    }
    g_diag.preserved_crc_before = MetaPreservedCrc(0U);
    if (!MetaVerify()) {
        return UPDATE_METADATA_BACKUP_MISMATCH;
    }
    if (!RECOVERY_VERSION_SUPPORTED(g_sector[0])) {
        return UPDATE_METADATA_RESIDENT_MISMATCH;
    }
    if (g_sector[4] != 0U) {
        return UPDATE_METADATA_PENDING;
    }
    g_diag.stage = UPDATE_METADATA_STAGE_UNLOCK;
    if (!MetaBegin()) {
        return result;
    }
    /* word0 is the target boot version, preserved from the verified shadow. */
    g_sector[1] = version;
    g_sector[2] = UPDATE_METADATA_APP_BLOCK;
    g_sector[3] = UPDATE_METADATA_APP_LENGTH;
    g_sector[4] = 0xFFFFFFFFUL;
    g_attempted = 1U;
    g_diag.destructive_started = 1U;
    g_diag.stage = UPDATE_METADATA_STAGE_ERASE;
    result = UPDATE_METADATA_AMBIGUOUS;
    if (!MetaErase() || !MetaReadable()) {
        goto lock;
    }
    /* erase 실패/부분 지우기를 다음 program으로 숨기지 않는다. */
    for (i = 0U; i < META_WORDS; ++i) {
        uint32_t address = UPDATE_METADATA_ADDRESS + 4U*i;
        if (MetaReadWord(address) != 0xFFFFFFFFUL) {
            g_diag.failed_address = address;
            goto lock;
        }
    }
    g_diag.stage = UPDATE_METADATA_STAGE_RESTORE;
    for (i = 0U; i < META_WORDS; ++i) {
        if ((i != 4U) && (g_sector[i] != 0xFFFFFFFFUL)) {
            uint32_t address = UPDATE_METADATA_ADDRESS + 4U*i;
            if (!MetaProgram(address, g_sector[i])) {
                g_diag.failed_address = address;
                goto lock;
            }
            ++g_diag.words_programmed;
        }
    }
    g_diag.stage = UPDATE_METADATA_STAGE_PRECOMMIT_VERIFY;
    if (!MetaVerify()) {
        goto lock;
    }
    g_diag.stage = UPDATE_METADATA_STAGE_CRC;
    g_diag.crc_write_started = 1U;
    if (!MetaProgram(UPDATE_METADATA_ADDRESS + 16U, crc)) {
        g_diag.failed_address = UPDATE_METADATA_ADDRESS + 16U;
        goto lock;
    }
    ++g_diag.words_programmed;
    g_sector[4] = crc;
    g_diag.stage = UPDATE_METADATA_STAGE_FINAL_VERIFY;
    if (MetaVerify()) {
        result = UPDATE_METADATA_OK;
    }
lock:
    /* 실패 stage를 보존한다. 성공 시에만 최종 lock 단계로 진전시킨다. */
    if (result == UPDATE_METADATA_OK) {
        g_diag.stage = UPDATE_METADATA_STAGE_LOCK;
    }
    g_diag.flash_locked = MetaEnd();
    if (!g_diag.flash_locked) {
        result = UPDATE_METADATA_AMBIGUOUS;
    }
    if (result == UPDATE_METADATA_OK) {
        g_diag.stage = UPDATE_METADATA_STAGE_COMPLETE;
    }
    return result;
}

/* 공개 read는 레코드를 정규화하지 않는다. 짧은 IRQ 배타 구간으로 commit과의
 * 동시 읽기를 배제하며, 받은 raw 5 words의 의미 판정은 caller가 수행한다. */
UpdateMetadataResult UpdateMetadata_Read(uint32_t words[UPDATE_METADATA_WORD_COUNT])
{
    UpdateMetadataResult result;
    uint32_t i;
    if (words == 0) {
        return UPDATE_METADATA_ARGUMENT;
    }
    result = MetaEnter();
    if (result != UPDATE_METADATA_OK) {
        return result;
    }
    if (g_busy) {
        result = UPDATE_METADATA_BUSY;
    } else if (!MetaReadable()) {
        result = UPDATE_METADATA_HARDWARE;
    } else {
        for (i = 0U; i < UPDATE_METADATA_WORD_COUNT; ++i) {
            words[i] = MetaReadWord(UPDATE_METADATA_ADDRESS + 4U*i);
        }
    }
    MetaLeave();
    return result;
}

/* 최종 기록 승인/재진입 방지 API다. erase가 한 번이라도 시작되면 이 부팅의
 * 두 번째 시도는 금지한다. 메모리 초기화/검증 실패는 flash unlock 전에 끝난다. */
UpdateMetadataResult UpdateMetadata_Commit(uint32_t version, uint32_t crc,
                                            uint32_t explicit_arm_token,
                                            void *scratch, uint32_t scratch_bytes)
{
    UpdateMetadataResult result;
    volatile uint32_t *diag_words;
    uint32_t i;
    if (explicit_arm_token != UPDATE_METADATA_ARM_TOKEN) {
        return UPDATE_METADATA_NOT_ARMED;
    }
    if ((version == 0xFFFFFFFFUL) || (crc == 0U) || (crc == 0xFFFFFFFFUL)
        || (scratch == 0) || (((uintptr_t)scratch & 3U) != 0U)
        || (scratch_bytes < UPDATE_METADATA_SECTOR_SIZE)
        /* 범위 검사는 잘못된 flash/peripheral 포인터를 막는다. SDRAM 실제 용량/
         * readiness 검증은 BSP arena의 계약이며 이 주소창만으로 증명하지 않는다. */
        || !((((uintptr_t)scratch >= 0x20000000UL)
              && ((uintptr_t)scratch <= 0x20030000UL - UPDATE_METADATA_SECTOR_SIZE))
             || (((uintptr_t)scratch >= 0xC0000000UL)
                 && ((uintptr_t)scratch <= 0xC4000000UL - UPDATE_METADATA_SECTOR_SIZE)))) {
        return UPDATE_METADATA_ARGUMENT;
    }
    result = MetaEnter();
    if (result != UPDATE_METADATA_OK) {
        return result;
    }
    if (g_busy || g_attempted) {
        result = g_busy ? UPDATE_METADATA_BUSY : UPDATE_METADATA_REVIEW_REQUIRED;
        MetaLeave();
        return result;
    }
#ifndef UPDATE_METADATA_TESTING
    /* 별도 linker를 잘못 쓰면 flash에서 실행되는 critical 경로를 시작하지 않는다. */
    if (((uintptr_t)MetaTransaction < 0x20000000UL)
        || ((uintptr_t)MetaTransaction >= 0x20030000UL)) {
        MetaLeave();
        return UPDATE_METADATA_HARDWARE;
    }
    /* One nonrenewable5s hardware-clock lease covers the whole S2 transaction.
     * Repeated busy polls cannot prolong it or hide a stuck flash controller. */
    if(!BSP_Watchdog_BeginFlash(MetaNow(),SystemCoreClock)){
        MetaLeave();return UPDATE_METADATA_HARDWARE;
    }
#endif
    g_busy = 1U;
    g_sector = (uint32_t *)scratch;
    diag_words = (volatile uint32_t *)&g_diag;
    for (i = 0U; i < sizeof(g_diag)/sizeof(uint32_t); ++i) {
        diag_words[i] = 0U;
    }
    result = MetaTransaction(version, crc);
#ifndef UPDATE_METADATA_TESTING
    if(!BSP_Watchdog_EndFlash())result=UPDATE_METADATA_AMBIGUOUS;
#endif
    g_diag.result = result;
    g_sector = 0;
    g_busy = 0U;
    MetaLeave();
    return result;
}

/* 마지막 transaction 결과만 복사한다. 호출 거절은 진행 중인 진단을 덮어쓰지
 * 않는다. 구조체 word 복사로 libc를 요구하지 않으며 NULL/잘못된 context는 무시. */
void UpdateMetadata_GetDiagnostics(UpdateMetadataDiagnostics *out)
{
    uint32_t i;
    if ((out != 0) && (MetaEnter() == UPDATE_METADATA_OK)) {
        for (i = 0U; i < sizeof(g_diag)/sizeof(uint32_t); ++i) {
            ((uint32_t *)out)[i] = ((volatile const uint32_t *)&g_diag)[i];
        }
        MetaLeave();
    }
}

#ifdef UPDATE_METADATA_TESTING
/* 독립 전원 부팅을 시험하는 코드만 사용한다. 이 함수는 제품 이미지에 없다. */
void UpdateMetadataTest_Reset(void)
{
    uint32_t i;
    g_busy = 0U;
    g_attempted = 0U;
    for (i = 0U; i < sizeof(g_diag)/sizeof(uint32_t); ++i) {
        ((volatile uint32_t *)&g_diag)[i] = 0U;
    }
}
#endif
