#include "Bootstrap_Image.h"
#include "miniz_tinfl.h"
#include <string.h>
extern const uint8_t g_bootstrap_stock_deflate[];
extern const uint32_t g_bootstrap_stock_deflate_bytes;
#define IMAGE_BYTES 0x70000U
/* Dedicated32KiB sliding dictionary. It is CPU-only and never given to DMA.
 * The inflater writes a full dictionary window; callers consume it in smaller
 * chunks. This keeps tinfl's power-of-two wrapping-buffer contract intact. */
#if NOODOE_UNINSTALL
static uint8_t dictionary[TINFL_LZ_DICT_SIZE] __attribute__((aligned(8)));
#else
static uint8_t dictionary[TINFL_LZ_DICT_SIZE] __attribute__((section(".ccm_bss.bootstrap"),aligned(8)));
#endif
static tinfl_decompressor inflater;
static uint32_t input_at,window_at,window_bytes,produced,initialized,failed,done;
void BootstrapImage_Reset(void)
{
    memset(&inflater,0,sizeof(inflater));tinfl_init(&inflater);
    input_at=window_at=window_bytes=produced=failed=done=0;initialized=1;
}
/* Inflate one complete dictionary window. Reject truncated, trailing and
 * overlong streams; image SHA verification remains the recovery core's job. */
static int Next(void)
{
    if(done||failed)return -1;
    size_t in=g_bootstrap_stock_deflate_bytes-input_at,out=sizeof(dictionary);
    tinfl_status s=tinfl_decompress(&inflater,g_bootstrap_stock_deflate+input_at,
        &in,dictionary,dictionary,&out,0);
    input_at+=(uint32_t)in;window_at=produced;window_bytes=(uint32_t)out;
    produced+=(uint32_t)out;
    if(s<0||!out||produced>IMAGE_BYTES||s==TINFL_STATUS_NEEDS_MORE_INPUT){failed=1;return -1;}
    if(s==TINFL_STATUS_DONE){done=1;if(produced!=IMAGE_BYTES||input_at!=g_bootstrap_stock_deflate_bytes){failed=1;return -1;}}
    return 0;
}
int BootstrapImage_Read(void *context,uint32_t offset,void *data,uint32_t bytes)
{
    (void)context;
    if(!data||!bytes||bytes>4096U||offset>IMAGE_BYTES||bytes>IMAGE_BYTES-offset)return -1;
    if(!initialized||offset<window_at)BootstrapImage_Reset();
    uint8_t *out=data;
    while(bytes){
        while(offset>=window_at+window_bytes){if(Next())return -1;}
        if(failed||offset<window_at)return -1;
        uint32_t n=window_at+window_bytes-offset;if(n>bytes)n=bytes;
        memcpy(out,dictionary+(offset-window_at),n);offset+=n;out+=n;bytes-=n;
    }
    return 0;
}
