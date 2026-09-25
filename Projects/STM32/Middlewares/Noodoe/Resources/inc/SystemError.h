#ifndef SYSTEM_ERROR_H
#define SYSTEM_ERROR_H
#include <stdint.h>
enum {SYSTEM_ERROR_RESOURCE=1,SYSTEM_ERROR_SDRAM,SYSTEM_ERROR_MEMORY,SYSTEM_ERROR_DISPLAY};
typedef struct {uint32_t code,detail,stage;} SystemErrorRecord;
typedef struct {uint32_t magic,version,count,active,stage,recovery,retry_request,retry_ack;
    SystemErrorRecord first,last,history[8];} SystemErrorDiagnostics;
extern volatile SystemErrorDiagnostics g_system_error;
/* Fixed storage, no allocation. First cause is preserved; bounded ring retains
 * recent context. Report from task context; existing HardFault record is separate. */
void SystemError_Report(uint32_t code,uint32_t detail);
uint32_t SystemError_GetStatus(SystemErrorRecord *out);
void SystemError_SetStage(uint32_t stage);
#endif
