#include "Update_Metadata.h"
static uint32_t expected_resident=0x000E0000U;
#include <stddef.h>

#define FLASH_BASE 0x08000000UL
#define WORDS (0x10000U/4U)
#define S2_WORD (0x8000U/4U)
#define SECTOR_WORDS (0x4000U/4U)
#define TEST_VERSION 0x00010002UL
#define TEST_CRC 0x12345678UL
#define CHECK(c) do { ++g_test_assertions; if (!(c)) { g_test_failure_line=__LINE__; return __LINE__; } } while (0)

volatile uint32_t g_test_suite, g_test_failure_line, g_test_assertions;
static uint32_t flash[WORDS], original[WORDS], scratch[SECTOR_WORDS];
static uint32_t entered, context_bad, readable, locked, erase_count, program_count;
static uint32_t lock_count, read_count, bad_access, last_program, test_fault, fail_index;
static uint32_t crc_before_preserve, precommit_reads, reentry_result, reentry;

enum { FAULT_NONE, FAULT_BEGIN, FAULT_ERASE, FAULT_PARTIAL_ERASE,
       FAULT_PROGRAM, FAULT_PREVERIFY, FAULT_FINALVERIFY, FAULT_BACKUP,
       FAULT_LOCK, FAULT_CRC, FAULT_CRC_PARTIAL };

/* 실제 엔진의 context 경계를 모사한다. test는 single-thread ARM 실행이며 이
 * 모델의 entered flag는 실제 NVIC/RTOS 배타성을 검증했다고 뜻하지 않는다. */
UpdateMetadataResult UpdateMetadataTest_Enter(void)
{
    if (context_bad) return UPDATE_METADATA_CONTEXT;
    ++entered;
    return UPDATE_METADATA_OK;
}
/* 모델 진입과 이탈의 균형을 검증할 수 있도록 감소시킨다. */
void UpdateMetadataTest_Leave(void) { if (!entered) ++bad_access; else --entered; }
/* 인가된 S2 read만 허용한다. 두 번째 읽기의 한 byte 오류를 선택적으로 주입. */
uint32_t UpdateMetadataTest_ReadWord(uint32_t address)
{
    uint32_t value;
    if (address<UPDATE_METADATA_ADDRESS || address>=UPDATE_METADATA_ADDRESS+UPDATE_METADATA_SECTOR_SIZE || (address&3U)) { ++bad_access; return 0U; }
    ++read_count;
    value=flash[(address-FLASH_BASE)/4U];
    if (test_fault==FAULT_BACKUP && read_count==SECTOR_WORDS+17U) return value^1U;
    if (program_count && last_program!=UPDATE_METADATA_ADDRESS+16U) ++precommit_reads;
    return value;
}
/* 하드웨어 BSY와는 달리 모델의 readable 상태는 test가 직접 결정한다. */
uint32_t UpdateMetadataTest_Readable(void) { return readable; }
/* unlock 실패에는 flash 내용 변화가 없음을 확인한다. */
uint32_t UpdateMetadataTest_Begin(void)
{
    if (!locked || test_fault==FAULT_BEGIN) return 0U;
    locked=0U;
    return 1U;
}
/* 실제 sector geometry와 같은 16KiB 범위만 지운다. 부분 지우기를 성공으로
 * 보고하는 결함도 엔진이 erase readback으로 잡는지 검사한다. */
uint32_t UpdateMetadataTest_Erase(void)
{
    uint32_t i;
    if (locked || !entered) { ++bad_access; return 0U; }
    ++erase_count;
    if (test_fault==FAULT_ERASE) { flash[S2_WORD+4U]=0x01010101U; return 0U; }
    for (i=0U;i<SECTOR_WORDS;++i) flash[S2_WORD+i]=0xFFFFFFFFUL;
    if (test_fault==FAULT_PARTIAL_ERASE) flash[S2_WORD+77U]=0U;
    return 1U;
}
/* flash bit는 1→0만 변한다. 최종 CRC 이전에 모든 보존 word가 원본인지,
 * CRC를 제외한 prefix가 완성되었는지 검사한다. 반환 실패와 partial CRC도 모사. */
