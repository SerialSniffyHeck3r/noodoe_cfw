#include "BSP_AmbientBitbang.h"
#include "bsp_ambient_bitbang_private.h"
#include "i2c.h"
#include <stddef.h>
#include <string.h>

/* This is an explicit measurement, not a replacement/recovery I2C driver.
 * OPT3001 permits >=10kHz;25us half periods target20kHz. Interrupts remain
 * enabled. A scheduling gap>10ms aborts, rather than claiming valid timing.
 * OPT3001's28ms SCL-low reset cannot be prevented while this task is preempted. */
#define ABB_HALF_US 25U
#define ABB_RISE_TIMEOUT_US 1000U
#define ABB_GAP_LIMIT_US 10000U
#define ABB_OPERATION_LIMIT_US 25000U
#define ABB_SCL 1U
#define ABB_SDA 2U
/* RM0090 section27.6.1: PE=0 clears ACK/POS/START/PEC/ALERT in hardware.
 * START/PEC/ALERT are rejected before takeover because replaying a command or
 * SMBus alert is outside this diagnostic. ACK/POS are saved receive policy. */
#define ABB_CR1_PE_CLEARED (I2C_CR1_PE|I2C_CR1_ACK|I2C_CR1_POS|I2C_CR1_START|I2C_CR1_PEC|I2C_CR1_ALERT)
#define ABB_CR1_RECEIVE_POLICY (I2C_CR1_ACK|I2C_CR1_POS)
volatile BSP_AmbientBitbang_Diagnostics g_bsp_ambient_bitbang;
/* Separate scratch keeps command4 from replacing command3 evidence. The shared
 * bus claim protects this pointer and all electrical state across both APIs. */
static volatile BSP_AmbientBitbang_Diagnostics abb_address_detail;
static volatile BSP_AmbientBitbang_Diagnostics *abb_diag=&g_bsp_ambient_bitbang;
#define ABB_DIAG (*abb_diag)
_Static_assert(sizeof(BSP_AmbientBitbang_Diagnostics)==BSP_AMBIENT_BITBANG_BYTES,
               "bitbang diagnostic wire ABI");
_Static_assert(offsetof(BSP_AmbientBitbang_Diagnostics,first_pre_lines)==184U,
               "version1 bitbang diagnostic prefix");
_Static_assert(offsetof(BSP_AmbientBitbang_Diagnostics,pullup_mode)==208U,
               "version2 bitbang diagnostic prefix");

typedef struct {
    uint32_t cycles_per_us,start,last,last_tick,start_tick;
    uint32_t scl_low,low_start,low_tick,taken,operation_limit_us;
} ABB_Run;
static ABB_Run abb;
static uint32_t abb_busy;

/* The host fixture replaces only electrical readback/cycle progression. All
 * register masks, transactions, restoration and failure paths are production C. */
#ifdef BSP_AMBIENT_BITBANG_TEST
extern uint32_t ABB_TestCycles(void);
extern void ABB_TestWrite(uint32_t line,uint32_t released);
extern uint32_t ABB_TestRead(void);
extern void ABB_TestCR1Write(uint32_t value);
#endif
/* Keep the peripheral's PE-off side effect represented at the fixture boundary
 * instead of treating CR1 as ordinary RAM. Production performs one MMIO write. */
static void ABB_CR1Write(uint32_t value)
{
    I2C3->CR1=value;
#ifdef BSP_AMBIENT_BITBANG_TEST
    ABB_TestCR1Write(value);
#endif
}
static uint32_t ABB_Cycles(void)
{
#ifdef BSP_AMBIENT_BITBANG_TEST
    return ABB_TestCycles();
#else
    return DWT->CYCCNT;
#endif
}

/* Read IDR, never infer the actual line from ODR. Open-drain HIGH releases the
 * wire. Normal diagnostics retain board bias; the explicit variant temporarily
 * adds only PC9's weak pull-up. This sample itself never changes either pull. */
static uint32_t ABB_Lines(void)
{
#ifdef BSP_AMBIENT_BITBANG_TEST
    uint32_t lines=ABB_TestRead();
#else
    uint32_t lines=((GPIOH->IDR>>7U)&1U)|(((GPIOC->IDR>>9U)&1U)<<1U);
#endif
    ABB_DIAG.last_lines=lines;
    if(lines&ABB_SCL)++ABB_DIAG.scl_high_checks;
    else ++ABB_DIAG.scl_low_checks;
    if(lines&ABB_SDA)++ABB_DIAG.sda_high_checks;
    else ++ABB_DIAG.sda_low_checks;
    return lines;
}

