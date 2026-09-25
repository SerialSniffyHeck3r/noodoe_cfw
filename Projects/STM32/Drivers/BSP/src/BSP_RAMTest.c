#include "BSP_RAMTest.h"
#include "BSP_RAMTest_Port.h"

volatile BSP_RAMTest_Diagnostics g_bsp_ramtest={.magic=BSP_RAMTEST_MAGIC,.version=BSP_RAMTEST_VERSION};
volatile BSP_RAMTest_Command g_bsp_ramtest_command={.version=BSP_RAMTEST_VERSION};
static BSP_RAMTest_Diagnostics status={.magic=BSP_RAMTEST_MAGIC,.version=BSP_RAMTEST_VERSION};
static uintptr_t arena;
static uint32_t words,cursor,seed,walk_bit,start_ms,dma_started,abort_started;
static uint32_t dma_count,dma_index,dma_owned,start_request,pending_result,pending_state;
static uint32_t dma_source[128] __attribute__((aligned(32)));
static uint32_t dma_destination[128] __attribute__((aligned(32)));

/* Only publication uses a critical section. The possibly millions of RAM
 * accesses, DMA waits and pattern calculations always execute with IRQs open.
 * A higher-priority consumer copies the last publication without spinning on
 * a preempted owner. SWD readers use the explicit odd/even sequence protocol. */
static void Publish(void)
{
    RAMTestPort_Enter();
    status.sequence=(g_bsp_ramtest.sequence+1U)|1U;
    g_bsp_ramtest.sequence=status.sequence;RAMTestPort_Barrier();
    g_bsp_ramtest=status;RAMTestPort_Barrier();
    g_bsp_ramtest.sequence=status.sequence+1U;
    RAMTestPort_Exit();
}
void BSP_RAMTest_GetDiagnostics(BSP_RAMTest_Diagnostics *out)
{
    if(!out)return;
    RAMTestPort_Enter();*out=g_bsp_ramtest;RAMTestPort_Exit();
}

/* Every DMA/CPU address is derived from our monotonic allocation plus a
 * bounded index. No API accepts an arbitrary caller-supplied physical address. */
BSP_RAMTest_Result BSP_RAMTest_Prepare(size_t bytes)
{
    if(!bytes)bytes=BSP_RAMTEST_DEFAULT_BYTES;
    if(bytes<BSP_RAMTEST_MIN_BYTES || bytes>BSP_RAMTEST_MAX_BYTES || (bytes&4095U))return BSP_RAMTEST_INVALID;
    if(arena)return bytes==status.arena_bytes?BSP_RAMTEST_OK:BSP_RAMTEST_INVALID;
    if(!RAMTestPort_Ready())return BSP_RAMTEST_NOT_READY;
    void *memory=RAMTestPort_Allocate(bytes);
    if(!memory)return BSP_RAMTEST_NO_MEMORY;
    arena=(uintptr_t)memory;words=(uint32_t)bytes/4U;
    status.arena_address=(uint32_t)arena;status.arena_bytes=(uint32_t)bytes;
    status.state=BSP_RAMTEST_READY;Publish();return BSP_RAMTEST_OK;
}

/* Address-derived values are injective over word indices for passes0/1.
 * Whole-area fill followed by a separate verify sweep exposes aliases that
 * immediate write/read pairs would miss. Later passes stress fixed data bits. */
static uint32_t Pattern(uint32_t index,uint32_t pass)
{
    uint32_t value=(uint32_t)(arena+index*4U)^seed;
    switch(pass){
    case 0:return value;
    case 1:return ~value;
    case 2:return 0x55AA55AAU;
    case 3:return 0xAA55AA55U;
    case 4:return 0U;
    case 5:return 0xFFFFFFFFU;
    default:
        value^=value<<13;value^=value>>17;value^=value<<5;
        return pass==6U?value:~value;
    }
}

static void Finish(uint32_t state,uint32_t result)
{
    status.state=state;status.result=result;status.phase=BSP_RAMTEST_PHASE_IDLE;
    status.completed_request=start_request;status.completed_operation=status.operation_id;
    status.elapsed_ms=RAMTestPort_Time()-start_ms;
    if(dma_owned && !status.dma_poisoned){RAMTestPort_DMARelease();dma_owned=0U;}
}

/* DMA remains confined to the reserved arena and static buffers during abort.
 * We cannot mark it reusable until hardware EN falls. An abort timeout poisons
 * the diagnostic for the rest of this boot instead of pretending it stopped. */
static void BeginAbort(uint32_t state,uint32_t result)
{
    pending_state=state;pending_result=result;abort_started=RAMTestPort_Time();
    status.phase=BSP_RAMTEST_PHASE_ABORT;
}
static void DataFailure(uintptr_t address,uint32_t expected,uint32_t observed)
{
    status.first_bad_address=(uint32_t)address;status.expected=expected;status.observed=observed;
    Finish(BSP_RAMTEST_FAILED,BSP_RAMTEST_DATA_ERROR);
}

