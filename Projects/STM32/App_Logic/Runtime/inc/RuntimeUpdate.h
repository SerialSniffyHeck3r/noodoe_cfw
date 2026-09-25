#ifndef RUNTIME_UPDATE_H
#define RUNTIME_UPDATE_H
#include "Update_Service.h"

typedef enum {
    RUNTIME_UPDATE_OK=0, RUNTIME_UPDATE_CONTEXT, RUNTIME_UPDATE_NOT_READY,
    RUNTIME_UPDATE_NO_MEMORY, RUNTIME_UPDATE_BAD_MEMORY, RUNTIME_UPDATE_ARGUMENT,
    RUNTIME_UPDATE_LOCKED
} RuntimeUpdate_Result;

typedef struct {
    uint32_t ready, result, service_address, service_bytes;
    uint32_t scratch_address, scratch_bytes, polls, last_platform_result;
    uint32_t enabled_transaction, commit_calls, reset_calls;
    /* Process 뒤의 eventual 진단이다. IOTask의 connected/authorized 변경과 원자적
     * transaction snapshot을 이루지 않으며 HUD는 진행 관측용으로만 읽는다. */
    uint32_t state, service_result, connected, authorized, received_bytes, verified_bytes;
    uint32_t commit_phase,commit_result,commit_detail,commit_destructive;
    uint32_t writer_result,resume_result;
} RuntimeUpdate_Diagnostics;
extern volatile RuntimeUpdate_Diagnostics g_runtime_update;

/* SPI5 소유 StorageTask에서 BSP_RAM/NOR 초기화 뒤 호출한다. 검증 SDRAM에 서비스와
 * 전용16KiB metadata scratch를 확보한다. 0=성공이며 실패 후 GetService는 NULL이다.
 * 성공 뒤 재호출은 서비스/진행 transaction을 초기화하지 않는다. 기본 authorization은
 * 잠김이고 Init은 NOR erase/program, metadata commit 또는 reset을 실행하지 않는다. */
uint32_t RuntimeUpdate_Init(void);

/* release/acquire로 완전히 초기화한 객체만 공개한다. 준비 전에는 NULL이다.
 * IOTask가 Handle/TakeReply/NotifyReplyTransmitted 및 SetConnected/Authorize를
 * 소유한다. 특히 SetConnected와 Authorize를 서로 다른 태스크에서 경쟁시키지 않는다.
 * 전체 NOR A/B 검증 뒤 명시 Authorize를 하기 전에는 updater가 계속 잠겨 있다. */
UpdateService *RuntimeUpdate_GetService(void);

/* 동일 StorageTask에서 호출한다. now_ms는 wrap 가능한 millisecond tick이다.
 * 준비 전에는 아무 일도 하지 않는다. 큐에 들어온 명시 요청만 UpdateService로 넘기며
 * metadata callback 동안 RTOS critical section으로 IRQ를 막으면 안 된다. */
void RuntimeUpdate_Process(uint32_t now_ms);
/* Called after bounded policy/BT pause work and before entering the writer.
 * Bootstrap runs its real all-owner health check here, never a blind feed. */
uint32_t RuntimeUpdate_FlashReady(void);

/* Bootstrap-only commit policy. 0 allows, nonzero denies; weak default denies.
 * CFW passes 44 canonical bytes freshly read at APP+0x200 after full verification.
 * STOCK passes NULL and has already passed the pinned stock version/SHA check.
 * The maintenance owner must prove compatible RSC and local recovery readiness. */
uint32_t BootstrapUpdate_ValidateTarget(uint32_t target,const uint8_t requirement44[44],
    uint32_t version,const uint8_t sha[32]);
#endif
