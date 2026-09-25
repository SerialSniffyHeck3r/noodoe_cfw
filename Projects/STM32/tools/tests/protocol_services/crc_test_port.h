#ifndef CRC_TEST_PORT_H
#define CRC_TEST_PORT_H
#include <stdint.h>

/* CRC 주변장치 경계만 대체한다. 구현의 lock/복원/word 조립 경로는 실제 BSP_CRC.c다. */
uint32_t CrcTest_MaskGet(void);
void CrcTest_MaskSet(uint32_t value);
void CrcTest_IrqDisable(void);
void CrcTest_Barrier(void);
void CrcTest_Sync(void);
uint32_t CrcTest_ClockRead(void);
void CrcTest_ClockWrite(uint32_t value);
void CrcTest_Reset(void);
void CrcTest_WordWrite(uint32_t value);
uint32_t CrcTest_ResultRead(void);

#define CRC_PORT_MASK_GET() CrcTest_MaskGet()
#define CRC_PORT_MASK_SET(value) CrcTest_MaskSet(value)
#define CRC_PORT_IRQ_DISABLE() CrcTest_IrqDisable()
#define CRC_PORT_BARRIER() CrcTest_Barrier()
#define CRC_PORT_SYNC() CrcTest_Sync()
#define CRC_PORT_CLOCK_READ() CrcTest_ClockRead()
#define CRC_PORT_CLOCK_WRITE(value) CrcTest_ClockWrite(value)
#define CRC_PORT_CLOCK_BIT (1UL << 12U)
#define CRC_PORT_RESET() CrcTest_Reset()
#define CRC_PORT_WORD_WRITE(value) CrcTest_WordWrite(value)
#define CRC_PORT_RESULT_READ() CrcTest_ResultRead()
#endif
