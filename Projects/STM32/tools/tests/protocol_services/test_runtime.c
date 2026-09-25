#include "test_common.h"
volatile uint32_t g_test_suite, g_test_failure_line, g_test_assertions;

/* freestanding 실제 ARM 시험의 libc 경계다. 장치/HAL을 흉내 내지 않고 표준
 * byte/string 함수만 제공하므로 파서 로직은 서비스 원본 C에서 실행된다. */
void *memset(void *dest, int value, size_t n)
{ unsigned char *p=dest; while(n--) *p++=(unsigned char)value; return dest; }
/* 겹치지 않는 객체 복사를 구현한다. */
void *memcpy(void *dest, const void *src, size_t n)
{ unsigned char *d=dest; const unsigned char *s=src; while(n--) *d++=*s++; return dest; }
/* Exact byte comparison used by the pinned stock manifest validator. */
int memcmp(const void *left,const void *right,size_t n)
{const unsigned char *a=left,*b=right;while(n--){if(*a!=*b)return (int)*a-(int)*b;++a;++b;}return 0;}
/* 파서의 앞쪽 소비처럼 겹치는 영역도 원래 순서를 유지한다. */
void *memmove(void *dest, const void *src, size_t n)
{ unsigned char *d=dest; const unsigned char *s=src; if(d<s) while(n--) *d++=*s++; else { d+=n; s+=n; while(n--) *--d=*--s; } return dest; }
/* NUL 이전 byte 수를 돌려준다. */
size_t strlen(const char *s) { const char *p=s; while(*p) ++p; return (size_t)(p-s); }
/* ASCII 비교 결과의 부호만 사용한다. */
int strcmp(const char *a,const char *b) { while(*a && *a==*b) {++a;++b;} return (unsigned char)*a-(unsigned char)*b; }
/* 최대 n byte 비교로 표준 경계를 유지한다. */
int strncmp(const char *a,const char *b,size_t n) { while(n--) { if(*a!=*b || !*a) return (unsigned char)*a-(unsigned char)*b; ++a;++b; } return 0; }
/* 짧은 adapter 오류 문자열 검색만을 위한 libc 구현이다. */
char *strstr(const char *s,const char *needle) { size_t n=strlen(needle); do { if(strncmp(s,needle,n)==0) return (char *)s; } while(*s++); return NULL; }
/* 최초 실패 suite/행을 RAM 진단에 남기고 caller로 반환한다. */
int test_main(void)
{
    int result;
    g_test_suite=1U; result=TestVehicle(); if(result) return result;
    g_test_suite=2U; result=TestGnss(); if(result) return result;
    g_test_suite=3U; result=TestObd(); if(result) return result;
    g_test_suite=4U; result=TestNdcp(); if(result) return result;
    g_test_suite=5U; result=TestUpdate(); if(result) return result;
    g_test_suite=0U; return 0;
}