/* One timestamp immediately before a digital sample. This deliberately does
 * not wait for a desired value or alter the clock/data outputs. The diagnostic
 * sequence is odd,so the host cannot accept a partially filled sample pair. */
static uint32_t ABB_Observe(volatile uint32_t *at_us)
{
    *at_us=(ABB_Cycles()-abb.start)/abb.cycles_per_us;
    return ABB_Lines();
}

/* Prefer CYCCNT resolution. Tick catches a preemption longer than the32-bit
 * DWT wrap. Tick is used only for a large gap, avoiding1ms quantization of
 * ordinary25us half periods. No CYCCNT reset affects the CPU-load observer. */
static uint32_t ABB_Duration(uint32_t now,uint32_t before,uint32_t now_ms,
                             uint32_t before_ms)
{
    uint32_t us=(now-before)/abb.cycles_per_us;
    uint32_t ms=now_ms-before_ms;
    if(ms>10U && ms>us/1000U)us=ms>0xFFFFFFFFU/1000U?0xFFFFFFFFU:ms*1000U;
    return us;
}
static uint32_t ABB_Time(uint32_t *now)
{
    *now=ABB_Cycles();uint32_t tick=HAL_GetTick();
    uint32_t gap=ABB_Duration(*now,abb.last,tick,abb.last_tick);
    if(gap>ABB_DIAG.max_gap_us)ABB_DIAG.max_gap_us=gap;
    abb.last=*now;abb.last_tick=tick;
    if(gap>ABB_GAP_LIMIT_US ||
       ABB_Duration(*now,abb.start,tick,abb.start_tick)>abb.operation_limit_us) {
        ABB_DIAG.timing_uncertain=1U;
        return BSP_ABB_TIMING_UNCERTAIN;
    }
    return BSP_ABB_OK;
}

/* BSRR is atomic and does not change any other GPIO pin. The low-duration
 * observer also runs during unconditional cleanup after a timing failure. */
static void ABB_Write(uint32_t line,uint32_t released)
{
    if(line==ABB_SCL) {
        uint32_t now=ABB_Cycles(),tick=HAL_GetTick();
        if(!released && !abb.scl_low) {
            abb.scl_low=1U;abb.low_start=now;abb.low_tick=tick;
        }
        else if(released && abb.scl_low) {
            uint32_t low=ABB_Duration(now,abb.low_start,tick,abb.low_tick);
            if(low>ABB_DIAG.max_low_us)ABB_DIAG.max_low_us=low;
            if(low>=28000U)ABB_DIAG.timing_uncertain=1U;
            abb.scl_low=0U;
        }
    }
    GPIO_TypeDef *port=line==ABB_SCL?GPIOH:GPIOC;
    uint32_t pin=line==ABB_SCL?GPIO_PIN_7:GPIO_PIN_9;
    port->BSRR=released?pin:(pin<<16U);
#ifdef BSP_AMBIENT_BITBANG_TEST
    ABB_TestWrite(line,released);
#endif
}

/* A finite iteration guard additionally catches a stopped/broken DWT even if
 * the HAL tick also stops. Its failure is CLOCK, not a fabricated wire NACK. */
static uint32_t ABB_Delay(void)
{
    uint32_t start=ABB_Cycles(),now=start;
    for(uint32_t spins=0U;spins<4096U;++spins) {
        uint32_t result=ABB_Time(&now);if(result)return result;
        if((now-start)>=ABB_HALF_US*abb.cycles_per_us)return BSP_ABB_OK;
    }
    ABB_DIAG.timing_uncertain=1U;return BSP_ABB_CLOCK;
}

/* Every rising edge waits for actual IDR HIGH. A held-low clock is reported
 * after1ms; no recovery pulse train is sent. The caller releases both wires. */
