#ifndef UPDATE_METADATA_H
#define UPDATE_METADATA_H

#include <stdint.h>
#include "Recovery_Target.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UPDATE_METADATA_ADDRESS       0x08008000UL
#define UPDATE_METADATA_SECTOR_SIZE   0x00004000UL
#define UPDATE_METADATA_WORD_COUNT    5U
#define UPDATE_METADATA_RESIDENT      0x000E0000UL
#define UPDATE_METADATA_APP_BLOCK     0x00007F90UL
#define UPDATE_METADATA_APP_LENGTH    0x00070000UL
/* 의도적인 최종 commit 호출임을 표시한다. 인증 비밀이나 이미지 검증 대체가 아니다. */
#define UPDATE_METADATA_ARM_TOKEN     0x554D4332UL

typedef enum {
    UPDATE_METADATA_OK = 0,
    UPDATE_METADATA_ARGUMENT,
    UPDATE_METADATA_NOT_ARMED,
    UPDATE_METADATA_CONTEXT,
    UPDATE_METADATA_BUSY,
    UPDATE_METADATA_RESIDENT_MISMATCH,
    UPDATE_METADATA_PENDING,
    UPDATE_METADATA_HARDWARE,
    UPDATE_METADATA_BACKUP_MISMATCH,
    /* erase를 시작한 뒤의 모든 실패: pending 또는 sector 손상을 배제할 수 없다. */
    UPDATE_METADATA_AMBIGUOUS,
    /* 이 부팅에서 이미 erase를 시도했다. 재시도 지우기는 제공하지 않는다. */
    UPDATE_METADATA_REVIEW_REQUIRED
} UpdateMetadataResult;

typedef enum {
    UPDATE_METADATA_STAGE_IDLE = 0,
    UPDATE_METADATA_STAGE_BACKUP,
    UPDATE_METADATA_STAGE_UNLOCK,
    UPDATE_METADATA_STAGE_ERASE,
    UPDATE_METADATA_STAGE_RESTORE,
    UPDATE_METADATA_STAGE_PRECOMMIT_VERIFY,
    UPDATE_METADATA_STAGE_CRC,
    UPDATE_METADATA_STAGE_FINAL_VERIFY,
    UPDATE_METADATA_STAGE_LOCK,
    UPDATE_METADATA_STAGE_COMPLETE
} UpdateMetadataStage;

typedef struct {
    uint32_t result;
    uint32_t stage;
    uint32_t destructive_started;
    uint32_t crc_write_started;
    uint32_t words_programmed;
    uint32_t failed_address;
    uint32_t flash_status;
    uint32_t flash_locked;
    uint32_t poll_exhausted;
    uint32_t preserved_crc_before;
    uint32_t preserved_crc_after;
} UpdateMetadataDiagnostics;

/* 정상 privileged Thread에서 5개 little-endian 워드를 읽는다. 쓰기/정상화 없음.
 * NULL/ISR/동시 commit/FLASH busy이면 실패한다. CRC=0이면 slot/length가 남아
 * 있어도 pending이 아니다. FFFFFFFF를 0으로 바꾸어 반환하지 않는다. */
UpdateMetadataResult UpdateMetadata_Read(uint32_t words[UPDATE_METADATA_WORD_COUNT]);

/* 최종 단계에서만 호출한다. caller가 448KiB staging 전체를 읽어 이미지/CRC/
 * 벡터/길이를 검증하고 staging의 다른 writer를 배제해야 한다. 이 함수는 NOR를
 * 검사하지 않는다. version!=FFFFFFFF, crc!=0/FFFFFFFF, token 일치를 요구한다.
 * 현 resident=000E0000/CRC=0일 때만 sector2를 갱신한다. scratch는 검증된 RAM의
 * 4-byte 정렬/최소16KiB 독점 버퍼다. caller가 SDRAM arena 등에서 미리 확보하고
 * 호출 중 DMA/다른 task가 접근하지 못하게 한다. 내부 정적16KiB는 사용하지 않는다.
 * 성공은 메타 readback 검증 완료이며 설치/재부팅 완료가 아니다. reset 없음.
 * AMBIGUOUS이면 재시도/재부팅을 자동으로 수행하지 말고 SWD로 상태를 확인한다.
 * x32 program에 필요한 안정적인 MCU VDD 2.7~3.6V는 caller의 하드웨어 전제다. */
UpdateMetadataResult UpdateMetadata_Commit(uint32_t version, uint32_t crc,
                                            uint32_t explicit_arm_token,
                                            void *scratch, uint32_t scratch_bytes);

/* Thread에서 직전 commit의 단계/읽기 검증 결과를 복사한다. NULL은 무시한다.
 * 함수 호출 중에는 IRQ를 막으므로 task 간 snapshot 복사는 찢어지지 않는다.
 * 하드웨어 BSY 고착은 RAM polling 종료 뒤 FLASH caller 복귀도 막을 수 있다. */
void UpdateMetadata_GetDiagnostics(UpdateMetadataDiagnostics *out);

#ifdef UPDATE_METADATA_TESTING
/* 실제 엔진 C를 시험용 flash 모델에 연결하는 경계다. 제품 빌드에는 없다.
 * Enter/Leave는 privileged Thread+IRQ 배타성을, Begin/End는 FLASH lock을 모사.
 * Erase는 sector2만, Program은 sector2 내 정렬 word만 허용해야 한다. */
UpdateMetadataResult UpdateMetadataTest_Enter(void);
void UpdateMetadataTest_Leave(void);
uint32_t UpdateMetadataTest_ReadWord(uint32_t address);
uint32_t UpdateMetadataTest_Readable(void);
uint32_t UpdateMetadataTest_Begin(void);
uint32_t UpdateMetadataTest_Erase(void);
uint32_t UpdateMetadataTest_Program(uint32_t address, uint32_t value);
uint32_t UpdateMetadataTest_End(void);
/* 각 독립 시험에서 전원 초기화에 해당하는 정적 상태만 지운다. 제품에는 없다. */
void UpdateMetadataTest_Reset(void);
#endif

#ifdef __cplusplus
}
#endif
#endif
