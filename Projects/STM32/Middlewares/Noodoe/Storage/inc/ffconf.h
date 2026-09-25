/* Project-owned FatFs R0.12c configuration. The upstream package is unchanged.
 * 4KiB logical sectors match NOR erase units and avoid512-byte read/modify/write.
 * Single-owner StorageService supplies locking; vendor reentrancy hooks are off.
 * Initial bring-up uses8.3 names, so no unverified Unicode/codepage dependency.
 */
#ifndef NOODOE_FFCONF_H
#define NOODOE_FFCONF_H
#define _FFCONF 68300
#define _FS_READONLY 0
#define _FS_MINIMIZE 0
#define _USE_STRFUNC 0
#define _USE_FIND 0
#define _USE_MKFS 1
#define _USE_FASTSEEK 0
#define _USE_EXPAND 0
#define _USE_CHMOD 0
#define _USE_LABEL 0
#define _USE_FORWARD 0
#define _CODE_PAGE 437
#define _USE_LFN 0
#define _MAX_LFN 255
#define _LFN_UNICODE 0
#define _STRF_ENCODE 0
#define _FS_RPATH 0
#define _VOLUMES 1
#define _STR_VOLUME_ID 0
#define _VOLUME_STRS "NOR"
#define _MULTI_PARTITION 0
#define _MIN_SS 4096
#define _MAX_SS 4096
#define _USE_TRIM 0
#define _FS_NOFSINFO 0
#define _FS_TINY 1
#define _FS_EXFAT 0
#define _FS_NORTC 1
#define _NORTC_MON 1
#define _NORTC_MDAY 1
#define _NORTC_YEAR 2026
#define _FS_LOCK 0
#define _FS_REENTRANT 0
#define _FS_TIMEOUT 1000
#define _SYNC_t void*
#endif