static uint32_t ABB_High(void)
{
    ABB_Write(ABB_SCL,1U);uint32_t start=ABB_Cycles(),now=start;
    for(uint32_t spins=0U;spins<4096U;++spins) {
        uint32_t result=ABB_Time(&now);if(result)return result;
        if(ABB_Lines()&ABB_SCL)return BSP_ABB_OK;
        if((now-start)/abb.cycles_per_us>=ABB_RISE_TIMEOUT_US)return BSP_ABB_SCL_TIMEOUT;
    }
    ABB_DIAG.timing_uncertain=1U;return BSP_ABB_CLOCK;
}
static uint32_t ABB_Low(void)
{
    uint32_t now,result=ABB_Time(&now);if(result)return result;
    ABB_Write(ABB_SCL,0U);
    uint32_t start=ABB_Cycles();
    /* Allow input synchronizer/board settling after BSRR instead of treating
     * the first stale HIGH sample as a wire that cannot be driven LOW. */
    for(uint32_t spins=0U;spins<4096U;++spins) {
        if((result=ABB_Time(&now)))return result;
        if(!(ABB_Lines()&ABB_SCL))return BSP_ABB_OK;
        if((now-start)/abb.cycles_per_us>=ABB_HALF_US)return BSP_ABB_SCL_TIMEOUT;
    }
    ABB_DIAG.timing_uncertain=1U;return BSP_ABB_CLOCK;
}

/* Only begin with measured idle HIGH. A repeated START first releases SDA
 * while SCL is LOW, raises SCL, then asserts SDA LOW. Neither path scans. */
static uint32_t ABB_Start(uint32_t repeated)
{
    ABB_DIAG.phase=repeated?BSP_ABB_PHASE_RESTART:BSP_ABB_PHASE_START;
    uint32_t result;
    if(repeated) {
        ABB_Write(ABB_SDA,1U);if((result=ABB_Delay()))return result;
        if((result=ABB_High()))return result;
    }
    if(ABB_Lines()!=3U)return BSP_ABB_BUS_BUSY;
    if((result=ABB_Delay()))return result;
    ABB_Write(ABB_SDA,0U);
    if((result=ABB_Delay()))return result;
    if(ABB_Lines()&ABB_SDA)return BSP_ABB_SDA_CONFLICT;
    return ABB_Low();
}

/* STOP is emitted only for a transaction we own and a non-timing/non-arbitration
 * failure. It contains no extra clock pulses. A conflict/timeout instead only
 * releases the two open-drain outputs during cleanup. */
static uint32_t ABB_Stop(void)
{
    ABB_DIAG.phase=BSP_ABB_PHASE_STOP;
    ABB_Write(ABB_SDA,0U);uint32_t result=ABB_Delay();if(result)return result;
    if((result=ABB_High()))return result;
    if((result=ABB_Delay()))return result;
    ABB_Write(ABB_SDA,1U);
    if((result=ABB_Delay()))return result;
    return ABB_Lines()==3U?BSP_ABB_OK:BSP_ABB_SDA_CONFLICT;
}

/* Transmit address/register-pointer bytes MSB-first. HIGH data is sampled
 * during SCL HIGH to detect another driver pulling it LOW. The ninth clock
 * samples a separate ACK; released SDA is not mistaken for an asserted ACK. */
static uint32_t ABB_Send(uint8_t data,uint32_t phase)
{
    ABB_DIAG.phase=phase;++ABB_DIAG.byte_index;
    for(uint32_t bit=0U;bit<8U;++bit) {
        ABB_DIAG.bit_index=bit;
        uint32_t released=(data&0x80U)!=0U;data<<=1U;
        ABB_Write(ABB_SDA,released);uint32_t result=ABB_Delay();if(result)return result;
        uint32_t first=ABB_DIAG.byte_index==1U && bit==0U;
        if(first)ABB_DIAG.first_pre_lines=
            ABB_Observe(&ABB_DIAG.first_pre_us);
        if((result=ABB_High()))return result;
        uint32_t lines;
        if(first) {
            lines=ABB_Observe(&ABB_DIAG.first_early_us);
            ABB_DIAG.first_early_lines=lines;
        }
        else lines=ABB_Lines();
        if(((lines&ABB_SDA)!=0U)!=released) {
            if(first) {
                /* Observe through the same25us HIGH period used by normal
                 * bits. Do not retry/reinterpret the bit: a LOW->HIGH during
                 * SCL HIGH may look like STOP to the sensor. If preempted,
                 * Delay marks timing_uncertain and the late timestamp exposes
                 * that longer gap; the initial conflict remains result6. */
                (void)ABB_Delay();
                ABB_DIAG.first_late_lines=
                    ABB_Observe(&ABB_DIAG.first_late_us);
            }
            return BSP_ABB_SDA_CONFLICT;
        }
        if((result=ABB_Delay()))return result;
        if((result=ABB_Low()))return result;
    }
    ABB_Write(ABB_SDA,1U);uint32_t result=ABB_Delay();if(result)return result;
    if((result=ABB_High()))return result;
    uint32_t ack=(ABB_Lines()&ABB_SDA)==0U;
    uint32_t index=ABB_DIAG.ack_count++;
    if(ack)ABB_DIAG.ack_mask|=1UL<<index;
    if((result=ABB_Delay()))return result;
    if((result=ABB_Low()))return result;
    return ack?BSP_ABB_OK:BSP_ABB_NACK;
}

