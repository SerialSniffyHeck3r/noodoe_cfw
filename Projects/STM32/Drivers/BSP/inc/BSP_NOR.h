#ifndef BSP_NOR_H
#define BSP_NOR_H
#include <stdint.h>
#include "stm32f4xx_hal.h"

#define BSP_NOR_CAPACITY_BYTES 0x08000000UL
#define BSP_NOR_EXPECTED_JEDEC_ID 0x00C2201BUL
#define BSP_NOR_READ_CHUNK_BYTES 4096U
#define BSP_NOR_PROVISIONED_TOKEN 0x50525631UL

typedef enum { BSP_NOR_OK=0, BSP_NOR_ARGUMENT, BSP_NOR_CONTEXT, BSP_NOR_NOT_READY,
    BSP_NOR_BUSY, BSP_NOR_IO, BSP_NOR_TIMEOUT, BSP_NOR_ID_MISMATCH,
    BSP_NOR_LOCKED, BSP_NOR_PROTECTED, BSP_NOR_VERIFY } BSP_NOR_Status;
typedef struct {
    uint32_t magic, version, ready, result, jedec_id, status_register, capacity_bytes;
    uint32_t spi_clock_hz, reads, bytes_read, dma_completions, dma_errors;
    uint32_t last_address, last_length, hal_status, hal_error, busy;
} BSP_NOR_Diagnostics;
extern volatile BSP_NOR_Diagnostics g_bsp_nor;

/* Task-only, after generated MX_SPI5_Init and DMA init. Reads JEDEC/status only.
 * No writes occur during initialization; all write capabilities begin locked. */
BSP_NOR_Status BSP_NOR_Init(void);
/* Entire 128MiB is readable, including factory BL/APP staging. Destination may
 * be unaligned; the BSP owns a DMA-accessible bounce buffer and restores wire
 * byte order after each 16-bit DMA transfer. This is a bounded blocking API. */
BSP_NOR_Status BSP_NOR_Read(uint32_t address, void *destination, uint32_t length);
/* Independent 8-bit polling read for DMA/endian verification; length<=256. */
BSP_NOR_Status BSP_NOR_ReadPolling(uint32_t address, void *destination, uint32_t length);
/* Forward from HAL_SPI_TxRxCpltCallback/HAL_SPI_ErrorCallback. IRQ priority4 is
 * above the RTOS syscall ceiling: these functions only update volatile flags. */
void BSP_NOR_OnTxRxComplete(SPI_HandleTypeDef *spi);
void BSP_NOR_OnError(SPI_HandleTypeDef *spi);
/* Host-confirmed complete A/B backup grants this boot/USB session's explicit
 * format permission and FS/NVM writes. USB reset/LOCK revoke only this host
 * permission. The token is an intent guard, not authentication or backup proof. */
BSP_NOR_Status BSP_NOR_UnlockStorage(uint32_t token);
void BSP_NOR_LockStorage(void);
/* Host format permission only; this is intentionally NOT the ordinary-write
 * query. A provisioned product can write files without being allowed to format. */
uint32_t BSP_NOR_IsStorageUnlocked(void);
/* Root Settings service alone calls this after validating the persisted marker
 * schema, exact layout, CRC and current MCU UID. A successful mount or presence
 * of old stock bytes is not provisioning. No media operation occurs here; both
 * permissions begin disabled at Init. This permit survives USB reset/LOCK but
 * not MCU reset, and permits only [0,0x07F80000) FS/NVM writes, never format. */
BSP_NOR_Status BSP_NOR_EnableProvisionedStorage(uint32_t token);
void BSP_NOR_DisableProvisionedStorage(void);
uint32_t BSP_NOR_CanWriteStorage(void);
/* StorageService holds its mutex across BeginFormat..EndFormat. While active,
 * every FS/NVM page/erase requires the host gate even if provisioned runtime
 * writes are enabled. USB reset therefore stops further format commands; an
 * already submitted physical command cannot be undone. OTA is independent. */
BSP_NOR_Status BSP_NOR_BeginFormat(void);
void BSP_NOR_EndFormat(void);
/* Page program accepts1..256 bytes within one256-byte page; caller must erase
 * first when any0->1 transition is needed. Erase requires4KiB alignment. Both
 * independently enforce the FS/NVM range and verify the resulting bytes. */
BSP_NOR_Status BSP_NOR_Program(uint32_t address,const void *source,uint32_t length);
BSP_NOR_Status BSP_NOR_Erase4K(uint32_t address);
/* OTA capability is separate from USB formatting and can only touch APP stage.
 * Session ID must be nonzero. It survives USB disconnect but never MCU reset.
 * No public API can erase/program the reserved64KiB BL staging interval. */
BSP_NOR_Status BSP_NOR_OTAEnable(uint32_t transaction_id);
void BSP_NOR_OTADisable(void);
BSP_NOR_Status BSP_NOR_OTAProgram(uint32_t address,const void *source,uint32_t length);
BSP_NOR_Status BSP_NOR_OTAErase4K(uint32_t address);
/* Audited CFW files only: CFG0(4), RIDE1(8), PIC2(32), CFWA3(16),
 * CFWB4(16), BOOT5(2), with counts in32KiB clusters. StorageTask installs maps
 * after FAT ownership AND purpose/UID validation. These capabilities never
 * make disk_write, formatting, legacy NVM or staging writable. */
void BSP_NOR_ClearContainers(void);
BSP_NOR_Status BSP_NOR_GrantContainer(uint32_t file,const uint32_t *clusters,uint32_t count);
BSP_NOR_Status BSP_NOR_ContainerProgram(uint32_t file,uint32_t address,const void *data,uint32_t bytes);
BSP_NOR_Status BSP_NOR_ContainerErase(uint32_t file,uint32_t address);
/* Bootstrap create-only transaction. Caller has audited FAT ownership and
 * two complete backup passes. This capability permits only one allocated file
 * extent plus explicitly selected FAT/root sectors1..8; it never opens generic
 * storage/formatting or staging. Revoke when authenticated session ends. */
BSP_NOR_Status BSP_NOR_ProvisionGrant(uint32_t first,uint32_t bytes,uint32_t metadata_mask);
void BSP_NOR_ProvisionLock(void);
BSP_NOR_Status BSP_NOR_ProvisionProgram(uint32_t address,const void *,uint32_t bytes);
BSP_NOR_Status BSP_NOR_ProvisionErase(uint32_t address);
#endif
