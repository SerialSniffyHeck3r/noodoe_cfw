#ifndef NOODOE_BTSTACK_CONFIG_H
#define NOODOE_BTSTACK_CONFIG_H
/* Pinned Classic-only build. No malloc pools, BLE, MFi, audio or eHCILL.
 * Firmware patches explicitly disable eHCILL until wake wiring is tested. */
#define ENABLE_CLASSIC
#define ENABLE_CC256X_BAUDRATE_CHANGE_FLOWCONTROL_BUG_WORKAROUND
#define HAVE_EMBEDDED_TIME_MS
#define HAVE_FREERTOS_TASK_NOTIFICATIONS
#define HAVE_ASSERT
/* Keep libc's fail-stop assertion, failed expression and exact source site.
 * Function names are recoverable from basename + line in the matching source;
 * do not store both the full vendor path and redundant function-name strings. */
#if defined(__FILE_NAME__) && !defined(NDEBUG)
#include <assert.h>
#define btstack_assert(condition) do { if(!(condition)) __assert_func(__FILE_NAME__, __LINE__, 0, #condition); } while(0)
#endif
#define HCI_ACL_PAYLOAD_SIZE 1021
#define MAX_NR_HCI_CONNECTIONS 1
#define MAX_NR_L2CAP_CHANNELS 3
#define MAX_NR_L2CAP_SERVICES 2
#define MAX_NR_RFCOMM_CHANNELS 1
#define MAX_NR_RFCOMM_MULTIPLEXERS 1
#define MAX_NR_RFCOMM_SERVICES 1
#define MAX_NR_SERVICE_RECORD_ITEMS 1
#define MAX_NR_BTSTACK_LINK_KEY_DB_MEMORY_ENTRIES 6
#endif