/* Two-byte OPT3001 words: ACK after first byte, NACK after second, then STOP.
 * The final NACK is checked HIGH on IDR and is not counted as a sensor ACK. */
static uint32_t ABB_Receive(uint8_t *out,uint32_t last)
{
    ABB_DIAG.phase=last?BSP_ABB_PHASE_READ_LOW:BSP_ABB_PHASE_READ_HIGH;
    ++ABB_DIAG.byte_index;uint8_t value=0U;
    ABB_Write(ABB_SDA,1U);
    for(uint32_t bit=0U;bit<8U;++bit) {
        ABB_DIAG.bit_index=bit;
        uint32_t result=ABB_Delay();if(result)return result;
        if((result=ABB_High()))return result;
        value=(uint8_t)((value<<1U)|((ABB_Lines()&ABB_SDA)!=0U));
        if((result=ABB_Delay()))return result;
        if((result=ABB_Low()))return result;
    }
    ABB_Write(ABB_SDA,last);uint32_t result=ABB_Delay();if(result)return result;
    if((result=ABB_High()))return result;
    if(((ABB_Lines()&ABB_SDA)!=0U)!=(last!=0U))return BSP_ABB_SDA_CONFLICT;
    if((result=ABB_Delay()))return result;
    if((result=ABB_Low()))return result;
    ABB_Write(ABB_SDA,1U);*out=value;return BSP_ABB_OK;
}

/* Private identity read. Callers are the fixed0x45 path or the bounded four
 * TI strap candidates below; register arguments are only7E/7F. No generic
 * register-write API and no configuration-data byte is emitted. */
static uint32_t ABB_ReadWord(uint8_t address,uint8_t reg,uint32_t *out)
{
    ABB_DIAG.reg=reg;uint32_t result=ABB_Start(0U);
    if(result)return result;
    if((result=ABB_Send((uint8_t)(address<<1U),BSP_ABB_PHASE_ADDRESS_WRITE)))return result;
    if((result=ABB_Send(reg,BSP_ABB_PHASE_REGISTER)))return result;
    if((result=ABB_Start(1U)))return result;
    if((result=ABB_Send((uint8_t)((address<<1U)|1U),BSP_ABB_PHASE_ADDRESS_READ)))return result;
    uint8_t high,low;
    if((result=ABB_Receive(&high,0U)))return result;
    if((result=ABB_Receive(&low,1U)))return result;
    *out=((uint32_t)high<<8U)|low;
    result=ABB_Stop();ABB_DIAG.stop_result=result;return result;
}

/* Snapshot only the two owned pins. Other pins on these ports can legitimately
 * change while interrupts/tasks run, so no whole GPIO register is restored. */
static uint32_t ABB_Pin(GPIO_TypeDef *port,uint32_t pin)
{
    return ((port->MODER>>(pin*2U))&3U)|(((port->OTYPER>>pin)&1U)<<2U)|
           (((port->OSPEEDR>>(pin*2U))&3U)<<3U)|(((port->PUPDR>>(pin*2U))&3U)<<5U)|
           (((port->AFR[pin/8U]>>((pin%8U)*4U))&15U)<<7U)|(((port->ODR>>pin)&1U)<<11U);
}
static uint32_t ABB_PinExpected(uint32_t pin)
{
    return (pin&((3U)|(1U<<2U)|(3U<<5U)|(15U<<7U)))==(2U|(1U<<2U)|(4U<<7U));
}

/* The short critical section covers pin-register RMW only. Keep PE disabled
 * through restoration. Switch back to AF before restoring an old LOW ODR;
 * doing that in OUTPUT mode would pull a wire LOW during cleanup. */
