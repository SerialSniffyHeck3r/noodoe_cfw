#include "bsp_fault.h"
/* Embedded libc assertions must use the same fail-stop/recovery contract as
 * CPU faults. stderr/abort has no console here and brings an unused buffered
 * stream implementation into flash. Keep all assertion evidence for SWD,
 * record a durable fault reason through the normal retained fault path, and
 * let BSP_FaultRecord stop watchdog feeding. No allocation, printing or NOR IO. */
volatile struct {
 const char *file,*function,*expression;
 uint32_t line;
} g_bsp_c_assert;
__attribute__((noreturn)) void __assert_func(const char *file,int line,
 const char *function,const char *expression)
{
 g_bsp_c_assert.file=file;g_bsp_c_assert.function=function;
 g_bsp_c_assert.expression=expression;g_bsp_c_assert.line=(uint32_t)line;
 BSP_FaultRecord(0xB0000000U|((uint32_t)line&0x00FFFFFFU));
}
