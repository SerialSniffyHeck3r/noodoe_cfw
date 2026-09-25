#ifndef NOODOE_GATE_BOARD_H
#define NOODOE_GATE_BOARD_H
#include <stdint.h>
uint32_t GateBoard_Init(void);
uint32_t GateBoard_Millis(void);
uint32_t GateBoard_IgnOn(void);
uint32_t GateBoard_Enter(void);
uint32_t GateBoard_Keys(void);
void GateBoard_UID(uint32_t uid[3]);
void GateBoard_Reset(void) __attribute__((noreturn));
void GateBoard_Jump(uint32_t vector) __attribute__((noreturn));
uint32_t GateBoard_NorInit(void);
uint32_t GateBoard_RawRead(void *,uint32_t address,void *,uint32_t bytes);
/* Exact permitted NOR intervals are installed only after ownership validation.
 * Slot staging is a separate capability, never a whole-NOR write enable. */
uint32_t GateBoard_SetWriteRange(uint32_t first,uint32_t bytes);
uint32_t GateBoard_NorErase(uint32_t address);
uint32_t GateBoard_NorProgram(uint32_t address,const void *,uint32_t bytes);
void GateBoard_LockNor(void);
uint32_t GateBoard_FlashErase(uint32_t sector);
uint32_t GateBoard_FlashProgram(uint32_t address,const void *,uint32_t bytes);
uint32_t GateBoard_FlashRead(void *,uint32_t offset,void *,uint32_t bytes);
uint32_t GateBoard_DisplayInit(void);
/* Fixed diagnostic record; usable before HAL/RTOS/SDRAM. First error persists. */
typedef struct {uint32_t magic,stage,error,ready,address,value,frames,skipped;} GateDisplayDiagnostics;
extern volatile GateDisplayDiagnostics g_gate_display;
void GateBoard_Display(uint32_t state,uint32_t progress,uint32_t error);
#endif
