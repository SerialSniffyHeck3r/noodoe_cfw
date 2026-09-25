#ifndef NOODOE_ROUTINE_UPDATE_H
#define NOODOE_ROUTINE_UPDATE_H
#include <stdint.h>
/* Read-only preflight runs in StorageTask, one4KiB step. Successful status is
 * bound to a live connection generation; the I/O owner applies authorization. */
uint32_t RoutineUpdate_Request(uint32_t epoch);
uint32_t RoutineUpdate_Status(uint32_t epoch,uint32_t *error);
void RoutineUpdate_Process(void);
#endif
