#ifndef RECOVERY_TARGET_H
#define RECOVERY_TARGET_H
#include <stdint.h>
/* An ABI family, not proof of a resident executable's identity. Keep this a
 * macro: metadata transactions execute from SRAM while flash is busy. */
#define RECOVERY_VERSION_SUPPORTED(v) ((v)==0x000E0000U||(v)==0x000F0000U)
#define RECOVERY_PANEL_SUPPORTED(v) ((v)==3U||(v)==4U)
/* Full lower64KiB is read-only. 0.14 retains its exact donor fingerprint;
 * observed SR0701/0.15 is first-use profile qualification, not an approved
 * executable hash. Persistent recovery must also match its captured binding. */
uint32_t RecoveryTarget_Verify(const uint8_t lower[65536]);
void RecoveryTarget_Hash(const uint8_t code[32768],uint8_t sha[32]);
uint32_t RecoveryTarget_Export(const uint8_t lower[65536],uint32_t offset,
 uint32_t bytes,uint8_t *out);
#endif
