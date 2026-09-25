#ifndef APP_RECOVERY_H
#define APP_RECOVERY_H
#include "Recovery_Core.h"
#define APP_RECOVERY_OPCODE 0x48U
enum {APP_RECOVERY_NORMAL=0,APP_RECOVERY_WAIT_RELEASE,APP_RECOVERY_WAIT_CONFIRM,
 APP_RECOVERY_VALIDATING,APP_RECOVERY_INSTALLING,APP_RECOVERY_FAILED,
 APP_RECOVERY_RESET_WAIT,APP_RECOVERY_RESET_EXECUTING};
typedef struct {uint32_t state,result,source_ready,verified,error,reason,display_error,physical_ms;} AppRecoveryStatus;
extern volatile AppRecoveryStatus g_app_recovery;
/* Product: validate the independent Gate handoff before kernel initialization.
 * Legacy Bootstrap retains its own explicit pre-kernel recovery path. */
void AppRecovery_EarlyRun(void);
/* Read before BSP_FaultClear: retain the original noinit fault record while
 * the one-shot fault intent enters waiting recovery. Does not consume intent. */
uint32_t AppRecovery_IsFaultBoot(void);
/* Bootstrap overrides the weak source selector with its embedded decoder. */
const RecoverySource *AppRecovery_EmbeddedSource(void);
/* Caller must stop/drain active writers first; boots into waiting, not install. */
void AppRecovery_RequestLocal(void);
/* Bootstrap's pre-HAL rescue consumes one CRC-like checked SRAM intent. */
uint32_t AppRecovery_ConsumeIntent(void);
void AppRecovery_RequestConfirmed(void);
void AppRecovery_MarkEarlyActive(void);
/* Bootstrap may pass its embedded, bounded decoder. NULL uses independent
 * CFWREC FAT audit. Workspace is caller-owned SRAM, aligned8, >=48KiB, never
 * allocated here; decoder storage must not overlap it. force_wait starts the
 * screen directly. Returns only on explicit cancel or normal fastfallthrough. */
void AppRecovery_Run(const RecoverySource *source,void *workspace,uint32_t bytes,uint32_t force_wait);
/* Fault-safe: latch evidence then reset, no flash/NOR/RTOS calls. */
void AppRecovery_FaultReset(uint32_t fault_code);
/* Primary I/O owner only. NDCP action0=query,1=request,2=confirm,3=cancel.
 *1/2 requireSTOC token. Confirmation queues a clean early recovery reboot;
 * it does not erase from the I/O task. ACK epoch and writer-drain are required. */
uint32_t AppRecovery_Control(uint32_t action,uint32_t token,uint32_t sequence,uint32_t now);
void AppRecovery_ControlDisconnected(void);
void AppRecovery_ReplySent(uint32_t sequence,uint32_t now);
void AppRecovery_RuntimeProcess(uint32_t now,uint32_t drained);
uint32_t AppRecovery_RuntimeRequested(void);
/* Product I/O owner observes OFF -> ENTER held -> ON/held2s, including OFF
 * sleep polling. The completed gesture queues a writer-drained stock restore.
 * A hung ON+ENTER reset instead waits in the Gate for a new observed sequence. */
void AppRecovery_RuntimeButtons(uint32_t now);
/* One4KiB internal-flash hash slice per call. Identity never relies on an
 * image manifest or externally supplied digest.0 means not ready yet. */
void AppRecovery_IdentityProcess(void);
uint32_t AppRecovery_Identity(uint8_t out[84],uint32_t role);
uint32_t AppRecovery_GateIdentity(uint8_t out[32]);
/* Product only: storage arbitration must permit writes. Read-only identity
 * hashing is deliberately separate from durable healthy-boot confirmation. */
void AppRecovery_StorageProcess(void);
void AppRecovery_TrialTick(uint32_t now,uint32_t writers_drained);
/* Product-only trial handshake. Exact boot generation + Product SHA, received
 * on the current authenticated SPP epoch; query is not confirmation. */
uint32_t AppRecovery_TrialRemaining(uint32_t now);
enum {TRIAL_RADIO=1,TRIAL_PHONE,TRIAL_SCREEN,TRIAL_HEALTH,TRIAL_SAVING,TRIAL_ERROR};
/* Read-only presentation of the same gates used by the storage owner. A live
 * radio link alone does not mean that the phone/user has acknowledged trial. */
uint32_t AppRecovery_TrialPhase(void);
uint32_t AppRecovery_VisualConfirm(uint32_t sequence,const uint8_t sha[32],uint32_t epoch);
uint32_t AppRecovery_ConfirmTrial(uint32_t sequence,const uint8_t sha[32],uint32_t epoch);
#endif