BSP_RAMTest_Result BSP_RAMTest_Start(uint32_t operation_id,uint32_t selected_seed)
{
    if(!arena || status.dma_poisoned)return BSP_RAMTEST_NOT_READY;
    if(status.state==BSP_RAMTEST_RUNNING)return BSP_RAMTEST_BUSY;
    if(!operation_id || operation_id==status.operation_id)return BSP_RAMTEST_WRONG_OPERATION;
    if(!RAMTestPort_DMAClaim())return BSP_RAMTEST_BUSY;
    dma_owned=1U;seed=selected_seed;cursor=walk_bit=dma_index=0U;start_request=0U;
    start_ms=RAMTestPort_Time();
    uint32_t ack=status.request_ack,ack_result=status.request_result;
    status=(BSP_RAMTest_Diagnostics){.magic=BSP_RAMTEST_MAGIC,.version=BSP_RAMTEST_VERSION,
        .state=BSP_RAMTEST_RUNNING,.phase=BSP_RAMTEST_PHASE_WALK,.operation_id=operation_id,
        .arena_address=(uint32_t)arena,.arena_bytes=words*4U,.request_ack=ack,.request_result=ack_result};
    Publish();return BSP_RAMTEST_OK;
}
BSP_RAMTest_Result BSP_RAMTest_Cancel(uint32_t operation_id)
{
    if(!operation_id || operation_id!=status.operation_id)return BSP_RAMTEST_WRONG_OPERATION;
    if(status.state!=BSP_RAMTEST_RUNNING)return BSP_RAMTEST_NOT_READY;
    if(status.phase==BSP_RAMTEST_PHASE_ABORT)return BSP_RAMTEST_OK;
    if(status.phase==BSP_RAMTEST_PHASE_DMA_WRITE || status.phase==BSP_RAMTEST_PHASE_DMA_READ)
        BeginAbort(BSP_RAMTEST_CANCELLED,BSP_RAMTEST_OK);
    else Finish(BSP_RAMTEST_CANCELLED,BSP_RAMTEST_OK);
    Publish();return BSP_RAMTEST_OK;
}

/* Single writer request protocol: snapshot body only after a new sequence and
 * confirm the commit word again. Bad versions/commands never alter a running
 * operation. Completion carries the original START sequence, not CANCEL's ACK. */
static void ConsumeCommand(void)
{
    uint32_t sequence=g_bsp_ramtest_command.request_seq;
    if(!sequence || (int32_t)(sequence-status.request_ack)<=0)return;
    RAMTestPort_Barrier();
    BSP_RAMTest_Command request=g_bsp_ramtest_command;
    RAMTestPort_Barrier();
    if(sequence!=g_bsp_ramtest_command.request_seq || request.request_seq!=sequence)return;
    BSP_RAMTest_Result result=BSP_RAMTEST_INVALID;
    if(request.version==BSP_RAMTEST_VERSION){
        if(request.command==BSP_RAMTEST_COMMAND_START){
            result=BSP_RAMTest_Start(request.operation_id,request.seed);
            if(result==BSP_RAMTEST_OK)start_request=sequence;
        }else if(request.command==BSP_RAMTEST_COMMAND_CANCEL)
            result=BSP_RAMTest_Cancel(request.operation_id);
    }
    status.request_ack=sequence;status.request_result=result;
}

/* One DMA chunk occupies512 bytes in each of two internal SRAM buffers. The
 * source stays immutable until both DMA directions and CPU comparison finish. */
static int BeginDMA(uintptr_t source,uintptr_t destination,uint32_t next_phase)
{
    if(!RAMTestPort_DMAStart(source,destination,dma_count*4U)){
        BeginAbort(BSP_RAMTEST_FAILED,BSP_RAMTEST_DMA_ERROR);return 0;
    }
    dma_started=RAMTestPort_Time();status.phase=next_phase;return 1;
}

