#ifndef NDCP_H
#define NDCP_H
#include <stdint.h>
#include <stddef.h>
#define NDCP_VERSION 1U
#define NDCP_HEADER_SIZE 16U
#define NDCP_PAYLOAD_MAX 1024U
#define NDCP_FRAME_MAX (NDCP_HEADER_SIZE+NDCP_PAYLOAD_MAX+4U)
#define NDCP_FLAG_RESPONSE 1U
#define NDCP_FLAG_ERROR 2U
#define NDCP_PARTIAL_TIMEOUT_MS 2000U
/* opcode00..1F는 전화 제어,40..4F는 updater 전용이다. payload는 callback 동안만
 * 유효한 parser 내부 view다. 다른 태스크로 보낼 때 반드시 내용을 복사한다. */
typedef struct {
    uint32_t opcode, flags, sequence, length;
    const uint8_t *payload;
} NDCP_Frame;
typedef void (*NDCP_Callback)(void *context,const NDCP_Frame *frame);
typedef struct {
    NDCP_Callback callback;
    void *context;
    uint32_t used,last_byte_ms,frames_ok,crc_errors,header_errors,discarded_bytes,timeouts;
    uint8_t bytes[NDCP_FRAME_MAX];
} NDCP_Parser;

/* CRC32/ISO-HDLC: initFFFFFFFF,reflected polynomialEDB88320,final xorFFFFFFFF.
 * host zlib.crc32와 같다. CRC는 전송 손상 검출이며 인증을 제공하지 않는다. */
uint32_t NDCP_Crc32(const uint8_t *data,size_t length);
void NDCP_Init(NDCP_Parser *parser,NDCP_Callback callback,void *context);
void NDCP_Feed(NDCP_Parser *parser,const uint8_t *data,size_t length,uint32_t now_ms);
void NDCP_Poll(NDCP_Parser *parser,uint32_t now_ms);
/* header magic NDCP,version/opcode,u16flags,u32seq,u16length,u16reserved0 뒤
 * payload와 u32CRC를 붙인다. 모든 정수는LE. 성공 frame byte수/실패0 반환한다. */
size_t NDCP_Encode(uint8_t *out,size_t capacity,uint32_t opcode,uint32_t flags,
                   uint32_t sequence,const uint8_t *payload,size_t length);
#endif
