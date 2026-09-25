#ifndef NOODOE_GATE_ABI_H
#define NOODOE_GATE_ABI_H
#include <stdint.h>
#define GATE_ABI_VERSION 1U
#define GATE_MAILBOX_ADDRESS 0x2002FF00U
#define GATE_MAILBOX_BYTES 256U
#define GATE_FLASH_BASE 0x08010000U
#define GATE_FLASH_BYTES 0x10000U
#define GATE_PRODUCT_BASE 0x08020000U
#define GATE_PRODUCT_BYTES 0x60000U
#define GATE_FEATURE_ADDRESS (GATE_FLASH_BASE+0x200U)
#define GATE_FEATURE_MAGIC 0x31544647U /* GFT1 */
#define GATE_FEATURE_DIAGNOSTIC 1U
/* Initial reinstall journal only: transaction/reset_epoch paired sentinel.
 * Existing Gate forwards these opaque fields without changing its ABI. */
#define GATE_RESTORE_RETAINED 0x52535452U
#define GATE_CONFIRM_MS 30000U
#define GATE_APP_CONFIRM_DEADLINE_MS 180000U
#define GATE_BOOT_FAILURE_LIMIT 3U
enum {GATE_REASON_NORMAL=0,GATE_REASON_WAIT=1,GATE_REASON_STOCK=2,
      GATE_REASON_FAULT=3,GATE_REASON_INSTALL=4,GATE_REASON_TRIAL_TIMEOUT=5,GATE_REASON_INIT_FAILED=6};
/* The first32 bytes carry the reset request. Remaining bytes are reserved,
 * excluded from both images' heaps/stacks and never evidence of durability. */
typedef struct {uint32_t magic,version,reason,sequence,attempts,confirmed,crc,inverse;} GateRetained;
/* Fault evidence has its own seal: request handoffs must not erase it. No NOR
 * access occurs in a fault handler. Cleared only after durable event readback. */
typedef struct {uint32_t magic,version,boot,code,registers[10],crc,inverse;} GateFault;
typedef struct {uint32_t magic,version,sequence,flags,transaction,generation,crc,inverse;} GateBootContext;
typedef struct {GateRetained request;GateFault fault;GateBootContext boot;uint8_t reserved[128];} GateMailbox;
_Static_assert(sizeof(GateMailbox)==256,"shared retained mailbox");
uint32_t GateFault_Valid(const GateFault *);
void GateFault_Seal(GateFault *);
uint32_t GateBootContext_Valid(const GateBootContext *);
void GateBootContext_Seal(GateBootContext *);
extern volatile GateMailbox g_recovery_mailbox;
uint32_t GatePolicy_Crc(const void *,uint32_t);
uint32_t GateRetained_Valid(const GateRetained *);
void GateRetained_Init(GateRetained *);
void GateRetained_Request(GateRetained *,uint32_t reason);
uint32_t GateRetained_BeforeBoot(GateRetained *);
uint32_t GateRetained_Confirm(GateRetained *,uint32_t healthy_ms);

/* Canonical little-endian on-disk format. Physical NOR pair swapping belongs
 * solely to the storage adapter, never these encoders or their callers. */
#define GATE_IMAGE_CONTAINER_BYTES 0x80000U
#define GATE_IMAGE_HEADER_BYTES 4096U
#define GATE_JOURNAL_BYTES 0x10000U
#define GATE_JOURNAL_RECORD_BYTES 4096U
#define GATE_JOURNAL_RECORDS 16U
#define GATE_COMMIT 0x31544D43U
#define GATE_NO_SLOT 0xffffffffU
#define GATE_IMAGE_MAGIC 0x314D4947U /* GIM1 */
#define GATE_IDENTITY_MAGIC 0x31444947U /* GID1 */
#define GATE_IDENTITY_OFFSET 0x7F000U
#define GATE_JOURNAL_MAGIC 0x314A4247U /* GBJ1 */
enum {GATE_J_READY=1,GATE_J_COPYING=2,GATE_J_BOOT_PENDING=3,
      GATE_J_CONFIRMED=4,GATE_J_WAIT=5,GATE_J_STOCK=6};
enum {GATE_F_TRIAL=1,GATE_F_ROLLED_BACK=2,GATE_F_RESULT_PENDING=4,GATE_F_RESET_PENDING=8,GATE_F_DIAGNOSTIC=16};
enum {GATE_IMAGE_PRODUCT=0,GATE_IMAGE_DIAGNOSTIC=1};
enum {GATE_FAILURE_NONE=0,GATE_FAILURE_RESET=1,GATE_FAILURE_FAULT=2,
      GATE_FAILURE_APP_TIMEOUT=3,GATE_FAILURE_INIT=4,GATE_FAILURE_IMAGE=5};
typedef struct {
 uint32_t uid[3],generation,version;
 uint8_t sha256[32],requirement[44];
 uint32_t kind;
} GateImageInfo;
typedef struct {
 uint32_t sequence,state,active,candidate,attempts;
 uint8_t active_sha[32],candidate_sha[32];
 uint32_t uid[3],active_generation,candidate_generation;
 uint32_t flags,previous,previous_generation;
 uint8_t previous_sha[32];
 uint32_t failed_version,restored_version,failure_reason,transaction,reset_epoch;
} GateJournalRecord;
/* IMAGE: magic0,version4=1,header8=4096,file12=0x80000,UID16,
 * base28=08020000,bytes32=0x60000,generation36,SHA40,requirement72[44],
 * ABI116=1,firmwareVersion120,kind124=0 Product/1 Diagnostic,reserved128..4087=FF,
 * CRC4088 over0..4087,commit4092. Always hash the full padded384KiB image.
 * JOURNAL: magic0,version4=1,sequence8,state12,active16,candidate20,
 * attempts24,flags28=0,activeSHA32,candidateSHA64,UID96,
 * activeGeneration108,candidateGeneration112,ABI116=1,
 * reserved120..4087=FF,CRC4088,commit4092. Serial sequence uses modulo32.
 * Commit is programmed separately LAST after full physical readback. */
uint32_t GateImage_Decode(const uint8_t header[4096],const uint32_t uid[3],GateImageInfo *);
void GateImage_Encode(uint8_t header[4096],const GateImageInfo *);
uint32_t GateJournal_Decode(const uint8_t record[4096],const uint32_t uid[3],GateJournalRecord *);
void GateJournal_Encode(uint8_t record[4096],const GateJournalRecord *);
uint32_t GateSequence_Newer(uint32_t a,uint32_t b);
/* Mutates only a caller-owned record; caller MUST append it durably before
 * any erase. False means local WAIT, never automatic stock restoration. */
uint32_t GateJournal_DiagnosticWait(const GateJournalRecord *);
uint32_t GateJournal_RequestRollback(GateJournalRecord *,uint32_t failure);
uint32_t GateIdentity_Decode(const uint8_t record[4096],const uint32_t uid[3],uint32_t slot);
void GateIdentity_Encode(uint8_t record[4096],const uint32_t uid[3],uint32_t slot);
#endif