static void Slice(uint32_t budget)
{
    if(status.state!=BSP_RAMTEST_RUNNING)return;
    if(status.phase==BSP_RAMTEST_PHASE_ABORT){
        if(RAMTestPort_DMAStop())Finish(pending_state,pending_result);
        else if(RAMTestPort_Time()-abort_started>=100U){
            status.dma_poisoned=1U;Finish(BSP_RAMTEST_FAILED,BSP_RAMTEST_ABORT_TIMEOUT);
        }
        return;
    }
    if(status.phase==BSP_RAMTEST_PHASE_DMA_WRITE || status.phase==BSP_RAMTEST_PHASE_DMA_READ){
        int result=RAMTestPort_DMAPoll();
        if(result<0){BeginAbort(BSP_RAMTEST_FAILED,BSP_RAMTEST_DMA_ERROR);return;}
        if(!result){
            if(RAMTestPort_Time()-dma_started>=100U)BeginAbort(BSP_RAMTEST_FAILED,BSP_RAMTEST_DMA_TIMEOUT);
            return;
        }
        if(status.phase==BSP_RAMTEST_PHASE_DMA_WRITE){
            status.dma_bytes_written+=dma_count*4U;
            (void)BeginDMA(arena+cursor*4U,(uintptr_t)dma_destination,BSP_RAMTEST_PHASE_DMA_READ);
            return;
        }
        status.dma_bytes_read+=dma_count*4U;dma_index=0U;status.phase=BSP_RAMTEST_PHASE_DMA_VERIFY;
    }
    while(budget && status.state==BSP_RAMTEST_RUNNING){
        if(status.phase==BSP_RAMTEST_PHASE_WALK){
            if(budget<2U)break;
            uint32_t expected=1UL<<(walk_bit&31U);if(walk_bit>=32U)expected=~expected;
            RAMTestPort_Store(arena+cursor*4U,expected);RAMTestPort_Barrier();
            uint32_t observed=RAMTestPort_Load(arena+cursor*4U);
            ++status.words_written;++status.words_read;budget-=2U;
            if(observed!=expected){DataFailure(arena+cursor*4U,expected,observed);break;}
            if(++walk_bit==64U){walk_bit=0U;cursor+=1024U;}
            if(cursor>=words){cursor=0U;status.phase=BSP_RAMTEST_PHASE_FILL;}
        }else if(status.phase==BSP_RAMTEST_PHASE_FILL){
            RAMTestPort_Store(arena+cursor*4U,Pattern(cursor,status.pass));
            ++status.words_written;++cursor;--budget;
            if(cursor==words){RAMTestPort_Barrier();cursor=0U;status.phase=BSP_RAMTEST_PHASE_VERIFY;}
        }else if(status.phase==BSP_RAMTEST_PHASE_VERIFY){
            uint32_t expected=Pattern(cursor,status.pass),observed=RAMTestPort_Load(arena+cursor*4U);
            ++status.words_read;--budget;
            if(observed!=expected){DataFailure(arena+cursor*4U,expected,observed);break;}
            if(++cursor==words){
                cursor=0U;
                if(++status.pass==8U){dma_index=0U;status.phase=BSP_RAMTEST_PHASE_DMA_FILL;}
                else status.phase=BSP_RAMTEST_PHASE_FILL;
            }
        }else if(status.phase==BSP_RAMTEST_PHASE_DMA_FILL){
            if(budget<2U)break;
            dma_count=words-cursor;if(dma_count>128U)dma_count=128U;
            uint32_t expected=Pattern(cursor+dma_index,6U)^0xD00DFEEDU;
            dma_source[dma_index]=expected;dma_destination[dma_index]=~expected;
            ++dma_index;budget-=2U;
            if(dma_index==dma_count){
                (void)BeginDMA((uintptr_t)dma_source,arena+cursor*4U,BSP_RAMTEST_PHASE_DMA_WRITE);break;
            }
        }else if(status.phase==BSP_RAMTEST_PHASE_DMA_VERIFY){
            if(budget<2U)break;
            uintptr_t address=arena+(cursor+dma_index)*4U;
            uint32_t expected=dma_source[dma_index],observed=RAMTestPort_Load(address);
            ++status.words_read;budget-=2U;
            if(observed==expected)observed=dma_destination[dma_index];
            if(observed!=expected){DataFailure(address,expected,observed);break;}
            if(++dma_index==dma_count){
                cursor+=dma_count;dma_index=0U;
                if(cursor==words){Finish(BSP_RAMTEST_PASSED,BSP_RAMTEST_OK);break;}
                status.phase=BSP_RAMTEST_PHASE_DMA_FILL;
            }
        }else break;
    }
}

void BSP_RAMTest_Service(uint32_t word_budget)
{
    uint32_t cycle_start=RAMTestPort_Cycles();
    if(!word_budget)word_budget=BSP_RAMTEST_DEFAULT_WORD_BUDGET;
    if(word_budget>BSP_RAMTEST_MAX_WORD_BUDGET)word_budget=BSP_RAMTEST_MAX_WORD_BUDGET;
    ConsumeCommand();Slice(word_budget);
    ++status.calls;status.cursor_bytes=cursor*4U;
    if(status.state==BSP_RAMTEST_RUNNING)status.elapsed_ms=RAMTestPort_Time()-start_ms;
    uint32_t elapsed=RAMTestPort_Cycles()-cycle_start;
    if(elapsed>status.max_slice_cycles)status.max_slice_cycles=elapsed;
    Publish();
}
