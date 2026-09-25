/* Production register/transaction code on emulated ARM. The electrical model
 * decodes START/STOP and clock edges; it never reads production phase/bit state
 * to decide a sensor byte. Thus byte order, ACK timing and register allowlist
 * are independently checked against an OPT3001-like I2C endpoint. */
#include <string.h>
#define BSP_AMBIENT_BITBANG_TEST 1
#include "BSP_AmbientBitbang.c"
#include "BSP_AmbientAddress.c"
I2C_HandleTypeDef hi2c3;
uint32_t SystemCoreClock;
volatile uint32_t g_assertions,g_failure_line,g_mock_error;
static uint32_t cycles,master_scl,master_sda,slave_sda,started,mode,bits,rx;
static uint32_t ackstage,nextmode,reg,txbyte,txindex,starts,stops,wire_count;
static uint32_t wire[64],nak_at,stuck_scl,stuck_sda,wrong_id,stopped_clock;
static uint32_t sensor_mask,selected_address,address_byte,wrong_man_mask,wrong_dev_mask;
static uint32_t gap_at,gap_cycles,wire_clocks,force_restore,clock_stretch;
static uint32_t max_masked,masked_writes;
static uint32_t reenter,reentry_result,low_settle,low_reads,cannot_low,sda_conflict;
static uint32_t stop_failure;
static uint32_t cr1_writes,cr1_log[8],drop_ack_restore;
static uint32_t early_glitch,early_reads,setup_low;
static uint32_t needs_pullup,saw_pc9_pullup,foreign_pull;
#define CHECK(x) do{++g_assertions;if(!(x)){g_failure_line=__LINE__;return 1U;}}while(0)
void *memset(void *p,int v,size_t n){uint8_t *b=p;for(size_t i=0;i<n;++i)b[i]=(uint8_t)v;return p;}
uint32_t HAL_GetTick(void){return cycles/168000U;}
/* RM0090: a PE-off CR1 write is not ordinary RAM. ACK/POS/START/PEC/ALERT
 * clear in hardware and cannot remain set while PE=0. Keep this mask explicit
 * and independent of the production mask so a regression cannot change both.
 * Log write order: PE must be restored before saved ACK/POS are written. */
