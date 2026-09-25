#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "RAM codec requires little-endian target"
#endif
#include "Bluetooth_KeyCodec.h"
#include <string.h>
static uint32_t U(const uint8_t *p){ uint32_t v;memcpy(&v,p,4);return v;}
/* Preserve the explicit LE32 wire bytes without a byte-store loop at every
 * call site. memcpy also preserves the contract for unaligned RAM buffers. */
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "This scalar codec requires a little-endian target"
#endif
static void W(uint8_t *p,uint32_t n){memcpy(p,&n,4);}
/* Stable field encoding is shared by Product and temporary Diagnostic. Pairing
 * keys are never logged or exposed by the diagnostic/status protocol. */
void Bluetooth_EncodeKeys(uint8_t b[152],const Bluetooth_KeyStore *k)
{W(b,1);W(b+4,k->generation);for(uint32_t i=0;i<6;i++){
 memcpy(b+8+i*24,k->keys[i].address,6);memcpy(b+14+i*24,k->keys[i].key,16);
 b[30+i*24]=k->keys[i].type;b[31+i*24]=k->keys[i].valid;}}
uint32_t Bluetooth_DecodeKeys(Bluetooth_KeyStore *k,const uint8_t *b,uint32_t n)
{
 if(!k||!b||n!=152||U(b)!=1)return 0;
 for(uint32_t i=0;i<6;i++)if(b[30+i*24]>8||b[31+i*24]>1)return 0;
 memset(k,0,sizeof(*k));k->version=1;k->generation=U(b+4);
 for(uint32_t i=0;i<6;i++){memcpy(k->keys[i].address,b+8+i*24,6);memcpy(k->keys[i].key,b+14+i*24,16);
  k->keys[i].type=b[30+i*24];k->keys[i].valid=b[31+i*24];}return 1;
}
