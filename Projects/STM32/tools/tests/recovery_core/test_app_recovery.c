#include "App_Recovery.h"
#include <stddef.h>
#include <stdint.h>
volatile uint32_t app_recovery_assertions;
#define CHECK(x) do{++app_recovery_assertions;if(!(x))return __LINE__;}while(0)
void *memcpy(void *d,const void *s,size_t n){uint8_t *a=d;const uint8_t*b=s;while(n--)*a++=*b++;return d;}
uint32_t HAL_GetUIDw0(void){return 0x11223344;}
uint32_t HAL_GetUIDw1(void){return 0x55667788;}
uint32_t HAL_GetUIDw2(void){return 0x99AABBCC;}
int app_recovery_test_main(void)
{
 CHECK(!AppRecovery_RuntimeRequested());
 CHECK(AppRecovery_Control(2,RECOVERY_CONFIRM_TOKEN,7,100)==RECOVERY_STATE);
 CHECK(AppRecovery_Control(1,0,7,100)==RECOVERY_ARGUMENT);
 CHECK(AppRecovery_Control(0,1,7,100)==RECOVERY_ARGUMENT);
 CHECK(!AppRecovery_Control(1,RECOVERY_CONFIRM_TOKEN,7,100));
 CHECK(AppRecovery_Control(2,RECOVERY_CONFIRM_TOKEN,7,30101)==RECOVERY_STATE);
 AppRecovery_ControlDisconnected();CHECK(!AppRecovery_RuntimeRequested());
 CHECK(!AppRecovery_Control(1,RECOVERY_CONFIRM_TOKEN,7,40000));
 CHECK(!AppRecovery_Control(2,RECOVERY_CONFIRM_TOKEN,8,40001));
 CHECK(AppRecovery_RuntimeRequested());
 AppRecovery_ReplySent(7,40002);AppRecovery_RuntimeProcess(50000,1);CHECK(AppRecovery_RuntimeRequested());
 AppRecovery_ReplySent(8,40002);AppRecovery_RuntimeProcess(41501,1);CHECK(AppRecovery_RuntimeRequested());
 AppRecovery_RuntimeProcess(50000,0);CHECK(AppRecovery_RuntimeRequested());
 AppRecovery_ControlDisconnected();AppRecovery_RuntimeProcess(50000,1);CHECK(!AppRecovery_RuntimeRequested());
 CHECK(!AppRecovery_Control(1,RECOVERY_CONFIRM_TOKEN,10,0xFFFFFE00));
 CHECK(!AppRecovery_Control(2,RECOVERY_CONFIRM_TOKEN,11,0xFFFFFF00));
 AppRecovery_ReplySent(11,0xFFFFFF00);AppRecovery_RuntimeProcess(0x4DB,1);CHECK(AppRecovery_RuntimeRequested());
 return 0;
}
/* Separate emulator invocations intercept actual CMSIS AIRCR writes rather
 * than replacing the production reset with a fake successful callback. */
void app_recovery_reset_test(void){AppRecovery_RuntimeProcess(0x4DC,1);}
uint32_t app_recovery_identity_test(void)
{uint8_t out[84];if(AppRecovery_Identity(out,2))return 1;
 for(uint32_t n=0;n<120;++n)AppRecovery_IdentityProcess();
 return AppRecovery_Identity(out,2)?0:2;}
void *memset(void *p,int c,size_t n){uint8_t *b=p;while(n--)*b++=(uint8_t)c;return p;}

int memcmp(const void *x,const void*y,size_t n){const uint8_t*a=x,*b=y;while(n--){if(*a!=*b)return (int)*a-(int)*b;++a;++b;}return 0;}