void ABB_TestCR1Write(uint32_t value)
{
    if(cr1_writes<8U)cr1_log[cr1_writes]=value;
    ++cr1_writes;
    if(!(value&1U))value&=~0x3D00U;
    if(drop_ack_restore && (value&1U))value&=~0x400U;
    I2C3->CR1=value;
}
uint32_t ABB_TestCycles(void)
{
    if(!stopped_clock)cycles+=168U;
    if(gap_at && wire_clocks>=gap_at && !master_scl){cycles+=gap_cycles;gap_at=0U;}
    if(reenter && ABB_DIAG.pin_changes && !__get_PRIMASK()) {
        reenter=0U;uint32_t seq=ABB_DIAG.sequence;
        reentry_result=BSP_AmbientBitbang_ReadIDs(999U,999U);
        if(ABB_DIAG.sequence!=seq || ABB_DIAG.operation_id==999U)++g_mock_error;
    }
    return cycles;
}
static uint32_t SensorByte(void)
{
    uint32_t index=selected_address-0x44U;
    uint32_t value=reg==0x7eU?((wrong_man_mask&(1U<<index))?0x1234U:0x5449U):
        ((wrong_id || (wrong_dev_mask&(1U<<index)))?0xABCDU:0x3001U);
    return txindex==0U?value>>8U:value&255U;
}
static void BeginReceive(void)
{mode=0U;bits=0U;rx=0U;ackstage=0U;slave_sda=1U;}
static void Rising(void)
{
    ++wire_clocks;
    if(mode==0U) {
        if(bits<8U){rx=(rx<<1U)|master_sda;++bits;}
        else ackstage=2U;
    }
    else if(mode==1U)++bits;
    else if(mode==2U) {
        if(txindex==0U && master_sda!=0U)++g_mock_error;
        if(txindex==1U && master_sda!=1U)++g_mock_error;
        ++txindex;ackstage=2U;
    }
}
static void Falling(void)
{
    if(mode==0U) {
        if(bits==8U && ackstage==0U) {
            if(wire_count>=64U){++g_mock_error;return;}
            wire[wire_count++]=rx;
            uint32_t nack=nak_at==wire_count;
            if(address_byte) {
                selected_address=rx>>1U;address_byte=0U;
                if(selected_address<0x44U || selected_address>0x47U)++g_mock_error;
                else if(!(sensor_mask&(1U<<(selected_address-0x44U))))nack=1U;
                nextmode=rx&1U;if(nextmode)txindex=0U;
            }
            else if(rx==0x7eU || rx==0x7fU){reg=rx;nextmode=0U;}
            else {++g_mock_error;nextmode=0U;}
            slave_sda=nack?1U:0U;ackstage=1U;
        }
        else if(ackstage==2U) {
            mode=nextmode;bits=0U;rx=0U;ackstage=0U;
            if(mode==1U){txbyte=SensorByte();slave_sda=(txbyte>>7U)&1U;}
            else slave_sda=1U;
        }
    }
    else if(mode==1U) {
        if(bits==8U){mode=2U;slave_sda=1U;ackstage=0U;}
        else slave_sda=(txbyte>>(7U-bits))&1U;
    }
    else if(mode==2U && ackstage==2U) {
        if(txindex<2U){mode=1U;bits=0U;txbyte=SensorByte();slave_sda=(txbyte>>7U)&1U;}
        else BeginReceive();
    }
}
void ABB_TestWrite(uint32_t line,uint32_t released)
{
    GPIO_TypeDef *port=line==1U?GPIOH:GPIOC;
    uint32_t pin=line==1U?7U:9U;
    if(released)port->ODR|=1UL<<pin;else port->ODR&=~(1UL<<pin);
    if(__get_PRIMASK()){++masked_writes;if(masked_writes>max_masked)max_masked=masked_writes;}
    else masked_writes=0U;
    /* AF mode is not driven by ODR; pin restoration must select AF before
     * reinstating an old LOW ODR. Ignore those writes electrically. */
    if(((port->MODER>>(pin*2U))&3U)!=1U)return;
    if(line==1U) {
        uint32_t old=master_scl;master_scl=released;
        if(!released)low_reads=low_settle;
        if(started && !old && released)Rising();
        if(started && old && !released)Falling();
    }
    else {
        uint32_t old=master_sda;master_sda=released;
        if(master_scl && old && !released) {started=1U;++starts;address_byte=1U;BeginReceive();}
        else if(master_scl && !old && released) {started=0U;++stops;slave_sda=1U;}
    }
}
uint32_t ABB_TestRead(void)
{
    uint32_t scl=master_scl,sda=master_sda&slave_sda;
    if(!master_scl && (low_reads || cannot_low)){scl=1U;if(low_reads)--low_reads;}
    if(stuck_scl || (clock_stretch && wire_clocks>=clock_stretch))scl=0U;
    if(stuck_sda)sda=0U;
    if(sda_conflict && wire_clocks>=sda_conflict)sda=0U;
    if(setup_low && starts && master_sda)sda=0U;
    uint32_t pc9_pull=(GPIOC->PUPDR>>18U)&3U;
    if(pc9_pull==1U)saw_pc9_pullup=1U;
    if(GPIOH->PUPDR&(3U<<14U))++g_mock_error;
    if(needs_pullup && starts && master_sda && pc9_pull!=1U)sda=0U;
    if(early_glitch && wire_clocks==1U && master_scl && early_reads<2U) {
        sda=0U;++early_reads;
    }
    if(stop_failure && stops>=stop_failure)sda=0U;
    if(force_restore && stops==2U) {
        if(force_restore==1U)I2C3->CCR^=1U;
        else if(force_restore==2U)I2C3->CR1^=I2C_CR1_NOSTRETCH;
        else I2C3->CR2^=1U;
        force_restore=0U;
    }
    if(foreign_pull && stops==2U) {
        GPIOC->PUPDR=(GPIOC->PUPDR&~(3U<<18U))|((foreign_pull==1U?2U:0U)<<18U);
        foreign_pull=0U;
    }
    /* Other GPIOs can change during the test: no full-port restore allowed. */
    GPIOH->ODR|=GPIO_PIN_1;GPIOC->MODER|=3U<<2U;
    GPIOC->PUPDR=(GPIOC->PUPDR&~(3U<<2U))|(2U<<2U);
    GPIOH->PUPDR=(GPIOH->PUPDR&~(3U<<4U))|(1U<<4U);
    GPIOH->IDR=(GPIOH->IDR&~GPIO_PIN_7)|(scl<<7U);
    GPIOC->IDR=(GPIOC->IDR&~GPIO_PIN_9)|(sda<<9U);
    return scl|(sda<<1U);
}
static void Reset(void)
{
    memset(&hi2c3,0,sizeof(hi2c3));memset(&abb,0,sizeof(abb));abb_busy=0U;
    memset((void*)&g_bsp_ambient_bitbang,0,sizeof(g_bsp_ambient_bitbang));
    memset((void*)&g_bsp_ambient_address,0,sizeof(g_bsp_ambient_address));
    memset((void*)&abb_address_detail,0,sizeof(abb_address_detail));
    abb_diag=&g_bsp_ambient_bitbang;
    memset(GPIOH,0,sizeof(*GPIOH));memset(GPIOC,0,sizeof(*GPIOC));memset(I2C3,0,sizeof(*I2C3));
    hi2c3.Instance=I2C3;hi2c3.State=HAL_I2C_STATE_READY;hi2c3.Lock=HAL_UNLOCKED;
    GPIOH->MODER=2U<<14U;GPIOH->OTYPER=GPIO_PIN_7;GPIOH->OSPEEDR=3U<<14U;
    GPIOH->AFR[0]=4U<<28U;GPIOC->MODER=2U<<18U;GPIOC->OTYPER=GPIO_PIN_9;
    GPIOC->OSPEEDR=3U<<18U;GPIOC->AFR[1]=4U<<4U;
    I2C3->CR1=I2C_CR1_PE|I2C_CR1_ACK;I2C3->CR2=42U;I2C3->CCR=210U;I2C3->TRISE=43U;
    RCC->AHB1ENR=RCC_AHB1ENR_GPIOCEN|RCC_AHB1ENR_GPIOHEN;RCC->APB1ENR=RCC_APB1ENR_I2C3EN;
    DWT->CTRL=DWT_CTRL_CYCCNTENA_Msk;SystemCoreClock=168000000U;
    cycles=0U;master_scl=master_sda=slave_sda=1U;started=mode=bits=rx=ackstage=0U;
    nextmode=reg=txbyte=txindex=starts=stops=wire_count=0U;
    nak_at=stuck_scl=stuck_sda=wrong_id=stopped_clock=0U;
    gap_at=gap_cycles=wire_clocks=force_restore=clock_stretch=0U;
    max_masked=masked_writes=0U;memset(wire,0,sizeof(wire));
    reenter=reentry_result=low_settle=low_reads=cannot_low=sda_conflict=0U;
    stop_failure=0U;
    cr1_writes=drop_ack_restore=0U;memset(cr1_log,0,sizeof(cr1_log));
    early_glitch=early_reads=setup_low=0U;
    needs_pullup=saw_pc9_pullup=foreign_pull=0U;
    sensor_mask=2U;selected_address=0x45U;address_byte=wrong_man_mask=wrong_dev_mask=0U;
}
static uint32_t Restored(void)
{
    return g_bsp_ambient_bitbang.restore_result==0U &&
        g_bsp_ambient_bitbang.saved_ph7==g_bsp_ambient_bitbang.final_ph7 &&
        g_bsp_ambient_bitbang.saved_pc9==g_bsp_ambient_bitbang.final_pc9 &&
        I2C3->CR1==(I2C_CR1_PE|I2C_CR1_ACK) && I2C3->CCR==210U &&
        hi2c3.Lock==HAL_UNLOCKED && !__get_PRIMASK() && max_masked<=2U;
}
uint32_t Ambient_BitbangTestMain(void)
{
    Reset();CHECK(sizeof(g_bsp_ambient_bitbang)==212U);
    CHECK(offsetof(BSP_AmbientBitbang_Diagnostics,first_pre_lines)==184U);
    CHECK(offsetof(BSP_AmbientBitbang_Diagnostics,pullup_mode)==208U);
    CHECK(BSP_AmbientBitbang_ReadIDs(7U,123U)==BSP_ABB_OK);
    CHECK(Restored() && g_bsp_ambient_bitbang.sequence==2U);
    CHECK(g_bsp_ambient_bitbang.operation_id==7U && g_bsp_ambient_bitbang.request_seq==123U);
    CHECK(g_bsp_ambient_bitbang.manufacturer==0x5449U && g_bsp_ambient_bitbang.device==0x3001U);
    CHECK(g_bsp_ambient_bitbang.ids_valid && g_bsp_ambient_bitbang.ack_count==6U && g_bsp_ambient_bitbang.ack_mask==63U);
    CHECK(wire_count==6U && wire[0]==0x8AU && wire[1]==0x7eU && wire[2]==0x8BU);
    CHECK(wire[3]==0x8AU && wire[4]==0x7fU && wire[5]==0x8BU);
    CHECK(starts==4U && stops==2U && !g_mock_error && !g_bsp_ambient_bitbang.timing_uncertain);
    CHECK(g_bsp_ambient_bitbang.scl_low_checks && g_bsp_ambient_bitbang.scl_high_checks);
    CHECK(g_bsp_ambient_bitbang.sda_low_checks && g_bsp_ambient_bitbang.sda_high_checks);
    CHECK((GPIOH->ODR&GPIO_PIN_1) && (GPIOC->MODER&(3U<<2U))==(3U<<2U));
    CHECK(g_bsp_ambient_bitbang.elapsed_us>4500U && g_bsp_ambient_bitbang.elapsed_us<15000U);
    CHECK(g_bsp_ambient_bitbang.version==3U && g_bsp_ambient_bitbang.bytes==212U);
    CHECK(g_bsp_ambient_bitbang.first_pre_lines==2U && g_bsp_ambient_bitbang.first_early_lines==3U);
    CHECK(g_bsp_ambient_bitbang.first_pre_us>0U &&
          g_bsp_ambient_bitbang.first_early_us>g_bsp_ambient_bitbang.first_pre_us);
    CHECK(!g_bsp_ambient_bitbang.first_late_us && !g_bsp_ambient_bitbang.first_late_lines);

    /* Every one of the six sensor ACKs can fail independently. No later byte
     * may be clocked after NACK; a STOP and exact restoration still happen. */
    for(uint32_t nack=1U;nack<=6U;++nack) {
        Reset();nak_at=nack;
        CHECK(BSP_AmbientBitbang_ReadIDs(nack,0U)==BSP_ABB_NACK);
        CHECK(Restored() && wire_count==nack && !g_bsp_ambient_bitbang.ids_valid);
        CHECK(g_bsp_ambient_bitbang.ack_count==nack && g_bsp_ambient_bitbang.ack_mask==(1UL<<(nack-1U))-1U);
        CHECK(g_bsp_ambient_bitbang.stop_result==0U);
    }
    Reset();wrong_id=1U;CHECK(BSP_AmbientBitbang_ReadIDs(1U,0U)==BSP_ABB_ID_MISMATCH);
    CHECK(Restored() && g_bsp_ambient_bitbang.ids_valid && g_bsp_ambient_bitbang.device==0xABCDU);
    Reset();stuck_sda=1U;CHECK(BSP_AmbientBitbang_ReadIDs(1U,0U)==BSP_ABB_BUS_BUSY);
    CHECK(!g_bsp_ambient_bitbang.pin_changes && g_bsp_ambient_bitbang.restore_result==2U && !starts);
    CHECK(!g_bsp_ambient_bitbang.first_pre_us && !g_bsp_ambient_bitbang.first_early_us &&
          !g_bsp_ambient_bitbang.first_late_us);
    Reset();stuck_scl=1U;CHECK(BSP_AmbientBitbang_ReadIDs(1U,0U)==BSP_ABB_BUS_BUSY);
    CHECK(!g_bsp_ambient_bitbang.pin_changes);
    Reset();clock_stretch=4U;CHECK(BSP_AmbientBitbang_ReadIDs(1U,0U)==BSP_ABB_SCL_TIMEOUT);
    CHECK(Restored() && !g_bsp_ambient_bitbang.ids_valid && g_bsp_ambient_bitbang.stop_result==2U);
    Reset();gap_at=4U;gap_cycles=168000U*35U;
    CHECK(BSP_AmbientBitbang_ReadIDs(1U,0U)==BSP_ABB_TIMING_UNCERTAIN);
    CHECK(Restored() && g_bsp_ambient_bitbang.timing_uncertain && g_bsp_ambient_bitbang.max_gap_us>=35000U);
    CHECK(g_bsp_ambient_bitbang.max_low_us>=35000U);
    Reset();stopped_clock=1U;CHECK(BSP_AmbientBitbang_ReadIDs(1U,0U)==BSP_ABB_CLOCK);
    CHECK(!g_bsp_ambient_bitbang.pin_changes);
    Reset();GPIOC->PUPDR=1U<<18U;CHECK(BSP_AmbientBitbang_ReadIDs(1U,0U)==BSP_ABB_PRECONDITION);
    CHECK(!g_bsp_ambient_bitbang.pin_changes && GPIOC->PUPDR==(1U<<18U));
    Reset();I2C3->CR2|=I2C_CR2_DMAEN;CHECK(BSP_AmbientBitbang_ReadIDs(1U,0U)==BSP_ABB_PRECONDITION);
    CHECK(!g_bsp_ambient_bitbang.pin_changes);
    Reset();__disable_irq();uint32_t result=BSP_AmbientBitbang_ReadIDs(1U,0U);__enable_irq();
    CHECK(result==BSP_ABB_CONTEXT && !g_bsp_ambient_bitbang.pin_changes);
    Reset();force_restore=1U;CHECK(BSP_AmbientBitbang_ReadIDs(1U,0U)==BSP_ABB_RESTORE_FAILED);
    CHECK(g_bsp_ambient_bitbang.restore_result==1U && I2C3->CCR==211U);
    CHECK(hi2c3.Lock==HAL_LOCKED && !(I2C3->CR1&I2C_CR1_PE) && !__get_PRIMASK());
    Reset();reenter=1U;CHECK(BSP_AmbientBitbang_ReadIDs(8U,2U)==BSP_ABB_OK);
    CHECK(reentry_result==BSP_ABB_PRECONDITION && Restored() && !g_mock_error);
    Reset();low_settle=3U;CHECK(BSP_AmbientBitbang_ReadIDs(1U,0U)==BSP_ABB_OK);
    CHECK(Restored() && !g_bsp_ambient_bitbang.timing_uncertain);
    Reset();cannot_low=1U;CHECK(BSP_AmbientBitbang_ReadIDs(1U,0U)==BSP_ABB_SCL_TIMEOUT);
    CHECK(Restored() && !wire_count);
    Reset();sda_conflict=1U;CHECK(BSP_AmbientBitbang_ReadIDs(1U,0U)==BSP_ABB_SDA_CONFLICT);
    CHECK(Restored() && !g_bsp_ambient_bitbang.ack_count);
    for(uint32_t failure=1U;failure<=2U;++failure) {
        Reset();stop_failure=failure;
        CHECK(BSP_AmbientBitbang_ReadIDs(1U,0U)==BSP_ABB_SDA_CONFLICT);
        CHECK(Restored() && g_bsp_ambient_bitbang.stop_result==BSP_ABB_SDA_CONFLICT);
        CHECK(g_bsp_ambient_bitbang.failed_phase==BSP_ABB_PHASE_STOP && wire_count==failure*3U);
    }
    /* Reproduce the real0x401 -> PE-off0x000 transition that the old plain-RAM
     * model missed, then test every saved ACK/POS combination with PE enabled. */
    Reset();ABB_CR1Write(I2C_CR1_ACK|I2C_CR1_POS);
    CHECK(I2C3->CR1==0U && I2C3->CR1!=(0xC01U&~I2C_CR1_PE));
    for(uint32_t policy=0U;policy<4U;++policy) {
        Reset();uint32_t saved=I2C_CR1_PE|(policy<<10U);I2C3->CR1=saved;
        CHECK(BSP_AmbientBitbang_ReadIDs(9U,0U)==BSP_ABB_OK);
        CHECK(g_bsp_ambient_bitbang.saved_cr1==saved && g_bsp_ambient_bitbang.final_cr1==saved);
        CHECK(g_bsp_ambient_bitbang.restore_result==0U && hi2c3.Lock==HAL_UNLOCKED);
        CHECK(cr1_writes==3U && cr1_log[0]==(saved&~1U) && cr1_log[1]==1U && cr1_log[2]==saved);
    }
    Reset();I2C3->CR1=0U;CHECK(BSP_AmbientBitbang_ReadIDs(1U,0U)==BSP_ABB_OK);
    CHECK(g_bsp_ambient_bitbang.final_cr1==0U && !g_bsp_ambient_bitbang.restore_result && cr1_writes==2U);
    for(uint32_t command=0U;command<2U;++command) {
        Reset();I2C3->CR1|=command?I2C_CR1_ALERT:I2C_CR1_PEC;
        CHECK(BSP_AmbientBitbang_ReadIDs(1U,0U)==BSP_ABB_PRECONDITION);
        CHECK(!g_bsp_ambient_bitbang.pin_changes && !cr1_writes);
    }
    for(uint32_t foreign=2U;foreign<=3U;++foreign) {
        Reset();force_restore=foreign;CHECK(BSP_AmbientBitbang_ReadIDs(1U,0U)==BSP_ABB_RESTORE_FAILED);
        CHECK(g_bsp_ambient_bitbang.restore_result==1U && hi2c3.Lock==HAL_LOCKED && !(I2C3->CR1&1U));
        CHECK(foreign==2U?(I2C3->CR1&I2C_CR1_NOSTRETCH)!=0U:I2C3->CR2==43U);
    }
    Reset();drop_ack_restore=1U;CHECK(BSP_AmbientBitbang_ReadIDs(1U,0U)==BSP_ABB_RESTORE_FAILED);
    CHECK(g_bsp_ambient_bitbang.restore_result==1U && hi2c3.Lock==HAL_LOCKED && I2C3->CR1==0U);
    /* Later HIGH must never convert an already invalid bit to success or
     * continue addressing the device. Compare the recorded setup/early/late
     * tuples against independent line faults without changing firmware policy. */
    Reset();early_glitch=1U;
    CHECK(BSP_AmbientBitbang_ReadIDs(33U,44U)==BSP_ABB_SDA_CONFLICT);
    CHECK(Restored() && g_bsp_ambient_bitbang.first_pre_lines==2U);
    CHECK(g_bsp_ambient_bitbang.first_early_lines==1U && g_bsp_ambient_bitbang.first_late_lines==3U);
    CHECK(g_bsp_ambient_bitbang.first_late_us-g_bsp_ambient_bitbang.first_early_us>=25U);
    CHECK(!wire_count && !g_bsp_ambient_bitbang.ids_valid && !g_bsp_ambient_bitbang.ack_count);
    CHECK(g_bsp_ambient_bitbang.failed_phase==BSP_ABB_PHASE_ADDRESS_WRITE &&
          g_bsp_ambient_bitbang.byte_index==1U && g_bsp_ambient_bitbang.bit_index==0U);
    Reset();setup_low=1U;
    CHECK(BSP_AmbientBitbang_ReadIDs(1U,0U)==BSP_ABB_SDA_CONFLICT);
    CHECK(Restored() && g_bsp_ambient_bitbang.first_pre_lines==0U);
    CHECK(g_bsp_ambient_bitbang.first_early_lines==1U && g_bsp_ambient_bitbang.first_late_lines==1U);
    CHECK(g_bsp_ambient_bitbang.first_late_us>g_bsp_ambient_bitbang.first_early_us && !wire_count);
    /* Explicit weak-bias experiment. The model behaves normally only when
     * the actual PC9 PUPDR field is UP; normal diagnostics must remain NOPULL.
     * This models a possible response,not the electrical identity of the PCB. */
    Reset();needs_pullup=1U;
    CHECK(BSP_AmbientBitbang_ReadIDs(1U,0U)==BSP_ABB_SDA_CONFLICT);
    CHECK(!saw_pc9_pullup && !g_bsp_ambient_bitbang.pullup_mode && Restored());
    Reset();needs_pullup=1U;
    CHECK(BSP_AmbientBitbang_ReadIDsWithPullup(2U,4U)==BSP_ABB_OK);
    CHECK(saw_pc9_pullup && g_bsp_ambient_bitbang.pullup_mode==1U && Restored());
    CHECK(g_bsp_ambient_bitbang.ids_valid && g_bsp_ambient_bitbang.manufacturer==0x5449U &&
          g_bsp_ambient_bitbang.device==0x3001U && wire_count==6U);
    CHECK((GPIOC->PUPDR&(3U<<18U))==0U && (GPIOH->PUPDR&(3U<<14U))==0U);
    CHECK((GPIOC->PUPDR&(3U<<2U))==(2U<<2U) && (GPIOH->PUPDR&(3U<<4U))==(1U<<4U));
    /* Without resetting production state/hardware registers, the next normal
     * diagnostic must not inherit the previous temporary weak pull-up. */
    wire_count=starts=stops=0U;saw_pc9_pullup=0U;masked_writes=max_masked=0U;
    CHECK(BSP_AmbientBitbang_ReadIDs(3U,5U)==BSP_ABB_SDA_CONFLICT);
    CHECK(!g_bsp_ambient_bitbang.pullup_mode && !saw_pc9_pullup && Restored());
    Reset();setup_low=1U;
    CHECK(BSP_AmbientBitbang_ReadIDsWithPullup(1U,0U)==BSP_ABB_SDA_CONFLICT);
    CHECK(Restored() && saw_pc9_pullup && !(GPIOC->PUPDR&(3U<<18U)));
    CHECK(g_bsp_ambient_bitbang.first_pre_lines==0U && g_bsp_ambient_bitbang.first_early_lines==1U &&
          g_bsp_ambient_bitbang.first_late_lines==1U && !wire_count);
    Reset();needs_pullup=1U;early_glitch=1U;
    CHECK(BSP_AmbientBitbang_ReadIDsWithPullup(1U,0U)==BSP_ABB_SDA_CONFLICT);
    CHECK(Restored() && g_bsp_ambient_bitbang.first_early_lines==1U &&
          g_bsp_ambient_bitbang.first_late_lines==3U && !wire_count && !g_bsp_ambient_bitbang.ids_valid);
    for(uint32_t foreign=1U;foreign<=2U;++foreign) {
        Reset();needs_pullup=1U;foreign_pull=foreign;
        CHECK(BSP_AmbientBitbang_ReadIDsWithPullup(1U,0U)==BSP_ABB_RESTORE_FAILED);
        CHECK(g_bsp_ambient_bitbang.restore_result==1U && !(GPIOC->PUPDR&(3U<<18U)));
        CHECK(g_bsp_ambient_bitbang.saved_pc9==g_bsp_ambient_bitbang.final_pc9 &&
              hi2c3.Lock==HAL_LOCKED && !(I2C3->CR1&I2C_CR1_PE));
    }
    for(uint32_t nack=1U;nack<=6U;++nack) {
        Reset();needs_pullup=1U;nak_at=nack;
        CHECK(BSP_AmbientBitbang_ReadIDsWithPullup(1U,0U)==BSP_ABB_NACK);
        CHECK(Restored() && saw_pc9_pullup && !(GPIOC->PUPDR&(3U<<18U)) && wire_count==nack);
    }
    Reset();needs_pullup=1U;clock_stretch=4U;
    CHECK(BSP_AmbientBitbang_ReadIDsWithPullup(1U,0U)==BSP_ABB_SCL_TIMEOUT);
    CHECK(Restored() && !(GPIOC->PUPDR&(3U<<18U)));
    Reset();needs_pullup=1U;gap_at=4U;gap_cycles=168000U*35U;
    CHECK(BSP_AmbientBitbang_ReadIDsWithPullup(1U,0U)==BSP_ABB_TIMING_UNCERTAIN);
    CHECK(Restored() && !(GPIOC->PUPDR&(3U<<18U)) && g_bsp_ambient_bitbang.timing_uncertain);
    Reset();GPIOC->PUPDR=1U<<18U;
    CHECK(BSP_AmbientBitbang_ReadIDsWithPullup(1U,0U)==BSP_ABB_PRECONDITION);
    CHECK(!g_bsp_ambient_bitbang.pin_changes && GPIOC->PUPDR==(1U<<18U) && !cr1_writes);
    GPIOC->PUPDR=0U;
    CHECK(BSP_AmbientBitbang_ReadIDs(2U,0U)==BSP_ABB_OK && Restored());
    Reset();stuck_sda=1U;
    CHECK(BSP_AmbientBitbang_ReadIDsWithPullup(1U,0U)==BSP_ABB_BUS_BUSY);
    CHECK(g_bsp_ambient_bitbang.pullup_mode==1U && !g_bsp_ambient_bitbang.pin_changes &&
          g_bsp_ambient_bitbang.restore_result==2U && !saw_pc9_pullup && !cr1_writes);
    CHECK(!g_mock_error);
    return 0U;
}