uint32_t UpdateMetadataTest_Program(uint32_t address,uint32_t value)
{
    uint32_t i,index;
    if (address<UPDATE_METADATA_ADDRESS || address>=UPDATE_METADATA_ADDRESS+UPDATE_METADATA_SECTOR_SIZE || (address&3U) || locked || !entered) { ++bad_access; return 0U; }
    index=(address-FLASH_BASE)/4U;
    ++program_count;
    last_program=address;
    if (reentry) {
        reentry=0U;
        reentry_result=UpdateMetadata_Commit(TEST_VERSION,TEST_CRC,UPDATE_METADATA_ARM_TOKEN,scratch,sizeof(scratch));
    }
    if (test_fault==FAULT_PROGRAM && program_count==fail_index) return 0U;
    if (address==UPDATE_METADATA_ADDRESS+16U) {
        if (flash[S2_WORD]!=expected_resident || flash[S2_WORD+1U]!=TEST_VERSION
            || flash[S2_WORD+2U]!=UPDATE_METADATA_APP_BLOCK || flash[S2_WORD+3U]!=UPDATE_METADATA_APP_LENGTH
            || flash[S2_WORD+4U]!=0xFFFFFFFFUL || precommit_reads<SECTOR_WORDS) ++crc_before_preserve;
        for (i=5U;i<SECTOR_WORDS;++i) if(flash[S2_WORD+i]!=original[S2_WORD+i]) ++crc_before_preserve;
        if(test_fault==FAULT_CRC) return 0U;
        if(test_fault==FAULT_CRC_PARTIAL) { flash[index]&=value|0xFFFF0000UL; return 0U; }
    }
    flash[index]&=value;
    if(test_fault==FAULT_PREVERIFY && index==S2_WORD+9U) flash[index]^=1U;
    if(test_fault==FAULT_FINALVERIFY && address==UPDATE_METADATA_ADDRESS+16U) flash[S2_WORD+100U]^=1U;
    return 1U;
}
/* engine가 unlock 후 어떤 exit를 택하든 lock을 요청하는지 기록한다. */
uint32_t UpdateMetadataTest_End(void)
{
    ++lock_count;
    if(test_fault==FAULT_LOCK) return 0U;
    locked=1U;
    return 1U;
}

/* 각 test는 새 부팅과 서로 다른 S2 데이터로 시작한다. S0/S1/S3 sentinel과
 * 기록하면 안 되는 erased word도 포함해 원본 보존을 byte 단위로 확인한다. */
static void Setup(uint32_t fault)
{
    uint32_t i;
    UpdateMetadataTest_Reset();
    for(i=0U;i<WORDS;++i) flash[i]=0xA5000000UL ^ (i*0x10203UL);
    for(i=5U;i<SECTOR_WORDS;i+=3U) flash[S2_WORD+i]=0xFFFFFFFFUL;
    flash[S2_WORD]=expected_resident;
    flash[S2_WORD+1U]=0U; flash[S2_WORD+2U]=0U;
    flash[S2_WORD+3U]=0U; flash[S2_WORD+4U]=0U;
    for(i=0U;i<WORDS;++i) original[i]=flash[i];
    entered=0U; context_bad=0U; readable=1U; locked=1U;
    erase_count=0U; program_count=0U; lock_count=0U; read_count=0U; bad_access=0U;
    last_program=0U; test_fault=fault; fail_index=1U;
    crc_before_preserve=0U; precommit_reads=0U; reentry=0U; reentry_result=0U;
}

/* 준비된 full staging이 있다는 caller 계약을 가정한 최종 API 호출이다. */
static UpdateMetadataResult Commit(void)
{
    return UpdateMetadata_Commit(TEST_VERSION,TEST_CRC,UPDATE_METADATA_ARM_TOKEN,scratch,sizeof(scratch));
}

