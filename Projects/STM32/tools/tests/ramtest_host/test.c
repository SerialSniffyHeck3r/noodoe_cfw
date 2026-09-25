#include "BSP_RAMTest.c"
#include <string.h>

static uint32_t assertions,ready=1U,allocations,now,cycles,critical,critical_errors;
static uint32_t writes,reads,budget_violation,outside_access,alias_mask=0xFFFFFFFFU,flip_read;
static uint32_t dma_claim,dma_active_mock,dma_pending,dma_mode,dma_stop_mode,dma_count_mock;
static uintptr_t dma_src,dma_dst;
#define BASE 0xC0010000U
#define BYTES 65536U
#define CHECK(x) do{++assertions;if(!(x))return __LINE__;}while(0)
void *memset(void *p,int c,size_t n){unsigned char *b=p;while(n--)*b++=(unsigned char)c;return p;}
void *memcpy(void *d,const void *s,size_t n){unsigned char *b=d;const unsigned char *a=s;while(n--)*b++=*a++;return d;}
uint32_t get_assertions(void){return assertions;}
int RAMTestPort_Ready(void){return ready;}
void *RAMTestPort_Allocate(size_t bytes){if(bytes!=BYTES)return NULL;++allocations;return(void*)BASE;}
uint32_t RAMTestPort_Time(void){return now;}
uint32_t RAMTestPort_Cycles(void){return ++cycles;}
void RAMTestPort_Enter(void){++critical;}
void RAMTestPort_Exit(void){if(critical)--critical;else ++critical_errors;}
void RAMTestPort_Barrier(void){}
static uintptr_t Map(uintptr_t address)
{
    if(address<BASE || address>=BASE+BYTES)++outside_access;
    return BASE+((address-BASE)&alias_mask);
}
void RAMTestPort_Store(uintptr_t address,uint32_t value)
{if(critical)++critical_errors;++writes;cycles+=3;*(uint32_t*)Map(address)=value;}
uint32_t RAMTestPort_Load(uintptr_t address)
{if(critical)++critical_errors;++reads;cycles+=3;return *(uint32_t*)Map(address)^(flip_read?1U:0U);}
int RAMTestPort_DMAClaim(void){if(dma_claim)return 0;dma_claim=1;return 1;}
void RAMTestPort_DMARelease(void){if(dma_active_mock)++critical_errors;else dma_claim=0;}
int RAMTestPort_DMAStart(uintptr_t source,uintptr_t destination,uint32_t bytes)
{
    if(dma_active_mock || bytes>512 || !dma_claim)return 0;
    dma_src=source;dma_dst=destination;dma_count_mock=bytes;dma_active_mock=1;dma_pending=1;return 1;
}
int RAMTestPort_DMAPoll(void)
{
    if(!dma_active_mock)return -1;
    if(dma_mode==1U)return 0;
    if(dma_mode==2U)return -1;
    if(dma_pending){--dma_pending;return 0;}
    memcpy((void*)dma_dst,(const void*)dma_src,dma_count_mock);
    if(dma_mode==3U && dma_src>=BASE && dma_src<BASE+BYTES)*(uint32_t*)dma_dst^=1U;
    dma_active_mock=0;return 1;
}
int RAMTestPort_DMAStop(void)
{
    if(dma_stop_mode==1U)return 0;
    if(dma_stop_mode==2U){dma_stop_mode=0;return 0;}
    dma_active_mock=0;return 1;
}
static void Step(uint32_t budget)
{
    uint32_t before=reads+writes;now+=2U;BSP_RAMTest_Service(budget);
    uint32_t actual=budget?budget:BSP_RAMTEST_DEFAULT_WORD_BUDGET;
    if(actual>BSP_RAMTEST_MAX_WORD_BUDGET)actual=BSP_RAMTEST_MAX_WORD_BUDGET;
    if(reads+writes-before>actual)++budget_violation;
}
static uint32_t Until(uint32_t phase)
{
    for(unsigned i=0;i<20000;i++){
        if(status.phase==phase)return 1;
        if(status.state!=BSP_RAMTEST_RUNNING)return 0;
        Step(256);
    }return 0;
}
static uint32_t Complete(void)
{
    for(unsigned i=0;i<20000 && status.state==BSP_RAMTEST_RUNNING;i++)Step(256);
    return status.state!=BSP_RAMTEST_RUNNING;
}
static void Command(uint32_t sequence,uint32_t version,uint32_t command,uint32_t operation)
{
    g_bsp_ramtest_command.version=version;g_bsp_ramtest_command.command=command;
    g_bsp_ramtest_command.operation_id=operation;g_bsp_ramtest_command.seed=0x13579BDF;
    g_bsp_ramtest_command.request_seq=sequence;Step(256);
}
uint32_t test_prepare(void)
{
    ready=0;CHECK(BSP_RAMTest_Prepare(BYTES)==BSP_RAMTEST_NOT_READY);CHECK(allocations==0);
    ready=1;CHECK(BSP_RAMTest_Prepare(4)==BSP_RAMTEST_INVALID);CHECK(allocations==0);
    CHECK(BSP_RAMTest_Prepare(BYTES)==0);CHECK(BSP_RAMTest_Prepare(BYTES)==0);CHECK(allocations==1);
    CHECK(BSP_RAMTest_Prepare(BYTES*2)==BSP_RAMTEST_INVALID);CHECK(allocations==1);
    CHECK(BSP_RAMTest_Start(0,1)==BSP_RAMTEST_WRONG_OPERATION);
    CHECK(BSP_RAMTest_Start(1,1)==0);CHECK(BSP_RAMTest_Start(2,1)==BSP_RAMTEST_BUSY);
    CHECK(BSP_RAMTest_Cancel(2)==BSP_RAMTEST_WRONG_OPERATION);CHECK(BSP_RAMTest_Cancel(1)==0);
    CHECK(status.state==BSP_RAMTEST_CANCELLED && !dma_claim);
    CHECK(BSP_RAMTest_Start(1,1)==BSP_RAMTEST_WRONG_OPERATION);
    return 0;
}
uint32_t test_success(void)
{
    *(uint32_t*)(BASE-4)=0xDEADBEEFU;*(uint32_t*)(BASE+BYTES)=0xC0FFEE11U;
    CHECK(BSP_RAMTest_Prepare(BYTES)==0);
    Command(5,1,1,77);CHECK(status.request_ack==5 && status.request_result==0);
    CHECK(Complete());CHECK(status.state==BSP_RAMTEST_PASSED && status.result==0);
    CHECK(status.completed_request==5 && status.completed_operation==77);
    CHECK(status.dma_bytes_written==BYTES && status.dma_bytes_read==BYTES);
    CHECK(status.words_written==8*BYTES/4+BYTES/4096*64);
    CHECK(status.words_read==9*BYTES/4+BYTES/4096*64);
    CHECK(status.max_slice_cycles>0 && (g_bsp_ramtest.sequence&1)==0);
    CHECK(*(uint32_t*)(BASE-4)==0xDEADBEEF && *(uint32_t*)(BASE+BYTES)==0xC0FFEE11);
    CHECK(!outside_access && !budget_violation && !critical_errors && !critical && !dma_claim);
    BSP_RAMTest_Diagnostics copy;BSP_RAMTest_GetDiagnostics(&copy);CHECK(copy.state==BSP_RAMTEST_PASSED);
    return 0;
}
uint32_t test_walk_failure(void)
{
    CHECK(BSP_RAMTest_Prepare(BYTES)==0);CHECK(BSP_RAMTest_Start(1,123)==0);
    flip_read=1;Step(256);CHECK(status.state==BSP_RAMTEST_FAILED);
    CHECK(status.first_bad_address==BASE && status.expected==1 && status.observed==0);
    CHECK(status.result==BSP_RAMTEST_DATA_ERROR && !dma_claim);return 0;
}
uint32_t test_alias_failure(void)
{
    CHECK(BSP_RAMTest_Prepare(BYTES)==0);CHECK(BSP_RAMTest_Start(1,123)==0);
    alias_mask=~4096U;CHECK(Complete());CHECK(status.state==BSP_RAMTEST_FAILED);
    CHECK(status.pass==0 && status.result==BSP_RAMTEST_DATA_ERROR);CHECK(!outside_access);return 0;
}
uint32_t test_mailbox_validation(void)
{
    CHECK(BSP_RAMTest_Prepare(BYTES)==0);
    Command(1,2,1,10);CHECK(status.request_result==BSP_RAMTEST_INVALID && status.state==BSP_RAMTEST_READY);
    Command(2,1,99,10);CHECK(status.request_result==BSP_RAMTEST_INVALID && status.state==BSP_RAMTEST_READY);
    Command(3,1,1,10);CHECK(status.request_result==0 && status.state==BSP_RAMTEST_RUNNING);
    Command(4,1,1,11);CHECK(status.request_result==BSP_RAMTEST_BUSY && status.operation_id==10);
    Command(5,1,2,11);CHECK(status.request_result==BSP_RAMTEST_WRONG_OPERATION && status.state==BSP_RAMTEST_RUNNING);
    Command(6,1,2,10);CHECK(status.request_result==0 && status.state==BSP_RAMTEST_CANCELLED);
    CHECK(status.completed_request==3 && status.completed_operation==10);
    Command(6,1,1,99);CHECK(status.state==BSP_RAMTEST_CANCELLED); /* duplicate commit */
    Command(4,1,1,99);CHECK(status.state==BSP_RAMTEST_CANCELLED && status.request_ack==6); /* stale commit */
    CHECK(!budget_violation && !critical_errors);return 0;
}
uint32_t test_dma_corruption(void)
{
    CHECK(BSP_RAMTest_Prepare(BYTES)==0);CHECK(BSP_RAMTest_Start(1,123)==0);
    CHECK(Until(BSP_RAMTEST_PHASE_DMA_READ));dma_mode=3;
    CHECK(Complete());CHECK(status.result==BSP_RAMTEST_DATA_ERROR && status.first_bad_address==BASE);
    CHECK(!dma_claim && !dma_active_mock);return 0;
}
uint32_t test_dma_timeout(void)
{
    CHECK(BSP_RAMTest_Prepare(BYTES)==0);CHECK(BSP_RAMTest_Start(1,123)==0);
    CHECK(Until(BSP_RAMTEST_PHASE_DMA_WRITE));dma_mode=1;
    CHECK(Until(BSP_RAMTEST_PHASE_ABORT));CHECK(status.state==BSP_RAMTEST_RUNNING && dma_claim);
    CHECK(Complete());CHECK(status.result==BSP_RAMTEST_DMA_TIMEOUT && !dma_claim && !dma_active_mock);
    return 0;
}
uint32_t test_dma_cancel(void)
{
    CHECK(BSP_RAMTest_Prepare(BYTES)==0);Command(19,1,1,75);
    CHECK(Until(BSP_RAMTEST_PHASE_DMA_WRITE));dma_stop_mode=2;
    Command(20,1,2,75);CHECK(status.state==BSP_RAMTEST_RUNNING && dma_active_mock);
    CHECK(status.request_ack==20 && !status.completed_operation);
    CHECK(Complete());CHECK(status.state==BSP_RAMTEST_CANCELLED && !dma_active_mock && !dma_claim);
    CHECK(status.completed_request==19 && status.completed_operation==75);return 0;
}
uint32_t test_stuck_dma_quarantined(void)
{
    CHECK(BSP_RAMTest_Prepare(BYTES)==0);CHECK(BSP_RAMTest_Start(1,123)==0);
    CHECK(Until(BSP_RAMTEST_PHASE_DMA_WRITE));dma_stop_mode=1;
    CHECK(BSP_RAMTest_Cancel(1)==0);CHECK(Complete());
    CHECK(status.result==BSP_RAMTEST_ABORT_TIMEOUT && status.dma_poisoned && dma_claim && dma_active_mock);
    CHECK(BSP_RAMTest_Start(2,123)==BSP_RAMTEST_NOT_READY);return 0;
}
