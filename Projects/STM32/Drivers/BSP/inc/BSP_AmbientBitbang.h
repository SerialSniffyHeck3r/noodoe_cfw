#ifndef BSP_AMBIENT_BITBANG_H
#define BSP_AMBIENT_BITBANG_H
#include <stdint.h>

#define BSP_AMBIENT_BITBANG_MAGIC 0x41424231U
#define BSP_AMBIENT_BITBANG_VERSION 3U
#define BSP_AMBIENT_BITBANG_BYTES 212U
#define BSP_AMBIENT_BITBANG_HZ 20000U

typedef enum {
    BSP_ABB_OK=0, BSP_ABB_CONTEXT, BSP_ABB_PRECONDITION, BSP_ABB_CLOCK,
    BSP_ABB_BUS_BUSY, BSP_ABB_SCL_TIMEOUT, BSP_ABB_SDA_CONFLICT,
    BSP_ABB_NACK, BSP_ABB_TIMING_UNCERTAIN, BSP_ABB_ID_MISMATCH,
    BSP_ABB_RESTORE_FAILED
} BSP_AmbientBitbang_Result;
typedef enum {
    BSP_ABB_PHASE_NONE=0, BSP_ABB_PHASE_PRECHECK, BSP_ABB_PHASE_IDLE,
    BSP_ABB_PHASE_START, BSP_ABB_PHASE_ADDRESS_WRITE, BSP_ABB_PHASE_REGISTER,
    BSP_ABB_PHASE_RESTART, BSP_ABB_PHASE_ADDRESS_READ, BSP_ABB_PHASE_READ_HIGH,
    BSP_ABB_PHASE_READ_LOW, BSP_ABB_PHASE_STOP, BSP_ABB_PHASE_RESTORE,
    BSP_ABB_PHASE_COMPLETE
} BSP_AmbientBitbang_Phase;

/* 53 little-endian u32 words; v1/v2's184/208-byte layouts are preserved. sequence
 * is odd while executing; the worker
 * publishes an even sequence LAST after restoring the pins. A host must read
 * sequence before/after the struct, require identical nonzero even values,
 * and match operation_id/request_seq to command3's completed124-byte mailbox.
 * Fresh IDs belong ONLY to this record; the normal HAL driver is unchanged.
 * result preserves the wire failure; restore_result is independent (0=exact
 * saved configuration,1=mismatch,2=not taken over). Both must be0 for success.
 * lines bit0=SCL/PH7,bit1=SDA/PC9; bit=1 means measured HIGH. ack_mask records
 * received ACKs in chronological bit order; six ACKs are required for two IDs.
 * pin words pack MODE[1:0],OTYPE[2],SPEED[4:3],PULL[6:5],AF[10:7],ODR[11].
 * IRQs remain enabled during clocking. timing_uncertain marks an excessive
 * scheduling gap; max_low_us>=28000 means the sensor's SCL-low timeout may
 * already have occurred. Firmware cannot bound preemption while not running. */
typedef struct {
    uint32_t magic,version,bytes,sequence,operation_id,request_seq,result,restore_result;
    uint32_t manufacturer,device,ids_valid,elapsed_us,phase,failed_phase,reg;
    uint32_t byte_index,bit_index,ack_count,ack_mask,initial_lines,last_lines;
    uint32_t scl_high_checks,scl_low_checks,sda_high_checks,sda_low_checks;
    uint32_t max_gap_us,timing_uncertain,clock_hz;
    uint32_t saved_cr1,final_cr1,saved_cr2,final_cr2,saved_ccr,final_ccr;
    uint32_t saved_trise,final_trise,saved_fltr,final_fltr;
    uint32_t saved_ph7,final_ph7,saved_pc9,final_pc9,pin_changes,stop_result;
    uint32_t completed_ms,max_low_us;
    /* Version2,offset184: first address byte's bit0 only. Each timestamp is us
     * since this diagnostic's DWT start;0 means that sample was not reached.
     * pre=25us setup elapsed,SCL not yet raised;early=SCL HIGH observed;
     * late=after the normal25us HIGH interval when early found a conflict.
     * A late HIGH cannot change the original conflict into success. Interrupt
     * latency is reported separately; these are digital samples,not a scope. */
    uint32_t first_pre_lines,first_pre_us,first_early_lines,first_early_us;
    uint32_t first_late_lines,first_late_us;
    /* Version3,offset208: requested variant0=normal NOPULL,1=one-shot PC9 UP.
     * A rejected precheck still reports1; use pin_changes/restore_result to
     * determine whether takeover occurred. PH7 is always NOPULL; final_pc9
     * must match the original NOPULL snapshot after a completed takeover. */
    uint32_t pullup_mode;
} BSP_AmbientBitbang_Diagnostics;
extern volatile BSP_AmbientBitbang_Diagnostics g_bsp_ambient_bitbang;

/* AmbientService worker only, privileged thread with interrupts enabled.
 * One diagnostic, fixed0x45/registers7E+7F, MSB-first two-byte reads. Register
 * pointer writes are necessary I2C reads; sensor configuration is never written.
 * Requires generated AF4/OD/NOPULL and inactive HAL/DMA/IRQ transfer. Temporarily
 * selects OUTPUT_OD on PH7/PC9 only, preserving pin bits, ODR and I2C3 timing/PE.
 * No scan, bus-clear train, supply GPIO, pull-up enable or automatic retry.
 * Nominal20kHz uses existing running DWT; actual preempted timing is reported.
 * A restore mismatch leaves PE off/HAL locked and prevents normal I2C work
 * until reprobed. Reentrant/context-rejected calls do not overwrite evidence. */
uint32_t BSP_AmbientBitbang_ReadIDs(uint32_t operation_id,uint32_t request_seq);
/* Same ID-only measurement with one explicit difference: temporarily select
 * PC9 PUPDR=UP,then restore its original NOPULL bits on every takeover exit.
 * PH7's pull and all unrelated pins remain untouched. This measures behavior
 * under added weak bias; success is not proof of the original pull-up network. */
uint32_t BSP_AmbientBitbang_ReadIDsWithPullup(uint32_t operation_id,uint32_t request_seq);
#endif