static void ABB_Restore(void)
{
    ABB_DIAG.phase=BSP_ABB_PHASE_RESTORE;
    ABB_Write(ABB_SCL,1U);ABB_Write(ABB_SDA,1U);
    uint32_t ph=ABB_Pin(GPIOH,7U),pc=ABB_Pin(GPIOC,9U);
    uint32_t pinmask=~(3U|(1U<<11U));
    uint32_t expected_pc=ABB_DIAG.saved_pc9;
    if(ABB_DIAG.pullup_mode)
        expected_pc=(expected_pc&~(3U<<5U))|(1U<<5U);
    uint32_t clean=(ph&3U)==1U && (pc&3U)==1U &&
        (ph&pinmask)==(ABB_DIAG.saved_ph7&pinmask) &&
        (pc&pinmask)==(expected_pc&pinmask) &&
        I2C3->CR1==(ABB_DIAG.saved_cr1&~ABB_CR1_PE_CLEARED) &&
        I2C3->CR2==ABB_DIAG.saved_cr2 &&
        I2C3->CCR==ABB_DIAG.saved_ccr &&
        I2C3->TRISE==ABB_DIAG.saved_trise &&
        I2C3->FLTR==ABB_DIAG.saved_fltr;
    uint32_t key=__get_PRIMASK();__disable_irq();
    GPIOH->MODER=(GPIOH->MODER&~(3UL<<14U))|((ABB_DIAG.saved_ph7&3U)<<14U);
    GPIOC->MODER=(GPIOC->MODER&~(3UL<<18U))|((ABB_DIAG.saved_pc9&3U)<<18U);
    /* Restore only the PC9 pull bits owned by the explicit experiment. Do so
     * with the output already back in AF and PE still off. An unexpected pull
     * observed above still quarantines I2C even if restoration succeeds. */
    if(ABB_DIAG.pullup_mode)
        GPIOC->PUPDR=(GPIOC->PUPDR&~(3UL<<18U))|
            (((ABB_DIAG.saved_pc9>>5U)&3U)<<18U);
    ABB_Write(ABB_SCL,(ABB_DIAG.saved_ph7>>11U)&1U);
    ABB_Write(ABB_SDA,(ABB_DIAG.saved_pc9>>11U)&1U);
    /* Timing registers were never modified; a foreign mismatch is reported,
     * not overwritten. ACK/POS were cleared by our PE=0, so restore PE first
     * then those saved policy bits. Writing ACK while PE remains0 cannot work.
     * No START/STOP/PEC/ALERT command is replayed. */
    ABB_CR1Write((I2C3->CR1&~I2C_CR1_PE)|
        (clean?(ABB_DIAG.saved_cr1&I2C_CR1_PE):0U));
    if(clean && (ABB_DIAG.saved_cr1&I2C_CR1_PE)) {
        __DSB();
        ABB_CR1Write((I2C3->CR1&~ABB_CR1_RECEIVE_POLICY)|
            (ABB_DIAG.saved_cr1&ABB_CR1_RECEIVE_POLICY));
    }
    __DSB();__set_PRIMASK(key);
    ABB_DIAG.final_ph7=ABB_Pin(GPIOH,7U);
    ABB_DIAG.final_pc9=ABB_Pin(GPIOC,9U);
    ABB_DIAG.final_cr1=I2C3->CR1;ABB_DIAG.final_cr2=I2C3->CR2;
    ABB_DIAG.final_ccr=I2C3->CCR;ABB_DIAG.final_trise=I2C3->TRISE;
    ABB_DIAG.final_fltr=I2C3->FLTR;
    ABB_DIAG.restore_result=!(clean &&
        ABB_DIAG.final_ph7==ABB_DIAG.saved_ph7 &&
        ABB_DIAG.final_pc9==ABB_DIAG.saved_pc9 &&
        ABB_DIAG.final_cr1==ABB_DIAG.saved_cr1);
    /* A mismatched mux/timing/IRQ configuration must not resume hardware I2C.
     * Keep the HAL lock and PE off until an explicit Probe deinitializes it. */
    if(ABB_DIAG.restore_result)ABB_CR1Write(I2C3->CR1&~I2C_CR1_PE);
    else hi2c3.Lock=HAL_UNLOCKED;
    ABB_DIAG.final_cr1=I2C3->CR1;
}

/* Publish the separate fixed-address record only after the shared bus claim.
 * Unvisited rows have attempted0,so a partial batch cannot look like4NACKs. */
