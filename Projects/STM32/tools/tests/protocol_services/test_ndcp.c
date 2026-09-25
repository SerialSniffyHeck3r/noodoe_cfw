#include "test_common.h"
#include "NDCP.h"
static NDCP_Parser parser;
static uint32_t calls,sequence,length,opcode;
static uint8_t last[NDCP_PAYLOAD_MAX];
/* callback 수명 안에서만 payload를 복사한다. */
static void Receive(void *context,const NDCP_Frame *frame)
{ (void)context;++calls;sequence=frame->sequence;length=frame->length;opcode=frame->opcode;if(length)memcpy(last,frame->payload,length); }
/* 실제 C encoder/parser의 모든 split과손상복구를독립CRC knownvector와대조한다. */
int TestNdcp(void)
{
    uint8_t wire[NDCP_FRAME_MAX],noise[NDCP_FRAME_MAX+32U],payload[NDCP_PAYLOAD_MAX];
    size_t n;uint32_t split,i;
    CHECK(NDCP_Crc32((const uint8_t *)"123456789",9U)==0xCBF43926U);
    CHECK(NDCP_Crc32(NULL,0U)==0U);
    memcpy(payload,"xxNDCPyy",8U);n=NDCP_Encode(wire,sizeof(wire),0x40U,0U,0x12345678U,payload,8U);
    CHECK(n==28U && wire[0]=='N' && wire[8]==0x78U && wire[12]==8U);
    for(split=0;split<=n;++split){calls=0;NDCP_Init(&parser,Receive,NULL);NDCP_Feed(&parser,wire,split,0U);NDCP_Feed(&parser,wire+split,n-split,1U);CHECK(calls==1U&&sequence==0x12345678U&&length==8U&&opcode==0x40U);CHECK(last[2]=='N');}
    calls=0;NDCP_Init(&parser,Receive,NULL);
    memcpy(noise,wire,n);noise[24]^=1U;memcpy(noise+n,wire,n);
    NDCP_Feed(&parser,noise,n*2U,10U);CHECK(calls==1U&&parser.crc_errors==1U);
    calls=0;NDCP_Init(&parser,Receive,NULL);memcpy(noise,wire,n);noise[4]=2U;memcpy(noise+n,wire,n);
    NDCP_Feed(&parser,noise,n*2U,20U);CHECK(calls==1U&&parser.header_errors>=1U);
    calls=0;NDCP_Init(&parser,Receive,NULL);memcpy(noise,wire,16U);noise[12]=0;noise[13]=4;memcpy(noise+16,wire,n);
    NDCP_Feed(&parser,noise,16U+n,30U);CHECK(!calls);NDCP_Poll(&parser,2030U);CHECK(calls==1U&&parser.timeouts==1U&&!parser.used);
    for(i=0;i<sizeof(payload);++i)payload[i]=(uint8_t)i;
    n=NDCP_Encode(wire,sizeof(wire),1U,NDCP_FLAG_RESPONSE,99U,payload,sizeof(payload));CHECK(n==NDCP_FRAME_MAX);
    calls=0;NDCP_Init(&parser,Receive,NULL);for(i=0;i<n;++i)NDCP_Feed(&parser,wire+i,1U,40U);
    CHECK(calls==1U&&length==1024U&&last[1023]==255U);
    CHECK(!NDCP_Encode(wire,10U,1U,0U,0U,NULL,0U));
    CHECK(!NDCP_Encode(wire,sizeof(wire),1U,4U,0U,NULL,0U));
    return 0;
}
