#ifndef BSP_AMBIENT_ADDRESS_H
#define BSP_AMBIENT_ADDRESS_H
#include <stdint.h>
#include "BSP_AmbientBitbang.h"

#define BSP_AMBIENT_ADDRESS_MAGIC 0x41424131U
#define BSP_AMBIENT_ADDRESS_VERSION 1U
#define BSP_AMBIENT_ADDRESS_BYTES 288U
#define BSP_AMBIENT_ADDRESS_COUNT 4U

/* One fixed TI ADDR strap candidate. attempted distinguishes an unvisited row
 * from an actual precondition failure. address_ack describes only the initial
 * address-only transaction; an ACK does not identify the responding IC.
 * ids_valid means both words were read,including a non-OPT3001 ID mismatch.
 * phase is the original failure phase,or COMPLETE for a valid matched pair.
 * stop_result=2 means no STOP was attempted. All durations are microseconds. */
typedef struct {
    uint32_t address,attempted,address_ack,result,phase;
    uint32_t manufacturer,device,ids_valid,elapsed_us,stop_result;
} BSP_AmbientAddress_Entry;

/* 72 little-endian u32 words. sequence is odd throughout the single takeover
 * and all four candidates; the producer commits a nonzero even sequence LAST
 * after restoring pin/I2C state. Match operation_id/request_seq with cmd4's
 * completed124-byte mailbox and require equal even seq before/after reading.
 * Masks use bit0..3 for0x44..0x47. At least one ID match is needed for result0;
 * a later line/timing/STOP failure still overrides earlier matches and aborts.
 * restore_result has the same0/1/2 meaning as the original ID diagnostic.
 * The original212-byte ID-only diagnostic remains unchanged by this API. */
typedef struct {
    uint32_t magic,version,bytes,sequence,operation_id,request_seq,result,restore_result;
    uint32_t attempted_mask,address_ack_mask,id_match_mask,elapsed_us,timing_uncertain,completed_ms;
    BSP_AmbientAddress_Entry entry[BSP_AMBIENT_ADDRESS_COUNT];
    uint32_t saved_cr1,final_cr1,saved_cr2,final_cr2,saved_ccr,final_ccr;
    uint32_t saved_trise,final_trise,saved_fltr,final_fltr;
    uint32_t saved_ph7,final_ph7,saved_pc9,final_pc9;
    uint32_t pin_changes,pullup_mode,max_gap_us,max_low_us;
} BSP_AmbientAddress_Diagnostics;
extern volatile BSP_AmbientAddress_Diagnostics g_bsp_ambient_address;

/* AmbientService owner only. One explicit nominal20kHz diagnostic with PC9
 * weak pull-up during the transaction,restoring original AF4/OD/NOPULL. PH7
 * and every other pin's pull are unchanged. Fixed0x44,45,46,47 only; no caller
 * address/register arguments. START+addressW+ACK+STOP first,then registers7E
 * and7F only at an ACKed candidate. No sensor config/general-call/power writes.
 * NACK and complete wrong IDs permit the next candidate. Electrical/timing or
 * STOP failure aborts the batch. <=100ms active budget,10ms scheduling-gap
 * limit; preemption itself cannot be bounded. Upper callers use the service. */
uint32_t BSP_AmbientAddress_ReadFixedCandidates(uint32_t operation_id,uint32_t request_seq);
#endif
