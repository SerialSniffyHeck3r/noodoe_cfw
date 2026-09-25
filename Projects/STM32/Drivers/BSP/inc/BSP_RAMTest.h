#ifndef BSP_RAMTEST_H
#define BSP_RAMTEST_H
#include <stddef.h>
#include <stdint.h>

#define BSP_RAMTEST_MAGIC 0x524D5431UL
#define BSP_RAMTEST_VERSION 1U
#define BSP_RAMTEST_DEFAULT_BYTES (4UL * 1024UL * 1024UL)
#define BSP_RAMTEST_MIN_BYTES (64UL * 1024UL)
#define BSP_RAMTEST_MAX_BYTES BSP_RAMTEST_DEFAULT_BYTES
#define BSP_RAMTEST_DEFAULT_WORD_BUDGET 256U
#define BSP_RAMTEST_MAX_WORD_BUDGET 1024U

typedef enum {
    BSP_RAMTEST_UNPREPARED=0, BSP_RAMTEST_READY, BSP_RAMTEST_RUNNING,
    BSP_RAMTEST_PASSED, BSP_RAMTEST_FAILED, BSP_RAMTEST_CANCELLED
} BSP_RAMTest_State;
typedef enum {
    BSP_RAMTEST_PHASE_IDLE=0, BSP_RAMTEST_PHASE_WALK,
    BSP_RAMTEST_PHASE_FILL, BSP_RAMTEST_PHASE_VERIFY,
    BSP_RAMTEST_PHASE_DMA_FILL, BSP_RAMTEST_PHASE_DMA_WRITE,
    BSP_RAMTEST_PHASE_DMA_READ, BSP_RAMTEST_PHASE_DMA_VERIFY,
    BSP_RAMTEST_PHASE_ABORT
} BSP_RAMTest_Phase;
typedef enum {
    BSP_RAMTEST_OK=0, BSP_RAMTEST_INVALID=1, BSP_RAMTEST_NOT_READY=2,
    BSP_RAMTEST_BUSY=3, BSP_RAMTEST_NO_MEMORY=4, BSP_RAMTEST_DATA_ERROR=5,
    BSP_RAMTEST_DMA_ERROR=6, BSP_RAMTEST_DMA_TIMEOUT=7,
    BSP_RAMTEST_ABORT_TIMEOUT=8, BSP_RAMTEST_WRONG_OPERATION=9
} BSP_RAMTest_Result;

/* All fields are fixed-width for SWD. sequence is odd during publication;
 * read sequence/record/sequence and accept only matching even sequences.
 * This deep result is separate from g_bsp_ram's basic ready/capacity proof. */
typedef struct {
    uint32_t magic,version,sequence,state,phase,result,operation_id;
    uint32_t arena_address,arena_bytes,pass,cursor_bytes,elapsed_ms;
    uint32_t words_written,words_read,dma_bytes_written,dma_bytes_read;
    uint32_t first_bad_address,expected,observed,max_slice_cycles;
    uint32_t request_ack,request_result,completed_request,completed_operation;
    uint32_t calls,dma_poisoned;
} BSP_RAMTest_Diagnostics;
extern volatile BSP_RAMTest_Diagnostics g_bsp_ramtest;

/* Debug host writes version/command/operation_id/seed first and request_seq
 * LAST. Use monotonically increasing sequences (skip zero on wrap) and await
 * request_ack before modifying that request body again. Stale commits are ignored.
 * START=1 requires a new nonzero operation ID. CANCEL=2 requires the current
 * operation ID. request_ack means accepted/rejected, not test completion.
 * completed_request is the START sequence whose operation actually ended. */
typedef struct {
    uint32_t version,command,operation_id,seed,request_seq;
} BSP_RAMTest_Command;
extern volatile BSP_RAMTest_Command g_bsp_ramtest_command;
#define BSP_RAMTEST_COMMAND_START 1U
#define BSP_RAMTEST_COMMAND_CANCEL 2U

/* Owner task only; reserve once after successful BSP_RAM_Init. Never calls
 * Init or frees/returns its arena. A repeat with the same size is idempotent.
 * Other clients' allocations and the original 64KiB scratch are untouched. */
BSP_RAMTest_Result BSP_RAMTest_Prepare(size_t bytes);
/* Owner task only. Start/Cancel return immediately; call Service periodically.
 * operation_id must not equal the previous operation ID on a repeated run. */
BSP_RAMTest_Result BSP_RAMTest_Start(uint32_t operation_id,uint32_t seed);
BSP_RAMTest_Result BSP_RAMTest_Cancel(uint32_t operation_id);
/* Consume at most one mailbox request and a bounded CPU slice. Zero selects
 * 256 word operations; larger values clamp to1024. DMA never busy-waits.
 * Use one low-priority task, initially every2ms, and measure max_slice_cycles. */
void BSP_RAMTest_Service(uint32_t word_budget);
/* Thread-safe short published snapshot; contains no wait for a lower task. */
void BSP_RAMTest_GetDiagnostics(BSP_RAMTest_Diagnostics *out);
#endif
