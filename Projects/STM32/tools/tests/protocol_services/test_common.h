#ifndef PROTOCOL_TEST_COMMON_H
#define PROTOCOL_TEST_COMMON_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
extern volatile uint32_t g_test_suite, g_test_failure_line, g_test_assertions;
#define CHECK(condition) do { ++g_test_assertions; if(!(condition)) { g_test_failure_line=__LINE__; return __LINE__; } } while(0)
int TestVehicle(void);
int TestGnss(void);
int TestObd(void);
int TestNdcp(void);
int TestUpdate(void);
#endif