static uint32_t ABB_AddressBegin(uint32_t id,uint32_t request_seq)
{
    uint32_t seq=(g_bsp_ambient_address.sequence&~1U)+1U;if(!seq)seq=1U;
    g_bsp_ambient_address.sequence=seq;__DMB();
    memset((void *)&g_bsp_ambient_address,0,12U);
    memset((void *)&g_bsp_ambient_address.operation_id,0,sizeof(g_bsp_ambient_address)-16U);
    g_bsp_ambient_address.magic=BSP_AMBIENT_ADDRESS_MAGIC;
    g_bsp_ambient_address.version=BSP_AMBIENT_ADDRESS_VERSION;
    g_bsp_ambient_address.bytes=BSP_AMBIENT_ADDRESS_BYTES;
    g_bsp_ambient_address.operation_id=id;g_bsp_ambient_address.request_seq=request_seq;
    g_bsp_ambient_address.restore_result=2U;g_bsp_ambient_address.pullup_mode=1U;
    for(uint32_t i=0U;i<BSP_AMBIENT_ADDRESS_COUNT;++i) {
        g_bsp_ambient_address.entry[i].address=0x44U+i;
        g_bsp_ambient_address.entry[i].result=BSP_ABB_PRECONDITION;
        g_bsp_ambient_address.entry[i].stop_result=2U;
    }
    return seq;
}

/* An address-only exchange is always terminated before an optional ID read.
 * NACK cleanup failure becomes a line failure and ends the entire batch.
 * No arbitrary address is accepted: the private caller supplies index0..3. */
static uint32_t ABB_AddressCandidate(uint32_t index,volatile BSP_AmbientAddress_Entry *row)
{
    if(index>=BSP_AMBIENT_ADDRESS_COUNT)return BSP_ABB_PRECONDITION;
    uint8_t address=(uint8_t)(0x44U+index);
    uint32_t begin=ABB_Cycles(),begin_ms=HAL_GetTick(),failed_phase=0U,word=0U;
    ABB_DIAG.stop_result=2U;
    uint32_t result=ABB_Start(0U);
    if(!result)result=ABB_Send((uint8_t)(address<<1U),BSP_ABB_PHASE_ADDRESS_WRITE);
    if(result)failed_phase=ABB_DIAG.phase;
    else row->address_ack=1U;
    if(result==BSP_ABB_OK || result==BSP_ABB_NACK) {
        uint32_t stop=ABB_Stop();ABB_DIAG.stop_result=stop;
        if(stop){result=stop;failed_phase=BSP_ABB_PHASE_STOP;}
    }
    if(result)goto finish;
    result=ABB_ReadWord(address,0x7EU,&word);
    if(!result) {
        row->manufacturer=word;
        result=ABB_ReadWord(address,0x7FU,&word);
        if(!result) {
            row->device=word;row->ids_valid=1U;
            if(row->manufacturer!=0x5449U || word!=0x3001U)
                result=BSP_ABB_ID_MISMATCH;
        }
    }
    if(result)failed_phase=ABB_DIAG.phase;
    if(result==BSP_ABB_NACK) {
        uint32_t stop=ABB_Stop();ABB_DIAG.stop_result=stop;
        if(stop){result=stop;failed_phase=BSP_ABB_PHASE_STOP;}
    }
finish:
    row->result=result;row->phase=result?failed_phase:BSP_ABB_PHASE_COMPLETE;
    row->stop_result=ABB_DIAG.stop_result;
    row->elapsed_us=ABB_Duration(ABB_Cycles(),begin,HAL_GetTick(),begin_ms);
    return result;
}

/* Copy aggregate restoration/timing evidence from the private scratch only
 * after the bus has been restored. Preserve the independent cmd3 record. */
static void ABB_AddressFinish(uint32_t seq)
{
#define ABB_COPY(field) g_bsp_ambient_address.field=ABB_DIAG.field
    ABB_COPY(result);ABB_COPY(restore_result);ABB_COPY(elapsed_us);
    ABB_COPY(timing_uncertain);ABB_COPY(completed_ms);
    ABB_COPY(saved_cr1);ABB_COPY(final_cr1);ABB_COPY(saved_cr2);ABB_COPY(final_cr2);
    ABB_COPY(saved_ccr);ABB_COPY(final_ccr);ABB_COPY(saved_trise);ABB_COPY(final_trise);
    ABB_COPY(saved_fltr);ABB_COPY(final_fltr);ABB_COPY(saved_ph7);ABB_COPY(final_ph7);
    ABB_COPY(saved_pc9);ABB_COPY(final_pc9);ABB_COPY(pin_changes);ABB_COPY(pullup_mode);
    ABB_COPY(max_gap_us);ABB_COPY(max_low_us);
#undef ABB_COPY
    uint32_t complete=seq+1U;if(!complete)complete=2U;
    __DMB();g_bsp_ambient_address.sequence=complete;
}

