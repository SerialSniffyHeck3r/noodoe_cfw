#ifndef NOODOE_EVE_ROM_TEXT_H
#define NOODOE_EVE_ROM_TEXT_H
#include <stdint.h>
/* FT81x Programming Guide 5.4.1-5.4.3: 148-byte ROM metrics, ASCII cells.
 * Scale advances and glyphs together by 110%, using inverse bitmap transform.
 * Uses only existing ROM pixels; no external font, RAM_G allocation or heap. */
typedef struct {uint8_t metrics[3][148];uint32_t valid;} EveRomText;
static inline uint32_t EveRomWord(const uint8_t *p)
{return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static inline void EveRomEmit(uint8_t *b,uint32_t *n,uint32_t v)
{for(uint32_t i=0;i<4;i++)b[(*n)++]=(uint8_t)(v>>(8*i));}
static inline uint32_t EveRomLoad(EveRomText *s,uint32_t (*read)(uint32_t,void*,uint32_t),uint32_t font)
{
 if(font<26||font>28)return 0;
 uint32_t bit=1U<<(font-26);if(s->valid&bit)return 1;
 uint8_t b[4];if(read(0x2ffffc,b,4))return 0;uint32_t root=EveRomWord(b);
 if(root<0x100000||root>0x2ff000)return 0;
 uint8_t *m=s->metrics[font-26];if(read(root+148*(font-16),m,148))return 0;
 if(EveRomWord(m+132)>1023||!EveRomWord(m+136)||EveRomWord(m+136)>128||!EveRomWord(m+140)||EveRomWord(m+140)>128||EveRomWord(m+144)>0x3fffff)return 0;
 s->valid|=bit;return 1;
}
static inline uint32_t EveRomDraw(EveRomText *s,uint8_t *b,uint32_t *n,uint32_t capacity,uint32_t y,uint32_t font,const char *text)
{
 if(font<26||font>28||!(s->valid&(1U<<(font-26))))return 0;
 uint8_t *m=s->metrics[font-26];uint32_t count=0,width=0;
 for(const char *p=text;*p;p++){if((uint8_t)*p<32||(uint8_t)*p>126)return 0;width+=m[(uint8_t)*p];count++;}
 if(*n+(count+11)*4>capacity||width*11>4300)return 0;
 uint32_t h=(EveRomWord(m+140)*11+9)/10,w=(EveRomWord(m+136)*11+9)/10;
 int32_t x10=2400-(int32_t)(width*11)/2;uint32_t top=y-h/2;
 EveRomEmit(b,n,0x22000000);EveRomEmit(b,n,0x0500001f);
 EveRomEmit(b,n,0x01000000|EveRomWord(m+144));
 EveRomEmit(b,n,0x07000000|(EveRomWord(m+128)<<19)|(EveRomWord(m+132)<<9)|EveRomWord(m+140));
 EveRomEmit(b,n,0x08000000|(1U<<20)|(w<<9)|h);
 EveRomEmit(b,n,0x150000e9);EveRomEmit(b,n,0x190000e9);EveRomEmit(b,n,0x1f000001);
 for(const char *p=text;*p;p++){EveRomEmit(b,n,0x80000000|((uint32_t)((x10+5)/10)<<21)|(top<<12)|(31U<<7)|(uint8_t)*p);x10+=11*m[(uint8_t)*p];}
 EveRomEmit(b,n,0x21000000);EveRomEmit(b,n,0x23000000);return 1;
}
#endif
