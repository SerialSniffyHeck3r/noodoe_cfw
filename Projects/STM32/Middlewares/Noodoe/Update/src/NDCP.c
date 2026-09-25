#include "Noodoe_Crc32.h"
#include "NDCP.h"
#include <string.h>

/* alignment를 가정하지 않고 little-endian byte를 읽는다. */
static uint32_t LE16(const uint8_t *p) { uint16_t value;memcpy(&value,p,2);return value; }
/* CRC/seq에 쓰이는32-bit little-endian 읽기다. */
static uint32_t LE32(const uint8_t *p) { return LE16(p)|(LE16(p+2)<<16); }
/* outbound 정수를 wire byte 순서대로 쓴다. */
/* Preserve the explicit LE32 wire bytes without a byte-store loop at every
 * call site. memcpy also preserves the contract for unaligned RAM buffers. */
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "This scalar codec requires a little-endian target"
#endif
static void Put32(uint8_t *p,uint32_t v) { memcpy(p,&v,4); }
/* 초기/final xor까지 포함한다. 빈 payload에도 header의 CRC가 계산된다. */
uint32_t NDCP_Crc32(const uint8_t *data,size_t length)
{if(!data&&length)return 0;return Noodoe_Crc32(data,(uint32_t)length);}
/* 앞쪽 후보를 소비한다. 전체 프레임을 처리한 경우에는 discarded에 포함하지 않는다. */
static void Drop(NDCP_Parser *p,uint32_t n,uint32_t discarded)
{ if(n>p->used)n=p->used;p->used-=n;if(discarded)p->discarded_bytes+=n;if(p->used)memmove(p->bytes,p->bytes+n,p->used); }
/* magic부터 길이/버전/reserved/CRC까지 맞는 frame만 callback으로 보낸다.
 * callback은 재귀 Feed를 호출하지 않으며 SPI/flash 작업 대신 큐에 복사해야 한다. */
static void Drain(NDCP_Parser *p,uint32_t now)
{
    while(p->used) {
        uint32_t length,total;NDCP_Frame frame;
        if(p->bytes[0]!='N') {Drop(p,1U,1U);continue;}
        if(p->used<4U)return;
        if(p->bytes[1]!='D'||p->bytes[2]!='C'||p->bytes[3]!='P') {Drop(p,1U,1U);continue;}
        if(p->used<NDCP_HEADER_SIZE)return;
        length=LE16(p->bytes+12);
        if(p->bytes[4]!=NDCP_VERSION || length>NDCP_PAYLOAD_MAX || LE16(p->bytes+14) || (LE16(p->bytes+6)&~3U)) {
            ++p->header_errors;Drop(p,1U,1U);continue;
        }
        total=NDCP_HEADER_SIZE+length+4U;
        if(p->used<total)return;
        if(NDCP_Crc32(p->bytes,total-4U)!=LE32(p->bytes+total-4U)) {++p->crc_errors;Drop(p,1U,1U);continue;}
        frame.opcode=p->bytes[5];frame.flags=LE16(p->bytes+6);frame.sequence=LE32(p->bytes+8);
        frame.length=length;frame.payload=p->bytes+NDCP_HEADER_SIZE;
        ++p->frames_ok;
        if(p->callback)p->callback(p->context,&frame);
        Drop(p,total,0U);
    }
    (void)now;
}
/* context/callback만 외부 포인터이며 나머지 parser 상태는 이 객체가 소유한다. */
void NDCP_Init(NDCP_Parser *p,NDCP_Callback callback,void *context)
{ if(!p)return;memset(p,0,sizeof(*p));p->callback=callback;p->context=context; }
/* 끊긴 header의 length가 후속 frame을 붙잡지 않도록 무수신2초 뒤 후보를 버린다. */
void NDCP_Poll(NDCP_Parser *p,uint32_t now)
{ if(!p)return;if(p->used && now-p->last_byte_ms>=NDCP_PARTIAL_TIMEOUT_MS){++p->timeouts;do{Drop(p,1U,1U);Drain(p,p->last_byte_ms);}while(p->used);} }
/* 수신 조각 길이와 무관하며 한 태스크에서만 호출한다. overflow시 할당을 늘리지 않는다. */
void NDCP_Feed(NDCP_Parser *p,const uint8_t *data,size_t length,uint32_t now)
{
    size_t i;if(!p||(!data&&length))return;NDCP_Poll(p,now);
    for(i=0;i<length;++i){if(p->used==NDCP_FRAME_MAX)Drop(p,1U,1U);p->bytes[p->used++]=data[i];p->last_byte_ms=now;Drain(p,now);}
}
/* 검증을 먼저 끝내고 출력하므로 부족한 caller buffer를 부분 frame으로 바꾸지 않는다. */
size_t NDCP_Encode(uint8_t *out,size_t capacity,uint32_t opcode,uint32_t flags,uint32_t sequence,const uint8_t *payload,size_t length)
{
    size_t total=NDCP_HEADER_SIZE+length+4U;
    if(!out||opcode>255U||(flags&~3U)||length>NDCP_PAYLOAD_MAX||capacity<total||(!payload&&length))return 0U;
    memcpy(out,"NDCP",4U);out[4]=NDCP_VERSION;out[5]=(uint8_t)opcode;
    out[6]=(uint8_t)flags;out[7]=(uint8_t)(flags>>8);Put32(out+8,sequence);
    out[12]=(uint8_t)length;out[13]=(uint8_t)(length>>8);out[14]=out[15]=0;
    if(length)memcpy(out+NDCP_HEADER_SIZE,payload,length);
    Put32(out+total-4U,NDCP_Crc32(out,total-4U));return total;
}
