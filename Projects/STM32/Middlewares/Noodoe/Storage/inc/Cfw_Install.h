#ifndef CFW_INSTALL_H
#define CFW_INSTALL_H
#include <stdint.h>
#define CFW_INSTALL_BYTES (1441792U)
/* Host supplies fixed-file images, initial clusters/root entries and the CRC
 * of the CURRENT logical FAT metadata. Host tool additionally requires two
 * current identical full backups and SHA256 preconditions/readbacks.
 * sequence is written last. Completion never re-enables ordinary writes;
 * explicit reboot repeats full FAT/purpose/UID validation. */
typedef struct {uint32_t magic,version,buffer,capacity,first[3],entry[3],metadata_crc,
    payload_crc,token,sequence,ack,state,error,progress;} CfwInstallMailbox;
extern volatile CfwInstallMailbox g_cfw_install;
void CfwInstall_Process(void);
uint32_t CfwInstall_Busy(void);
#endif