static uint32_t ABB_RunIDs(uint32_t operation_id,uint32_t request_seq,uint32_t pullup_mode,
                           uint32_t address_mode)
{
    /* Reject reentry before touching the running transaction or its evidence.
     * The entire wire transaction remains interruptible after this claim. */
    if(__get_IPSR() || __get_PRIMASK() || __get_BASEPRI() || __get_FAULTMASK() ||
       (__get_CONTROL()&1U))return BSP_ABB_CONTEXT;
    uint32_t claim=__get_PRIMASK();__disable_irq();
    if(abb_busy){__set_PRIMASK(claim);return BSP_ABB_PRECONDITION;}
    abb_busy=1U;__set_PRIMASK(claim);
    abb_diag=address_mode?&abb_address_detail:&g_bsp_ambient_bitbang;
    uint32_t address_seq=address_mode?ABB_AddressBegin(operation_id,request_seq):0U;
    uint32_t previous=ABB_DIAG.sequence;
    uint32_t seq=(previous&~1U)+1U;if(!seq)seq=1U;
    /* Invalidate before clearing the remaining bytes; a reader can never accept
     * an old even sequence around a partially initialized new result. */
    ABB_DIAG.sequence=seq;__DMB();
    memset((void *)&ABB_DIAG,0,12U);
    memset((void *)&ABB_DIAG.operation_id,0,sizeof(ABB_DIAG)-16U);
    memset(&abb,0,sizeof(abb));
    abb.operation_limit_us=address_mode?100000U:ABB_OPERATION_LIMIT_US;
    ABB_DIAG.magic=BSP_AMBIENT_BITBANG_MAGIC;
    ABB_DIAG.version=BSP_AMBIENT_BITBANG_VERSION;
    ABB_DIAG.bytes=BSP_AMBIENT_BITBANG_BYTES;
    ABB_DIAG.operation_id=operation_id;ABB_DIAG.request_seq=request_seq;
    ABB_DIAG.pullup_mode=pullup_mode;
    ABB_DIAG.restore_result=2U;ABB_DIAG.stop_result=2U;
    ABB_DIAG.phase=BSP_ABB_PHASE_PRECHECK;
    uint32_t result=BSP_ABB_PRECONDITION;
    if(!operation_id || hi2c3.Instance!=I2C3 || hi2c3.State!=HAL_I2C_STATE_READY ||
       hi2c3.Lock!=HAL_UNLOCKED || !(RCC->APB1ENR&RCC_APB1ENR_I2C3EN) ||
       (RCC->AHB1ENR&(RCC_AHB1ENR_GPIOCEN|RCC_AHB1ENR_GPIOHEN))!=
         (RCC_AHB1ENR_GPIOCEN|RCC_AHB1ENR_GPIOHEN))goto finish;
    ABB_DIAG.saved_ph7=ABB_Pin(GPIOH,7U);
    ABB_DIAG.saved_pc9=ABB_Pin(GPIOC,9U);
    ABB_DIAG.saved_cr1=I2C3->CR1;ABB_DIAG.saved_cr2=I2C3->CR2;
    ABB_DIAG.saved_ccr=I2C3->CCR;ABB_DIAG.saved_trise=I2C3->TRISE;
    ABB_DIAG.saved_fltr=I2C3->FLTR;
    if(!ABB_PinExpected(ABB_DIAG.saved_ph7) ||
       !ABB_PinExpected(ABB_DIAG.saved_pc9) ||
       (I2C3->CR1&(I2C_CR1_START|I2C_CR1_STOP|I2C_CR1_SWRST|I2C_CR1_PEC|I2C_CR1_ALERT)) ||
       (I2C3->CR2&(I2C_CR2_DMAEN|I2C_CR2_ITEVTEN|I2C_CR2_ITBUFEN|I2C_CR2_ITERREN)))goto finish;
    ABB_DIAG.clock_hz=SystemCoreClock;
    if(!(DWT->CTRL&DWT_CTRL_CYCCNTENA_Msk) || SystemCoreClock<1000000U ||
       SystemCoreClock>180000000U){result=BSP_ABB_CLOCK;goto finish;}
    abb.cycles_per_us=SystemCoreClock/1000000U;
    abb.start=abb.last=ABB_Cycles();abb.start_tick=abb.last_tick=HAL_GetTick();
    if((result=ABB_Delay()))goto finish;
    ABB_DIAG.initial_lines=ABB_Lines();
    if(ABB_DIAG.initial_lines!=3U){result=BSP_ABB_BUS_BUSY;goto finish;}

    /* Existing HAL transfer is idle; claim its lock before PE/pin mux changes.
     * Preload ODR HIGH while still AF, then switch only MODE to output. */
    hi2c3.Lock=HAL_LOCKED;
    uint32_t key=__get_PRIMASK();__disable_irq();
    ABB_CR1Write(I2C3->CR1&~I2C_CR1_PE);
    ABB_Write(ABB_SCL,1U);ABB_Write(ABB_SDA,1U);
    /* Both public variants require the original AF4/OD/NOPULL baseline. Only
     * this explicit variant adds weak bias to PC9; no push-pull or PH7 change.
     * Keep unrelated pins' concurrently used PUPDR fields intact. */
    if(pullup_mode)GPIOC->PUPDR=(GPIOC->PUPDR&~(3UL<<18U))|(1UL<<18U);
    GPIOH->MODER=(GPIOH->MODER&~(3UL<<14U))|(1UL<<14U);
    GPIOC->MODER=(GPIOC->MODER&~(3UL<<18U))|(1UL<<18U);
    __DSB();__set_PRIMASK(key);abb.taken=1U;ABB_DIAG.pin_changes=1U;
    ABB_DIAG.phase=BSP_ABB_PHASE_IDLE;
    if(ABB_Lines()!=3U){result=BSP_ABB_BUS_BUSY;goto restore;}
    if(address_mode) {result=BSP_AmbientAddress_Execute(ABB_AddressCandidate);goto restore;}
    uint32_t word=0U;
    result=ABB_ReadWord(0x45U,0x7EU,&word);
    if(!result) {
        ABB_DIAG.manufacturer=word;
        result=ABB_ReadWord(0x45U,0x7FU,&word);
        if(!result) {
            ABB_DIAG.device=word;ABB_DIAG.ids_valid=1U;
            if(ABB_DIAG.manufacturer!=0x5449U || word!=0x3001U)
                result=BSP_ABB_ID_MISMATCH;
        }
    }
restore:
    if(result)ABB_DIAG.failed_phase=ABB_DIAG.phase;
    if(!address_mode && result==BSP_ABB_NACK)ABB_DIAG.stop_result=ABB_Stop();
    else if(!address_mode && (result==BSP_ABB_OK || result==BSP_ABB_ID_MISMATCH))
        ABB_DIAG.stop_result=BSP_ABB_OK;
    ABB_Restore();
    if(!result && ABB_DIAG.restore_result)result=BSP_ABB_RESTORE_FAILED;
finish:
    if(result && !ABB_DIAG.failed_phase)
        ABB_DIAG.failed_phase=ABB_DIAG.phase;
    if(abb.cycles_per_us)ABB_DIAG.elapsed_us=
        ABB_Duration(ABB_Cycles(),abb.start,HAL_GetTick(),abb.start_tick);
    ABB_DIAG.completed_ms=HAL_GetTick();
    ABB_DIAG.result=result;ABB_DIAG.phase=BSP_ABB_PHASE_COMPLETE;
    /* sequence0 remains invalid, including a counter wrap after2^31 runs. */
    uint32_t complete=seq+1U;if(!complete)complete=2U;
    __DMB();ABB_DIAG.sequence=complete;
    if(address_mode)ABB_AddressFinish(address_seq);
    abb_diag=&g_bsp_ambient_bitbang;
    claim=__get_PRIMASK();__disable_irq();abb_busy=0U;__set_PRIMASK(claim);
    return result;
}

uint32_t BSP_AmbientBitbang_ReadIDs(uint32_t operation_id,uint32_t request_seq)
{
    return ABB_RunIDs(operation_id,request_seq,0U,0U);
}
uint32_t BSP_AmbientBitbang_ReadIDsWithPullup(uint32_t operation_id,uint32_t request_seq)
{
    return ABB_RunIDs(operation_id,request_seq,1U,0U);
}

/* Private engine entry used only by the fixed-candidate public facade. */
uint32_t BSP_AmbientBitbang_RunAddressDiagnostic(uint32_t id,uint32_t sequence)
{
    return ABB_RunIDs(id,sequence,1U,1U);
}