/* S2 외부가 한 word도 바뀌지 않았는지 검사한다. */
static uint32_t OutsideUnchanged(void)
{
    uint32_t i;
    for(i=0U;i<WORDS;++i) if((i<S2_WORD || i>=S2_WORD+SECTOR_WORDS) && flash[i]!=original[i]) return 0U;
    return 1U;
}

/* 인자/arm/context/현 resident/pending에 대한 거절은 erase 없이 끝나야 한다. */
static int TestReject(void)
{
    uint32_t words[5];
    Setup(FAULT_NONE);
    CHECK(UpdateMetadata_Read(0)==UPDATE_METADATA_ARGUMENT);
    CHECK(UpdateMetadata_Read(words)==UPDATE_METADATA_OK);
    CHECK(words[0]==expected_resident && words[4]==0U);
    CHECK(UpdateMetadata_Commit(TEST_VERSION,TEST_CRC,0U,scratch,sizeof(scratch))==UPDATE_METADATA_NOT_ARMED);
    CHECK(UpdateMetadata_Commit(TEST_VERSION,0U,UPDATE_METADATA_ARM_TOKEN,scratch,sizeof(scratch))==UPDATE_METADATA_ARGUMENT);
    CHECK(UpdateMetadata_Commit(TEST_VERSION,0xFFFFFFFFUL,UPDATE_METADATA_ARM_TOKEN,scratch,sizeof(scratch))==UPDATE_METADATA_ARGUMENT);
    CHECK(UpdateMetadata_Commit(0xFFFFFFFFUL,TEST_CRC,UPDATE_METADATA_ARM_TOKEN,scratch,sizeof(scratch))==UPDATE_METADATA_ARGUMENT);
    CHECK(UpdateMetadata_Commit(TEST_VERSION,TEST_CRC,UPDATE_METADATA_ARM_TOKEN,0,sizeof(scratch))==UPDATE_METADATA_ARGUMENT);
    CHECK(UpdateMetadata_Commit(TEST_VERSION,TEST_CRC,UPDATE_METADATA_ARM_TOKEN,scratch,sizeof(scratch)-1U)==UPDATE_METADATA_ARGUMENT);
    CHECK(UpdateMetadata_Commit(TEST_VERSION,TEST_CRC,UPDATE_METADATA_ARM_TOKEN,(void *)((uintptr_t)scratch+1U),sizeof(scratch))==UPDATE_METADATA_ARGUMENT);
    CHECK(UpdateMetadata_Commit(TEST_VERSION,TEST_CRC,UPDATE_METADATA_ARM_TOKEN,(void *)UPDATE_METADATA_ADDRESS,sizeof(scratch))==UPDATE_METADATA_ARGUMENT);
    context_bad=1U; CHECK(Commit()==UPDATE_METADATA_CONTEXT); CHECK(UpdateMetadata_Read(words)==UPDATE_METADATA_CONTEXT);
    context_bad=0U; readable=0U; CHECK(Commit()==UPDATE_METADATA_HARDWARE);
    readable=1U; flash[S2_WORD]^=1U; CHECK(Commit()==UPDATE_METADATA_RESIDENT_MISMATCH);
    flash[S2_WORD]=expected_resident; flash[S2_WORD+4U]=TEST_CRC; CHECK(Commit()==UPDATE_METADATA_PENDING);
    flash[S2_WORD+4U]=0xFFFFFFFFUL; CHECK(Commit()==UPDATE_METADATA_PENDING);
    CHECK(!erase_count && !program_count && locked && !entered && !bad_access);
    return 0;
}

