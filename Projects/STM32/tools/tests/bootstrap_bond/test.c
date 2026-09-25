#include "Bootstrap_Bond.h"
#include "Bluetooth_KeyCodec.h"
#include "bluetooth_keys.h"
#include <string.h>
extern uint32_t FastInit(void);
static BootstrapStorage *s;static BootstrapBond bond;
volatile Bluetooth_Diagnostics g_bluetooth;
void *memmove(void *d,const void *s,size_t n){uint8_t *a=d;const uint8_t *b=s;if(a<b)for(size_t i=0;i<n;i++)a[i]=b[i];else while(n){n--;a[n]=b[n];}return d;}
uint32_t BondStart(void){s=(BootstrapStorage *)FastInit();memset(&bond,0,sizeof(bond));
 bd_addr_t address={1,2,3,4,5,6};link_key_t key;memset(key,0x5a,16);Bluetooth_KeyDB()->put_link_key(address,key,4);
 return BootstrapBond_Begin(&bond,s);}
uint32_t BondStep(uint32_t n){while(n--&&bond.phase)BootstrapBond_Process(&bond,s);return bond.phase;}
uint32_t BondError(void){return bond.error;}
uint32_t BondRestore(void){s=(BootstrapStorage *)FastInit();uint32_t e=BootstrapStorage_Inspect(s,BS_CONFIG);if(e)return e;
 for(unsigned i=0;i<10000&&s->state!=BS_SAVED&&s->state!=BS_FAILED;i++)BootstrapStorage_Process(s);
 if(s->state!=BS_SAVED)return s->error;
 e=BootstrapBond_Restore(s);Bluetooth_KeyStore keys;Bluetooth_ExportKeys(&keys);Bluetooth_EncodeKeys((uint8_t *)0x11000000,&keys);return e;}
uint32_t BondRekey(void){bd_addr_t address={1,2,3,4,5,6};link_key_t key;memset(key,0xa5,16);Bluetooth_KeyDB()->put_link_key(address,key,4);return 0;}