/* 성공은 정확한 20-byte 레코드+나머지 byte 보존+CRC-last+재시도 차단을 요구. */
static int TestSuccess(void)
{
    uint32_t i,words[5];
    UpdateMetadataDiagnostics diag;
    Setup(FAULT_NONE);
    /* BL 성공 후 slot/length가 남는 정상 상태도 허용해야 한다. */
    flash[S2_WORD+2U]=UPDATE_METADATA_APP_BLOCK; flash[S2_WORD+3U]=UPDATE_METADATA_APP_LENGTH;
    reentry=1U;
    CHECK(Commit()==UPDATE_METADATA_OK);
    CHECK(reentry_result==UPDATE_METADATA_BUSY);
    CHECK(UpdateMetadata_Read(words)==UPDATE_METADATA_OK);
    CHECK(words[0]==expected_resident && words[1]==TEST_VERSION);
    CHECK(words[2]==UPDATE_METADATA_APP_BLOCK && words[3]==UPDATE_METADATA_APP_LENGTH && words[4]==TEST_CRC);
    for(i=5U;i<SECTOR_WORDS;++i) CHECK(flash[S2_WORD+i]==original[S2_WORD+i]);
    CHECK(OutsideUnchanged()); CHECK(!bad_access && !entered && locked && lock_count==1U && erase_count==1U);
    CHECK(!crc_before_preserve && last_program==UPDATE_METADATA_ADDRESS+16U);
    UpdateMetadata_GetDiagnostics(&diag);
    CHECK(diag.result==UPDATE_METADATA_OK && diag.stage==UPDATE_METADATA_STAGE_COMPLETE);
    CHECK(diag.crc_write_started==1U && diag.destructive_started==1U && diag.flash_locked==1U);
    CHECK(diag.words_programmed==program_count && diag.preserved_crc_before==diag.preserved_crc_after);
    CHECK(Commit()==UPDATE_METADATA_REVIEW_REQUIRED); CHECK(erase_count==1U);
    return 0;
}

/* erase 전 불안정 read/unlock 실패와 erase 이후의 모든 오류를 구별한다. */
static int TestFailures(void)
{
    uint32_t fault;
    UpdateMetadataDiagnostics diag;
    Setup(FAULT_BACKUP); CHECK(Commit()==UPDATE_METADATA_BACKUP_MISMATCH);
    CHECK(!erase_count && !program_count && !lock_count && locked);
    Setup(FAULT_BEGIN); CHECK(Commit()==UPDATE_METADATA_HARDWARE);
    CHECK(!erase_count && !program_count && !lock_count && locked);
    for(fault=FAULT_ERASE;fault<=FAULT_CRC_PARTIAL;++fault) {
        if(fault==FAULT_BACKUP) continue;
        Setup(fault);
        CHECK(Commit()==UPDATE_METADATA_AMBIGUOUS);
        CHECK(erase_count==1U && lock_count==1U && !bad_access && !entered);
        CHECK(OutsideUnchanged());
        UpdateMetadata_GetDiagnostics(&diag);
        CHECK(diag.result==UPDATE_METADATA_AMBIGUOUS && diag.destructive_started);
        CHECK(diag.flash_locked==(fault!=FAULT_LOCK));
        if(fault==FAULT_CRC || fault==FAULT_CRC_PARTIAL || fault==FAULT_FINALVERIFY || fault==FAULT_LOCK) CHECK(diag.crc_write_started);
        else CHECK(!diag.crc_write_started);
        CHECK(Commit()==UPDATE_METADATA_REVIEW_REQUIRED); CHECK(erase_count==1U);
    }
    /* 뒤쪽 보존 영역에서 program 실패해도 CRC를 건드리지 않는다. */
    Setup(FAULT_PROGRAM); fail_index=100U;
    CHECK(Commit()==UPDATE_METADATA_AMBIGUOUS); CHECK(last_program!=UPDATE_METADATA_ADDRESS+16U);
    CHECK(flash[S2_WORD+4U]==0xFFFFFFFFUL && locked && OutsideUnchanged());
    return 0;
}

/* freestanding ARM entry: 첫 실패 suite/line이 RAM 진단에 남는다. */
int test_main(void)
{
    int result;
    for(expected_resident=0x000E0000U;expected_resident<=0x000F0000U;expected_resident+=0x10000U){
    g_test_suite=1U; result=TestReject(); if(result) return result;
    g_test_suite=2U; result=TestSuccess(); if(result) return result;
    g_test_suite=3U; result=TestFailures(); if(result) return result;
    }
    g_test_suite=0U; return 0;
}
